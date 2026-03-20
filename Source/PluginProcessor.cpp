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
        juce::StringArray { "Clear", "Tape", "Tube", "Console", "Grit" },
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
    delayTextureParam = apvts.getRawParameterValue ("delay_texture");
    mixWetParam       = apvts.getRawParameterValue ("mix_wet");

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

    reverb.prepare (spec);
    reverb.reset();

    reverbPostFilter.prepare (spec);
    reverbPostFilter.reset();
    reverbPostFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    reverbPostFilter.setCutoffFrequency (5000.0f);
    reverbPostFilter.setResonance (0.707f);

    delay.prepare (spec);
    delay.reset();
    // Default to 120 BPM quarter note until the host supplies a tempo
    delay.setDelay (static_cast<float> (sampleRate * 60.0 / 120.0));

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
    reverb.reset();
    delay.reset();
    dryWetMixer.reset();
    eqFilter1.reset();
    eqFilter2.reset();
    reverbPostFilter.reset();
    toneConsoleLoMid.reset();
    toneConsoleHiShelf.reset();
    safetyClipper.reset();
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

    // Sync delay to host tempo (quarter note)
    {
        double currentBpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto positionInfo = ph->getPosition())
                if (positionInfo->getBpm().hasValue())
                    currentBpm = *positionInfo->getBpm();

        const float quarterNoteSeconds = 60.0f / static_cast<float> (currentBpm);
        const float delayInSamples     = quarterNoteSeconds * static_cast<float> (getSampleRate());
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
            case 1: // High Cut — low-pass sweep 20 kHz → 500 Hz
            {
                // invert: 0 = fully open (20 kHz), 100 = most closed (500 Hz)
                const float cutoff = juce::jlimit (500.0f, 20000.0f,
                                                   20000.0f * (1.0f - eqSweep / 100.0f)
                                                   + 500.0f  * (eqSweep / 100.0f));
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                eqFilter1.setCutoffFrequency (cutoff);
                eqFilter1.setResonance (0.707f);
                eqFilter1.process (context);
                break;
            }
            case 2: // Vocal — sweepable HP (20–80 Hz) + fixed high shelf (+0 to +6 dB at 10 kHz)
            {
                // Filter 1: low-cut, sweep 20 Hz (knob=0) → 80 Hz (knob=100)
                const float hpFreq = juce::jmap (eqSweep, 0.0f, 100.0f, 20.0f, 80.0f);
                eqFilter1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
                eqFilter1.setCutoffFrequency (hpFreq);
                eqFilter1.setResonance (0.707f);
                eqFilter1.process (context);

                // Filter 2: high shelf at 10 kHz, Q=0.3, gain 1.0 (0 dB) → 1.995 (+6 dB)
                const float shelfGain = juce::jmap (eqSweep, 0.0f, 100.0f, 1.0f, 1.995f);
                *eqFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, 10000.0f, 0.300f, shelfGain);
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

    // 0.5. TONE — 5 saturation/character modes, driven by color_vintage knob (0–100)
    {
        const float drive = colorVintage / 100.0f;   // 0.0 – 1.0

        switch (toneMode)
        {
            case 0: // Clear — identity; tiny soft-limit only to prevent accidental peaks
            {
                if (drive > 0.01f)
                {
                    const float g = 1.0f + drive * 0.08f;
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* p = block.getChannelPointer (static_cast<size_t> (ch));
                        for (int n = 0; n < numSa; ++n)
                            p[n] = std::tanh (p[n] * g) / g;
                    }
                }
                break;
            }

            case 1: // Tape — tanh waveshaper; slider pushes input harder into saturation
            {
                const float inputGain  = 1.0f + drive * 5.0f;           // 1× – 6×
                const float normFactor = 1.0f / std::tanh (inputGain);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* p = block.getChannelPointer (static_cast<size_t> (ch));
                    for (int n = 0; n < numSa; ++n)
                        p[n] = std::tanh (p[n] * inputGain) * normFactor;
                }
                break;
            }

            case 2: // Tube — asymmetric clip (DC bias → even-order harmonics)
            {
                const float tubeDrive  = 1.0f + drive * 6.0f;           // 1× – 7×
                const float bias       = drive * 0.20f;                  // 0 – 0.2 DC offset
                const float normFactor = 1.0f / std::tanh ((1.0f + bias) * tubeDrive);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* p = block.getChannelPointer (static_cast<size_t> (ch));
                    for (int n = 0; n < numSa; ++n)
                        p[n] = std::tanh ((p[n] + bias) * tubeDrive) * normFactor;
                }
                break;
            }

            case 3: // Console — gentle saturation + 250 Hz warmth + 8 kHz sheen
            {
                // Very soft saturation (Neve/SSL glue character)
                const float consoleDrive = 1.0f + drive * 1.5f;         // 1× – 2.5×
                const float normFactor   = 1.0f / std::tanh (consoleDrive);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* p = block.getChannelPointer (static_cast<size_t> (ch));
                    for (int n = 0; n < numSa; ++n)
                        p[n] = std::tanh (p[n] * consoleDrive) * normFactor;
                }
                // Low-mid warmth bell @ 250 Hz (up to +4 dB)
                *toneConsoleLoMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    currentSampleRate, 250.0f, 0.5f, 1.0f + drive * 0.8f);
                toneConsoleLoMid.process (context);

                // Air shelf @ 8 kHz (up to +2 dB)
                *toneConsoleHiShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                    currentSampleRate, 8000.0f, 0.7f, 1.0f + drive * 0.4f);
                toneConsoleHiShelf.process (context);
                break;
            }

            case 4: // Grit — hard clip + subtle bit-depth reduction (MPC/SP-404 crunch)
            {
                if (drive > 0.01f)
                {
                    const float threshold  = juce::jmap (drive, 0.0f, 1.0f, 1.0f, 0.35f);
                    const float bitsRemoved = drive * 3.0f;              // 0 – 3 bits removed
                    const float quantSteps  = std::pow (2.0f, std::max (1.0f, 16.0f - bitsRemoved));

                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* p = block.getChannelPointer (static_cast<size_t> (ch));
                        for (int n = 0; n < numSa; ++n)
                        {
                            // Hard clip then renormalise to unity
                            float x = juce::jlimit (-threshold, threshold, p[n]) / threshold;
                            // Bit reduction
                            if (bitsRemoved > 0.5f)
                                x = std::round (x * quantSteps) / quantSteps;
                            p[n] = x;
                        }
                    }
                }
                break;
            }

            default: break;
        }
    }

    // 1. Phaser (vibe_phaser) -- subtle sweep, fully off at 0
    {
        const float mix   = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f, 0.6f);
        const float depth = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f, 0.5f);
        const float rate  = juce::jmap (vibePhaser, 0.0f, 100.0f, 0.1f, 2.0f);
        phaser.setMix   (mix);
        phaser.setDepth (depth);
        phaser.setRate  (rate);
        phaser.process  (context);
    }

    // 2. Delay (delay_texture) — tempo-synced to 1/4 note; knob controls feedback + wet
    if (delayTexture > 0.1f)
    {
        const float feedback = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.70f);
        const float wetAmt   = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.40f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* ptr = block.getChannelPointer (static_cast<size_t> (ch));
            for (int n = 0; n < numSa; ++n)
            {
                const float in  = ptr[n];
                const float del = delay.popSample (ch);
                delay.pushSample (ch, in + del * feedback);
                ptr[n] = in + del * wetAmt;
            }
        }
    }

    // 3. Reverb — 5 modes, each with distinct character
    if (spaceReverb > 0.1f)
    {
        const int reverbMode = static_cast<int> (std::round (reverbModeParam->load()));
        juce::Reverb::Parameters p;
        p.dryLevel   = 1.0f;
        p.freezeMode = 0.0f;

        switch (reverbMode)
        {
            case 0: // Studio — tight, high damping, mono-ish, post LPF 5 kHz
                p.roomSize = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.15f, 0.30f);
                p.damping  = 0.88f;
                p.wetLevel = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.30f);
                p.width    = 0.50f;
                reverb.setParameters (p);
                reverb.process (context);
                reverbPostFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                reverbPostFilter.setCutoffFrequency (5000.0f);
                reverbPostFilter.setResonance (0.707f);
                reverbPostFilter.process (context);
                break;

            case 1: // Plate — bright, low damping, wide, no post EQ
                p.roomSize = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.55f, 0.75f);
                p.damping  = 0.10f;
                p.wetLevel = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.45f);
                p.width    = 1.0f;
                reverb.setParameters (p);
                reverb.process (context);
                break;

            case 2: // Chamber — balanced, natural, moderate damping
                p.roomSize = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.45f, 0.65f);
                p.damping  = 0.50f;
                p.wetLevel = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.40f);
                p.width    = 0.80f;
                reverb.setParameters (p);
                reverb.process (context);
                break;

            case 3: // Hall — large, long tail, open highs, fully wide
                p.roomSize = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.80f, 1.00f);
                p.damping  = 0.30f;
                p.wetLevel = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.50f);
                p.width    = 1.0f;
                reverb.setParameters (p);
                reverb.process (context);
                break;

            case 4: // Abyss — near-freeze, very long, post LPF @ 600 Hz for darkness
                p.roomSize = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.95f, 1.00f);
                p.damping  = 0.05f;
                p.wetLevel = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.65f);
                p.width    = 1.0f;
                reverb.setParameters (p);
                reverb.process (context);
                reverbPostFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                reverbPostFilter.setCutoffFrequency (600.0f);
                reverbPostFilter.setResonance (0.707f);
                reverbPostFilter.process (context);
                break;

            default: break;
        }
    }

    // 4. Stereo width (stereo_width) -- M/S matrix, 50 = unity
    if (numCh >= 2)
    {
        const float widthGain = juce::jmap (stereoWidth, 0.0f, 100.0f, 0.0f, 2.0f);
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        for (int n = 0; n < numSa; ++n)
        {
            const float mid  = 0.5f * (L[n] + R[n]);
            const float side = 0.5f * (L[n] - R[n]) * widthGain;
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
// Reads live RMS / ZCR metrics and applies a heuristic preset to the APVTS.
// setValueNotifyingHost expects a 0.0–1.0 normalised value.
void AfroplugAudioProcessor::triggerAIAnalysis()
{
    const float rms = currentRMS.load();
    const float zcr = currentZCR.load();

    auto set = [&] (const char* id, float val01)
    {
        apvts.getParameter (id)->setValueNotifyingHost (val01);
    };

    // Helper: map a 0–100 knob value to 0.0–1.0 for a plain 0-100 parameter
    auto norm = [] (float pct) { return pct / 100.0f; };

    // Helper: normalise a choice index into 0.0–1.0 (5 choices → steps 0/4 … 4/4)
    auto modeNorm = [] (int idx) { return static_cast<float> (idx) / 4.0f; };

    if (zcr > 0.15f)
    {
        // Condition A — High ZCR: likely Vocals / Bright Leads
        set ("eq_mode",      modeNorm (2));       // Vocal
        set ("eq_sweep",     norm (70.0f));
        set ("space_reverb", norm (60.0f));
        set ("delay_texture",norm (30.0f));
        set ("vibe_phaser",  norm (20.0f));
    }
    else if (zcr < 0.05f && rms > 0.15f)
    {
        // Condition B — Low ZCR + High RMS: likely Bass / Drums
        set ("eq_mode",      modeNorm (0));       // Low Cut
        set ("eq_sweep",     norm (20.0f));
        set ("color_vintage",norm (80.0f));
        set ("space_reverb", norm (10.0f));
        set ("delay_texture",norm (0.0f));
    }
    else if (rms < 0.05f)
    {
        // Condition C — Mid ZCR + Low RMS: likely Pads / Quiet Guitars
        set ("eq_mode",      modeNorm (4));       // Underwater
        set ("eq_sweep",     norm (50.0f));
        set ("space_reverb", norm (80.0f));
        set ("delay_texture",norm (70.0f));
        set ("stereo_width", norm (90.0f));
    }
    else
    {
        // Condition D — Catch-all / default vibe
        set ("eq_mode",      modeNorm (1));       // High Cut
        set ("eq_sweep",     norm (40.0f));
        set ("color_vintage",norm (50.0f));
        set ("space_reverb", norm (40.0f));
    }
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
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
