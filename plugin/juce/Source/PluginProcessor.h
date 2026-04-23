#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Lm1894Model.h"
#include "app/MeterState.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

class Lm1894Processor : public juce::AudioProcessor
{
public:
    Lm1894Processor();
    ~Lm1894Processor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LM1894 DNR"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    lm1894::MeterState& getMeterState() { return meterState_; }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

    void setStandaloneInputEnabled(bool enabled) { standaloneInputEnabled_.store(enabled, std::memory_order_relaxed); }
    bool getStandaloneInputEnabled() const { return standaloneInputEnabled_.load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    lm1894::Lm1894Parameters gatherParameters() const;

    juce::AudioProcessorValueTreeState apvts_;
    lm1894::Lm1894Model model_;
    lm1894::MeterState meterState_;
    std::atomic<bool> standaloneInputEnabled_{true}; // true = pass audio; false = mute (standalone only)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Lm1894Processor)
};
