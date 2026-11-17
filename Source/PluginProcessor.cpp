#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
const int laneMidiNote[vbm::NUM_LANES] = { 36, 48, 38, 39, 42, 46, 45, 41, 49 };   // 808 = 48 + pitch

int laneForNote (int note)
{
    switch (note)
    {
        case 35: case 36: return vbm::KICK;
        case 37: case 45: case 47: return vbm::PERC;
        case 38: case 40: return vbm::SNARE;
        case 39: return vbm::CLAP;
        case 42: case 44: return vbm::HAT;
        case 46: return vbm::OHAT;
        case 41: case 43: return vbm::STOMP;
        case 49: case 57: return vbm::FX;
        default: return note >= 48 ? vbm::BASS : -1;
    }
}

int keyOffset (int key) { return key <= 5 ? key : key - 12; }

// ---------------------------------------------------------------- song (de)serialisation for edited patterns
void writeSong (const vbm::Song& s, OutputStream& o)
{
    o.writeInt (0x56424d31);   // 'VBM1'
    o.writeInt (s.numBars); o.writeInt (s.style); o.writeFloat (s.refBpm);
    o.writeInt ((int) s.seed); o.writeInt (s.bankIndex); o.writeString (String (s.name));
    o.writeInt ((int) s.sections.size());
    for (auto& sec : s.sections) { o.writeInt ((int) sec.type); o.writeInt (sec.startBar); o.writeInt (sec.numBars); }
    for (auto& bar : s.bars)
    {
        o.writeInt ((int) bar.size());
        for (auto& e : bar)
        {
            o.writeShort ((short) e.tick); o.writeByte ((char) e.lane); o.writeByte ((char) e.flags);
            o.writeFloat (e.vel); o.writeFloat (e.pitch); o.writeFloat (e.pitch2); o.writeInt (e.len);
        }
    }
    auto wv = [&] (const std::vector<float>& v) { for (float x : v) o.writeFloat (x); };
    wv (s.lp); wv (s.hp); wv (s.verb); wv (s.riser); wv (s.gain);
    for (auto k : s.kill) o.writeByte ((char) k);
}

std::shared_ptr<vbm::Song> readSong (InputStream& in)
{
    if (in.readInt() != 0x56424d31) return nullptr;
    auto s = std::make_shared<vbm::Song>();
    const int bars = in.readInt();
    if (bars < 1 || bars > 256) return nullptr;
    s->allocate (bars);
    s->style = in.readInt(); s->refBpm = in.readFloat();
    s->seed = (uint32_t) in.readInt(); s->bankIndex = in.readInt(); s->name = in.readString().toStdString();
    const int ns = in.readInt();
    if (ns < 0 || ns > 512) return nullptr;
    for (int i = 0; i < ns; ++i)
    {
        vbm::Section sec;
        sec.type = (vbm::SectionType) in.readInt(); sec.startBar = in.readInt(); sec.numBars = in.readInt();
        s->sections.push_back (sec);
    }
    for (auto& bar : s->bars)
    {
        const int ne = in.readInt();
        if (ne < 0 || ne > 4096) return nullptr;
        for (int i = 0; i < ne; ++i)
        {
            vbm::Event e;
            e.tick = in.readShort(); e.lane = (uint8_t) in.readByte(); e.flags = (uint8_t) in.readByte();
            e.vel = in.readFloat(); e.pitch = in.readFloat(); e.pitch2 = in.readFloat(); e.len = in.readInt();
            if (e.tick < 0 || e.tick >= vbm::TPBAR || e.lane >= vbm::NUM_LANES) return nullptr;
            bar.push_back (e);
        }
    }
    auto rv = [&] (std::vector<float>& v) { for (auto& x : v) x = in.readFloat(); };
    rv (s->lp); rv (s->hp); rv (s->verb); rv (s->riser); rv (s->gain);
    for (auto& k : s->kill) k = (uint8_t) in.readByte();
    return s;
}
} // namespace

