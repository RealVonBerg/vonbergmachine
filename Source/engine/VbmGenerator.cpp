// VONBERGMACHINE - arrangement / pattern generator
// Style data was measured from the five reference tracks (ROCKY I-V):
//   tempo, 16th-step onset profiles per band, 808 gate/sidechain profile,
//   section lengths, filter moves (LP / HP breaks), stops, reverb throws, rolls.
#include "VbmGenerator.h"
#include <cstdio>

namespace vbm
{
namespace
{
// ---------------------------------------------------------------- data
struct BassNote { int start, len; };
using BassTpl = std::vector<BassNote>;

enum BassFamily { BF_LONGGAP = 0, BF_ROLLING, BF_BOUNCE, BF_SUSTAIN };

const std::vector<BassTpl>& bassTemplates (int fam)
{
    static const std::vector<BassTpl> longGap = {
        { {0, 9}, {12, 2} }, { {0, 8}, {12, 3} }, { {0, 10}, {12, 2}, {14, 1} },
        { {0, 6}, {8, 2}, {12, 3} }, { {0, 9}, {11, 1}, {12, 3} }, { {0, 7}, {10, 2}, {12, 2} } };
    static const std::vector<BassTpl> rolling = {
        { {0, 4}, {4, 4}, {8, 4}, {12, 4} }, { {0, 6}, {6, 6}, {12, 4} }, { {0, 8}, {8, 8} },
        { {0, 3}, {3, 5}, {8, 3}, {11, 5} }, { {0, 7}, {7, 1}, {8, 8} }, { {0, 6}, {6, 2}, {8, 6}, {14, 2} } };
    static const std::vector<BassTpl> bounce = {
        { {0, 3}, {3, 4}, {7, 1}, {8, 3}, {11, 4}, {15, 1} },
        { {0, 2}, {3, 3}, {7, 1}, {8, 2}, {11, 3}, {14, 2} },
        { {0, 3}, {3, 3}, {6, 2}, {8, 3}, {11, 3}, {14, 2} },
        { {0, 3}, {3, 4}, {8, 3}, {11, 3}, {14, 2} } };
    static const std::vector<BassTpl> sustain = {
        { {0, 16} }, { {0, 14}, {14, 2} }, { {0, 8}, {8, 8} }, { {0, 12}, {12, 4} } };
    switch (fam)
    {
        case BF_LONGGAP: return longGap;
        case BF_ROLLING: return rolling;
        case BF_BOUNCE:  return bounce;
        default:         return sustain;
    }
}

struct StyleDef
{
    const char* name;
    float bpm;
    int key;
    std::vector<const char*> kick, perc, clap, hat, ohat, snare;
    std::vector<int> bassFams;
    float stompBackbeat;   // probability a drop layers STOMP on the backbeat
    float snareLayer;      // probability the snare doubles the clap
    float ghostPerc;       // amount of extra ghost percussion
    float glide;           // probability of 808 slides
    float intro[4];        // LPGROOVE, HPDRUMS, DIVE, SPARSE
    float trans[4];        // STOP, HPBREAK, LPBREAK, BUILD
    float outro[3];        // LPFADE, DIVETAIL, CUT
};

// 'X' accent, 'x' normal, 'o' medium, 'g' ghost, '.' rest
const std::vector<StyleDef>& styles()
{
    static const std::vector<StyleDef> s = {
        // ROCKY I  ~100 BPM : four-on-floor, rolling distorted 808, dense paired hats, LP dips
        { "ROCKY I", 100.0f, 3,
          { "X...X...X...X...", "X...X...X...X.x.", "X...X...X..xX..." },
          { "..X....X..X....X", "..X..g.X..X..g.X", "..X.g..X..X.g..X", "..X....X..X..X.X" },
          { "....X.......X...", "....X.......X..g" },
          { "..x.xx.x..x.x..x", "x.x.x.x.x.x.x.x.", "xx..xx..xx..xx..", "..x.xx.x..x.xx.x" },
          { "................", "......x.......x." },
          { "....x.......x...", "..............x." },
          { BF_ROLLING, BF_SUSTAIN, BF_ROLLING },
          0.30f, 0.50f, 0.6f, 0.25f,
          { 1.0f, 0.8f, 0.5f, 2.0f }, { 1.0f, 0.6f, 1.2f, 0.9f }, { 1.0f, 0.6f, 0.6f } },

        // ROCKY II ~130 BPM : long gated 808s with gaps, mandelao percussion, HP breaks + reverb
        { "ROCKY II", 130.0f, 11,
          { "X.......X.......", "X.......X...X...", "X......xX......." },
          { "...X..X.......X.", "X..X..X.......X.", "...X..X...X...X.", "...X..X.......X." },
          { "....X.......X...", "....X.......X.X." },
          { "X..XX.X.X...X.X.", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "X..xX.x.X..xX.x." },
          { "................" },
          { "....x.......x...", "......x.......x." },
          { BF_LONGGAP },
          0.40f, 0.40f, 0.4f, 0.15f,
          { 0.6f, 1.5f, 0.6f, 0.6f }, { 1.6f, 1.6f, 0.3f, 0.7f }, { 0.4f, 0.4f, 1.2f } },

        // ROCKY III ~123 BPM : constant 808, LP breaks that sweep open, crash on the 1
        { "ROCKY III", 123.0f, 1,
          { "X.......X.......", "X.......X....X..", "X...X...X......." },
          { "..X....X..X....X", "..X....X..X..X.X", "..X..g.X..X....X" },
          { "....X.......X..." },
          { "..x....x..x....x", "x.x.x.x.x.x.x.x.", "x.xxx.xxx.xxx.xx" },
          { "................", "..............x." },
          { "....x.......x..." },
          { BF_ROLLING, BF_LONGGAP, BF_SUSTAIN },
          0.30f, 0.60f, 0.5f, 0.20f,
          { 1.0f, 0.6f, 0.4f, 1.0f }, { 0.8f, 0.5f, 2.0f, 0.8f }, { 1.0f, 0.5f, 0.8f } },

        // ROCKY IV ~124 BPM : bouncing pumped 808, 808 dive intro, HP breaks with snare-roll builds
        { "ROCKY IV", 124.0f, 11,
          { "X...X...X...X...", "X...X...X...X..x" },
          { "..X....X..X....X", "..X....X..X..g.X", "..X.g..X..X....X" },
          { "....X.......X..." },
          { "..x.x..x..x.x..x", "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx", "..x.x..x..x.x.xx" },
          { "................", "..x...x...x...x." },
          { "....x.......x..." },
          { BF_BOUNCE, BF_ROLLING, BF_BOUNCE },
          0.35f, 0.50f, 0.5f, 0.25f,
          { 0.5f, 0.8f, 2.0f, 0.6f }, { 1.0f, 1.8f, 0.6f, 1.1f }, { 0.6f, 1.6f, 0.6f } },

        // ROCKY V  ~117 BPM : four-on-floor stomp, sustained pumping bass, LP intro, hard stops
        { "ROCKY V", 117.0f, 3,
          { "X...X...X...X..." },
          { "..X....X..X....X", "..X....X..X..X.X", "..X..g.X..X..g.X" },
          { "....X.......X..." },
          { "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "..x.x.xx..x.x.xx" },
          { "................", "..x...x...x...x." },
          { "....x.......x...", "...............x" },
          { BF_SUSTAIN, BF_ROLLING, BF_SUSTAIN },
          0.60f, 0.30f, 0.4f, 0.10f,
          { 2.0f, 0.5f, 0.4f, 0.6f }, { 2.0f, 0.8f, 0.6f, 0.6f }, { 0.8f, 0.6f, 1.0f } },
    };
    return s;
}

// 808 root movement per 4-bar phrase (semitones relative to key)
const std::vector<std::array<int, 4>>& progressions()
{
    static const std::vector<std::array<int, 4>> p = {
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, -2, -2}, {0, 0, 0, -2},
        {0, 3, 0, -2}, {0, 0, 5, 3}, {0, -4, 0, -2}, {0, 0, 1, 0}, {0, 0, 0, 5}, {0, 0, 3, 0},
        {0, 0, -5, -5}, {0, 1, 0, 1} };
    return p;
}

float velOf (char c)
{
    switch (c)
    {
        case 'X': return 1.0f;
        case 'x': return 0.80f;
        case 'o': return 0.62f;
        case 'g': return 0.42f;
        default:  return 0.0f;
    }
}

// ---------------------------------------------------------------- builder
enum IntroType { IN_LPGROOVE = 0, IN_HPDRUMS, IN_DIVE, IN_SPARSE };
enum TransType { TR_STOP = 0, TR_HPBREAK, TR_LPBREAK, TR_BUILD };
enum OutroType { OUT_LPFADE = 0, OUT_DIVETAIL, OUT_CUT };

constexpr uint32_t laneBit (int l) { return 1u << l; }
constexpr uint32_t ALL_LANES = (1u << NUM_LANES) - 1;

struct Groove
{
    const char* kick; const char* perc; const char* clap; const char* hat; const char* hatB;
    const char* ohat; const char* snare;
    BassTpl bassA, bassB;
    std::array<int, 4> prog;
    bool stomp = false, snareLayer = false, useB = false;
    float ghost = 0.0f, glide = 0.0f;
    float hatVel = 1.0f;
};

struct Builder
{
    Song& s;
    const StyleDef& st;
    Rng& r;
    Groove base;

