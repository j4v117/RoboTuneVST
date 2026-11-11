#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class RoboTuneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    RoboTuneAudioProcessorEditor (RoboTuneAudioProcessor&);
    ~RoboTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RoboTuneAudioProcessor& audioProcessor;

    juce::ComboBox rootBox, scaleBox;
    juce::Slider speedSlider, mixSlider;
    juce::Label rootLabel, scaleLabel, speedLabel, mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment, mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoboTuneAudioProcessorEditor)
};
