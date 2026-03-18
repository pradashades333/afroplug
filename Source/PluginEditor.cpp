#include "PluginEditor.h"

//==============================================================================
// Colour palette — swap these constants when the custom Figma skin lands
//==============================================================================
namespace AC   // Afroplug Colours
{
    const juce::Colour bg         { 0xff0d0d12 };   // near-black background
    const juce::Colour panelBg    { 0xff141419 };   // card surface
    const juce::Colour panelBg2   { 0xff111116 };   // slightly deeper (bottom panels)
    const juce::Colour border     { 0xff252530 };   // panel outline
    const juce::Colour separator  { 0xff1e1e28 };   // section divider
    const juce::Colour textPri    { 0xfff0f0f0 };   // primary white
    const juce::Colour textMuted  { 0xff666677 };   // subdued / footer text
    const juce::Colour textDim    { 0xff444455 };   // min/max labels
    // Per-module accent colours (matching the design)
    const juce::Colour red        { 0xffe05050 };   // DYNAMICS
    const juce::Colour cyan       { 0xff00ccff };   // TONE / DELAY
    const juce::Colour purple     { 0xffaa77ee };   // SPACE / REVERB
    const juce::Colour yellow     { 0xfff0c040 };   // SFX / AI / MIX
}

//==============================================================================
AfroplugAudioProcessorEditor::AfroplugAudioProcessorEditor (AfroplugAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // =========================================================================
    // HEADER
    // =========================================================================
    makeLabel (headerTitleLabel, "AFROPLUG", 15.0f, AC::textPri, juce::Justification::centredLeft);
    headerTitleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));

    prevPresetBtn.setButtonText ("<");
    prevPresetBtn.setColour (juce::TextButton::buttonColourId,  AC::panelBg);
    prevPresetBtn.setColour (juce::TextButton::textColourOffId, AC::textMuted);
    addAndMakeVisible (prevPresetBtn);

    nextPresetBtn.setButtonText (">");
    nextPresetBtn.setColour (juce::TextButton::buttonColourId,  AC::panelBg);
    nextPresetBtn.setColour (juce::TextButton::textColourOffId, AC::textMuted);
    addAndMakeVisible (nextPresetBtn);

    presetCombo.addItem ("Lagos Nights", 1);
    presetCombo.setSelectedId (1, juce::dontSendNotification);
    presetCombo.setJustificationType (juce::Justification::centred);
    presetCombo.setColour (juce::ComboBox::backgroundColourId,   AC::panelBg);
    presetCombo.setColour (juce::ComboBox::outlineColourId,      AC::border);
    presetCombo.setColour (juce::ComboBox::textColourId,         AC::textPri);
    presetCombo.setColour (juce::ComboBox::arrowColourId,        AC::textMuted);
    addAndMakeVisible (presetCombo);

    makeLabel (categoryLabel, "MELODY FX", 11.0f, AC::textMuted, juce::Justification::centredRight);
    categoryLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    // =========================================================================
    // TOP ROW — EQ  (eq_sweep, red)
    // =========================================================================
    makeLabel (eqTitleLabel, "EQ", 11.0f, AC::red, juce::Justification::centred);
    eqTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (eqSweepSlider, juce::Colour (0xfff25050));

    // TOP ROW — TONE  (color_vintage, cyan)
    makeLabel (toneTitleLabel, "TONE", 11.0f, AC::cyan, juce::Justification::centred);
    toneTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeLabel (toneSubLabel, "< Clear >", 11.0f, AC::textMuted, juce::Justification::centred);
    makeHSlider (colorVintageSlider, AC::cyan);

    // TOP ROW — SPACE  (vibe_phaser, purple)
    makeLabel (spaceTitleLabel, "SPACE", 11.0f, AC::purple, juce::Justification::centred);
    spaceTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeLabel (spaceSubLabel, "< Room >", 11.0f, AC::textMuted, juce::Justification::centred);
    makeHSlider (vibePhaserSlider, AC::purple);

    // TOP ROW — SFX  (stereo_width, yellow)
    makeLabel (sfxTitleLabel, "SFX", 11.0f, AC::yellow, juce::Justification::centred);
    sfxTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeLabel (sfxSubLabel, "< Wide >", 11.0f, AC::textMuted, juce::Justification::centred);
    makeHSlider (stereoWidthSlider, AC::yellow);

    // =========================================================================
    // BOTTOM ROW — REVERB  (space_reverb, purple)
    // =========================================================================
    makeLabel (reverbTitleLabel, "REVERB", 11.0f, AC::textMuted, juce::Justification::centred);
    makeRotary (spaceReverbSlider, AC::purple);
    makeLabel  (reverbValueLabel, "28%", 13.0f, AC::textPri,  juce::Justification::centred);
    makeLabel  (reverbMinLabel,   "0",   10.0f, AC::textDim,  juce::Justification::centredLeft);
    makeLabel  (reverbMaxLabel,   "100", 10.0f, AC::textDim,  juce::Justification::centredRight);

    // BOTTOM ROW — AI  (center large button)
    aiButton.setButtonText ("★  AI");
    aiButton.setColour (juce::TextButton::buttonColourId,    AC::panelBg2);
    aiButton.setColour (juce::TextButton::buttonOnColourId,  AC::panelBg2);
    aiButton.setColour (juce::TextButton::textColourOffId,   AC::yellow);
    aiButton.setColour (juce::TextButton::textColourOnId,    AC::yellow.brighter (0.2f));
    addAndMakeVisible (aiButton);

    makeLabel (aiAnalyzeLabel, "ANALYZE", 10.0f, AC::textMuted, juce::Justification::centred);
    aiAnalyzeLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));

    // BOTTOM ROW — DELAY  (delay_texture, cyan)
    makeLabel (delayTitleLabel, "DELAY", 11.0f, AC::textMuted, juce::Justification::centred);
    makeRotary (delayTextureSlider, AC::cyan);
    makeLabel  (delayValueLabel, "22%", 13.0f, AC::textPri,  juce::Justification::centred);
    makeLabel  (delayMinLabel,   "0",   10.0f, AC::textDim,  juce::Justification::centredLeft);
    makeLabel  (delayMaxLabel,   "100", 10.0f, AC::textDim,  juce::Justification::centredRight);

    // =========================================================================
    // MIX BAR  (mix_wet, yellow/white)
    // =========================================================================
    makeLabel (mixLabel, "MIX", 11.0f, AC::textMuted, juce::Justification::centredLeft);
    mixLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (mixWetSlider, AC::yellow);
    mixWetSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    makeLabel (mixValueLabel, "62%", 11.0f, AC::textMuted, juce::Justification::centredRight);

    // =========================================================================
    // APVTS Attachments — created AFTER all sliders are configured
    // =========================================================================
    auto& av = processorRef.apvts;
    eqSweepAttachment      = std::make_unique<SliderAttachment> (av, "eq_sweep",       eqSweepSlider);
    colorVintageAttachment = std::make_unique<SliderAttachment> (av, "color_vintage", colorVintageSlider);
    vibePhaserAttachment   = std::make_unique<SliderAttachment> (av, "vibe_phaser",   vibePhaserSlider);
    stereoWidthAttachment  = std::make_unique<SliderAttachment> (av, "stereo_width",  stereoWidthSlider);
    spaceReverbAttachment  = std::make_unique<SliderAttachment> (av, "space_reverb",  spaceReverbSlider);
    delayTextureAttachment = std::make_unique<SliderAttachment> (av, "delay_texture", delayTextureSlider);
    mixWetAttachment       = std::make_unique<SliderAttachment> (av, "mix_wet",       mixWetSlider);

    // Live value-label updates — lambdas fire whenever the slider moves
    // (via host automation, APVTS, or direct drag)
    spaceReverbSlider.onValueChange = [this]()
    {
        reverbValueLabel.setText (juce::String ((int) std::round (spaceReverbSlider.getValue())) + "%",
                                  juce::dontSendNotification);
    };
    delayTextureSlider.onValueChange = [this]()
    {
        delayValueLabel.setText (juce::String ((int) std::round (delayTextureSlider.getValue())) + "%",
                                 juce::dontSendNotification);
    };
    mixWetSlider.onValueChange = [this]()
    {
        mixValueLabel.setText (juce::String ((int) std::round (mixWetSlider.getValue())) + "%",
                               juce::dontSendNotification);
    };

    // Sync initial display to whatever value the APVTS loaded from state
    refreshValueLabels();

    // =========================================================================
    // AI button — randomises all 6 APVTS params as a proof of concept
    // =========================================================================
    aiButton.onClick = [this]()
    {
        auto& rng    = juce::Random::getSystemRandom();
        auto& params = processorRef.apvts;
        // setValueNotifyingHost takes normalised [0,1] → maps to full 0–100 range
        params.getParameter ("eq_sweep")      ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("color_vintage") ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("vibe_phaser")   ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("stereo_width")  ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("space_reverb")  ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("delay_texture") ->setValueNotifyingHost (rng.nextFloat());
        params.getParameter ("mix_wet")       ->setValueNotifyingHost (rng.nextFloat());
    };

    setSize (900, 528);
}