    Builder (Song& song, const StyleDef& style, Rng& rng) : s (song), st (style), r (rng) {}

    // ---------- primitive helpers
    void add (int bar, int tick, int lane, float vel, float pitch = 0.0f, int len = TPS, float pitch2 = 1e9f, uint8_t flags = 0)
    {
        if (bar < 0 || bar >= s.numBars || vel <= 0.0f) return;
        while (tick >= TPBAR) { tick -= TPBAR; ++bar; if (bar >= s.numBars) return; }
        Event e;
        e.tick = tick; e.lane = (uint8_t) lane; e.vel = std::clamp (vel, 0.0f, 1.0f);
        e.pitch = pitch; e.pitch2 = (pitch2 > 1e8f) ? pitch : pitch2; e.len = std::max (3, len); e.flags = flags;
        s.bars[(size_t) bar].push_back (e);
    }

    void addPat (int bar, int lane, const char* p, float velScale = 1.0f, float pitch = 0.0f)
    {
        if (p == nullptr) return;
        for (int i = 0; i < STEPS && p[i] != 0; ++i)
        {
            float v = velOf (p[i]);
            if (v > 0.0f) add (bar, i * TPS, lane, v * velScale, pitch);
        }
    }

    void clearRange (int bar, int fromTick, int toTick, uint32_t laneMask)
    {
        if (bar < 0 || bar >= s.numBars) return;
        auto& ev = s.bars[(size_t) bar];
        ev.erase (std::remove_if (ev.begin(), ev.end(), [&] (const Event& e) {
            return (laneMask & laneBit (e.lane)) && e.tick >= fromTick && e.tick < toTick; }), ev.end());
        // truncate 808 notes that would ring into the cleared range
        if (laneMask & laneBit (BASS))
            for (auto& e : ev)
                if (e.lane == BASS && e.tick < fromTick && e.tick + e.len > fromTick)
                    e.len = std::max (6, fromTick - e.tick);
    }

