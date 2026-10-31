// PhatFX DSP — an independent reimplementation of a classic multi-effect's
// signal chain. Algorithms and curves were derived from behavioural analysis
// of the original plug-in; this code is written from scratch.
//
// All modules are JUCE-free so they can be unit tested headless.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal" // exact compares are deliberate coefficient-cache checks
#endif

namespace phat
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

// --------------------------------------------------------------------------
// Parameter value curves (unit 0..1 -> engineering value)
// --------------------------------------------------------------------------
namespace curve
{
// Multimode filter cutoff: 80 Hz .. 16 kHz with a steep top end.
inline float cutoffHz (float v)
{
    if (v >= 1.0f) return 16000.0f;
    if (v <= 0.0f) return 80.0f;
    const float x = -0.25f * std::log10 (1.0f - v);
    if (x > 1.0f) return 16000.0f;
    return 80.0f + 15920.0f * std::max (0.0f, x);
}

// Bandpass section edge frequency, 20 Hz .. 20 kHz, logarithmic.
inline float bandHz (float v) { return 20.0f * std::pow (1000.0f, clampf (v, 0.0f, 1.0f)); }

// Master volume taper (0 dB at full scale).
inline float masterGain (float v) { return (float) std::pow ((double) std::max (0.0f, v), 2.737); }

// Compressor amount -> threshold in dB (display only).
inline float compressDb (float v) { return 20.0f * std::log10 (std::max (1.0e-6f, 1.0f - v)); }

inline int bitDepth (float v) { return (int) ((1.0f - clampf (v, 0.0f, 1.0f)) * 15.0f + 1.0f); }

inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
} // namespace curve

