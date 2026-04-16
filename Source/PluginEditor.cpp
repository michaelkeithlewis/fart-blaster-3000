#include "PluginEditor.h"
#include "BinaryData.h"

// =============================================================================
// Custom LookAndFeel
// =============================================================================

void FartLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float sliderPos, float startAngle, float endAngle,
                                       juce::Slider&)
{
    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(10.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Outer glow
    g.setColour(juce::Colour(0x2033ff33));
    g.fillEllipse(cx - radius - 4, cy - radius - 4, (radius + 4) * 2, (radius + 4) * 2);

    // Dark background ring
    g.setColour(juce::Colour(0xff0a1a0a));
    g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

    // Track arc (dim)
    {
        juce::Path track;
        track.addCentredArc(cx, cy, radius - 6, radius - 6, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff1a3a1a));
        g.strokePath(track, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // Value arc (bright green)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius - 6, radius - 6, 0.0f, startAngle, angle, true);
        g.setColour(juce::Colour(0xff33ff33));
        g.strokePath(arc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    // Center knob body (brown gradient)
    float knobR = radius * 0.55f;
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff6a4a2a), cx, cy - knobR,
        juce::Colour(0xff3d2b1f), cx, cy + knobR, false));
    g.fillEllipse(cx - knobR, cy - knobR, knobR * 2, knobR * 2);

    // Knob border
    g.setColour(juce::Colour(0xff88664a));
    g.drawEllipse(cx - knobR, cy - knobR, knobR * 2, knobR * 2, 2.0f);

    // Pointer line
    {
        juce::Path pointer;
        auto pLen = radius * 0.45f;
        pointer.addRoundedRectangle(-2.0f, -pLen, 4.0f, pLen * 0.7f, 2.0f);
        g.setColour(juce::Colour(0xffffff00));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(cx, cy));
    }

    // Dot at pointer tip
    {
        float dotX = cx + std::sin(angle) * (radius - 6);
        float dotY = cy - std::cos(angle) * (radius - 6);
        g.setColour(juce::Colour(0xffffff00));
        g.fillEllipse(dotX - 4, dotY - 4, 8, 8);
    }
}

// =============================================================================
// Clickable brand button with Truck Packer logo
// =============================================================================

BrandButton::BrandButton(const juce::Image& logoImg) : logo(logoImg)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void BrandButton::paint(juce::Graphics& g)
{
    float alpha = hovering ? 1.0f : 0.6f;

    g.setColour(juce::Colour(0xffaaaaaa).withAlpha(alpha * 0.7f));
    g.setFont(juce::Font("Papyrus", 12.0f, juce::Font::plain));
    g.drawText("Made by", getLocalBounds().removeFromTop(14), juce::Justification::centred);

    if (logo.isValid())
    {
        float aspect = static_cast<float>(logo.getWidth()) / static_cast<float>(logo.getHeight());
        int imgH = 22;
        int imgW = static_cast<int>(imgH * aspect);
        int imgX = (getWidth() - imgW) / 2;
        g.setOpacity(alpha);
        g.drawImage(logo, imgX, 16, imgW, imgH,
                    0, 0, logo.getWidth(), logo.getHeight());
    }
}

void BrandButton::mouseUp(const juce::MouseEvent&)
{
    juce::URL("https://truckpacker.com").launchInDefaultBrowser();
}

void BrandButton::mouseEnter(const juce::MouseEvent&) { hovering = true; repaint(); }
void BrandButton::mouseExit(const juce::MouseEvent&)  { hovering = false; repaint(); }

// =============================================================================
// Editor
// =============================================================================

FartBlasterEditor::FartBlasterEditor(FartBlasterProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&fartLnf);

    auto setupSlider = [this](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    };

    setupSlider(howMuchSlider);
    setupSlider(howWetSlider);

    auto setupLabel = [this](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colour(0xff44ff44));
        l.setFont(juce::Font("Papyrus", 22.0f, juce::Font::bold));
        addAndMakeVisible(l);
    };

    setupLabel(howMuchLabel, "HOW MUCH?");
    setupLabel(howWetLabel, "HOW WET?");

    howMuchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "howMuch", howMuchSlider);
    howWetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "howWet", howWetSlider);

    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::logotruckpacker_png, BinaryData::logotruckpacker_pngSize);
    brandButton = std::make_unique<BrandButton>(logoImage);
    addAndMakeVisible(*brandButton);

    // setSize must be last -- it triggers resized() which needs all members ready
    setSize(520, 480);
    startTimerHz(30);
}

FartBlasterEditor::~FartBlasterEditor()
{
    setLookAndFeel(nullptr);
}

