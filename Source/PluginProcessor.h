#pragma once
#include <JuceHeader.h>
#include "engine/VbmEngine.h"
#include "engine/VbmGenerator.h"

class VbmProcessor : public juce::AudioProcessor,
                     private juce::Timer
{
public:
    VbmProcessor();
    ~VbmProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VONBERGMACHINE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------ patterns (message thread)
    void loadBankPattern (int index);
    void nextPattern (int delta);
    void randomPattern();
    void regenerate();                       // same pattern, current style / length
    int  getBankIndex() const   { return bankIndex; }
    bool isRandomPattern() const { return usingRandom; }
    vbm::SongPtr getSong() const { return engine.getSong(); }
    int  getStyleSel() const;                // -1 auto, 0..4
    int  getBarsSetting() const;
    int  getKeySemis() const;                // effective key 0..11
    void toggleStep (int bar, int step, int lane);
    std::atomic<int> songVersion { 0 };

    // ------------------------------------------------------------ samples
    bool loadSampleFile (int lane, const juce::File& file);
    void clearSample (int lane);
    juce::String getSampleName (int lane) const;
    bool isUserSample (int lane) const { return ! engine.isDefaultSample (lane); }
    vbm::SamplePtr getSampleData (int lane) const { return engine.getSample (lane); }
    static bool isAudioFile (const juce::String& path);

    // ------------------------------------------------------------ export
    juce::File renderAudioFile();
    juce::File writeMidiFile();

    // ------------------------------------------------------------ transport / UI
    std::atomic<bool>   previewPlaying { false };
    std::atomic<double> previewPpq { 0.0 };
    std::atomic<bool>   hostIsPlaying { false };
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<bool>   hostHasTempo { false };
    void setPreviewPosition (int bar);

    vbm::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    static juce::String laneParamId (const char* name, int lane) { return juce::String (name) + juce::String (lane); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    vbm::EngineParams buildParams() const;
    void setSongInternal (vbm::SongPtr s);
    void timerCallback() override;

    struct ParamPtrs
    {
        std::atomic<float> *style = nullptr, *bars = nullptr, *key = nullptr, *swing = nullptr, *humanize = nullptr,
                           *scDepth = nullptr, *scRelease = nullptr, *verbSize = nullptr, *verbMix = nullptr,
                           *filterMoves = nullptr, *riser = nullptr, *glide = nullptr, *drive = nullptr,
                           *master = nullptr, *transitions = nullptr, *freeRun = nullptr;
        std::array<std::atomic<float>*, vbm::NUM_LANES> vol {}, tune {}, decay {}, send {}, pan {}, mute {}, solo {};
    } pp;

    juce::AudioFormatManager formats;
    juce::String samplePaths[vbm::NUM_LANES];

    int bankIndex = 0;
    bool usingRandom = false;
    uint32_t randomSeed = 1;
    int lastStyleSel = -99, lastBars = -1;
    bool songEdited = false;

    // objects released on the message thread only (never on the audio thread)
    std::vector<std::pair<juce::uint32, std::shared_ptr<const void>>> trash;
    juce::CriticalSection trashLock;
    void retire (std::shared_ptr<const void> p);

    // free-run transport
    bool lastHostPlaying = false;
    double freeRunPpq = 0.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbmProcessor)
};
