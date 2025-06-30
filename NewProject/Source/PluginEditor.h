/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom circular button class
class CircularButton : public juce::Component
{
public:
    CircularButton();
    ~CircularButton() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    // Button state
    void setToggleState(bool shouldBeToggled);
    bool getToggleState() const { return isToggled; }

    // Callback for when button is clicked
    std::function<void()> onClick;

private:
    bool isToggled = false;
    bool isMouseOver = false;
};

//==============================================================================
// Custom vertical knob class
class VerticalKnob : public juce::Component
{
public:
    VerticalKnob();
    ~VerticalKnob() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    // Value control (0-4 for 5 steps)
    void setValue(int newStep); // 0 to 4
    int getValue() const { return currentStep; }

    // Label
    void setLabel(const juce::String& labelText) { label = labelText; }

    // Callback for value changes
    std::function<void(int)> onValueChange;

private:
    int currentStep = 2; // Default to middle position (step 3)
    bool isMouseOver = false;
    bool isDragging = false;
    juce::Point<int> lastMousePos;
    juce::String label = "KNOB";

    // Visual parameters
    static constexpr float knobRadius = 18.0f;
    static constexpr float trackWidth = 8.0f;
    static constexpr float trackHeight = 110.0f;
    static constexpr int numSteps = 5;

    // Helper functions
    void drawHeart(juce::Graphics& g, juce::Point<float> center, float size, juce::Colour colour);
};

//==============================================================================
/**
*/
class NewProjectAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    NewProjectAudioProcessorEditor(NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isCompressorActive() const { return bypassButton.getToggleState(); }

private:
    NewProjectAudioProcessor& audioProcessor;
    juce::Image backgroundImage;

    // UI Components
    CircularButton bypassButton;
    juce::Label statusLabel;
    VerticalKnob compressionKnob;

    // Callbacks
    void onBypassButtonClicked();
    void onCompressionValueChanged(int step);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessorEditor)
};