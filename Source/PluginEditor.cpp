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
    makeLabel (headerTitleLabel, "SOUL FX", 15.0f, AC::textPri,
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

    // Populate from disk
    {
        auto presets = processorRef.getAvailablePresets();
        for (int i = 0; i < presets.size(); ++i)
            presetCombo.addItem (presets[i], i + 1);
        if (presets.isEmpty())
            presetCombo.addItem ("(no presets)", 1);
        presetCombo.setSelectedId (1, juce::dontSendNotification);
    }
    presetCombo.setJustificationType (juce::Justification::centred);
    presetCombo.setColour (juce::ComboBox::backgroundColourId, AC::panelBg);
    presetCombo.setColour (juce::ComboBox::outlineColourId,    AC::gridLine);
    presetCombo.setColour (juce::ComboBox::textColourId,       AC::textPri);
    presetCombo.setColour (juce::ComboBox::arrowColourId,      AC::textMuted);
    presetCombo.onChange = [this]()
    {
        const juce::String name = presetCombo.getText();
        if (name != "(no presets)")
            processorRef.loadPreset (name);
    };
    addAndMakeVisible (presetCombo);

    prevPresetBtn.onClick = [this]()
    {
        const int cur = presetCombo.getSelectedId();
        if (cur > 1) presetCombo.setSelectedId (cur - 1);
    };
    nextPresetBtn.onClick = [this]()
    {
        const int cur   = presetCombo.getSelectedId();
        const int total = presetCombo.getNumItems();
        if (cur < total) presetCombo.setSelectedId (cur + 1);
    };

    savePresetBtn.setButtonText ("Save");
    savePresetBtn.setColour (juce::TextButton::buttonColourId,  AC::panelBg);
    savePresetBtn.setColour (juce::TextButton::textColourOffId, AC::textMuted);
    savePresetBtn.onClick = [this]()
    {
        auto* alertWin = new juce::AlertWindow ("Save Preset",
                                                "Enter a name for the preset:",
                                                juce::MessageBoxIconType::NoIcon);
        alertWin->addTextEditor ("name", "", "Preset name:");
        alertWin->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        alertWin->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        alertWin->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, alertWin] (int result)
            {
                if (result == 1)
                {
                    const juce::String name =
                        alertWin->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty())
                    {
                        processorRef.savePreset (name);

                        // Refresh combo list
                        presetCombo.clear (juce::dontSendNotification);
                        auto presets = processorRef.getAvailablePresets();
                        for (int i = 0; i < presets.size(); ++i)
                            presetCombo.addItem (presets[i], i + 1);

                        const int idx = presets.indexOf (name);
                        if (idx >= 0)
                            presetCombo.setSelectedId (idx + 1, juce::dontSendNotification);
                    }
                }
            }),
            true);
    };
    addAndMakeVisible (savePresetBtn);

    makeLabel (categoryLabel, "AFROPLUG", 11.0f, AC::textMuted,
               juce::Justification::centredRight);
    categoryLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    // =========================================================================
    // TOP ROW — EQ (red)
    // =========================================================================
    makeLabel (eqTitleLabel, "EQ", 11.0f, AC::red, juce::Justification::centred);
    eqTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (eqSweepSlider, AC::red);

    eqModeSelector.addItem ("Low Cut",    1);
    eqModeSelector.addItem ("High Cut",   2);
    eqModeSelector.addItem ("Vocal",      3);
    eqModeSelector.addItem ("Radio",      4);
    eqModeSelector.addItem ("Underwater", 5);
    eqModeSelector.setSelectedId (1, juce::dontSendNotification);
    eqModeSelector.setJustificationType (juce::Justification::centred);
    eqModeSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d12));
    eqModeSelector.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff1a1a1a));
    eqModeSelector.setColour (juce::ComboBox::textColourId,       juce::Colour (0xff666677));
    eqModeSelector.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xff444455));
    eqModeSelector.onChange = [this]() { repaint(); };
    addAndMakeVisible (eqModeSelector);

    // TOP ROW — TONE (cyan)
    makeLabel (toneTitleLabel, "TONE", 11.0f, AC::cyan, juce::Justification::centred);
    toneTitleLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    makeHSlider (colorVintageSlider, AC::cyan);

    toneModeSelector.addItem ("Clear",   1);
    toneModeSelector.addItem ("Tape",    2);
    toneModeSelector.addItem ("Tube",    3);
    toneModeSelector.addItem ("Console", 4);
    toneModeSelector.addItem ("Grit",    5);
    toneModeSelector.setSelectedId (1, juce::dontSendNotification);
    toneModeSelector.setJustificationType (juce::Justification::centred);
    toneModeSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d12));
    toneModeSelector.setColour (juce::ComboBox::outlineColourId,    AC::cyan);
    toneModeSelector.setColour (juce::ComboBox::textColourId,       AC::cyan);
    toneModeSelector.setColour (juce::ComboBox::arrowColourId,      AC::cyan);
    addAndMakeVisible (toneModeSelector);

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
    makeLabel  (reverbMinLabel, "0",   10.0f, AC::textDim, juce::Justification::centredLeft);
    makeLabel  (reverbMaxLabel, "100", 10.0f, AC::textDim, juce::Justification::centredRight);

    reverbModeSelector.addItem ("Studio",  1);
    reverbModeSelector.addItem ("Plate",   2);
    reverbModeSelector.addItem ("Chamber", 3);
    reverbModeSelector.addItem ("Hall",    4);
    reverbModeSelector.addItem ("Abyss",   5);
    reverbModeSelector.setSelectedId (1, juce::dontSendNotification);
    reverbModeSelector.setJustificationType (juce::Justification::centred);
    reverbModeSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d12));
    reverbModeSelector.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xffaa77ee));
    reverbModeSelector.setColour (juce::ComboBox::textColourId,       juce::Colour (0xffaa77ee));
    reverbModeSelector.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xffaa77ee));
    toneModeSelector.onChange  = [this]() { repaint(); };
    reverbModeSelector.onChange = [this]() { repaint(); };
    addAndMakeVisible (reverbModeSelector);

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
    eqSweepAttachment      = std::make_unique<SliderAttachment>   (av, "eq_sweep",       eqSweepSlider);
    eqModeAttachment       = std::make_unique<ComboBoxAttachment> (av, "eq_mode",        eqModeSelector);
    toneModeAttachment     = std::make_unique<ComboBoxAttachment> (av, "tone_mode",      toneModeSelector);
    reverbModeAttachment   = std::make_unique<ComboBoxAttachment> (av, "reverb_mode",    reverbModeSelector);
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
    // AI button — analyse live audio and apply smart preset
    // =========================================================================
    aiButton.onClick = [this]() { processorRef.triggerAIAnalysis(); };

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

    // =========================================================================
    // Reverb mode icon (bottom-row reverb panel, top area)
    // =========================================================================
    auto drawReverbModeIcon = [&] (int mode, float cx, float cy, float sz,
                                   juce::Colour colour)
    {
        using MP = juce::MathConstants<float>;
        g.setColour (colour);
        const float r = sz * 0.5f;

        switch (mode)
        {
            case 0: // Studio — rounded hollow square + centred solid square
            {
                juce::Path outer;
                const float os = sz * 0.85f;
                outer.addRoundedRectangle (cx - os*0.5f, cy - os*0.5f, os, os, sz*0.14f);
                g.strokePath (outer, juce::PathStrokeType (1.8f));
                const float ds = sz * 0.28f;
                g.fillRect (juce::Rectangle<float> (cx - ds*0.5f, cy - ds*0.5f, ds, ds));
                break;
            }
            case 1: // Plate — slanted parallelogram + 2 hollow circles at top corners
            {
                const float skew = sz*0.18f, hw = sz*0.42f, hh = sz*0.40f;
                juce::Path para;
                para.startNewSubPath (cx - hw + skew, cy - hh);
                para.lineTo          (cx + hw + skew, cy - hh);
                para.lineTo          (cx + hw - skew, cy + hh);
                para.lineTo          (cx - hw - skew, cy + hh);
                para.closeSubPath();
                g.strokePath (para, juce::PathStrokeType (1.8f,
                    juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
                const float dr = sz * 0.09f;
                g.drawEllipse (cx - hw + skew - dr, cy - hh - dr, dr*2.0f, dr*2.0f, 1.5f);
                g.drawEllipse (cx + hw + skew - dr, cy - hh - dr, dr*2.0f, dr*2.0f, 1.5f);
                break;
            }
            case 2: // Chamber — two concentric hollow hexagons
            {
                auto hexPath = [&] (float hexR)
                {
                    juce::Path hex;
                    for (int i = 0; i < 6; ++i)
                    {
                        const float a  = MP::pi * 0.5f + (float)i * MP::twoPi / 6.0f;
                        const float px = cx + hexR * std::cos (a);
                        const float py = cy - hexR * std::sin (a);
                        if (i == 0) hex.startNewSubPath (px, py);
                        else        hex.lineTo (px, py);
                    }
                    hex.closeSubPath();
                    return hex;
                };
                g.strokePath (hexPath (r * 0.93f), juce::PathStrokeType (1.8f));
                g.strokePath (hexPath (r * 0.55f), juce::PathStrokeType (1.4f));
                break;
            }
            case 3: // Hall — 3 nested upward archways
            {
                const float baseY = cy + r * 0.45f;
                for (float archR : { r*0.30f, r*0.58f, r*0.88f })
                {
                    juce::Path arch;
                    arch.addCentredArc (cx, baseY, archR, archR, 0.0f,
                                        MP::pi, MP::twoPi, true);
                    g.strokePath (arch, juce::PathStrokeType (1.8f,
                        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                break;
            }
            case 4: // Abyss — solid dot + 2 dashed concentric circles
            {
                g.fillEllipse (cx - sz*0.075f, cy - sz*0.075f, sz*0.15f, sz*0.15f);
                for (float circleR : { r*0.40f, r*0.82f })
                {
                    juce::Path circle;
                    circle.addCentredArc (cx, cy, circleR, circleR,
                                          0.0f, 0.0f, MP::twoPi, true);
                    juce::Path dashed;
                    float dp[] = { circleR * 0.50f, circleR * 0.28f };
                    juce::PathStrokeType (1.5f).createDashedStroke (dashed, circle, dp, 2);
                    g.fillPath (dashed);
                }
                break;
            }
            default: break;
        }
    };

    // =========================================================================
    // EQ mode icon — HP/LP curves, mic, bell, waves
    // =========================================================================
    auto drawEqModeIcon = [&] (int mode, float cx, float cy, float sz, juce::Colour colour)
    {
        using PS = juce::PathStrokeType;
        const PS stroke (2.0f, PS::curved, PS::rounded);
        g.setColour (colour);
        const float r = sz * 0.5f;

        switch (mode)
        {
            case 0: // Low Cut — HP filter curve: rises from bottom-left, flat to right
            {
                juce::Path c;
                c.startNewSubPath (cx - r*0.82f, cy + r*0.45f);
                c.cubicTo (cx - r*0.38f, cy + r*0.45f,
                           cx - r*0.08f, cy - r*0.45f,
                           cx + r*0.12f, cy - r*0.45f);
                c.lineTo (cx + r*0.82f, cy - r*0.45f);
                g.strokePath (c, stroke);
                break;
            }
            case 1: // High Cut — LP filter curve: flat from left, drops to right
            {
                juce::Path c;
                c.startNewSubPath (cx - r*0.82f, cy - r*0.45f);
                c.lineTo (cx - r*0.12f, cy - r*0.45f);
                c.cubicTo (cx + r*0.08f, cy - r*0.45f,
                           cx + r*0.38f, cy + r*0.45f,
                           cx + r*0.82f, cy + r*0.45f);
                g.strokePath (c, stroke);
                break;
            }
            case 2: // Vocal — microphone (capsule + stem + base)
            {
                const float capW = r * 0.30f;
                const float capH = r * 0.52f;
                const float capT = cy - r * 0.68f;
                juce::Path cap;
                cap.addRoundedRectangle (cx - capW, capT, capW*2.0f, capH, capW);
                g.strokePath (cap, juce::PathStrokeType (1.8f));
                g.drawLine (cx, capT + capH,       cx, cy + r*0.22f, 1.8f);
                g.drawLine (cx, cy + r*0.22f,      cx, cy + r*0.62f, 1.8f);
                g.drawLine (cx - r*0.42f, cy + r*0.62f, cx + r*0.42f, cy + r*0.62f, 1.8f);
                break;
            }
            case 3: // Radio — narrow bandpass bell
            {
                juce::Path bell;
                bell.startNewSubPath (cx - r*0.82f, cy + r*0.40f);
                bell.cubicTo (cx - r*0.50f, cy + r*0.40f,
                              cx - r*0.26f, cy - r*0.58f,
                              cx,           cy - r*0.58f);
                bell.cubicTo (cx + r*0.26f, cy - r*0.58f,
                              cx + r*0.50f, cy + r*0.40f,
                              cx + r*0.82f, cy + r*0.40f);
                g.strokePath (bell, stroke);
                break;
            }
            case 4: // Underwater — three stacked wave ripples
            {
                const PS waveStroke (1.8f, PS::curved, PS::rounded);
                for (int i = 0; i < 3; ++i)
                {
                    const float yb  = cy - r*0.28f + (float)i * r*0.30f;
                    const float amp = r * 0.20f;
                    juce::Path wave;
                    wave.startNewSubPath (cx - r*0.80f, yb);
                    wave.cubicTo (cx - r*0.20f, yb - amp,
                                  cx + r*0.20f, yb + amp,
                                  cx + r*0.80f, yb);
                    g.strokePath (wave, waveStroke);
                }
                break;
            }
            default: break;
        }
    };

    // =========================================================================
    // Tone mode icon — circle/dial, two reels, pill+pins, 3 faders, zigzag
    // =========================================================================
    auto drawToneModeIcon = [&] (int mode, float cx, float cy, float sz, juce::Colour colour)
    {
        using PS = juce::PathStrokeType;
        const PS st (2.0f, PS::curved, PS::rounded);
        g.setColour (colour);
        const float r = sz * 0.5f;

        switch (mode)
        {
            case 0: // Clear — hollow circle with a short tick at 12 o'clock
            {
                g.drawEllipse (cx - r*0.55f, cy - r*0.55f, r*1.10f, r*1.10f, 2.0f);
                g.drawLine (cx, cy - r*0.55f - r*0.26f,
                            cx, cy - r*0.55f,            2.0f);
                break;
            }
            case 1: // Tape — two hollow circles, straight line through their centres
            {
                const float rr = r * 0.30f;
                const float dx = r * 0.56f;
                g.drawEllipse (cx - dx - rr, cy - rr, rr*2.0f, rr*2.0f, 2.0f);
                g.drawEllipse (cx + dx - rr, cy - rr, rr*2.0f, rr*2.0f, 2.0f);
                g.drawLine    (cx - dx - rr, cy, cx + dx + rr, cy, 2.0f);
                break;
            }
            case 2: // Tube — vertical pill (rounded rect) + 2 pin dashes at bottom
            {
                const float pw = r * 0.38f;
                const float ph = r * 0.90f;
                const float pt = cy - ph * 0.5f;
                g.drawRoundedRectangle (cx - pw, pt, pw*2.0f, ph, pw, 2.0f);
                const float pinY1 = cy + ph*0.5f + r*0.10f;
                const float pinY2 = cy + ph*0.5f + r*0.32f;
                for (float px : { cx - pw*0.45f, cx + pw*0.45f })
                    g.drawLine (px, pinY1, px, pinY2, 2.0f);
                break;
            }
            case 3: // Console — 3 vertical tracks with fader caps at different heights
            {
                const float tTop = cy - r*0.72f;
                const float tBot = cy + r*0.72f;
                const float spacing = r * 0.58f;
                const float capW    = r * 0.32f;
                const float capH    = r * 0.16f;
                // cap Y positions staggered: high, low, mid
                const float capYs[3] = { cy - r*0.30f, cy + r*0.28f, cy - r*0.05f };
                for (int i = 0; i < 3; ++i)
                {
                    const float tx = cx + (i - 1) * spacing;
                    g.drawLine  (tx, tTop, tx, tBot, 2.0f);
                    g.drawRect  (juce::Rectangle<float> (tx - capW, capYs[i] - capH,
                                                         capW*2.0f, capH*2.0f), 2.0f);
                }
                break;
            }
            case 4: // Grit — sharp zigzag (hard-clipped wave)
            {
                const float left = cx - r*0.82f;
                const float segW = r * 1.64f / 4.0f;
                const float hi   = cy - r*0.46f;
                const float lo   = cy + r*0.46f;
                juce::Path z;
                z.startNewSubPath (left,           lo);
                z.lineTo           (left + segW,   hi);
                z.lineTo           (left + segW*2, lo);
                z.lineTo           (left + segW*3, hi);
                z.lineTo           (left + segW*4, lo);
                g.strokePath (z, PS (2.0f, PS::mitered, PS::square));
                break;
            }
            default: break;
        }
    };

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

    // --- EQ: mode icon (changes with selector) ---
    {
        const int   mode = eqModeSelector.getSelectedItemIndex();   // 0-based, matches AudioParameterChoice index
        const float icx  = (float) eqPanelRect.getCentreX();
        const float icy  = (float) (eqPanelRect.getY() + 77);
        drawEqModeIcon (mode, icx, icy, 44.0f, AC::red);
    }

    // --- TONE: mode icon (changes with selector) ---
    {
        const int   mode = toneModeSelector.getSelectedItemIndex();
        const float icx  = (float) tonePanelRect.getCentreX();
        const float icy  = (float) (tonePanelRect.getY() + 77);
        drawToneModeIcon (mode, icx, icy, 44.0f, AC::cyan);
    }

    // --- SPACE: reverb mode icon (changes with selector) ---
    {
        const int   mode = reverbModeSelector.getSelectedItemIndex();
        const float icx  = (float) spacePanelRect.getCentreX();
        const float icy  = (float) (spacePanelRect.getY() + 77);
        drawReverbModeIcon (mode, icx, icy, 40.0f, AC::purple);
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
        savePresetBtn.setBounds (h.removeFromRight (44));
        h.removeFromRight (6);
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

        // Mode selectors — all sit just above their panel's sweep slider
        eqModeSelector.setBounds (eqPanelRect.getX() + 10,
                                  eqPanelRect.getBottom() - 58,
                                  eqPanelRect.getWidth() - 20, 18);

        toneModeSelector.setBounds (tonePanelRect.getX() + 10,
                                    tonePanelRect.getBottom() - 58,
                                    tonePanelRect.getWidth() - 20, 18);

        reverbModeSelector.setBounds (spacePanelRect.getX() + 10,
                                      spacePanelRect.getBottom() - 58,
                                      spacePanelRect.getWidth() - 20, 18);
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

        // Reverb panel — just the wet knob, no mode selector here
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
