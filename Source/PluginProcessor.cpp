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
    stereoParam = apvts.getRawParameterValue("stereo");
    moodParam = apvts.getRawParameterValue("mood");
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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("stereo", 1),
        "Stereo",
        true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("mood", 1),
        "Mood",
        false));

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

    moodEnv = 0.0f;
    moodGateSamples = 0;
    moodArmed = true;
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

    const float pan = rng.nextFloat() * 2.0f - 1.0f;
    const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float gL = std::cos(panAngle);
    const float gR = std::sin(panAngle);

    for (auto& v : voices)
    {
        if (!v.active)
        {
            v.sampleIndex = rng.nextInt(static_cast<int>(samples.size()));
            v.position = 0.0;
            v.gainL = gL;
            v.gainR = gR;
            v.active = true;
            return;
        }
    }

    voices[0].sampleIndex = rng.nextInt(static_cast<int>(samples.size()));
    voices[0].position = 0.0;
    voices[0].gainL = gL;
    voices[0].gainR = gR;
    voices[0].active = true;
}

void FartBlasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float howMuch = howMuchParam->load();
    float howWet = howWetParam->load();
    const bool stereo = stereoParam->load() > 0.5f;
    const bool mood = moodParam->load() > 0.5f;
    constexpr float monoGain = 0.7071f;

    fartBuffer.setSize(2, numSamples, false, false, true);
    fartBuffer.clear();

    if (mood)
    {
        // --- MOOD mode: fire farts off the INPUT signal's energy ---
        // Measure the incoming audio (before we layer any farts on), follow its
        // envelope, and blast a fart on an onset. HOW MUCH becomes sensitivity;
        // a refractory gate + hysteresis stop it from machine-gunning on a
        // sustained loud passage. Reacts to the music's mood, not the clock.
        float lvl = 0.0f;
        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
            lvl = juce::jmax(lvl, buffer.getMagnitude(ch, 0, numSamples));

        // one-pole follower: fast attack so transients register, slower release
        const float coeff = (lvl > moodEnv) ? 0.6f : 0.05f;
        moodEnv += coeff * (lvl - moodEnv);

        // HOW MUCH -> sensitivity: right = hair trigger (low threshold, short gap)
        const float thresh = juce::jmap(howMuch, 0.0f, 1.0f, 0.35f, 0.015f);
        const int minGap = juce::jmax(1,
            static_cast<int>(juce::jmap(howMuch, 0.0f, 1.0f, 0.45f, 0.09f) * hostSampleRate));

        moodGateSamples -= numSamples;
        if (moodArmed && moodEnv > thresh && moodGateSamples <= 0)
        {
            triggerFart();
            moodArmed = false;
            moodGateSamples = minGap;
        }
        if (moodEnv < thresh * 0.55f)   // must fall back down before re-arming
            moodArmed = true;

        currentIntervalSec.store(-1.0f);   // sentinel: editor shows MOOD, not an interval
    }
    else if (howMuch < 0.001f)
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

    // Render active voices into fartBuffer with per-voice equal-power pan
    float* fartL = fartBuffer.getWritePointer(0);
    float* fartR = fartBuffer.getWritePointer(1);

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
            const float gL = stereo ? voice.gainL : monoGain;
            const float gR = stereo ? voice.gainR : monoGain;
            fartL[i] += val * gL;
            fartR[i] += val * gR;
            voice.position += ratio;
        }
    }

    // Add dry fart signal to output (always audible)
    int chans = juce::jmin(numChannels, 2);
    for (int ch = 0; ch < chans; ++ch)
        buffer.addFrom(ch, 0, fartBuffer, ch, 0, numSamples);

    // --- Ping-pong delay ---
    // L input + R echo feedback writes into L line; R input + L echo feedback writes into R line.
    // Result: echoes bounce L→R→L→R at delayTimeSec intervals.
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

            const float fbL = stereo ? echoR : echoL;
            const float fbR = stereo ? echoL : echoR;
            delayLineL[static_cast<size_t>(delayWritePos)] = inL + fbL * delayFeedback;
            delayLineR[static_cast<size_t>(delayWritePos)] = inR + fbR * delayFeedback;

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