    void clearBar (int bar, uint32_t mask = ALL_LANES) { clearRange (bar, 0, TPBAR, mask); }

    // automation over absolute steps [a, b) linearly from v0 to v1
    static void ramp (std::vector<float>& v, int a, int b, float v0, float v1)
    {
        const int n = (int) v.size();
        for (int i = a; i < b; ++i)
        {
            if (i < 0 || i >= n) continue;
            const float t = (b - a) > 1 ? (float) (i - a) / (float) (b - a - 1) : 1.0f;
            v[(size_t) i] = v0 + (v1 - v0) * t;
        }
    }
    static void fill (std::vector<float>& v, int a, int b, float x) { ramp (v, a, b, x, x); }

    int stepOf (int bar, int step = 0) const { return bar * STEPS + step; }

    // accelerating roll (snare / clap / perc / 808).  segments: list of {lengthInTicks, divisionTicks}
    void roll (int lane, int bar, int startTick, const std::vector<std::pair<int, int>>& segs,
               float v0, float v1, float p0, float p1)
    {
        int total = 0; for (auto& sg : segs) total += sg.first;
        int t = startTick;
        for (auto& sg : segs)
        {
            const int end = t + sg.first;
            for (int k = t; k < end; k += sg.second)
            {
                const float f = (float) (k - startTick) / (float) std::max (1, total);
                const float jitter = 1.0f - 0.08f * r.uni();
                add (bar, k, lane, (v0 + (v1 - v0) * f) * jitter, p0 + (p1 - p0) * f, sg.second);
            }
            t = end;
        }
    }

    // classic snare-roll build: 8ths -> 16ths -> 32nds -> 64ths with rising velocity & pitch
    void snareRoll (int bar, int bars, int lane = SNARE, bool full = true)
    {
        const float pRise = (float) r.pick (std::vector<int> { 0, 3, 5, 7, 12 });
        std::vector<std::pair<int, int>> segs;
        if (bars >= 2)       segs = { {TPBAR, 48}, {TPBAR / 2, 24}, {TPBAR / 4, 12}, {TPBAR / 8, 6}, {TPBAR / 8, 3} };
        else if (full)       segs = { {48, 24}, {48, 12}, {48, 6}, {24, 4}, {24, 3} };
        else                 segs = { {48, 12}, {24, 6}, {24, 3} };
        const int start = (bars >= 2 || full) ? 0 : TPBAR / 2;
        roll (lane, bar, start, segs, full ? 0.7f : 0.75f, 1.0f, 0.0f, pRise);
        if (r.chance (0.45f))   // layer a second roll at half the density
        {
            std::vector<std::pair<int, int>> s2;
            if (bars >= 2)  s2 = { {TPBAR, 48}, {TPBAR, 24} };
            else if (full)  s2 = { {TPBAR / 2, 24}, {TPBAR / 2, 12} };
            else            s2 = { {TPBAR / 4, 24}, {TPBAR / 4, 12} };
            roll (lane == SNARE ? CLAP : SNARE, bar, start, s2, 0.3f, 0.75f, 0.0f, pRise * 0.5f);
        }
    }

    void hatRoll (int bar, int beat)
    {
        clearRange (bar, beat * TPB, (beat + 1) * TPB, laneBit (HAT) | laneBit (OHAT));
        const int div = r.pick (std::vector<int> { 6, 8, 6, 4 });   // 32nds, 16th triplets, 48ths
        for (int k = 0; k < TPB; k += div)
            add (bar, beat * TPB + k, HAT, 0.45f + 0.5f * (float) k / TPB, r.chance (0.3f) ? (float) k / TPB * 4.0f : 0.0f);
    }

