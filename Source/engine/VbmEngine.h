// VONBERGMACHINE - playback engine (no JUCE dependency)
#pragma once
#include "VbmCore.h"
#include "VbmDsp.h"
#include <atomic>

namespace vbm
{
struct LaneParams
{
    float volDb = -6.0f;   // -60..+6
    float tune  = 0.0f;    // semitones
    float decay = 1.0f;    // 0..1 (1 = natural length; 808: release time)
    float send  = 0.0f;    // reverb send 0..1
    float pan   = 0.0f;    // -1..1
    bool  mute  = false;
    bool  solo  = false;
};

struct EngineParams
{
    std::array<LaneParams, NUM_LANES> lane;
    float swing       = 0.0f;   // 0..1
    float humanize    = 0.25f;  // 0..1
    float scDepth     = 0.75f;  // sidechain depth 0..1
    float scRelease   = 0.16f;  // seconds
    float verbSize    = 0.45f;  // 0..1
    float verbMix     = 1.0f;   // 0..1.5 return level
    float filterMoves = 1.0f;   // 0..1 amount of automatic LP/HP moves
    float drive       = 0.2f;   // 0..1
    float masterDb    = -2.0f;
    float glide       = 0.07f;  // 808 portamento seconds
    float riserLevel  = 0.6f;   // 0..1
    int   key         = 0;      // 0 = C .. 11 = B
    bool  transitions = true;   // automatic filters / reverb throws / risers / cuts
};

struct Transport
{
    bool   playing = false;
    double ppq     = 0.0;    // position in quarter notes
    double bpm     = 120.0;
};

EngineParams defaultEngineParams();

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void     setSong (SongPtr s)        { std::atomic_store (&song, std::move (s)); }
    SongPtr  getSong() const            { return std::atomic_load (&song); }
    void     setSample (int lane, SamplePtr s);          // nullptr -> built-in default
    SamplePtr getSample (int lane) const;
    bool     isDefaultSample (int lane) const { return std::atomic_load (&user[(size_t) lane]) == nullptr; }

    void setParams (const EngineParams& p) { params = p; }
    const EngineParams& getParams() const  { return params; }

    // live trigger (MIDI) - call from the audio thread before process()
    void noteOn (int lane, float vel, float pitch, int sampleOffset);

    void process (float* outL, float* outR, int numSamples, const Transport& t);

    double getSampleRate() const { return sr; }

    // UI feedback (read from the message thread)
    std::atomic<double>   uiTick { -1.0 };
    std::atomic<uint32_t> uiHits[NUM_LANES];
    std::atomic<float>    uiDuck { 0.0f };
    std::atomic<float>    uiPeak { 0.0f };

private:
    struct Voice
    {
        bool active = false;
        int lane = 0;
        SamplePtr smp;
        double pos = 0, baseRate = 1, rate = 1;
        float semis = 0, target = 0, slope = 0; int slopeLeft = 0;
        float glideCoef = 1.0f;
        float gain = 1.0f;
        int gate = 0x7fffffff;
        float rel = 1.0f, relMul = 1.0f; bool releasing = false;
        float fade = 1.0f, fadeStep = 0.0f;      // choke fade-out
        float att = 0.0f, attStep = 1.0f;
        uint32_t age = 0;
        int rateCounter = 0;
    };

    struct Sched { int offset; int lane; float vel, pitch, pitch2; int len; uint8_t flags; bool kill; };

    void collect (const Song& s, double t0, double tps, int n);
    void fire (const Sched& e, double samplesPerTick);
    void startVoice (int lane, float vel, float pitch, float pitch2, int lenTicks, uint8_t flags, double spt);
    void renderVoices (int from, int to);
    void updateRate (Voice& v, int lane);
    float automation (const std::vector<float>& v, double tick, bool interp) const;

    double sr = 44100.0;
    int maxBlock = 512;
    EngineParams params;
    SongPtr song;
    std::array<SamplePtr, NUM_LANES> defaults;
    std::array<SamplePtr, NUM_LANES> user;

    std::array<Voice, 48> voices;
    uint32_t ageCounter = 0;
    std::vector<Sched> sched, pending;
    std::array<std::vector<float>, NUM_LANES * 2> laneBuf;

    // state
    bool wasPlaying = false;
    double expectedTick = -1.0;
    float scEnv = 0.0f; int scAttack = 0;
    FdnReverb reverb;
    int killLeft = 0, killLen = 1;
    std::array<Svf, 4> hpF, lpF;    // 2 x 2-pole per channel
    float lpS = 1.0f, hpS = 0.0f, verbS = 1.0f, riserS = 0.0f, gainS = 1.0f;
    float lpT = 1.0f, hpT = 0.0f, verbT = 1.0f, riserT = 0.0f, gainT = 1.0f;
    float ctlCoef = 0.99f, gainCoef = 0.99f;
    Svf riserL, riserR;
    uint32_t nzL = 0x1234567u, nzR = 0x7654321u;
    DcBlock dcL, dcR;
    float curSize = -1.0f;
    double curTick = 0.0, curTps = 0.0;
    float scDisplay = 0.0f;
};
} // namespace vbm
