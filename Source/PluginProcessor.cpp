#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AfroplugAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // All 6 parameters share a 0–100 scale for a consistent UI feel.
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

    addParam ("color_vintage",  "Color / Vintage",  50.0f);  // TONE
    addParam ("vibe_phaser",    "Vibe / Phaser",     50.0f);  // SPACE row
    addParam ("stereo_width",   "Stereo Width",      50.0f);  // SFX row
    addParam ("space_reverb",   "Space / Reverb",    28.0f);  // REVERB knob
    addParam ("delay_texture",  "Delay / Texture",   22.0f);  // DELAY knob
    addParam ("mix_wet",        "Mix (Wet)",         62.0f);  // MIX bar

    return layout;
}

//==============================================================================
AfroplugAudioProcessor::AfroplugAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AFROPLUG_STATE", createParameterLayout())
{
    // Cache atomic pointers — valid for the processor lifetime
    colorVintageParam = apvts.getRawParameterValue ("color_vintage");
    vibePhaserParam   = apvts.getRawParameterValue ("vibe_phaser");
    stereoWidthParam  = apvts.getRawParameterValue ("stereo_width");
    spaceReverbParam  = apvts.getRawParameterValue ("space_reverb");
    delayTextureParam = apvts.getRawParameterValue ("delay_texture");
    mixWetParam       = apvts.getRawParameterValue ("mix_wet");

    // Set the waveshaper transfer function once — tanh soft saturation.
    // Drive scaling is applied via block.multiplyBy() in processBlock so
    // this lambda itself never changes and is never allocated on the audio thread.
    waveShaper.functionToUse = [] (float x) { return std::tanh (x); };
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

    // ── Phaser ────────────────────────────────────────────────────────────────
    phaser.prepare (spec);
    phaser.reset();
    // Fixed character settings — only rate/depth change per block
    phaser.setCentreFrequency (1000.0f);
    phaser.setFeedback        (0.7f);
    phaser.setMix             (1.0f);  // 100% wet; dryWetMixer owns the final blend

    // ── WaveShaper ────────────────────────────────────────────────────────────
    waveShaper.prepare (spec);
    waveShaper.reset();
    // functionToUse is set in the constructor — no allocation here

    // ── Reverb ────────────────────────────────────────────────────────────────
    reverb.prepare (spec);
    reverb.reset();

    // ── Delay ─────────────────────────────────────────────────────────────────
    delay.prepare (spec);
    delay.reset();
    // Set a safe default delay of 400 ms, capped to the DelayLine's 44100-sample max
    const float defaultDelaySamples = std::min (static_cast<float> (sampleRate * 0.4),
                                                44098.0f);
    delay.setDelay (defaultDelaySamples);

    // ── DryWetMixer ───────────────────────────────────────────────────────────
    dryWetMixer.prepare (spec);
    dryWetMixer.reset();
    dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
}

void AfroplugAudioProcessor::releaseResources()
{
    phaser.reset();
    waveShaper.reset();
    reverb.reset();
    delay.reset();
    dryWetMixer.reset();
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

    // ── 1. Atomic parameter reads — no locks, no ValueTree on the audio thread ──
    const float colorVintage = colorVintageParam->load();  // 0–100
    const float vibePhaser   = vibePhaserParam  ->load();
    const float stereoWidth  = stereoWidthParam ->load();
    const float spaceReverb  = spaceReverbParam ->load();
    const float delayTexture = delayTextureParam->load();
    const float mixWet       = mixWetParam      ->load();

    const int numCh = buffer.getNumChannels();
    const int numSa = buffer.getNumSamples();

    // ── 2. AudioBlock view + processing context ──────────────────────────────
    juce::dsp::AudioBlock<float>            block   (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // ── 3. Save dry signal into the mixer before any wet processing ───────────
    dryWetMixer.setWetMixProportion (mixWet / 100.0f);
    dryWetMixer.pushDrySamples (block);

    // ── 4. WaveShaper — tanh soft saturation (color_vintage) ─────────────────
    // Skip at zero so the fully-dry pass is bit-exact.
    if (colorVintage > 0.5f)
    {
        // drive:       1.0 (barely touched) → 5.0 (heavy clip)
        // compensation: normalises peak output back to ±1.0 so downstream
        //               modules see consistent levels regardless of drive.
        const float drive   = juce::jmap (colorVintage, 0.0f, 100.0f, 1.0f, 5.0f);
        const float invComp = 1.0f / std::tanh (drive);

        block.multiplyBy (drive);          // pre-gain — no allocation, no heap
        waveShaper.process (context);      // applies tanh() per sample
        block.multiplyBy (invComp);        // restore perceived level
    }

    // ── 5. Phaser — LFO-driven all-pass sweep (vibe_phaser) ──────────────────
    phaser.setRate  (juce::jmap (vibePhaser, 0.0f, 100.0f, 0.1f, 4.0f));  // Hz
    phaser.setDepth (juce::jmap (vibePhaser, 0.0f, 100.0f, 0.0f, 1.0f));
    phaser.process (context);

    // ── 6. Delay with feedback — manual per-sample loop (delay_texture) ──────
    // DelayLine has no built-in feedback path, so we implement it explicitly.
    // All arithmetic is on stack scalars — no allocation.
    {
        // Map 0–100 to 10 ms–500 ms, capped to the 44100-sample DelayLine max
        const float delayMs      = juce::jmap (delayTexture, 0.0f, 100.0f, 10.0f, 500.0f);
        const float delaySamples = std::min (static_cast<float> (delayMs * 0.001 * currentSampleRate),
                                             44098.0f);
        const float feedback     = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.65f);

        delay.setDelay (delaySamples);  // interpolated — safe to call per block

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* ptr = block.getChannelPointer (static_cast<size_t> (ch));

            for (int n = 0; n < numSa; ++n)
            {
                const float inputSample   = ptr[n];
                const float delayedSample = delay.popSample (ch);           // read first …
                delay.pushSample (ch, inputSample + delayedSample * feedback); // … then write
                ptr[n] = delayedSample;                                      // wet output
            }
        }
    }

    // ── 7. Reverb — room size and damping driven by space_reverb ─────────────
    // wetLevel = 1 / dryLevel = 0: the reverb runs fully wet in the chain;
    // the actual dry/wet balance is owned by dryWetMixer at the end.
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.1f, 0.9f);
        p.damping    = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.8f);
        p.wetLevel   = 1.0f;
        p.dryLevel   = 0.0f;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }
    reverb.process (context);

    // ── 8. Stereo width — Mid/Side matrix (stereo_width) ─────────────────────
    // width = 50  →  widthGain = 1.0 (unity, no change)
    // width = 0   →  widthGain = 0.0 (full mono collapse)
    // width = 100 →  widthGain = 2.0 (doubled side signal)
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

    // ── 9. Final dry/wet blend ────────────────────────────────────────────────
    // mixWetSamples adds the saved dry signal (from pushDrySamples) back into
    // the block according to the proportion set in step 3.
    dryWetMixer.mixWetSamples (block);
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
