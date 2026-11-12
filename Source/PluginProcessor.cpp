#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

namespace
{
using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float n) { return a * std::pow (b / a, n); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}

Attr unit (const char* u, int decimals)
{
    return Attr().withStringFromValueFunction ([=] (float v, int) { return juce::String (v, decimals).replaceCharacter ('.', ',') + " " + juce::String (juce::CharPointer_UTF8 (u)); })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.replaceCharacter (',', '.').getFloatValue(); });
}

juce::String u8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

constexpr double maxPreDelay = 0.25;
}

juce::AudioProcessorValueTreeState::ParameterLayout SaalAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray names;
    for (auto& r : saal::rooms()) names.add (u8 (r.name));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "room", 1 }, "Raum", names, 0));
    l.add (std::make_unique<APF> (juce::ParameterID { "distance", 1 }, "Abstand", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "length", 1 }, u8 ("L\xc3\xa4nge"), juce::NormalisableRange<float> (50.0f, 200.0f, 0.1f), 100.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "damping", 1 }, u8 ("D\xc3\xa4mpfung"), juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "early", 1 }, "Erstreflexionen", juce::NormalisableRange<float> (-12.0f, 6.0f, 0.1f), 0.0f, unit ("dB", 1)));
    l.add (std::make_unique<APF> (juce::ParameterID { "predelay", 1 }, "Pre-Delay", juce::NormalisableRange<float> (0.0f, 250.0f, 0.1f), 10.0f, unit ("ms", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "lowcut", 1 }, "Low Cut", logRange (20.0f, 800.0f), 100.0f, unit ("Hz", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "highcut", 1 }, "High Cut", logRange (2000.0f, 20000.0f), 16000.0f, unit ("Hz", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "width", 1 }, "Breite", juce::NormalisableRange<float> (0.0f, 150.0f, 0.1f), 100.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "out", 1 }, "Ausgang", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, unit ("dB", 1)));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Raum an", true));
    return l;
}

const std::vector<SaalAudioProcessor::Preset>& SaalAudioProcessor::presets()
{
    static const std::vector<Preset> p = {
        { "Standard",                      { { "room", 0 }, { "distance", 40 }, { "length", 100 }, { "damping", 30 }, { "early", 0 },  { "predelay", 10 }, { "lowcut", 100 }, { "highcut", 16000 }, { "width", 100 }, { "mix", 25 } } },
        { "Gesang \xc2\xb7 Studio A weit", { { "room", 0 }, { "distance", 65 }, { "length", 105 }, { "damping", 35 }, { "early", -3 }, { "predelay", 45 }, { "lowcut", 180 }, { "highcut", 14000 }, { "width", 110 }, { "mix", 20 } } },
        { "Drums \xc2\xb7 Steinraum",      { { "room", 2 }, { "distance", 50 }, { "length", 90 },  { "damping", 20 }, { "early", 3 },  { "predelay", 0 },  { "lowcut", 80 },  { "highcut", 16000 }, { "width", 120 }, { "mix", 30 } } },
        { "Gitarre \xc2\xb7 Holzraum",     { { "room", 1 }, { "distance", 35 }, { "length", 100 }, { "damping", 35 }, { "early", 0 },  { "predelay", 8 },  { "lowcut", 120 }, { "highcut", 12000 }, { "width", 100 }, { "mix", 22 } } },
        { "Kleiner Raum \xc2\xb7 dicht",   { { "room", 3 }, { "distance", 50 }, { "length", 100 }, { "damping", 30 }, { "early", 2 },  { "predelay", 0 },  { "lowcut", 120 }, { "highcut", 14000 }, { "width", 100 }, { "mix", 30 } } },
        { "Hallkammer der 60er",           { { "room", 4 }, { "distance", 50 }, { "length", 100 }, { "damping", 40 }, { "early", -2 }, { "predelay", 25 }, { "lowcut", 200 }, { "highcut", 10000 }, { "width", 100 }, { "mix", 22 } } },
        { "Konzertsaal",                   { { "room", 5 }, { "distance", 45 }, { "length", 100 }, { "damping", 30 }, { "early", 0 },  { "predelay", 20 }, { "lowcut", 100 }, { "highcut", 14000 }, { "width", 110 }, { "mix", 25 } } },
        { "Kirche",                        { { "room", 6 }, { "distance", 50 }, { "length", 100 }, { "damping", 35 }, { "early", -2 }, { "predelay", 30 }, { "lowcut", 150 }, { "highcut", 12000 }, { "width", 115 }, { "mix", 25 } } },
        { "Treppenhaus",                   { { "room", 7 }, { "distance", 45 }, { "length", 100 }, { "damping", 25 }, { "early", 0 },  { "predelay", 5 },  { "lowcut", 150 }, { "highcut", 14000 }, { "width", 90 },  { "mix", 25 } } },
        { "Send/Aux (100 % Raum)",         { { "room", 0 }, { "distance", 50 }, { "length", 100 }, { "damping", 30 }, { "early", 0 },  { "predelay", 20 }, { "lowcut", 150 }, { "highcut", 14000 }, { "width", 100 }, { "mix", 100 } } },
    };
    return p;
}

void SaalAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr && juce::MessageManager::getInstance()->isThisTheMessageThread())
        useBuiltIn ((int) presets()[(size_t) index].values[0].second);
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

const juce::String SaalAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size())
               ? u8 (presets()[(size_t) index].name) : juce::String();
}

SaalAudioProcessor::SaalAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("Klangbild Saal Raumrechner"),
      apvts (*this, nullptr, "KlangbildSaal", createLayout())
{
    pRoom = apvts.getRawParameterValue ("room");       pDist = apvts.getRawParameterValue ("distance");
    pLen = apvts.getRawParameterValue ("length");      pDamp = apvts.getRawParameterValue ("damping");
    pEarly = apvts.getRawParameterValue ("early");     pPre = apvts.getRawParameterValue ("predelay");
    pLow = apvts.getRawParameterValue ("lowcut");      pHigh = apvts.getRawParameterValue ("highcut");
    pWidth = apvts.getRawParameterValue ("width");     pMix = apvts.getRawParameterValue ("mix");
    pOut = apvts.getRawParameterValue ("out");         pOn = apvts.getRawParameterValue ("on");
    startThread (juce::Thread::Priority::low);
}

SaalAudioProcessor::~SaalAudioProcessor()
{
    alive->store (false);
    stopThread (4000);
}

bool SaalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

//==============================================================================
juce::File SaalAudioProcessor::roomsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
               .getChildFile ("Application Support")
#endif
               .getChildFile ("Klangbild").getChildFile (u8 ("R\xc3\xa4ume"));
}

bool SaalAudioProcessor::loadIrFile (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { error = "Datei nicht lesbar: " + f.getFileName(); return false; }
    if (reader->lengthInSamples < 64) { error = "Datei ist zu kurz"; return false; }
    const int n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * (saal::maxIrSeconds + 2.0)));
    juce::AudioBuffer<float> b ((int) juce::jlimit (1u, 2u, reader->numChannels), n);
    reader->read (&b, 0, n, 0, true, b.getNumChannels() > 1);
    {
        const juce::ScopedLock sl (rawLock);
        rawIr = std::move (b);
        rawRate = reader->sampleRate;
        rawFile = f;
    }
    apvts.state.setProperty ("irFile", f.getFullPathName(), nullptr);
    ++sourceVersion;
    needsRegen = true;
    notify();
    return true;
}

void SaalAudioProcessor::useBuiltIn (int roomIndex)
{
    {
        const juce::ScopedLock sl (rawLock);
        rawIr.setSize (0, 0);
        rawFile = juce::File();
    }
    apvts.state.setProperty ("irFile", juce::String(), nullptr);
    if (auto* p = apvts.getParameter ("room"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, (int) saal::rooms().size() - 1, roomIndex)));
    ++sourceVersion;
    needsRegen = true;
    notify();
}

