#pragma once

#include <JuceHeader.h>

class FartBlasterProcessor : public juce::AudioProcessor
{
public:
    FartBlasterProcessor();
    ~FartBlasterProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct FartSample
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 22050.0;
    };

    struct Voice
    {
        int sampleIndex = -1;
        double position = 0.0;
        bool active = false;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void loadSamples();
    void triggerFart();
    float getRandomInterval();

    std::vector<FartSample> samples;
    static constexpr int maxVoices = 16;
    std::array<Voice, maxVoices> voices;

    juce::Reverb reverb;
    juce::AudioBuffer<float> fartBuffer;

    // Stereo delay line
    int delayBufSize = 0;
    std::vector<float> delayLineL, delayLineR;
    int delayWritePos = 0;
    int delaySamples = 0;
    static constexpr float delayTimeSec = 0.34f;
    static constexpr float delayFeedback = 0.35f;

    double hostSampleRate = 44100.0;
    int samplesUntilNextFart = 1;
    float lastHowMuch = -1.0f;
    juce::Random rng;

    std::atomic<float>* howMuchParam = nullptr;
    std::atomic<float>* howWetParam = nullptr;

    // Readable by editor for interval display
    std::atomic<float> currentIntervalSec{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FartBlasterProcessor)
};