AfroplugAudioProcessorEditor::~AfroplugAudioProcessorEditor() {}

//==============================================================================
void AfroplugAudioProcessorEditor::refreshValueLabels()
{
    reverbValueLabel.setText (juce::String ((int) std::round (spaceReverbSlider.getValue()))  + "%",
                              juce::dontSendNotification);
    delayValueLabel.setText  (juce::String ((int) std::round (delayTextureSlider.getValue())) + "%",
                              juce::dontSendNotification);
    mixValueLabel.setText    (juce::String ((int) std::round (mixWetSlider.getValue()))       + "%",
                              juce::dontSendNotification);
}

//==============================================================================
// Helpers
//==============================================================================
void AfroplugAudioProcessorEditor::makeLabel (juce::Label& lbl, const juce::String& text,
                                               float fontSize, juce::Colour colour,
                                               juce::Justification just)
{
    lbl.setText (text, juce::dontSendNotification);
    lbl.setFont (juce::FontOptions (fontSize));
    lbl.setColour (juce::Label::textColourId, colour);
    lbl.setJustificationType (just);
    addAndMakeVisible (lbl);
}

void AfroplugAudioProcessorEditor::makeHSlider (juce::Slider& s, juce::Colour trackColour)
{
    s.setSliderStyle    (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle   (juce::Slider::NoTextBox, true, 0, 0);
    s.setColour (juce::Slider::backgroundColourId,  AC::border);
    s.setColour (juce::Slider::trackColourId,        trackColour.withAlpha (0.7f));
    s.setColour (juce::Slider::thumbColourId,        trackColour);
    addAndMakeVisible (s);
}

void AfroplugAudioProcessorEditor::makeRotary (juce::Slider& s, juce::Colour fillColour)
{
    s.setSliderStyle    (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle   (juce::Slider::NoTextBox, true, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId,    fillColour);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, AC::border);
    s.setColour (juce::Slider::thumbColourId,               fillColour.brighter (0.2f));
    addAndMakeVisible (s);
}

//==============================================================================
void AfroplugAudioProcessorEditor::paint (juce::Graphics& g)
{
    // --- Background ---
    g.fillAll (AC::bg);

    // --- Section separator lines ---
    auto drawSep = [&](int y)
    {
        g.setColour (AC::separator);
        g.fillRect (0, y, getWidth(), 1);
    };

    const int topRowY    = 56;
    const int botRowY    = topRowY + 188;
    const int mixBarY    = botRowY + 232;

    drawSep (topRowY);
    drawSep (botRowY);
    drawSep (mixBarY);

    // --- Panel backgrounds + borders (top row) ---
    auto drawPanel = [&](const juce::Rectangle<int>& r, juce::Colour accent)
    {
        g.setColour (AC::panelBg);
        g.fillRoundedRectangle (r.toFloat().reduced (1.0f), 4.0f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 4.0f, 1.0f);
    };

    drawPanel (eqPanelRect, AC::red);
    drawPanel (tonePanelRect,     AC::cyan);
    drawPanel (spacePanelRect,    AC::purple);
    drawPanel (sfxPanelRect,      AC::yellow);

    // --- Panel backgrounds + borders (bottom row) ---
    auto drawBotPanel = [&](const juce::Rectangle<int>& r, juce::Colour accent)
    {
        g.setColour (AC::panelBg2);
        g.fillRoundedRectangle (r.toFloat().reduced (1.0f), 6.0f);
        g.setColour (accent.withAlpha (0.25f));
        g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 6.0f, 1.0f);
    };

    drawBotPanel (reverbPanelRect, AC::purple);
    drawBotPanel (delayPanelRect,  AC::cyan);

    // AI panel — slightly lighter surface, no strong accent border
    g.setColour (AC::panelBg);
    g.fillRoundedRectangle (aiPanelRect.toFloat().reduced (1.0f), 6.0f);
    g.setColour (AC::border);
    g.drawRoundedRectangle (aiPanelRect.toFloat().reduced (1.0f), 6.0f, 1.0f);

    // --- Mix bar background ---
    g.setColour (AC::panelBg2);
    g.fillRect (mixBarRect);

    // --- Plugin title font override (bold via paint for better kerning) ---
    g.setColour (AC::textPri);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
}