    void percFill (int bar)
    {
        clearRange (bar, 3 * TPB, TPBAR, laneBit (PERC));
        const int div = r.chance (0.5f) ? 12 : 8;
        for (int k = 3 * TPB; k < TPBAR; k += div)
            add (bar, k, PERC, 0.5f + 0.5f * (float) (k - 3 * TPB) / TPB, 0.0f);
    }

    void stompThrow (int bar, int tick, float verbAmt)
    {
        clearRange (bar, tick, TPBAR, ALL_LANES);
        add (bar, tick, STOMP, 1.0f);
        if (r.chance (0.5f)) add (bar, tick, CLAP, 0.9f);
        fill (s.verb, stepOf (bar, tick / TPS), stepOf (bar + 1), verbAmt);
    }

    // ---------- groove for one bar
    void grooveBar (int bar, const Groove& g, int idx, int len, uint32_t mask)
    {
        const bool second = idx >= 4 && g.useB;
        if (mask & laneBit (KICK))  addPat (bar, KICK, g.kick);
        if (mask & laneBit (PERC))  addPat (bar, PERC, g.perc, 0.95f);
        if (mask & laneBit (CLAP))  addPat (bar, CLAP, g.clap);
        if (mask & laneBit (SNARE) && g.snareLayer) addPat (bar, SNARE, g.snare, 0.85f);
        if (mask & laneBit (STOMP) && g.stomp) addPat (bar, STOMP, g.clap, 0.8f);
        if (mask & laneBit (HAT))
        {
            const char* hp = (second && g.hatB) ? g.hatB : g.hat;
            for (int i = 0; i < STEPS; ++i)
            {
                float v = velOf (hp[i]);
                if (v <= 0.0f) continue;
                const float acc = (i % 4 == 0) ? 1.0f : ((i % 2 == 0) ? 0.82f : 0.66f);
                add (bar, i * TPS, HAT, v * acc * g.hatVel);
            }
        }
        if (mask & laneBit (OHAT)) addPat (bar, OHAT, g.ohat, 0.8f);

        // ghost percussion in the 2nd half of a drop
        if ((mask & laneBit (PERC)) && idx >= 2 && g.ghost > 0.0f)
        {
            const int n = (int) (g.ghost * 2.0f + r.uni());
            for (int k = 0; k < n; ++k)
            {
                const int stp = r.pick (std::vector<int> { 5, 6, 9, 11, 13, 14 });
                if (g.perc[stp] == '.') add (bar, stp * TPS, PERC, 0.35f + 0.15f * r.uni());
            }
        }

        if (mask & laneBit (BASS))
        {
            const BassTpl& tpl = second ? g.bassB : g.bassA;
            const int root = g.prog[(size_t) (idx % 4)];
            for (size_t n = 0; n < tpl.size(); ++n)
            {
                float p = (float) root;
                uint8_t fl = 0;
                const bool last = n + 1 == tpl.size();
                // phonk 808 slides: last note of a phrase jumps an octave / slides
                if (last && tpl.size() > 1 && (idx % 4 == 3) && r.chance (0.45f))
                {
                    p = (float) root + (float) r.pick (std::vector<int> { 12, 12, 7, -5, 5 });
                    if (r.chance (0.6f)) fl = EV_GLIDE;
                }
                else if (n > 0 && r.chance (g.glide * 0.35f))
                {
                    p = (float) root + (float) r.pick (std::vector<int> { 12, 7, 3, -2 });
                    fl = EV_GLIDE;
                }
                add (bar, tpl[n].start * TPS, BASS, 0.95f, p, tpl[n].len * TPS, 1e9f, fl);
            }
        }
        (void) len;
    }

    Groove makeGroove (bool variantOfBase)
    {
        Groove g;
        if (variantOfBase)
        {
            g = base;
            if (r.chance (0.5f)) g.hat = r.pick (st.hat);
            if (r.chance (0.4f)) g.perc = r.pick (st.perc);
            if (r.chance (0.5f)) g.bassA = r.pick (bassTemplates (r.pick (st.bassFams)));
            g.bassB = r.pick (bassTemplates (r.pick (st.bassFams)));
            g.useB = r.chance (0.55f);
            g.prog = r.pick (progressions());
            g.stomp = r.chance (st.stompBackbeat);
            g.snareLayer = r.chance (st.snareLayer);
            g.hatB = r.pick (st.hat);
            g.ghost = st.ghostPerc * (0.5f + r.uni());
            return g;
        }
        g.kick  = r.pick (st.kick);
        g.perc  = r.pick (st.perc);
        g.clap  = r.pick (st.clap);
        g.hat   = r.pick (st.hat);
        g.hatB  = r.pick (st.hat);
        g.ohat  = r.chance (0.35f) ? r.pick (st.ohat) : "................";
        g.snare = r.pick (st.snare);
        const int fam = r.pick (st.bassFams);
        g.bassA = r.pick (bassTemplates (fam));
        g.bassB = r.pick (bassTemplates (r.chance (0.7f) ? fam : r.pick (st.bassFams)));
        g.useB  = r.chance (0.5f);
        g.prog  = r.pick (progressions());
        g.stomp = r.chance (st.stompBackbeat);
        g.snareLayer = r.chance (st.snareLayer);
        g.ghost = st.ghostPerc * (0.5f + r.uni());
        g.glide = st.glide * (0.5f + r.uni());
        g.hatVel = 0.85f + 0.15f * r.uni();
        return g;
    }

