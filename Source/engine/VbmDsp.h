// VONBERGMACHINE - DSP building blocks (no JUCE dependency)
#pragma once
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>

namespace vbm
{
constexpr float PI_F = 3.14159265358979f;

// Zavalishin TPT state-variable filter
struct Svf
{
    float g = 0, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;

    void set (float hz, float q, float sr)
    {
        hz = std::clamp (hz, 10.0f, sr * 0.49f);
        g = std::tan (PI_F * hz / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    // returns lp, bp, hp by reference
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    inline float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
    inline float bp (float x) { float l, b, h; tick (x, l, b, h); return b; }
};

struct OnePole
{
    float a = 0, z = 0;
    void setLP (float hz, float sr) { a = std::exp (-2.0f * PI_F * hz / sr); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

struct DcBlock
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    inline float tick (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// 8-line feedback delay network reverb, Householder mixing, damped, stereo out
class FdnReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxLen = (int) (sr * 0.25f) + 8;
        for (auto& d : lines) { d.assign ((size_t) maxLen, 0.0f); }
        pre.assign ((size_t) (sr * 0.1f) + 8, 0.0f);
        inHp.set (220.0f, 0.707f, sr);
        inLp.set (9000.0f, 0.707f, sr);
        setSize (0.6f);
        clear();
    }

    void setSize (float s)   // 0..1
    {
        size = std::clamp (s, 0.0f, 1.0f);
        static const float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 79.3f };
        const float scale = 0.55f + 1.25f * size;
        const float rt60 = 0.6f + 5.4f * size * size;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = std::min ((int) lines[(size_t) i].size() - 2, std::max (8, (int) (base[i] * scale * 0.001f * sr)));
            gains[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * sr));
        }
        damp.fill (0);
        dampA = std::exp (-2.0f * PI_F * (4500.0f + 3500.0f * (1.0f - size)) / sr);
        preLen = std::min ((int) pre.size() - 2, (int) (0.012f * sr));
    }

    void clear()
    {
        for (auto& d : lines) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        damp.fill (0); pos.fill (0); prePos = 0;
        inHp.reset(); inLp.reset();
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        float x = 0.5f * (inL + inR);
        x = inLp.lp (inHp.hp (x));
        // predelay
        pre[(size_t) prePos] = x;
        int rp = prePos - preLen; if (rp < 0) rp += (int) pre.size();
        const float xin = pre[(size_t) rp];
        if (++prePos >= (int) pre.size()) prePos = 0;

        float o[8];
        float sum = 0;
        for (int i = 0; i < 8; ++i)
        {
            auto& d = lines[(size_t) i];
            int r = pos[i] - len[i]; if (r < 0) r += (int) d.size();
            float v = d[(size_t) r];
            damp[i] = v + dampA * (damp[i] - v);
            o[i] = damp[i] * gains[i];
            sum += o[i];
        }
        const float h = sum * 0.25f;   // 2/N
        static const float sgn[8] = { 1, -1, 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < 8; ++i)
        {
            auto& d = lines[(size_t) i];
            d[(size_t) pos[i]] = o[i] - h + xin * sgn[i] * 0.35f;
            if (++pos[i] >= (int) d.size()) pos[i] = 0;
        }
        outL = (o[0] + o[2] + o[4] + o[6]) * 0.6f;
        outR = (o[1] + o[3] + o[5] + o[7]) * 0.6f;
    }

private:
    float sr = 44100.0f, size = 0.6f, dampA = 0.5f;
    std::array<std::vector<float>, 8> lines;
    std::array<int, 8> len {}, pos {};
    std::array<float, 8> gains {}, damp {};
    std::vector<float> pre;
    int prePos = 0, preLen = 0;
    Svf inHp, inLp;
};

inline float softClip (float x)
{
    // smooth saturator, unity slope at 0, approaches +-1
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float dbToGain (float db) { return db <= -79.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
} // namespace vbm
