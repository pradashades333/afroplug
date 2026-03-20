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

    // Called from the UI thread when the AI button is pressed
    void triggerAIAnalysis();

    // Live audio-feature meters (updated every processBlock, read on UI thread)
    std::atomic<float> currentRMS { 0.0f };
    std::atomic<float> currentZCR { 0.0f };

    // =========================================================================
    // Preset management — all called from the UI / message thread
    // =========================================================================
    juce::StringArray getAvailablePresets() const;
    void loadPreset  (const juce::String& presetName);
    void savePreset  (const juce::String& presetName);

private:
    //==============================================================================
    // Atomic parameter pointers — read directly on the audio thread, no locks
    std::atomic<float>* eqSweepParam       { nullptr };   // EQ sweep (0–100)
    std::atomic<float>* eqModeParam        { nullptr };   // EQ mode (0–4)
    std::atomic<float>* reverbModeParam    { nullptr };   // Reverb mode (0–4)
    std::atomic<float>* colorVintageParam  { nullptr };   // TONE knob (drive)
    std::atomic<float>* toneModeParam      { nullptr };   // Tone mode (0–4)
    std::atomic<float>* vibePhaserParam    { nullptr };   // SPACE row
    std::atomic<float>* stereoWidthParam   { nullptr };   // SFX row
    std::atomic<float>* spaceReverbParam   { nullptr };   // REVERB knob
    std::atomic<float>* delayTextureParam  { nullptr };   // DELAY knob
    std::atomic<float>* mixWetParam        { nullptr };   // MIX bar

    //==============================================================================
    // Cached spec — set once in prepareToPlay, used in processBlock
    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // RMS smoothing state (audio thread only)
    float prevSample { 0.0f };

    //==============================================================================
    // DSP modules (juce::dsp namespace)
    //
    // Chain order:  WaveShaper → Phaser → Delay → Reverb → [stereo width] → DryWetMixer
    //
    juce::dsp::Phaser<float>      phaser;
    juce::dsp::Reverb             reverb;
    juce::dsp::DelayLine<float>   delay { 576000 };  // max 3 s @ 192 kHz
    juce::dsp::DryWetMixer<float>                    dryWetMixer;
    juce::dsp::StateVariableTPTFilter<float> eqFilter1;   // primary EQ (HP/LP/BP)

    // High-shelf for Vocal mode — ProcessorDuplicator gives stereo support for IIR
    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;
    StereoIIR eqFilter2;

    // Post-reverb filter — used for Studio (LPF 5 kHz) and Abyss (LPF 600 Hz)
    juce::dsp::StateVariableTPTFilter<float> reverbPostFilter;

    // Console mode EQ — low-mid bell @ 250 Hz + high shelf @ 8 kHz
    StereoIIR toneConsoleLoMid;
    StereoIIR toneConsoleHiShelf;

    // Safety soft clipper — last in chain, keeps output below 0 dBFS
    juce::dsp::WaveShaper<float, std::function<float(float)>> safetyClipper;

    // Preset directory (Documents/Afroplug/Soul FX/Presets)
    juce::File presetDir;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfroplugAudioProcessor)
};
