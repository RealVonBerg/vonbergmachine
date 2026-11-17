// VONBERGMACHINE - core types (no JUCE dependency)
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <array>
#include <algorithm>

namespace vbm
{
enum Lane : int { KICK = 0, BASS, SNARE, CLAP, HAT, OHAT, PERC, STOMP, FX, NUM_LANES };

inline const char* laneName (int l)
{
    static const char* n[] = { "KICK", "808", "SNARE", "CLAP", "HI-HAT", "OPEN HAT", "PERC", "STOMP", "IMPACT" };
    return (l >= 0 && l < NUM_LANES) ? n[l] : "?";
}

constexpr int TPB  = 48;          // ticks per beat
constexpr int TPS  = 12;          // ticks per 16th step
constexpr int TPBAR = 192;        // ticks per 4/4 bar
constexpr int STEPS = 16;

enum EventFlags : uint8_t { EV_NONE = 0, EV_GLIDE = 1 };

struct Event
{
    int     tick   = 0;      // tick inside the bar (0..191)
    uint8_t lane   = 0;
    uint8_t flags  = 0;
    float   vel    = 1.0f;   // 0..1
    float   pitch  = 0.0f;   // semitones (808: scale-relative to key; drums: pitch shift)
    float   pitch2 = 0.0f;   // pitch at end of note (dives / rising rolls)
    int     len    = 12;     // length in ticks (808 gate)
};

enum SectionType : uint8_t { SEC_INTRO = 0, SEC_DROP, SEC_STOP, SEC_HPBREAK, SEC_LPBREAK, SEC_BUILD, SEC_OUTRO };

inline const char* sectionName (int t)
{
    static const char* n[] = { "INTRO", "DROP", "STOP", "HP BREAK", "LP BREAK", "BUILD", "OUTRO" };
    return (t >= 0 && t <= SEC_OUTRO) ? n[t] : "?";
}

struct Section
{
    SectionType type = SEC_DROP;
    int startBar = 0;
    int numBars  = 1;
};

// One full arrangement ("pattern") produced by the generator
struct Song
{
    int      numBars = 64;
    int      style   = 0;
    float    refBpm  = 124.0f;
    uint32_t seed    = 0;
    int      bankIndex = -1;     // 0..499 for factory bank, -1 for random
    std::string name;

    std::vector<std::vector<Event>> bars;   // events per bar, sorted by tick
    std::vector<Section> sections;

    // automation, one value per 16th step (numBars*16 entries)
    std::vector<float>   lp;      // 0..1 normalized log cutoff (1 = open)
    std::vector<float>   hp;      // 0..1 normalized log cutoff (0 = open)
    std::vector<float>   verb;    // reverb send multiplier (1 = normal)
    std::vector<float>   riser;   // 0..1 noise riser intensity
    std::vector<float>   gain;    // master gain automation 0..1
    std::vector<uint8_t> kill;    // 1 = cut reverb tail at this step

    void allocate (int bars_)
    {
        numBars = bars_;
        bars.assign ((size_t) bars_, {});
        const size_t n = (size_t) bars_ * STEPS;
        lp.assign (n, 1.0f); hp.assign (n, 0.0f); verb.assign (n, 1.0f);
        riser.assign (n, 0.0f); gain.assign (n, 1.0f); kill.assign (n, 0);
    }

    void sortBars()
    {
        for (auto& b : bars)
            std::stable_sort (b.begin(), b.end(), [] (const Event& a, const Event& c) { return a.tick < c.tick; });
    }

    int sectionAt (int bar) const
    {
        for (size_t i = 0; i < sections.size(); ++i)
            if (bar >= sections[i].startBar && bar < sections[i].startBar + sections[i].numBars)
                return (int) i;
        return -1;
    }
};

// cutoff mappings shared by engine + UI
inline float lpNormToHz (float v) { return 60.0f * std::pow (20000.0f / 60.0f, std::clamp (v, 0.0f, 1.0f)); }
inline float hpNormToHz (float v) { return 20.0f * std::pow (100.0f, std::clamp (v, 0.0f, 1.0f)); }

// deterministic RNG (xorshift64*)
struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull) { if (s == 0) s = 1; next(); next(); }
    uint32_t next()
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return (uint32_t) ((s * 0x2545F4914F6CDD1Dull) >> 32);
    }
    float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }
    int   range (int lo, int hi) { return lo + (int) (next() % (uint32_t) (hi - lo + 1)); }   // inclusive
    bool  chance (float p) { return uni() < p; }
    template <typename T> const T& pick (const std::vector<T>& v) { return v[next() % v.size()]; }
    int weighted (std::initializer_list<float> w)
    {
        float sum = 0; for (float x : w) sum += x;
        float r = uni() * sum; int i = 0;
        for (float x : w) { if (r < x) return i; r -= x; ++i; }
        return (int) w.size() - 1;
    }
};

inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}

// Sample data for one lane (default synthesized or user-dropped)
struct SampleBuf
{
    std::vector<float> L, R;
    double sampleRate = 44100.0;
    std::string name;
    std::string path;      // empty for built-in
    bool builtIn = true;
    int length() const { return (int) L.size(); }
};
using SamplePtr = std::shared_ptr<const SampleBuf>;
using SongPtr   = std::shared_ptr<const Song>;

} // namespace vbm
