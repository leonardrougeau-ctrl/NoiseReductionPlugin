#include "PluginEditor.h"

// ============================================================================
// DarkLookAndFeel: custom rotary knob drawing
// ============================================================================
void NoiseReductionProcessorEditor::DarkLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    auto radius = static_cast<float> (juce::jmin (width, height)) / 2.0f - 4.0f;
    auto centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    auto centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Background circle
    g.setColour (juce::Colour (0xff2a2a4e));
    g.fillEllipse (rx, ry, rw, rw);

    // Track arc (full range, dim)
    {
        juce::Path track;
        track.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff1a1a3e));
        g.strokePath (track, juce::PathStrokeType (4.0f));
    }

    // Value arc (current value, bright teal)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0xff00d4ff));
        g.strokePath (valueArc, juce::PathStrokeType (4.0f));
    }

    // Outline ring
    g.setColour (juce::Colour (0xff00d4ff).withAlpha (0.5f));
    g.drawEllipse (rx, ry, rw, rw, 1.5f);

    // Pointer line
    {
        juce::Path pointer;
        auto pointerLength = radius * 0.55f;
        auto pointerThickness = 2.5f;
        pointer.addRectangle (-pointerThickness * 0.5f, -pointerLength,
                              pointerThickness, pointerLength);
        pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                    .translated (centreX, centreY));
        g.setColour (juce::Colour (0xff00d4ff));
        g.fillPath (pointer);
    }
}

// ============================================================================
// Editor constructor / destructor
// ============================================================================
NoiseReductionProcessorEditor::NoiseReductionProcessorEditor (NoiseReductionProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p), spectrumAnalyzer (p)
{
    setSize (1000, 550);
    setLookAndFeel (&darkLnf);

    // --- Title ---
    titleLabel.setText ("Noise Reduction", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Spectral Subtraction", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (13.0f, juce::Font::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    subtitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitleLabel);

    // --- Spectrum Analyzer ---
    addAndMakeVisible (spectrumAnalyzer);

    // --- Controls Section ---
    controlsSectionLabel.setText ("CONTROLS", juce::dontSendNotification);
    controlsSectionLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    controlsSectionLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00d4ff));
    controlsSectionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (controlsSectionLabel);

    // Helper lambda to configure a rotary knob
    auto configureKnob = [this](juce::Slider& knob, juce::Label& label, const juce::String& text,
                                const juce::String& suffix)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        knob.setTextValueSuffix (suffix);
        knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00d4ff));
        knob.setColour (juce::Slider::thumbColourId, juce::Colour (0xff00d4ff));
        knob.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        knob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff2a2a4e));
        knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (knob);

        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (12.0f, juce::Font::plain));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };

    // --- Reduction knob ---
    configureKnob (reductionKnob, reductionLabel, "Reduction", "%");
    reductionKnob.setTooltip ("Amount of noise reduction to apply (0% = no reduction, 100% = maximum)");
    reductionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "reduction", reductionKnob);

    // --- Input Gain knob ---
    configureKnob (inputGainKnob, inputGainLabel, "Input Gain", "x");
    inputGainKnob.setTooltip ("Boost or cut the input signal before processing (0.0x to 2.0x)");
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "inputGain", inputGainKnob);

    // --- Output Gain knob ---
    configureKnob (outputGainKnob, outputGainLabel, "Output Gain", "x");
    outputGainKnob.setTooltip ("Boost or cut the output signal after processing (0.0x to 2.0x)");
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "outputGain", outputGainKnob);

    // --- Learn Noise button ---
    learnNoiseBtn.setButtonText ("Learn Noise");
    learnNoiseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a4e));
    learnNoiseBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    learnNoiseBtn.setTooltip ("Capture the current audio as the noise profile to subtract");
    learnNoiseBtn.onClick = [this]()
    {
        audioProcessor.noiseProfileRequest.store (true);
        statusLabel.setText ("Noise Profile: Captured",
                            juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00ff88));
    };
    addAndMakeVisible (learnNoiseBtn);

    // --- Clear Profile button ---
    clearProfileBtn.setButtonText ("Clear Profile");
    clearProfileBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a4e));
    clearProfileBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff6666));
    clearProfileBtn.setTooltip ("Clear the learned noise profile (disables noise reduction)");
    clearProfileBtn.onClick = [this]()
    {
        audioProcessor.clearNoiseProfile();
        statusLabel.setText ("Noise Profile: Cleared",
                            juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff6666));
    };
    addAndMakeVisible (clearProfileBtn);

    // --- Difference Mode toggle (persisted via APVTS AudioParameterBool) ---
    differenceModeBtn.setButtonText ("Difference Mode");
    differenceModeBtn.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    differenceModeBtn.setTooltip ("When enabled, hear only the removed noise instead of the cleaned signal");
    diffModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "diffMode", differenceModeBtn);
    addAndMakeVisible (differenceModeBtn);

    // --- Status label ---
    statusLabel.setText ("Noise Profile: Not Captured", juce::dontSendNotification);
    statusLabel.setFont (juce::Font (11.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff666688));
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);
}

