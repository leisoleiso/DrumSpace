#include "GenerationManager.h"

namespace midispace {

GenerationManager::GenerationManager(ModelInterface& model) : model_(model) {}

GenerationManager::~GenerationManager() {
    stopTimer();
    *alive_ = false;
    pool_.removeAllJobs(true, 2000);
}

void GenerationManager::onBallReleased(const LatentVector& z) {
    pendingZ_ = z;
    startTimer(120);   // short debounce so dragging feels immediate
}

void GenerationManager::onBallMoved() {
    stopTimer();
}

void GenerationManager::timerCallback() {
    stopTimer();

    if (onGenerating)
        onGenerating();   // on the message thread

    const LatentVector z = pendingZ_;
    auto alive = alive_;

    pool_.addJob([this, alive, z]() {
        try {
            auto notes = model_.decode(z, 0.3f);
            juce::MessageManager::callAsync([this, alive, notes]() {
                if (*alive && onResult)
                    onResult(notes);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (*alive && onFailed)
                    onFailed(msg);
            });
        }
    });
}

} // namespace midispace
