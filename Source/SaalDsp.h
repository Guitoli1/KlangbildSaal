#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <random>
#include <vector>

// Klangbild Saal – Faltungshall mit berechneten Räumen.
// Eingebaute Räume werden aus Raummaßen (Spiegelquellen für die Erstreflexionen) und Nachhallzeiten
// je Oktavband (diffuser Nachhall) als Impulsantwort berechnet. Eigene IRs (WAV/AIFF) oder selbst
// gemessene Räume werden mit denselben Reglern (Länge, Dämpfung, Erstreflexionen) geformt.
namespace saal
{
constexpr int numBands = 7;
constexpr std::array<double, numBands> bandCentres { 125, 250, 500, 1000, 2000, 4000, 8000 };
constexpr double maxIrSeconds = 8.0;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad make (double sr, double f, bool high, double q = 0.70710678)
    {
        Biquad b;
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (5.0, sr * 0.45, f) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        if (high) { b.b0 = (1 + c) * 0.5; b.b1 = -(1 + c); b.b2 = (1 + c) * 0.5; }
        else      { b.b0 = (1 - c) * 0.5; b.b1 = 1 - c;    b.b2 = (1 - c) * 0.5; }
        b.b0 /= a0; b.b1 /= a0; b.b2 /= a0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
        return b;
    }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Linkwitz-Riley 4. Ordnung (zwei Butterworth hintereinander)
struct LR4
{
    Biquad a, b;
    static LR4 make (double sr, double f, bool high) { LR4 l; l.a = Biquad::make (sr, f, high); l.b = l.a; return l; }
    inline double process (double x) { return b.process (a.process (x)); }
};

// Signal in 7 Oktavbänder zerlegen (Summe = Allpass des Originals)
inline std::vector<std::vector<float>> splitBands (const float* x, int n, double sr)
{
    std::vector<std::vector<float>> bands (numBands, std::vector<float> ((size_t) n));
    std::vector<double> rest (x, x + n);
    for (int b = 0; b < numBands - 1; ++b)
    {
        const double fc = std::sqrt (bandCentres[(size_t) b] * bandCentres[(size_t) b + 1]);
        auto lp = LR4::make (sr, fc, false), hp = LR4::make (sr, fc, true);
        for (int i = 0; i < n; ++i)
        {
            bands[(size_t) b][(size_t) i] = (float) lp.process (rest[(size_t) i]);
            rest[(size_t) i] = hp.process (rest[(size_t) i]);
        }
    }
    for (int i = 0; i < n; ++i)
        bands[numBands - 1][(size_t) i] = (float) rest[(size_t) i];
    return bands;
}

//==============================================================================
struct RoomDef
{
    const char* name;        // Anzeige
    const char* short_;      // kurze Beschreibung im Fenster
    double L, W, H;          // Meter
    std::array<double, numBands> t60;   // Sekunden je Oktavband
    double reflect;          // Reflexionsgrad der Wände (Erstreflexionen)
    double wallCut;          // Höhenverlust je Reflexion (Hz)
    double rMin, rMax;       // Mikrofonabstand
    int maxOrder;            // Ordnung der Spiegelquellen
};

inline const std::vector<RoomDef>& rooms()
{
    static const std::vector<RoomDef> r = {
        { "Studio A \xc2\xb7 gro\xc3\x9f" "er Aufnahmeraum", "Holzboden, hohe Decke, warm und offen",
          16, 11, 6.5, { 1.55, 1.45, 1.35, 1.28, 1.18, 1.02, 0.80 }, 0.86, 9000, 1.0, 9.0, 4 },
        { "Studio B \xc2\xb7 Holzraum", "mittelgro\xc3\x9f, lebendig, Holzverkleidung",
          9, 6.5, 4, { 0.92, 0.88, 0.82, 0.78, 0.72, 0.62, 0.48 }, 0.82, 8000, 0.8, 5.5, 4 },
        { "Steinraum \xc2\xb7 Drums", "Naturstein, hell und kr\xc3\xa4" "ftig \xe2\x80\x93 der Drum-Raum",
          7, 5.5, 4.5, { 1.20, 1.18, 1.12, 1.10, 1.06, 0.96, 0.78 }, 0.92, 12000, 0.8, 5.0, 4 },
        { "Kleiner Raum \xc2\xb7 trocken", "kurz und dicht \xe2\x80\x93 macht Spuren r\xc3\xa4umlich, ohne Hall",
          4.5, 3.8, 2.8, { 0.46, 0.40, 0.36, 0.33, 0.30, 0.27, 0.22 }, 0.72, 7000, 0.5, 3.0, 4 },
        { "Hallkammer \xc2\xb7 Beton", "gekachelte Echokammer wie in den 60ern",
          7, 4.5, 3.5, { 2.50, 2.65, 2.60, 2.50, 2.25, 1.85, 1.35 }, 0.95, 11000, 1.0, 4.5, 5 },
        { "Konzertsaal", "gro\xc3\x9f" "er Saal mit Holz und Polstern, weich und breit",
          38, 24, 15, { 2.40, 2.20, 2.00, 1.90, 1.70, 1.40, 1.00 }, 0.84, 7500, 4.0, 25.0, 3 },
        { "Kirche", "Steinkirche, sehr lang und erhaben",
          45, 18, 20, { 4.60, 4.30, 4.00, 3.70, 3.20, 2.50, 1.70 }, 0.92, 9000, 5.0, 30.0, 3 },
        { "Treppenhaus", "hoch und schmal, mit Flatterecho",
          3.5, 3.0, 16, { 2.20, 2.30, 2.40, 2.40, 2.20, 1.90, 1.40 }, 0.94, 10000, 1.0, 8.0, 6 },
    };
    return r;
}

struct Settings
{
    int room = 0;
    float distance = 0.4f;   // 0..1
    float length = 1.0f;     // 0,5..2
    float damping = 0.3f;    // 0..1
    float earlyDb = 0.0f;    // −12..+6
};

struct Result
{
    juce::AudioBuffer<float> ir;     // Stereo, Energie je Kanal = 1
    float wetGainDb = 0;             // Abstand: weiter weg = mehr Raum
    float t60 = 0;                   // Mitte (500 Hz/1 kHz), gemessen
    std::array<std::vector<float>, 3> decay;   // Abklingkurven tief/mitte/hoch in dB, 10-ms-Raster
    double micDistance = 0;
};

inline double micDistanceFor (const RoomDef& r, float d01)
{
    return r.rMin * std::pow (r.rMax / r.rMin, (double) juce::jlimit (0.0f, 1.0f, d01));
}

inline std::array<double, numBands> effectiveT60 (const RoomDef& r, const Settings& s)
{
    static constexpr std::array<double, numBands> dampW { 0.0, 0.0, 0.05, 0.15, 0.35, 0.55, 0.65 };
    std::array<double, numBands> t {};
    for (int b = 0; b < numBands; ++b)
        t[(size_t) b] = juce::jmax (0.08, r.t60[(size_t) b] * s.length * (1.0 - dampW[(size_t) b] * s.damping * 1.1));
    return t;
}

//==============================================================================
// Hilfen für Anzeige und Normierung
inline void normaliseEnergy (juce::AudioBuffer<float>& ir)
{
    double e = 0;
    for (int c = 0; c < ir.getNumChannels(); ++c)
        for (int i = 0; i < ir.getNumSamples(); ++i)
            e += (double) ir.getSample (c, i) * ir.getSample (c, i);
    e /= juce::jmax (1, ir.getNumChannels());
    if (e > 1e-12)
        ir.applyGain ((float) (1.0 / std::sqrt (e)));
}

// Schroeder-Rückwärtsintegration → dB-Kurve
inline std::vector<float> schroeder (const std::vector<float>& x, double sr, int stepMs = 10)
{
    const int n = (int) x.size();
    std::vector<double> e ((size_t) n + 1, 0.0);
    for (int i = n - 1; i >= 0; --i)
        e[(size_t) i] = e[(size_t) i + 1] + (double) x[(size_t) i] * x[(size_t) i];
    const double e0 = juce::jmax (1e-20, e[0]);
    const int step = juce::jmax (1, (int) (sr * stepMs / 1000.0));
    std::vector<float> out;
    for (int i = 0; i < n; i += step)
        out.push_back ((float) juce::jmax (-90.0, 10.0 * std::log10 (juce::jmax (1e-12, e[(size_t) i] / e0))));
    return out;
}

// T60 aus der Abklingkurve (−5 … −25 dB, auf 60 dB hochgerechnet)
inline float t60FromCurve (const std::vector<float>& c, int stepMs = 10)
{
    int i5 = -1, i25 = -1;
    for (int i = 0; i < (int) c.size(); ++i)
    {
        if (i5 < 0 && c[(size_t) i] <= -5.0f) i5 = i;
        if (i25 < 0 && c[(size_t) i] <= -25.0f) { i25 = i; break; }
    }
    if (i5 < 0 || i25 <= i5) return 0.0f;
    return (float) ((i25 - i5) * stepMs / 1000.0 * 3.0);
}

inline void fillDisplay (Result& res, double sr)
{
    const int n = res.ir.getNumSamples();
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i)
        mono[(size_t) i] = 0.5f * (res.ir.getSample (0, i) + res.ir.getSample (juce::jmin (1, res.ir.getNumChannels() - 1), i));
    auto bands = splitBands (mono.data(), n, sr);
    std::vector<float> lo ((size_t) n), mid ((size_t) n), hi ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        lo[(size_t) i] = bands[0][(size_t) i] + bands[1][(size_t) i];
        mid[(size_t) i] = bands[2][(size_t) i] + bands[3][(size_t) i];
        hi[(size_t) i] = bands[4][(size_t) i] + bands[5][(size_t) i] + bands[6][(size_t) i];
    }
    res.decay[0] = schroeder (lo, sr);
    res.decay[1] = schroeder (mid, sr);
    res.decay[2] = schroeder (hi, sr);
    res.t60 = t60FromCurve (res.decay[1]);
}

