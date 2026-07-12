#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NoiseReductionProcessor : public juce::AudioProcessor
{
public:
    static constexpr int FFTOrder = 11; // 2^11 = 2048 samples
    static constexpr int FFTSize  = 1 << FFTOrder;
    static constexpr int ScopeSize = 512;
    static constexpr int hopSize = FFTSize / 2;
    static constexpr int outputRingSize = FFTSize * 8;

    NoiseReductionProcessor();
    ~NoiseReductionProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Lock-free raw FFT data buffer for the spectrum analyzer.
    // Audio thread writes complex pairs (re, im, re, im...) for first ScopeSize bins.
    // UI thread reads in timer callback and computes dB scaling.
    struct FFTDataBuffer
    {
        std::array<float, ScopeSize * 2> complexData {};  // [re,im,re,im,...]
        std::atomic<bool> ready { false };
    };

    FFTDataBuffer& getFFTData() { return fftData; }

    juce::AudioProcessorValueTreeState apvts;

    // Noise reduction state (readable from GUI thread)
    std::atomic<bool> noiseProfileLearned { false };
    std::atomic<bool> noiseProfileRequest { false };

    // Cached raw parameter pointers (one-time lookup, lock-free reads on audio thread)
    std::atomic<float>* reductionParam { nullptr };
    // differenceMode is now an APVTS AudioParameterBool (id: "diffMode") for automatic persistence.
    // Cached as atomic<float>*: 0.0f = normal, 1.0f = difference mode
    std::atomic<float>* differenceModeParam { nullptr };

    // Cached raw parameter pointers for input/output gain
    std::atomic<float>* inputGainParam { nullptr };
    std::atomic<float>* outputGainParam { nullptr };

    // Clear the noise profile (called from GUI thread)
    void clearNoiseProfile()
    {
        std::fill (noiseProfile.begin(), noiseProfile.end(), 0.0f);
        noiseProfileLearned.store (false);
    }

    // Get current sample rate (used by SpectrumAnalyzer for frequency labels)
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    FFTDataBuffer fftData;

    // Per-channel FFT processing buffers
    std::vector<std::vector<float>> fftInputBuffers;   // one per channel
    std::vector<std::vector<float>> fftOutputBuffers;  // one per channel
    std::vector<int> fifoIndices;                       // one per channel

    juce::dsp::FFT forwardFFT { FFTOrder };
    juce::dsp::WindowingFunction<float> window { FFTSize, juce::dsp::WindowingFunction<float>::hann, true };

    // Per-channel overlap-add processing buffers (64-bit counters to avoid wrap issues)
    std::vector<std::vector<float>> olaBuffers;  // one per channel
    std::vector<std::vector<float>> outputRingBuffers; // one per channel
    std::vector<juce::int64> outputWritePositions; // one per channel
    std::vector<juce::int64> outputReadPositions;  // one per channel

    // Noise profile (linear magnitudes, one per FFT bin)
    std::vector<float> noiseProfile;

    // Smoothed reduction parameter to eliminate zipper noise
    juce::SmoothedValue<float> smoothedReduction;

    // Current sample rate (set in prepareToPlay)
    double currentSampleRate = 44100.0;

    void processFrame (int channel);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseReductionProcessor)
};