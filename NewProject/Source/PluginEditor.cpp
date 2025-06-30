/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// CircularButton Implementation
CircularButton::CircularButton()
{
    setSize(70, 70);
}

void CircularButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto center = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

    // Draw outer ring
    g.setColour(juce::Colours::darkgrey);
    g.drawEllipse(bounds.reduced(2), 3.0f);

    // Draw button background
    if (isMouseOver)
        g.setColour(juce::Colours::lightgrey.withAlpha(0.8f));
    else
        g.setColour(juce::Colours::grey.withAlpha(0.6f));

    g.fillEllipse(bounds.reduced(5));

    // Draw LED light in center
    if (isToggled)
    {
        g.setColour(juce::Colours::blue.brighter());
        g.fillEllipse(center.x - radius * 0.5f, center.y - radius * 0.5f, radius, radius);
        g.setColour(juce::Colours::blue.withAlpha(0.3f));
        g.fillEllipse(center.x - radius * 0.8f, center.y - radius * 0.8f, radius * 1.6f, radius * 1.6f);
    }
    else
    {
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(center.x - radius * 0.5f, center.y - radius * 0.5f, radius, radius);
    }

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(center.x - radius * 0.5f, center.y - radius * 0.5f, radius, radius, 1.0f);
}

void CircularButton::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        setToggleState(!isToggled);
        if (onClick)
            onClick();
    }
}

void CircularButton::mouseEnter(const juce::MouseEvent& e)
{
    isMouseOver = true;
    repaint();
}

void CircularButton::mouseExit(const juce::MouseEvent& e)
{
    isMouseOver = false;
    repaint();
}

void CircularButton::setToggleState(bool shouldBeToggled)
{
    if (isToggled != shouldBeToggled)
    {
        isToggled = shouldBeToggled;
        repaint();
    }
}

//==============================================================================
// VerticalKnob Implementation
VerticalKnob::VerticalKnob()
{
    setSize(50, 180); // Wider and taller
}

void VerticalKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto center = bounds.getCentre();

    // Calculate track area
    auto trackArea = juce::Rectangle<float>(
        center.x - trackWidth * 0.5f,
        25.0f,
        trackWidth,
        trackHeight
    );

    // Draw track background
    g.setColour(juce::Colours::darkgrey.darker());
    g.fillRoundedRectangle(trackArea, trackWidth * 0.5f);

    // Draw track border
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(trackArea, trackWidth * 0.5f, 1.0f);

    // Draw step markers and fills
    for (int i = 0; i < numSteps; ++i)
    {
        auto stepY = trackArea.getY() + (trackHeight / (numSteps - 1)) * i;
        auto stepHeight = (i == 0 || i == numSteps - 1) ? trackHeight / (numSteps - 1) : trackHeight / (numSteps - 1);

        // Draw step fill if active
        if (i >= (numSteps - 1 - currentStep) && currentStep > 0 && i < numSteps - 1)
        {
            juce::Colour stepColour;

            // 부모 에디터에서 버튼 상태 확인
            bool isCompressorActive = true;
            if (auto* editor = dynamic_cast<NewProjectAudioProcessorEditor*>(getParentComponent()))
            {
                isCompressorActive = editor->isCompressorActive();
            }

            // 활성화 상태에서만 색칠! (else 부분 완전히 제거)
            if (isCompressorActive)
            {
                switch (i)
                {
                case 0: stepColour = juce::Colours::darkblue; break;
                case 1: stepColour = juce::Colours::deepskyblue; break;
                case 2: stepColour = juce::Colours::cyan; break;
                case 3: stepColour = juce::Colours::lightblue; break;
                }

                auto fillArea = juce::Rectangle<float>(
                    trackArea.getX() + 1,
                    stepY,
                    trackArea.getWidth() - 2,
                    stepHeight
                );

                g.setColour(stepColour.withAlpha(0.8f));
                g.fillRoundedRectangle(fillArea, (trackWidth - 2) * 0.5f);
            }
        }
    }

    // Draw step numbers on the right
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    for (int i = 0; i < numSteps; ++i)
    {
        auto stepY = trackArea.getY() + (trackHeight / (numSteps - 1)) * i;
        auto stepNumber = numSteps - i; // 5 at top, 1 at bottom

        g.drawText(juce::String(stepNumber),
            static_cast<int>(trackArea.getRight() + 5),
            static_cast<int>(stepY - 6),
            15, 12,
            juce::Justification::centredLeft);
    }

    // Draw knob handle (heart shape)
    auto knobY = trackArea.getY() + (trackHeight / (numSteps - 1)) * (numSteps - 1 - currentStep);
    auto knobCenter = juce::Point<float>(center.x, knobY);

    // Heart shadow
    drawHeart(g, knobCenter.translated(1, 1), knobRadius, juce::Colours::black.withAlpha(0.3f));

    // Heart main color
    juce::Colour heartColour;
    if (isMouseOver || isDragging)
        heartColour = juce::Colours::hotpink;
    else
        heartColour = juce::Colours::pink;

    drawHeart(g, knobCenter, knobRadius, heartColour);

    // Heart outline
    drawHeart(g, knobCenter, knobRadius, juce::Colours::darkred);

    // Draw label
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(label,
        0, static_cast<int>(trackArea.getBottom() + 10),
        getWidth(), 15,
        juce::Justification::centred);
}

