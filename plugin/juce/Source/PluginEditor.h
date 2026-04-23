#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Lm1894LookAndFeel.h"

class Lm1894Processor;

class Lm1894Editor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit Lm1894Editor(Lm1894Processor&);
    ~Lm1894Editor() override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

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
    float meterDisplayLevel_ = 0.0f;       // smoothed 0-1 for bar drawing
    juce::Rectangle<float> barBounds_;     // set in resized(), used in paint() + timer

    // Attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        sensitivityAtt_, minBwAtt_, maxBwAtt_, attackAtt_, releaseAtt_,
        outputTrimAtt_, stageCountAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAtt_;

    Lm1894LookAndFeel lnf_;

    juce::TextButton enableInputButton_;
    void updateInputButton(bool enabled);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Lm1894Editor)
};
