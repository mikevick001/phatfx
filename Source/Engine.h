// PhatFX processing engine (JUCE-free). Chain order and per-block behaviour
// follow the original plug-in:
//   Bandpass -> Xcita -> BitCrush -> Mech -> Multimode filter -> Flanger ->
//   Tube -> MagicEQ -> Compressor -> (+ band-reject remainder) -> Master mix/vol -> clip
// Processing runs in 64-sample blocks; LFOs update once per block.
#pragma once

#include "DSP.h"

namespace phat
{
enum Param
{
    P_bpOn, P_bpLow, P_bpHigh, P_bpQLow, P_bpQHigh, P_bpBRMix, P_bpLink,
    P_distOn, P_distBit, P_distMech, P_distTube, P_distXcita,
    P_mmOn, P_mmMix, P_mmType, P_mmAttack, P_mmDecay, P_mmEnv, P_mmCutoff, P_mmRes,
    P_flOn, P_flAmount, P_flRate,
    P_eqOn, P_eqAmount, P_eqTune,
    P_compOn, P_compAmount, P_compRelease,
    P_masterMix, P_masterVol,
    P_lfo1On, P_lfo1Target, P_lfo1Shape, P_lfo1Depth, P_lfo1Rate, P_lfo1Sync,
    P_lfo2On, P_lfo2Target, P_lfo2Shape, P_lfo2Depth, P_lfo2Rate, P_lfo2Sync,
    P_xyXTarget, P_xyYTarget, P_compMode, P_eqMode,
    P_COUNT
};

// LFO target index -> Param (matches params::lfoTargets()).
inline Param lfoTargetParam (int i)
{
    static const Param t[] = { P_bpLow, P_bpHigh, P_bpQLow, P_bpQHigh, P_bpBRMix, P_distBit, P_distMech,
                               P_distTube, P_distXcita, P_mmMix, P_mmAttack, P_mmDecay, P_mmEnv, P_mmCutoff,
                               P_mmRes, P_flAmount, P_flRate, P_eqAmount, P_eqTune, P_compAmount,
                               P_compRelease, P_masterMix, P_masterVol };
    constexpr int n = (int) (sizeof (t) / sizeof (t[0]));
    return t[std::max (0, std::min (n - 1, i))];
}
constexpr int kNumLfoTargets = 23;

struct Engine
{
    static constexpr int kBlock = 64;

    float base[P_COUNT] {};   // unit values; choices = index; toggles = 0/1
    float eff[P_COUNT] {};    // after LFO modulation
    float sr = 44100;

    BandPass bandpass;
    Exciter exciter;
    Distortion dist;
    FilterSection filter;
    Flanger flanger;
    MagicEQ magicEq;
    Compressor comp;
    Lfo lfo[2];

    std::vector<float> dryL, dryR;
    float brMixState = 0, mixState = 1, volState = 1;
    float meter[2] = { 0, 0 };
    float lfoValue[2] = { 0, 0 };

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        bandpass.prepare (sr, kBlock);
        exciter.prepare (sr);
        filter.prepare (sr);
        flanger.prepare (sr);
        magicEq.prepare (sr);
        comp.prepare (sr);
        for (auto& l : lfo) l.reset();
        dryL.assign (kBlock, 0.0f);
        dryR.assign (kBlock, 0.0f);
        brMixState = base[P_bpBRMix];
        mixState = base[P_masterMix];
        volState = curve::masterGain (base[P_masterVol]);
        meter[0] = meter[1] = 0;
    }

    void reset() { prepare (sr); }

    static bool on (float v) { return v > 0.5f; }

    // The original adds the LFO to the target's value in its own units and
    // clamps the result to 0..1. For targets whose range isn't 0..1 (times,
    // rates, tune) this pins them low; reproduced here for fidelity.
    static void engRange (Param p, float& lo, float& hi)
    {
        lo = 0.0f; hi = 1.0f;
        if (p == P_mmAttack) { lo = 0.01f; hi = 1.0f; }
        else if (p == P_mmDecay) { lo = 0.2f; hi = 3.0f; }
        else if (p == P_flRate) { lo = 0.02f; hi = 2.0f; }
        else if (p == P_eqTune) { lo = 60.0f; hi = 120.0f; }
        else if (p == P_compRelease) { lo = 100.0f; hi = 2000.0f; }
        else if (p == P_compAmount) { lo = 0.0f; hi = 0.99f; }
    }

    void applyLfos()
    {
        std::copy (std::begin (base), std::end (base), std::begin (eff));
        for (int i = 0; i < 2; ++i)
        {
            const int o = i * (P_lfo2On - P_lfo1On);
            if (! on (base[P_lfo1On + o])) continue;
            const Param target = lfoTargetParam ((int) base[P_lfo1Target + o]);
            float lo, hi;
            engRange (target, lo, hi);
            const float engValue = clampf (lo + (hi - lo) * eff[target] + lfo[i].out, 0.0f, 1.0f);
            eff[target] = (engValue - lo) / (hi - lo);   // may leave 0..1 for non-unit ranges
        }
    }

