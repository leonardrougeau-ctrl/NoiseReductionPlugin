#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer (NoiseReductionProcessor& p)
    : processor (p)
{
    displayedMagnitudes.fill (0.0f);
    currentSampleRate = processor.getCurrentSampleRate();
    startTimerHz (30); // 30 FPS update rate
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::setSampleRate (double newSampleRate)
{
    currentSampleRate = newSampleRate;
}

void SpectrumAnalyzer::timerCallback()
{
    // Read raw complex FFT data published by the audio thread
    // The expensive dB magnitude computation happens HERE (UI thread),
    // NOT on the audio thread.
    auto& fftData = processor.getFFTData();

    if (fftData.ready.load())
    {
        // Compute dB magnitudes from raw complex pairs [re, im, re, im...]
        constexpr float minDb = -100.0f;
        constexpr float maxDb = 0.0f;

        for (size_t i = 0; i < displayedMagnitudes.size(); ++i)
        {
            float real = fftData.complexData[2 * i];
            float imag = fftData.complexData[2 * i + 1];
            float magnitude = std::sqrt (real * real + imag * imag);
            float db = 20.0f * std::log10 (std::max (magnitude, 1e-20f));
            float newMag = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));

            // Apply exponential smoothing for visual stability
            constexpr float alpha = 0.3f;
            displayedMagnitudes[i] += alpha * (newMag - displayedMagnitudes[i]);
        }

        fftData.ready.store (false);
        repaint();
    }
}

void SpectrumAnalyzer::resized() {}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto analyzerArea = bounds;

    // Background
    g.fillAll (juce::Colour (0xff0f0f23));

    // Draw grid, labels, and spectrum
    drawBackgroundGrid (g, analyzerArea);
    drawFrequencyLabels (g, analyzerArea);
    drawDecibelLabels (g, analyzerArea);
    drawSpectrum (g, analyzerArea);
}

void SpectrumAnalyzer::drawBackgroundGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    const int margin = 50;
    const int plotWidth = width - margin - 10;
    const int plotHeight = height - 30; // leave room for frequency labels
    const int plotX = margin;
    const int plotY = 5;

    g.setColour (juce::Colour (0xff1a1a3e));

    // Horizontal grid lines (dB levels: 0, -20, -40, -60, -80, -100 dB)
    for (int db = 0; db >= -100; db -= 20)
    {
        float normPos = static_cast<float>(db + 100) / 100.0f;
        int y = plotY + static_cast<int>((1.0f - normPos) * static_cast<float>(plotHeight));
        g.drawLine (static_cast<float>(plotX), static_cast<float>(y),
                     static_cast<float>(plotX + plotWidth), static_cast<float>(y), 1.0f);
    }

    // Vertical grid lines (frequency decades and sub-divisions)
    const double frequencies[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const double sampleRate = currentSampleRate;

    for (double freq : frequencies)
    {
        if (freq >= sampleRate / 2.0)
            break;

        float normPos = static_cast<float>(freq / (sampleRate / 2.0));
        // Use log scale for frequency axis
        float logNorm = std::log10 (normPos * 9.0f + 1.0f) / std::log10 (10.0f);
        int x = plotX + static_cast<int>(logNorm * static_cast<float>(plotWidth));

        g.drawLine (static_cast<float>(x), static_cast<float>(plotY),
                     static_cast<float>(x), static_cast<float>(plotY + plotHeight), 1.0f);
    }

    // Border around plot area
    g.setColour (juce::Colour (0xff2a2a5e));
    g.drawRect (static_cast<float>(plotX), static_cast<float>(plotY),
                static_cast<float>(plotWidth), static_cast<float>(plotHeight), 1.0f);
}

