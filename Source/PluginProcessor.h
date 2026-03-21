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

    // Called from the UI thread when the AI button is pressed.
    // Returns a short descriptor of the detected signal type (e.g. "VOCALS").
    juce::String triggerAIAnalysis();

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
    std::atomic<float>* delayDivisionParam { nullptr };   // delay time division (1:2 … 1:8)
    std::atomic<float>* pingPongParam      { nullptr };   // ping-pong on/off
    std::atomic<float>* mixWetParam        { nullptr };   // MIX bar

    //==============================================================================
    // Cached spec — set once in prepareToPlay, used in processBlock
    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // RMS smoothing state (audio thread only)
    float prevSample { 0.0f };

    // DC blocker state — catches any DC offset from asymmetric saturation (Tube mode).
    // 1-pole HP: y[n] = x[n] − x[n−1] + R·y[n−1],  R≈0.9998 ≈ 3 Hz @ 44.1 kHz
    static constexpr int kMaxCh = 2;
    float dcBlockX[kMaxCh] {};
    float dcBlockY[kMaxCh] {};

    // Tape HF roll-off — 1-pole LP applied post-saturation.
    // Simulates the natural high-frequency darkening of magnetic tape.
    // Coefficient computed from sample rate in prepareToPlay.
    float tapeHfLP[kMaxCh]  {};
    float tapeHfLPCoef      { 0.72f };   // default ≈ 9 kHz @ 44.1 kHz

    // Delay feedback LP — filters the feedback signal to remove harsh HF buildup.
    // Real analog delay units have transformer / tape head HF roll-off in the loop.
    float delayFbLP[kMaxCh] {};
    float delayFbLPCoef     { 0.51f };   // default ≈ 5 kHz @ 44.1 kHz

    // Air / OTT — 3-band crossover filter states (base sample rate, per channel).
    // LP1 at 300 Hz isolates the low band; LP2 at 3 kHz splits mid from high.
    float ottLP1[kMaxCh]  {};
    float ottLP2[kMaxCh]  {};
    float ottLP1Coef      { 0.042f };   // recomputed in prepareToPlay
    float ottLP2Coef      { 0.355f };   // recomputed in prepareToPlay

    // Crunch pre-emphasis / de-emphasis state (toneConsoleLoMid / toneConsoleHiShelf)
    // — no extra state vars needed; filters are reused via toneConsoleLoMid / toneConsoleHiShelf

    // Stereo-width side-channel HP — removes sub-200 Hz from side so bass stays
    // mono-compatible. Prevents low-frequency phase cancellation on mono playback.
    float sideHPx1  { 0.0f };   // HP state: previous input sample
    float sideHPy1  { 0.0f };   // HP state: previous output sample
    float sideHPR   { 0.972f }; // pole = exp(-2π·200/sr); recomputed in prepareToPlay

    //==============================================================================
    // DSP modules (juce::dsp namespace)
    //
    // Chain order:  WaveShaper → Phaser → Delay → Reverb → [stereo width] → DryWetMixer
    //
    juce::dsp::Phaser<float>      phaser;
    // Lagrange3pt interpolation removes the pitch-modulation zipper noise from
    // fractional delay reads (much better than the default Linear interpolation).
    juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay { 576000 };
    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::dsp::StateVariableTPTFilter<float> eqFilter1;

    using StereoIIR = juce::dsp::ProcessorDuplicator<
                          juce::dsp::IIR::Filter<float>,
                          juce::dsp::IIR::Coefficients<float>>;
    StereoIIR eqFilter2;
    StereoIIR toneConsoleLoMid;
    StereoIIR toneConsoleHiShelf;

    juce::dsp::WaveShaper<float, std::function<float(float)>> safetyClipper;

    // 4× oversampler — wraps the TONE saturation to prevent aliasing harmonics.
    // filterHalfBandPolyphaseIIR gives near-perfect reconstruction at low CPU cost.
    juce::dsp::Oversampling<float> oversampling {
        2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true   // use integer latency
    };

    // ── FDN Reverb ────────────────────────────────────────────────────────────
    // 4-tap Feedback Delay Network with Householder reflection feedback matrix.
    // L and R channels use slightly different delay lengths for stereo decorrelation.
    // Each tap has a 1-pole LP damping filter whose coefficient controls HF decay rate.
    static constexpr int kFDNTaps = 4;
    struct FDNLine {
        std::vector<float> buf;
        int   writePos  = 0;
        int   delayLen  = 0;   // active read offset (always < buf.size())
        float dampZ     = 0.0f;
    };
    std::array<FDNLine, kFDNTaps> fdnL, fdnR;

    // Preset directory (Documents/Afroplug/Soul FX/Presets)
    juce::File presetDir;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfroplugAudioProcessor)
};
