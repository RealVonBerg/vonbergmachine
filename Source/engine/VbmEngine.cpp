// VONBERGMACHINE - playback engine
#include "VbmEngine.h"
#include "VbmKit.h"

namespace vbm
{
EngineParams defaultEngineParams()
{
    EngineParams p;
    //                      vol    tune decay send  pan
    p.lane[KICK]  = { -7.0f, 0.0f, 1.0f, 0.00f, 0.0f };
    p.lane[BASS]  = { -2.0f, 0.0f, 0.35f, 0.00f, 0.0f };
    p.lane[SNARE] = { -9.0f, 0.0f, 1.0f, 0.10f, 0.0f };
    p.lane[CLAP]  = { -8.0f, 0.0f, 1.0f, 0.12f, 0.0f };
    p.lane[HAT]   = { -5.0f, 0.0f, 1.0f, 0.02f, 0.08f };
    p.lane[OHAT]  = { -8.0f, 0.0f, 1.0f, 0.06f, -0.1f };
    p.lane[PERC]  = { -12.0f, 0.0f, 1.0f, 0.06f, 0.0f };
    p.lane[STOMP] = { -7.0f, 0.0f, 1.0f, 0.28f, 0.0f };
    p.lane[FX]    = { -9.0f, 0.0f, 1.0f, 0.30f, 0.0f };
    return p;
}

Engine::Engine()
{
    params = defaultEngineParams();
    for (auto& h : uiHits) h.store (0);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    const bool srChanged = std::abs (sampleRate - sr) > 0.5 || defaults[0] == nullptr;
    sr = sampleRate;
    maxBlock = std::max (32, maxBlockSize);
    if (srChanged)
        for (int l = 0; l < NUM_LANES; ++l)
            std::atomic_store (&defaults[(size_t) l], makeDefaultSample (l, sr));
    for (auto& b : laneBuf) b.assign ((size_t) maxBlock, 0.0f);
    sched.reserve (1024);
    pending.reserve (256);
    reverb.prepare (sr);
    curSize = -1.0f;
    ctlCoef  = std::exp (-1.0f / (0.006f * (float) sr));
    gainCoef = std::exp (-1.0f / (0.0025f * (float) sr));
    killLen = std::max (1, (int) (0.004 * sr));
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) { v.active = false; v.smp.reset(); }
    reverb.clear();
    for (auto& f : hpF) f.reset();
    for (auto& f : lpF) f.reset();
    riserL.reset(); riserR.reset();
    scEnv = 0; scAttack = 0; killLeft = 0;
    wasPlaying = false; expectedTick = -1.0;
    lpS = lpT = 1.0f; hpS = hpT = 0.0f; verbS = verbT = 1.0f; riserS = riserT = 0.0f; gainS = gainT = 1.0f;
    pending.clear();
}

void Engine::setSample (int lane, SamplePtr s)
{
    if (lane < 0 || lane >= NUM_LANES) return;
    std::atomic_store (&user[(size_t) lane], std::move (s));
}

SamplePtr Engine::getSample (int lane) const
{
    auto u = std::atomic_load (&user[(size_t) lane]);
    return u ? u : std::atomic_load (&defaults[(size_t) lane]);
}

void Engine::noteOn (int lane, float vel, float pitch, int sampleOffset)
{
    if (lane < 0 || lane >= NUM_LANES || pending.size() >= 250) return;
    pending.push_back ({ std::max (0, sampleOffset), lane, vel, pitch, pitch, lane == BASS ? TPB * 2 : TPS, 0, false });
}

float Engine::automation (const std::vector<float>& v, double tick, bool interp) const
{
    const int n = (int) v.size();
    if (n == 0) return 0.0f;
    const double st = tick / (double) TPS;
    int i0 = (int) std::floor (st);
    const float fr = (float) (st - i0);
    i0 = ((i0 % n) + n) % n;
    if (! interp) return v[(size_t) i0];
    const int i1 = (i0 + 1) % n;
    return v[(size_t) i0] + (v[(size_t) i1] - v[(size_t) i0]) * fr;
}

