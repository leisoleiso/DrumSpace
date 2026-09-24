#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "Ball.h"
#include "MidiNode.h"
#include "SpatialEngine.h"

namespace midispace {

// The user-defined 2D control space: renders anchors + ball, handles dragging,
// and shows the live spatial weights (anchor-preserving interpolation preview).
class CanvasComponent : public juce::Component {
public:
    CanvasComponent();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    std::vector<Anchor> buildAnchors() const;
    juce::Point<float> ballPosition() const { return ball_.position; }
    void setBallPosition(juce::Point<float> pos) { ball_.position = pos; repaint(); }
    std::vector<float> getWeights() const;

    int getNodeCount() const { return static_cast<int>(nodes_.size()); }
    const std::vector<MidiNode>& getNodes() const { return nodes_; }
    int getSelectedNodeIndex() const { return selectedNodeIndex_; }

    void setNodeLatents(const std::vector<LatentVector>& lats);
    void setNodeLatent(int index, const LatentVector& latent);
    void setNodeMotif(int index, const std::vector<NoteEvent>& motif);
    void addNode(const juce::String& name, juce::Point<float> pos,
                 const std::vector<NoteEvent>& motif, const LatentVector& latent);
    void removeNode(int index);
    void setNodes(const std::vector<MidiNode>& nodes);

    std::function<void()> onBallMoved;                // ball position changed (cancel pending gen)
    std::function<void()> onBallReleased;             // mouse-up after dragging the ball
    std::function<void(int)> onNodeDoubleClicked;     // double-click a node -> edit its motif
    std::function<void()> onNodesChanged;             // any node add/remove/move/motif change

private:
    static constexpr float kNodeRadius = 20.0f;
    static constexpr float kBallRadius = 16.0f;
    static constexpr float kSnapEps = 12.0f;   // smaller snap range for finer control

    void rebuildSpatialEngine();
    int hitTest(juce::Point<float> p) const;
    void drawWeights(juce::Graphics& g);
    juce::Colour nextColour() const;
    juce::Point<float> spaceCentre() const;
    float spaceRadius() const;

    std::vector<MidiNode> nodes_;
    Ball ball_;
    std::unique_ptr<SpatialEngine> engine_;

    int dragNodeIndex_ = -2;        // >=0 node, -1 ball, -2 none
    juce::Point<float> dragOffset_;
    int selectedNodeIndex_ = -1;
    juce::uint32 lastClickTime_ = 0;
    int lastClickIndex_ = -2;
    bool laidOutDefaults_ = false;  // one-shot circular default placement
    juce::Point<float> lastCentre_; // for scaling node/ball positions on resize
    float lastRadius_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanvasComponent)
};

} // namespace midispace
