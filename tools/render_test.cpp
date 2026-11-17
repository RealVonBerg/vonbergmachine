// Offline test harness for the VONBERGMACHINE engine (not part of the plugin build).
//   render_test render <bankIndex|rnd:seed> <out.wav> [bars] [style]
//   render_test bank            -> validates the 500 factory patterns
#include "../Source/engine/VbmGenerator.h"
#include "../Source/engine/VbmEngine.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <map>

using namespace vbm;

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) L.size();
    const uint32_t dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lrint (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f);
        int16_t b = (int16_t) std::lrint (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

static void dumpSong (const Song& s)
{
    std::printf ("%s  bars=%d  bpm=%.0f\n", s.name.c_str(), s.numBars, s.refBpm);
    for (auto& sec : s.sections) std::printf ("  %-9s bar %3d  x%d\n", sectionName (sec.type), sec.startBar, sec.numBars);
}

static void printBar (const Song& s, int bar)
{
    std::printf ("bar %d\n", bar);
    for (int l = 0; l < NUM_LANES; ++l)
    {
        char row[49]; std::memset (row, '.', 48); row[48] = 0;
        for (auto& e : s.bars[(size_t) bar]) if (e.lane == l) row[e.tick / 4] = e.vel > 0.9f ? 'X' : (e.vel > 0.6f ? 'x' : 'g');
        std::printf ("  %-8s %s\n", laneName (l), row);
    }
}

int main (int argc, char** argv)
{
    if (argc >= 2 && std::strcmp (argv[1], "bank") == 0)
    {
        std::set<std::string> hashes;
        std::map<int, int> secCount;
        int total = 0, minEv = 1 << 30, maxEv = 0;
        for (int sel = -1; sel < NUM_STYLES; ++sel)
            for (int i = 0; i < BANK_SIZE; ++i)
            {
                auto s = generateBank (i, sel, 64);
                std::string h;
                int ev = 0;
                for (int b = 0; b < s->numBars; ++b)
                    for (auto& e : s->bars[(size_t) b])
                    {
                        h += std::to_string (b * 192 + e.tick) + ":" + std::to_string (e.lane) + ":" + std::to_string ((int) (e.pitch * 10)) + ",";
                        ++ev;
                        if (e.tick < 0 || e.tick >= TPBAR || e.lane >= NUM_LANES) { std::printf ("BAD EVENT %d\n", i); return 1; }
                    }
                int sumBars = 0;
                for (auto& sec : s->sections) { sumBars += sec.numBars; secCount[sec.type]++; }
                if (sumBars != s->numBars) { std::printf ("BAD PLAN %d sel %d (%d)\n", i, sel, sumBars); dumpSong (*s); return 1; }
                hashes.insert (h);
                minEv = std::min (minEv, ev); maxEv = std::max (maxEv, ev);
                ++total;
            }
        std::printf ("patterns=%d unique=%zu events/pattern min=%d max=%d\n", total, hashes.size(), minEv, maxEv);
        for (auto& kv : secCount) std::printf ("  %-9s %d\n", sectionName (kv.first), kv.second);
        return 0;
    }

    if (argc < 3) { std::printf ("usage\n"); return 1; }
    const int bars = argc > 3 ? std::atoi (argv[3]) : 64;
    const int style = argc > 4 ? std::atoi (argv[4]) : -1;
    SongPtr s;
    if (std::strncmp (argv[1], "rnd:", 4) == 0) s = generateRandom ((uint32_t) std::strtoul (argv[1] + 4, nullptr, 10), style, bars);
    else s = generateBank (std::atoi (argv[1]), style, bars);
    dumpSong (*s);
    const char* showBar = std::getenv ("SHOWBARS");
    if (showBar) { int a = 0, b = 0; std::sscanf (showBar, "%d-%d", &a, &b); for (int k = a; k <= b && k < s->numBars; ++k) printBar (*s, k); }

    const int sr = 44100;
    Engine eng;
    eng.prepare (sr, 512);
    eng.setSong (s);
    {
        EngineParams ep = defaultEngineParams();
        if (const char* so = std::getenv ("SOLO")) { int l = std::atoi (so); for (int k = 0; k < NUM_LANES; ++k) ep.lane[(size_t) k].mute = (k != l); }
        if (const char* dr = std::getenv ("DRIVE")) ep.drive = (float) std::atof (dr);
        if (const char* nt = std::getenv ("NOTRANS")) ep.transitions = false;
        eng.setParams (ep);
    }
    Transport t; t.playing = true; t.bpm = s->refBpm; t.ppq = 0;
    const double songSec = s->numBars * 4 * 60.0 / t.bpm;
    const int total = (int) ((songSec + 2.0) * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int k = std::min (512, total - pos);
        t.playing = pos < (int) (songSec * sr);
        eng.process (L.data() + pos, R.data() + pos, k, t);
        t.ppq += (double) k / sr * t.bpm / 60.0;
    }
    writeWav (argv[2], L, R, sr);
    return 0;
}
