// Headless tests of the plugin layer: standalone transport, capture mode against a fake host
// timeline, state save / restore and editor construction (with an optional screenshot).
//
//   ToneAyanPluginTests                 run all tests
//   ToneAyanPluginTests --screenshot f  also write an editor screenshot PNG

#include "PluginProcessor.h"
#include "StateIO.h"

#include "support/Signals.h"

#include "model/Edits.h"

#include <juce_audio_utils/juce_audio_utils.h>

using namespace tone;
using namespace tone::plugin;

namespace
{
juce::File screenshotFile;

struct FakePlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setTimeInSamples (time);
        info.setTimeInSeconds ((double) time / rate);
        return info;
    }
    bool playing = false;
    juce::int64 time = 0;
    double rate = 48000.0;
};

/** Pumps the message loop until the predicate holds (or times out). */
bool waitFor (const std::function<bool()>& done, int timeoutMs = 30000)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (! done())
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        if (juce::Time::getMillisecondCounter() - start > (juce::uint32) timeoutMs)
            return false;
    }
    return true;
}

bool sessionIdle (EditSession& s)
{
    return s.hasAnalysis() && ! s.isAnalysing() && ! s.isRendering();
}

std::vector<float> melody (double sr)
{
    auto x = test::vowel (test::noteSequence ({ 57, 60, 64, 62 }, 0.35), 1.4, sr);
    test::applyFades (x, sr);
    return x;
}

/** Runs the processor block by block and returns channel 0 of the output. */
std::vector<float> runBlocks (ToneAyanProcessor& proc, FakePlayHead* ph, const std::vector<float>& input, int block = 512)
{
    std::vector<float> out;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    for (size_t pos = 0; pos < input.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, input.size() - pos);
        buf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
            buf.copyFrom (c, 0, input.data() + pos, n);
        proc.processBlock (buf, midi);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
        if (ph != nullptr)
            ph->time += n;
    }
    return out;
}

class ToneAyanPluginTests : public juce::UnitTest
{
public:
    ToneAyanPluginTests() : UnitTest ("Tone-ayan plugin", "Tone-ayan") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("Standalone: open a file, edit, play back the render");
        {
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
            ToneAyanProcessor proc;
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
            expect (proc.isStandalone());
            proc.setPlayConfigDetails (2, 2, sr, 512);
            proc.prepareToPlay (sr, 512);

            const auto wav = juce::File::createTempFile (".wav");
            const auto x = melody (sr);
            expect (writeWav (wav, x, sr));
            juce::String error;
            expect (proc.importAudioFile (wav, error), error);
            auto& session = proc.getSession();
            expect (waitFor ([&] { return sessionIdle (session); }), "analysis / render timed out");
            expectEquals ((int) session.getProject().notes.size(), 4);

            session.perform ("Transpose", [&] (Project& p) { edits::transpose (p, session.selectionOrAll(), 3.0f); });
            expect (waitFor ([&] { return ! session.isRendering(); }));

            proc.setTransportPosition (0.0);
            proc.setTransportPlaying (true);
            const auto out = runBlocks (proc, nullptr, std::vector<float> (x.size(), 0.0f));
            const float measured = test::measuredMidi (out, sr, 0.75, 0.95); // third note (E4 -> G4)
            expectWithinAbsoluteError (measured, 67.0f, 0.05f);
            wav.deleteFile();
        }