// Erstreflexionen (erste 80 ms nach dem Einsatz) lauter/leiser, weich übergeblendet
inline void applyEarlyBalance (juce::AudioBuffer<float>& ir, double sr, float earlyDb, int onset)
{
    if (std::abs (earlyDb) < 0.05f) return;
    const float g = juce::Decibels::decibelsToGain (earlyDb);
    const int e1 = onset + (int) (0.060 * sr), e2 = onset + (int) (0.100 * sr);
    for (int c = 0; c < ir.getNumChannels(); ++c)
        for (int i = 0; i < juce::jmin (e2, ir.getNumSamples()); ++i)
        {
            const float w = i <= e1 ? 1.0f : 1.0f - (float) (i - e1) / (float) (e2 - e1);
            ir.setSample (c, i, ir.getSample (c, i) * (1.0f + (g - 1.0f) * w));
        }
}

//==============================================================================
// Eingebauten Raum berechnen
inline Result generateRoom (const Settings& s, double sr)
{
    const auto& room = rooms()[(size_t) juce::jlimit (0, (int) rooms().size() - 1, s.room)];
    const auto T = effectiveT60 (room, s);
    const double tMax = *std::max_element (T.begin(), T.end());
    const double c = 343.0;
    const double V = room.L * room.W * room.H;
    const double mixTime = juce::jlimit (0.015, 0.12, std::sqrt (V) * 0.001 * 1.2);   // ≈ √V ms

    // Quelle und Mikrofone (ORTF: 17 cm, ±55°, Nieren, zur Quelle gerichtet)
    const double sx = room.L * 0.28, sy = room.W * 0.5, sz = juce::jmin (1.6, room.H * 0.4);
    const double r = juce::jmin (micDistanceFor (room, s.distance), room.L - sx - 0.4);
    const double mx = sx + r, my = sy, mz = juce::jmin (1.8, room.H * 0.45);
    const double spread = 0.085;
    const std::array<double, 2> micY { my + spread, my - spread };
    const std::array<double, 2> micAng { juce::MathConstants<double>::pi + 0.96, juce::MathConstants<double>::pi - 0.96 };   // Richtung im Grundriss

    const double tailLen = juce::jmin (maxIrSeconds, mixTime + tMax * 1.15 + 0.05);
    const int n = (int) std::ceil (tailLen * sr);
    Result res;
    res.micDistance = r;
    res.ir.setSize (2, n);
    res.ir.clear();

    // --- Erstreflexionen über Spiegelquellen, nach Ordnung getrennt gerendert (Höhenverlust je Ordnung)
    const int maxO = room.maxOrder;
    std::vector<std::array<std::vector<float>, 2>> byOrder ((size_t) maxO + 1);
    for (auto& o : byOrder) for (auto& ch : o) ch.assign ((size_t) n, 0.0f);
    const double dDirect = std::sqrt ((mx - sx) * (mx - sx) + (mz - sz) * (mz - sz));
    double firstRefl = 1e9;

    auto imagePos = [] (int i, double len, double src) { return i * len + ((i & 1) ? len - src : src); };
    for (int i = -maxO; i <= maxO; ++i)
        for (int j = -maxO; j <= maxO; ++j)
            for (int k = -maxO; k <= maxO; ++k)
            {
                const int order = std::abs (i) + std::abs (j) + std::abs (k);
                if (order == 0 || order > maxO) continue;
                const double ix = imagePos (i, room.L, sx), iy = imagePos (j, room.W, sy), iz = imagePos (k, room.H, sz);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double dx = ix - mx, dy = iy - micY[(size_t) ch], dz = iz - mz;
                    const double d = std::sqrt (dx * dx + dy * dy + dz * dz);
                    // Nierencharakteristik
                    const double hor = std::sqrt (dx * dx + dy * dy);
                    const double cosA = (dx * std::cos (micAng[(size_t) ch]) + dy * std::sin (micAng[(size_t) ch])) / juce::jmax (1e-9, d);
                    juce::ignoreUnused (hor);
                    const double pattern = 0.5 * (1.0 + cosA);
                    const double g = std::pow (room.reflect, order) * pattern * dDirect / d;
                    const double t = (d - dDirect) / c;
                    if (t <= 0 || t * sr >= n - 2) continue;
                    firstRefl = juce::jmin (firstRefl, t);
                    const double pos = t * sr;
                    const int p0 = (int) pos;
                    const float fr = (float) (pos - p0);
                    byOrder[(size_t) order][(size_t) ch][(size_t) p0] += (float) g * (1.0f - fr);
                    byOrder[(size_t) order][(size_t) ch][(size_t) p0 + 1] += (float) g * fr;
                }
            }
    if (firstRefl > 1e8) firstRefl = 0.002;

    double erEnergy = 0;
    for (int o = 1; o <= maxO; ++o)
    {
        const double cut = room.wallCut * std::pow (0.78, o - 1) * (1.0 - 0.45 * s.damping);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto lp = Biquad::make (sr, cut, false);
            auto& v = byOrder[(size_t) o][(size_t) ch];
            for (int t = 0; t < n; ++t)
            {
                const float y = (float) lp.process (v[(size_t) t]);
                res.ir.addSample (ch, t, y);
                erEnergy += (double) y * y;
            }
        }
    }
    erEnergy *= 0.5;

    // --- Diffuser Nachhall: Rauschen je Oktavband mit eigener Abklingzeit
    std::mt19937 rng (1234u + (unsigned) s.room * 77u);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    const int onset = (int) (firstRefl * sr);
    const int mixS = juce::jmax (onset + 1, (int) (mixTime * sr));
    // Diffusfeld-Energie relativ zum Direktschall: (r / rc)², rc = 0,057·√(V/T)
    const double rc = 0.057 * std::sqrt (V / juce::jmax (0.1, 0.5 * (T[2] + T[3])));
    const double tailEnergyTarget = (r * r) / (rc * rc) * 0.5;

    std::array<std::vector<float>, 2> tail;
    for (int ch = 0; ch < 2; ++ch)
    {
        std::vector<float> noise ((size_t) n);
        for (auto& v : noise) v = nd (rng);
        auto bands = splitBands (noise.data(), n, sr);
        tail[(size_t) ch].assign ((size_t) n, 0.0f);
        for (int b = 0; b < numBands; ++b)
        {
            const double k = 6.9078 / (T[(size_t) b] * sr);
            for (int t = onset; t < n; ++t)
            {
                // Echodichte baut sich bis zur Mischzeit auf
                const double x = (double) (t - onset) / (double) (mixS - onset);
                const double build = x >= 1.0 ? 1.0 : x * x * (3.0 - 2.0 * x);
                tail[(size_t) ch][(size_t) t] += (float) (bands[(size_t) b][(size_t) t] * std::exp (-k * (t - onset)) * build);
            }
        }
        // Ausblenden am Ende
        const int fade = juce::jmax (1, n / 10);
        for (int t = n - fade; t < n; ++t)
            tail[(size_t) ch][(size_t) t] *= 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) (t - (n - fade)) / (float) fade));
    }
    double tailE = 0;
    for (auto& tc : tail) for (auto v : tc) tailE += (double) v * v;
    tailE *= 0.5;
    const float tailGain = (float) std::sqrt (tailEnergyTarget / juce::jmax (1e-12, tailE));
    for (int ch = 0; ch < 2; ++ch)
        for (int t = 0; t < n; ++t)
            res.ir.addSample (ch, t, tail[(size_t) ch][(size_t) t] * tailGain);

    applyEarlyBalance (res.ir, sr, s.earlyDb, onset);

    // Gesamtenergie relativ zum (nicht enthaltenen) Direktschall → Abstandseindruck
    const double totalE = erEnergy + tailEnergyTarget;
    res.wetGainDb = (float) juce::jlimit (-9.0, 6.0, 0.5 * 10.0 * std::log10 (juce::jmax (1e-6, totalE)));
    normaliseEnergy (res.ir);
    fillDisplay (res, sr);
    return res;
}