//==============================================================================
void AfroplugAudioProcessorEditor::resized()
{
    auto full = getLocalBounds();

    // =========================================================================
    // Section strips
    // =========================================================================
    const int headerH  = 56;
    const int topRowH  = 188;
    const int botRowH  = 232;
    const int mixBarH  = 52;
    // remaining = footer

    auto headerArea  = full.removeFromTop (headerH);
    auto topRowArea  = full.removeFromTop (topRowH);
    auto botRowArea  = full.removeFromTop (botRowH);
    auto mixArea     = full;   // fills remaining height

    mixBarRect = mixArea;

    // =========================================================================
    // HEADER
    // =========================================================================
    {
        auto h = headerArea.reduced (16, 0);

        // Brand (left)
        headerTitleLabel.setBounds (h.removeFromLeft (130));

        // Category (right)
        categoryLabel.setBounds (h.removeFromRight (130));

        // Preset nav (centre of whatever remains)
        h.reduce (8, 0);
        prevPresetBtn.setBounds (h.removeFromLeft  (28));
        h.removeFromLeft (4);
        nextPresetBtn.setBounds (h.removeFromRight (28));
        h.removeFromRight (4);
        presetCombo.setBounds (h.withSizeKeepingCentre (
                                   juce::jmin (220, h.getWidth()), 26));
    }

    // =========================================================================
    // TOP ROW — 4 equal columns
    // =========================================================================
    {
        const int colW  = topRowArea.getWidth() / 4;
        const int pad   = 6;    // inset from column edge

        auto layoutTopPanel = [&](juce::Rectangle<int> col,
                                  juce::Rectangle<int>& panelRectOut,
                                  juce::Label& titleLbl,
                                  juce::Label& subLbl,
                                  juce::Slider* slider)   // nullptr for DYNAMICS
        {
            panelRectOut = col.reduced (pad, pad);
            auto inner   = panelRectOut.reduced (10, 8);

            // Title at top
            titleLbl.setBounds (inner.removeFromTop (22));
            inner.removeFromTop (4);

            if (slider != nullptr)
            {
                // Sub-label in middle (< Type >)
                subLbl.setBounds (inner.removeFromTop (18));
                inner.removeFromTop (4);
                // Slider fills remaining space (bottom strip ~30 px)
                auto sliderArea = inner.removeFromBottom (30);
                slider->setBounds (sliderArea);
            }
            else
            {
                // DYNAMICS: placeholder sub-label only, centred in remaining
                subLbl.setBounds (inner.withSizeKeepingCentre (inner.getWidth(), 18));
            }
        };

        auto col0 = topRowArea.removeFromLeft (colW);
        auto col1 = topRowArea.removeFromLeft (colW);
        auto col2 = topRowArea.removeFromLeft (colW);
        auto col3 = topRowArea; // remaining (== colW or colW±1 due to integer division)

        layoutTopPanel (col0, eqPanelRect, eqTitleLabel, dynamicsSubLabel, &eqSweepSlider);
        layoutTopPanel (col1, tonePanelRect,     toneTitleLabel,     toneSubLabel,     &colorVintageSlider);
        layoutTopPanel (col2, spacePanelRect,    spaceTitleLabel,    spaceSubLabel,    &vibePhaserSlider);
        layoutTopPanel (col3, sfxPanelRect,      sfxTitleLabel,      sfxSubLabel,      &stereoWidthSlider);
    }

    // =========================================================================
    // BOTTOM ROW — Reverb | AI | Delay  (35 : 30 : 35 split)
    // =========================================================================
    {
        const int sideW = (botRowArea.getWidth() * 35) / 100;   // ≈315 px
        auto reverbArea = botRowArea.removeFromLeft  (sideW);
        auto delayArea  = botRowArea.removeFromRight (sideW);
        auto aiArea     = botRowArea;                            // ≈270 px

        // Helper: lay out a labelled rotary knob panel
        auto layoutKnobPanel = [&](juce::Rectangle<int> area,
                                   juce::Rectangle<int>& panelOut,
                                   juce::Label& titleLbl,
                                   juce::Slider& knob,
                                   juce::Label& valueLbl,
                                   juce::Label& minLbl,
                                   juce::Label& maxLbl)
        {
            panelOut    = area.reduced (6, 6);
            auto inner  = panelOut.reduced (12, 10);

            // Title at top
            titleLbl.setBounds (inner.removeFromTop (22));
            inner.removeFromTop (2);

            // Min/max at the very bottom
            auto bottomStrip = inner.removeFromBottom (18);
            minLbl.setBounds (bottomStrip.removeFromLeft  (40));
            maxLbl.setBounds (bottomStrip.removeFromRight (40));

            // Value label (%) just above min/max
            valueLbl.setBounds (inner.removeFromBottom (22));
            inner.removeFromBottom (4);

            // Rotary knob centred in remaining space
            const int knobSize = juce::jmin (inner.getWidth(), inner.getHeight(), 150);
            knob.setBounds (inner.withSizeKeepingCentre (knobSize, knobSize));
        };

        layoutKnobPanel (reverbArea, reverbPanelRect,
                         reverbTitleLabel, spaceReverbSlider,
                         reverbValueLabel, reverbMinLabel, reverbMaxLabel);

        layoutKnobPanel (delayArea,  delayPanelRect,
                         delayTitleLabel,  delayTextureSlider,
                         delayValueLabel,  delayMinLabel, delayMaxLabel);

        // AI panel
        aiPanelRect = aiArea.reduced (6, 6);
        auto aiInner = aiPanelRect.reduced (16, 12);
        aiAnalyzeLabel.setBounds (aiInner.removeFromBottom (22));
        aiButton.setBounds (aiInner);
    }

    // =========================================================================
    // MIX BAR
    // =========================================================================
    {
        auto h = mixArea.reduced (16, 0);

        mixLabel.setBounds       (h.removeFromLeft  (52));
        h.removeFromLeft (6);
        mixValueLabel.setBounds  (h.removeFromRight (52));
        h.removeFromRight (6);
        mixWetSlider.setBounds   (h);
    }

}