// =====================================================================================
VbmProcessor::VbmProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VBM", createLayout())
{
    auto P = [this] (const String& id) { auto* r = apvts.getRawParameterValue (id); jassert (r != nullptr); return r; };
    pp.style = P ("style"); pp.bars = P ("bars"); pp.key = P ("key");
    pp.swing = P ("swing"); pp.humanize = P ("humanize"); pp.scDepth = P ("scDepth"); pp.scRelease = P ("scRelease");
    pp.verbSize = P ("verbSize"); pp.verbMix = P ("verbMix"); pp.filterMoves = P ("filterMoves"); pp.riser = P ("riser");
    pp.glide = P ("glide"); pp.drive = P ("drive"); pp.master = P ("master");
    pp.transitions = P ("transitions"); pp.freeRun = P ("freeRun");
    for (int l = 0; l < vbm::NUM_LANES; ++l)
    {
        pp.vol[(size_t) l] = P (laneParamId ("vol", l));   pp.tune[(size_t) l] = P (laneParamId ("tune", l));
        pp.decay[(size_t) l] = P (laneParamId ("decay", l)); pp.send[(size_t) l] = P (laneParamId ("send", l));
        pp.pan[(size_t) l] = P (laneParamId ("pan", l));   pp.mute[(size_t) l] = P (laneParamId ("mute", l));
        pp.solo[(size_t) l] = P (laneParamId ("solo", l));
    }
    formats.registerBasicFormats();
    rng.setSeedRandomly();
    engine.prepare (44100.0, 512);
    lastStyleSel = getStyleSel();
    lastBars = getBarsSetting();
    loadBankPattern (0);
    startTimerHz (10);
}

VbmProcessor::~VbmProcessor()
{
    stopTimer();
}

AudioProcessorValueTreeState::ParameterLayout VbmProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto def = vbm::defaultEngineParams();

    StringArray styles { "AUTO" };
    for (int i = 0; i < vbm::NUM_STYLES; ++i) styles.add (vbm::styleName (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", styles, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Length",
                                                         StringArray { "32 BARS", "48 BARS", "64 BARS", "96 BARS" }, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "808 Key",
                                                         StringArray { "AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    auto fl = [&p] (const char* id, const char* name, float lo, float hi, float d, float centre = -1.0f)
    {
        NormalisableRange<float> r (lo, hi);
        if (centre > lo && centre < hi) r.setSkewForCentre (centre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, d));
    };
    fl ("swing",       "Swing",          0.0f, 1.0f, def.swing);
    fl ("humanize",    "Humanize",       0.0f, 1.0f, def.humanize);
    fl ("scDepth",     "Sidechain",      0.0f, 1.0f, def.scDepth);
    fl ("scRelease",   "SC Release",     0.03f, 0.6f, def.scRelease, 0.16f);
    fl ("verbSize",    "Reverb Size",    0.0f, 1.0f, def.verbSize);
    fl ("verbMix",     "Reverb Mix",     0.0f, 1.5f, def.verbMix);
    fl ("filterMoves", "Filter Moves",   0.0f, 1.0f, def.filterMoves);
    fl ("riser",       "Riser",          0.0f, 1.0f, def.riserLevel);
    fl ("glide",       "808 Glide",      0.0f, 0.4f, def.glide, 0.08f);
    fl ("drive",       "Drive",          0.0f, 1.0f, def.drive);
    fl ("master",      "Master",        -24.0f, 6.0f, def.masterDb);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "transitions", 1 }, "Transitions", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeRun", 1 }, "Free Run", false));

    for (int l = 0; l < vbm::NUM_LANES; ++l)
    {
        const String ln (vbm::laneName (l));
        const auto& lp = def.lane[(size_t) l];
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { laneParamId ("vol", l), 1 }, ln + " Vol",
                                                            NormalisableRange<float> (-60.0f, 6.0f, 0.0f, 2.5f), lp.volDb));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { laneParamId ("tune", l), 1 }, ln + " Tune",
                                                            NormalisableRange<float> (-24.0f, 24.0f, 0.01f), lp.tune));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { laneParamId ("decay", l), 1 }, ln + " Decay",
                                                            NormalisableRange<float> (0.0f, 1.0f), lp.decay));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { laneParamId ("send", l), 1 }, ln + " Reverb",
                                                            NormalisableRange<float> (0.0f, 1.0f), lp.send));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { laneParamId ("pan", l), 1 }, ln + " Pan",
                                                            NormalisableRange<float> (-1.0f, 1.0f), lp.pan));
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { laneParamId ("mute", l), 1 }, ln + " Mute", false));
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { laneParamId ("solo", l), 1 }, ln + " Solo", false));
    }
    return { p.begin(), p.end() };
}