// ------------------------------------------------------------------ scheduling
void Engine::collect (const Song& s, double t0, double tps, int n)
{
    const double songLen = (double) s.numBars * TPBAR;
    double a = t0, remaining = (double) n * tps, base = 0.0;
    const float swingShift = params.swing * 5.0f;
    const float hum = params.humanize;
    int guard = 0;
    while (remaining > 1e-9 && guard++ < 4)
    {
        const double segEnd = std::min (songLen, a + remaining);
        const int barA = std::max (0, (int) std::floor (a / TPBAR) - 1);
        const int barB = std::min (s.numBars - 1, (int) std::floor (segEnd / TPBAR));
        for (int bar = barA; bar <= barB; ++bar)
        {
            const auto& evs = s.bars[(size_t) bar];
            for (size_t i = 0; i < evs.size(); ++i)
            {
                const Event& e = evs[i];
                double et = (double) bar * TPBAR + e.tick;
                if (e.tick % (2 * TPS) == TPS) et += swingShift;
                const uint32_t h = hash32 ((uint32_t) bar * 977u + (uint32_t) i * 131u + s.seed);
                const float u1 = (float) (h & 0xffff) / 65535.0f, u2 = (float) (h >> 16) / 65535.0f;
                if (e.lane != BASS) et += hum * 1.6 * u1;
                if (et >= a && et < segEnd)
                {
                    float vel = e.vel;
                    if (e.lane != BASS && e.lane != FX && e.tick % TPB != 0) vel *= 1.0f - hum * 0.22f * u2;
                    sched.push_back ({ (int) (base + (et - a) / tps), e.lane, vel, e.pitch, e.pitch2, e.len, e.flags, false });
                }
            }
        }
        // reverb kills on step boundaries
        for (int k = (int) std::ceil (a / TPS); (double) k * TPS < segEnd; ++k)
            if (k >= 0 && k < (int) s.kill.size() && s.kill[(size_t) k])
                sched.push_back ({ (int) (base + ((double) k * TPS - a) / tps), -1, 0, 0, 0, 0, 0, true });

        base += (segEnd - a) / tps;
        remaining -= (segEnd - a);
        a = 0.0;
    }
}

void Engine::updateRate (Voice& v, int lane)
{
    float semis = v.semis + params.lane[(size_t) lane].tune;
    if (lane == BASS) semis += (float) (params.key <= 5 ? params.key : params.key - 12);
    v.rate = v.baseRate * std::exp2 (semis / 12.0f);
}

void Engine::startVoice (int lane, float vel, float pitch, float pitch2, int lenTicks, uint8_t flags, double spt)
{
    SamplePtr smp = getSample (lane);
    if (! smp || smp->length() < 2) return;
    const LaneParams& lp = params.lane[(size_t) lane];
    const float glideSamples = std::max (1.0f, params.glide * (float) sr);

    if (lane == BASS)
    {
        // mono 808 with legato glide
        Voice* cur = nullptr;
        for (auto& v : voices)
            if (v.active && v.lane == BASS && ! v.releasing && v.fadeStep == 0.0f) { cur = &v; break; }
        if (cur != nullptr && (flags & EV_GLIDE))
        {
            cur->target = pitch;
            cur->glideCoef = 1.0f - std::exp (-4.0f / glideSamples);
            cur->gate = (int) (lenTicks * spt);
            cur->slopeLeft = (int) (lenTicks * spt);
            cur->slope = (pitch2 - pitch) / (float) std::max (1, cur->slopeLeft);
            cur->gain = std::max (cur->gain, vel);
            return;
        }
        for (auto& v : voices)
            if (v.active && v.lane == BASS && v.fadeStep == 0.0f) v.fadeStep = 1.0f / (0.003f * (float) sr);
    }
    else if (lane == KICK || lane == HAT || lane == OHAT || lane == PERC || lane == SNARE || lane == CLAP || lane == STOMP || lane == FX)
    {
        int maxPoly = (lane == KICK) ? 1 : (lane == HAT ? 2 : 3);
        // choke: closed hat chokes open hat
        if (lane == HAT)
            for (auto& v : voices)
                if (v.active && v.lane == OHAT && v.fadeStep == 0.0f) v.fadeStep = 1.0f / (0.012f * (float) sr);
        int count = 0; Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.active && v.lane == lane && v.fadeStep == 0.0f)
            {
                ++count;
                if (oldest == nullptr || v.age < oldest->age) oldest = &v;
            }
        if (count >= maxPoly && oldest != nullptr) oldest->fadeStep = 1.0f / (0.004f * (float) sr);
    }

    // allocate
    Voice* nv = nullptr;
    for (auto& v : voices) if (! v.active) { nv = &v; break; }
    if (nv == nullptr)
    {
        for (auto& v : voices) if (nv == nullptr || v.age < nv->age) nv = &v;
    }
    Voice& v = *nv;
    v = Voice();
    v.active = true;
    v.lane = lane;
    v.smp = smp;
    v.pos = 0.0;
    v.baseRate = smp->sampleRate / sr;
    v.semis = v.target = pitch;
    v.slopeLeft = (int) (lenTicks * spt);
    v.slope = (pitch2 - pitch) / (float) std::max (1, v.slopeLeft);
    v.glideCoef = 1.0f;
    v.gain = std::pow (std::clamp (vel, 0.0f, 1.0f), 1.15f);
    v.age = ++ageCounter;
    v.attStep = 1.0f / std::max (1.0f, 0.0004f * (float) sr);
    v.att = smp->builtIn ? 1.0f : 0.0f;

    if (lane == BASS)
    {
        v.gate = (int) (lenTicks * spt);
        const float relT = 0.025f + lp.decay * lp.decay * 0.9f;
        v.relMul = std::exp (-1.0f / (relT * (float) sr));
    }
    else
    {
        if (lp.decay < 0.995f)
        {
            const double natural = smp->length() / (v.baseRate * std::exp2 ((pitch + lp.tune) / 12.0f));
            v.gate = std::max ((int) (0.012 * sr), (int) (natural * (0.04 + 0.96 * lp.decay * lp.decay)));
        }
        v.relMul = std::exp (-1.0f / (0.03f * (float) sr));
    }
    updateRate (v, lane);

    if (lane == KICK || lane == STOMP)
    {
        scAttack = std::max (1, (int) (0.0015 * sr));
        scDisplay = 1.0f;
    }
    uiHits[(size_t) lane].fetch_add (1);
}

