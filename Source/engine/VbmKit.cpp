// VONBERGMACHINE - built-in synthesized drum kit
#include "VbmKit.h"
#include "VbmDsp.h"

namespace vbm
{
namespace
{
struct Noise
{
    uint32_t s;
    explicit Noise (uint32_t seed) : s (seed ? seed : 1) {}
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

inline float expEnv (float t, float tau) { return std::exp (-t / tau); }

void normalize (std::vector<float>& v, float peak)
{
    float m = 0; for (float x : v) m = std::max (m, std::abs (x));
    if (m > 0) for (float& x : v) x *= peak / m;
}

void fadeTail (std::vector<float>& v, float sr, float ms)
{
    const int n = std::min ((int) v.size(), (int) (sr * ms * 0.001f));
    for (int i = 0; i < n; ++i) v[v.size() - 1 - (size_t) i] *= (float) i / (float) n;
}

std::vector<float> kick (float sr)
{
    const int n = (int) (sr * 0.5f);
    std::vector<float> o ((size_t) n);
    Noise nz (11); Svf hp; hp.set (3000.0f, 0.7f, sr);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = 52.0f + 170.0f * expEnv (t, 0.022f);
        ph += f / sr;
        float body = (float) std::sin (2.0 * PI_F * ph);
        float amp = t < 0.05f ? 1.0f : expEnv (t - 0.05f, 0.17f);
        float click = hp.hp (nz.next()) * expEnv (t, 0.0025f) * 0.5f;
        o[(size_t) i] = std::tanh (1.3f * (body * amp + click));
    }
    normalize (o, 0.98f); fadeTail (o, sr, 20);
    return o;
}

std::vector<float> bass808 (float sr)
{
    const int n = (int) (sr * 3.6f);
    std::vector<float> o ((size_t) n);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = BASS_ROOT_HZ * (1.0f + 1.6f * expEnv (t, 0.010f));
        ph += f / sr;
        const float s1 = (float) std::sin (2.0 * PI_F * ph);
        const float s2 = (float) std::sin (4.0 * PI_F * ph) * 0.08f;
        const float att = std::min (1.0f, t / 0.0015f);
        const float amp = att * (0.82f + 0.18f * expEnv (t, 0.09f)) * expEnv (t, 4.5f);
        // the references are distorted 808s: harmonics roughly -12..-25 dB
        o[(size_t) i] = std::tanh (1.8f * (s1 + s2) * amp) / std::tanh (1.8f);
    }
    normalize (o, 0.98f); fadeTail (o, sr, 60);
    return o;
}

std::vector<float> snare (float sr)
{
    const int n = (int) (sr * 0.35f);
    std::vector<float> o ((size_t) n);
    Noise nz (23); Svf bp, hp, lpn; bp.set (3200.0f, 0.7f, sr); hp.set (900.0f, 0.7f, sr); lpn.set (9000.0f, 0.7f, sr);
    double ph = 0, ph2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = 190.0f + 70.0f * expEnv (t, 0.012f);
        ph += f / sr; ph2 += (f * 1.58f) / sr;
        const float tone = ((float) std::sin (2.0 * PI_F * ph) + 0.4f * (float) std::sin (2.0 * PI_F * ph2)) * expEnv (t, 0.055f);
        const float nn = lpn.lp (hp.hp (bp.bp (nz.next()) * 2.0f + nz.next() * 0.3f)) * expEnv (t, 0.10f);
        o[(size_t) i] = std::tanh (1.6f * (0.30f * tone + 1.0f * nn));
    }
    normalize (o, 0.95f); fadeTail (o, sr, 25);
    return o;
}

std::vector<float> clap (float sr)
{
    const int n = (int) (sr * 0.42f);
    std::vector<float> o ((size_t) n);
    Noise nz (37); Svf bp, hp; bp.set (2100.0f, 0.9f, sr); hp.set (950.0f, 0.7f, sr);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        float env = 0;
        const float bursts[3] = { 0.0f, 0.0095f, 0.019f };
        for (float b : bursts)
            if (t >= b) env = std::max (env, expEnv (t - b, 0.0055f));
        if (t >= 0.027f) env = std::max (env, 0.85f * expEnv (t - 0.027f, 0.11f));
        o[(size_t) i] = hp.hp (bp.bp (nz.next() * 3.0f)) * env;
    }
    normalize (o, 0.95f); fadeTail (o, sr, 25);
    return o;
}

std::vector<float> hat (float sr, float tau, float len, uint32_t seed)
{
    const int n = (int) (sr * len);
    std::vector<float> o ((size_t) n);
    Noise nz (seed); Svf hp1, hp2, bp, lp1; hp1.set (4800.0f, 0.7f, sr); hp2.set (4800.0f, 0.7f, sr); bp.set (7000.0f, 1.1f, sr); lp1.set (11000.0f, 0.7f, sr);
    static const float fr[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    double ph[6] = { 0, 0.1, 0.2, 0.3, 0.4, 0.5 };
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        float m = 0;
        for (int k = 0; k < 6; ++k) { ph[k] += fr[k] * 1.72 / sr; m += (std::fmod (ph[k], 1.0) < 0.5 ? 1.0f : -1.0f); }
        const float x = m * 0.12f + nz.next() * 0.7f;
        const float env = std::min (1.0f, t / 0.0006f) * expEnv (t, tau);
        o[(size_t) i] = lp1.lp (hp2.hp (hp1.hp (x)) * 0.6f + bp.bp (x) * 1.0f) * env;
    }
    normalize (o, 0.9f); fadeTail (o, sr, std::min (30.0f, len * 300.0f));
    return o;
}