int VbmProcessor::getStyleSel() const
{
    return (int) pp.style->load() - 1;
}

int VbmProcessor::getBarsSetting() const
{
    static const int b[] = { 32, 48, 64, 96 };
    return b[jlimit (0, 3, (int) pp.bars->load())];
}

int VbmProcessor::getKeySemis() const
{
    const int k = (int) pp.key->load();
    if (k > 0) return k - 1;
    auto s = engine.getSong();
    return vbm::styleKey (s ? s->style : 0);
}

vbm::EngineParams VbmProcessor::buildParams() const
{
    vbm::EngineParams e = vbm::defaultEngineParams();
    e.swing = pp.swing->load(); e.humanize = pp.humanize->load();
    e.scDepth = pp.scDepth->load(); e.scRelease = pp.scRelease->load();
    e.verbSize = pp.verbSize->load(); e.verbMix = pp.verbMix->load();
    e.filterMoves = pp.filterMoves->load(); e.riserLevel = pp.riser->load();
    e.glide = pp.glide->load(); e.drive = pp.drive->load(); e.masterDb = pp.master->load();
    e.transitions = pp.transitions->load() > 0.5f;
    e.key = getKeySemis();
    for (size_t l = 0; l < (size_t) vbm::NUM_LANES; ++l)
    {
        auto& lp = e.lane[l];
        lp.volDb = pp.vol[l]->load(); lp.tune = pp.tune[l]->load();
        lp.decay = pp.decay[l]->load(); lp.send = pp.send[l]->load();
        lp.pan = pp.pan[l]->load();
        lp.mute = pp.mute[l]->load() > 0.5f; lp.solo = pp.solo[l]->load() > 0.5f;
    }
    return e;
}

bool VbmProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet().isDisabled() || layouts.getMainInputChannelSet() == out;
}

void VbmProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, jmax (512, samplesPerBlock));
    lastHostPlaying = false;
}

void VbmProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    buffer.clear();

    engine.setParams (buildParams());

    double bpm = 0.0, ppq = 0.0;
    bool hostPlay = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            hostPlay = pos->getIsPlaying();
        }
    }
    auto song = engine.getSong();
    hostHasTempo.store (bpm > 0.0);
    if (bpm <= 0.0) bpm = song ? (double) song->refBpm : 120.0;
    currentBpm.store (bpm);

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double adv = (double) n / sr * bpm / 60.0;
    const bool freeRun = pp.freeRun->load() > 0.5f;

    vbm::Transport t;
    t.bpm = bpm;
    if (hostPlay)
    {
        if (! lastHostPlaying) freeRunPpq = 0.0;
        t.playing = true;
        t.ppq = freeRun ? freeRunPpq : ppq;
        freeRunPpq += adv;
        previewPlaying.store (false);
    }
    else if (previewPlaying.load())
    {
        t.playing = true;
        t.ppq = previewPpq.load();
        previewPpq.store (t.ppq + adv);
    }
    lastHostPlaying = hostPlay;
    hostIsPlaying.store (hostPlay);

    const int key = getKeySemis();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn()) continue;
        const int lane = laneForNote (m.getNoteNumber());
        if (lane < 0) continue;
        const float pitch = lane == vbm::BASS ? (float) (m.getNoteNumber() - 48 - keyOffset (key)) : 0.0f;
        engine.noteOn (lane, m.getFloatVelocity(), pitch, meta.samplePosition);
    }
    midi.clear();

    float* L = buffer.getWritePointer (0);
    if (buffer.getNumChannels() > 1)
    {
        engine.process (L, buffer.getWritePointer (1), n, t);
    }
    else
    {
        HeapBlock<float> tmp ((size_t) n);
        engine.process (L, tmp.get(), n, t);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmp[i]);
    }
}

