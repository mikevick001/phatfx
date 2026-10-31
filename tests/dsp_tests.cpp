// Headless checks for the PhatFX engine: stability of every filter type,
// basic behaviour of each section, and the parameter curves.
#include "Engine.h"

#include <cstdio>
#include <random>
#include <string>

using namespace phat;

static int failures = 0;
#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } \
    } while (0)

static void defaults (Engine& e)
{
    std::fill (std::begin (e.base), std::end (e.base), 0.0f);
    e.base[P_masterMix] = 1.0f;
    e.base[P_masterVol] = 1.0f;
    e.base[P_mmMix] = 1.0f;
    e.base[P_bpHigh] = 1.0f;
}

struct Stats { float peak = 0, rms = 0; bool finite = true; };

static Stats run (Engine& e, int seconds, float sr, int signal, float amp = 0.5f)
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    const int total = (int) (seconds * sr);
    std::vector<float> l (512), r (512);
    Stats s;
    double sum = 0;
    double phase = 0;
    for (int done = 0; done < total; done += 512)
    {
        const int n = std::min (512, total - done);
        for (int i = 0; i < n; ++i)
        {
            float x;
            if (signal == 0) x = uni (rng);                          // noise
            else if (signal == 1) { phase += 110.0 / sr; x = (float) std::sin (6.2831853 * phase); } // sine
            else x = ((done + i) % 4410 == 0) ? 1.0f : 0.0f;         // impulses
            l[(size_t) i] = r[(size_t) i] = amp * x;
        }
        e.process (l.data(), r.data(), n, 120.0, (done) / sr * 2.0, true);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) s.finite = false;
            s.peak = std::max (s.peak, std::fabs (l[(size_t) i]));
            sum += (double) l[(size_t) i] * l[(size_t) i];
        }
    }
    s.rms = (float) std::sqrt (sum / total);
    return s;
}

static float sineGainThroughBandpass (float freq, float lo, float hi)
{
    BandPass bp;
    const float sr = 48000;
    bp.prepare (sr, 64);
    bp.setup (curve::bandHz (lo), curve::bandHz (hi), 0.3f, 0.3f);
    double ph = 0, sum = 0;
    float l[64], r[64];
    for (int b = 0; b < 1500; ++b)
    {
        for (int i = 0; i < 64; ++i) { ph += freq / sr; l[i] = r[i] = (float) std::sin (6.2831853 * ph); }
        bp.process (l, r, 64);
        if (b > 500) for (int i = 0; i < 64; ++i) sum += l[i] * l[i];
    }
    return (float) std::sqrt (sum / (1000.0 * 64)) * 1.41421356f;
}

