#include "PluginProcessor.h"
#include "PluginEditor.h"  // for createEditor()
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <algorithm>

PhaseVocoderAudioProcessor::PhaseVocoderAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

PhaseVocoderAudioProcessor::~PhaseVocoderAudioProcessor() = default;

const juce::String PhaseVocoderAudioProcessor::getName() const           { return JucePlugin_Name; }
bool PhaseVocoderAudioProcessor::acceptsMidi() const                     { return true; }
bool PhaseVocoderAudioProcessor::producesMidi() const                    { return false; }
bool PhaseVocoderAudioProcessor::isMidiEffect() const                    { return false; }
double PhaseVocoderAudioProcessor::getTailLengthSeconds() const          { return 0.0; }

int PhaseVocoderAudioProcessor::getNumPrograms()                         { return 1; }
int PhaseVocoderAudioProcessor::getCurrentProgram()                      { return 0; }
void PhaseVocoderAudioProcessor::setCurrentProgram (int)                 {}
const juce::String PhaseVocoderAudioProcessor::getProgramName (int)      { return {}; }
void PhaseVocoderAudioProcessor::changeProgramName (int,const juce::String&) {}

void PhaseVocoderAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void PhaseVocoderAudioProcessor::setStateInformation (const void*,int)   {}

void PhaseVocoderAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    analysisBuffer.setSize  (1, fftSize);  analysisBuffer.clear();
    synthesisBuffer.setSize (1, fftSize);  synthesisBuffer.clear();

    window.resize (fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1)));

    fftBuffer  .assign (fftSize,       0.0f);
    magnitudes .assign (fftSize/2 + 1, 0.0f);
    phases     .assign (fftSize/2 + 1, 0.0f);
    logMag     .assign (fftSize/2 + 1, 0.0f);
    cepstrum   .assign (fftSize,       0.0f);
    envelope   .assign (fftSize/2 + 1, 0.0f);

    analysisWritePos  = 0;
    synthesisReadPos  = 0;
    samplesSinceHop   = 0;
}

void PhaseVocoderAudioProcessor::releaseResources() {}

bool PhaseVocoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto c = l.getMainInputChannelSet();
    return c == l.getMainOutputChannelSet()
        && (c == juce::AudioChannelSet::mono() || c == juce::AudioChannelSet::stereo());
}

void PhaseVocoderAudioProcessor::handleMidi (juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            currentMidiNote = msg.getNoteNumber();
        else if (msg.isNoteOff())
            currentMidiNote = -1;
    }
}

void PhaseVocoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer& midiMessages)
{
    keyboardState.processNextMidiBuffer (midiMessages,
                                         0, buffer.getNumSamples(),
                                         true);
    handleMidi (midiMessages);

    // simple debug print:
    if (currentMidiNote >= 0)
        DBG("MIDI note on:  " << currentMidiNote);
    else
        DBG("MIDI note off");
    const int numSamples = buffer.getNumSamples();
    const float dryWetMix = 0.3f;

    auto* inL  = buffer.getReadPointer  (0);
    auto* inR  = buffer.getNumChannels() > 1 ? buffer.getReadPointer  (1) : nullptr;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = inL[i] + (inR ? inR[i] : inL[i]);
        analysisBuffer.setSample (0, analysisWritePos, 0.5f * x);

        if (++samplesSinceHop >= hopSize)
        {
            analyseFrame();
            synthesisFrame();
            samplesSinceHop = 0;
        }

        float wet = synthesisBuffer.getSample (0, synthesisReadPos);
        synthesisBuffer.setSample (0, synthesisReadPos, 0.0f);

        float dry = inL[i];
        float y   = dry * (1.0f - dryWetMix) + wet * dryWetMix;

        outL[i] = y;
        if (outR) outR[i] = y;

        if (++analysisWritePos >= fftSize)  analysisWritePos = 0;
        if (++synthesisReadPos >= fftSize) synthesisReadPos  = 0;
    }
}
void PhaseVocoderAudioProcessor::analyseFrame()
{  
     
   // 0) Copy & window
   for (int i = 0; i < fftSize; ++i)
       fftBuffer[i] = analysisBuffer.getSample (0, (analysisWritePos + i) % fftSize) * window[i];


   // 1) Forward FFT
   fft.performRealOnlyForwardTransform (fftBuffer.data());
   const float* d = fftBuffer.data();


   // 2) Extract magnitudes & phases
   magnitudes[0]         = std::abs (d[0]);   phases[0]         = 0.0f;
   magnitudes[fftSize/2] = std::abs (d[1]);   phases[fftSize/2] = 0.0f;
   for (int k = 1; k < fftSize/2; ++k)
   {
       float re = d[2*k], im = d[2*k + 1];
       magnitudes[k] = std::hypot (re, im);
       phases[k]     = std::atan2 (im, re);
   }

   // ——— NEW: very basic pitch detection ———
    // skip bin 0 (DC), look for the max magnitude bin:
    auto maxIt = std::max_element (magnitudes.begin() + 1, magnitudes.end());
    int  maxBin = int (std::distance (magnitudes.begin(), maxIt));
    double freqHz = maxBin * currentSampleRate / double (fftSize);
    DBG ("Detected pitch: " << freqHz << " Hz");


   // 4.1) Peak detection
   std::vector<int> peakBins;
   for (int k = 1; k < fftSize/2 - 1; ++k)
       if (magnitudes[k] >= magnitudes[k-1] && magnitudes[k] > magnitudes[k+1])
           peakBins.push_back(k);
   DBG("Peaks detected: " << peakBins.size());


   // 4.2) Nearest‑peak mapping
   std::vector<int> nearestPeak (fftSize/2 + 1, 0);
   if (! peakBins.empty())
       for (int k = 0; k <= fftSize/2; ++k)
       {
           int best = peakBins[0], bestDist = std::abs(k - best);
           for (int p : peakBins)
           {
               int d = std::abs (k - p);
               if (d < bestDist) { bestDist = d; best = p; }
           }
           nearestPeak[k] = best;
       }
   DBG("nearestPeak[10] = " << nearestPeak[10]
      << "   nearestPeak[" << fftSize/4 << "] = " << nearestPeak[fftSize/4]);


   // 3.1) Cepstral envelope estimation
   for (int k = 0; k <= fftSize/2; ++k)
       logMag[k] = std::log (magnitudes[k] + 1e-9f);


   fftBuffer[0] = logMag[0];
   fftBuffer[1] = logMag[fftSize/2];
   for (int k = 1; k < fftSize/2; ++k)
   {
       fftBuffer[2*k]     = logMag[k];
       fftBuffer[2*k + 1] = 0.0f;
   }
   fft.performRealOnlyInverseTransform (fftBuffer.data());
   for (int n = 0; n < fftSize; ++n)
       cepstrum[n] = fftBuffer[n];


   // 3.2) Lifter & rebuild envelope
   const int lifterOrder = 15;
   for (int n = lifterOrder; n < fftSize - lifterOrder; ++n)
       cepstrum[n] = 0.0f;
   for (int n = 0; n < fftSize; ++n)
       fftBuffer[n] = cepstrum[n];
   fft.performRealOnlyForwardTransform (fftBuffer.data());
   envelope[0]         = std::exp (fftBuffer[0]);
   envelope[fftSize/2] = std::exp (fftBuffer[1]);
   for (int k = 1; k < fftSize/2; ++k)
       envelope[k] = std::exp (fftBuffer[2*k]);


   // 3.3) Formant‑preserving pitch‑shift (two voices)
   std::vector<float> magOut1 (magnitudes.size(), 0.0f), phOut1 (phases.size(), 0.0f);
   std::vector<float> magOut2 (magnitudes.size(), 0.0f), phOut2 (phases.size(), 0.0f);
   const double ratio1 = kPitchRatio5th;
   const double ratio2 = std::pow(2.0, 4.0/12.0);


   for (int k = 0; k <= fftSize/2; ++k)
   {
       // shift #1
       double src1 = k / ratio1;
       int    k1   = (int)std::floor(src1);
       float  f1   = (float)(src1 - k1);
       if (k1 >= 0 && k1+1 <= fftSize/2)
       {
           float m1 = juce::jmap(f1, magnitudes[k1], magnitudes[k1+1]);
           float p1 = juce::jmap(f1, phases[k1],     phases[k1+1]);
           float e1 = juce::jmap(f1, envelope[k1],   envelope[k1+1]);
           magOut1[k] = m1 * e1;
           phOut1 [k] = p1;
       }
       // shift #2
       double src2 = k / ratio2;
       int    k2   = (int)std::floor(src2);
       float  f2   = (float)(src2 - k2);
       if (k2 >= 0 && k2+1 <= fftSize/2)
       {
           float m2 = juce::jmap(f2, magnitudes[k2], magnitudes[k2+1]);
           float p2 = juce::jmap(f2, phases[k2],     phases[k2+1]);
           float e2 = juce::jmap(f2, envelope[k2],   envelope[k2+1]);
           magOut2[k] = m2 * e2;
           phOut2 [k] = p2;
       }
   }


   // 4.3) Phase‑locking
   for (int k = 0; k <= fftSize/2; ++k)
   {
       int pk = nearestPeak[k];
       phOut1[k] = phOut1[pk];
       phOut2[k] = phOut2[pk];
   }


   // 5) Pack & IFFT
   fftBuffer[0] = magOut1[0] + magOut2[0];
   fftBuffer[1] = magOut1[fftSize/2] + magOut2[fftSize/2];
   for (int k = 1; k < fftSize/2; ++k)
   {
       float re = magOut1[k] * std::cos(phOut1[k])
                + magOut2[k] * std::cos(phOut2[k]);
       float im = magOut1[k] * std::sin(phOut1[k])
                + magOut2[k] * std::sin(phOut2[k]);
       fftBuffer[2*k    ] = re;
       fftBuffer[2*k + 1] = im;
   }


   fft.performRealOnlyInverseTransform (fftBuffer.data());
}


void PhaseVocoderAudioProcessor::synthesisFrame()
{
    for (int i = 0; i < fftSize; ++i)
    {
        int idx = (analysisWritePos + i) % fftSize;
        synthesisBuffer.addSample (0, idx, fftBuffer[i] * window[i]);
    }
}

juce::AudioProcessorEditor* PhaseVocoderAudioProcessor::createEditor()
{
    return new PhaseVocoderAudioProcessorEditor (*this);
}
bool PhaseVocoderAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PhaseVocoderAudioProcessor(); }
