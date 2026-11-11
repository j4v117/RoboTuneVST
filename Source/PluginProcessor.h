#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

class RoboTuneAudioProcessor : public juce::AudioProcessor
{
public:
    RoboTuneAudioProcessor();
    ~RoboTuneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Detector de tono (Autocorrelación)
    float detectPitch(const std::vector<float>& buffer, double sampleRate);
    float quantizeToScale(float freq, int rootNote, int scaleType);

    // Buffers circulares para Pitch Shifting robótico
    std::vector<float> analysisBuffer;
    int analysisWritePos = 0;
    const int analysisSize = 2048;

    std::vector<float> delayLine;
    int delayWritePos = 0;
    const int delaySize = 8192;
    float phasor = 0.0f;

    float currentRatio = 1.0f;
    float targetRatio = 1.0f;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoboTuneAudioProcessor)
};