// --------------------------------------------------------------------------
// Basic building blocks
// --------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0; }

    void copyCoeffs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    inline float process (float x)
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = (std::fabs (y) < 1.0e-20f) ? 0.0f : y;
        return y1;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    static double w0 (double sr, double f) { return 2.0 * 3.141592653589793 * std::min (f, sr * 0.49) / sr; }

    void lowPass (double sr, double f, double q)
    {
        const double w = w0 (sr, f), c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void highPass (double sr, double f, double q)
    {
        const double w = w0 (sr, f), c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void bandPass (double sr, double f, double q) // 0 dB peak gain
    {
        const double w = w0 (sr, f), c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void notch (double sr, double f, double q)
    {
        const double w = w0 (sr, f), c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set (1, -2 * c, 1, 1 + a, -2 * c, 1 - a);
    }
    void peaking (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = w0 (sr, f), c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
};

// Linear-slope envelope follower (attack/release given in seconds).
struct EnvelopeFollower
{
    float attack = 0.01f, release = 0.1f, sr = 44100.0f;
    float up = 0, down = 0, env = 0;

    void setTimes (float a, float r) { attack = a; release = r; update(); }
    void setSampleRate (float s) { sr = s; update(); }
    void update()
    {
        up = attack > 0 ? 1.0f / (attack * sr) : 1.0e9f;
        down = release > 0 ? 1.0f / (release * sr) : 1.0e9f;
    }
    void reset() { env = 0; }

    inline float process (float x)
    {
        x = std::fabs (x);
        if (x > env) env = std::min (x, env + up);
        else env = std::max (x, env - down);
        return env;
    }
};

// Soft knee used by the tube stage, the compressor and the filter output.
// Linear below `knee`, rounds off to reach `ceiling` at |x| == range.
inline float softKnee (float x, float knee, float range, float ceiling)
{
    const float ax = std::fabs (x);
    if (ax <= knee) return x;
    float t = (ax - knee) / std::max (1.0e-6f, range - knee);
    const float u = (1.0f - t) * 0.70710678f;
    const float y = knee + (ceiling - knee) * 2.0f * (0.5f - u * u);
    return x < 0 ? -y : y;
}

// --------------------------------------------------------------------------
// Distortion block: Tube / Mech / Bit reduction
// --------------------------------------------------------------------------
struct Distortion
{
    bool legacyBitCrush = false;

    // Tube: gain 1..5 into a rounded knee at 0.6, ceiling 1.0.
    static inline float tube (float x, float amt)
    {
        const float g = 1.0f + 4.0f * amt;
        return softKnee (x * g, 0.6f, g, 1.0f);
    }

    // Mech: gain 1..11 into a linear compression knee at 0.75.
    static inline float mech (float x, float amt)
    {
        const float g = 1.0f + 10.0f * amt;
        const float y = x * g, ay = std::fabs (y);
        if (ay <= 0.75f) return y;
        const float r = 0.75f + (ay - 0.75f) * (0.25f / (g - 0.75f));
        return y < 0 ? -r : r;
    }

    inline float bitReduce (float x, float amt) const
    {
        const int bits = curve::bitDepth (amt);
        const float q = (float) (1 << bits);
        const float v = q * x + 0.5f;
        const float r = legacyBitCrush ? (float) (int) v : std::floor (v);
        return r / q;
    }
};

// --------------------------------------------------------------------------
// Exciter ("Xcita"): dynamic high-band enhancer
// --------------------------------------------------------------------------
struct Exciter
{
    Biquad hp[2];
    EnvelopeFollower env;
    float sr = 44100;

    void prepare (float s)
    {
        sr = s;
        env.setTimes (0.01f, 0.1f);
        env.setSampleRate (s);
        for (auto& h : hp) { h.highPass (s, 7000.0, 1.0); h.reset(); }
        env.reset();
    }

    void process (float* l, float* r, int n, float amt)
    {
        for (int i = 0; i < n; ++i)
        {
            const float hl = hp[0].process (l[i]);
            const float hr = hp[1].process (r[i]);
            const float e = env.process ((hl + hr) * 300.0f * 0.5f);
            const float g = std::min (10.0f * e, 3.0f);
            const float wl = clampf (l[i] + g * hl, -0.999f, 0.999f);
            const float wr = clampf (r[i] + g * hr, -0.999f, 0.999f);
            l[i] = amt * wl + (1.0f - amt) * l[i];
            r[i] = amt * wr + (1.0f - amt) * r[i];
        }
    }
};

// --------------------------------------------------------------------------
// Compressor: makeup-first soft-knee peak compressor with linear release
// --------------------------------------------------------------------------
struct Compressor
{
    float sr = 44100, amount = 0, gain = 1, slope = 0;
    EnvelopeFollower env; // used by "Smooth" mode

    void prepare (float s)
    {
        sr = s;
        env.setTimes (0.03f, 0.08f);
        env.setSampleRate (s);
        reset();
    }
    void reset() { gain = 1; slope = 0; env.reset(); }

    // amt: 0..0.99, releaseMs: 100..2000, smooth: use envelope instead of raw peak
    void process (float* l, float* r, int n, float amt, float releaseMs, bool smooth)
    {
        const float relSamples = releaseMs * 0.001f * sr;
        const float dAmt = (amt - amount) / (float) std::max (1, n);
        for (int i = 0; i < n; ++i)
        {
            const float makeup = 1.0f / (1.0f - amount);
            float peak = std::max (std::fabs (l[i]), std::fabs (r[i]));
            if (smooth) peak = env.process (peak);
            float ol = l[i] * makeup, or_ = r[i] * makeup;
            const float p = peak * makeup;
            if (p > 0.31f)
            {
                const float t = (p - 0.31f) / std::max (1.0e-6f, makeup - 0.31f);
                const float u = (1.0f - t) * 0.70710678f;
                const float target = (0.31f + 1.38f * (0.5f - u * u)) / p;
                if (target < gain)
                {
                    gain = target;
                    slope = (1.0f - gain) / std::max (1.0f, relSamples);
                }
            }
            if (gain < 1.0f)
            {
                ol *= gain; or_ *= gain;
                gain = std::min (1.0f, gain + slope);
            }
            l[i] = ol; r[i] = or_;
            amount += dAmt;
        }
        amount = amt;
    }
};

// --------------------------------------------------------------------------
// MagicEQ: tuned low-end peak + 40 Hz high-pass with level protection
// --------------------------------------------------------------------------
struct MagicEQ
{
    Biquad peak[2], hp[2];
    EnvelopeFollower env;
    float sr = 44100, tune = -1, gain = 1, slope = 0;

    void prepare (float s)
    {
        sr = s;
        env.setTimes (0.0075f, 0.05f);
        env.setSampleRate (s);
        for (auto& h : hp) { h.highPass (s, 40.0, 0.5); h.reset(); }
        for (auto& p : peak) p.reset();
        tune = -1; gain = 1; slope = 0;
        env.reset();
    }

    // clipMode: true = hard clip at +/-1, false = slow limiter
    void process (float* l, float* r, int n, float amt, float tuneHz, bool clipMode)
    {
        if (tuneHz != tune)
        {
            tune = tuneHz;
            peak[0].peaking (sr, tuneHz, 1.0, 10.0);
            peak[1].copyCoeffs (peak[0]);
        }
        for (int i = 0; i < n; ++i)
        {
            float yl = hp[0].process (peak[0].process (l[i]));
            float yr = hp[1].process (peak[1].process (r[i]));
            if (clipMode)
            {
                yl = clampf (yl, -1.0f, 1.0f);
                yr = clampf (yr, -1.0f, 1.0f);
            }
            else
            {
                const float e = env.process (std::max (std::fabs (yl), std::fabs (yr)));
                if (e > 1.0f)
                {
                    const float target = 1.0f / e;
                    if (target < gain)
                    {
                        gain = target;
                        slope = (1.0f - gain) / (7.5f * sr);
                    }
                }
                if (gain < 1.0f)
                {
                    yl *= gain; yr *= gain;
                    gain = std::min (1.0f, gain + slope);
                }
            }
            l[i] = amt * yl + (1.0f - amt) * l[i];
            r[i] = amt * yr + (1.0f - amt) * r[i];
        }
    }
};

// --------------------------------------------------------------------------
// Flanger: mono-fed modulated delay, quadrature L/R modulation
// --------------------------------------------------------------------------
struct Flanger
{
    std::vector<float> buf;
    int writePos = 0;
    float sr = 44100, phase = 0;
    static constexpr float baseDelay = 0.0012f, modDepth = 0.0043f, feedback = 0.0f;

    void prepare (float s)
    {
        sr = s;
        buf.assign ((size_t) ((baseDelay + modDepth) * s) + 8, 0.0f);
        writePos = 0;
        phase = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    inline float read (float delaySamples) const
    {
        const int size = (int) buf.size();
        float pos = (float) writePos - delaySamples;
        while (pos < 0) pos += (float) size;
        const int i0 = (int) pos;
        const float frac = pos - (float) i0;
        const int i1 = (i0 + 1) % size;
        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    void process (float* l, float* r, int n, float amount, float rateHz)
    {
        const float mix = amount * 0.5f;
        const float inc = rateHz / sr;
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float ml = 0.5f + 0.5f * std::sin (kTwoPi * phase);
            const float mr = 0.5f + 0.5f * std::sin (kTwoPi * (phase + 0.25f));
            const float dl = read ((baseDelay + ml * modDepth) * sr);
            const float dr = read ((baseDelay + mr * modDepth) * sr);
            float w = 0.5f * (l[i] + r[i]) + feedback * 0.5f * (dl + dr);
            if (std::fabs (w) < 1.0e-20f) w = 0;
            buf[(size_t) writePos] = w;
            writePos = (writePos + 1) % (int) buf.size();
            l[i] = (1.0f - mix) * l[i] + mix * dl;
            r[i] = (1.0f - mix) * r[i] + mix * dr;
        }
    }
};

// --------------------------------------------------------------------------
// Bandpass section: 24 dB/oct HP + 24 dB/oct LP, band-reject remainder kept
// --------------------------------------------------------------------------
struct BandPass
{
    Biquad hp[2][2], lp[2][2];
    float sr = 44100, lo = -1, hi = -1, qlo = -1, qhi = -1;
    std::vector<float> rejL, rejR;

    void prepare (float s, int maxBlock)
    {
        sr = s;
        lo = hi = qlo = qhi = -1;
        rejL.assign ((size_t) maxBlock, 0.0f);
        rejR.assign ((size_t) maxBlock, 0.0f);
        for (auto& c : hp) for (auto& b : c) b.reset();
        for (auto& c : lp) for (auto& b : c) b.reset();
    }

    void setup (float fLo, float fHi, float qLo, float qHi)
    {
        if (fLo == lo && fHi == hi && qLo == qlo && qHi == qhi) return;
        lo = fLo; hi = fHi; qlo = qLo; qhi = qHi;
        Biquad h, p;
        h.highPass (sr, fLo, (1.0 - qLo) + 0.6);
        p.lowPass (sr, fHi, 1.5 * qHi + 0.25);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 2; ++s) { hp[c][s].copyCoeffs (h); lp[c][s].copyCoeffs (p); }
    }

    void process (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r[i];
            const float yl = lp[0][1].process (lp[0][0].process (hp[0][1].process (hp[0][0].process (xl))));
            const float yr = lp[1][1].process (lp[1][0].process (hp[1][1].process (hp[1][0].process (xr))));
            rejL[(size_t) i] = xl - yl;
            rejR[(size_t) i] = xr - yr;
            l[i] = yl; r[i] = yr;
        }
    }
};

// --------------------------------------------------------------------------
// Multimode filter: 33 types
// --------------------------------------------------------------------------
enum FilterType
{
    LP2_BQ, LP2_FAT, BP2_BQ, BP2_FAT, HP2_BQ, HP2_FAT, FORMANT, PEAKING, NOTCH,
    COMB_P, COMB_M, RINGMOD, TUBE, MECH, BITRED,
    LP2_SVF, LP2_K20, LP2_XP, LP2_MG, LP4_XP, LP4_MG,
    BP2_SVF, BP2_K20, BP2_XP, BP2_MG, BP4_XP, BP4_MG,
    HP2_SVF, HP2_K20, HP2_XP, HP2_MG, HP4_XP, HP4_MG,
    NUM_FILTER_TYPES
};

inline const char* filterTypeName (int t)
{
    static const char* names[] = {
        "LP2 BQ", "LP2 Fat", "BP2 BQ", "BP2 Fat", "HP2 BQ", "HP2 Fat", "Formant", "Peaking", "Notch",
        "CombP", "CombM", "RingMod", "Tube", "Mech", "Bit Red",
        "LP2 SVF HQ", "LP2 K20 HQ", "LP2 XP HQ", "LP2 MG HQ", "LP4 XP HQ", "LP4 MG HQ",
        "BP2 SVF HQ", "BP2 K20 HQ", "BP2 XP HQ", "BP2 MG HQ", "BP4 XP HQ", "BP4 MG HQ",
        "HP2 SVF HQ", "HP2 K20 HQ", "HP2 XP HQ", "HP2 MG HQ", "HP4 XP HQ", "HP4 MG HQ" };
    return (t >= 0 && t < NUM_FILTER_TYPES) ? names[t] : "?";
}

inline float fastTanh (float x)
{
    x = clampf (x, -4.5f, 4.5f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Diode-style clipper used in the K20 resonance path.
inline float diode (float x) { return x / (1.0f + std::fabs (x)); }

// One channel of the analog-modelled filters.
struct AnalogChannel
{
    // SVF / K20 state
    float ic1 = 0, ic2 = 0;
    // ladder state
    float s[4] = { 0, 0, 0, 0 };

    void reset()
    {
        ic1 = ic2 = 0;
        for (auto& v : s) v = 0;
    }
};

struct MultiModeFilter
{
    float sr = 44100;
    int type = LP2_BQ;
    float cutoff = 1000, res = 0;
    float lastCut = -1, lastRes = -1;
    int lastType = -1;

    Biquad bq[2], formant[2][3];
    float formantGain[3] = { 1, 1, 1 };
    AnalogChannel an[2];

    // comb
    std::vector<float> comb[2];
    int combPos = 0;
    // ring mod
    float ringPhase = 0;
    // bit reduction sample-and-hold
    float holdVal[2] = { 0, 0 };
    int holdCount = 0;

    // analog coefficients
    float g = 0, k = 0, a1 = 0, a2 = 0, a3 = 0; // SVF
    float ladderG = 0, ladderK = 0;

    void prepare (float s)
    {
        sr = s;
        const int maxDelay = (int) (s / 20.0f) + 4;
        for (auto& c : comb) c.assign ((size_t) maxDelay, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : bq) b.reset();
        for (auto& c : formant) for (auto& b : c) b.reset();
        for (auto& a : an) a.reset();
        for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
        combPos = 0; ringPhase = 0; holdCount = 0;
        holdVal[0] = holdVal[1] = 0;
        lastCut = lastRes = -1; lastType = -1;
    }

    static float resToQ (float r) { return 0.70710678f * std::pow (20.0f, clampf (r, 0.0f, 1.0f)); }

    void setFormant (float cutNorm, float r)
    {
        // Vowel table (A, E, I, O, U) F1..F3 in Hz; morph by cutoff.
        static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                       { 450, 800, 2830 }, { 325, 700, 2530 } };
        static const float G[3] = { 1.0f, 0.63f, 0.25f };
        const float pos = clampf (cutNorm, 0.0f, 1.0f) * 4.0f;
        const int i0 = std::min (3, (int) pos);
        const float t = pos - (float) i0;
        const double q = 5.0 + 20.0 * r;
        for (int f = 0; f < 3; ++f)
        {
            const float hz = curve::lerp (F[i0][f], F[i0 + 1][f], t);
            formant[0][f].bandPass (sr, hz, q);
            formant[1][f].copyCoeffs (formant[0][f]);
            formantGain[f] = G[f];
        }
    }

    void update (int newType, float cutHz, float cutNorm, float r)
    {
        if (newType != lastType)
        {
            const int prev = lastType;
            lastType = newType;
            if (prev >= 0) { for (auto& a : an) a.reset(); for (auto& b : bq) b.reset(); }
            lastCut = -1;
        }
        type = newType;
        if (cutHz == lastCut && r == lastRes) return;
        lastCut = cutHz; lastRes = r;
        cutoff = cutHz; res = r;

        switch (type)
        {
            case LP2_BQ: case LP2_FAT: bq[0].lowPass (sr, cutHz, resToQ (r)); break;
            case BP2_BQ: case BP2_FAT: bq[0].bandPass (sr, cutHz, resToQ (r) * 1.5f); break;
            case HP2_BQ: case HP2_FAT: bq[0].highPass (sr, cutHz, resToQ (r)); break;
            case PEAKING: bq[0].peaking (sr, cutHz, 2.0, 24.0 * r); break;
            case NOTCH: bq[0].notch (sr, cutHz, 0.3 + 6.0 * r); break;
            case FORMANT: setFormant (cutNorm, r); break;
            default: break;
        }
        bq[1].copyCoeffs (bq[0]);

        // Zero-delay-feedback (TPT) analog models, prewarped at the host rate.
        const float fc = std::min (cutHz, sr * 0.45f);
        g = std::tan (kPi * fc / sr);
        k = 2.0f - 1.98f * r;               // SVF damping
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        ladderG = g / (1.0f + g);
        ladderK = 4.2f * r;                  // ladder resonance (self-oscillates near 1)
    }

    // --- analog cores, one sample at 2x rate -------------------------------
    inline float svf (AnalogChannel& c, float x, int mode, bool k20)
    {
        const float v3 = x - c.ic2;
        const float v1 = a1 * c.ic1 + a2 * v3;
        const float v2 = c.ic2 + a2 * c.ic1 + a3 * v3;
        c.ic1 = 2.0f * v1 - c.ic1;
        c.ic2 = 2.0f * v2 - c.ic2;
        if (k20)
        {
            // MS-20 flavour: diode clipping in the resonant (band) path and a
            // soft limit on the low-pass state so high resonance screams but stays bounded.
            c.ic1 = 1.4f * diode (c.ic1 * (0.7f + res));
            c.ic2 = 1.5f * fastTanh (c.ic2 * (1.0f / 1.5f));
        }
        if (mode == 0) return v2;
        if (mode == 1) return v1;
        return x - k * v1 - v2;
    }

    inline float ladder (AnalogChannel& c, float x, int mode, bool ota)
    {
        const float fb = c.s[3];
        const float u = fastTanh (x - ladderK * fb);
        float in = u;
        float y[4];
        for (int i = 0; i < 4; ++i)
        {
            // OTA (XP) saturates every stage input; Moog (MG) saturates only at
            // the feedback summing point (above), with linear stages.
            const float drive = ota ? fastTanh (in) : in;
            const float v = ladderG * (drive - c.s[i]);
            y[i] = v + c.s[i];
            c.s[i] = y[i] + v;
            in = y[i];
        }
        const float A = u;
        switch (mode)
        {
            case 0: return y[1] * (1.0f + 0.5f * ladderK);                          // LP2
            case 1: return y[3] * (1.0f + 0.5f * ladderK);                          // LP4
            case 2: return (2.0f * y[0] - 2.0f * y[1]) * (1.0f + 0.5f * ladderK);         // BP2
            case 3: return (4.0f * y[1] - 8.0f * y[2] + 4.0f * y[3]) * (1.0f + 0.5f * ladderK); // BP4
            case 4: return A - 2.0f * y[0] + y[1];                                  // HP2
            default: return A - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3];     // HP4
        }
    }

    inline float processSample (int ch, float x)
    {
        auto& c = an[ch];
        switch (type)
        {
            case LP2_BQ: case BP2_BQ: case HP2_BQ: case PEAKING: case NOTCH:
                return bq[ch].process (x);
            case LP2_FAT: case BP2_FAT: case HP2_FAT:
            {
                const float drive = 1.0f + 2.0f * res;
                return fastTanh (bq[ch].process (fastTanh (x * drive)) * 1.2f) / std::sqrt (drive);
            }
            case FORMANT:
            {
                float y = 0;
                for (int f = 0; f < 3; ++f) y += formantGain[f] * formant[ch][f].process (x);
                return y * 5.0f;
            }
            case COMB_P: case COMB_M:
            {
                auto& buf = comb[ch];
                const int size = (int) buf.size();
                const float delay = clampf (sr / cutoff, 1.0f, (float) (size - 2));
                float pos = (float) combPos - delay;
                while (pos < 0) pos += (float) size;
                const int i0 = (int) pos;
                const float fr = pos - (float) i0;
                const float d = buf[(size_t) i0] + fr * (buf[(size_t) ((i0 + 1) % size)] - buf[(size_t) i0]);
                const float fb = (type == COMB_P ? 1.0f : -1.0f) * 0.97f * res;
                float y = x + fb * d;
                if (std::fabs (y) < 1.0e-20f) y = 0;
                buf[(size_t) combPos] = clampf (y, -4.0f, 4.0f);
                return y * (1.0f - 0.5f * res);
            }
            case RINGMOD:
            {
                const float m = std::sin (kTwoPi * ringPhase);
                return x * curve::lerp (1.0f, m, 0.5f + 0.5f * res);
            }
            case TUBE: return Distortion::tube (x, std::min (1.0f, cutoff / 16000.0f + res));
            case MECH: return Distortion::mech (x, std::min (1.0f, cutoff / 16000.0f + res));
            case BITRED:
            {
                // cutoff -> bit depth, resonance -> sample-rate reduction
                const int bits = std::max (1, (int) (1.0f + 15.0f * std::sqrt (cutoff / 16000.0f)));
                const float q = (float) (1 << bits);
                const int hold = 1 + (int) (res * res * 31.0f);
                if (holdCount % hold == 0) holdVal[ch] = std::floor (q * x + 0.5f) / q;
                return holdVal[ch];
            }
            case LP2_SVF: return svf (c, x, 0, false);
            case BP2_SVF: return svf (c, x, 1, false);
            case HP2_SVF: return svf (c, x, 2, false);
            case LP2_K20: return svf (c, x, 0, true);
            case BP2_K20: return svf (c, x, 1, true);
            case HP2_K20: return svf (c, x, 2, true);
            case LP2_XP: return ladder (c, x, 0, true);
            case LP4_XP: return ladder (c, x, 1, true);
            case BP2_XP: return ladder (c, x, 2, true);
            case BP4_XP: return ladder (c, x, 3, true);
            case HP2_XP: return ladder (c, x, 4, true);
            case HP4_XP: return ladder (c, x, 5, true);
            case LP2_MG: return ladder (c, x, 0, false);
            case LP4_MG: return ladder (c, x, 1, false);
            case BP2_MG: return ladder (c, x, 2, false);
            case BP4_MG: return ladder (c, x, 3, false);
            case HP2_MG: return ladder (c, x, 4, false);
            case HP4_MG: return ladder (c, x, 5, false);
            default: return x;
        }
    }

    void advance()
    {
        if (type == COMB_P || type == COMB_M) combPos = (combPos + 1) % (int) comb[0].size();
        if (type == RINGMOD) { ringPhase += cutoff / sr; if (ringPhase >= 1) ringPhase -= 1; }
        if (type == BITRED) ++holdCount;
    }
};

// Wrapper with the envelope follower, wet/dry mix and output soft knee.
struct FilterSection
{
    MultiModeFilter filter;
    EnvelopeFollower env;
    float sr = 44100;

    void prepare (float s)
    {
        sr = s;
        filter.prepare (s);
        env.setSampleRate (s);
        env.reset();
    }

    // cutoffParam/envAmt/res/mix are unit values; attack/decay in seconds.
    void process (float* l, float* r, int n, int type, float cutoffParam, float envAmt, float res,
                  float mix, float attack, float decay)
    {
        if (env.attack != attack || env.release != decay) env.setTimes (attack, decay);

        // The envelope is tracked per sample, the filter updated every 16 samples.
        constexpr int sub = 16;
        for (int start = 0; start < n; start += sub)
        {
            const int m = std::min (sub, n - start);
            float e = env.env;
            for (int i = 0; i < m; ++i)
                e = env.process ((std::fabs (l[start + i]) + std::fabs (r[start + i])) * 0.5f);
            const float envNorm = std::min (1.0f, e * 2.0f);
            const float x = envAmt * envNorm * cutoffParam + (1.0f - envAmt) * cutoffParam;
            filter.update (type, curve::cutoffHz (x), x, res);

            for (int i = start; i < start + m; ++i)
            {
                const float dl = l[i], dr = r[i];
                float wl = filter.processSample (0, dl);
                float wr = filter.processSample (1, dr);
                filter.advance();
                wl = softKnee (clampf (wl, -1.0f, 1.0f), 0.7f, 1.0f, 1.0f);
                wr = softKnee (clampf (wr, -1.0f, 1.0f), 0.7f, 1.0f, 1.0f);
                l[i] = mix * wl + (1.0f - mix) * dl;
                r[i] = mix * wr + (1.0f - mix) * dr;
            }
        }
    }
};

// --------------------------------------------------------------------------
// LFO
// --------------------------------------------------------------------------
enum LfoShape { SINE, TRIANGLE, RAMP_UP, RAMP_DOWN, SQUARE, RANDOM_SQUARE, RANDOM_TRIANGLE,
                INV_SINE, INV_TRIANGLE, INV_SQUARE, NUM_LFO_SHAPES };

inline const char* lfoShapeName (int s)
{
    static const char* n[] = { "Sine", "Triangle", "Ramp Up", "Ramp Down", "Square", "Random Square",
                               "Random Triangle", "Inverted Sine", "Inverted Triangle", "Inverted Square" };
    return (s >= 0 && s < NUM_LFO_SHAPES) ? n[s] : "?";
}

// Tempo-synced LFO periods in bars, fastest first.
constexpr int kNumSyncRates = 24;
inline float syncRateBars (int i)
{
    static const float bars[kNumSyncRates] = { 1.f / 48, 1.f / 32, 3.f / 64, 1.f / 24, 1.f / 16, 3.f / 32,
                                               1.f / 12, 1.f / 8, 3.f / 16, 1.f / 6, 1.f / 4, 3.f / 8,
                                               1.f / 3, 1.f / 2, 3.f / 4, 2.f / 3, 1.f, 1.5f,
                                               4.f / 3, 2.f, 3.f, 8.f / 3, 4.f, 6.f };
    return bars[std::max (0, std::min (kNumSyncRates - 1, i))];
}
inline const char* syncRateName (int i)
{
    static const char* n[kNumSyncRates] = { "1/32T", "1/32", "1/32*", "1/16T", "1/16", "1/16*", "1/8T", "1/8",
                                            "1/8*", "1/4T", "1/4", "1/4*", "1/2T", "1/2", "1/2*", "1/1T",
                                            "1/1", "1/1*", "2/1T", "2/1", "2/1*", "4/1T", "4/1", "4/1*" };
    return n[std::max (0, std::min (kNumSyncRates - 1, i))];
}
// Rate knob (unit) -> sync index: knob up = faster.
inline int syncIndex (float v) { return std::min (kNumSyncRates - 1, (int) ((1.0f - v) * kNumSyncRates)); }
// Free-running rate: 0.02 .. 8 Hz, exponential.
inline float freeRateHz (float v) { return 0.02f * std::pow (400.0f, clampf (v, 0.0f, 1.0f)); }

struct Lfo
{
    double phase = 0;
    float randA = 0, randB = 0;
    uint32_t seed = 0x1234567u;
    int lastCycle = -1;

    float nextRandom()
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) ((seed >> 8) & 0xFFFF) / 32767.5f - 1.0f;
    }

    void reset() { phase = 0; lastCycle = -1; }

    // Advance by `seconds` at `hz`, or set an absolute phase (from host).
    void advance (double hz, double seconds) { setPhase (phase + hz * seconds); }

    void setPhase (double p)
    {
        const int cycle = (int) std::floor (p);
        if (cycle != lastCycle)
        {
            randA = randB;
            randB = nextRandom();
            lastCycle = cycle;
        }
        phase = p;
    }

    // Bipolar value in -1..1
    float value (int shape) const
    {
        const float p = (float) (phase - std::floor (phase));
        switch (shape)
        {
            case SINE: return std::sin (kTwoPi * p);
            case TRIANGLE: return 1.0f - 4.0f * std::fabs (p - 0.5f) ;
            case RAMP_UP: return 2.0f * p - 1.0f;
            case RAMP_DOWN: return 1.0f - 2.0f * p;
            case SQUARE: return p < 0.5f ? 1.0f : -1.0f;
            case RANDOM_SQUARE: return randB;
            case RANDOM_TRIANGLE: return curve::lerp (randA, randB, p);
            case INV_SINE: return -std::sin (kTwoPi * p);
            case INV_TRIANGLE: return -(1.0f - 4.0f * std::fabs (p - 0.5f));
            case INV_SQUARE: return p < 0.5f ? -1.0f : 1.0f;
            default: return 0;
        }
    }
};

} // namespace phat

#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif
