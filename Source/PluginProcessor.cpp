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
    colorVintageParam = apvts.getRawParameterValue ("color_vintage");
    vibePhaserParam   = apvts.getRawParameterValue ("vibe_phaser");
    stereoWidthParam  = apvts.getRawParameterValue ("stereo_width");
    spaceReverbParam  = apvts.getRawParameterValue ("space_reverb");
    delayTextureParam = apvts.getRawParameterValue ("delay_texture");
    mixWetParam       = apvts.getRawParameterValue ("mix_wet");
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

    delay.prepare (spec);
    delay.reset();
    delay.setDelay (static_cast<float> (sampleRate * 0.25));

    dryWetMixer.prepare (spec);
    dryWetMixer.reset();
    dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    eqFilter.prepare (spec);
    eqFilter.reset();
    eqFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    eqFilter.setCutoffFrequency (20.0f);   // default: fully open (20 Hz ≈ flat)
    eqFilter.setResonance (0.707f);        // Butterworth Q — no resonant peak
}

void AfroplugAudioProcessor::releaseResources()
{
    phaser.reset();
    reverb.reset();
    delay.reset();
    dryWetMixer.reset();
    eqFilter.reset();
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
    const float vibePhaser   = vibePhaserParam  ->load();
    const float stereoWidth  = stereoWidthParam ->load();
    const float spaceReverb  = spaceReverbParam ->load();
    const float delayTexture = delayTextureParam->load();
    const float mixWet       = mixWetParam      ->load();

    const int numCh = buffer.getNumChannels();
    const int numSa = buffer.getNumSamples();

    juce::dsp::AudioBlock<float>              block   (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // Save dry signal
    dryWetMixer.setWetMixProportion (mixWet / 100.0f);
    dryWetMixer.pushDrySamples (block);

    // 0. EQ high-pass sweep (eq_sweep) — exponential 20 Hz → 2000 Hz
    {
        // At 0 the cutoff sits at 20 Hz (effectively bypassed), at 100 it reaches 2 kHz
        const float freq = 20.0f * std::pow (100.0f, eqSweep / 100.0f);
        eqFilter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, freq));
        eqFilter.process (context);
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

    // 2. Delay (delay_texture) -- dry always passes, echo blended gently
    if (delayTexture > 0.1f)
    {
        const float delaySmpl = std::min (
            juce::jmap (delayTexture, 0.0f, 100.0f, 80.0f, 600.0f) * 0.001f
                * static_cast<float> (currentSampleRate),
            static_cast<float> (192000 - 2));
        const float feedback = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.40f);
        const float wetAmt   = juce::jmap (delayTexture, 0.0f, 100.0f, 0.0f, 0.30f);

        delay.setDelay (delaySmpl);

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

    // 3. Reverb (space_reverb) -- adds ambience, dry preserved
    if (spaceReverb > 0.1f)
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.2f, 0.8f);
        p.damping    = 0.5f;
        p.wetLevel   = juce::jmap (spaceReverb, 0.0f, 100.0f, 0.0f, 0.35f);
        p.dryLevel   = 1.0f;
        p.width      = 0.8f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
        reverb.process (context);
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
