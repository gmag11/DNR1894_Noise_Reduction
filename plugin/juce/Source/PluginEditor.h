#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class Lm1894Processor;

class Lm1894Editor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit Lm1894Editor(Lm1894Processor&);
    ~Lm1894Editor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Lm1894Processor& processor_;

    // Sliders.
    juce::Slider sensitivitySlider_;
    juce::Slider minBwSlider_;
    juce::Slider maxBwSlider_;
    juce::Slider attackSlider_;
    juce::Slider releaseSlider_;
    juce::Slider outputTrimSlider_;
    juce::Slider stageCountSlider_;
    juce::ComboBox profileBox_;
    juce::ToggleButton bypassButton_{"Bypass"};

    // Labels.
    juce::Label sensitivityLabel_, minBwLabel_, maxBwLabel_,
                attackLabel_, releaseLabel_, outputTrimLabel_,
                stageCountLabel_, profileLabel_;

    // Meter.
    juce::Label meterLabel_;

    // Attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        sensitivityAtt_, minBwAtt_, maxBwAtt_, attackAtt_, releaseAtt_,
        outputTrimAtt_, stageCountAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Lm1894Editor)
};
