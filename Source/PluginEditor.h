#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VintageUI.h"
#include "RackStyle.h"

//==============================================================================
// Fenster mit Grundriss des Raums (Quelle, Mikrofone, erste Reflexionen) und Abklingkurven
class RoomView : public juce::Component
{
public:
    explicit RoomView (VintageLookAndFeel& l) : lnf (l) {}
    void setData (const SaalAudioProcessor::Display& d) { data = d; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void paintPlan (juce::Graphics&, juce::Rectangle<float>);
    void paintCustom (juce::Graphics&, juce::Rectangle<float>);
    void paintDecay (juce::Graphics&, juce::Rectangle<float>);
    VintageLookAndFeel& lnf;
    SaalAudioProcessor::Display data;
};

//==============================================================================
class MeasurePanel : public juce::Component
{
public:
    MeasurePanel (SaalAudioProcessor&, VintageLookAndFeel&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    std::function<void()> onClose;

private:
    SaalAudioProcessor& proc;
    VintageLookAndFeel& lnf;
    juce::TextEditor name;
    juce::Slider level;
    juce::TextButton startBtn, closeBtn;
    juce::Rectangle<int> textArea, progressArea, inputArea, statusArea, levelLabel, nameLabel;
    juce::String status;
};

//==============================================================================
class SaalAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SaalAudioProcessorEditor (SaalAudioProcessor&);
    ~SaalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // für Tests
    void openMeasurePanel();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; juce::String name; std::unique_ptr<SliderAttachment> attachment; bool big = false; };
    struct RoomCombo : juce::ComboBox
    {
        std::function<void()> beforePopup;
        void showPopup() override { if (beforePopup) beforePopup(); juce::ComboBox::showPopup(); }
    };

    Knob& addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps = {});
    void timerCallback() override;
    void renderPanel();
    void fillRoomBox();
    void syncRoomBox();
    void roomChosen();
    void chooseFile();

    SaalAudioProcessor& proc;
    RackLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ToggleButton onSwitch;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    juce::ComboBox presetBox;
    RoomCombo roomBox;
    juce::TextButton loadBtn, measureBtn;
    juce::Array<juce::File> roomFiles;
    RoomView view { lnf };
    VuMeter vu { lnf, "OUT" };
    struct Dimmer : juce::Component { void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black.withAlpha (0.5f)); } } dimmer;
    std::unique_ptr<MeasurePanel> measure;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastDisplayVersion = -1;
    juce::String description;

    static constexpr int rackW = 1440, rackH = 540;
    juce::Rectangle<int> nameplate, vuBezel, tagArea, switchLabel, descArea, viewBezel;
    std::array<juce::Rectangle<int>, 4> sections;
    juce::Image panelImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaalAudioProcessorEditor)
};