void VerticalKnob::drawHeart(juce::Graphics& g, juce::Point<float> center, float size, juce::Colour colour)
{
    juce::Path heartPath;

    // Heart shape parameters
    float heartSize = size * 0.8f;
    float cx = center.x;
    float cy = center.y;

    // Create heart shape using bezier curves
    heartPath.startNewSubPath(cx, cy + heartSize * 0.3f);

    // Left side of heart
    heartPath.cubicTo(cx - heartSize * 0.5f, cy - heartSize * 0.2f,
        cx - heartSize * 0.8f, cy - heartSize * 0.6f,
        cx - heartSize * 0.3f, cy - heartSize * 0.8f);

    // Left curve to top
    heartPath.cubicTo(cx - heartSize * 0.1f, cy - heartSize * 0.9f,
        cx + heartSize * 0.1f, cy - heartSize * 0.9f,
        cx + heartSize * 0.3f, cy - heartSize * 0.8f);

    // Right side of heart
    heartPath.cubicTo(cx + heartSize * 0.8f, cy - heartSize * 0.6f,
        cx + heartSize * 0.5f, cy - heartSize * 0.2f,
        cx, cy + heartSize * 0.3f);

    heartPath.closeSubPath();

    if (colour == juce::Colours::darkred) // Outline
    {
        g.setColour(colour);
        g.strokePath(heartPath, juce::PathStrokeType(1.5f));
    }
    else // Fill
    {
        g.setColour(colour);
        g.fillPath(heartPath);
    }
}

void VerticalKnob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        isDragging = true;
        lastMousePos = e.getPosition();
    }
}

void VerticalKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        auto deltaY = lastMousePos.y - e.getPosition().y; // Inverted: up = increase
        auto sensitivity = 28.0f; // Pixels per step

        if (std::abs(deltaY) >= sensitivity)
        {
            auto stepChange = static_cast<int>(deltaY / sensitivity);
            auto newStep = juce::jlimit(0, numSteps - 1, currentStep + stepChange);

            if (newStep != currentStep)
            {
                setValue(newStep);
                if (onValueChange)
                    onValueChange(currentStep);
            }

            lastMousePos = e.getPosition();
        }
    }
}

void VerticalKnob::mouseEnter(const juce::MouseEvent& e)
{
    isMouseOver = true;
    repaint();
}

void VerticalKnob::mouseExit(const juce::MouseEvent& e)
{
    isMouseOver = false;
    isDragging = false;
    repaint();
}

void VerticalKnob::setValue(int newStep)
{
    currentStep = juce::jlimit(0, numSteps - 1, newStep);
    repaint();
}

//==============================================================================
// NewProjectAudioProcessorEditor Implementation
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor(NewProjectAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Load background image
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::background_png,
        BinaryData::background_pngSize);

    // Setup bypass button at position (203, 450)
    bypassButton.setBounds(203 - 35, 450 - 35, 70, 70);
    bypassButton.setToggleState(false);
    bypassButton.onClick = [this]() { onBypassButtonClicked(); };
    addAndMakeVisible(bypassButton);

    // Setup status label
    statusLabel.setText("BYPASSED", juce::dontSendNotification);
    statusLabel.setBounds(10, 10, 200, 25);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.7f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(statusLabel);

    // Setup compression knob
    compressionKnob.setBounds(33, 72, 80, 220); // More to the right, bigger size
    compressionKnob.setLabel("COMP");
    compressionKnob.setValue(2); // Start at step 3 (middle)
    compressionKnob.onValueChange = [this](int step) { onCompressionValueChanged(step); };
    addAndMakeVisible(compressionKnob);

    // Set editor size
    setSize(400, 600);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
}

//==============================================================================
void NewProjectAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Draw background image
    if (backgroundImage.isValid())
    {
        g.drawImageWithin(backgroundImage,
            0, 0, getWidth(), getHeight(),
            juce::RectanglePlacement::fillDestination);
    }
    else
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(15.0f));
        g.drawFittedText("Image not found!", getLocalBounds(), juce::Justification::centred, 1);
    }

    // Draw position marker for reference (you can remove this later)
    g.setColour(juce::Colours::red.withAlpha(0.5f));
    g.drawLine(203 - 10, 450, 203 + 10, 450, 2.0f);
    g.drawLine(203, 450 - 10, 203, 450 + 10, 2.0f);
}

void NewProjectAudioProcessorEditor::resized()
{
    // Components have fixed positions for now
}

void NewProjectAudioProcessorEditor::onBypassButtonClicked()
{
    bool isActive = bypassButton.getToggleState();

    if (isActive)
    {
        statusLabel.setText("ACTIVE", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    }
    else
    {
        statusLabel.setText("BYPASSED", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    compressionKnob.repaint();
    audioProcessor.setAutoCompressionEnabled(isActive);
}

void NewProjectAudioProcessorEditor::onCompressionValueChanged(int step)
{
    // Update status to show compression level
    if (bypassButton.getToggleState()) // If active
    {
        statusLabel.setText("LEVEL " + juce::String(step + 1), juce::dontSendNotification);
    }

    // Here you would connect to the processor to set compression amount
    // audioProcessor.setCompressionLevel(step);
}