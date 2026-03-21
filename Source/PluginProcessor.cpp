#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AfroplugAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addParam = [&](const juce::String& id,
                        const juce::String& name,
                        float               defaultVal)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },
            name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
            defaultVal));
    };

    addParam ("eq_sweep",       "EQ Sweep",          0.0f);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "eq_mode", 1 },
        "EQ Mode",
        juce::StringArray { "Low Cut", "High Cut", "Vocal", "Radio", "Underwater" },
        0));   // default: Low Cut

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "tone_mode", 1 },
        "Tone Mode",
        juce::StringArray { "Warm", "Tape", "Tube", "Air", "Crunch" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "reverb_mode", 1 },
        "Reverb Mode",
        juce::StringArray { "Studio", "Plate", "Chamber", "Hall", "Abyss" },
        0));

    addParam ("color_vintage",  "Color / Vintage",  50.0f);
    addParam ("vibe_phaser",    "Vibe / Phaser",      0.0f);
    addParam ("stereo_width",   "Stereo Width",      50.0f);
    addParam ("space_reverb",   "Space / Reverb",     0.0f);
    addParam ("delay_texture",  "Delay / Texture",    0.0f);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "delay_division", 1 },
        "Delay Division",
        juce::StringArray { "1:2", "1:3", "1:4", "1:5", "1:6", "1:7", "1:8" },
        2));   // default index 2 = "1:4" (quarter note)

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "delay_pingpong", 1 },
        "Ping-Pong",
        false));

    addParam ("mix_wet",        "Mix (Wet)",         100.0f);

    return layout;
}

//==============================================================================
AfroplugAudioProcessor::AfroplugAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AFROPLUG_STATE", createParameterLayout())
{
    eqSweepParam      = apvts.getRawParameterValue ("eq_sweep");
    eqModeParam       = apvts.getRawParameterValue ("eq_mode");
    reverbModeParam   = apvts.getRawParameterValue ("reverb_mode");
    colorVintageParam = apvts.getRawParameterValue ("color_vintage");
    toneModeParam     = apvts.getRawParameterValue ("tone_mode");
    vibePhaserParam   = apvts.getRawParameterValue ("vibe_phaser");
    stereoWidthParam  = apvts.getRawParameterValue ("stereo_width");
    spaceReverbParam  = apvts.getRawParameterValue ("space_reverb");
    delayTextureParam  = apvts.getRawParameterValue ("delay_texture");
    delayDivisionParam = apvts.getRawParameterValue ("delay_division");
    pingPongParam      = apvts.getRawParameterValue ("delay_pingpong");
    mixWetParam        = apvts.getRawParameterValue ("mix_wet");

    // Create preset directory on first run
    presetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("Afroplug")
                    .getChildFile ("Soul FX")
                    .getChildFile ("Presets");
    presetDir.createDirectory();
}

AfroplugAudioProcessor::~AfroplugAudioProcessor() {}

//==============================================================================
void AfroplugAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (getTotalNumOutputChannels());

    phaser.prepare (spec);
    phaser.reset();
    phaser.setCentreFrequency (800.0f);
    phaser.setFeedback        (0.25f);

    delay.prepare (spec);
    delay.reset();
    delay.setDelay (static_cast<float> (sampleRate * 60.0 / 120.0));

    // ── 4× oversampler ────────────────────────────────────────────────────────
    oversampling.initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampling.reset();

    // Reset DC blocker state
    for (int i = 0; i < kMaxCh; ++i)
        dcBlockX[i] = dcBlockY[i] = 0.0f;

    // Precompute LP coefficients scaled to the actual sample rate.
    // Formula: a = 1 - exp(-2π·fc/fs)  →  y += a·(x - y)
    const float twoPiOverSr = juce::MathConstants<float>::twoPi / (float) sampleRate;
    tapeHfLPCoef = 1.0f - std::exp (-twoPiOverSr * 9000.0f);   // 9 kHz  — tape air roll
    delayFbLPCoef= 1.0f - std::exp (-twoPiOverSr * 4500.0f);   // 4.5 kHz — analog repeat warmth
    sideHPR      = std::exp (-twoPiOverSr * 200.0f);           // 200 Hz HP pole — side-channel bass blocker
    ottLP1Coef   = 1.0f - std::exp (-twoPiOverSr * 200.0f);   // Air low/mid split at 200 Hz
    ottLP2Coef   = 1.0f - std::exp (-twoPiOverSr * 2000.0f);  // Air mid/high split at 2 kHz
    for (int i = 0; i < kMaxCh; ++i)
        tapeHfLP[i] = delayFbLP[i] = ottLP1[i] = ottLP2[i] = 0.0f;
    sideHPx1 = sideHPy1 = 0.0f;

    // ── FDN Reverb — pre-allocate at max needed size for this sample rate ─────
    // Base delay lengths (samples at 44.1 kHz). Chosen as coprime values so
    // echo density builds smoothly without periodic clustering.
    static const int kBase[kFDNTaps] = { 1637, 2053, 2593, 3271 };
    const double srRatio = sampleRate / 44100.0;
    for (int i = 0; i < kFDNTaps; ++i)
    {
        // Allocate 2.2× the largest mode scale (Abyss = 2.0×) to stay safe.
        const int maxL = static_cast<int> (kBase[i] * srRatio * 2.2);
        fdnL[i].buf.assign (maxL, 0.0f);
        fdnL[i].writePos = 0;
        fdnL[i].delayLen = static_cast<int> (kBase[i] * srRatio);
        fdnL[i].dampZ    = 0.0f;

        // R channel: 3.1 % longer — breaks comb-filtering between channels.
        const int maxR = static_cast<int> (kBase[i] * srRatio * 2.2 * 1.05);
        fdnR[i].buf.assign (maxR, 0.0f);
        fdnR[i].writePos = 0;
        fdnR[i].delayLen = static_cast<int> (kBase[i] * srRatio * 1.031);
        fdnR[i].dampZ    = 0.0f;
    }

    dryWetMixer.prepare (spec);
    dryWetMixer.reset();
    dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    eqFilter1.prepare (spec);
    eqFilter1.reset();
    eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    eqFilter1.setCutoffFrequency (20.0f);
    eqFilter1.setResonance (0.707f);

    // Initialise eqFilter2 (StereoIIR / high shelf) with identity coefficients (0 dB)
    eqFilter2.prepare (spec);
    eqFilter2.reset();
    *eqFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 10000.0f, 0.300f, 1.0f);

    // Console tone EQ filters (initialised to unity gain)
    toneConsoleLoMid.prepare (spec);
    toneConsoleLoMid.reset();
    *toneConsoleLoMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 250.0f, 0.5f, 1.0f);

    toneConsoleHiShelf.prepare (spec);
    toneConsoleHiShelf.reset();
    *toneConsoleHiShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 8000.0f, 0.7f, 1.0f);

    // Safety soft clipper: tanh soft-limit keeping peaks just below 0 dBFS
    safetyClipper.functionToUse = [] (float x) { return std::tanh (x) * 0.988f; };
    safetyClipper.prepare (spec);
}

