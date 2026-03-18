#pragma once

#include <JuceHeader.h>

//==============================================================================
class AfroplugAudioProcessor : public juce::AudioProcessor
{
public:
    AfroplugAudioProcessor();
    ~AfroplugAudioProcessor() override;

    //==============================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName()        const override;
    bool  acceptsMidi()                 const override;
    bool  producesMidi()                const override;
    bool  isMidiEffect()                const override;
    double getTailLengthSeconds()       const override;

    //==============================================================================
    int  getNumPrograms()                                           override;
    int  getCurrentProgram()                                        override;
    void setCurrentProgram (int)                                    override;
    const juce::String getProgramName (int)                         override;
    void changeProgramName (int, const juce::String&)               override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData)          override;
    void setStateInformation (const void* data, int sizeInBytes)    override;

    //==============================================================================
    // Public APVTS — PluginEditor attaches all 6 sliders to it
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    // Atomic parameter pointers — read directly on the audio thread, no locks
    std::atomic<float>* eqSweepParam       { nullptr };   // EQ
    std::atomic<float>* colorVintageParam  { nullptr };   // TONE
    std::atomic<float>* vibePhaserParam    { nullptr };   // SPACE row
    std::atomic<float>* stereoWidthParam   { nullptr };   // SFX row
    std::atomic<float>* spaceReverbParam   { nullptr };   // REVERB knob
    std::atomic<float>* delayTextureParam  { nullptr };   // DELAY knob
    std::atomic<float>* mixWetParam        { nullptr };   // MIX bar

    //==============================================================================
    // Cached spec — set once in prepareToPlay, used in processBlock
    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    //==============================================================================
    // DSP modules (juce::dsp namespace)
    //
    // Chain order:  WaveShaper → Phaser → Delay → Reverb → [stereo width] → DryWetMixer
    //
    juce::dsp::Phaser<float>      phaser;
    juce::dsp::Reverb             reverb;
    juce::dsp::DelayLine<float>   delay { 192000 };  // max ~4 s @ 48 kHz
    juce::dsp::DryWetMixer<float>                   dryWetMixer;
    juce::dsp::StateVariableTPTFilter<float>         eqFilter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfroplugAudioProcessor)
};
