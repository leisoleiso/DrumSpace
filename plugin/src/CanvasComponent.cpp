#include "CanvasComponent.h"

namespace midispace {

CanvasComponent::CanvasComponent() {
    // Three default nodes placed equidistant on a circle (equilateral triangle),
    // ball in the centre.  Positions are finalised in resized() once we know
    // the actual canvas size.
    nodes_ = {
        { "1", "1", { 0.0f, 0.0f }, paletteColour(0) },
        { "2", "2", { 0.0f, 0.0f }, paletteColour(1) },
        { "3", "3", { 0.0f, 0.0f }, paletteColour(2) },
    };
    ball_.position = { 0.0f, 0.0f };
    rebuildSpatialEngine();
}

juce::Point<float> CanvasComponent::spaceCentre() const {
    return juce::Point<float>(getWidth() * 0.5f, getHeight() * 0.5f);
}

float CanvasComponent::spaceRadius() const {
    return juce::jmin(getWidth(), getHeight()) * 0.5f - kNodeRadius - 6.0f;
}

void CanvasComponent::resized() {
    const auto c = spaceCentre();
    const float r = std::max(spaceRadius(), 1.0f);

    if (!laidOutDefaults_) {
        // First layout: place default nodes equidistantly on the circle.
        laidOutDefaults_ = true;
        const int n = static_cast<int>(nodes_.size());
        for (int i = 0; i < n; ++i) {
            const float ang = juce::MathConstants<float>::twoPi * i / static_cast<float>(n)
                            - juce::MathConstants<float>::halfPi;   // start at top
            nodes_[i].position = juce::Point<float>(
                c.x + r * std::cos(ang), c.y + r * std::sin(ang));
        }
        ball_.position = c;
    } else if (lastRadius_ > 0.0f) {
        // Window resized: remap every position so its (centre,radius) ratio
        // stays the same, keeping nodes/ball glued to the circular space.
        const float scale = r / lastRadius_;
        for (auto& n : nodes_) {
            const auto v = n.position - lastCentre_;
            n.position = c + v * scale;
        }
        const auto bv = ball_.position - lastCentre_;
        ball_.position = c + bv * scale;
    }

    lastCentre_ = c;
    lastRadius_ = r;
    rebuildSpatialEngine();
    repaint();
}

juce::Colour CanvasComponent::nextColour() const {
    for (int i = 0; i < 6; ++i) {
        bool used = false;
        for (const auto& node : nodes_) {
            if (node.colour == paletteColour(i)) { used = true; break; }
        }
        if (!used)
            return paletteColour(i);
    }
    return paletteColour(static_cast<int>(nodes_.size()));
}

std::vector<Anchor> CanvasComponent::buildAnchors() const {
    std::vector<Anchor> anchors;
    anchors.reserve(nodes_.size());
    for (const auto& n : nodes_)
        anchors.push_back({ n.id, n.position });
    return anchors;
}

std::vector<float> CanvasComponent::getWeights() const {
    if (!engine_)
        return {};
    return engine_->weights(ball_.position, "idw", 2.0f, kSnapEps);
}

void CanvasComponent::setNodeLatents(const std::vector<LatentVector>& lats) {
    const size_t n = std::min(nodes_.size(), lats.size());
    for (size_t i = 0; i < n; ++i)
        nodes_[i].latent = lats[i];
}

void CanvasComponent::setNodeLatent(int index, const LatentVector& latent) {
    if (index >= 0 && index < static_cast<int>(nodes_.size()))
        nodes_[index].latent = latent;
}

void CanvasComponent::setNodeMotif(int index, const std::vector<NoteEvent>& motif) {
    if (index >= 0 && index < static_cast<int>(nodes_.size())) {
        nodes_[index].motif = motif;
        if (onNodesChanged)
            onNodesChanged();
    }
}

void CanvasComponent::addNode(const juce::String& name, juce::Point<float> pos,
                              const std::vector<NoteEvent>& motif,
                              const LatentVector& latent) {
    MidiNode n;
    n.id = name;
    n.name = name;
    n.position = pos;
    n.colour = nextColour();
    n.motif = motif;
    n.latent = latent;
    nodes_.push_back(n);
    selectedNodeIndex_ = static_cast<int>(nodes_.size()) - 1;
    rebuildSpatialEngine();
    if (onNodesChanged)
        onNodesChanged();
    repaint();
}

void CanvasComponent::removeNode(int index) {
    if (index < 0 || index >= static_cast<int>(nodes_.size()))
        return;
    nodes_.erase(nodes_.begin() + index);
    selectedNodeIndex_ = -1;
    rebuildSpatialEngine();
    if (onNodesChanged)
        onNodesChanged();
    repaint();
}

void CanvasComponent::setNodes(const std::vector<MidiNode>& nodes) {
    nodes_ = nodes;
    laidOutDefaults_ = true;   // positions come from saved state, not the default layout
    selectedNodeIndex_ = -1;
    rebuildSpatialEngine();
    repaint();
}

void CanvasComponent::rebuildSpatialEngine() {
    engine_ = std::make_unique<SpatialEngine>(buildAnchors());
}

int CanvasComponent::hitTest(juce::Point<float> p) const {
    if (p.getDistanceFrom(ball_.position) <= kBallRadius + 2.0f)
        return -1;
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i)
        if (p.getDistanceFrom(nodes_[i].position) <= kNodeRadius + 2.0f)
            return i;
    return -2;
}