void AfroplugAudioProcessor::releaseResources()
{
    phaser.reset();
    delay.reset();
    dryWetMixer.reset();
    eqFilter1.reset();
    eqFilter2.reset();
    toneConsoleLoMid.reset();
    toneConsoleHiShelf.reset();
    safetyClipper.reset();
    oversampling.reset();
    for (int i = 0; i < kMaxCh; ++i)
        tapeHfLP[i] = delayFbLP[i] = dcBlockX[i] = dcBlockY[i]
                    = ottLP1[i] = ottLP2[i] = 0.0f;
    sideHPx1 = sideHPy1 = 0.0f;
    for (int i = 0; i < kFDNTaps; ++i)
    {
        std::fill (fdnL[i].buf.begin(), fdnL[i].buf.end(), 0.0f);
        std::fill (fdnR[i].buf.begin(), fdnR[i].buf.end(), 0.0f);
        fdnL[i].writePos = fdnR[i].writePos = 0;
        fdnL[i].dampZ    = fdnR[i].dampZ    = 0.0f;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AfroplugAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}
#endif

//==============================================================================
void AfroplugAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // ── Deferred state reset (triggered by preset load or DAW session restore) ──
    // Runs on the audio thread so it's safe to call reset() on all DSP objects.
    if (resetStateRequested.exchange (false))
    {
        phaser.reset();
        delay.reset();
        dryWetMixer.reset();
        eqFilter1.reset();
        eqFilter2.reset();
        toneConsoleLoMid.reset();
        toneConsoleHiShelf.reset();
        for (int i = 0; i < kMaxCh; ++i)
            tapeHfLP[i] = delayFbLP[i] = dcBlockX[i] = dcBlockY[i]
                        = ottLP1[i]    = ottLP2[i]    = 0.0f;
        sideHPx1 = sideHPy1 = prevSample = 0.0f;
        for (int i = 0; i < kFDNTaps; ++i)
        {
            std::fill (fdnL[i].buf.begin(), fdnL[i].buf.end(), 0.0f);
            std::fill (fdnR[i].buf.begin(), fdnR[i].buf.end(), 0.0f);
            fdnL[i].writePos = fdnR[i].writePos = 0;
            fdnL[i].dampZ    = fdnR[i].dampZ    = 0.0f;
        }
        prevEqMode   = -1;   // force filter flush on next block
        prevToneMode = -1;
    }

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float eqSweep      = eqSweepParam     ->load();
    const int   eqMode       = static_cast<int> (std::round (eqModeParam->load()));
    const float colorVintage = colorVintageParam->load();
    const int   toneMode     = static_cast<int> (std::round (toneModeParam->load()));
    const float vibePhaser   = vibePhaserParam  ->load();
    const float stereoWidth  = stereoWidthParam ->load();
    const float spaceReverb  = spaceReverbParam ->load();
    const float delayTexture = delayTextureParam->load();
    const float mixWet       = mixWetParam      ->load();

    const int numCh = buffer.getNumChannels();
    const int numSa = buffer.getNumSamples();

    juce::dsp::AudioBlock<float>              block   (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // Read host tempo — used for both delay time and any future tempo-sync
    double currentBpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto positionInfo = ph->getPosition())
            if (positionInfo->getBpm().hasValue())
                currentBpm = *positionInfo->getBpm();

    // Sync delay time to selected division (1:2 … 1:8)
    // divIdx 0→denom 2 (half note), 2→denom 4 (quarter), 6→denom 8 (eighth)
    {
        const int   divIdx        = static_cast<int> (std::round (delayDivisionParam->load()));
        const int   denom         = divIdx + 2;
        // Clamp to one sample below the DelayLine's compile-time max (576000).
        // Without this, very low BPM (< ~40) at 192 kHz overflows the buffer.
        const float delayInSamples = juce::jmin (
            static_cast<float> (60.0 / currentBpm * 4.0 / denom * getSampleRate()),
            575999.0f);
        delay.setDelay (delayInSamples);
    }

    // Measure audio features (dry signal, channel 0 only)
    if (numSa > 0 && numCh > 0)
    {
        const float* ch0 = buffer.getReadPointer (0);

        // RMS
        float sumSq = 0.0f;
        for (int n = 0; n < numSa; ++n)
            sumSq += ch0[n] * ch0[n];
        const float blockRMS = std::sqrt (sumSq / static_cast<float> (numSa));
        currentRMS.store (0.9f * currentRMS.load() + 0.1f * blockRMS);

        // ZCR — count sign changes / buffer size
        int crossings = 0;
        float prev = prevSample;
        for (int n = 0; n < numSa; ++n)
        {
            if ((ch0[n] >= 0.0f) != (prev >= 0.0f))
                ++crossings;
            prev = ch0[n];
        }
        prevSample = prev;
        const float blockZCR = static_cast<float> (crossings) / static_cast<float> (numSa);
        currentZCR.store (0.9f * currentZCR.load() + 0.1f * blockZCR);
    }

    // Save dry signal
    dryWetMixer.setWetMixProportion (mixWet / 100.0f);
    dryWetMixer.pushDrySamples (block);

    // 0. EQ — 5 modes, sweep knob modulates the key frequency in each mode
    {
        // Flush IIR state when the mode changes — prevents the accumulated history
        // of the previous mode (e.g. Vocal shelf boost) bleeding through as a transient.
        if (eqMode != prevEqMode)
        {
            eqFilter1.reset();
            eqFilter2.reset();
            prevEqMode = eqMode;
        }

        // Helper: exponential map 0–100 → loHz–hiHz
        auto expMap = [] (float t, float lo, float hi) -> float {
            return lo * std::pow (hi / lo, t / 100.0f);
        };

        switch (eqMode)
        {
            case 0: // Low Cut — high-pass sweep 20 Hz → 2 kHz
            {
                const float freq = expMap (eqSweep, 20.0f, 2000.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
                eqFilter1.setCutoffFrequency (freq);
                eqFilter1.setResonance (0.707f);
                eqFilter1.process (context);
                break;
            }
            case 1: // High Cut — low-pass sweep 20 kHz → 500 Hz (exponential = musical)
            {
                const float cutoff = expMap (100.0f - eqSweep, 500.0f, 20000.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                eqFilter1.setCutoffFrequency (cutoff);
                eqFilter1.setResonance (0.707f);
                eqFilter1.process (context);
                break;
            }
            case 2: // Vocal — sweepable HP (30–200 Hz) + presence shelf (+3 to +12 dB at 8 kHz)
            {
                // Wider HP sweep: removes more mud/boom at high knob values
                const float hpFreq = expMap (eqSweep, 30.0f, 200.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
                eqFilter1.setCutoffFrequency (hpFreq);
                eqFilter1.setResonance (0.9f);   // slight resonant peak adds vocal presence
                eqFilter1.process (context);

                // High shelf at 8 kHz: +3 dB (knob=0) → +12 dB (knob=100)
                // Brings forward air, sibilance, and presence — makes vocals cut through
                const float shelfGain = juce::jmap (eqSweep, 0.0f, 100.0f, 1.413f, 3.981f);
                *eqFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, 8000.0f, 0.5f, shelfGain);
                eqFilter2.process (context);
                break;
            }
            case 3: // Radio — BP centred 500 Hz → 3 kHz, narrow Q
            {
                const float centreFreq = expMap (eqSweep, 500.0f, 3000.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
                eqFilter1.setCutoffFrequency (centreFreq);
                eqFilter1.setResonance (3.0f);   // narrow telephone-band peak
                eqFilter1.process (context);
                break;
            }
            case 4: // Underwater — steep LP sweep 800 Hz → 80 Hz
            {
                const float freq = expMap (100.0f - eqSweep, 80.0f, 800.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                eqFilter1.setCutoffFrequency (freq);
                eqFilter1.setResonance (1.2f);   // slight resonant muddiness
                eqFilter1.process (context);
                break;
            }
            default: break;
        }
    }

    // 0.5. TONE — 5 saturation modes, all processed at 4× sample rate.
    // Flush tone filter states when mode changes to avoid transient pops.
    // Oversampling prevents aliasing harmonics from folding back into the audio band,
    // which is what makes basic waveshapers sound harsh and "digital".
    {
        if (toneMode != prevToneMode)
        {
            toneConsoleLoMid.reset();
            toneConsoleHiShelf.reset();
            // Also flush OTT crossover state so band history from previous mode
            // doesn't saturate the new mode's first few blocks.
            for (int i = 0; i < kMaxCh; ++i)
                ottLP1[i] = ottLP2[i] = 0.0f;
            prevToneMode = toneMode;
        }

        const float drive = colorVintage / 100.0f;

        // ── Crunch pre-emphasis — boost ~2.2 kHz before saturation ──────────
        // Pre-emphasis sharpens the mid-range going into the saturator so that
        // the resulting harmonic distortion lands on meaningful upper harmonics
        // (the "crack" and "bite" of a driven console channel strip).
        // The complementary de-emphasis after the saturator removes the boost,
        // leaving only the saturated harmonic content behind — net result is
        // a focused crunch rather than a broad fuzz.
        if (toneMode == 4 && drive > 0.01f)
        {
            const float peakGainLin = 1.0f + drive * 5.0f;  // up to +14 dB at 2.2 kHz
            *toneConsoleLoMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 2200.0f, 1.2f, peakGainLin);
            toneConsoleLoMid.process (context);
        }

        // ── Upsample to 4× ────────────────────────────────────────────────────
        auto osBlock = oversampling.processSamplesUp (block);
        const int osN = static_cast<int> (osBlock.getNumSamples());

        switch (toneMode)
        {
            case 0: // Warm — even-order (2nd harmonic) saturation, like a transformer
            {
                // y = tanh(x + k·x²)
                // The x² term is asymmetric → generates 2nd harmonic content (even-order).
                // tanh provides a soft ceiling. Small-signal slope at x=0 is 1 (unity gain).
                // DC offset from x² is removed by the DC blocker after downsample.
                if (drive > 0.01f)
                {
                    const float k = drive * 0.55f;
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* p = osBlock.getChannelPointer (static_cast<size_t> (ch));
                        for (int n = 0; n < osN; ++n)
                        {
                            const float x = p[n];
                            p[n] = std::tanh (x + k * x * x);
                        }
                    }
                }
                break;
            }

            case 1: // Tape — soft tanh saturation, unity small-signal gain
            {
                // Normalise by inputGain (the small-signal slope at x=0), NOT by
                // tanh(inputGain) (the saturation ceiling). Dividing by tanh(k) < k
                // would boost quiet signals proportionally — ear-raping at high drive.
                const float inputGain  = 1.0f + drive * 3.5f;
                const float normFactor = 1.0f / inputGain;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* p = osBlock.getChannelPointer (static_cast<size_t> (ch));
                    for (int n = 0; n < osN; ++n)
                        p[n] = std::tanh (p[n] * inputGain) * normFactor;
                }
                break;
            }

            case 2: // Tube — asymmetric DC-biased clip → even-order harmonics (warm)
            {
                // Bias shifts the curve so positive peaks saturate harder (real tube stage).
                // dc  = tanh(bias*k) — DC injected at zero input, must be subtracted.
                // Normalise by the *small-signal gain* f'(0) = k·sech²(bias·k)
                //                                          = k·(1 - dc²)
                // This keeps quiet signals at input level instead of boosting them.
                const float tubeDrive = 1.0f + drive * 4.5f;
                const float bias      = drive * 0.20f;
                const float dc        = std::tanh (bias * tubeDrive);
                const float ssGain    = tubeDrive * (1.0f - dc * dc); // sech²=1-tanh²
                const float norm      = 1.0f / std::max (ssGain, 1e-6f);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* p = osBlock.getChannelPointer (static_cast<size_t> (ch));
                    for (int n = 0; n < osN; ++n)
                        p[n] = (std::tanh ((p[n] + bias) * tubeDrive) - dc) * norm;
                }
                break;
            }

            case 3: // Air — oversampled portion is pass-through.
                    // The 3-band OTT-style processing runs at base rate (after downsample)
                    // because the crossover filter coefficients are computed at base SR.
                break;

            case 4: // Crunch — hard-driven tanh saturation, aggressive mid crunch.
                    // Heavy drive (k up to 9×) hard-clips the signal, generating rich
                    // odd and even harmonics. Normalised at small-signal unity so no
                    // volume jump. Pre/de-emphasis shapes the harmonic content around 2 kHz.
                if (drive > 0.01f)
                {
                    const float k    = 1.0f + drive * 8.0f;
                    const float norm = 1.0f / k;
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* p = osBlock.getChannelPointer (static_cast<size_t> (ch));
                        for (int n = 0; n < osN; ++n)
                            p[n] = std::tanh (p[n] * k) * norm;
                    }
                }
                break;

            default: break;
        }

        // ── Downsample back to base rate ──────────────────────────────────────
        oversampling.processSamplesDown (block);

        // DC blocker — 1-pole HP catches any residual DC (especially from Tube bias).
        // y[n] = x[n] − x[n−1] + R·y[n−1]   (R = 0.9990 → fc ≈ 7 Hz @ 44.1 kHz)
        // Faster than 0.9998 (≈1.4 Hz) — clears Tube-mode bias transients in ~4 ms.
        {
            constexpr float R = 0.9990f;
            for (int ch = 0; ch < numCh && ch < kMaxCh; ++ch)
            {
                float* p = block.getChannelPointer (static_cast<size_t> (ch));
                for (int n = 0; n < numSa; ++n)
                {
                    const float xn = p[n];
                    p[n]           = xn - dcBlockX[ch] + R * dcBlockY[ch];
                    dcBlockX[ch]   = xn;
                    dcBlockY[ch]   = p[n];
                }
            }
        }

        // ── Tape HF roll ──────────────────────────────────────────────────────
        // 1-pole LP that darkens proportionally with drive (mode 1 only).
        if (toneMode == 1)
        {
            const float a = 1.0f - drive * (1.0f - tapeHfLPCoef);
            for (int ch = 0; ch < numCh && ch < kMaxCh; ++ch)
            {
                float* p = block.getChannelPointer (static_cast<size_t> (ch));
                for (int n = 0; n < numSa; ++n)
                {
                    tapeHfLP[ch] += a * (p[n] - tapeHfLP[ch]);
                    p[n] = tapeHfLP[ch];
                }
            }
        }

        // ── Air / OTT — 3-band upward-companding saturation ───────────────
        // True OTT character: each band is processed with a transfer curve that
        // expands quiet signals AND compresses loud ones simultaneously.
        //
        // Key technique: normalise at a reference amplitude (-20 dBFS = 0.10).
        //   • Signals quieter than the ref get EXPANDED (gain > 1) — things that
        //     were buried in the mix get pulled forward
        //   • Signals louder than the ref get COMPRESSED (gain < 1) — peaks squash
        //   • The combination is the hyper-dense "everything is present" OTT character
        //
        // Per-band aggressiveness:
        //   Low  (< 200 Hz) — moderate, keeps bass punchy without blowing up subs
        //   Mid  (200 Hz–2 kHz) — very heavy, OTT's signature "punch in the face"
        //   High (> 2 kHz)  — heavy + slight lift for that bright shimmering air
        if (toneMode == 3 && drive > 0.01f)
        {
            constexpr float kRef = 0.10f;   // reference amplitude (-20 dBFS)

            // Per-band drive amounts — kept moderate so upstream EQ boosts
            // (e.g. Vocal shelf +12 dB) don't push the output into ear-rape territory.
            // Mid is still the most aggressive band (OTT punch) but capped sensibly.
            const float kLow  = 1.0f + drive * 4.0f;
            const float kMid  = 1.0f + drive * 8.0f;
            const float kHigh = 1.0f + drive * 6.0f;

            // Normalisation factors: output equals kRef at input == kRef
            const float nLow  = kRef / std::tanh (kRef * kLow);
            const float nMid  = kRef / std::tanh (kRef * kMid);
            const float nHigh = kRef / std::tanh (kRef * kHigh);

            // Reduced makeup — previous 0.7× headroom multiplier was stacking on top
            // of EQ boosts and causing loudness spikes when used with Vocal preset.
            const float makeup = 1.0f + drive * 0.30f;

            for (int ch = 0; ch < numCh && ch < kMaxCh; ++ch)
            {
                float* p = block.getChannelPointer (static_cast<size_t> (ch));
                for (int n = 0; n < numSa; ++n)
                {
                    const float x = p[n];

                    ottLP1[ch] += ottLP1Coef * (x - ottLP1[ch]);   // 200 Hz LP
                    ottLP2[ch] += ottLP2Coef * (x - ottLP2[ch]);   // 2 kHz LP

                    const float low  = ottLP1[ch];
                    const float mid  = ottLP2[ch] - ottLP1[ch];
                    const float high = x - ottLP2[ch];

                    const float pLow  = std::tanh (low  * kLow)  * nLow;
                    const float pMid  = std::tanh (mid  * kMid)  * nMid;
                    const float pHigh = std::tanh (high * kHigh) * nHigh;

                    p[n] = (pLow + pMid + pHigh) * makeup;
                }
            }
        }

        // ── Crunch de-emphasis — shelve above ~4 kHz post-saturation ─────
        // Cuts the top end that was boosted by pre-emphasis, leaving a natural
        // warm crunch rather than a harsh fizz. The high-shelf dips above 4 kHz,
        // pulling back the brittle overtones while keeping the punchy mid crunch.
        if (toneMode == 4 && drive > 0.01f)
        {
            const float shelfGainLin = 1.0f / (1.0f + drive * 3.0f);  // cut HF by up to -12 dB
            *toneConsoleHiShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                currentSampleRate, 4000.0f, 0.7f, shelfGainLin);
            toneConsoleHiShelf.process (context);
        }
    }

    // 1. Phaser (vibe_phaser) — lush vintage sweep
    // Mix capped at 0.45 (0.6 was too heavy and obvious).
    // Center frequency sweeps 400→1200 Hz with the knob for tonal variation.
    // Feedback adds resonance to the phase notches (thickens the effect).
    {
        const float mix  = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f,   0.45f);
        const float dep  = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f,   0.60f);
        // Exponential rate curve: 0.05→1.20 Hz with more resolution at slow speeds.
        // At 50% knob: 0.245 Hz (vs 0.625 Hz linear) — gives better control in the
        // slow lush range without sacrificing fast tremolo at the top of the knob.
        const float rate = 0.05f * std::pow (1.20f / 0.05f, vibePhaser / 100.0f);
        const float cf   = juce::jmap (vibePhaser, 0.0f, 100.0f, 400.0f, 1200.0f);
        const float fb   = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f,   0.35f);
        phaser.setMix             (mix);
        phaser.setDepth           (dep);
        phaser.setRate            (rate);
        phaser.setCentreFrequency (cf);
        phaser.setFeedback        (fb);
        phaser.process            (context);
    }

    // 2. Delay (delay_texture) — tempo-synced to selected division.
    // LP-filtered feedback: each repeat gets progressively darker (tape/BBD warmth).
    // Ping-pong mode: output of L feeds into R delay input and vice-versa, so the
    // signal bounces left-right across each repeat.
    if (delayTexture > 0.1f)
    {
        const float feedback  = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.65f);
        const float wetAmt    = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.40f);
        const bool  pingPong  = numCh >= 2 && pingPongParam->load() > 0.5f;

        if (pingPong)
        {
            // Cross-feed: L delay reads → goes into R delay write, R → L
            float* L = buffer.getWritePointer (0);
            float* R = buffer.getWritePointer (1);
            for (int n = 0; n < numSa; ++n)
            {
                const float dL = delay.popSample (0);
                const float dR = delay.popSample (1);
                // Feedback LP: R output darkens into L, L output darkens into R
                delayFbLP[0] += delayFbLPCoef * (dR * feedback - delayFbLP[0]);
                delayFbLP[1] += delayFbLPCoef * (dL * feedback - delayFbLP[1]);
                delay.pushSample (0, L[n] + delayFbLP[0]);
                delay.pushSample (1, R[n] + delayFbLP[1]);
                L[n] += dL * wetAmt;
                R[n] += dR * wetAmt;
            }
        }
        else
        {
            for (int ch = 0; ch < numCh && ch < kMaxCh; ++ch)
            {
                float* ptr = block.getChannelPointer (static_cast<size_t> (ch));
                for (int n = 0; n < numSa; ++n)
                {
                    const float in  = ptr[n];
                    const float del = delay.popSample (ch);
                    delayFbLP[ch] += delayFbLPCoef * (del * feedback - delayFbLP[ch]);
                    delay.pushSample (ch, in + delayFbLP[ch]);
                    ptr[n] = in + del * wetAmt;
                }
            }
        }
    }

    // 3. REVERB — 4-tap FDN with Householder reflection feedback matrix.
    //
    // Architecture:
    //   • 4 delay taps per channel (L and R have slightly different lengths = stereo width)
    //   • Householder mixing: new_s[i] = s[i] - 0.5 * sum(s)  — orthogonal, energy-preserving
    //   • Per-tap 1-pole LP damping: dampZ += coef * (s - dampZ)
    //     where coef near 0 = very dark tail, coef near 1 = very bright tail
    //   • Global feedback decay controls RT60
    //
    if (spaceReverb > 0.1f)
    {
        const int reverbMode = static_cast<int> (std::round (reverbModeParam->load()));

        float fdnDecay, fdnDampCoef, fdnWet, fdnScale;
        switch (reverbMode)
        {
            case 0: // Studio — tight, intimate, dark
                fdnDecay    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.75f, 0.84f);
                fdnDampCoef = 0.14f;   // strong HF roll-off in feedback = dark tail
                fdnWet      = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.30f);
                fdnScale    = 0.55f;   // short delays = small room
                break;
            case 1: // Plate — bright, washy, wide
                fdnDecay    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.88f, 0.94f);
                fdnDampCoef = 0.65f;   // less HF roll-off = bright plate sheen
                fdnWet      = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.45f);
                fdnScale    = 1.0f;
                break;
            case 2: // Chamber — natural, balanced
                fdnDecay    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.84f, 0.91f);
                fdnDampCoef = 0.38f;
                fdnWet      = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.40f);
                fdnScale    = 1.0f;
                break;
            case 3: // Hall — long, open, expansive
                fdnDecay    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.90f, 0.97f);
                fdnDampCoef = 0.50f;
                fdnWet      = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.50f);
                fdnScale    = 1.5f;   // longer delays = larger hall
                break;
            case 4: // Abyss — near-infinite, pitch-black tail
                fdnDecay    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.97f, 0.999f);
                fdnDampCoef = 0.10f;   // maximum HF roll-off = dark endless void
                fdnWet      = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.65f);
                fdnScale    = 2.0f;
                break;
            default:
                fdnDecay = 0.85f; fdnDampCoef = 0.35f; fdnWet = 0.30f; fdnScale = 1.0f;
                break;
        }

        // Update active delay lengths (no allocation — buffers sized at 2.2× max at prepare)
        static const int kBase[kFDNTaps] = { 1637, 2053, 2593, 3271 };
        const double srRatio = currentSampleRate / 44100.0;
        for (int i = 0; i < kFDNTaps; ++i)
        {
            fdnL[i].delayLen = juce::jmax (1, static_cast<int> (kBase[i] * srRatio * fdnScale));
            fdnR[i].delayLen = juce::jmax (1, static_cast<int> (kBase[i] * srRatio * fdnScale * 1.031));
        }

        // Per-sample FDN step.
        // Correct signal order: read → Householder mix → damp → decay → write.
        // dampMult: per-tap scaling of fdnDampCoef so L/R have subtly different
        // HF decay rates — this de-correlates the two channels without needing
        // separate reverb engines, adding real stereo width to the tail.
        static constexpr float kLDampMult[kFDNTaps] = { 0.90f, 1.00f, 1.10f, 1.05f };
        static constexpr float kRDampMult[kFDNTaps] = { 1.00f, 0.85f, 1.15f, 0.95f };

        auto fdnStep = [&] (std::array<FDNLine, kFDNTaps>& fdn,
                            float input, const float* dampMult) -> float
        {
            // 1. Read all taps
            float d[kFDNTaps];
            for (int i = 0; i < kFDNTaps; ++i)
            {
                const int sz = static_cast<int> (fdn[i].buf.size());
                const int rp = (fdn[i].writePos - fdn[i].delayLen + sz) % sz;
                d[i] = fdn[i].buf[rp];
            }

            // 2. Householder reflection — orthogonal 4×4 mixing matrix
            // H[i,j] = δ[i,j] − 0.5   (energy-preserving: H·H^T = I for N=4)
            const float sum = d[0] + d[1] + d[2] + d[3];
            float f[kFDNTaps];
            for (int i = 0; i < kFDNTaps; ++i)
                f[i] = d[i] - 0.5f * sum;

            // 3. Per-tap 1-pole LP damping (varied per channel for stereo diffusion)
            for (int i = 0; i < kFDNTaps; ++i)
            {
                const float coef = juce::jlimit (0.0f, 1.0f, fdnDampCoef * dampMult[i]);
                fdn[i].dampZ += coef * (f[i] - fdn[i].dampZ);
                f[i] = fdn[i].dampZ * fdnDecay;
            }

            // 4. Write input + feedback back to delay lines
            for (int i = 0; i < kFDNTaps; ++i)
            {
                fdn[i].buf[fdn[i].writePos] = input + f[i];
                fdn[i].writePos = (fdn[i].writePos + 1) % static_cast<int> (fdn[i].buf.size());
            }

            // 5. Output: average of pre-mix tap readings
            return (d[0] + d[1] + d[2] + d[3]) * 0.25f;
        };

        float* L = buffer.getWritePointer (0);
        float* R = numCh >= 2 ? buffer.getWritePointer (1) : L;
        for (int n = 0; n < numSa; ++n)
        {
            L[n] += fdnStep (fdnL, L[n], kLDampMult) * fdnWet;
            if (numCh >= 2) R[n] += fdnStep (fdnR, R[n], kRDampMult) * fdnWet;
        }
    }

    // 4. Stereo width (stereo_width) -- M/S matrix, 50 = unity.
    // Capped at 1.5× side gain. The side signal is high-passed at 200 Hz before
    // re-encoding so that bass below 200 Hz stays fully mono — prevents comb
    // filtering and level loss on mono speakers / club systems.
    if (numCh >= 2)
    {
        const float widthGain = juce::jmap (stereoWidth, 0.0f, 100.0f, 0.0f, 1.5f);
        // HP coefficients: y[n] = B*(x[n]-x[n-1]) + R*y[n-1]  (unity gain at HF)
        const float hpB = (1.0f + sideHPR) * 0.5f;
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        for (int n = 0; n < numSa; ++n)
        {
            const float mid   = 0.5f * (L[n] + R[n]);
            const float sideIn = 0.5f * (L[n] - R[n]) * widthGain;
            // 1-pole HP at 200 Hz — removes sub-bass from side channel
            const float side  = hpB * sideIn - hpB * sideHPx1 + sideHPR * sideHPy1;
            sideHPx1 = sideIn;
            sideHPy1 = side;
            L[n] = mid + side;
            R[n] = mid - side;
        }
    }

    // 5. Final dry/wet blend (mix_wet)
    dryWetMixer.mixWetSamples (block);

    // 6. Safety soft clipper — always last, prevents any hot signal from clipping the DAW
    safetyClipper.process (context);
}

