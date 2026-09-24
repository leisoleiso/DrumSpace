#pragma once

#include "ModelInterface.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

namespace midispace {

// Ball-release -> debounce -> background decode -> result on the GUI thread.
// Never blocks the audio or message thread on model inference.
class GenerationManager : public juce::Timer {
public:
    explicit GenerationManager(ModelInterface& model);
    ~GenerationManager() override;

    void onBallReleased(const LatentVector& interpolatedZ);   // schedule after debounce
    void onBallMoved();                                       // cancel pending generation

    std::function<void()> onGenerating;
    std::function<void(const std::vector<NoteEvent>&)> onResult;
    std::function<void(const juce::String&)> onFailed;

private:
    void timerCallback() override;

    ModelInterface& model_;
    juce::ThreadPool pool_{1};
    LatentVector pendingZ_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerationManager)
};

} // namespace midispace
