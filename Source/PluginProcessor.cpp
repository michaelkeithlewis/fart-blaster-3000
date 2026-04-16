#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

FartBlasterProcessor::FartBlasterProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    howMuchParam = apvts.getRawParameterValue("howMuch");
    howWetParam = apvts.getRawParameterValue("howWet");
    loadSamples();
}

FartBlasterProcessor::~FartBlasterProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout FartBlasterProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("howMuch", 1),
        "How Much?",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("howWet", 1),
        "How Wet?",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f));

    return { params.begin(), params.end() };
}

void FartBlasterProcessor::loadSamples()
{
    juce::WavAudioFormat wavFormat;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSize);
        if (data == nullptr || dataSize == 0)
            continue;

        auto* stream = new juce::MemoryInputStream(data, static_cast<size_t>(dataSize), false);
        std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(stream, true));
        if (reader == nullptr)
            continue;

        FartSample sample;
        sample.sampleRate = reader->sampleRate;
        sample.buffer.setSize(1, static_cast<int>(reader->lengthInSamples));
        reader->read(&sample.buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, false);
        samples.push_back(std::move(sample));
    }
}

void FartBlasterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    fartBuffer.setSize(2, samplesPerBlock);

    // Single reverb with moderate settings (~1.5s tail)
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.72f;
    rp.damping = 0.3f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;

    reverb.setParameters(rp);
    reverb.setSampleRate(sampleRate);

    // Stereo delay line (2 seconds max)
    delayBufSize = static_cast<int>(2.0 * sampleRate);
    delayLineL.assign(static_cast<size_t>(delayBufSize), 0.0f);
    delayLineR.assign(static_cast<size_t>(delayBufSize), 0.0f);
    delayWritePos = 0;
    delaySamples = static_cast<int>(delayTimeSec * sampleRate);

    for (auto& v : voices)
        v.active = false;

    samplesUntilNextFart = 1;
    lastHowMuch = -1.0f;
}

void FartBlasterProcessor::releaseResources()
{
    reverb.reset();
}

bool FartBlasterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}

float FartBlasterProcessor::getRandomInterval()
{
    float howMuch = howMuchParam->load();
    float inv = 1.0f - howMuch;
    float curved = inv * inv * inv * inv;

    constexpr float minSec = 0.05f;
    constexpr float maxSec = 60.0f;
    float base = minSec + (maxSec - minSec) * curved;

    float jitter = 0.75f + rng.nextFloat() * 0.5f;
    return base * jitter;
}

void FartBlasterProcessor::triggerFart()
{
    if (samples.empty())
        return;

    for (auto& v : voices)
    {
        if (!v.active)
        {
            v.sampleIndex = rng.nextInt(static_cast<int>(samples.size()));
            v.position = 0.0;
            v.active = true;
            return;
        }
    }

    voices[0].sampleIndex = rng.nextInt(static_cast<int>(samples.size()));
    voices[0].position = 0.0;
    voices[0].active = true;
}

void FartBlasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float howMuch = howMuchParam->load();
    float howWet = howWetParam->load();

    fartBuffer.setSize(2, numSamples, false, false, true);
    fartBuffer.clear();

    if (howMuch < 0.001f)
    {
        for (auto& v : voices)
            v.active = false;
        samplesUntilNextFart = 1;
        currentIntervalSec.store(0.0f);
    }
    else
    {
        // Knob moved significantly → kill voices and retrigger immediately
        if (std::abs(howMuch - lastHowMuch) > 0.015f)
        {
            for (auto& v : voices)
                v.active = false;
            samplesUntilNextFart = 1;
        }

        samplesUntilNextFart -= numSamples;
        while (samplesUntilNextFart <= 0)
        {
            triggerFart();
            float interval = getRandomInterval();
            currentIntervalSec.store(interval);
            samplesUntilNextFart += juce::jmax(1, static_cast<int>(interval * hostSampleRate));
        }
    }
    lastHowMuch = howMuch;

    // Render active voices into fartBuffer channel 0
    float* fartData = fartBuffer.getWritePointer(0);

    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;

        if (voice.sampleIndex < 0 || voice.sampleIndex >= static_cast<int>(samples.size()))
        {
            voice.active = false;
            continue;
        }

        auto& sample = samples[static_cast<size_t>(voice.sampleIndex)];
        double ratio = sample.sampleRate / hostSampleRate;
        const float* src = sample.buffer.getReadPointer(0);
        int srcLen = sample.buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            int pos0 = static_cast<int>(voice.position);
            if (pos0 >= srcLen - 1)
            {
                voice.active = false;
                break;
            }

            float frac = static_cast<float>(voice.position - pos0);
            float val = src[pos0] * (1.0f - frac) + src[pos0 + 1] * frac;
            fartData[i] += val;
            voice.position += ratio;
        }
    }

    // Copy mono fart signal to stereo
    fartBuffer.copyFrom(1, 0, fartBuffer, 0, 0, numSamples);

    // Add dry fart signal to output (always audible)
    int chans = juce::jmin(numChannels, 2);
    for (int ch = 0; ch < chans; ++ch)
        buffer.addFrom(ch, 0, fartBuffer, ch, 0, numSamples);

    // --- Delay processing ---
    // Write fart signal into delay line, add echo-only output to the main buffer
    if (delayBufSize > 0)
    {
        float* outL = (numChannels > 0) ? buffer.getWritePointer(0) : nullptr;
        float* outR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = fartBuffer.getSample(0, i);
            float inR = fartBuffer.getSample(1, i);

            int readPos = delayWritePos - delaySamples;
            if (readPos < 0) readPos += delayBufSize;

            float echoL = delayLineL[static_cast<size_t>(readPos)];
            float echoR = delayLineR[static_cast<size_t>(readPos)];

            delayLineL[static_cast<size_t>(delayWritePos)] = inL + echoL * delayFeedback;
            delayLineR[static_cast<size_t>(delayWritePos)] = inR + echoR * delayFeedback;

            if (outL != nullptr) outL[i] += echoL * howWet;
            if (outR != nullptr) outR[i] += echoR * howWet;

            delayWritePos = (delayWritePos + 1) % delayBufSize;
        }
    }

    // --- Reverb processing (wet-only output of dry fart signal) ---
    reverb.processStereo(fartBuffer.getWritePointer(0),
                         fartBuffer.getWritePointer(1), numSamples);

    for (int ch = 0; ch < chans; ++ch)
        buffer.addFrom(ch, 0, fartBuffer, ch, 0, numSamples, howWet);
}

void FartBlasterProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FartBlasterProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* FartBlasterProcessor::createEditor()
{
    return new FartBlasterEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FartBlasterProcessor();
}
