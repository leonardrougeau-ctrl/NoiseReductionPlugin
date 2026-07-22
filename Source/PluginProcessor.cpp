#include "PluginProcessor.h"
#include "PluginEditor.h"

NoiseReductionProcessor::NoiseReductionProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Reserve space for up to 2 channels (stereo)
    const int initialChannels = 2;

    fftInputBuffers.resize (initialChannels);
    fftOutputBuffers.resize (initialChannels);
    fifoIndices.resize (initialChannels, 0);
    olaBuffers.resize (initialChannels);
    outputRingBuffers.resize (initialChannels);
    outputWritePositions.resize (initialChannels, 0);
    outputReadPositions.resize (initialChannels, 0);

    for (int ch = 0; ch < initialChannels; ++ch)
    {
        fftInputBuffers[ch].resize (FFTSize, 0.0f);
        fftOutputBuffers[ch].resize (FFTSize * 2, 0.0f);
        olaBuffers[ch].resize (FFTSize, 0.0f);
        outputRingBuffers[ch].resize (outputRingSize, 0.0f);
    }

    noiseProfile.resize (FFTSize / 2, 0.0f);

    // One-time cache of raw parameter pointers for lock-free audio thread access
    reductionParam = apvts.getRawParameterValue ("reduction");
    differenceModeParam = apvts.getRawParameterValue ("diffMode");
    inputGainParam = apvts.getRawParameterValue ("inputGain");
    outputGainParam = apvts.getRawParameterValue ("outputGain");
}

NoiseReductionProcessor::~NoiseReductionProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
NoiseReductionProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "reduction", "Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        "diffMode", "Difference Mode", false));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "inputGain", "Input Gain",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f),
        1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f),
        1.0f));

    return layout;
}

void NoiseReductionProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Resize per-channel buffers to match actual number of channels
    const int numChannels = getTotalNumInputChannels();

    fftInputBuffers.resize (numChannels);
    fftOutputBuffers.resize (numChannels);
    fifoIndices.resize (numChannels, 0);
    olaBuffers.resize (numChannels);
    outputRingBuffers.resize (numChannels);
    outputWritePositions.resize (numChannels, 0);
    outputReadPositions.resize (numChannels, 0);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        fftInputBuffers[ch].assign (FFTSize, 0.0f);
        fftOutputBuffers[ch].assign (FFTSize * 2, 0.0f);
        olaBuffers[ch].assign (FFTSize, 0.0f);
        outputRingBuffers[ch].assign (outputRingSize, 0.0f);
        fifoIndices[ch] = 0;
        outputWritePositions[ch] = 0;
        outputReadPositions[ch] = 0;
    }

    // Clear FFT data buffer (not the noise profile — that persists across bypass/re-enable)
    std::fill (fftData.complexData.begin(), fftData.complexData.end(), 0.0f);
    noiseProfileRequest.store (false);
    fftData.ready.store (false);

    // Set up the smoothed reduction parameter
    // Use ramp length of ~50ms for smooth transitions
    smoothedReduction.reset (sampleRate, 0.05);
    smoothedReduction.setCurrentAndTargetValue (reductionParam->load() / 100.0f);

    setLatencySamples (FFTSize);
}

void NoiseReductionProcessor::releaseResources() {}