    // ---------- sections
    void dropEntry (int bar, bool afterBreak)
    {
        if (! afterBreak) return;
        s.kill[(size_t) stepOf (bar)] = 1;
        add (bar, 0, FX, 0.9f + 0.1f * r.uni());
        if (r.chance (0.55f)) add (bar, 0, STOMP, 1.0f);
    }

    void drop (int bar0, int n, int nextType, bool afterBreak, const Groove& g)
    {
        for (int i = 0; i < n; ++i)
            grooveBar (bar0 + i, g, i % 8, n, ALL_LANES);
        dropEntry (bar0, afterBreak);

        // small fills on the 4th bar of each phrase
        for (int i = 3; i < n - 1; i += 4)
        {
            const int b = bar0 + i;
            if (! r.chance (0.7f)) continue;
            switch (r.weighted ({ 1.2f, 1.0f, 0.6f, 0.7f, 0.5f }))
            {
                case 0: hatRoll (b, 3); break;
                case 1: percFill (b); break;
                case 2: clearRange (b, 14 * TPS, TPBAR, ALL_LANES); break;                       // air gap
                case 3: roll (SNARE, b, 3 * TPB, { {24, 6}, {24, 3} }, 0.4f, 0.95f, 0.0f, 3.0f); break; // half-beat snare roll
                default: hatRoll (b, 1); hatRoll (b, 3); break;
            }
        }

        // big fill on the last bar depending on what follows
        const int lb = bar0 + n - 1;
        switch (nextType)
        {
            case TR_STOP:
                switch (r.weighted ({ 1.0f, 1.0f, 0.8f, 0.6f }))
                {
                    case 0: clearRange (lb, r.pick (std::vector<int> { 12, 13, 14 }) * TPS, TPBAR, ALL_LANES); break;
                    case 1: stompThrow (lb, 12 * TPS, 3.0f); break;
                    case 2: snareRoll (lb, 1, SNARE, false); clearRange (lb, 15 * TPS, TPBAR, laneBit (BASS) | laneBit (KICK)); break;
                    default: break;
                }
                break;
            case TR_HPBREAK:
                if (r.chance (0.5f)) percFill (lb); else hatRoll (lb, 3);
                if (r.chance (0.4f)) clearRange (lb, 14 * TPS, TPBAR, laneBit (BASS) | laneBit (KICK));
                break;
            case TR_LPBREAK:
                if (r.chance (0.6f)) hatRoll (lb, 3);
                break;
            case TR_BUILD:
                if (r.chance (0.5f)) clearRange (lb, 15 * TPS, TPBAR, ALL_LANES);
                break;
            default: // outro
                if (r.chance (0.5f)) hatRoll (lb, 3);
                break;
        }
    }

    void stop (int bar)
    {
        clearBar (bar);
        switch (r.weighted ({ 1.2f, 1.0f, 0.7f, 0.6f }))
        {
            case 0:  // stomp + reverb throw, tail is cut on the drop
                add (bar, 0, STOMP, 1.0f); if (r.chance (0.5f)) add (bar, 0, CLAP, 0.85f);
                fill (s.verb, stepOf (bar), stepOf (bar + 1), 4.0f);
                break;
            case 1:  // dead silence (only the previous tails)
                fill (s.verb, stepOf (bar), stepOf (bar + 1), 2.0f);
                break;
            case 2:  // 808 dive
                add (bar, 0, BASS, 0.95f, 0.0f, 14 * TPS, -12.0f);
                add (bar, 0, FX, 0.7f);
                break;
            default: // half stop: stomp on the 1, clap throw on the 3
                add (bar, 0, STOMP, 1.0f); add (bar, 2 * TPB, CLAP, 0.8f);
                fill (s.verb, stepOf (bar), stepOf (bar + 1), 3.0f);
                break;
        }
    }