void CanvasComponent::mouseDown(const juce::MouseEvent& e) {
    const auto p = e.position;
    const int hit = hitTest(p);

    if (hit >= 0) {
        const juce::uint32 now = juce::Time::getMillisecondCounter();
        if (lastClickIndex_ == hit && (now - lastClickTime_) < 400) {
            lastClickIndex_ = -2;
            lastClickTime_ = 0;
            if (onNodeDoubleClicked)
                onNodeDoubleClicked(hit);
            return;
        }
        lastClickIndex_ = hit;
        lastClickTime_ = now;
        dragNodeIndex_ = hit;
        selectedNodeIndex_ = hit;
        dragOffset_ = nodes_[hit].position - p;
    } else if (hit == -1) {
        ball_.dragging = true;
        dragNodeIndex_ = -1;
        selectedNodeIndex_ = -1;
        lastClickIndex_ = -2;
    } else {
        dragNodeIndex_ = -2;
        selectedNodeIndex_ = -1;
        lastClickIndex_ = -2;
    }
    repaint();
}

void CanvasComponent::mouseDrag(const juce::MouseEvent& e) {
    const auto p = e.position;
    // Clamp a position to the circular control space.  spaceRadius() is the
    // max centre distance (so a node's rim stays inside the canvas); use it
    // directly for both nodes and ball so dragging matches the default ring.
    const auto clampToCircle = [this](juce::Point<float> pos) {
        const auto c = spaceCentre();
        const float R = std::max(spaceRadius(), 1.0f);
        auto v = pos - c;
        const float d = v.getDistanceFromOrigin();
        if (d > R)
            v = v * (R / std::max(d, 1e-6f));
        return c + v;
    };

    if (ball_.dragging) {
        ball_.position = clampToCircle(p);
        if (onBallMoved)
            onBallMoved();
        repaint();
    } else if (dragNodeIndex_ >= 0) {
        nodes_[dragNodeIndex_].position = clampToCircle(p + dragOffset_);
        rebuildSpatialEngine();
        repaint();
    }
}

void CanvasComponent::mouseUp(const juce::MouseEvent&) {
    const bool wasDraggingBall = ball_.dragging;
    const bool wasDraggingNode = (dragNodeIndex_ >= 0);
    ball_.dragging = false;
    dragNodeIndex_ = -2;
    if (wasDraggingBall && onBallReleased)
        onBallReleased();
    if (wasDraggingNode && onNodesChanged)
        onNodesChanged();
    repaint();
}

void CanvasComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(18, 22, 36));

    const auto c = spaceCentre();
    const float R = spaceRadius() + kNodeRadius;   // outer boundary incl. node radius

    // Circular control space: faint rings + spokes.
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawEllipse(c.x - R, c.y - R, 2.0f * R, 2.0f * R, 2.0f);
    const int rings = 3;
    for (int i = 1; i <= rings; ++i) {
        const float rr = R * i / static_cast<float>(rings);
        g.drawEllipse(c.x - rr, c.y - rr, 2.0f * rr, 2.0f * rr, 1.0f);
    }
    const int spokes = 12;
    for (int i = 0; i < spokes; ++i) {
        const float ang = juce::MathConstants<float>::twoPi * i / spokes;
        g.drawLine(c.x, c.y,
                   c.x + R * std::cos(ang), c.y + R * std::sin(ang), 1.0f);
    }

    // Lines from centre to each node (visualise the geometric relationships).
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    for (const auto& n : nodes_)
        g.drawLine(c.x, c.y, n.position.x, n.position.y, 1.0f);

    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
        const auto& n = nodes_[i];
        g.setColour(n.colour);
        g.fillEllipse(n.position.x - kNodeRadius, n.position.y - kNodeRadius,
                      2.0f * kNodeRadius, 2.0f * kNodeRadius);
        if (i == selectedNodeIndex_) {
            g.setColour(juce::Colours::white);
            g.drawEllipse(n.position.x - kNodeRadius - 3.0f,
                          n.position.y - kNodeRadius - 3.0f,
                          2.0f * (kNodeRadius + 3.0f),
                          2.0f * (kNodeRadius + 3.0f), 2.0f);
        }
        // Node label: index number + a small first-note pitch letter beside it,
        // horizontally centred as one group.
        const juce::String pitch = midispace::firstNoteName(n);
        const float cx = n.position.x;
        const float cy = n.position.y;

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        const float numW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), n.name);
        g.setFont(juce::Font(9.0f));
        const float pitchW = pitch.isEmpty() ? 0.0f
                            : juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), pitch);
        const float gap = pitch.isEmpty() ? 0.0f : 3.0f;
        const float totalW = numW + gap + pitchW;
        const float startX = cx - totalW * 0.5f;

        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(n.name,
                   static_cast<int>(startX),
                   static_cast<int>(cy - 12),
                   static_cast<int>(numW + 1.0f),
                   24,
                   juce::Justification::left, false);

        if (pitch.isNotEmpty()) {
            g.setFont(juce::Font(9.0f));
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawText(pitch,
                       static_cast<int>(startX + numW + gap),
                       static_cast<int>(cy - 7),
                       static_cast<int>(pitchW + 2.0f),
                       14,
                       juce::Justification::left, false);
        }
    }

    g.setColour(ball_.colour);
    g.fillEllipse(ball_.position.x - kBallRadius, ball_.position.y - kBallRadius,
                  2.0f * kBallRadius, 2.0f * kBallRadius);
    g.setColour(juce::Colours::white);
    g.drawEllipse(ball_.position.x - kBallRadius, ball_.position.y - kBallRadius,
                  2.0f * kBallRadius, 2.0f * kBallRadius, 2.0f);

    drawWeights(g);
}

void CanvasComponent::drawWeights(juce::Graphics& g) {
    if (!engine_)
        return;

    const auto w = engine_->weights(ball_.position, "idw", 2.0f, kSnapEps);
    const auto nearest = engine_->nearestAnchor(ball_.position, kSnapEps);

    juce::String s = "weights:  ";
    for (size_t i = 0; i < nodes_.size() && i < w.size(); ++i)
        s += nodes_[i].name + "=" + juce::String(w[i], 2) + "   ";
    if (nearest.first >= 0)
        s += "   [at anchor " + nodes_[nearest.first].name + "]";

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText(s, 10, 10, getWidth() - 20, 24, juce::Justification::left, false);
}

} // namespace midispace