bool NoiseReductionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void NoiseReductionProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // Apply input gain
    const float inGain = inputGainParam->load();
    if (inGain != 1.0f)
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
            buffer.applyGain (ch, 0, numSamples, inGain);
    }

    // Update the smoothed reduction target from the parameter
    smoothedReduction.setTargetValue (reductionParam->load() / 100.0f);

    // Process each channel independently for true stereo
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* channelData = buffer.getReadPointer (ch);

        // Feed input samples into the FFT FIFO and process complete frames
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = channelData[i];
            fftInputBuffers[ch][static_cast<size_t>(fifoIndices[ch]++)] = sample;

            if (fifoIndices[ch] == FFTSize)
            {
                processFrame (ch);

                // Shift by hop size for overlap-add
                fifoIndices[ch] = hopSize;
                std::memmove (fftInputBuffers[ch].data(),
                              fftInputBuffers[ch].data() + hopSize,
                              static_cast<size_t>(hopSize) * sizeof (float));
            }
        }

        // Read processed output from the ring buffer
        auto* outChannelData = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float outSample = 0.0f;
            if (outputReadPositions[ch] < outputWritePositions[ch])
            {
                outSample = outputRingBuffers[ch][static_cast<size_t>(outputReadPositions[ch] % outputRingSize)];
                outputReadPositions[ch]++;
            }

            outChannelData[i] = outSample;
        }
    }

    // Clear any extra output channels (e.g., if output has more channels than input)
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Apply output gain
    const float outGain = outputGainParam->load();
    if (outGain != 1.0f)
    {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            buffer.applyGain (ch, 0, numSamples, outGain);
    }

    // Safety hard-clipper: prevent any samples from exceeding ±1.0
    // Spectral subtraction's overlap-add IFFT can produce transient peaks
    // that exceed 0 dBFS even with unity gain, due to phase cancellation
    // and the double-Hann window reconstruction.
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (channelData[i] > 1.0f)
                channelData[i] = 1.0f;
            else if (channelData[i] < -1.0f)
                channelData[i] = -1.0f;
        }
    }
}

void NoiseReductionProcessor::processFrame (int channel)
{
    // 1. Copy input to working buffer and apply analysis window
    std::copy (fftInputBuffers[channel].begin(),
               fftInputBuffers[channel].begin() + FFTSize,
               fftOutputBuffers[channel].begin());
    window.multiplyWithWindowingTable (fftOutputBuffers[channel].data(),
                                       static_cast<size_t>(FFTSize));

    // 2. Zero-pad for complex FFT (imaginary parts in second half)
    std::fill (fftOutputBuffers[channel].begin() + FFTSize,
               fftOutputBuffers[channel].end(),
               0.0f);

    // 3. Forward FFT
    forwardFFT.performRealOnlyForwardTransform (fftOutputBuffers[channel].data(), true);

    // 4. Publish raw complex FFT data for the spectrum analyzer (channel 0 only).
    //    The magnitude/dB computation (expensive sqrt + log10) is done on the
    //    UI thread in SpectrumAnalyzer::timerCallback(), NOT on the audio thread.
    //    This eliminates a major source of audio dropouts.
    if (channel == 0)
    {
        for (int i = 0; i < ScopeSize; ++i)
        {
            fftData.complexData[static_cast<size_t>(2 * i)] =
                fftOutputBuffers[channel][static_cast<size_t>(2 * i)];
            fftData.complexData[static_cast<size_t>(2 * i + 1)] =
                fftOutputBuffers[channel][static_cast<size_t>(2 * i + 1)];
        }
        fftData.ready.store (true);
    }

    // 5. Capture noise profile if requested by the GUI
    //    Uses atomic exchange (no mutex) for lock-free GUI->audio communication
    if (channel == 0 && noiseProfileRequest.exchange (false))
    {
        for (int i = 0; i < FFTSize / 2; ++i)
        {
            float real = fftOutputBuffers[channel][static_cast<size_t>(2 * i)];
            float imag = fftOutputBuffers[channel][static_cast<size_t>(2 * i + 1)];
            noiseProfile[static_cast<size_t>(i)] = std::sqrt (real * real + imag * imag);
        }
        noiseProfileLearned.store (true);
    }

    // 6. Apply spectral subtraction using the stored noise profile
    //    Lock-free reads from cached atomic pointer + atomic<bool> flags
    const float reduction = smoothedReduction.getNextValue();
    const bool diffMode = differenceModeParam->load() > 0.5f;

    if (noiseProfileLearned.load() && reduction > 0.0f)
    {
        constexpr float overSubtraction = 3.0f;
        constexpr float spectralFloor = 0.02f;
        const int numBins = FFTSize / 2;

        for (int i = 0; i < numBins; ++i)
        {
            float real = fftOutputBuffers[channel][static_cast<size_t>(2 * i)];
            float imag = fftOutputBuffers[channel][static_cast<size_t>(2 * i + 1)];

            // Precompute magnitude squared to avoid sqrt when gain would be 1.0
            float magSq = real * real + imag * imag;
            float currentMag = std::sqrt (magSq);
            float nProfile = noiseProfile[static_cast<size_t>(i)];

            // Compute gain: subtract noise floor scaled by reduction amount
            float gain = 1.0f;
            if (currentMag > 1e-10f)
                gain = std::max (1.0f - overSubtraction * reduction * nProfile / currentMag,
                                 spectralFloor);

            if (diffMode)
            {
                // Output only the removed audio (noise that was subtracted)
                fftOutputBuffers[channel][static_cast<size_t>(2 * i)]     = real * (1.0f - gain);
                fftOutputBuffers[channel][static_cast<size_t>(2 * i + 1)] = imag * (1.0f - gain);
            }
            else
            {
                // Normal mode: output the cleaned audio
                fftOutputBuffers[channel][static_cast<size_t>(2 * i)]     = real * gain;
                fftOutputBuffers[channel][static_cast<size_t>(2 * i + 1)] = imag * gain;
            }
        }
    }

    // 7. Inverse FFT to get time-domain signal back
    forwardFFT.performRealOnlyInverseTransform (fftOutputBuffers[channel].data());

    // 8. Apply synthesis window
    window.multiplyWithWindowingTable (fftOutputBuffers[channel].data(),
                                       static_cast<size_t>(FFTSize));

    // 9. Overlap-add: accumulate into the OLA buffer
    for (int i = 0; i < FFTSize; ++i)
        olaBuffers[channel][static_cast<size_t>(i)] += fftOutputBuffers[channel][static_cast<size_t>(i)];

    // 10. Write the first hopSize accumulated samples to the output ring buffer
    for (int i = 0; i < hopSize; ++i)
    {
        outputRingBuffers[channel][static_cast<size_t>((outputWritePositions[channel] + i) % outputRingSize)] =
            olaBuffers[channel][static_cast<size_t>(i)];
    }
    outputWritePositions[channel] += hopSize;

    // 11. Shift the OLA buffer by hopSize for the next frame
    std::memmove (olaBuffers[channel].data(),
                  olaBuffers[channel].data() + hopSize,
                  static_cast<size_t>(hopSize) * sizeof (float));
    std::fill (olaBuffers[channel].begin() + hopSize, olaBuffers[channel].end(), 0.0f);
}