//==============================================================================
// Eigene oder gemessene IR formen
inline Result shapeCustom (const juce::AudioBuffer<float>& raw, double sr, const Settings& s)
{
    Result res;
    const int chs = raw.getNumChannels();
    if (raw.getNumSamples() < 16 || chs == 0) { res.ir.setSize (2, 1); res.ir.clear(); res.ir.setSample (0, 0, 1); res.ir.setSample (1, 0, 1); return res; }

    // gemeinsamer Anfang: erste Stelle über −40 dB vom Spitzenwert, 1 ms Vorlauf
    float peak = 0;
    for (int c = 0; c < chs; ++c) peak = juce::jmax (peak, raw.getMagnitude (c, 0, raw.getNumSamples()));
    int start = 0;
    const float thr = peak * 0.01f;
    for (int i = 0; i < raw.getNumSamples() && start == 0; ++i)
        for (int c = 0; c < chs; ++c)
            if (std::abs (raw.getSample (c, i)) > thr) { start = juce::jmax (0, i - (int) (0.001 * sr)); if (start == 0) start = 1; break; }
    start = juce::jmax (0, start - 1);
    const int n = juce::jmin (raw.getNumSamples() - start, (int) (maxIrSeconds * sr));

    res.ir.setSize (2, n);
    for (int c = 0; c < 2; ++c)
        res.ir.copyFrom (c, 0, raw, juce::jmin (c, chs - 1), start, n);

    // Nachhallzeit schätzen (Mitte)
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (res.ir.getSample (0, i) + res.ir.getSample (1, i));
    const float tOrig = juce::jmax (0.1f, t60FromCurve (schroeder (mono, sr)));

    // Länge: zusätzliche Exponentialkurve (verlängern begrenzt auf +24 dB)
    const double lenF = juce::jlimit (0.5, 2.0, (double) s.length);
    const double a = 6.9078 / tOrig * (1.0 / lenF - 1.0);   // >0 kürzen, <0 verlängern
    // Dämpfung: Höhen (über 3 kHz) klingen schneller ab
    const double aHi = 6.9078 / tOrig * 1.6 * s.damping;
    for (int c = 0; c < 2; ++c)
    {
        auto lp = LR4::make (sr, 3000, false), hp = LR4::make (sr, 3000, true);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const double x = res.ir.getSample (c, i);
            const double lo = lp.process (x), hi = hp.process (x);
            const double gLen = juce::jmin (16.0, std::exp (-a * t));
            res.ir.setSample (c, i, (float) ((lo + hi * std::exp (-aHi * t)) * gLen));
        }
        const int fade = juce::jmax (1, n / 12);
        for (int i = n - fade; i < n; ++i)
            res.ir.setSample (c, i, res.ir.getSample (c, i) * 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) (i - (n - fade)) / (float) fade)));
    }
    // Direktschall der Messung bleibt, Erstreflexionen nach ihm
    applyEarlyBalance (res.ir, sr, s.earlyDb, (int) (0.003 * sr));
    normaliseEnergy (res.ir);
    fillDisplay (res, sr);
    return res;
}

