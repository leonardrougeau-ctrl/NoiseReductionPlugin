#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"

class NoiseReductionProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NoiseReductionProcessorEditor (NoiseReductionProcessor&);
    ~NoiseReductionProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NoiseReductionProcessor& audioProcessor;

    SpectrumAnalyzer spectrumAnalyzer;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // --- Controls ---
    juce::Label controlsSectionLabel;

    // Reduction knob
    juce::Slider reductionKnob;
    juce::Label reductionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reductionAttachment;

    // Input gain knob
    juce::Slider inputGainKnob;
    juce::Label inputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;

    // Output gain knob
    juce::Slider outputGainKnob;
    juce::Label outputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    juce::TextButton learnNoiseBtn;
    juce::TextButton clearProfileBtn;
    juce::ToggleButton differenceModeBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diffModeAttachment;
    juce::Label statusLabel;

    // Custom dark look-and-feel for the rotary knob
    struct DarkLookAndFeel : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider& slider) override;
    };

    DarkLookAndFeel darkLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseReductionProcessorEditor)
};