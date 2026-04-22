#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

Lm1894Processor::Lm1894Processor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Lm1894Processor::createParameterLayout()
{
    using namespace lm1894;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kSensitivity, 1), "Sensitivity",
        juce::NormalisableRange<float>(defaults::kSensitivityRange.min, defaults::kSensitivityRange.max, 0.1f),
        defaults::kSensitivityRange.defaultVal));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kMinBandwidth, 1), "Min Bandwidth",
        juce::NormalisableRange<float>(defaults::kMinBandwidthRange.min, defaults::kMinBandwidthRange.max, 1.0f,
            0.3f),  // skew for log-like behaviour
        defaults::kMinBandwidthRange.defaultVal));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kMaxBandwidth, 1), "Max Bandwidth",
        juce::NormalisableRange<float>(defaults::kMaxBandwidthRange.min, defaults::kMaxBandwidthRange.max, 1.0f,
            0.3f),
        defaults::kMaxBandwidthRange.defaultVal));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kAttack, 1), "Attack",
        juce::NormalisableRange<float>(defaults::kAttackRange.min, defaults::kAttackRange.max, 0.01f, 0.5f),
        defaults::kAttackRange.defaultVal));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kRelease, 1), "Release",
        juce::NormalisableRange<float>(defaults::kReleaseRange.min, defaults::kReleaseRange.max, 0.1f, 0.5f),
        defaults::kReleaseRange.defaultVal));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(param::kSourceProfile, 1), "Source Profile",
        juce::StringArray{"Generic", "Tape", "FM", "TV"}, 1));  // default: Tape

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(param::kStageCount, 1), "Stages",
        1, 4, static_cast<int>(defaults::kStageCountRange.defaultVal)));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(param::kBypass, 1), "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(param::kOutputTrim, 1), "Output Trim",
        juce::NormalisableRange<float>(defaults::kOutputTrimRange.min, defaults::kOutputTrimRange.max, 0.1f),
        defaults::kOutputTrimRange.defaultVal));

    return { params.begin(), params.end() };
}

lm1894::Lm1894Parameters Lm1894Processor::gatherParameters() const
{
    using namespace lm1894;
    Lm1894Parameters p;
    p.sensitivityDb  = apvts_.getRawParameterValue(param::kSensitivity)->load();
    p.minBandwidthHz = apvts_.getRawParameterValue(param::kMinBandwidth)->load();
    p.maxBandwidthHz = apvts_.getRawParameterValue(param::kMaxBandwidth)->load();
    p.attackMs       = apvts_.getRawParameterValue(param::kAttack)->load();
    p.releaseMs      = apvts_.getRawParameterValue(param::kRelease)->load();
    p.sourceProfile  = static_cast<SourceProfile>(
                         static_cast<int>(apvts_.getRawParameterValue(param::kSourceProfile)->load()));
    p.stageCount     = static_cast<int>(apvts_.getRawParameterValue(param::kStageCount)->load());
    p.bypass         = apvts_.getRawParameterValue(param::kBypass)->load() >= 0.5f;
    p.outputTrimDb   = apvts_.getRawParameterValue(param::kOutputTrim)->load();
    return p;
}

void Lm1894Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lm1894::ProcessSpec spec{};
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels      = 2;
    model_.prepare(spec);
    model_.setParameters(gatherParameters());
}

void Lm1894Processor::releaseResources()
{
    model_.reset();
}

void Lm1894Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Ensure stereo.
    if (buffer.getNumChannels() < 2) return;

    model_.setParameters(gatherParameters());

    lm1894::StereoBufferView view{};
    view.left       = buffer.getWritePointer(0);
    view.right      = buffer.getWritePointer(1);
    view.numSamples = static_cast<uint32_t>(buffer.getNumSamples());
    model_.process(view);

    model_.publishMeters(meterState_);
}

void Lm1894Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Lm1894Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* Lm1894Processor::createEditor()
{
    return new Lm1894Editor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Lm1894Processor();
}
