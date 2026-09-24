#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>

#include "CanvasComponent.h"
#include "DrumPatterns.h"
#include "GenerationManager.h"
#include "HttpModelInterface.h"
#include "MusicalControls.h"

class DrumSpaceAudioProcessor;

class DrumSpaceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit DrumSpaceAudioProcessorEditor(DrumSpaceAudioProcessor& p);
    ~DrumSpaceAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void startLatentLoad();
    void handleBallReleased();
    void regenerate();
    void addResultAsNode();
    void addNewNode();
    void deleteSelectedNode();
    void saveResult();
    void startCapture();
    void finishCapture();
    void applyPreset();
    void pushNodeState();
    void applyAndPlay();
    void showDrumMapEditor();

    midispace::LatentVector interpolate(const std::vector<float>& weights) const;
    static juce::String notesToString(const std::vector<midispace::NoteEvent>& notes);
    juce::String nextNodeName() const;
    void makeSlider(juce::Slider& s, juce::Label& l, const juce::String& text,
                    double minV, double maxV, double step, double init,
                    std::function<void()> onChange);

    DrumSpaceAudioProcessor& processor_;
    midispace::CanvasComponent canvas_;
    midispace::HttpModelInterface model_;
    midispace::GenerationManager gen_;

    midispace::LatentVector lastZ_;
    std::vector<midispace::NoteEvent> lastRawResult_;
    std::vector<midispace::NoteEvent> currentResult_;
    midispace::MusicalControls controls_;
    int captureNodeIndex_ = -1;

    juce::Label statusLabel_;
    juce::Label resultLabel_;

    juce::TextButton playButton_{"Play"};
    juce::TextButton stopButton_{"Stop"};
    juce::TextButton regenButton_{"Regen"};
    juce::TextButton saveButton_{"Save .mid"};
    juce::TextButton newButton_{"New"};
    juce::TextButton deleteButton_{"Delete"};
    juce::TextButton addResultButton_{"Add result"};
    juce::TextButton captureButton_{"Capture"};
    juce::ComboBox presetBox_;
    juce::TextButton applyButton_{"Apply"};
    juce::Label presetCaption_;
    juce::TextButton mapButton_{"Drum Map"};

    juce::Slider densitySlider_;
    juce::Label densityLabel_;

    juce::ThreadPool loadPool_{1};
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSpaceAudioProcessorEditor)
};
