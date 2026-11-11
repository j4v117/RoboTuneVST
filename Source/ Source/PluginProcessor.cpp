#include "PluginProcessor.h"
#include "PluginEditor.h"

RoboTuneAudioProcessor::RoboTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

RoboTuneAudioProcessor::~RoboTuneAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout RoboTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray notes = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::StringArray scales = { "Cromatica (Todas)", "Mayor", "Menor" };

    params.push_back(std::make_unique<juce::AudioParameterChoice>("ROOT", "Nota Raiz", notes, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("SCALE", "Escala", scales, 2)); // Menor por defecto
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SPEED", "Retune Speed (ms)", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix (Dry/Wet)", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

void RoboTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    analysisBuffer.assign(analysisSize, 0.0f);
    delayLine.assign(delaySize, 0.0f);
    analysisWritePos = 0;
    delayWritePos = 0;
    phasor = 0.0f;
    currentRatio = 1.0f;
    targetRatio = 1.0f;
}

void RoboTuneAudioProcessor::releaseResources() {}

float RoboTuneAudioProcessor::detectPitch(const std::vector<float>& buffer, double sampleRate)
{
    // Rango vocal humano: 75 Hz a 900 Hz
    int minLag = static_cast<int>(sampleRate / 900.0);
    int maxLag = static_cast<int>(sampleRate / 75.0);
    int halfSize = static_cast<int>(buffer.size()) / 2;
    if (maxLag >= halfSize) maxLag = halfSize - 1;

    // Verificar si hay suficiente volumen (Noise Gate interno)
    float energy = 0.0f;
    for (int i = 0; i < halfSize; ++i)
        energy += buffer[i] * buffer[i];
    if (energy < 0.0005f) return 0.0f;

    // Autocorrelación normalizada
    float bestCorr = -1.0f;
    int bestLag = -1;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        float corr = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;

        for (int i = 0; i < halfSize; ++i)
        {
            float s1 = buffer[i];
            float s2 = buffer[i + lag];
            corr += s1 * s2;
            norm1 += s1 * s1;
            norm2 += s2 * s2;
        }

        float denom = std::sqrt(norm1 * norm2);
        if (denom > 0.00001f)
        {
            float normalizedCorr = corr / denom;
            if (normalizedCorr > bestCorr)
            {
                bestCorr = normalizedCorr;
                bestLag = lag;
            }
        }
    }

    if (bestCorr > 0.65f && bestLag > 0)
        return static_cast<float>(sampleRate) / static_cast<float>(bestLag);

    return 0.0f;
}

float RoboTuneAudioProcessor::quantizeToScale(float freq, int rootNote, int scaleType)
{
    if (freq <= 0.0f) return 0.0f;

    // Convertir frecuencia a nota MIDI continua
    float midiNote = 69.0f + 12.0f * std::log2(freq / 440.0f);
    int roundedMidi = static_cast<int>(std::round(midiNote));

    // Máscaras de intervalos
    const bool majorScale[12] = { true, false, true, false, true, true, false, true, false, true, false, true };
    const bool minorScale[12] = { true, false, true, true, false, true, false, true, true, false, true, false };

    int bestMidi = roundedMidi;
    float minDistance = 999.0f;

    // Buscar la nota permitida más cercana
    for (int candidate = roundedMidi - 6; candidate <= roundedMidi + 6; ++candidate)
    {
        int noteInOctave = ((candidate - rootNote) % 12 + 12) % 12;
        bool allowed = true;

        if (scaleType == 1) allowed = majorScale[noteInOctave];
        else if (scaleType == 2) allowed = minorScale[noteInOctave];

        if (allowed)
        {
            float dist = std::abs(midiNote - static_cast<float>(candidate));
            if (dist < minDistance)
            {
                minDistance = dist;
                bestMidi = candidate;
            }
        }
    }

    return 440.0f * std::pow(2.0f, (static_cast<float>(bestMidi) - 69.0f) / 12.0f);
}

void RoboTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    int rootNote = static_cast<int>(*apvts.getRawParameterValue("ROOT"));
    int scaleType = static_cast<int>(*apvts.getRawParameterValue("SCALE"));
    float speedMs = *apvts.getRawParameterValue("SPEED");
    float mix = *apvts.getRawParameterValue("MIX");

    // Coeficiente de suavizado (0 ms = salto instantáneo robótico)
    float smoothCoeff = (speedMs <= 0.5f) ? 1.0f : (1.0f - std::exp(-1.0f / (static_cast<float>(currentSampleRate) * (speedMs * 0.001f))));

    const float* inLeft = buffer.getReadPointer(0);
    const float* inRight = numChannels > 1 ? buffer.getReadPointer(1) : inLeft;
    float* outLeft = buffer.getWritePointer(0);
    float* outRight = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    const float windowSizeSamples = 1600.0f; // Tamaño de grano óptimo para timbre robótico

    for (int i = 0; i < numSamples; ++i)
    {
        float monoIn = 0.5f * (inLeft[i] + inRight[i]);

        // Llenar buffer de análisis
        analysisBuffer[analysisWritePos] = monoIn;
        analysisWritePos++;

        if (analysisWritePos >= analysisSize)
        {
            float detectedFreq = detectPitch(analysisBuffer, currentSampleRate);
            if (detectedFreq > 0.0f)
            {
                float targetFreq = quantizeToScale(detectedFreq, rootNote, scaleType);
                if (targetFreq > 0.0f)
                    targetRatio = juce::jlimit(0.5f, 2.0f, targetFreq / detectedFreq);
            }
            // Desplazar mitad del buffer para análisis continuo
            std::copy(analysisBuffer.begin() + analysisSize / 2, analysisBuffer.end(), analysisBuffer.begin());
            analysisWritePos = analysisSize / 2;
        }

        // Aplicar Retune Speed
        currentRatio += smoothCoeff * (targetRatio - currentRatio);

        // Guardar audio en la línea de retardo
        delayLine[delayWritePos] = monoIn;

        // Motor Granular Dual-Tap (Pitch Shifting)
        float phasorRate = (1.0f - currentRatio) / windowSizeSamples;
        phasor += phasorRate;
        if (phasor >= 1.0f) phasor -= 1.0f;
        if (phasor < 0.0f) phasor += 1.0f;

        float phasor2 = phasor + 0.5f;
        if (phasor2 >= 1.0f) phasor2 -= 1.0f;

        auto readTap = [&](float p) -> float
        {
            float delaySamples = p * windowSizeSamples + 2.0f;
            float readPos = static_cast<float>(delayWritePos) - delaySamples;
            while (readPos < 0.0f) readPos += static_cast<float>(delaySize);

            int idx1 = static_cast<int>(readPos) % delaySize;
            int idx2 = (idx1 + 1) % delaySize;
            float frac = readPos - std::floor(readPos);

            float sample = delayLine[idx1] * (1.0f - frac) + delayLine[idx2] * frac;
            // Ventana triangular de crossfade
            float gain = 1.0f - std::abs(2.0f * p - 1.0f);
            return sample * gain;
        };

        float wetSignal = readTap(phasor) + readTap(phasor2);

        delayWritePos = (delayWritePos + 1) % delaySize;

        // Mezcla Dry/Wet
        outLeft[i] = inLeft[i] * (1.0f - mix) + wetSignal * mix;
        if (outRight != nullptr)
            outRight[i] = inRight[i] * (1.0f - mix) + wetSignal * mix;
    }
}

juce::AudioProcessorEditor* RoboTuneAudioProcessor::createEditor()
{
    return new RoboTuneAudioProcessorEditor (*this);
}

void RoboTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RoboTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoboTuneAudioProcessor();
}