void Engine::fire (const Sched& e, double spt)
{
    if (e.kill)
    {
        if (params.transitions) killLeft = killLen;
        return;
    }
    startVoice (e.lane, e.vel, e.pitch, e.pitch2, e.len, e.flags, spt);
}

void Engine::renderVoices (int from, int to)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const SampleBuf& s = *v.smp;
        const int len = s.length();
        float* bl = laneBuf[(size_t) v.lane * 2].data();
        float* br = laneBuf[(size_t) v.lane * 2 + 1].data();
        const float* sl = s.L.data();
        const float* sr2 = s.R.empty() ? sl : s.R.data();
        for (int i = from; i < to; ++i)
        {
            const int ip = (int) v.pos;
            if (ip >= len - 1) { v.active = false; break; }
            const float fr = (float) (v.pos - ip);
            float a = sl[ip] + (sl[ip + 1] - sl[ip]) * fr;
            float b = sr2[ip] + (sr2[ip + 1] - sr2[ip]) * fr;

            float env = v.gain;
            if (v.att < 1.0f) { v.att = std::min (1.0f, v.att + v.attStep); env *= v.att; }
            if (v.gate <= 0 && ! v.releasing) v.releasing = true;
            else --v.gate;
            if (v.releasing) { v.rel *= v.relMul; env *= v.rel; if (v.rel < 0.0003f) { v.active = false; break; } }
            if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) { v.active = false; break; } env *= v.fade; }

            bl[i] += a * env;
            br[i] += b * env;

            // pitch movement (dives, slides, glides)
            if (v.slopeLeft > 0) { v.target += v.slope; --v.slopeLeft; }
            if (v.semis != v.target)
            {
                v.semis += (v.target - v.semis) * v.glideCoef;
                if (std::abs (v.target - v.semis) < 0.001f) v.semis = v.target;
            }
            if (++v.rateCounter >= 8) { v.rateCounter = 0; updateRate (v, v.lane); }
            v.pos += v.rate;
        }
        if (! v.active) v.smp.reset();
    }
}

