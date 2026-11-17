#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VbmContent;

class VbmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VbmLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
};

class VbmEditor : public juce::AudioProcessorEditor
{
public:
    explicit VbmEditor (VbmProcessor&);
    ~VbmEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1200, baseH = 780;

private:
    VbmProcessor& proc;
    VbmLookAndFeel lnf;
    std::unique_ptr<VbmContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbmEditor)
};
