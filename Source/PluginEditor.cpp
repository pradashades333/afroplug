#include "PluginEditor.h"

//==============================================================================
// Colour palette
//==============================================================================
namespace AC
{
    const juce::Colour bg        { 0xff0d0d12 };   // global background
    const juce::Colour panelBg   { 0xff0d0d0d };   // flat module background
    const juce::Colour panelBg2  { 0xff111116 };   // mix bar background
    const juce::Colour gridLine  { 0xff1a1a1a };   // 1px separator / grid lines
    const juce::Colour textPri   { 0xfff0f0f0 };
    const juce::Colour textMuted { 0xff666677 };
    const juce::Colour textDim   { 0xff444455 };
    const juce::Colour red       { 0xfff25050 };   // EQ
    const juce::Colour cyan      { 0xff00ccff };   // TONE / DELAY
    const juce::Colour purple    { 0xffaa77ee };   // SPACE / REVERB
    const juce::Colour yellow    { 0xfff0c040 };   // SFX / AI / MIX
}

//==============================================================================
AfroplugAudioProcessorEditor::AfroplugAudioProcessorEditor (AfroplugAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Apply custom LookAndFeel to the whole editor
    setLookAndFeel (&customLaf);

    // =========================================================================
    // HEADER
    // =========================================================================
    makeLabel (headerTitleLabel, "AFROPLUG", 15.0f, AC::textPri,
               juce::Justification::centredLeft);
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
    presetCombo.setColour (juce::ComboBox::backgroundColourId, AC::panelBg);
    presetCombo.setColour (juce::ComboBox::outlineColourId,    AC::gridLine);
    presetCombo.setColour (juce::ComboBox::textColourId,       AC::textPri);
    presetCombo.setColour (juce::ComboBox::arrowColourId,      AC::textMuted);
    addAndMakeVisible (presetCombo);

    makeLabel (categoryLabel, "MELODY FX", 11.0f, AC::textMuted,
               juce::Justification::centredRight);
    categoryLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    // =========================================================================
    // TOP ROW — EQ (red)
    // =========================================================================
    makeLabel (eqTitleLabel, "EQ", 11.0f, AC::red, juce::Justification::centred);
    eqTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (eqSweepSlider, AC::red);
    // toneSubLabel / spaceSubLabel / sfxSubLabel / dynamicsSubLabel intentionally
    // NOT added — < Clear > / < Room > / < Wide > labels removed per spec

    // TOP ROW — TONE (cyan)
    makeLabel (toneTitleLabel, "TONE", 11.0f, AC::cyan, juce::Justification::centred);
    toneTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (colorVintageSlider, AC::cyan);

    // TOP ROW — SPACE (purple)
    makeLabel (spaceTitleLabel, "SPACE", 11.0f, AC::purple, juce::Justification::centred);
    spaceTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (vibePhaserSlider, AC::purple);

    // TOP ROW — SFX (yellow)
    makeLabel (sfxTitleLabel, "SFX", 11.0f, AC::yellow, juce::Justification::centred);
    sfxTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (stereoWidthSlider, AC::yellow);

    // =========================================================================
    // BOTTOM ROW — REVERB (purple)
    // =========================================================================
    makeLabel (reverbTitleLabel, "REVERB", 11.0f, AC::textMuted,
               juce::Justification::centred);
    makeRotary (spaceReverbSlider, AC::purple);
    // reverbValueLabel not displayed — TextBoxCenter handles the live value
    makeLabel  (reverbMinLabel, "0",   10.0f, AC::textDim, juce::Justification::centredLeft);
    makeLabel  (reverbMaxLabel, "100", 10.0f, AC::textDim, juce::Justification::centredRight);

    // BOTTOM ROW — AI button (circle, centre)
    aiButton.setButtonText ("AI");
    aiButton.setLookAndFeel (&aiButtonLaf);
    addAndMakeVisible (aiButton);

    makeLabel (aiAnalyzeLabel, "ANALYZE", 10.0f, AC::textMuted,
               juce::Justification::centred);
    aiAnalyzeLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));

    // BOTTOM ROW — DELAY (cyan)
    makeLabel (delayTitleLabel, "DELAY", 11.0f, AC::textMuted,
               juce::Justification::centred);
    makeRotary (delayTextureSlider, AC::cyan);
    // delayValueLabel not displayed — TextBoxCenter handles the live value
    makeLabel  (delayMinLabel, "0",   10.0f, AC::textDim, juce::Justification::centredLeft);
    makeLabel  (delayMaxLabel, "100", 10.0f, AC::textDim, juce::Justification::centredRight);

    // =========================================================================
    // MIX BAR (yellow)
    // =========================================================================
    makeLabel (mixLabel, "MIX", 11.0f, AC::textMuted, juce::Justification::centredLeft);
    mixLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (mixWetSlider, AC::yellow);
    mixWetSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    makeLabel (mixValueLabel, "100%", 11.0f, AC::textMuted,
               juce::Justification::centredRight);

    // =========================================================================
    // APVTS Attachments
    // =========================================================================
    auto& av = processorRef.apvts;
    eqSweepAttachment      = std::make_unique<SliderAttachment> (av, "eq_sweep",       eqSweepSlider);
    colorVintageAttachment = std::make_unique<SliderAttachment> (av, "color_vintage",  colorVintageSlider);
    vibePhaserAttachment   = std::make_unique<SliderAttachment> (av, "vibe_phaser",    vibePhaserSlider);
    stereoWidthAttachment  = std::make_unique<SliderAttachment> (av, "stereo_width",   stereoWidthSlider);
    spaceReverbAttachment  = std::make_unique<SliderAttachment> (av, "space_reverb",   spaceReverbSlider);
    delayTextureAttachment = std::make_unique<SliderAttachment> (av, "delay_texture",  delayTextureSlider);
    mixWetAttachment       = std::make_unique<SliderAttachment> (av, "mix_wet",        mixWetSlider);

    // Live mix value label
    mixWetSlider.onValueChange = [this]()
    {
        mixValueLabel.setText (
            juce::String ((int) std::round (mixWetSlider.getValue())) + "%",
            juce::dontSendNotification);
    };

    refreshValueLabels();

    // =========================================================================
    // AI button — randomises all 7 APVTS parameters
    // =========================================================================
    aiButton.onClick = [this]()
    {
        auto& rng    = juce::Random::getSystemRandom();
        auto& params = processorRef.apvts;
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

AfroplugAudioProcessorEditor::~AfroplugAudioProcessorEditor()
{
    // Clear LookAndFeel pointers before the LAF instances are destroyed
    aiButton.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void AfroplugAudioProcessorEditor::refreshValueLabels()
{
    mixValueLabel.setText (
        juce::String ((int) std::round (mixWetSlider.getValue())) + "%",
        juce::dontSendNotification);
}

//==============================================================================
// Helpers
//==============================================================================
void AfroplugAudioProcessorEditor::makeLabel (juce::Label& lbl,
                                               const juce::String& text,
                                               float fontSize,
                                               juce::Colour colour,
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
    s.setSliderStyle  (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1a1a1a));
    s.setColour (juce::Slider::trackColourId,      trackColour.withAlpha (0.8f));
    s.setColour (juce::Slider::thumbColourId,      trackColour);
    addAndMakeVisible (s);
}

void AfroplugAudioProcessorEditor::makeRotary (juce::Slider& s, juce::Colour fillColour)
{
    s.setSliderStyle  (juce::Slider::RotaryHorizontalVerticalDrag);
    // NoTextBox — value is drawn directly inside the arc by drawRotarySlider
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId,    fillColour);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252530));
    addAndMakeVisible (s);
}