    void hpBreak (int bar0, int n, const Groove& g)
    {
        const float hpv = 0.80f + 0.08f * r.uni();   // ~ 800 - 1100 Hz
        Groove gb = g; gb.useB = false;
        for (int i = 0; i < n; ++i)
            grooveBar (bar0 + i, gb, i, n, laneBit (PERC) | laneBit (CLAP) | laneBit (HAT) | laneBit (SNARE) | laneBit (OHAT));
        ramp (s.hp, stepOf (bar0), stepOf (bar0, 2), 0.3f, hpv);
        fill (s.hp, stepOf (bar0, 2), stepOf (bar0 + n), hpv);
        fill (s.verb, stepOf (bar0), stepOf (bar0 + n), 2.4f + r.uni());
        if (r.chance (0.6f)) { add (bar0, 0, STOMP, 1.0f); add (bar0, 0, FX, 0.6f); }

        const int lb = bar0 + n - 1;
        switch (r.weighted ({ 1.0f, 1.2f, 0.6f }))
        {
            case 0: // 808 swell back in: HP opens over the last half bar, 808 slides up
                ramp (s.hp, stepOf (lb, 8), stepOf (lb + 1), hpv, 0.0f);
                add (lb, 8 * TPS, BASS, 0.9f, -12.0f, 8 * TPS, 0.0f);
                break;
            case 1: // snare roll + riser, HP keeps climbing
                clearBar (lb, laneBit (SNARE) | laneBit (CLAP));
                snareRoll (lb, 1, SNARE, true);
                ramp (s.riser, stepOf (lb), stepOf (lb + 1), 0.0f, 1.0f);
                ramp (s.hp, stepOf (lb), stepOf (lb + 1), hpv, std::min (0.95f, hpv + 0.1f));
                clearRange (lb, 15 * TPS, TPBAR, ALL_LANES);
                s.gain[(size_t) stepOf (lb, 15)] = 0.0f;
                break;
            default: // dry cut
                clearRange (lb, 12 * TPS, TPBAR, ALL_LANES);
                add (lb, 12 * TPS, STOMP, 0.9f);
                break;
        }
    }

    void lpBreak (int bar0, int n, const Groove& g)
    {
        Groove gl = g; gl.useB = false;
        for (int i = 0; i < n; ++i) grooveBar (bar0 + i, gl, i, n, ALL_LANES);
        add (bar0, 0, FX, 0.95f);                                 // crash/impact on the 1
        s.kill[(size_t) stepOf (bar0)] = 1;
        const float low = 0.22f + 0.08f * r.uni();               // ~ 230 - 330 Hz
        fill (s.lp, stepOf (bar0, 2), stepOf (bar0 + 1), low);
        ramp (s.lp, stepOf (bar0), stepOf (bar0, 2), 1.0f, low);
        const int lb = bar0 + n - 1;
        ramp (s.lp, stepOf (bar0 + 1), stepOf (lb, 12), low, 0.8f);
        ramp (s.lp, stepOf (lb, 12), stepOf (lb + 1), 0.8f, 1.0f);
        if (r.chance (0.5f)) { clearBar (lb, laneBit (SNARE)); snareRoll (lb, 1, SNARE, false); }
        // the level drops away on the final beat (as in the reference)
        const float gl4[4] = { 0.85f, 0.6f, 0.4f, 0.0f };
        for (int k = 0; k < 4; ++k) s.gain[(size_t) stepOf (lb, 12 + k)] = gl4[k];
    }

    void build (int bar0, int n, const Groove& g)
    {
        for (int i = 0; i < n; ++i)
        {
            const int b = bar0 + i;
            clearBar (b);
            if (r.chance (0.6f)) for (int q = 0; q < 4; ++q) add (b, q * TPB, KICK, 0.55f + 0.1f * q + 0.15f * i);
            addPat (b, CLAP, g.clap, 0.8f);
            if (i == 0 && r.chance (0.5f)) addPat (b, HAT, "x.x.x.x.x.x.x.x.", 0.6f);
        }
        snareRoll (bar0, n, SNARE, true);
        ramp (s.riser, stepOf (bar0), stepOf (bar0 + n), 0.0f, 1.0f);
        ramp (s.hp, stepOf (bar0), stepOf (bar0 + n), 0.0f, 0.42f + 0.1f * r.uni());
        fill (s.verb, stepOf (bar0), stepOf (bar0 + n), 1.5f);
        const int lb = bar0 + n - 1;
        clearRange (lb, 15 * TPS, TPBAR, ALL_LANES);
        s.gain[(size_t) stepOf (lb, 15)] = 0.0f;
    }