bool SaalAudioProcessor::isCustom() const { const juce::ScopedLock sl (rawLock); return rawIr.getNumSamples() > 0; }
juce::String SaalAudioProcessor::customName() const { const juce::ScopedLock sl (rawLock); return rawFile.getFileNameWithoutExtension(); }
juce::File SaalAudioProcessor::customFile() const { const juce::ScopedLock sl (rawLock); return rawFile; }

saal::Settings SaalAudioProcessor::currentSettings() const
{
    saal::Settings s;
    s.room = (int) pRoom->load();
    s.distance = pDist->load() * 0.01f;
    s.length = pLen->load() * 0.01f;
    s.damping = pDamp->load() * 0.01f;
    s.earlyDb = pEarly->load();
    return s;
}

void SaalAudioProcessor::regenerate (const saal::Settings& s, double rate)
{
    saal::Result res;
    bool custom = false;
    double irRate = rate;
    juce::String name;
    {
        juce::AudioBuffer<float> raw;
        {
            const juce::ScopedLock sl (rawLock);
            if (rawIr.getNumSamples() > 0)
            {
                raw.makeCopyOf (rawIr);
                irRate = rawRate;
                custom = true;
                name = rawFile.getFileNameWithoutExtension();
            }
        }
        res = custom ? saal::shapeCustom (raw, irRate, s) : saal::generateRoom (s, rate);
    }
    if (custom) res.wetGainDb = 0;
    res.wetGainDb -= 3.0f;   // Raum mit 100 % Mix etwa so laut wie das trockene Signal
    {
        const juce::ScopedLock sl (displayLock);
        display.custom = custom;
        display.room = s.room;
        display.name = custom ? name : u8 (saal::rooms()[(size_t) s.room].name);
        display.micDistance = res.micDistance;
        display.t60 = res.t60;
        display.wetGainDb = res.wetGainDb;
        display.decay = res.decay;
    }
    wetGainDb = res.wetGainDb;
    {
        auto b = std::make_unique<juce::AudioBuffer<float>> (std::move (res.ir));
        std::unique_ptr<juce::AudioBuffer<float>> oldPending, oldUsed;
        {
            const juce::SpinLock::ScopedLockType sl (pendingLock);
            oldPending = std::move (pendingIr);
            oldUsed = std::move (usedIr);
            pendingIr = std::move (b);
            pendingRate = irRate;
        }
    }
    ++displayVersion;
}

void SaalAudioProcessor::run()
{
    saal::Settings last; last.room = -1;
    int lastSource = -1;
    double lastRate = 0;
    int stableCount = 0;
    saal::Settings candidate;
    while (! threadShouldExit())
    {
        wait (40);
        if (threadShouldExit()) break;
        if (measureState.load() == analysing) { finishMeasurement(); continue; }

        const auto s = currentSettings();
        const double rate = sr.load();
        const bool changed = needsRegen.load() || lastSource != sourceVersion.load() || ! juce::exactlyEqual (rate, lastRate)
                          || s.room != last.room || ! juce::exactlyEqual (s.distance, last.distance) || ! juce::exactlyEqual (s.length, last.length)
                          || ! juce::exactlyEqual (s.damping, last.damping) || ! juce::exactlyEqual (s.earlyDb, last.earlyDb);
        if (! changed) { stableCount = 0; continue; }
        // beim Drehen kurz abwarten, bis der Regler ruht
        const bool same = s.room == candidate.room && juce::exactlyEqual (s.distance, candidate.distance) && juce::exactlyEqual (s.length, candidate.length)
                       && juce::exactlyEqual (s.damping, candidate.damping) && juce::exactlyEqual (s.earlyDb, candidate.earlyDb);
        candidate = s;
        if (! same && ! needsRegen.load()) { stableCount = 0; continue; }
        if (++stableCount < 2 && ! needsRegen.load()) continue;

        needsRegen = false;
        lastSource = sourceVersion.load();
        lastRate = rate;
        last = s;
        stableCount = 0;
        regenerate (s, rate);
    }
}