NoiseReductionProcessorEditor::~NoiseReductionProcessorEditor()
{
    setLookAndFeel (nullptr);
}

// ============================================================================
// paint
// ============================================================================
void NoiseReductionProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colour (0xff1a1a2e));

    const int w = getWidth();
    const int h = getHeight();
    const int controlsWidth = 220;

    // Controls panel background (right side)
    g.setColour (juce::Colour (0xff151528));
    g.fillRect (w - controlsWidth, 0, controlsWidth, h);

    // Vertical divider between spectrum and controls
    g.setColour (juce::Colour (0xff2a2a4e));
    g.drawLine (static_cast<float> (w - controlsWidth), 0.0f,
                static_cast<float> (w - controlsWidth), static_cast<float> (h), 1.0f);

    // Subtle border around spectrum area
    auto spectrumArea = juce::Rectangle<int> (0, 60, w - controlsWidth, h - 60).reduced (20, 10);
    g.setColour (juce::Colour (0xff2a2a4e));
    g.drawRect (spectrumArea, 1);

    // Decorative line under section labels in controls panel
    auto controlsX = w - controlsWidth;
    g.setColour (juce::Colour (0xff00d4ff).withAlpha (0.3f));
    g.drawLine (static_cast<float> (controlsX + 30), 93.0f,
                static_cast<float> (w - 30), 93.0f, 1.0f);
}

// ============================================================================
// resized
// ============================================================================
void NoiseReductionProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Controls panel on the right
    auto controlsPanel = bounds.removeFromRight (220);

    // Top bar for title
    auto topBar = bounds.removeFromTop (60);
    titleLabel.setBounds (topBar.removeFromTop (30));
    subtitleLabel.setBounds (topBar);

    // Spectrum analyzer fills the remaining left area
    spectrumAnalyzer.setBounds (bounds.reduced (20, 10));

    // --- Controls layout (top to bottom) ---
    auto panel = controlsPanel.reduced (15, 15);

    controlsSectionLabel.setBounds (panel.removeFromTop (25));
    panel.removeFromTop (10);

    // Reduction knob + label
    reductionLabel.setBounds (panel.removeFromTop (18));
    panel.removeFromTop (2);
    reductionKnob.setBounds (panel.removeFromTop (85));
    panel.removeFromTop (8);

    // Input Gain knob + label
    inputGainLabel.setBounds (panel.removeFromTop (18));
    panel.removeFromTop (2);
    inputGainKnob.setBounds (panel.removeFromTop (85));
    panel.removeFromTop (8);

    // Output Gain knob + label
    outputGainLabel.setBounds (panel.removeFromTop (18));
    panel.removeFromTop (2);
    outputGainKnob.setBounds (panel.removeFromTop (85));
    panel.removeFromTop (8);

    // Learn Noise button
    learnNoiseBtn.setBounds (panel.removeFromTop (35));
    panel.removeFromTop (5);

    // Clear Profile button
    clearProfileBtn.setBounds (panel.removeFromTop (35));
    panel.removeFromTop (5);

    // Difference Mode toggle
    differenceModeBtn.setBounds (panel.removeFromTop (25));
    panel.removeFromTop (10);

    // Status label
    statusLabel.setBounds (panel.removeFromTop (25));
}