juce::AudioProcessorEditor* NoiseReductionProcessor::createEditor()
{
    return new NoiseReductionProcessorEditor (*this);
}

void NoiseReductionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save APVTS parameters (reduction, diffMode, inputGain, outputGain)
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // Save noise profile data
    // Store noiseProfileLearned flag and the noise profile vector
    xml->setAttribute ("noiseProfileLearned", noiseProfileLearned.load());

    juce::String profileStr;
    for (float val : noiseProfile)
        profileStr += juce::String (val) + ",";

    xml->setAttribute ("noiseProfile", profileStr);

    copyXmlToBinary (*xml, destData);
}

void NoiseReductionProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        // Restore noise profile data
        bool learned = xml->getBoolAttribute ("noiseProfileLearned", false);
        noiseProfileLearned.store (learned);

        juce::String profileStr = xml->getStringAttribute ("noiseProfile", "");
        if (profileStr.isNotEmpty())
        {
            juce::StringArray tokens;
            tokens.addTokens (profileStr, ",", "");
            int numValues = juce::jmin (tokens.size(), static_cast<int>(noiseProfile.size()));
            for (int i = 0; i < numValues; ++i)
                noiseProfile[static_cast<size_t>(i)] = tokens[i].getFloatValue();
        }
    }
}

// Required: create the static instance that the framework uses
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseReductionProcessor();
}