//==============================================================================
void SaalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    conv.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    conv.reset();
    // Raum sofort berechnen, damit auch der erste Block (Bounce) schon klingt
    regenerate (currentSettings(), sampleRate);
    {
        const juce::SpinLock::ScopedLockType sl (pendingLock);
        if (pendingIr != nullptr)
        {
            conv.loadImpulseResponse (std::move (*pendingIr), pendingRate, juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            pendingIr.reset();
        }
    }
    needsRegen = false;

    wet.setSize (2, maxBlock);
    const int preLen = (int) (maxPreDelay * sampleRate) + maxBlock + 4;
    preBufL.assign ((size_t) preLen, 0.0f);
    preBufR.assign ((size_t) preLen, 0.0f);
    preWrite = 0;
    smPre.reset (sampleRate, 0.08);   smPre.setCurrentAndTargetValue (pPre->load() * 0.001f * (float) sampleRate);
    smMix.reset (sampleRate, 0.03);   smMix.setCurrentAndTargetValue (pMix->load() * 0.01f * (pOn->load() > 0.5f ? 1.0f : 0.0f));
    smWidth.reset (sampleRate, 0.03); smWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    smWet.reset (sampleRate, 0.15);   smWet.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (wetGainDb.load()));
    smOut.reset (sampleRate, 0.03);   smOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    lastLow = lastHigh = -1;
    lcL.reset(); lcR.reset(); hcL.reset(); hcR.reset();
    vuCoef = (float) (1.0 - std::exp (-1.0 / (0.01 * sampleRate)));
    vuStateL = vuStateR = 0;
}

void SaalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int inCh = getTotalNumInputChannels();
    const int n = buffer.getNumSamples();
    if (inCh == 0 || buffer.getNumChannels() < 2)
        return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const bool mono = inCh == 1;

    // ---- Raum messen: Sweep ausgeben, Eingang aufnehmen
    if (measureState.load() == running)
    {
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            const int p = measurePos + i;
            if (p < recording.getNumSamples())
            {
                recording.setSample (0, p, L[i]);
                if (recording.getNumChannels() > 1) recording.setSample (1, p, mono ? L[i] : R[i]);
                pk = juce::jmax (pk, std::abs (L[i]), mono ? 0.0f : std::abs (R[i]));
            }
            const int sp = p - measureLead;
            const float out = (sp >= 0 && sp < (int) sweepData.size()) ? sweepData[(size_t) sp] * measureGain : 0.0f;
            L[i] = R[i] = out;
        }
        if (pk > 0.98f) measureClipped = true;
        measureInput = juce::jmax (pk, measureInput.load() * 0.9f);
        measurePos += n;
        measureProgress = juce::jmin (1.0f, (float) measurePos / (float) juce::jmax (1, recording.getNumSamples()));
        if (measurePos >= recording.getNumSamples())
        {
            measureState = analysing;
            notify();
        }
        return;
    }

    // ---- neue Impulsantwort übernehmen (ohne Speicheranforderung im Audio-Thread)
    {
        const juce::SpinLock::ScopedTryLockType tl (pendingLock);
        if (tl.isLocked() && pendingIr != nullptr)
        {
            conv.loadImpulseResponse (std::move (*pendingIr), pendingRate, juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            usedIr = std::move (pendingIr);   // Freigabe später im Rechen-Thread
        }
    }

    const double rate = sr.load();
    if (! juce::exactlyEqual (pLow->load(), lastLow))   { lastLow = pLow->load();   lowCut = eq::makeHighPass (rate, lastLow, 0.7071); }
    if (! juce::exactlyEqual (pHigh->load(), lastHigh)) { lastHigh = pHigh->load(); highCut = eq::makeLowPass (rate, juce::jmin ((double) lastHigh, rate * 0.45), 0.7071); }
    smPre.setTargetValue (pPre->load() * 0.001f * (float) rate);
    smMix.setTargetValue (pMix->load() * 0.01f * (pOn->load() > 0.5f ? 1.0f : 0.0f));
    smWidth.setTargetValue (pWidth->load() * 0.01f);
    smWet.setTargetValue (juce::Decibels::decibelsToGain (wetGainDb.load()));
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    if (wet.getNumSamples() < n) return;   // Host hält sich nicht an die Blockgröße

    // ---- Pre-Delay vor der Faltung; Eingang leicht stereo (Mitte + 30 % Seite)
    const int pl = (int) preBufL.size();
    float* wl = wet.getWritePointer (0);
    float* wr = wet.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float dl = L[i], dr = mono ? dl : R[i];
        const float m = 0.5f * (dl + dr), s = 0.5f * (dl - dr) * 0.3f;
        preBufL[(size_t) preWrite] = m + s;
        preBufR[(size_t) preWrite] = m - s;
        const float d = smPre.getNextValue();
        float rp = (float) preWrite - d;
        while (rp < 0) rp += (float) pl;
        const int r0 = (int) rp;
        const int r1 = (r0 + 1) % pl;
        const float fr = rp - (float) r0;
        wl[i] = preBufL[(size_t) r0] + (preBufL[(size_t) r1] - preBufL[(size_t) r0]) * fr;
        wr[i] = preBufR[(size_t) r0] + (preBufR[(size_t) r1] - preBufR[(size_t) r0]) * fr;
        preWrite = (preWrite + 1) % pl;
    }

    {
        juce::dsp::AudioBlock<float> blk (wet.getArrayOfWritePointers(), 2, (size_t) n);
        conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    float pkL = 0, pkR = 0;
    for (int i = 0; i < n; ++i)
    {
        float a = hcL.process (highCut, lcL.process (lowCut, wl[i]));
        float b = hcR.process (highCut, lcR.process (lowCut, wr[i]));
        const float w = smWidth.getNextValue();
        const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * w;
        const float g = smWet.getNextValue();
        a = (mid + side) * g;
        b = (mid - side) * g;

        const float dl = L[i], dr = mono ? dl : R[i];
        const float m = smMix.getNextValue();
        const float dryG = std::cos (m * juce::MathConstants<float>::halfPi);
        const float wetG = std::sin (m * juce::MathConstants<float>::halfPi);
        const float o = smOut.getNextValue();
        L[i] = (dl * dryG + a * wetG) * o;
        R[i] = (dr * dryG + b * wetG) * o;

        vuStateL += (std::abs (L[i]) - vuStateL) * vuCoef;
        vuStateR += (std::abs (R[i]) - vuStateR) * vuCoef;
        pkL = juce::jmax (pkL, std::abs (L[i]));
        pkR = juce::jmax (pkR, std::abs (R[i]));
    }
    vuL.store (vuStateL); vuR.store (vuStateR);
    meterL.store (juce::jmax (pkL, meterL.load())); meterR.store (juce::jmax (pkR, meterR.load()));
}

//==============================================================================
bool SaalAudioProcessor::startMeasurement (float levelDb, const juce::String& name)
{
    if (measureState.load() == running || measureState.load() == analysing)
        return false;
    const double rate = sr.load();
    sweep.sr = rate; sweep.seconds = 8.0; sweep.f1 = 20.0; sweep.f2 = juce::jmin (20000.0, rate * 0.45);
    sweepData.resize ((size_t) sweep.length());
    for (int i = 0; i < sweep.length(); ++i) sweepData[(size_t) i] = sweep.sample (i);
    measureLead = (int) (0.5 * rate);
    recording.setSize (2, measureLead + sweep.length() + (int) (4.5 * rate));
    recording.clear();
    measureGain = juce::Decibels::decibelsToGain (juce::jlimit (-40.0f, -3.0f, levelDb));
    measureName = name.trim().isNotEmpty() ? name.trim() : juce::String ("Mein Raum");
    measurePos = 0;
    measureProgress = 0; measureInput = 0; measureClipped = false;
    measureMessage.clear();
    measureState = running;
    return true;
}

