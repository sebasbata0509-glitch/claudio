#include "PluginProcessor.h"

#include <set>

namespace
{
using namespace nebula;

class ParameterTests final : public juce::UnitTest
{
public:
    ParameterTests() : juce::UnitTest ("Parameters", "Nebula") {}

    void runTest() override
    {
        NebulaProcessor proc;
        auto& apvts = proc.getAPVTS();
        const auto& params = proc.getParameters();

        beginTest ("layout is complete and IDs are unique");
        {
            std::set<juce::String> seen;
            for (auto* p : params)
            {
                auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
                expect (withId != nullptr);
                expect (seen.insert (withId->getParameterID()).second, "duplicate id " + withId->getParameterID());
                expect (withId->getVersionHint() == 1);
            }
            logMessage ("parameter count: " + juce::String (params.size()));
            expect (params.size() > 250);
        }

        beginTest ("every layer has the common controls");
        for (int l = 0; l < kNumLayers; ++l)
            for (auto* n : { "vol", "pan", "mute", "solo", "octave", "attack", "decay", "sustain", "release",
                             "filter_on", "ftype", "cutoff", "reso", "drive", "fenv", "fkey" })
                expect (apvts.getParameter (ids::layer (l, n)) != nullptr, ids::layer (l, n));

        beginTest ("every mod destination points at a real continuous parameter");
        {
            const auto& dests = modDestinations();
            expectEquals (juce::String (dests[0].paramId), juce::String());
            for (size_t i = 1; i < dests.size(); ++i)
            {
                auto* p = apvts.getParameter (dests[i].paramId);
                expect (p != nullptr, dests[i].paramId);
                expect (dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr, juce::String ("not float: ") + dests[i].paramId);
            }
            expectEquals (modDestinationNames().size(), (int) dests.size());
        }

        beginTest ("choice lists match their enums");
        expectEquals (wavetableNames().size(), (int) Wavetable::Metallic + 1);
        expectEquals (filterTypeNames().size(), (int) FilterType::HP24 + 1);
        expectEquals (modSourceNames().size(), (int) ModSource::Macro4 + 1);
        expectEquals (fxSlotNames().size(), kNumFxSlots);
        expectEquals (syncDivisionNames().size(), 15);
        expectWithinAbsoluteError (syncDivisionBeats (6), 1.0, 1e-9);   // 1/4
        expectWithinAbsoluteError (syncDivisionBeats (99), 0.125, 1e-9); // clamped

        beginTest ("every parameter formats its values");
        for (auto* p : params)
            for (float v : { 0.0f, 0.37f, 1.0f })
                expect (p->getText (v, 32).isNotEmpty(), p->getName (64));

        beginTest ("state round-trip keeps parameters and extra state");
        {
            auto set = [&] (const char* id, float norm) { apvts.getParameter (id)->setValueNotifyingHost (norm); };
            set ("l1_cutoff", 0.123f);
            set ("rv_decay", 0.9f);
            set ("mm5_dst", 0.5f);
            proc.setFxOrder ({ 5, 4, 3, 2, 1, 0 });
            proc.getExtraState().setProperty (NebulaProcessor::samplePathId, "C:/samples/choir.wav", nullptr);

            juce::MemoryBlock block;
            proc.getStateInformation (block);

            NebulaProcessor other;
            other.setStateInformation (block.getData(), (int) block.getSize());
            auto& o = other.getAPVTS();

            for (auto* id : { "l1_cutoff", "rv_decay", "mm5_dst" })
                expectWithinAbsoluteError (o.getParameter (id)->getValue(), apvts.getParameter (id)->getValue(), 1e-6f);

            const std::array<int, kNumFxSlots> expected { 5, 4, 3, 2, 1, 0 };
            expect (other.getFxOrder() == expected);
            expectEquals (other.getExtraState()[NebulaProcessor::samplePathId].toString(), juce::String ("C:/samples/choir.wav"));
        }

        beginTest ("invalid FX order is rejected");
        {
            NebulaProcessor p2;
            const auto before = p2.getFxOrder();
            p2.setFxOrder ({ 0, 0, 1, 2, 3, 4 });
            expect (p2.getFxOrder() == before);
        }

        beginTest ("garbage state is ignored");
        {
            NebulaProcessor p2;
            const char junk[] = "not a nebula state";
            p2.setStateInformation (junk, (int) sizeof (junk));
            expectWithinAbsoluteError (p2.getAPVTS().getParameter ("l1_cutoff")->getValue(),
                                       p2.getAPVTS().getParameter ("l1_cutoff")->getDefaultValue(), 1e-6f);
        }
    }
};

class ProcessorSmokeTests final : public juce::UnitTest
{
public:
    ProcessorSmokeTests() : juce::UnitTest ("Processor smoke", "Nebula") {}

    void runTest() override
    {
        beginTest ("stereo-only output, no input");
        {
            NebulaProcessor proc;
            juce::AudioProcessor::BusesLayout layout;
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (proc.checkBusesLayoutSupported (layout));
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
            expect (! proc.checkBusesLayoutSupported (layout));
        }

        beginTest ("processes blocks of odd sizes with MIDI and stays finite");
        {
            NebulaProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);
            proc.prepareToPlay (48000.0, 512);

            juce::Random rng (1);
            for (int b = 0; b < 200; ++b)
            {
                const int n = 1 + rng.nextInt (512);
                juce::AudioBuffer<float> buf (2, n);
                juce::MidiBuffer midi;
                if (b % 7 == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48 + rng.nextInt (24), (juce::uint8) 100), 0);
                if (b % 11 == 0)
                    midi.addEvent (juce::MidiMessage::allNotesOff (1), n - 1);
                proc.processBlock (buf, midi);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        if (! std::isfinite (buf.getSample (ch, i)) || std::abs (buf.getSample (ch, i)) > 1.0f)
                        {
                            expect (false, "bad sample");
                            return;
                        }
            }
            proc.releaseResources();
        }
    }
};

ParameterTests parameterTests;
ProcessorSmokeTests processorSmokeTests;
} // namespace
