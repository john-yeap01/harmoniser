#pragma once


#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>


class PhaseVocoderAudioProcessor  : public juce::AudioProcessor
{
public:
   PhaseVocoderAudioProcessor();
   ~PhaseVocoderAudioProcessor() override;


   //==============================================================================
   void prepareToPlay (double sampleRate, int samplesPerBlock) override;
   void releaseResources() override;
   bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   void processBlock (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) override;


   //==============================================================================
   juce::AudioProcessorEditor* createEditor() override;
   bool hasEditor() const override;


   //==============================================================================
   const juce::String getName() const override;
   bool acceptsMidi() const override;
   bool producesMidi() const override;
   bool isMidiEffect() const override;
   double getTailLengthSeconds() const override;


   //==============================================================================
   int getNumPrograms() override;
   int getCurrentProgram() override;
   void setCurrentProgram (int index) override;
   const juce::String getProgramName (int index) override;
   void changeProgramName (int index, const juce::String& newName) override;

    
   //==============================================================================
   void getStateInformation (juce::MemoryBlock& destData) override;
   void setStateInformation (const void* data, int sizeInBytes) override;

   juce::MidiKeyboardState keyboardState;


private:
   // FFT parameters
   static constexpr int fftOrder = 11;              // 2^11 = 2048
   static constexpr int fftSize  = 1 << fftOrder;
   static constexpr int hopSize  = fftSize / 4;     // 75% overlap


   static constexpr double kPitchRatio5th = 1.4983070768766815;


   juce::dsp::FFT            fft { fftOrder };
   juce::AudioBuffer<float>  analysisBuffer;
   juce::AudioBuffer<float>  synthesisBuffer;
   std::vector<float>        window, fftBuffer;
   std::vector<float>        magnitudes, phases;
   std::vector<float>        logMag, cepstrum, envelope;


   int analysisWritePos    = 0;
   int synthesisReadPos    = 0;
   int samplesSinceHop     = 0;


   // **New members for startup gate**
   bool havePrimed         = false;
   int  samplesSinceStart  = 0;


   // helpers
   void analyseFrame();
   void synthesisFrame();
   void handleMidi(juce::MidiBuffer& midiMessages);

   int currentMidiNote = -1;
   int   referenceMidiNote = 60;      // C4
    double pitchRatio       = 1.0;     // no shift by default

    double currentSampleRate = 44100.0;

    std::vector<int> activeNotes;
    std::vector<float> analysisFrameBuffer { fftSize };


   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseVocoderAudioProcessor)
};