    // ---------- intro / outro
    void intro (int type, int n, const Groove& g)
    {
        switch (type)
        {
            case IN_LPGROOVE:
            {
                const float lpv = 0.40f + 0.12f * r.uni();
                for (int i = 0; i < n; ++i)
                    grooveBar (i, g, i, n, (i < n / 2 ? (laneBit (BASS) | laneBit (KICK) | laneBit (CLAP) | laneBit (HAT))
                                                       : ALL_LANES) & ~laneBit (STOMP));
                fill (s.lp, 0, stepOf (n - 1, 8), lpv);
                ramp (s.lp, stepOf (n - 1, 8), stepOf (n), lpv, 1.0f);
                fill (s.gain, 0, stepOf (n), 0.7f);
                if (r.chance (0.5f)) { clearBar (n - 1, laneBit (SNARE)); snareRoll (n - 1, 1, SNARE, false); }
                clearRange (n - 1, 15 * TPS, TPBAR, ALL_LANES);
                s.gain[(size_t) stepOf (n - 1, 15)] = 0.0f;
                break;
            }
            case IN_HPDRUMS:
            {
                for (int i = 0; i < n - 1; ++i)
                    grooveBar (i, g, i, n, laneBit (PERC) | laneBit (CLAP) | laneBit (HAT) | laneBit (OHAT));
                fill (s.hp, 0, stepOf (n - 1), 0.82f);
                fill (s.verb, 0, stepOf (n), 2.0f);
                build (n - 1, 1, g);
                break;
            }
            case IN_DIVE:
            {
                add (0, 0, BASS, 1.0f, 24.0f, 2 * TPBAR - TPS, 0.0f);   // 808 dive from 2 octaves up
                add (0, 0, FX, 1.0f);
                fill (s.verb, 0, stepOf (2), 1.6f);
                for (int i = 2; i < n - 1; ++i)
                {
                    add (i, TPB, STOMP, 0.7f); add (i, 3 * TPB, STOMP, 0.8f);
                    addPat (i, PERC, g.perc, 0.6f);
                }
                fill (s.hp, stepOf (2), stepOf (n - 1), 0.6f);
                fill (s.verb, stepOf (2), stepOf (n - 1), 2.5f);
                build (n - 1, 1, g);
                break;
            }
            default: // IN_SPARSE
            {
                const float lpv = 0.5f + 0.1f * r.uni();
                for (int i = 0; i < n - 1; ++i)
                {
                    add (i, 0, KICK, 0.8f);
                    add (i, 0, BASS, 0.85f, (float) (i % 2 ? -5 : 0), 6 * TPS);
                    add (i, 10 * TPS, KICK, 0.6f);
                    add (i, 10 * TPS, BASS, 0.8f, -12.0f, 6 * TPS, 0.0f);  // gliding swell
                    if (i % 2 == 1) add (i, 12 * TPS, CLAP, 0.6f);
                }
                fill (s.lp, 0, stepOf (n - 1), lpv);
                const int lb = n - 1;
                for (int k = 0; k < 8; ++k) add (lb, k * 24, BASS, 0.5f + 0.06f * k, (float) k, 12);   // 808 roll rising
                snareRoll (lb, 1, SNARE, true);
                ramp (s.lp, stepOf (lb), stepOf (lb + 1), lpv, 1.0f);
                ramp (s.riser, stepOf (lb), stepOf (lb + 1), 0.0f, 0.8f);
                clearRange (lb, 15 * TPS, TPBAR, ALL_LANES);
                s.gain[(size_t) stepOf (lb, 15)] = 0.0f;
                break;
            }
        }
    }

