#include "PluginEditor.h"
#include "PluginProcessor.h"

PhaseVocoderAudioProcessorEditor::PhaseVocoderAudioProcessorEditor (PhaseVocoderAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processorRef (p),
      // 3) Now we can safely initialize keyboardComponent
      keyboardComponent (processorRef.keyboardState,
                         juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (keyboardComponent);
    setSize (600, 150);
}

PhaseVocoderAudioProcessorEditor::~PhaseVocoderAudioProcessorEditor() = default;

void PhaseVocoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PhaseVocoderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    keyboardComponent.setBounds (area.removeFromBottom (80));
}