//==============================================================================
// Raum messen: exponentieller Sinus-Sweep (Farina) und Entfaltung
struct Sweep
{
    double sr = 48000, seconds = 8.0, f1 = 20.0, f2 = 20000.0;
    int length() const { return (int) (seconds * sr); }
    float sample (int i) const
    {
        const double t = i / sr, Lr = seconds / std::log (f2 / f1);
        const double fade = juce::jmin (1.0, juce::jmin (t / 0.05, (seconds - t) / 0.02));
        return (float) (std::sin (2.0 * juce::MathConstants<double>::pi * f1 * Lr * (std::exp (t / Lr) - 1.0)) * juce::jmax (0.0, fade));
    }
};

// rec: aufgenommene Kanäle (Sweep + Ausklang). Ergebnis: IR ab dem Direktschall, max. irSeconds.
inline juce::AudioBuffer<float> deconvolve (const juce::AudioBuffer<float>& rec, const Sweep& sw, double irSeconds = 6.0)
{
    const int ns = sw.length();
    const int nr = rec.getNumSamples();
    int order = 1;
    while ((1 << order) < nr + ns) ++order;
    const int N = 1 << order;
    juce::dsp::FFT fft (order);

    // inverses Filter: zeitgespiegelter Sweep mit −6 dB/Oktave
    std::vector<std::complex<float>> inv ((size_t) N), tmp ((size_t) N), X ((size_t) N);
    const double Lr = sw.seconds / std::log (sw.f2 / sw.f1);
    for (int i = 0; i < ns; ++i)
    {
        const double t = i / sw.sr;
        inv[(size_t) i] = { (float) (sw.sample (ns - 1 - i) * std::exp (-t / Lr)), 0.0f };
    }
    fft.perform (inv.data(), tmp.data(), false);
    std::swap (inv, tmp);

    // Normierung: Sweep ⊛ Invers soll einen Einheitsimpuls ergeben
    std::vector<std::complex<float>> sref ((size_t) N), S ((size_t) N);
    for (int i = 0; i < ns; ++i) sref[(size_t) i] = { sw.sample (i), 0.0f };
    fft.perform (sref.data(), S.data(), false);
    for (int i = 0; i < N; ++i) sref[(size_t) i] = S[(size_t) i] * inv[(size_t) i];
    fft.perform (sref.data(), S.data(), true);
    float refPeak = 0;
    for (int i = 0; i < N; ++i) refPeak = juce::jmax (refPeak, std::abs (S[(size_t) i].real()));
    const float norm = refPeak > 0 ? 1.0f / refPeak : 1.0f;

    std::vector<std::vector<float>> lin ((size_t) rec.getNumChannels());
    int peakIdx = 0; float peak = 0;
    for (int c = 0; c < rec.getNumChannels(); ++c)
    {
        std::fill (X.begin(), X.end(), std::complex<float> {});
        for (int i = 0; i < nr; ++i) X[(size_t) i] = { rec.getSample (c, i), 0.0f };
        fft.perform (X.data(), tmp.data(), false);
        for (int i = 0; i < N; ++i) tmp[(size_t) i] *= inv[(size_t) i];
        fft.perform (tmp.data(), X.data(), true);
        lin[(size_t) c].resize ((size_t) N);
        for (int i = 0; i < N; ++i) lin[(size_t) c][(size_t) i] = X[(size_t) i].real() * norm;
        // lineare IR liegt ab ns−1 (Verzerrungsanteile davor)
        for (int i = ns - 1; i < N; ++i)
            if (std::abs (lin[(size_t) c][(size_t) i]) > peak) { peak = std::abs (lin[(size_t) c][(size_t) i]); peakIdx = i; }
    }
    const int pre = (int) (0.002 * sw.sr);
    const int start = juce::jmax (ns - 1, peakIdx - pre);
    const int len = juce::jmin ((int) (irSeconds * sw.sr), N - start);
    juce::AudioBuffer<float> ir (juce::jmax (1, rec.getNumChannels()), len);
    for (int c = 0; c < rec.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i)
        {
            float w = 1.0f;
            if (i < pre) w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (float) i / (float) pre));
            ir.setSample (c, i, lin[(size_t) c][(size_t) (start + i)] * w);
        }
    // Rauschboden abschneiden: wo die Abklingkurve flach wird
    std::vector<float> mono ((size_t) len);
    for (int i = 0; i < len; ++i) mono[(size_t) i] = ir.getSample (0, i);
    float floorRms = 0;
    {
        const int tailS = juce::jmax (1, len / 10);
        double e = 0; for (int i = len - tailS; i < len; ++i) e += mono[(size_t) i] * mono[(size_t) i];
        floorRms = (float) std::sqrt (e / tailS);
    }
    int cut = len;
    const int win = (int) (0.02 * sw.sr);
    for (int i = 0; i + win < len; i += win)
    {
        double e = 0; for (int k = i; k < i + win; ++k) e += mono[(size_t) k] * mono[(size_t) k];
        if (i > (int) (0.1 * sw.sr) && std::sqrt (e / win) < floorRms * 2.0f) { cut = juce::jmin (len, i + win * 4); break; }
    }
    ir.setSize (ir.getNumChannels(), cut, true);
    const int fade = juce::jmax (1, cut / 10);
    for (int c = 0; c < ir.getNumChannels(); ++c)
        for (int i = cut - fade; i < cut; ++i)
            ir.setSample (c, i, ir.getSample (c, i) * (float) (cut - i) / (float) fade);
    return ir;
}
} // namespace saal