// =====================================================================================  patterns
void VbmProcessor::retire (std::shared_ptr<const void> p)
{
    if (p == nullptr) return;
    const ScopedLock sl (trashLock);
    trash.emplace_back (Time::getMillisecondCounter(), std::move (p));
}

void VbmProcessor::setSongInternal (vbm::SongPtr s)
{
    auto old = engine.getSong();
    engine.setSong (std::move (s));
    retire (old);
    ++songVersion;
}

void VbmProcessor::regenerate()
{
    const int styleSel = getStyleSel();
    const int bars = getBarsSetting();
    lastStyleSel = styleSel; lastBars = bars;
    setSongInternal (usingRandom ? vbm::generateRandom (randomSeed, styleSel, bars)
                                 : vbm::generateBank (bankIndex, styleSel, bars));
    songEdited = false;
}

void VbmProcessor::loadBankPattern (int index)
{
    bankIndex = ((index % vbm::BANK_SIZE) + vbm::BANK_SIZE) % vbm::BANK_SIZE;
    usingRandom = false;
    regenerate();
}

void VbmProcessor::nextPattern (int delta)
{
    loadBankPattern (bankIndex + delta);
}

void VbmProcessor::randomPattern()
{
    randomSeed = (uint32_t) rng.nextInt() ^ (uint32_t) Time::getMillisecondCounter();
    if (randomSeed == 0) randomSeed = 1;
    usingRandom = true;
    regenerate();
}

void VbmProcessor::toggleStep (int bar, int step, int lane)
{
    auto cur = engine.getSong();
    if (cur == nullptr || bar < 0 || bar >= cur->numBars || step < 0 || step >= vbm::STEPS || lane < 0 || lane >= vbm::NUM_LANES) return;
    auto s = std::make_shared<vbm::Song> (*cur);
    auto& ev = s->bars[(size_t) bar];
    const int a = step * vbm::TPS, b = a + vbm::TPS;
    const auto before = ev.size();
    ev.erase (std::remove_if (ev.begin(), ev.end(), [&] (const vbm::Event& e) {
        return e.lane == lane && e.tick >= a && e.tick < b; }), ev.end());
    if (ev.size() == before)
    {
        vbm::Event e;
        e.tick = a; e.lane = (uint8_t) lane; e.vel = 0.9f;
        e.len = lane == vbm::BASS ? 4 * vbm::TPS : vbm::TPS;
        ev.push_back (e);
        s->sortBars();
    }
    songEdited = true;
    setSongInternal (s);
}

void VbmProcessor::setPreviewPosition (int bar)
{
    previewPpq.store ((double) bar * 4.0);
}

void VbmProcessor::timerCallback()
{
    if (getStyleSel() != lastStyleSel || getBarsSetting() != lastBars)
        regenerate();

    const auto now = Time::getMillisecondCounter();
    const ScopedLock sl (trashLock);
    trash.erase (std::remove_if (trash.begin(), trash.end(), [now] (const auto& e) { return now - e.first > 3000; }), trash.end());
}

