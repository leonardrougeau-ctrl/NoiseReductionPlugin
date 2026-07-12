#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include "PluginProcessor.h"

class NoiseReductionProcessor;

class SpectrumAnalyzer : public juce::Component, public juce::Timer
{
public:
    explicit SpectrumAnalyzer (NoiseReductionProcessor&);
    ~SpectrumAnalyzer() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Called by the processor when sample rate changes
    void setSampleRate (double newSampleRate);

private:
    NoiseReductionProcessor& processor;

    // Smoothed dB magnitudes (0..1 normalized) for display — computed on UI thread
    std::array<float, NoiseReductionProcessor::ScopeSize> displayedMagnitudes {};
    static constexpr int smoothingPoints = 4;

    double currentSampleRate = 44100.0;

    void drawBackgroundGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyLabels (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDecibelLabels (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSpectrum (juce::Graphics& g, juce::Rectangle<int> bounds);

    // Helper: build a spectrum path (used for both fill and line drawing)
    void buildSpectrumPath (juce::Path& path, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};