        beginTest ("Capture mode: record against the host timeline, analyse, replace on playback");
        juce::MemoryBlock savedState;
        {
            ToneAyanProcessor proc;
            expect (! proc.isStandalone());
            FakePlayHead ph;
            ph.rate = sr;
            proc.setPlayHead (&ph);
            proc.setPlayConfigDetails (2, 2, sr, 512);
            proc.prepareToPlay (sr, 512);

            const auto x = melody (sr);
            ph.time = (juce::int64) (2.0 * sr); // the take starts 2 s into the song
            proc.setCaptureArmed (true);
            ph.playing = true;
            const auto passThrough = runBlocks (proc, &ph, x);
            expectEquals (passThrough[(size_t) (0.5 * sr)], x[(size_t) (0.5 * sr)], "input passes through while capturing");

            ph.playing = false;
            runBlocks (proc, &ph, std::vector<float> (2048, 0.0f));
            auto& session = proc.getSession();
            expect (waitFor ([&] { return session.hasSource(); }, 5000), "capture was not finalised after stop");
            expect (! proc.isCaptureArmed());
            expectWithinAbsoluteError (proc.getClipOffsetSeconds(), 2.0, 1.0e-9);
            expectEquals ((int) session.getSource()->samples.size(), (int) x.size());
            expect (waitFor ([&] { return sessionIdle (session); }));
            expectEquals ((int) session.getProject().notes.size(), 4);

            session.perform ("Transpose", [&] (Project& p) { edits::transpose (p, session.selectionOrAll(), -2.0f); });
            expect (waitFor ([&] { return ! session.isRendering(); }));

            // Play the song again: the clip region is replaced by the render; outside it the input passes.
            ph.time = 0;
            ph.playing = true;
            std::vector<float> songInput ((size_t) (4.0 * sr), 0.01f);
            const auto out = runBlocks (proc, &ph, songInput);
            expectEquals (out[(size_t) (1.0 * sr)], 0.01f, "outside the clip the input passes through");
            std::vector<float> clipOut (out.begin() + (std::ptrdiff_t) (2.0 * sr), out.begin() + (std::ptrdiff_t) (2.0 * sr) + (std::ptrdiff_t) x.size());
            expectWithinAbsoluteError (test::measuredMidi (clipOut, sr, 0.75, 0.95), 62.0f, 0.05f);

            proc.getStateInformation (savedState);
            proc.setPlayHead (nullptr);
        }

        beginTest ("State save / restore keeps audio, analysis and edits");
        {
            ToneAyanProcessor proc;
            proc.setStateInformation (savedState.getData(), (int) savedState.getSize());
            auto& session = proc.getSession();
            expect (session.hasSource() && session.hasAnalysis());
            expectEquals ((int) session.getProject().notes.size(), 4);
            expectWithinAbsoluteError (session.getProject().notes[2].pitchShift, -2.0f, 1.0e-6f);
            expectWithinAbsoluteError (proc.getClipOffsetSeconds(), 2.0, 1.0e-9);
            expect (session.canUndo() == false);
            expect (waitFor ([&] { return ! session.isRendering(); }));

            // Corrupt state must not crash.
            ToneAyanProcessor other;
            juce::MemoryBlock junk (savedState);
            static_cast<char*> (junk.getData())[40] ^= 0x5a;
            junk.setSize (junk.getSize() / 2);
            other.setStateInformation (junk.getData(), (int) junk.getSize());
        }

        beginTest ("Editor opens, shows notes and can be resized");
        {
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
            ToneAyanProcessor proc;
            juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
            const auto wav = juce::File::createTempFile (".wav");
            auto x = test::vowel ([] (double t) { return 220.0 * std::exp2 ((t < 0.6 ? 0.0 : (t < 1.2 ? 3.0 : 7.0)) / 12.0
                                                                         + 0.4 / 12.0 * std::sin (2.0 * kPi * 5.5 * t)); }, 1.8, sr);
            test::applyFades (x, sr);
            expect (writeWav (wav, x, sr));
            juce::String error;
            proc.importAudioFile (wav, error);
            auto& session = proc.getSession();
            expect (waitFor ([&] { return sessionIdle (session); }));
            const auto& notes = session.getProject().notes;
            if (notes.size() >= 3)
            {
                session.perform ("Edit", [&] (Project& p)
                {
                    edits::transpose (p, { p.notes[1].id }, -1.0f);
                    edits::setModulation (p, { p.notes[2].id }, 0.2f);
                    edits::moveTime (p, { p.notes[2].id }, 0.15);
                });
                session.setSelection ({ notes[1].id });
            }

            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            editor->setSize (1320, 760);
            proc.setTransportPosition (0.9);
            waitFor ([] { return false; }, 300);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            expect (image.isValid());
            if (screenshotFile != juce::File())
            {
                screenshotFile.deleteFile();
                juce::FileOutputStream os (screenshotFile);
                juce::PNGImageFormat().writeImageToStream (image, os);
                logMessage ("wrote " + screenshotFile.getFullPathName());
            }
            editor->setSize (900, 520);
            waitFor ([] { return false; }, 50);
            editor.reset();
            wav.deleteFile();
        }
    }
};

ToneAyanPluginTests pluginTests;
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshot")
            screenshotFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Tone-ayan");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;
    std::printf ("\n%s: %d failures\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