// =====================================================================================  samples
bool VbmProcessor::isAudioFile (const String& path)
{
    return File (path).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

bool VbmProcessor::loadSampleFile (int lane, const File& file)
{
    if (lane < 0 || lane >= vbm::NUM_LANES || ! file.existsAsFile()) return false;
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) return false;

    const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 20.0));
    const int ch = (int) jlimit (1u, 2u, reader->numChannels);
    AudioBuffer<float> b (ch, len);
    reader->read (&b, 0, len, 0, true, true);

    auto s = std::make_shared<vbm::SampleBuf>();
    s->sampleRate = reader->sampleRate;
    s->L.assign (b.getReadPointer (0), b.getReadPointer (0) + len);
    if (ch > 1) s->R.assign (b.getReadPointer (1), b.getReadPointer (1) + len);
    else        s->R = s->L;
    s->name = file.getFileNameWithoutExtension().toStdString();
    s->path = file.getFullPathName().toStdString();
    s->builtIn = false;

    auto old = isUserSample (lane) ? engine.getSample (lane) : nullptr;
    engine.setSample (lane, s);
    retire (old);
    samplePaths[lane] = file.getFullPathName();
    ++songVersion;
    return true;
}

void VbmProcessor::clearSample (int lane)
{
    if (lane < 0 || lane >= vbm::NUM_LANES) return;
    auto old = isUserSample (lane) ? engine.getSample (lane) : nullptr;
    engine.setSample (lane, nullptr);
    retire (old);
    samplePaths[lane].clear();
    ++songVersion;
}

String VbmProcessor::getSampleName (int lane) const
{
    auto s = engine.getSample (lane);
    return s ? String (s->name) : String();
}

// =====================================================================================  export
File VbmProcessor::renderAudioFile()
{
    auto song = engine.getSong();
    if (song == nullptr) return {};
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double bpm = hostHasTempo.load() ? currentBpm.load() : (double) song->refBpm;

    vbm::Engine off;
    off.prepare (sr, 1024);
    for (int l = 0; l < vbm::NUM_LANES; ++l)
        off.setSample (l, isUserSample (l) ? engine.getSample (l) : nullptr);
    off.setParams (buildParams());
    off.setSong (song);

    const int songLen = (int) std::ceil ((double) song->numBars * 4.0 * 60.0 / bpm * sr);
    const int total = songLen + (int) (3.0 * sr);
    AudioBuffer<float> buf (2, total);
    buf.clear();
    vbm::Transport t;
    t.bpm = bpm; t.ppq = 0.0;
    for (int pos = 0; pos < total;)
    {
        int k = jmin (1024, total - pos);
        if (pos < songLen) k = jmin (k, songLen - pos);
        t.playing = pos < songLen;
        off.process (buf.getWritePointer (0, pos), buf.getWritePointer (1, pos), k, t);
        t.ppq += (double) k / sr * bpm / 60.0;
        pos += k;
    }

    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("VONBERGMACHINE");
    dir.createDirectory();
    const String base = File::createLegalFileName (String (song->name).replace ("  ", " ").trim()).replace (" ", "_");
    auto f = dir.getChildFile ("VONBERGMACHINE_" + base + "_" + String (roundToInt (bpm)) + "bpm.wav");
    f.deleteFile();
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    if (os == nullptr) return {};
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return {};
    os.release();
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    w.reset();
    return f;
}

