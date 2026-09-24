#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace midispace {

// A musical anchor placed by the user in 2D space.  This is the C++ port of
// phase1_validate/src/spatial_engine.py — pure geometry + weights, no MIDI,
// no model, no UI.
struct Anchor {
    juce::String id;
    juce::Point<float> position;
};

class SpatialEngine {
public:
    explicit SpatialEngine(std::vector<Anchor> anchors)
        : anchors_(std::move(anchors)) {}

    const std::vector<Anchor>& getAnchors() const { return anchors_; }
    void setAnchors(std::vector<Anchor> anchors) { anchors_ = std::move(anchors); }

    // (index, distance) of the nearest anchor; index = -1 if the ball is not
    // within snapEps of any anchor.
    std::pair<int, float> nearestAnchor(juce::Point<float> ballPos,
                                        float snapEps = 0.001f) const {
        if (anchors_.empty())
            return { -1, std::numeric_limits<float>::infinity() };

        int best = 0;
        float bestDist = ballPos.getDistanceFrom(anchors_[0].position);
        for (size_t i = 1; i < anchors_.size(); ++i) {
            const float d = ballPos.getDistanceFrom(anchors_[i].position);
            if (d < bestDist) {
                bestDist = d;
                best = static_cast<int>(i);
            }
        }
        if (bestDist <= snapEps)
            return { best, bestDist };
        return { -1, bestDist };
    }

    // Influence weights summing to 1.0.  Within snapEps of an anchor the result
    // is one-hot (anchor identity preservation).  method: "idw" (default) or
    // "nearest".
    std::vector<float> weights(juce::Point<float> ballPos,
                               const juce::String& method = "idw",
                               float power = 2.0f,
                               float snapEps = 0.001f) const {
        const int n = static_cast<int>(anchors_.size());
        std::vector<float> w(n, 0.0f);
        if (n == 0)
            return w;

        const auto nearest = nearestAnchor(ballPos, snapEps);
        if (nearest.first >= 0) {
            w[nearest.first] = 1.0f;
            return w;
        }

        if (method == "nearest") {
            int best = 0;
            float bestDist = std::numeric_limits<float>::infinity();
            for (int i = 0; i < n; ++i) {
                const float d = ballPos.getDistanceFrom(anchors_[i].position);
                if (d < bestDist) {
                    bestDist = d;
                    best = i;
                }
            }
            w[best] = 1.0f;
            return w;
        }

        // Inverse-distance weighting (default).
        std::vector<float> inv(n, 0.0f);
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) {
            const float d = std::max(ballPos.getDistanceFrom(anchors_[i].position), 1e-12f);
            inv[i] = 1.0f / std::pow(d, power);
            sum += inv[i];
        }
        if (sum > 0.0f)
            for (int i = 0; i < n; ++i)
                w[i] = inv[i] / sum;
        return w;
    }

private:
    std::vector<Anchor> anchors_;
};

} // namespace midispace