    // Called after each block, like the original: the new value applies next block.
    void advanceLfos (double bpm, double ppq, bool playing, int n)
    {
        for (int i = 0; i < 2; ++i)
        {
            const int o = i * (P_lfo2On - P_lfo1On);
            const float rate = base[P_lfo1Rate + o];
            if (on (base[P_lfo1Sync + o]))
            {
                const double beats = syncRateBars (syncIndex (rate)) * 4.0;
                const double safeBpm = bpm > 0 ? bpm : 120.0;
                if (playing) lfo[i].setPhase (ppq / beats);
                else lfo[i].advance (safeBpm / 60.0 / beats, (double) n / sr);
            }
            else
                lfo[i].advance (freeRateHz (rate), (double) n / sr);
            lfo[i].slew ((int) base[P_lfo1Shape + o], base[P_lfo1Depth + o], n, sr);
            lfoValue[i] = lfo[i].value ((int) base[P_lfo1Shape + o]);
        }
    }

    // In-place stereo processing.
    void process (float* L, float* R, int numSamples, double bpm = 120.0, double ppq = 0.0, bool playing = false)
    {
        int done = 0;
        while (done < numSamples)
        {
            const int n = std::min (kBlock, numSamples - done);
            const double blockPpq = ppq + (bpm / 60.0) * (double) (done + n) / sr;
            applyLfos();
            processBlock (L + done, R + done, n);
            advanceLfos (bpm, blockPpq, playing, n);
            done += n;
        }
    }

    void processBlock (float* l, float* r, int n)
    {
        std::copy (l, l + n, dryL.begin());
        std::copy (r, r + n, dryR.begin());
        const float* e = eff;

        const bool bpOn = on (e[P_bpOn]);
        if (bpOn)
        {
            bandpass.setup (curve::bandHz (e[P_bpLow]), curve::bandHz (e[P_bpHigh]), e[P_bpQLow], e[P_bpQHigh]);
            bandpass.process (l, r, n);
        }

        bool tube = false;
        if (on (e[P_distOn]))
        {
            if (e[P_distXcita] > 0) exciter.process (l, r, n, e[P_distXcita]);
            if (e[P_distBit] > 0)
                for (int i = 0; i < n; ++i) { l[i] = dist.bitReduce (l[i], e[P_distBit]); r[i] = dist.bitReduce (r[i], e[P_distBit]); }
            if (e[P_distMech] > 0)
                for (int i = 0; i < n; ++i) { l[i] = Distortion::mech (l[i], e[P_distMech]); r[i] = Distortion::mech (r[i], e[P_distMech]); }
            tube = e[P_distTube] > 0;
        }

        if (on (e[P_mmOn]))
            filter.process (l, r, n, (int) e[P_mmType], e[P_mmCutoff], e[P_mmEnv], e[P_mmRes], e[P_mmMix],
                            0.01f + 0.99f * e[P_mmAttack], 0.2f + 2.8f * e[P_mmDecay]);

        if (on (e[P_flOn])) flanger.process (l, r, n, e[P_flAmount], 0.02f + 1.98f * e[P_flRate]);

        if (tube)
            for (int i = 0; i < n; ++i) { l[i] = Distortion::tube (l[i], e[P_distTube]); r[i] = Distortion::tube (r[i], e[P_distTube]); }

        if (on (e[P_eqOn])) magicEq.process (l, r, n, e[P_eqAmount], 60.0f + 60.0f * e[P_eqTune], on (e[P_eqMode]));

        if (on (e[P_compOn]))
            comp.process (l, r, n, std::min (0.99f, 0.99f * e[P_compAmount]), 100.0f + 1900.0f * e[P_compRelease], on (e[P_compMode]));

        // Band-reject remainder is added back after the chain.
        const float brTarget = e[P_bpBRMix];
        if (bpOn)
        {
            const float d = (brTarget - brMixState) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                l[i] += brMixState * bandpass.rejL[(size_t) i];
                r[i] += brMixState * bandpass.rejR[(size_t) i];
                brMixState += d;
            }
        }
        brMixState = brTarget;

        // Master: wet/dry mix and volume, smoothed across the block, then clip.
        const float mixT = e[P_masterMix], volT = curve::masterGain (e[P_masterVol]);
        const float dm = (mixT - mixState) / (float) n, dv = (volT - volState) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            l[i] = clampf ((mixState * l[i] + (1.0f - mixState) * dryL[(size_t) i]) * volState, -0.9999f, 0.9999f);
            r[i] = clampf ((mixState * r[i] + (1.0f - mixState) * dryR[(size_t) i]) * volState, -0.9999f, 0.9999f);
            mixState += dm; volState += dv;
        }
        mixState = mixT; volState = volT;

        // Peak meters with ~2 s full-scale fall.
        const float fall = 2.0f / sr * (float) n;
        for (int c = 0; c < 2; ++c)
        {
            const float* x = c == 0 ? l : r;
            float pk = std::max (0.0f, meter[c] - fall);
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::fabs (x[i]));
            meter[c] = pk;
        }
    }
};
} // namespace phat
