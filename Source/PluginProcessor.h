#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "SaalDsp.h"
#include "EQDsp.h"

class SaalAudioProcessor : public juce::AudioProcessor, private juce::Thread
{
public:
    SaalAudioProcessor();
    ~SaalAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return saal::maxIrSeconds + 0.3; }

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Preset>& presets();
    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Raumquelle
    static juce::File roomsFolder();
    bool loadIrFile (const juce::File& f, juce::String& error);   // eigene IR (Nachrichten-Thread)
    void useBuiltIn (int roomIndex);
    bool isCustom() const;
    juce::String customName() const;
    juce::File customFile() const;

    // Anzeige
    struct Display
    {
        bool custom = false;
        int room = 0;
        juce::String name;
        double micDistance = 0;
        float t60 = 0, wetGainDb = 0;
        std::array<std::vector<float>, 3> decay;
    };
    Display getDisplay() const { const juce::ScopedLock sl (displayLock); return display; }
    std::atomic<int> displayVersion { 0 };

    // Raum messen
    enum MeasureState { idle = 0, running, analysing, done, failed, finishing };
    bool startMeasurement (float levelDb, const juce::String& name);
    void cancelMeasurement();
    std::atomic<int> measureState { idle };
    std::atomic<float> measureProgress { 0 }, measureInput { 0 };
    std::atomic<bool> measureClipped { false };
    juce::String measureMessage;       // nur Nachrichten-Thread
    juce::File lastMeasurement;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> vuL { 0 }, vuR { 0 }, meterL { 0 }, meterR { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void run() override;
    saal::Settings currentSettings() const;
    void regenerate (const saal::Settings& s, double rate);
    void finishMeasurement();

    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    juce::SpinLock pendingLock;
    std::unique_ptr<juce::AudioBuffer<float>> pendingIr, usedIr;   // usedIr wird im Rechen-Thread freigegeben
    double pendingRate = 48000;
    std::atomic<float> wetGainDb { 0 };

    // eigene IR (roh) – Zugriff nur unter rawLock
    juce::CriticalSection rawLock;
    juce::AudioBuffer<float> rawIr;
    double rawRate = 48000;
    juce::File rawFile;
    std::atomic<int> sourceVersion { 0 };
    std::atomic<bool> needsRegen { true };

    mutable juce::CriticalSection displayLock;
    Display display;

    // Signalweg
    juce::AudioBuffer<float> wet;
    std::vector<float> preBufL, preBufR;
    int preWrite = 0;
    juce::SmoothedValue<float> smPre, smMix, smWidth, smWet, smOut;
    eq::Coeffs lowCut, highCut;
    eq::BiquadState lcL, lcR, hcL, hcR;
    float lastLow = -1, lastHigh = -1;

    std::atomic<float>* pRoom = nullptr; std::atomic<float>* pDist = nullptr; std::atomic<float>* pLen = nullptr;
    std::atomic<float>* pDamp = nullptr; std::atomic<float>* pEarly = nullptr; std::atomic<float>* pPre = nullptr;
    std::atomic<float>* pLow = nullptr;  std::atomic<float>* pHigh = nullptr; std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMix = nullptr;  std::atomic<float>* pOut = nullptr;  std::atomic<float>* pOn = nullptr;

    // Messung
    saal::Sweep sweep;
    std::vector<float> sweepData;
    juce::AudioBuffer<float> recording;
    int measurePos = 0, measureLead = 0;
    float measureGain = 0.125f;
    juce::String measureName;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    float vuStateL = 0, vuStateR = 0, vuCoef = 0.001f;
    int currentProgram = 0;
    std::atomic<double> sr { 48000.0 };
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaalAudioProcessor)
};