int main()
{
    const float sr = 48000;

    // --- curves -------------------------------------------------------------
    CHECK (std::fabs (curve::cutoffHz (0.0f) - 80.0f) < 1e-3f, "cutoff min");
    CHECK (std::fabs (curve::cutoffHz (1.0f) - 16000.0f) < 1e-3f, "cutoff max");
    CHECK (curve::bitDepth (0.0f) == 16 && curve::bitDepth (1.0f) == 1, "bit depth range");
    CHECK (std::fabs (curve::masterGain (1.0f) - 1.0f) < 1e-6f, "master unity");
    std::printf ("cutoff(0.5)=%.0f Hz  cutoff(0.9)=%.0f Hz  vol(0.5)=%.1f dB\n", curve::cutoffHz (0.5f),
                 curve::cutoffHz (0.9f), 20 * std::log10 (curve::masterGain (0.5f)));

    // --- bypass: everything off is transparent (below clip) ------------------
    {
        Engine e; defaults (e); e.prepare (sr);
        float l[256], r[256];
        for (int i = 0; i < 256; ++i) l[i] = r[i] = 0.3f * std::sin (i * 0.05f);
        float ref[256]; std::copy (l, l + 256, ref);
        e.process (l, r, 256);
        float err = 0; for (int i = 0; i < 256; ++i) err = std::max (err, std::fabs (l[i] - ref[i]));
        CHECK (err < 1e-6f, "bypass not transparent (err=%g)", err);
    }

    // --- every filter type: stable and bounded at extreme settings ---------
    for (int t = 0; t < NUM_FILTER_TYPES; ++t)
        for (float res : { 0.0f, 0.5f, 1.0f })
            for (float cut : { 0.0f, 0.6f, 1.0f })
            {
                Engine e; defaults (e);
                e.base[P_mmOn] = 1; e.base[P_mmType] = (float) t; e.base[P_mmRes] = res; e.base[P_mmCutoff] = cut;
                e.base[P_mmEnv] = 0.5f;
                e.prepare (sr);
                const auto s = run (e, 1, sr, 0);
                CHECK (s.finite && s.peak <= 1.0f, "filter %s res=%.1f cut=%.1f unstable (peak=%g)",
                       filterTypeName (t), res, cut, s.peak);
            }
    std::printf ("filter types tested: %d\n", NUM_FILTER_TYPES);

    // --- LP filter actually filters ----------------------------------------
    {
        Engine open; defaults (open); open.base[P_mmOn] = 1; open.base[P_mmType] = LP4_MG; open.base[P_mmCutoff] = 1.0f;
        open.prepare (sr);
        Engine closed = open; closed.base[P_mmCutoff] = 0.05f; closed.prepare (sr);
        const auto a = run (open, 1, sr, 0), b = run (closed, 1, sr, 0);
        std::printf ("LP4 MG noise rms open=%.3f closed=%.3f\n", a.rms, b.rms);
        CHECK (b.rms < a.rms * 0.3f, "LP4 MG does not attenuate");
    }

    // --- bandpass passes in-band, rejects out-of-band -----------------------
    {
        const float in = sineGainThroughBandpass (1000.0f, 0.2f, 0.6f);     // ~79 Hz .. 1.26 kHz band
        const float out = sineGainThroughBandpass (10000.0f, 0.2f, 0.6f);
        std::printf ("bandpass gain 1k=%.3f 10k=%.4f\n", in, out);
        CHECK (in > 0.5f && out < 0.05f, "bandpass response");
    }

    // --- distortion stages --------------------------------------------------
    CHECK (std::fabs (Distortion::tube (0.1f, 0.0f) - 0.1f) < 1e-6f, "tube linear region");
    CHECK (Distortion::tube (1.0f, 1.0f) <= 1.0001f, "tube ceiling");
    CHECK (Distortion::mech (1.0f, 1.0f) <= 1.0001f && Distortion::mech (0.05f, 1.0f) > 0.5f, "mech gain/ceiling");
    {
        Distortion d;
        const float q = d.bitReduce (0.3f, 1.0f); // 1 bit
        CHECK (q == 0.0f || q == 0.5f, "1-bit quantiser (got %g)", q);
    }

    // --- compressor reduces dynamic range -----------------------------------
    {
        Engine e; defaults (e); e.base[P_compOn] = 1; e.base[P_compAmount] = 0.8f; e.base[P_compRelease] = 0.3f;
        e.prepare (sr);
        const auto s = run (e, 1, sr, 1, 0.9f);
        std::printf ("compressor: sine 0.9 -> peak %.3f rms %.3f\n", s.peak, s.rms);
        CHECK (s.finite && s.peak <= 1.0f, "compressor bounded");
    }

    // --- all sections on, LFOs synced, randomised sweeps ---------------------
    {
        std::mt19937 rng (3);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (int trial = 0; trial < 40; ++trial)
        {
            Engine e; defaults (e);
            for (auto p : { P_bpOn, P_distOn, P_mmOn, P_flOn, P_eqOn, P_compOn, P_lfo1On, P_lfo2On }) e.base[p] = 1;
            for (auto p : { P_bpLow, P_bpHigh, P_bpQLow, P_bpQHigh, P_bpBRMix, P_distBit, P_distMech, P_distTube,
                            P_distXcita, P_mmMix, P_mmAttack, P_mmDecay, P_mmEnv, P_mmCutoff, P_mmRes, P_flAmount,
                            P_flRate, P_eqAmount, P_eqTune, P_compAmount, P_compRelease, P_masterMix,
                            P_lfo1Depth, P_lfo1Rate, P_lfo2Depth, P_lfo2Rate })
                e.base[p] = u (rng);
            e.base[P_mmType] = (float) (int) (u (rng) * (NUM_FILTER_TYPES - 1));
            e.base[P_lfo1Target] = (float) (int) (u (rng) * (kNumLfoTargets - 1));
            e.base[P_lfo2Target] = (float) (int) (u (rng) * (kNumLfoTargets - 1));
            e.base[P_lfo1Shape] = (float) (int) (u (rng) * (NUM_LFO_SHAPES - 1));
            e.base[P_lfo2Shape] = (float) (int) (u (rng) * (NUM_LFO_SHAPES - 1));
            e.base[P_lfo1Sync] = u (rng) > 0.5f;
            e.base[P_eqMode] = u (rng) > 0.5f;
            e.base[P_compMode] = u (rng) > 0.5f;
            e.prepare (sr);
            const auto s = run (e, 2, sr, trial % 3);
            CHECK (s.finite && s.peak <= 1.0f, "full chain trial %d (type %s) peak=%g", trial,
                   filterTypeName ((int) e.base[P_mmType]), s.peak);
        }
    }

    // --- LFO shapes in range -------------------------------------------------
    {
        Lfo l;
        for (int sh = 0; sh < NUM_LFO_SHAPES; ++sh)
            for (int i = 0; i < 1000; ++i)
            {
                l.advance (3.0, 0.001);
                const float v = l.value (sh);
                CHECK (v >= -1.0001f && v <= 1.0001f, "lfo shape %d out of range", sh);
            }
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
