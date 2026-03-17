#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * AfroplugAudioProcessorEditor  —  Milestone 1 UI
 *
 * Structural skeleton of the Figma design using only standard JUCE components
 * and a dark colour palette.  No custom LookAndFeel, no custom graphics.
 *
 * Window: 900 × 560 px
 *
 * Sections (top → bottom):
 *   Header    56 px   — brand, preset nav, category label
 *   Top Row  188 px   — 4 equal panels: DYNAMICS / TONE / SPACE / SFX
 *   Bot Row  232 px   — REVERB knob | AI button | DELAY knob
 *   Mix Bar   52 px   — full-width MIX slider
 *   Footer    32 px   — version / format info
 */
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
    juce::Label       headerTitleLabel;   // "AFROPLUG"
    juce::TextButton  prevPresetBtn;      // "<"
    juce::ComboBox    presetCombo;        // "Lagos Nights"
    juce::TextButton  nextPresetBtn;      // ">"
    juce::Label       categoryLabel;      // "MELODY FX"

    // =========================================================================
    // TOP ROW — DYNAMICS  (placeholder, no APVTS param)
    // =========================================================================
    juce::Label dynamicsTitleLabel;       // "DYNAMICS"  (red)
    juce::Label dynamicsSubLabel;         // "< Tube >"

    // TOP ROW — TONE  →  color_vintage
    juce::Label  toneTitleLabel;          // "TONE"  (cyan)
    juce::Label  toneSubLabel;            // "< Clear >"
    juce::Slider colorVintageSlider;

    // TOP ROW — SPACE  →  vibe_phaser
    juce::Label  spaceTitleLabel;         // "SPACE"  (purple)
    juce::Label  spaceSubLabel;           // "< Room >"
    juce::Slider vibePhaserSlider;

    // TOP ROW — SFX  →  stereo_width
    juce::Label  sfxTitleLabel;           // "SFX"  (yellow)
    juce::Label  sfxSubLabel;             // "< Wide >"
    juce::Slider stereoWidthSlider;

    // =========================================================================
    // BOTTOM ROW — REVERB  →  space_reverb
    // =========================================================================
    juce::Label  reverbTitleLabel;        // "REVERB"
    juce::Slider spaceReverbSlider;       // rotary
    juce::Label  reverbValueLabel;        // "28%"
    juce::Label  reverbMinLabel;          // "0"
    juce::Label  reverbMaxLabel;          // "100"

    // BOTTOM ROW — AI button (center)
    juce::TextButton aiButton;
    juce::Label      aiAnalyzeLabel;      // "ANALYZE"

    // BOTTOM ROW — DELAY  →  delay_texture
    juce::Label  delayTitleLabel;         // "DELAY"
    juce::Slider delayTextureSlider;      // rotary
    juce::Label  delayValueLabel;         // "22%"
    juce::Label  delayMinLabel;           // "0"
    juce::Label  delayMaxLabel;           // "100"

    // =========================================================================
    // MIX BAR  →  mix_wet
    // =========================================================================
    juce::Label  mixLabel;               // "MIX"
    juce::Slider mixWetSlider;
    juce::Label  mixValueLabel;          // "62%"

    // =========================================================================
    // FOOTER
    // =========================================================================
    // Panel bounds — set in resized(), read in paint() for border drawing
    // =========================================================================
    juce::Rectangle<int> dynamicsPanelRect;
    juce::Rectangle<int> tonePanelRect;
    juce::Rectangle<int> spacePanelRect;
    juce::Rectangle<int> sfxPanelRect;
    juce::Rectangle<int> reverbPanelRect;
    juce::Rectangle<int> aiPanelRect;
    juce::Rectangle<int> delayPanelRect;
    juce::Rectangle<int> mixBarRect;

    // =========================================================================
    // APVTS Attachments — declared AFTER sliders so they are destroyed first
    // =========================================================================
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> colorVintageAttachment;
    std::unique_ptr<SliderAttachment> vibePhaserAttachment;
    std::unique_ptr<SliderAttachment> stereoWidthAttachment;
    std::unique_ptr<SliderAttachment> spaceReverbAttachment;
    std::unique_ptr<SliderAttachment> delayTextureAttachment;
    std::unique_ptr<SliderAttachment> mixWetAttachment;

    // =========================================================================
    // Helpers
    // =========================================================================
    void makeLabel      (juce::Label&,      const juce::String& text,
                         float fontSize,    juce::Colour colour,
                         juce::Justification just = juce::Justification::centred);

    void makeHSlider    (juce::Slider&, juce::Colour trackColour);
    void makeRotary     (juce::Slider&, juce::Colour fillColour);

    // Updates percent labels from current slider values
    void refreshValueLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfroplugAudioProcessorEditor)
};
