#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AfroplugAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Helper — all 6 parameters share a 0–100 scale for a consistent UI feel.
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
    addParam ("vibe_phaser",    "Vibe / Phaser",     50.0f);  // SPACE
    addParam ("stereo_width",   "Stereo Width",      50.0f);  // SFX
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
    // Cache atomic pointers — valid for the processor lifetime, safe on any thread
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

    // Pre-allocate dry buffer once here — processBlock must never allocate
    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock,
                       /*keepExisting=*/false, /*clearExtra=*/true, /*avoidRealloc=*/false);
}

void AfroplugAudioProcessor::releaseResources() {}

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

    // --- 1. Atomic parameter reads (no locks, no ValueTree on audio thread) ---
    const float colorVintage = colorVintageParam->load();   // 0–100
    const float vibePhaser   = vibePhaserParam  ->load();
    const float stereoWidth  = stereoWidthParam ->load();
    const float spaceReverb  = spaceReverbParam ->load();
    const float delayTexture = delayTextureParam->load();
    const float mixWet       = mixWetParam      ->load();

    const int numCh = buffer.getNumChannels();
    const int numSa = buffer.getNumSamples();

    // --- 2. Save dry signal (pre-allocated, no heap activity) ---
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSa);

    // --- 3. DSP chain (each method is a pass-through stub until Milestone 2) ---
    processPhaser     (buffer, vibePhaser   / 100.0f);
    processSaturation (buffer, colorVintage / 100.0f);
    processReverb     (buffer, spaceReverb  / 100.0f);
    processWidth      (buffer, stereoWidth  / 100.0f);
    processDelay      (buffer, delayTexture / 100.0f);

    // --- 4. Dry / wet blend ---
    const float wet = mixWet / 100.0f;
    const float dry = 1.0f - wet;

    for (int ch = 0; ch < numCh; ++ch)
    {
        buffer.applyGain (ch, 0, numSa, wet);
        buffer.addFrom   (ch, 0, dryBuffer, ch, 0, numSa, dry);
    }
}

//==============================================================================
// DSP scaffold stubs — audio passes through unmodified until Milestone 2.
// Each method receives the buffer and a 0..1 normalised amount.

void AfroplugAudioProcessor::processPhaser (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
{
    // TODO (Milestone 2): LFO-driven all-pass phaser chain
    //   amount → phaser depth / sweep rate
}

void AfroplugAudioProcessor::processSaturation (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
{
    // TODO (Milestone 2): tanh soft-saturation with drive = jmap(amount, 1.f, 5.f)
    //   Pre-gain → waveshaper → compensation gain
}

void AfroplugAudioProcessor::processReverb (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
{
    // TODO (Milestone 2): juce::dsp::Reverb — roomSize / wetLevel scaled by amount
}

void AfroplugAudioProcessor::processWidth (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
{
    // TODO (Milestone 2): mid-side stereo width matrix
    //   amount=0.5 → unity, amount=0 → mono, amount=1 → max width
}

void AfroplugAudioProcessor::processDelay (juce::AudioBuffer<float>& /*buffer*/, float /*amount*/)
{
    // TODO (Milestone 2): tempo-syncable delay line with feedback and texture filter
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
const juce::String AfroplugAudioProcessor::getProgramName (int)       { return {}; }
void  AfroplugAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AfroplugAudioProcessor();
}