// ------------------------------------------------------------------ main process
void Engine::process (float* outL, float* outR, int n, const Transport& t)
{
    if (n > maxBlock)
    {
        // split oversize blocks
        int done = 0;
        Transport tt = t;
        while (done < n)
        {
            const int k = std::min (maxBlock, n - done);
            process (outL + done, outR + done, k, tt);
            tt.ppq += (double) k / sr * tt.bpm / 60.0;
            done += k;
        }
        return;
    }

    for (auto& b : laneBuf) std::fill (b.begin(), b.begin() + n, 0.0f);
    sched.clear();

    if (params.verbSize != curSize) { curSize = params.verbSize; reverb.setSize (curSize); }

    SongPtr sp = getSong();
    const double bpm = std::clamp (t.bpm, 20.0, 400.0);
    const double tps = bpm / 60.0 * TPB / sr;            // ticks per sample
    const double spt = 1.0 / tps;
    double t0 = 0.0;
    const bool playing = t.playing && sp != nullptr && t.ppq >= 0.0;

    if (playing)
    {
        const double songLen = (double) sp->numBars * TPBAR;
        t0 = std::fmod (t.ppq * TPB, songLen);
        if (t0 < 0) t0 += songLen;
        const bool jumped = ! wasPlaying || expectedTick < 0.0
                            || (std::abs (t0 - expectedTick) > 1.5 && std::abs (std::abs (t0 - expectedTick) - songLen) > 1.5);
        if (jumped && wasPlaying)
            for (auto& v : voices) if (v.active && v.fadeStep == 0.0f) v.fadeStep = 1.0f / (0.006f * (float) sr);
        collect (*sp, t0, tps, n);
        expectedTick = std::fmod (t0 + n * tps, songLen);
        uiTick.store (t0);
    }
    else
    {
        if (wasPlaying)
        {
            // transport stopped: let tails ring but close 808 gates
            for (auto& v : voices) if (v.active && v.lane == BASS) v.gate = std::min (v.gate, 0);
        }
        expectedTick = -1.0;
        uiTick.store (-1.0);
    }
    wasPlaying = playing;

    for (auto& p : pending) { Sched s2 = p; s2.offset = std::min (s2.offset, n - 1); sched.push_back (s2); }
    pending.clear();
    std::stable_sort (sched.begin(), sched.end(), [] (const Sched& a, const Sched& b) { return a.offset < b.offset; });

    // lane gains
    bool anySolo = false;
    for (auto& l : params.lane) anySolo |= l.solo;
    float gL[NUM_LANES], gR[NUM_LANES], snd[NUM_LANES];
    for (int l = 0; l < NUM_LANES; ++l)
    {
        const LaneParams& lp = params.lane[(size_t) l];
        const bool audible = ! lp.mute && (! anySolo || lp.solo);
        const float g = audible ? dbToGain (lp.volDb) : 0.0f;
        const float ang = (std::clamp (lp.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * PI_F;
        gL[l] = g * std::cos (ang) * 1.41421f;
        gR[l] = g * std::sin (ang) * 1.41421f;
        snd[l] = g * lp.send;
    }

    const float moves = params.transitions ? params.filterMoves : 0.0f;
    const float driveG = dbToGain (params.drive * 15.0f);
    const float outG = dbToGain (params.masterDb);
    const float scRelMul = std::exp (-1.0f / (std::max (0.02f, params.scRelease) * (float) sr));
    const float wetLevel = params.verbMix;
    float peak = 0.0f;

    int i = 0; size_t ei = 0;
    while (i < n)
    {
        while (ei < sched.size() && sched[ei].offset <= i) fire (sched[ei++], spt);
        int next = n;
        if (ei < sched.size()) next = std::min (next, sched[ei].offset);
        int end = std::min (next, i + 32);
        if (end <= i) end = i + 1;

        // automation targets for this chunk
        if (playing && params.transitions)
        {
            const double songLen = (double) sp->numBars * TPBAR;
            double tick = std::fmod (t0 + i * tps, songLen);
            lpT    = 1.0f - (1.0f - automation (sp->lp, tick, true)) * moves;
            hpT    = automation (sp->hp, tick, true) * moves;
            verbT  = automation (sp->verb, tick, true);
            riserT = automation (sp->riser, tick, true) * params.riserLevel;
            gainT  = automation (sp->gain, tick, false);
        }
        else if (! playing)
        {
            lpT = 1.0f; hpT = 0.0f; verbT = 1.0f; riserT = 0.0f; gainT = 1.0f;
        }
        else
        {
            lpT = 1.0f; hpT = 0.0f; verbT = 1.0f; riserT = 0.0f; gainT = 1.0f;
        }

        renderVoices (i, end);

        // filter coefficients from smoothed control values (updated per chunk)
        const float cc = std::pow (ctlCoef, (float) (end - i));
        lpS = lpT + (lpS - lpT) * cc;
        hpS = hpT + (hpS - hpT) * cc;
        riserS = riserT + (riserS - riserT) * cc;
        verbS = verbT + (verbS - verbT) * cc;
        const float lpHz = std::min (lpNormToHz (lpS), (float) sr * 0.45f);
        const float hpHz = hpNormToHz (hpS);
        for (int c = 0; c < 2; ++c)
        {
            lpF[(size_t) c * 2].set (lpHz, 0.54f, (float) sr);
            lpF[(size_t) c * 2 + 1].set (lpHz, 1.3f, (float) sr);
            hpF[(size_t) c * 2].set (hpHz, 0.54f, (float) sr);
            hpF[(size_t) c * 2 + 1].set (hpHz, 1.2f, (float) sr);
        }
        const bool lpOn = lpS < 0.995f, hpOn = hpS > 0.004f;
        const float riserHz = 250.0f * std::exp2 (riserS * 6.2f);
        riserL.set (riserHz, 1.6f, (float) sr); riserR.set (riserHz * 1.03f, 1.6f, (float) sr);
        const float riserAmp = 0.22f * std::pow (riserS, 2.2f);

        for (int k = i; k < end; ++k)
        {
            // sidechain envelope (kick / stomp duck the 808)
            if (scAttack > 0) { scEnv += (1.0f - scEnv) / (float) scAttack; --scAttack; if (scAttack == 0) scEnv = 1.0f; }
            else scEnv *= scRelMul;
            const float duck = 1.0f - params.scDepth * scEnv * scEnv * (3.0f - 2.0f * scEnv);

            float dl = 0, dr = 0, sl = 0;
            for (int l = 0; l < NUM_LANES; ++l)
            {
                float a = laneBuf[(size_t) l * 2][(size_t) k], b = laneBuf[(size_t) l * 2 + 1][(size_t) k];
                if (l == BASS) { a *= duck; b *= duck; }
                dl += a * gL[l]; dr += b * gR[l];
                sl += (a + b) * 0.5f * snd[l];
            }

            // riser
            if (riserAmp > 1e-5f)
            {
                nzL ^= nzL << 13; nzL ^= nzL >> 17; nzL ^= nzL << 5;
                nzR ^= nzR << 13; nzR ^= nzR >> 17; nzR ^= nzR << 5;
                const float rl = riserL.bp ((float) (int32_t) nzL * 4.6e-10f) * riserAmp;
                const float rr = riserR.bp ((float) (int32_t) nzR * 4.6e-10f) * riserAmp;
                dl += rl; dr += rr; sl += (rl + rr) * 0.3f;
            }

            // reverb with tail-kill on drops
            float wl = 0, wr = 0;
            reverb.process (sl * verbS, sl * verbS, wl, wr);
            float wg = wetLevel * (1.0f - 0.35f * params.scDepth * scEnv);
            if (killLeft > 0)
            {
                wg *= (float) killLeft / (float) killLen;
                if (--killLeft == 0) reverb.clear();
            }
            float L = dl + wl * wg, R = dr + wr * wg;

            if (hpOn)
            {
                L = hpF[1].hp (hpF[0].hp (L));
                R = hpF[3].hp (hpF[2].hp (R));
            }
            if (lpOn)
            {
                L = lpF[1].lp (lpF[0].lp (L));
                R = lpF[3].lp (lpF[2].lp (R));
            }

            gainS = gainT + (gainS - gainT) * gainCoef;
            L *= gainS; R *= gainS;

            L = softClip (dcL.tick (L) * driveG) * outG;
            R = softClip (dcR.tick (R) * driveG) * outG;
            L = std::clamp (L, -0.995f, 0.995f);
            R = std::clamp (R, -0.995f, 0.995f);
            outL[k] = L; outR[k] = R;
            peak = std::max (peak, std::max (std::abs (L), std::abs (R)));
        }
        i = end;
    }
    // keep filter states clean when bypassed
    if (lpS >= 0.995f) for (auto& f : lpF) f.reset();
    if (hpS <= 0.004f) for (auto& f : hpF) f.reset();

    uiDuck.store (1.0f - params.scDepth * scEnv);
    uiPeak.store (peak);
    curTick = t0; curTps = tps;
}
} // namespace vbm
