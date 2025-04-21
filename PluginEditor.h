#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class PhaseVocoderAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PhaseVocoderAudioProcessorEditor (PhaseVocoderAudioProcessor&);
    ~PhaseVocoderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // 1) Make sure processorRef comes first
    PhaseVocoderAudioProcessor& processorRef;

    // 2) Declare keyboardComponent without initializing it here
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseVocoderAudioProcessorEditor)
};