//==============================================================================
// AI Analysis — called from the UI thread (button click)
// Reads live RMS / ZCR metrics, applies a heuristic preset, and returns a
// short label describing the detected signal type for the UI to display.
juce::String AfroplugAudioProcessor::triggerAIAnalysis()
{
    const float rms = currentRMS.load();
    const float zcr = currentZCR.load();

    auto set = [&] (const char* id, float val01)
    {
        apvts.getParameter (id)->setValueNotifyingHost (val01);
    };

    auto norm     = [] (float pct) { return pct / 100.0f; };
    auto modeNorm = [] (int idx)   { return static_cast<float> (idx) / 4.0f; };

    if (zcr > 0.15f)
    {
        // High ZCR: vocals / bright leads — air-boost EQ, lush verb, subtle delay
        set ("eq_mode",       modeNorm (2));
        set ("eq_sweep",      norm (70.0f));
        set ("tone_mode",     modeNorm (0));   // Warm
        set ("color_vintage", norm (35.0f));
        set ("space_reverb",  norm (60.0f));
        set ("reverb_mode",   modeNorm (1));   // Plate
        set ("delay_texture", norm (30.0f));
        set ("vibe_phaser",   norm (20.0f));
        set ("stereo_width",  norm (60.0f));
        set ("mix_wet",       norm (85.0f));
        return "VOCALS";
    }

    if (zcr < 0.05f && rms > 0.15f)
    {
        // Low ZCR + high RMS: bass / kick — tight HP, tape saturation, no reverb tail
        set ("eq_mode",       modeNorm (0));   // Low Cut
        set ("eq_sweep",      norm (20.0f));
        set ("tone_mode",     modeNorm (1));   // Tape
        set ("color_vintage", norm (75.0f));
        set ("space_reverb",  norm (10.0f));
        set ("reverb_mode",   modeNorm (0));   // Studio
        set ("delay_texture", norm (0.0f));
        set ("vibe_phaser",   norm (0.0f));
        set ("stereo_width",  norm (45.0f));
        set ("mix_wet",       norm (90.0f));
        return "BASS";
    }

    if (rms < 0.05f)
    {
        // Quiet + mid ZCR: pads / ambient — wide, long reverb, drifting delay
        set ("eq_mode",       modeNorm (4));   // Underwater
        set ("eq_sweep",      norm (50.0f));
        set ("tone_mode",     modeNorm (0));   // Warm
        set ("color_vintage", norm (30.0f));
        set ("space_reverb",  norm (80.0f));
        set ("reverb_mode",   modeNorm (3));   // Hall
        set ("delay_texture", norm (65.0f));
        set ("vibe_phaser",   norm (35.0f));
        set ("stereo_width",  norm (85.0f));
        set ("mix_wet",       norm (75.0f));
        return "PADS";
    }

    // Mid ZCR + mid RMS: melodic / guitars — balanced treatment
    set ("eq_mode",       modeNorm (1));   // High Cut
    set ("eq_sweep",      norm (40.0f));
    set ("tone_mode",     modeNorm (3));   // Air
    set ("color_vintage", norm (50.0f));
    set ("space_reverb",  norm (45.0f));
    set ("reverb_mode",   modeNorm (2));   // Chamber
    set ("delay_texture", norm (25.0f));
    set ("vibe_phaser",   norm (10.0f));
    set ("stereo_width",  norm (55.0f));
    set ("mix_wet",       norm (80.0f));
    return "MELODY";
}