void SpectrumAnalyzer::drawFrequencyLabels (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    const int margin = 50;
    const int plotWidth = width - margin - 10;
    const int plotY = 5;
    const int plotHeight = height - 30;

    g.setColour (juce::Colour (0xff8888aa));
    g.setFont (10.0f);

    const double sampleRate = currentSampleRate;
    const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    const double frequencies[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (int i = 0; i < 9; ++i)
    {
        if (frequencies[i] >= sampleRate / 2.0)
            break;

        float normPos = static_cast<float>(frequencies[i] / (sampleRate / 2.0));
        float logNorm = std::log10 (normPos * 9.0f + 1.0f) / std::log10 (10.0f);
        int x = margin + static_cast<int>(logNorm * static_cast<float>(plotWidth));

        g.drawText (labels[i], x - 15, plotY + plotHeight + 2, 30, 20,
                     juce::Justification::centred, false);
    }

    // "Hz" label
    g.drawText ("Hz", width - 30, plotY + plotHeight + 2, 30, 20,
                 juce::Justification::centredLeft, false);
}

void SpectrumAnalyzer::drawDecibelLabels (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const int height = bounds.getHeight();
    const int plotY = 5;
    const int plotHeight = height - 30;

    g.setColour (juce::Colour (0xff8888aa));
    g.setFont (10.0f);

    for (int db = 0; db >= -100; db -= 20)
    {
        float normPos = static_cast<float>(db + 100) / 100.0f;
        int y = plotY + static_cast<int>((1.0f - normPos) * static_cast<float>(plotHeight));

        juce::String label = juce::String (db) + " dB";
        g.drawText (label, 0, y - 8, 48, 16, juce::Justification::centredRight, false);
    }
}

void SpectrumAnalyzer::buildSpectrumPath (juce::Path& path, juce::Rectangle<int> bounds)
{
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    const int margin = 50;
    const int plotWidth = width - margin - 10;
    const int plotHeight = height - 30;
    const int plotX = margin;
    const int plotY = 5;

    const double sampleRate = currentSampleRate;
    constexpr int numBins = NoiseReductionProcessor::ScopeSize;

    bool started = false;

    for (int pixel = 0; pixel < plotWidth; ++pixel)
    {
        // Map pixel to log-frequency bin
        float logNorm = static_cast<float>(pixel) / static_cast<float>(plotWidth);
        float freqNorm = (std::pow (10.0f, logNorm) - 1.0f) / 9.0f;
        float freq = freqNorm * static_cast<float>(sampleRate / 2.0);

        // Map frequency to FFT bin index
        float binFloat = freq / static_cast<float>(sampleRate / 2.0f)
                         * static_cast<float>(numBins);
        int binIndex = static_cast<int>(binFloat);

        if (binIndex >= numBins)
            binIndex = numBins - 1;
        if (binIndex < 0)
            binIndex = 0;

        float magnitude = displayedMagnitudes[static_cast<size_t>(binIndex)];

        // Average with neighbors for smoothing
        if (binIndex > 0 && binIndex < numBins - 1)
        {
            magnitude = (displayedMagnitudes[static_cast<size_t>(binIndex - 1)] * 0.25f
                       + displayedMagnitudes[static_cast<size_t>(binIndex)] * 0.5f
                       + displayedMagnitudes[static_cast<size_t>(binIndex + 1)] * 0.25f);
        }

        float x = static_cast<float>(plotX + pixel);
        float y = static_cast<float>(plotY) +
                  static_cast<float>(plotHeight) * (1.0f - magnitude);

        if (! started)
        {
            path.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            path.lineTo (x, y);
        }
    }
}

void SpectrumAnalyzer::drawSpectrum (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    const int margin = 50;
    const int plotWidth = width - margin - 10;
    const int plotHeight = height - 30;
    const int plotX = margin;
    const int plotY = 5;

    // Build the spectrum curve path using the shared helper
    juce::Path spectrumPath;
    buildSpectrumPath (spectrumPath, bounds);

    // Create gradient fill under the spectrum
    juce::ColourGradient gradient (juce::Colour (0xff00d4ff), 0.0f, static_cast<float>(plotY),
                                    juce::Colour (0xff0066ff), 0.0f,
                                    static_cast<float>(plotY + plotHeight), false);

    // Fill area under the curve: close the path to the bottom corners
    spectrumPath.lineTo (static_cast<float>(plotX + plotWidth),
                          static_cast<float>(plotY + plotHeight));
    spectrumPath.lineTo (static_cast<float>(plotX),
                          static_cast<float>(plotY + plotHeight));
    spectrumPath.closeSubPath();

    g.setGradientFill (gradient);
    g.fillPath (spectrumPath);

    // Draw the spectrum line on top (brighter) — rebuild just the curve
    juce::Path spectrumLine;
    buildSpectrumPath (spectrumLine, bounds);

    g.setColour (juce::Colour (0xff00d4ff));
    g.strokePath (spectrumLine, juce::PathStrokeType (2.0f));
}