//==============================================================================
void AfroplugAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w        = getWidth();
    const int topRowY  = 56;
    const int topRowH  = 188;
    const int botRowY  = topRowY + topRowH;
    const int botRowH  = 232;
    const int mixBarY  = botRowY + botRowH;

    // --- Global background ---
    g.fillAll (AC::bg);

    // --- Top row: flat dark fill, then vertical grid lines ---
    g.setColour (AC::panelBg);
    g.fillRect  (0, topRowY, w, topRowH);

    g.setColour (AC::gridLine);
    g.fillRect  (w / 4,     topRowY, 1, topRowH);
    g.fillRect  (w / 2,     topRowY, 1, topRowH);
    g.fillRect  (3 * w / 4, topRowY, 1, topRowH);

    // --- Bottom row: flat dark fill + vertical dividers ---
    g.setColour (AC::panelBg);
    g.fillRect  (0, botRowY, w, botRowH);

    const int sideW = w * 35 / 100;
    g.setColour (AC::gridLine);
    g.fillRect  (sideW,     botRowY, 1, botRowH);
    g.fillRect  (w - sideW, botRowY, 1, botRowH);

    // --- Horizontal section separators ---
    g.setColour (AC::gridLine);
    g.fillRect  (0, topRowY, w, 1);
    g.fillRect  (0, botRowY, w, 1);
    g.fillRect  (0, mixBarY, w, 1);

    // --- Mix bar background ---
    g.setColour (AC::panelBg2);
    g.fillRect  (mixBarRect);

    // =========================================================================
    // Top-row module icons + preset selector text
    //
    // Layout maths (derived from resized()):
    //   panelRect = col.reduced(6, 6)            → height 176 px
    //   inner     = panelRect.reduced(10, 8)     → height 160 px
    //   title:    inner top 22 px + 8 px gap     → content starts 38 px below panelRect.top
    //   slider:   inner bottom 30 px             → content ends   38 px above panelRect.bottom
    //   Preset text (14 px):  4 px above slider start → panelRect.bottom − 56
    //   Icon centre Y:        centre of [+38 … −60]   → panelRect.top  + 77
    // =========================================================================

    // Helper: draw "< Word >" with dim arrows and lighter centre word
    auto drawPresetText = [&](const juce::Rectangle<int>& panel,
                               const juce::String& word)
    {
        const juce::Rectangle<float> tr {
            (float)(panel.getX() + 10),
            (float)(panel.getBottom() - 56),
            (float)(panel.getWidth() - 20),
            14.0f
        };
        juce::AttributedString str;
        str.setJustification (juce::Justification::centred);
        str.append ("< ",  juce::Font (juce::FontOptions (10.0f)), juce::Colour (0xff444455));
        str.append (word,  juce::Font (juce::FontOptions (10.0f)), juce::Colour (0xff666677));
        str.append (" >",  juce::Font (juce::FontOptions (10.0f)), juce::Colour (0xff444455));
        str.draw (g, tr);
    };

    const juce::PathStrokeType stroke2 (2.0f, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded);

    // --- EQ: sine-wave path (red 0xfff25050) ---
    {
        drawPresetText (eqPanelRect, "Highpass");
        const float icx = (float)eqPanelRect.getCentreX();
        const float icy = (float)(eqPanelRect.getY() + 77);

        juce::Path wave;
        constexpr int pts = 48;
        for (int i = 0; i <= pts; i++)
        {
            const float t = (float)i / pts;
            const float x = icx - 28.0f + 56.0f * t;
            const float y = icy - 12.0f * std::sin (t * 4.0f * juce::MathConstants<float>::pi);
            if (i == 0) wave.startNewSubPath (x, y);
            else         wave.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xfff25050));
        g.strokePath (wave, stroke2);
    }

    // --- TONE: knob circle + centre dot + 12-o'clock pointer (light blue 0xff3cb4d6) ---
    {
        drawPresetText (tonePanelRect, "Clear");
        const float icx = (float)tonePanelRect.getCentreX();
        const float icy = (float)(tonePanelRect.getY() + 77);
        const float r   = 18.0f;

        g.setColour (juce::Colour (0xff3cb4d6));
        g.drawEllipse (icx - r, icy - r, r * 2.0f, r * 2.0f, 2.0f);   // outer ring
        g.drawLine    (icx, icy - 4.0f, icx, icy - r + 2.0f, 2.0f);   // pointer up
        g.fillEllipse (icx - 4.0f, icy - 4.0f, 8.0f, 8.0f);           // centre dot (drawn last)
    }

    // --- SPACE: small filled square + larger rounded outer square (purple 0xffa173f2) ---
    {
        drawPresetText (spacePanelRect, "Room");
        const float icx = (float)spacePanelRect.getCentreX();
        const float icy = (float)(spacePanelRect.getY() + 77);

        g.setColour (juce::Colour (0xffa173f2));
        g.drawRoundedRectangle (icx - 18.0f, icy - 18.0f, 36.0f, 36.0f, 4.0f, 2.0f);  // outer
        g.fillRect (juce::Rectangle<float> (icx - 5.0f, icy - 5.0f, 10.0f, 10.0f));    // inner dot
    }

    // --- SFX: Venn-diagram overlapping circles (yellow 0xffffd600) ---
    {
        drawPresetText (sfxPanelRect, "Wide");
        const float icx = (float)sfxPanelRect.getCentreX();
        const float icy = (float)(sfxPanelRect.getY() + 77);
        const float r   = 14.0f;
        const float off = 9.0f;

        g.setColour (juce::Colour (0xffffd600));
        g.drawEllipse (icx - off - r, icy - r, r * 2.0f, r * 2.0f, 2.0f);   // left circle
        g.drawEllipse (icx + off - r, icy - r, r * 2.0f, r * 2.0f, 2.0f);   // right circle
    }
}