void SaalAudioProcessor::cancelMeasurement()
{
    if (measureState.load() == running)
        measureState = idle;
}

void SaalAudioProcessor::finishMeasurement()
{
    measureState = finishing;
    float peak = 0;
    for (int c = 0; c < recording.getNumChannels(); ++c)
        peak = juce::jmax (peak, recording.getMagnitude (c, 0, recording.getNumSamples()));
    if (peak < 0.003f)
    {
        juce::MessageManager::callAsync ([this, a = alive] { if (! a->load()) return; measureMessage = u8 ("Kein Signal am Eingang \xe2\x80\x93 Mikrofon, Spur-Eingang und Monitoring pr\xc3\xbc" "fen."); measureState = failed; });
        return;
    }
    // Aufnahme ab dem Sweep-Start (Vorlauf enthält nur Stille + Latenz)
    juce::AudioBuffer<float> rec (recording.getNumChannels(), recording.getNumSamples() - measureLead);
    for (int c = 0; c < rec.getNumChannels(); ++c)
        rec.copyFrom (c, 0, recording, c, measureLead, rec.getNumSamples());
    // gleich aufgenommene Kanäle (Mono-Mikro) → eine Spur
    bool same = true;
    for (int i = 0; i < rec.getNumSamples() && same; i += 7)
        if (std::abs (rec.getSample (0, i) - rec.getSample (1, i)) > 1e-6f) same = false;
    if (same) rec.setSize (1, rec.getNumSamples(), true);

    auto ir = saal::deconvolve (rec, sweep, 6.0);
    const float m = ir.getMagnitude (0, ir.getNumSamples());
    if (m > 0) ir.applyGain (0.5f / m);

    auto folder = roomsFolder();
    folder.createDirectory();
    auto file = folder.getChildFile (juce::File::createLegalFileName (measureName) + ".wav");
    if (file.exists()) file = file.getNonexistentSibling (true);
    bool ok = false;
    {
        juce::WavAudioFormat wav;
        if (auto os = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sweep.sr, (unsigned) ir.getNumChannels(), 24, {}, 0)))
            {
                os.release();
                ok = w->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
            }
    }
    const bool clipped = measureClipped.load();
    juce::MessageManager::callAsync ([this, a = alive, file, ok, clipped]
    {
        if (! a->load()) return;
        juce::String err;
        if (ok && loadIrFile (file, err))
        {
            lastMeasurement = file;
            measureMessage = u8 ("Gemessen und gesichert: ") + file.getFileNameWithoutExtension()
                           + (clipped ? u8 (" \xe2\x80\x93 Achtung, der Eingang war \xc3\xbc" "bersteuert, besser mit weniger Pegel wiederholen.") : juce::String());
            measureState = done;
        }
        else
        {
            measureMessage = "Speichern fehlgeschlagen. " + err;
            measureState = failed;
        }
    });
}

//==============================================================================
void SaalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SaalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
    const juce::String path = tree.getProperty ("irFile", juce::String()).toString();
    apvts.replaceState (tree);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->convertTo0to1 (rp->convertFrom0to1 (rp->getValue())));

    juce::String err;
    const juce::File f (path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File());
    if (f.existsAsFile() && loadIrFile (f, err))
        return;
    // keine (oder fehlende) eigene Datei → eingebauter Raum
    {
        const juce::ScopedLock sl (rawLock);
        rawIr.setSize (0, 0);
        rawFile = juce::File();
    }
    apvts.state.setProperty ("irFile", juce::String(), nullptr);
    ++sourceVersion;
    needsRegen = true;
    notify();
}

juce::AudioProcessorEditor* SaalAudioProcessor::createEditor() { return new kb::ScaledEditor (*this, new SaalAudioProcessorEditor (*this)); }

#ifndef KB_EMBEDDED
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SaalAudioProcessor(); }
#endif
