#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
uint32_t packOrder (const std::array<int, nebula::kNumFxSlots>& order) noexcept
{
    uint32_t packed = 0;
    for (int i = 0; i < nebula::kNumFxSlots; ++i)
        packed |= (uint32_t) (order[(size_t) i] & 0xf) << (4 * i);
    return packed;
}

std::array<int, nebula::kNumFxSlots> defaultOrder() noexcept
{
    std::array<int, nebula::kNumFxSlots> order {};
    for (int i = 0; i < nebula::kNumFxSlots; ++i)
        order[(size_t) i] = i;
    return order;
}

juce::String orderToString (const std::array<int, nebula::kNumFxSlots>& order)
{
    juce::StringArray s;
    for (auto i : order)
        s.add (juce::String (i));
    return s.joinIntoString (",");
}
} // namespace

//==============================================================================
NebulaProcessor::NebulaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NEBULA", nebula::createParameterLayout())
{
    packedFxOrder.store (packOrder (defaultOrder()));
    getExtraState().setProperty (fxOrderId, orderToString (defaultOrder()), nullptr);
}

NebulaProcessor::~NebulaProcessor() = default;

//==============================================================================
void NebulaProcessor::prepareToPlay (double, int) {}

void NebulaProcessor::releaseResources() {}

bool NebulaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void NebulaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);
    buffer.clear();
}

//==============================================================================
std::array<int, nebula::kNumFxSlots> NebulaProcessor::getFxOrder() const noexcept
{
    const auto packed = packedFxOrder.load();
    std::array<int, nebula::kNumFxSlots> order {};
    for (int i = 0; i < nebula::kNumFxSlots; ++i)
        order[(size_t) i] = (int) ((packed >> (4 * i)) & 0xf);
    return order;
}

bool NebulaProcessor::isValidFxOrder (const std::array<int, nebula::kNumFxSlots>& order) noexcept
{
    std::array<bool, nebula::kNumFxSlots> seen {};
    for (auto i : order)
    {
        if (i < 0 || i >= nebula::kNumFxSlots || seen[(size_t) i])
            return false;
        seen[(size_t) i] = true;
    }
    return true;
}

void NebulaProcessor::setFxOrder (const std::array<int, nebula::kNumFxSlots>& order)
{
    if (! isValidFxOrder (order))
        return;
    packedFxOrder.store (packOrder (order));
    getExtraState().setProperty (fxOrderId, orderToString (order), nullptr);
}

void NebulaProcessor::syncFxOrderFromState()
{
    auto tokens = juce::StringArray::fromTokens (getExtraState()[fxOrderId].toString(), ",", {});
    std::array<int, nebula::kNumFxSlots> order = defaultOrder();

    if (tokens.size() == nebula::kNumFxSlots)
        for (int i = 0; i < nebula::kNumFxSlots; ++i)
            order[(size_t) i] = tokens[i].getIntValue();

    setFxOrder (isValidFxOrder (order) ? order : defaultOrder());
}

//==============================================================================
void NebulaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", stateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NebulaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
        return;

    apvts.replaceState (tree);
    syncFxOrderFromState();
}

//==============================================================================
juce::AudioProcessorEditor* NebulaProcessor::createEditor()
{
    return new NebulaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaProcessor();
}