void FartBlasterEditor::timerCallback()
{
    stinkPhase += 0.06f;

    float howMuch = processor.apvts.getRawParameterValue("howMuch")->load();
    if (howMuch < 0.001f)
    {
        intervalText = "OFF";
    }
    else if (howMuch > 0.98f)
    {
        intervalText = "NONSTOP";
    }
    else
    {
        float inv = 1.0f - howMuch;
        float curved = inv * inv * inv * inv;
        float sec = 0.05f + 59.95f * curved;
        if (sec >= 10.0f)
            intervalText = "every ~" + juce::String(static_cast<int>(sec)) + "s";
        else
            intervalText = "every ~" + juce::String(sec, 1) + "s";
    }

    repaint();
}

void FartBlasterEditor::drawStinkLines(juce::Graphics& g, float x, float y, float scale)
{
    for (int i = 0; i < 3; ++i)
    {
        float alpha = 0.07f + 0.04f * std::sin(stinkPhase + i * 1.5f);
        g.setColour(juce::Colour(0xff33ff33).withAlpha(alpha));

        float wave = 6.0f * std::sin(stinkPhase * 0.8f + i * 2.1f) * scale;
        float dx = i * 14.0f * scale;

        juce::Path stink;
        stink.startNewSubPath(x + dx, y);
        stink.cubicTo(x + dx - 12 * scale + wave, y - 50 * scale,
                      x + dx + 18 * scale - wave, y - 100 * scale,
                      x + dx - 6 * scale + wave * 0.5f, y - 150 * scale);
        g.strokePath(stink, juce::PathStrokeType(2.0f + i * 0.6f));
    }
}

void FartBlasterEditor::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();

    // Background: muddy brown gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff3d2b1f), 0.0f, 0.0f,
        juce::Colour(0xff1a0e08), 0.0f, static_cast<float>(h), false));
    g.fillAll();

    // Subtle border glow
    g.setColour(juce::Colour(0x3333ff33));
    g.drawRect(getLocalBounds(), 2);

    // Animated stink lines
    drawStinkLines(g, 30.0f, static_cast<float>(h) - 60.0f, 1.0f);
    drawStinkLines(g, static_cast<float>(w) - 70.0f, static_cast<float>(h) - 80.0f, 0.8f);
    drawStinkLines(g, static_cast<float>(w) * 0.5f - 15.0f, static_cast<float>(h) - 50.0f, 0.9f);
    drawStinkLines(g, 80.0f, static_cast<float>(h) - 120.0f, 0.6f);
    drawStinkLines(g, static_cast<float>(w) - 120.0f, static_cast<float>(h) - 110.0f, 0.7f);

    // Title shadow
    g.setColour(juce::Colour(0x88000000));
    g.setFont(juce::Font("Papyrus", 44.0f, juce::Font::bold));
    g.drawFittedText("FART BLASTER 3000", 3, 18, w, 55,
                     juce::Justification::centred, 1);

    // Title
    g.setColour(juce::Colour(0xff33ff33));
    g.drawFittedText("FART BLASTER 3000", 0, 15, w, 55,
                     juce::Justification::centred, 1);

    // Subtitle
    g.setColour(juce::Colour(0xccff8800));
    g.setFont(juce::Font("Papyrus", 16.0f, juce::Font::italic));
    g.drawFittedText("~ The Ultimate Flatulence Experience ~", 0, 68, w, 25,
                     juce::Justification::centred, 1);

    // Decorative separator line
    g.setColour(juce::Colour(0x4433ff33));
    g.drawHorizontalLine(96, 40.0f, static_cast<float>(w) - 40.0f);

    // Interval readout below HOW MUCH knob
    if (intervalText.isNotEmpty())
    {
        g.setColour(juce::Colour(0xaaff8800));
        g.setFont(juce::Font("Papyrus", 15.0f, juce::Font::plain));
        int knobSize = 190;
        int spacing = 30;
        int startX = (w - (knobSize * 2 + spacing)) / 2;
        g.drawFittedText(intervalText, startX, 334, knobSize, 20,
                         juce::Justification::centred, 1);
    }

    // Separator above footer branding
    g.setColour(juce::Colour(0x2233ff33));
    g.drawHorizontalLine(h - 52, 60.0f, static_cast<float>(w) - 60.0f);
}

void FartBlasterEditor::resized()
{
    int knobSize = 190;
    int spacing = 30;
    int totalWidth = knobSize * 2 + spacing;
    int startX = (getWidth() - totalWidth) / 2;
    int knobY = 110;
    int labelY = knobY + knobSize + 2;

    howMuchSlider.setBounds(startX, knobY, knobSize, knobSize);
    howMuchLabel.setBounds(startX, labelY, knobSize, 30);

    howWetSlider.setBounds(startX + knobSize + spacing, knobY, knobSize, knobSize);
    howWetLabel.setBounds(startX + knobSize + spacing, labelY, knobSize, 30);

    // Brand button in the footer area
    brandButton->setBounds((getWidth() - 200) / 2, getHeight() - 48, 200, 42);
}