File VbmProcessor::writeMidiFile()
{
    auto song = engine.getSong();
    if (song == nullptr) return {};
    const double bpm = hostHasTempo.load() ? currentBpm.load() : (double) song->refBpm;
    const int key = keyOffset (getKeySemis());
    constexpr int tpq = 960, mul = tpq / vbm::TPB;

    MidiMessageSequence tempo, drums, bass;
    tempo.addEvent (MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0.0);
    tempo.addEvent (MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    drums.addEvent (MidiMessage::textMetaEvent (3, "VBM Drums"), 0.0);
    bass.addEvent (MidiMessage::textMetaEvent (3, "VBM 808"), 0.0);

    for (int b = 0; b < song->numBars; ++b)
        for (auto& e : song->bars[(size_t) b])
        {
            const double t0 = (double) ((b * vbm::TPBAR + e.tick) * mul);
            if (e.lane == vbm::BASS)
            {
                const int note = jlimit (0, 127, 48 + key + roundToInt (e.pitch));
                bass.addEvent (MidiMessage::noteOn (2, note, jlimit (0.05f, 1.0f, e.vel)), t0);
                bass.addEvent (MidiMessage::noteOff (2, note), t0 + e.len * mul - 1);
            }
            else
            {
                const int note = laneMidiNote[e.lane];
                drums.addEvent (MidiMessage::noteOn (10, note, jlimit (0.05f, 1.0f, e.vel)), t0);
                drums.addEvent (MidiMessage::noteOff (10, note), t0 + jmax (20, vbm::TPS * mul / 2));
            }
        }
    drums.updateMatchedPairs();
    bass.updateMatchedPairs();

    MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (tempo); mf.addTrack (drums); mf.addTrack (bass);

    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("VONBERGMACHINE");
    dir.createDirectory();
    const String base = File::createLegalFileName (String (song->name).replace ("  ", " ").trim()).replace (" ", "_");
    auto f = dir.getChildFile ("VONBERGMACHINE_" + base + ".mid");
    f.deleteFile();
    FileOutputStream os (f);
    if (! os.openedOk()) return {};
    mf.writeTo (os);
    os.flush();
    return f;
}

// =====================================================================================  state
void VbmProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml == nullptr) return;
    auto* ex = xml->createNewChildElement ("VBMDATA");
    ex->setAttribute ("bank", bankIndex);
    ex->setAttribute ("random", usingRandom ? 1 : 0);
    ex->setAttribute ("seed", String ((int64) randomSeed));
    for (int l = 0; l < vbm::NUM_LANES; ++l)
        if (samplePaths[l].isNotEmpty()) ex->setAttribute ("sample" + String (l), samplePaths[l]);
    if (songEdited)
        if (auto s = engine.getSong())
        {
            MemoryOutputStream mo;
            writeSong (*s, mo);
            ex->setAttribute ("song", mo.getMemoryBlock().toBase64Encoding());
        }
    copyXmlToBinary (*xml, destData);
}

void VbmProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    std::unique_ptr<XmlElement> ex;
    if (auto* e = xml->getChildByName ("VBMDATA"))
    {
        ex = std::make_unique<XmlElement> (*e);
        xml->removeChildElement (e, true);
    }
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));

    if (ex != nullptr)
    {
        bankIndex = jlimit (0, vbm::BANK_SIZE - 1, ex->getIntAttribute ("bank", 0));
        usingRandom = ex->getIntAttribute ("random", 0) != 0;
        randomSeed = (uint32_t) ex->getStringAttribute ("seed", "1").getLargeIntValue();
        for (int l = 0; l < vbm::NUM_LANES; ++l)
        {
            const String p = ex->getStringAttribute ("sample" + String (l));
            if (p.isNotEmpty() && File (p).existsAsFile()) loadSampleFile (l, File (p));
            else if (isUserSample (l)) clearSample (l);
        }
        regenerate();
        const String songB64 = ex->getStringAttribute ("song");
        if (songB64.isNotEmpty())
        {
            MemoryBlock mb;
            if (mb.fromBase64Encoding (songB64))
            {
                MemoryInputStream in (mb, false);
                if (auto s = readSong (in))
                {
                    setSongInternal (s);
                    songEdited = true;
                }
            }
        }
    }
    else
    {
        regenerate();
    }
}

AudioProcessorEditor* VbmProcessor::createEditor()
{
    return new VbmEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VbmProcessor();
}
