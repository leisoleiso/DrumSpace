#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>
#include <set>
#include <memory>

#include "ModelInterface.h"
#include "DrumMap.h"

// DrumSpace is a pure MIDI generator: it outputs generated drum patterns as
// MIDI (so FL can route them to a drum synth) and captures incoming MIDI (FL
// piano roll) into a selected node.  No built-in audio synth.
class DrumSpaceAudioProcessor : public juce::AudioProcessor {
public:
    DrumSpaceAudioProcessor();
    ~DrumSpaceAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- model-facing API (called from the editor) ---
    void setResult(const std::vector<midispace::NoteEvent>& notes, double bpm = 120.0);
    void play();
    void stop();
    bool isPlaying() const;
    bool saveResultToFile(const juce::File& file);

    void startCapture();
    std::vector<midispace::NoteEvent> stopCapture();
    bool isCapturing() const { return capturing_; }
    double getBpm() const;

    // Node-state persistence (XML string; the editor builds/parses it).
    void setNodeStateXml(const juce::String& xml) { nodeStateXml_ = xml; }
    juce::String getNodeStateXml() const { return nodeStateXml_; }

    // Drum key mapping: GM pitch <-> the host drum synth's key layout.
    void setDrumMap(const midispace::DrumMap& m) { juce::ScopedLock sl(lock_); drumMap_ = m; }
    midispace::DrumMap getDrumMap() const { juce::ScopedLock sl(lock_); return drumMap_; }

private:
    juce::CriticalSection lock_;
    juce::MidiMessageSequence midiSeq_;        // currently-playing loop
    juce::MidiMessageSequence pendingSeq_;     // next loop, waits for the current note to end
    double loopLenSec_ = 0.0;
    double pendingLoopLenSec_ = 0.0;
    double posSec_ = 0.0;
    double bpm_ = 120.0;
    bool hasPending_ = false;
    std::set<int> activeNotes_;                // notes currently sounding
    std::atomic<bool> playing_{ false };
    std::atomic<bool> sendAllNotesOff_{ false };

    std::atomic<bool> capturing_{ false };
    juce::CriticalSection captureLock_;
    struct CaptureEvent { double timeSec; bool noteOn; int pitch; };
    std::vector<CaptureEvent> captureEvents_;
    double captureTime_ = 0.0;
    double captureMaxSeconds_ = 4.0;
    double firstNoteTime_ = -1.0;

    juce::String nodeStateXml_;
    midispace::DrumMap drumMap_;

    // Local inference server (PyInstaller exe), auto-launched on load.
    std::unique_ptr<juce::ChildProcess> serverProcess_;
    bool serverChecked_ = false;
    void ensureServerRunning();
    void stopServer();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSpaceAudioProcessor)
};
