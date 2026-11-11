#include "PluginProcessor.h"
#include "PluginEditor.h"

RoboTuneAudioProcessorEditor::RoboTuneAudioProcessorEditor (RoboTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (420, 260);

    // Configurar desplegables
    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible(rootBox);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "ROOT", rootBox);

    scaleBox.addItemList({ "Cromatica (Todas)", "Mayor", "Menor" }, 1);
    addAndMakeVisible(scaleBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "SCALE", scaleBox);

    // Configurar perillas
    auto setupKnob = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(label);
    };

    setupKnob(speedSlider, speedLabel, "RETUNE SPEED (0 = ROBO)");
    setupKnob(mixSlider, mixLabel, "MIX (DRY / WET)");

    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "SPEED", speedSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "MIX", mixSlider);

    rootLabel.setText("NOTA RAIZ", juce::dontSendNotification);
    addAndMakeVisible(rootLabel);
    scaleLabel.setText("ESCALA", juce::dontSendNotification);
    addAndMakeVisible(scaleLabel);
}

RoboTuneAudioProcessorEditor::~RoboTuneAudioProcessorEditor() {}

void RoboTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12141a));

    g.setColour (juce::Colours::cyan);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawFittedText ("ROBOTUNE VST3", 0, 12, getWidth(), 30, juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff232733));
    g.drawRoundedRectangle (15.0f, 50.0f, getWidth() - 30.0f, getHeight() - 65.0f, 10.0f, 1.5f);
}

void RoboTuneAudioProcessorEditor::resized()
{
    rootLabel.setBounds(30, 62, 160, 20);
    rootBox.setBounds(30, 82, 160, 28);

    scaleLabel.setBounds(230, 62, 160, 20);
    scaleBox.setBounds(230, 82, 160, 28);

    speedLabel.setBounds(30, 122, 170, 20);
    speedSlider.setBounds(55, 142, 120, 95);

    mixLabel.setBounds(220, 122, 170, 20);
    mixSlider.setBounds(245, 142, 120, 95);
}
