#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// -------------------------------------------------------------------
// Lm1894LookAndFeel
//
// Rotary knob with a full 270° arc:
//   - Track arc (gray) shows the complete range.
//   - Value arc (blue) fills from minimum up to current position.
//   - A small pointer dot marks the exact value.
//   - Start angle: 225° (bottom-left), end: 495° = 135° (bottom-right).
// -------------------------------------------------------------------
class Lm1894LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Lm1894LookAndFeel()
    {
        // Text boxes.
        setColour(juce::Slider::textBoxTextColourId,     juce::Colour(0xffcccccc));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::Slider::textBoxOutlineColourId,  juce::Colour(0xff444444));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float /*rotaryStartAngle*/,
                          float /*rotaryEndAngle*/,
                          juce::Slider& /*slider*/) override
    {
        // Fixed 270° sweep, starting at bottom-left (225°) → bottom-right (495°=135°).
        constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f; // 225°
        constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f; // 495°
        constexpr float kSweep      = kEndAngle - kStartAngle;                // 270°

        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float r  = juce::jmin(width, height) * 0.5f - 4.0f;

        // ---- Background circle ----
        g.setColour(juce::Colour(0xff2b2b2b));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        // ---- Track arc (full range, gray) ----
        {
            juce::Path track;
            track.addCentredArc(cx, cy, r - 2.0f, r - 2.0f,
                                0.0f, kStartAngle, kEndAngle, true);
            juce::PathStrokeType stroke(3.0f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
            g.setColour(juce::Colour(0xff404040));
            g.strokePath(track, stroke);
        }

        // ---- Value arc (filled portion, blue) ----
        const float valueAngle = kStartAngle + sliderPosProportional * kSweep;
        if (sliderPosProportional > 0.0f)
        {
            juce::Path value;
            value.addCentredArc(cx, cy, r - 2.0f, r - 2.0f,
                                0.0f, kStartAngle, valueAngle, true);
            juce::PathStrokeType stroke(3.0f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
            g.setColour(juce::Colour(0xff4ea8de)); // sky blue
            g.strokePath(value, stroke);
        }

        // ---- Pointer dot ----
        const float dotR   = 3.5f;
        const float dotDist = r - 9.0f;
        const float dotX   = cx + dotDist * std::sin(valueAngle);
        const float dotY   = cy - dotDist * std::cos(valueAngle);
        g.setColour(juce::Colour(0xffffffff));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

        // ---- Tick marks at min and max (small lines on track) ----
        auto drawTick = [&](float angle)
        {
            const float inner = r - 8.0f;
            const float outer = r - 1.0f;
            float sx = cx + inner * std::sin(angle);
            float sy = cy - inner * std::cos(angle);
            float ex = cx + outer * std::sin(angle);
            float ey = cy - outer * std::cos(angle);
            g.setColour(juce::Colour(0xff666666));
            g.drawLine(sx, sy, ex, ey, 1.5f);
        };
        drawTick(kStartAngle);
        drawTick(kEndAngle);
    }
};
