#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "params/ParameterIds.h"

static void setupSlider(juce::Slider& s, juce::Label& l, const juce::String& name,
                         juce::Component& parent)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    parent.addAndMakeVisible(s);
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.attachToComponent(&s, false);
    parent.addAndMakeVisible(l);
}

Lm1894Editor::Lm1894Editor(Lm1894Processor& p)
    : AudioProcessorEditor(p), processor_(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);
    auto& apvts = p.getAPVTS();

    setupSlider(sensitivitySlider_, sensitivityLabel_, "Sensitivity", *this);
    setupSlider(minBwSlider_,       minBwLabel_,       "Min BW",      *this);
    setupSlider(maxBwSlider_,       maxBwLabel_,       "Max BW",      *this);
    setupSlider(attackSlider_,      attackLabel_,      "Attack",      *this);
    setupSlider(releaseSlider_,     releaseLabel_,     "Release",     *this);
    setupSlider(outputTrimSlider_,  outputTrimLabel_,  "Trim",        *this);
    setupSlider(stageCountSlider_,  stageCountLabel_,  "Stages",      *this);
    stageCountSlider_.setSliderStyle(juce::Slider::IncDecButtons);
    stageCountSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 18);

    profileBox_.addItemList({"Generic", "Tape", "FM", "TV"}, 1);
    addAndMakeVisible(profileBox_);
    profileLabel_.setText("Profile", juce::dontSendNotification);
    profileLabel_.attachToComponent(&profileBox_, true);
    addAndMakeVisible(profileLabel_);

    addAndMakeVisible(bypassButton_);

    meterLabel_.setText("-- dB", juce::dontSendNotification);
    meterLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(meterLabel_);

    // Attachments.
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    sensitivityAtt_ = std::make_unique<SliderAtt>(apvts, lm1894::param::kSensitivityDb,  sensitivitySlider_);
    minBwAtt_       = std::make_unique<SliderAtt>(apvts, lm1894::param::kMinBandwidthHz, minBwSlider_);
    maxBwAtt_       = std::make_unique<SliderAtt>(apvts, lm1894::param::kMaxBandwidthHz, maxBwSlider_);
    attackAtt_      = std::make_unique<SliderAtt>(apvts, lm1894::param::kAttackMs,        attackSlider_);
    releaseAtt_     = std::make_unique<SliderAtt>(apvts, lm1894::param::kReleaseMs,       releaseSlider_);
    outputTrimAtt_  = std::make_unique<SliderAtt>(apvts, lm1894::param::kOutputTrimDb,    outputTrimSlider_);
    stageCountAtt_  = std::make_unique<SliderAtt>(apvts, lm1894::param::kStageCount,      stageCountSlider_);

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    profileAtt_ = std::make_unique<ComboAtt>(apvts, lm1894::param::kSourceProfile, profileBox_);

    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    bypassAtt_ = std::make_unique<BtnAtt>(apvts, lm1894::param::kBypass, bypassButton_);

    setSize(520, 360);
    startTimerHz(15);
}

void Lm1894Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("LM1894 Dynamic Noise Reduction", getLocalBounds().removeFromTop(30),
               juce::Justification::centred);
}

void Lm1894Editor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // title

    const int knobW = 70, knobH = 80;
    auto row1 = area.removeFromTop(knobH + 20);
    sensitivitySlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(5);
    minBwSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(5);
    maxBwSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(5);
    attackSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(5);
    releaseSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(5);
    outputTrimSlider_.setBounds(row1.removeFromLeft(knobW));

    area.removeFromTop(10);
    auto row2 = area.removeFromTop(30);
    stageCountSlider_.setBounds(row2.removeFromLeft(100));
    row2.removeFromLeft(60); // space for "Profile" label
    profileBox_.setBounds(row2.removeFromLeft(100));
    row2.removeFromLeft(10);
    bypassButton_.setBounds(row2.removeFromLeft(80));

    area.removeFromTop(10);
    meterLabel_.setBounds(area.removeFromTop(24));
}

void Lm1894Editor::timerCallback()
{
    auto& m = processor_.getMeterState();
    float reduction = m.estimatedReductionDb.load(std::memory_order_relaxed);
    meterLabel_.setText(juce::String(reduction, 1) + " dB reduction",
                        juce::dontSendNotification);
}