    void outro (int type, int bar0, int n, const Groove& g)
    {
        switch (type)
        {
            case OUT_LPFADE:
                for (int i = 0; i < n; ++i) grooveBar (bar0 + i, g, i, n, ALL_LANES & ~laneBit (STOMP));
                ramp (s.lp, stepOf (bar0), stepOf (bar0 + n), 1.0f, 0.12f);
                for (int k = 0; k < n * STEPS; ++k)
                    s.gain[(size_t) stepOf (bar0, k)] = 1.0f - (float) k / (float) (n * STEPS);
                break;
            case OUT_DIVETAIL:
                add (bar0, 0, BASS, 1.0f, 0.0f, std::min (n, 2) * TPBAR - TPS, -12.0f);
                add (bar0, 0, FX, 1.0f); add (bar0, 0, STOMP, 1.0f);
                s.kill[(size_t) stepOf (bar0)] = 1;
                fill (s.verb, stepOf (bar0), stepOf (bar0 + n), 2.2f);
                break;
            default: // OUT_CUT
                add (bar0, 0, STOMP, 1.0f); add (bar0, 0, CLAP, 0.9f); add (bar0, 0, FX, 0.8f);
                s.kill[(size_t) stepOf (bar0)] = 1;
                fill (s.verb, stepOf (bar0), stepOf (bar0 + n), 4.0f);
                break;
        }
    }
};
} // namespace

const char* styleName (int style) { return styles()[(size_t) std::clamp (style, 0, NUM_STYLES - 1)].name; }
float styleBpm (int style)        { return styles()[(size_t) std::clamp (style, 0, NUM_STYLES - 1)].bpm; }
int styleKey (int style)          { return styles()[(size_t) std::clamp (style, 0, NUM_STYLES - 1)].key; }

SongPtr generateSong (uint32_t seed, int style, int bars, int bankIndex)
{
    style = std::clamp (style, 0, NUM_STYLES - 1);
    bars  = std::clamp (bars, 16, 128);
    const StyleDef& st = styles()[(size_t) style];
    auto song = std::make_shared<Song>();
    Song& s = *song;
    s.allocate (bars);
    s.style = style; s.refBpm = st.bpm; s.seed = seed; s.bankIndex = bankIndex;

    Rng r (seed ^ ((uint64_t) style << 40) ^ ((uint64_t) bars << 48));
    Builder b (s, st, r);
    b.base = b.makeGroove (false);

    // ---------------- plan
    std::vector<Section> plan;
    const int introType = r.weighted ({ st.intro[0], st.intro[1], st.intro[2], st.intro[3] });
    int introLen = 4;
    if (introType == IN_LPGROOVE) introLen = (bars >= 64 && r.chance (0.5f)) ? 8 : 4;
    else if (introType == IN_HPDRUMS) introLen = r.range (2, 4);
    const int outroType = r.weighted ({ st.outro[0], st.outro[1], st.outro[2] });
    const int outroLen = bars >= 48 ? r.range (2, 4) : 2;

    plan.push_back ({ SEC_INTRO, 0, introLen });
    int bar = introLen;
    int rem = bars - introLen - outroLen;
    std::vector<int> transTypes;
    while (rem > 0)
    {
        int dl = std::min (8, rem);
        int after = rem - dl;
        if (after > 0 && after < 5) { dl += after; after = 0; }
        plan.push_back ({ SEC_DROP, bar, dl });
        bar += dl; rem -= dl;
        if (rem <= 0) break;

        int tt = r.weighted ({ st.trans[0], st.trans[1], st.trans[2], st.trans[3] });
        // avoid the same transition twice in a row
        if (! transTypes.empty() && transTypes.back() == tt && r.chance (0.6f))
            tt = r.weighted ({ st.trans[0], st.trans[1], st.trans[2], st.trans[3] });
        int tl = 1;
        if (tt == TR_HPBREAK) tl = r.range (2, 3);
        else if (tt == TR_LPBREAK) tl = r.range (2, 3);
        else if (tt == TR_BUILD) tl = r.range (1, 2);
        tl = std::min (tl, std::max (1, rem - 4));
        if (rem - tl < 4) { tt = TR_STOP; tl = std::max (0, std::min (1, rem - 4)); }
        if (tl <= 0) { plan.back().numBars += rem; bar += rem; rem = 0; break; }
        const SectionType stp = tt == TR_STOP ? SEC_STOP : tt == TR_HPBREAK ? SEC_HPBREAK : tt == TR_LPBREAK ? SEC_LPBREAK : SEC_BUILD;
        plan.push_back ({ stp, bar, tl });
        transTypes.push_back (tt);
        bar += tl; rem -= tl;
        // HP breaks are often followed by a short build
        if (tt == TR_HPBREAK && rem >= 6 && r.chance (0.3f))
        {
            plan.push_back ({ SEC_BUILD, bar, 1 }); transTypes.back() = TR_HPBREAK;
            bar += 1; rem -= 1;
        }
    }
    plan.push_back ({ SEC_OUTRO, bar, outroLen });
    s.sections = plan;

    // ---------------- render
    b.intro (introType, introLen, b.base);
    Groove g = b.base;
    int dropCount = 0;
    for (size_t i = 1; i + 1 < plan.size(); ++i)
    {
        const Section& sec = plan[i];
        const SectionType prev = plan[i - 1].type;
        if (sec.type == SEC_DROP)
        {
            if (dropCount > 0) g = b.makeGroove (true);
            const SectionType nx = plan[i + 1].type;
            const int nt = nx == SEC_STOP ? TR_STOP : nx == SEC_HPBREAK ? TR_HPBREAK : nx == SEC_LPBREAK ? TR_LPBREAK
                         : nx == SEC_BUILD ? TR_BUILD : -1;
            const bool afterBreak = prev != SEC_LPBREAK;   // LP breaks flow straight into the drop
            b.drop (sec.startBar, sec.numBars, nt, afterBreak, g);
            ++dropCount;
        }
        else if (sec.type == SEC_STOP)     b.stop (sec.startBar);
        else if (sec.type == SEC_HPBREAK)  b.hpBreak (sec.startBar, sec.numBars, g);
        else if (sec.type == SEC_LPBREAK)  b.lpBreak (sec.startBar, sec.numBars, g);
        else if (sec.type == SEC_BUILD)    b.build (sec.startBar, sec.numBars, g);
    }
    b.outro (outroType, plan.back().startBar, plan.back().numBars, g);

    s.sortBars();
    char nm[64];
    if (bankIndex >= 0) std::snprintf (nm, sizeof (nm), "%03d  %s", bankIndex + 1, st.name);
    else                std::snprintf (nm, sizeof (nm), "RND-%05X  %s", (unsigned) (seed & 0xFFFFF), st.name);
    s.name = nm;
    return song;
}

SongPtr generateBank (int bankIndex, int styleSel, int bars)
{
    bankIndex = std::clamp (bankIndex, 0, BANK_SIZE - 1);
    const int style = styleSel < 0 ? bankIndex % NUM_STYLES : styleSel;
    const uint32_t seed = hash32 (0xB0A7u + (uint32_t) bankIndex * 7919u + (uint32_t) (styleSel + 1) * 104729u);
    return generateSong (seed, style, bars, bankIndex);
}

SongPtr generateRandom (uint32_t seed, int styleSel, int bars)
{
    const int style = styleSel < 0 ? (int) (hash32 (seed ^ 0x51u) % NUM_STYLES) : styleSel;
    return generateSong (seed, style, bars, -1);
}
} // namespace vbm
