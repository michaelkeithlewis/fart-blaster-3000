#pragma once

#include "PluginProcessor.h"

class FartLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
};

class BrandButton : public juce::Component
{
public:
    explicit BrandButton(const juce::Image& logoImg);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    juce::Image logo;
    bool hovering = false;
};

class FartBlasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FartBlasterEditor(FartBlasterProcessor&);
    ~FartBlasterEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawStinkLines(juce::Graphics& g, float x, float y, float scale);

    FartBlasterProcessor& processor;
    FartLookAndFeel fartLnf;

    juce::Slider howMuchSlider;
    juce::Slider howWetSlider;
    juce::Label howMuchLabel;
    juce::Label howWetLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> howMuchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> howWetAttachment;

    juce::Image logoImage;
    std::unique_ptr<BrandButton> brandButton;

    juce::String intervalText;
    float stinkPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FartBlasterEditor)
};
