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
    stageCountSlider_.setSliderStyle(juce::Slider::IncDecButtons);
    stageCountSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 18);
    addAndMakeVisible(stageCountSlider_);
    stageCountLabel_.setText("Stages", juce::dontSendNotification);
    stageCountLabel_.setJustificationType(juce::Justification::centredRight);
    stageCountLabel_.attachToComponent(&stageCountSlider_, true);  // true = left side
    addAndMakeVisible(stageCountLabel_);

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

    if (processor_.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        // Start muted — safe default to avoid feedback on launch.
        processor_.setStandaloneInputEnabled(false);
        updateInputButton(false);
        enableInputButton_.onClick = [this]()
        {
            bool nowEnabled = !processor_.getStandaloneInputEnabled();
            processor_.setStandaloneInputEnabled(nowEnabled);
            updateInputButton(nowEnabled);
        };
        addAndMakeVisible(enableInputButton_);
    }

    setSize(520, 260);
    startTimerHz(15);
}

void Lm1894Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("DNR1894 Dynamic Noise Reduction", getLocalBounds().removeFromTop(30),
               juce::Justification::centred);

    if (barBounds_.isEmpty()) return;

    // Track (full range, dark).
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(barBounds_, 4.0f);

    // Fill (active portion, green gradient left→right).
    const float fillW = barBounds_.getWidth() * meterDisplayLevel_;
    if (fillW > 0.5f)
    {
        juce::Rectangle<float> fill(barBounds_.getX(), barBounds_.getY(),
                                    fillW, barBounds_.getHeight());
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff0d5c0d), barBounds_.getX(),           0.0f,
            juce::Colour(0xff44ff66), barBounds_.getRight(),        0.0f,
            false));
        g.fillRoundedRectangle(fill, 4.0f);

        // Soft glow at the leading edge.
        const float glowR = barBounds_.getHeight() * 1.2f;
        const float glowX = barBounds_.getX() + fillW;
        const float glowY = barBounds_.getCentreY();
        juce::ColourGradient glow(juce::Colour(0x8844ff66), glowX, glowY,
                                  juce::Colour(0x0044ff66), glowX + glowR, glowY, true);
        g.setGradientFill(glow);
        g.fillEllipse(glowX - glowR, glowY - glowR, glowR * 2.0f, glowR * 2.0f);
    }

    // Track border.
    g.setColour(juce::Colour(0xff404040));
    g.drawRoundedRectangle(barBounds_, 4.0f, 1.0f);
}

void Lm1894Editor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // title

    // Each knob slot: label (18) + knob body (70) + text box (20) = 108 px total.
    // We hand the full 108 px to the slider so JUCE can place label+body+textbox
    // without overflowing into the row below.
    const int knobW      = 70;
    const int labelH     = 18;
    const int knobBodyH  = 70;
    const int textBoxH   = 20;
    const int knobTotalH = labelH + knobBodyH + textBoxH; // 108
    const int gap        = 5;

    auto row1 = area.removeFromTop(knobTotalH);
    sensitivitySlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(gap);
    minBwSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(gap);
    maxBwSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(gap);
    attackSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(gap);
    releaseSlider_.setBounds(row1.removeFromLeft(knobW));
    row1.removeFromLeft(gap);
    outputTrimSlider_.setBounds(row1.removeFromLeft(knobW));

    area.removeFromTop(8);
    auto row2 = area.removeFromTop(30);
    row2.removeFromLeft(52);  // space for "Stages" label on the left
    stageCountSlider_.setBounds(row2.removeFromLeft(80));
    row2.removeFromLeft(50); // space for "Profile" label
    profileBox_.setBounds(row2.removeFromLeft(100));
    row2.removeFromLeft(10);
    bypassButton_.setBounds(row2.removeFromLeft(80));
#if JUCE_STANDALONE_APPLICATION
    if (enableInputButton_.isVisible())
    {
        row2.removeFromLeft(8);
        enableInputButton_.setBounds(row2.removeFromLeft(90));
    }
#endif

    area.removeFromTop(8);
    // Bandwidth bar.
    barBounds_ = area.removeFromTop(10).reduced(0, 1).toFloat();
    area.removeFromTop(6);
    meterLabel_.setBounds(area.removeFromTop(20));
}

void Lm1894Editor::timerCallback()
{
    auto& m = processor_.getMeterState();

    // Smoothed bar level (first-order, ~80 ms at 15 Hz).
    float raw = m.normalizedBandwidthOpen.load(std::memory_order_relaxed);
    meterDisplayLevel_ += 0.15f * (raw - meterDisplayLevel_);
    repaint(barBounds_.toNearestInt());

    float reduction = m.estimatedReductionDb.load(std::memory_order_relaxed);
    meterLabel_.setText(juce::String(reduction, 1) + " dB reduction",
                        juce::dontSendNotification);
}

void Lm1894Editor::updateInputButton(bool enabled)
{
    if (enabled)
    {
        enableInputButton_.setButtonText("[ON]  Input");
        enableInputButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0d3d0d));
        enableInputButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff44ee44));
    }
    else
    {
        enableInputButton_.setButtonText("[OFF] Input");
        enableInputButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a1010));
        enableInputButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdd4444));
    }
}