std::vector<float> perc (float sr)
{
    // short tonal "ta" snap (rim / conga hybrid) used for the mandelao syncopations
    const int n = (int) (sr * 0.24f);
    std::vector<float> o ((size_t) n);
    Noise nz (51); Svf bp; bp.set (3300.0f, 0.8f, sr);
    double ph = 0, ph2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = 380.0f + 160.0f * expEnv (t, 0.008f);
        ph += f / sr; ph2 += 1720.0 / sr;
        const float body = (float) std::sin (2.0 * PI_F * ph) * expEnv (t, 0.045f);
        const float ring = (std::fmod (ph2, 1.0) < 0.5 ? 0.25f : -0.25f) * expEnv (t, 0.012f);
        const float snap = bp.bp (nz.next() * 2.5f) * expEnv (t, 0.018f);
        o[(size_t) i] = std::tanh (2.0f * (0.38f * body + ring + 1.2f * snap));
    }
    normalize (o, 0.93f); fadeTail (o, sr, 20);
    return o;
}

std::vector<float> stomp (float sr)
{
    const int n = (int) (sr * 0.95f);
    std::vector<float> o ((size_t) n);
    Noise nz (77); Svf lpN, bpC, bpT; lpN.set (900.0f, 0.7f, sr); bpC.set (1500.0f, 1.0f, sr); bpT.set (1400.0f, 0.5f, sr);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = 46.0f + 60.0f * expEnv (t, 0.03f);
        ph += f / sr;
        const float thump = (float) std::sin (2.0 * PI_F * ph) * expEnv (t, 0.13f);
        const float body = lpN.lp (nz.next() * 2.0f) * expEnv (t, 0.085f);
        float cEnv = expEnv (t, 0.045f);
        if (t > 0.011f) cEnv = std::max (cEnv, 0.8f * expEnv (t - 0.011f, 0.05f));
        const float crack = bpC.bp (nz.next() * 3.0f) * cEnv;
        const float tail = bpT.bp (nz.next()) * expEnv (t, 0.30f) * 0.35f;
        o[(size_t) i] = std::tanh (2.4f * (0.9f * thump + 0.6f * body + 0.8f * crack + tail));
    }
    normalize (o, 0.97f); fadeTail (o, sr, 40);
    return o;
}

std::vector<float> impact (float sr, std::vector<float>& right)
{
    const int n = (int) (sr * 2.6f);
    std::vector<float> o ((size_t) n);
    right.assign ((size_t) n, 0.0f);
    Noise nzL (91), nzR (93); Svf hpL, hpR, sh; hpL.set (3200.0f, 0.7f, sr); hpR.set (3200.0f, 0.7f, sr); sh.set (6500.0f, 2.0f, sr);
    double ph = 0, pm[4] = { 0, 0, 0, 0 };
    static const float mf[4] = { 433.0f, 587.0f, 811.0f, 1093.0f };
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float f = 30.0f + 50.0f * expEnv (t, 0.12f);
        ph += f / sr;
        const float boom = (float) std::sin (2.0 * PI_F * ph) * expEnv (t, 0.45f) * 0.9f;
        float metal = 0;
        for (int k = 0; k < 4; ++k) { pm[k] += mf[k] * 3.1 / sr; metal += (std::fmod (pm[k], 1.0) < 0.5 ? 1.0f : -1.0f); }
        const float crashEnv = std::min (1.0f, t / 0.002f) * expEnv (t, 0.75f);
        const float cl = hpL.hp (nzL.next() + metal * 0.08f) * crashEnv;
        const float cr = hpR.hp (nzR.next() + metal * 0.08f) * crashEnv;
        const float hit = sh.bp (nzL.next()) * expEnv (t, 0.03f) * 0.8f;
        o[(size_t) i] = std::tanh (1.5f * (boom + 0.55f * cl + hit));
        right[(size_t) i] = std::tanh (1.5f * (boom + 0.55f * cr + hit));
    }
    float m = 0; for (size_t i = 0; i < o.size(); ++i) m = std::max ({ m, std::abs (o[i]), std::abs (right[i]) });
    if (m > 0) for (size_t i = 0; i < o.size(); ++i) { o[i] *= 0.95f / m; right[i] *= 0.95f / m; }
    fadeTail (o, sr, 120); fadeTail (right, sr, 120);
    return o;
}
} // namespace

SamplePtr makeDefaultSample (int lane, double sampleRate)
{
    const float sr = (float) sampleRate;
    auto s = std::make_shared<SampleBuf>();
    s->sampleRate = sampleRate;
    s->builtIn = true;
    switch (lane)
    {
        case KICK:  s->L = kick (sr);    s->name = "VBM Kick";      break;
        case BASS:  s->L = bass808 (sr); s->name = "VBM 808";       break;
        case SNARE: s->L = snare (sr);   s->name = "VBM Snare";     break;
        case CLAP:  s->L = clap (sr);    s->name = "VBM Clap";      break;
        case HAT:   s->L = hat (sr, 0.022f, 0.12f, 61); s->name = "VBM Hat"; break;
        case OHAT:  s->L = hat (sr, 0.20f, 0.75f, 63);  s->name = "VBM Open Hat"; break;
        case PERC:  s->L = perc (sr);    s->name = "VBM Perc";      break;
        case STOMP: s->L = stomp (sr);   s->name = "VBM Stomp";     break;
        default:    { std::vector<float> r; s->L = impact (sr, r); s->R = std::move (r); s->name = "VBM Impact"; break; }
    }
    if (s->R.empty()) s->R = s->L;
    return s;
}
} // namespace vbm
