#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// CustomLookAndFeel
//   - Rotary sliders: background arc + rounded filled arc, NO thumb dot
//   - Linear sliders: 2 px thin track + small thumb dot
//==============================================================================
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const float cx     = x + width  * 0.5f;
        const float cy     = y + height * 0.5f;
        const float radius = juce::jmin (width, height) * 0.5f - 10.0f;

        if (radius <= 0.0f) return;

        const juce::PathStrokeType stroke (12.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);

        // Background track arc
        juce::Path bgArc;
        bgArc.addCentredArc (cx, cy, radius, radius, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff252530));
        g.strokePath (bgArc, stroke);

        // Foreground filled arc
        if (sliderPos > 0.001f)
        {
            const float angle = startAngle + sliderPos * (endAngle - startAngle);
            juce::Path fgArc;
            fgArc.addCentredArc (cx, cy, radius, radius, 0.0f,
                                 startAngle, angle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            g.strokePath (fgArc, stroke);
        }
        // No thumb dot drawn intentionally

        // Value text centred inside the arc
        g.setColour (juce::Colours::white);
        g.setFont   (juce::FontOptions (13.0f, juce::Font::bold));
        const juce::String valStr =
            juce::String ((int) std::round (slider.getValue())) + "%";
        g.drawText (valStr,
                    juce::Rectangle<float> (cx - 30.0f, cy - 10.0f, 60.0f, 20.0f),
                    juce::Justification::centred, false);
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float /*minPos*/, float /*maxPos*/,
                           juce::Slider::SliderStyle /*style*/,
                           juce::Slider& slider) override
    {
        const float trackH  = 2.0f;
        const float centreY = y + height * 0.5f;

        // Background track — very dark, barely visible
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRoundedRectangle ((float)x, centreY - trackH * 0.5f,
                                (float)width, trackH, 1.0f);

        // Filled (left-of-thumb) portion
        const float fillW = sliderPos - (float)x;
        if (fillW > 0.0f)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle ((float)x, centreY - trackH * 0.5f,
                                    fillW, trackH, 1.0f);
        }

        // Thumb dot
        const float thumbR = 5.0f;
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (sliderPos - thumbR, centreY - thumbR,
                       thumbR * 2.0f, thumbR * 2.0f);
    }
};

//==============================================================================
// AIButtonLookAndFeel
//   - Draws the AI button as a perfect filled circle, no border, yellow text
//==============================================================================
class AIButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*bgColour*/,
                               bool /*highlighted*/, bool /*down*/) override
    {
        auto  b   = button.getLocalBounds().toFloat();
        float sz  = juce::jmin (b.getWidth(), b.getHeight());
        auto  circ = juce::Rectangle<float> (b.getCentreX() - sz * 0.5f,
                                             b.getCentreY() - sz * 0.5f, sz, sz);
        g.setColour (juce::Colour (0xff151515));
        g.fillEllipse (circ);
        // No outline drawn
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*highlighted*/, bool /*down*/) override
    {
        g.setColour (juce::Colour (0xffffd600));
        g.setFont   (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText  ("AI", button.getLocalBounds(),
                     juce::Justification::centred, false);
    }
};

//==============================================================================
class AfroplugAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AfroplugAudioProcessorEditor (AfroplugAudioProcessor&);
    ~AfroplugAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    AfroplugAudioProcessor& processorRef;

    // =========================================================================
    // HEADER
    // =========================================================================
    juce::Label       headerTitleLabel;
    juce::TextButton  prevPresetBtn;
    juce::ComboBox    presetCombo;
    juce::TextButton  nextPresetBtn;
    juce::TextButton  savePresetBtn;
    juce::Label       categoryLabel;

    // =========================================================================
    // TOP ROW
    // =========================================================================
    juce::Label    eqTitleLabel;
    juce::Label    dynamicsSubLabel;    // layout arg — not displayed
    juce::Slider   eqSweepSlider;
    juce::ComboBox eqModeSelector;

    juce::Label    toneTitleLabel;
    juce::Label    toneSubLabel;      // not displayed
    juce::Slider   colorVintageSlider;
    juce::ComboBox toneModeSelector;

    juce::Label  spaceTitleLabel;
    juce::Label  spaceSubLabel;       // not displayed
    juce::Slider vibePhaserSlider;

    juce::Label  sfxTitleLabel;
    juce::Label  sfxSubLabel;         // not displayed
    juce::Slider stereoWidthSlider;

    // =========================================================================
    // BOTTOM ROW
    // =========================================================================
    juce::Label    reverbTitleLabel;
    juce::Slider   spaceReverbSlider;
    juce::ComboBox reverbModeSelector;
    juce::Label  reverbValueLabel;    // updated by onValueChange (not displayed)
    juce::Label  reverbMinLabel;
    juce::Label  reverbMaxLabel;

    juce::TextButton aiButton;
    juce::Label      aiAnalyzeLabel;

    juce::Label  delayTitleLabel;
    juce::Slider delayTextureSlider;
    juce::Label  delayValueLabel;     // updated by onValueChange (not displayed)
    juce::Label  delayMinLabel;
    juce::Label  delayMaxLabel;

    // =========================================================================
    // MIX BAR
    // =========================================================================
    juce::Label  mixLabel;
    juce::Slider mixWetSlider;
    juce::Label  mixValueLabel;

    // =========================================================================
    // Panel bounds — set in resized(), used in paint()
    // =========================================================================
    juce::Rectangle<int> eqPanelRect;
    juce::Rectangle<int> tonePanelRect;
    juce::Rectangle<int> spacePanelRect;
    juce::Rectangle<int> sfxPanelRect;
    juce::Rectangle<int> reverbPanelRect;
    juce::Rectangle<int> aiPanelRect;
    juce::Rectangle<int> delayPanelRect;
    juce::Rectangle<int> mixBarRect;

    // =========================================================================
    // APVTS Attachments — declared AFTER sliders, destroyed before them
    // =========================================================================
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment>   eqSweepAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboBoxAttachment> eqModeAttachment;
    std::unique_ptr<ComboBoxAttachment> toneModeAttachment;
    std::unique_ptr<ComboBoxAttachment> reverbModeAttachment;
    std::unique_ptr<SliderAttachment> colorVintageAttachment;
    std::unique_ptr<SliderAttachment> vibePhaserAttachment;
    std::unique_ptr<SliderAttachment> stereoWidthAttachment;
    std::unique_ptr<SliderAttachment> spaceReverbAttachment;
    std::unique_ptr<SliderAttachment> delayTextureAttachment;
    std::unique_ptr<SliderAttachment> mixWetAttachment;

    // =========================================================================
    // LookAndFeel instances — declared LAST so destroyed FIRST
    //   (components must not outlive their LookAndFeel)
    // =========================================================================
    CustomLookAndFeel   customLaf;
    AIButtonLookAndFeel aiButtonLaf;

    // =========================================================================
    // Helpers
    // =========================================================================
    void makeLabel   (juce::Label&, const juce::String& text,
                      float fontSize, juce::Colour colour,
                      juce::Justification just = juce::Justification::centred);
    void makeHSlider (juce::Slider&, juce::Colour trackColour);
    void makeRotary  (juce::Slider&, juce::Colour fillColour);
    void refreshValueLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfroplugAudioProcessorEditor)
};