//==============================================================================
// Preset management
//==============================================================================
juce::StringArray AfroplugAudioProcessor::getAvailablePresets() const
{
    juce::StringArray names;
    for (auto& f : presetDir.findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

void AfroplugAudioProcessor::loadPreset (const juce::String& presetName)
{
    const juce::File f = presetDir.getChildFile (presetName + ".xml");
    if (! f.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse (f);
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        resetStateRequested.store (true);   // flush DSP state on next audio block
    }
}

void AfroplugAudioProcessor::savePreset (const juce::String& presetName)
{
    if (presetName.isEmpty()) return;
    const juce::File f = presetDir.getChildFile (presetName + ".xml");
    auto state = apvts.copyState();
    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        xml->writeTo (f);
}

//==============================================================================
void AfroplugAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AfroplugAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        resetStateRequested.store (true);   // flush DSP state on next audio block
    }
}

//==============================================================================
bool AfroplugAudioProcessor::hasEditor() const         { return true; }
juce::AudioProcessorEditor* AfroplugAudioProcessor::createEditor()
    { return new AfroplugAudioProcessorEditor (*this); }

const juce::String AfroplugAudioProcessor::getName()  const { return JucePlugin_Name; }
bool  AfroplugAudioProcessor::acceptsMidi()            const { return false; }
bool  AfroplugAudioProcessor::producesMidi()           const { return false; }
bool  AfroplugAudioProcessor::isMidiEffect()           const { return false; }
double AfroplugAudioProcessor::getTailLengthSeconds()  const { return 2.0; }
int   AfroplugAudioProcessor::getNumPrograms()               { return 1; }
int   AfroplugAudioProcessor::getCurrentProgram()            { return 0; }
void  AfroplugAudioProcessor::setCurrentProgram (int)        {}
const juce::String AfroplugAudioProcessor::getProgramName (int)            { return {}; }
void  AfroplugAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AfroplugAudioProcessor();
}