//==============================================================================
void AfroplugAudioProcessorEditor::resized()
{
    auto full = getLocalBounds();

    const int headerH = 56;
    const int topRowH = 188;
    const int botRowH = 232;

    auto headerArea = full.removeFromTop (headerH);
    auto topRowArea = full.removeFromTop (topRowH);
    auto botRowArea = full.removeFromTop (botRowH);
    auto mixArea    = full;

    mixBarRect = mixArea;

    // =========================================================================
    // HEADER
    // =========================================================================
    {
        auto h = headerArea.reduced (16, 0);
        headerTitleLabel.setBounds (h.removeFromLeft  (130));
        categoryLabel.setBounds    (h.removeFromRight (130));
        h.reduce (8, 0);
        prevPresetBtn.setBounds (h.removeFromLeft  (28));
        h.removeFromLeft (4);
        nextPresetBtn.setBounds (h.removeFromRight (28));
        h.removeFromRight (4);
        presetCombo.setBounds (h.withSizeKeepingCentre (
                                   juce::jmin (220, h.getWidth()), 26));
    }

    // =========================================================================
    // TOP ROW — 4 equal columns, sub-labels removed
    // =========================================================================
    {
        const int colW = topRowArea.getWidth() / 4;
        const int pad  = 6;

        // Simplified layout: title at top, slider in lower third — no sub-label space
        auto layoutTopPanel = [&](juce::Rectangle<int> col,
                                  juce::Rectangle<int>& panelRectOut,
                                  juce::Label& titleLbl,
                                  juce::Slider& slider)
        {
            panelRectOut = col.reduced (pad, pad);
            auto inner   = panelRectOut.reduced (10, 8);
            titleLbl.setBounds (inner.removeFromTop (22));
            inner.removeFromTop (8);
            slider.setBounds (inner.removeFromBottom (30));
        };

        auto col0 = topRowArea.removeFromLeft (colW);
        auto col1 = topRowArea.removeFromLeft (colW);
        auto col2 = topRowArea.removeFromLeft (colW);
        auto col3 = topRowArea;

        layoutTopPanel (col0, eqPanelRect,    eqTitleLabel,    eqSweepSlider);
        layoutTopPanel (col1, tonePanelRect,  toneTitleLabel,  colorVintageSlider);
        layoutTopPanel (col2, spacePanelRect, spaceTitleLabel, vibePhaserSlider);
        layoutTopPanel (col3, sfxPanelRect,   sfxTitleLabel,   stereoWidthSlider);
    }

    // =========================================================================
    // BOTTOM ROW — Reverb | AI | Delay  (35 : 30 : 35)
    // =========================================================================
    {
        const int sideW = (botRowArea.getWidth() * 35) / 100;
        auto reverbArea = botRowArea.removeFromLeft  (sideW);
        auto delayArea  = botRowArea.removeFromRight (sideW);
        auto aiArea     = botRowArea;

        auto layoutKnobPanel = [&](juce::Rectangle<int> area,
                                   juce::Rectangle<int>& panelOut,
                                   juce::Label& titleLbl,
                                   juce::Slider& knob,
                                   juce::Label& minLbl,
                                   juce::Label& maxLbl)
        {
            panelOut   = area.reduced (6, 6);
            auto inner = panelOut.reduced (12, 10);

            titleLbl.setBounds (inner.removeFromTop (22));
            inner.removeFromTop (4);

            auto bottomStrip = inner.removeFromBottom (18);
            minLbl.setBounds (bottomStrip.removeFromLeft  (40));
            maxLbl.setBounds (bottomStrip.removeFromRight (40));
            inner.removeFromBottom (4);

            const int knobSize = juce::jmin (inner.getWidth(), inner.getHeight(), 150);
            knob.setBounds (inner.withSizeKeepingCentre (knobSize, knobSize));
        };

        layoutKnobPanel (reverbArea, reverbPanelRect,
                         reverbTitleLabel, spaceReverbSlider,
                         reverbMinLabel, reverbMaxLabel);

        layoutKnobPanel (delayArea, delayPanelRect,
                         delayTitleLabel, delayTextureSlider,
                         delayMinLabel, delayMaxLabel);

        // AI panel
        aiPanelRect  = aiArea.reduced (6, 6);
        auto aiInner = aiPanelRect.reduced (16, 12);
        aiAnalyzeLabel.setBounds (aiInner.removeFromBottom (22));
        aiButton.setBounds (aiInner);
    }

    // =========================================================================
    // MIX BAR
    // =========================================================================
    {
        auto h = mixArea.reduced (16, 0);
        mixLabel.setBounds      (h.removeFromLeft  (52));
        h.removeFromLeft (6);
        mixValueLabel.setBounds (h.removeFromRight (52));
        h.removeFromRight (6);
        mixWetSlider.setBounds  (h);
    }
}
