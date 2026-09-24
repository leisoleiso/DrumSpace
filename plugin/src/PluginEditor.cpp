#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiFileWriter.h"

namespace {

// motif  -> "pitch,start,end; pitch,start,end; ..."
static juce::String motifToString(const std::vector<midispace::NoteEvent>& motif) {
    juce::StringArray parts;
    for (const auto& e : motif)
        parts.add(juce::String(e.pitch) + "," + juce::String(e.startQuarter) + "," + juce::String(e.endQuarter));
    return parts.joinIntoString(";");
}

static std::vector<midispace::NoteEvent> stringToMotif(const juce::String& s) {
    std::vector<midispace::NoteEvent> out;
    if (s.isEmpty())
        return out;
    for (const auto& tok : juce::StringArray::fromTokens(s, ";", "")) {
        const auto f = juce::StringArray::fromTokens(tok, ",", "");
        if (f.size() >= 3) {
            midispace::NoteEvent e;
            e.pitch = f[0].getIntValue();
            e.startQuarter = f[1].getFloatValue();
            e.endQuarter = f[2].getFloatValue();
            out.push_back(e);
        }
    }
    return out;
}

// latent -> comma-separated floats
static juce::String latentToString(const midispace::LatentVector& latent) {
    juce::StringArray parts;
    for (float v : latent)
        parts.add(juce::String(v));
    return parts.joinIntoString(",");
}

static midispace::LatentVector stringToLatent(const juce::String& s) {
    midispace::LatentVector out;
    if (s.isEmpty())
        return out;
    for (const auto& tok : juce::StringArray::fromTokens(s, ",", ""))
        out.push_back(tok.getFloatValue());
    return out;
}

static juce::String nodesToXml(const std::vector<midispace::MidiNode>& nodes,
                               juce::Point<float> ballPos) {
    juce::ValueTree root("DrumSpace");
    root.setProperty("ballX", ballPos.x, nullptr);
    root.setProperty("ballY", ballPos.y, nullptr);
    for (const auto& n : nodes) {
        juce::ValueTree node("Node");
        node.setProperty("name", n.name, nullptr);
        node.setProperty("x", n.position.x, nullptr);
        node.setProperty("y", n.position.y, nullptr);
        node.setProperty("colour", n.colour.toString(), nullptr);
        node.setProperty("motif", motifToString(n.motif), nullptr);
        node.setProperty("latent", latentToString(n.latent), nullptr);
        root.addChild(node, -1, nullptr);
    }
    if (auto xml = root.createXml())
        return xml->toString();
    return {};
}

static std::vector<midispace::MidiNode> xmlToNodes(const juce::String& xmlStr,
                                                   juce::Point<float>& ballPosOut) {
    std::vector<midispace::MidiNode> nodes;
    if (xmlStr.isEmpty())
        return nodes;
    auto xml = juce::XmlDocument::parse(xmlStr);
    if (xml == nullptr)
        return nodes;
    auto root = juce::ValueTree::fromXml(*xml);
    if (!root.isValid())
        return nodes;

    if (root.hasProperty("ballX") && root.hasProperty("ballY"))
        ballPosOut = juce::Point<float>(
            static_cast<float>(static_cast<double>(root.getProperty("ballX"))),
            static_cast<float>(static_cast<double>(root.getProperty("ballY"))));

    for (const auto& node : root) {
        midispace::MidiNode n;
        n.name = node.getProperty("name").toString();
        n.id = n.name;
        n.position = juce::Point<float>(
            static_cast<float>(static_cast<double>(node.getProperty("x"))),
            static_cast<float>(static_cast<double>(node.getProperty("y"))));
        const auto colourProp = node.getProperty("colour");
        if (colourProp.isString() && colourProp.toString().isNotEmpty())
            n.colour = juce::Colour::fromString(colourProp.toString());
        n.motif = stringToMotif(node.getProperty("motif").toString());
        n.latent = stringToLatent(node.getProperty("latent").toString());
        nodes.push_back(n);
    }
    return nodes;
}

} // namespace

// Modal editor for the 9-drum key mapping, with named save/load presets.
class DrumMapEditorComponent : public juce::Component {
public:
    DrumMapEditorComponent(midispace::DrumMap map,
                           std::function<void(midispace::DrumMap)> onOk)
        : map_(map), onOk_(std::move(onOk)) {
        for (int i = 0; i < 9; ++i) {
            labels_[i].setText(midispace::drumClassName(i), juce::dontSendNotification);
            labels_[i].setFont(13.0f);
            addAndMakeVisible(labels_[i]);

            for (int p = 0; p < 128; ++p)
                boxes_[i].addItem(juce::MidiMessage::getMidiNoteName(p, true, false, 4), p + 1);
            boxes_[i].setSelectedId(map_.out[i] + 1, juce::dontSendNotification);
            addAndMakeVisible(boxes_[i]);
        }

        presetCaption_.setText("Preset", juce::dontSendNotification);
        presetCaption_.setFont(11.0f);
        addAndMakeVisible(presetCaption_);
        refreshPresetList();
        addAndMakeVisible(presetBox_);

        loadButton_.onClick = [this] { loadSelectedPreset(); };
        addAndMakeVisible(loadButton_);

        deleteButton_.onClick = [this] { deleteSelectedPreset(); };
        addAndMakeVisible(deleteButton_);

        defaultButton_.onClick = [this] { applyDefaultMap(); };
        addAndMakeVisible(defaultButton_);

        nameLabel_.setText("Name", juce::dontSendNotification);
        nameLabel_.setFont(11.0f);
        addAndMakeVisible(nameLabel_);
        addAndMakeVisible(nameEditor_);

        saveButton_.onClick = [this] { savePreset(); };
        addAndMakeVisible(saveButton_);

        exportButton_.onClick = [this] { exportMap(); };
        addAndMakeVisible(exportButton_);

        importButton_.onClick = [this] { importMap(); };
        addAndMakeVisible(importButton_);

        okButton_.onClick = [this] {
            midispace::DrumMap m = map_;
            for (int i = 0; i < 9; ++i)
                m.out[i] = boxes_[i].getSelectedId() - 1;
            onOk_(m);
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        addAndMakeVisible(okButton_);

        setSize(420, 9 * 30 + 205);
    }

    void resized() override {
        auto a = getLocalBounds().reduced(10);

        auto presetRow = a.removeFromTop(30);
        presetCaption_.setBounds(presetRow.removeFromLeft(50));
        presetBox_.setBounds(presetRow.removeFromLeft(130));
        loadButton_.setBounds(presetRow.removeFromLeft(55).reduced(4, 3));
        deleteButton_.setBounds(presetRow.removeFromLeft(55).reduced(4, 3));
        defaultButton_.setBounds(presetRow.removeFromLeft(80).reduced(4, 3));

        for (int i = 0; i < 9; ++i) {
            auto row = a.removeFromTop(30);
            labels_[i].setBounds(row.removeFromLeft(90));
            boxes_[i].setBounds(row);
        }

        auto saveRow = a.removeFromTop(34);
        nameLabel_.setBounds(saveRow.removeFromLeft(40));
        nameEditor_.setBounds(saveRow.removeFromLeft(130).reduced(0, 4));
        saveButton_.setBounds(saveRow.removeFromLeft(60).reduced(4, 3));
        okButton_.setBounds(saveRow.reduced(40, 4));

        auto ioRow = a.removeFromTop(34);
        exportButton_.setBounds(ioRow.removeFromLeft(100).reduced(4, 3));
        importButton_.setBounds(ioRow.removeFromLeft(100).reduced(4, 3));
    }

private:
    midispace::DrumMap map_;
    std::function<void(midispace::DrumMap)> onOk_;
    juce::Label labels_[9];
    juce::ComboBox boxes_[9];

    juce::Label presetCaption_, nameLabel_;
    juce::ComboBox presetBox_;
    juce::TextButton loadButton_{"Load"};
    juce::TextButton deleteButton_{"Delete"};
    juce::TextButton defaultButton_{"Default"};
    juce::TextButton saveButton_{"Save"};
    juce::TextButton exportButton_{"Export..."};
    juce::TextButton importButton_{"Import..."};
    juce::TextButton okButton_{"OK"};
    juce::TextEditor nameEditor_;

    midispace::DrumMap currentMapFromBoxes() const {
        midispace::DrumMap m = map_;
        for (int i = 0; i < 9; ++i)
            m.out[i] = boxes_[i].getSelectedId() - 1;
        return m;
    }

    void applyMapToBoxes(const midispace::DrumMap& m) {
        for (int i = 0; i < 9; ++i)
            boxes_[i].setSelectedId(m.out[i] + 1, juce::dontSendNotification);
    }

    void refreshPresetList() {
        const auto dir = midispace::drumMapDirectory();
        presetBox_.clear(juce::dontSendNotification);
        if (dir.isDirectory()) {
            for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.txt"))
                presetBox_.addItem(f.getFileNameWithoutExtension(), presetBox_.getNumItems() + 1);
        }
        if (presetBox_.getNumItems() == 0)
            presetBox_.addItem("(none)", 1);
        presetBox_.setSelectedId(1, juce::dontSendNotification);
    }

    void savePreset() {
        auto name = nameEditor_.getText().trim();
        if (name.isEmpty()) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Save drum map", "Enter a name first.");
            return;
        }
        const auto dir = midispace::drumMapDirectory();
        dir.createDirectory();
        auto f = dir.getChildFile(name).withFileExtension(".txt");
        if (f.replaceWithText(currentMapFromBoxes().toString())) {
            refreshPresetList();
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                   "Save drum map", "Saved: " + name);
        }
    }

    void loadSelectedPreset() {
        const auto dir = midispace::drumMapDirectory();
        auto name = presetBox_.getText();
        auto f = dir.getChildFile(name).withFileExtension(".txt");
        if (f.existsAsFile()) {
            applyMapToBoxes(midispace::DrumMap::fromString(f.loadFileAsString()));
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                   "Load drum map", "Loaded: " + name);
        }
    }

    void deleteSelectedPreset() {
        const auto dir = midispace::drumMapDirectory();
        auto name = presetBox_.getText();
        auto f = dir.getChildFile(name).withFileExtension(".txt");
        if (!f.existsAsFile())
            return;
        if (f.deleteFile()) {
            refreshPresetList();
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                   "Delete drum map", "Deleted: " + name);
        }
    }

    void applyDefaultMap() {
        // Reset all 9 keys to the GM defaults.
        midispace::DrumMap m;
        applyMapToBoxes(m);
    }

    void exportMap() {
        // Export the current mapping to a user-chosen file (text, 9 numbers).
        auto* fc = new juce::FileChooser(
            "Export drum map as...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.txt");
        fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, fc](const juce::FileChooser& chooser) {
                const auto chosen = chooser.getResult();
                delete fc;
                if (chosen == juce::File())
                    return;
                auto f = chosen.withFileExtension(".txt");
                if (f.replaceWithText(currentMapFromBoxes().toString()))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                        "Export drum map", "Exported to:\n" + f.getFullPathName());
            });
    }

    void importMap() {
        // Import a mapping from a user-chosen file into the current boxes.
        auto* fc = new juce::FileChooser(
            "Import drum map",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.txt");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fc](const juce::FileChooser& chooser) {
                const auto chosen = chooser.getResult();
                delete fc;
                if (chosen == juce::File())
                    return;
                auto m = midispace::DrumMap::fromString(chosen.loadFileAsString());
                applyMapToBoxes(m);
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Import drum map", "Imported from:\n" + chosen.getFullPathName());
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumMapEditorComponent)
};

DrumSpaceAudioProcessorEditor::DrumSpaceAudioProcessorEditor(DrumSpaceAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      model_("http://127.0.0.1:8766"),
      gen_(model_) {
    setSize(900, 700);
    setResizable(true, true);
    setResizeLimits(700, 560, 2000, 1400);
    addAndMakeVisible(canvas_);

    statusLabel_.setFont(14.0f);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel_.setText("Loading latents...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel_);

    resultLabel_.setFont(13.0f);
    resultLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(resultLabel_);

    canvas_.onBallMoved = [this] { gen_.onBallMoved(); };
    canvas_.onBallReleased = [this] { handleBallReleased(); pushNodeState(); };
    canvas_.onNodesChanged = [this] { pushNodeState(); };

    gen_.onGenerating = [this] {
        statusLabel_.setText("Generating...", juce::dontSendNotification);
    };
    gen_.onResult = [this](const std::vector<midispace::NoteEvent>& raw) {
        statusLabel_.setText("Ready", juce::dontSendNotification);
        lastRawResult_ = raw;
        applyAndPlay();
    };
    gen_.onFailed = [this](const juce::String& msg) {
        statusLabel_.setText("Generation failed", juce::dontSendNotification);
        resultLabel_.setText(msg, juce::dontSendNotification);
    };

    playButton_.onClick = [this] { processor_.play(); };
    stopButton_.onClick = [this] { processor_.stop(); };
    regenButton_.onClick = [this] { regenerate(); };
    saveButton_.onClick = [this] { saveResult(); };
    newButton_.onClick = [this] { addNewNode(); };
    deleteButton_.onClick = [this] { deleteSelectedNode(); };
    addResultButton_.onClick = [this] { addResultAsNode(); };
    captureButton_.onClick = [this] { startCapture(); };
    applyButton_.onClick = [this] { applyPreset(); };
    mapButton_.onClick = [this] { showDrumMapEditor(); };
    for (auto* b : { &playButton_, &stopButton_, &regenButton_, &saveButton_,
                     &newButton_, &deleteButton_, &addResultButton_, &captureButton_, &applyButton_,
                     &mapButton_ })
        addAndMakeVisible(b);

    // Make the drum-map button stand out.
    mapButton_.setButtonText("DRUM MAP");
    mapButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb4552a));
    mapButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    presetCaption_.setFont(11.0f);
    presetCaption_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    presetCaption_.setText("Pattern", juce::dontSendNotification);
    addAndMakeVisible(presetCaption_);

    const auto presets = midispace::drumPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        presetBox_.addItem(presets[i].name, i + 1);
    presetBox_.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(presetBox_);

    makeSlider(densitySlider_, densityLabel_, "Density", 0.1, 1.0, 0.1, 1.0, [this] {
        controls_.density = static_cast<float>(densitySlider_.getValue());
        applyAndPlay();
    });

    // Restore node state from the processor's in-memory state.  This survives
    // editor close/reopen (the processor outlives the editor) but resets to
    // defaults when the plugin is deleted and reloaded (fresh processor).
    const auto defaults = midispace::allMotifs();
    juce::Point<float> ballPos = canvas_.ballPosition();
    std::vector<midispace::MidiNode> loaded;
    const auto stateXml = processor_.getNodeStateXml();
    if (stateXml.isNotEmpty())
        loaded = xmlToNodes(stateXml, ballPos);

    if (!loaded.empty()) {
        for (size_t i = 0; i < loaded.size() && i < defaults.size(); ++i)
            if (loaded[i].motif.empty())
                loaded[i].motif = defaults[i];
        for (size_t i = 0; i < loaded.size(); ++i)
            if (loaded[i].colour == juce::Colour())
                loaded[i].colour = midispace::paletteColour(static_cast<int>(i));
        canvas_.setNodes(loaded);
        canvas_.setBallPosition(ballPos);
    } else {
        for (size_t i = 0; i < defaults.size() && i < static_cast<size_t>(canvas_.getNodeCount()); ++i)
            canvas_.setNodeMotif(static_cast<int>(i), defaults[i]);
    }

    startLatentLoad();
    startTimer(150);
}

DrumSpaceAudioProcessorEditor::~DrumSpaceAudioProcessorEditor() {
    stopTimer();
    *alive_ = false;
    loadPool_.removeAllJobs(true, 2000);
}

void DrumSpaceAudioProcessorEditor::timerCallback() {
    if (captureNodeIndex_ >= 0 && !processor_.isCapturing())
        finishCapture();
}

void DrumSpaceAudioProcessorEditor::makeSlider(juce::Slider& s, juce::Label& l,
                                               const juce::String& text,
                                               double minV, double maxV, double step,
                                               double init, std::function<void()> onChange) {
    l.setText(text, juce::dontSendNotification);
    l.setFont(12.0f);
    l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(l);

    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 18);
    s.setRange(minV, maxV, step);
    s.setValue(init, juce::dontSendNotification);
    s.onValueChange = [onChange] { onChange(); };
    addAndMakeVisible(s);
}

void DrumSpaceAudioProcessorEditor::startLatentLoad() {
    // Only encode nodes whose latent is missing (cached latents skip this).
    const auto& nodes = canvas_.getNodes();
    std::vector<int> toEncodeIdx;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        if (nodes[i].latent.empty() && !nodes[i].motif.empty())
            toEncodeIdx.push_back(i);

    if (toEncodeIdx.empty()) {
        statusLabel_.setText("Ready - drag the ball, click a node to select",
                             juce::dontSendNotification);
        return;
    }

    std::vector<std::vector<midispace::NoteEvent>> toEncode;
    for (int i : toEncodeIdx)
        toEncode.push_back(nodes[i].motif);

    auto alive = alive_;
    loadPool_.addJob([this, alive, toEncodeIdx, toEncode]() {
        try {
            std::vector<midispace::LatentVector> lats;
            for (const auto& m : toEncode)
                lats.push_back(model_.encode(m));
            juce::MessageManager::callAsync([this, alive, toEncodeIdx, lats]() {
                if (!*alive)
                    return;
                for (size_t i = 0; i < toEncodeIdx.size() && i < lats.size(); ++i)
                    canvas_.setNodeLatent(toEncodeIdx[i], lats[i]);
                pushNodeState();   // persist the freshly-encoded latents
                statusLabel_.setText("Ready - drag the ball, click a node to select",
                                     juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Latent load failed", juce::dontSendNotification);
                resultLabel_.setText(msg, juce::dontSendNotification);
            });
        }
    });
}

void DrumSpaceAudioProcessorEditor::handleBallReleased() {
    const auto& nodes = canvas_.getNodes();
    if (nodes.empty())
        return;

    const auto w = canvas_.getWeights();

    for (size_t i = 0; i < w.size() && i < nodes.size(); ++i) {
        if (w[i] >= 0.999f && !nodes[i].motif.empty()) {
            statusLabel_.setText("At anchor " + nodes[i].name, juce::dontSendNotification);
            lastRawResult_ = nodes[i].motif;
            applyAndPlay();
            return;
        }
    }

    if (nodes[0].latent.empty()) {
        statusLabel_.setText("Latents not ready", juce::dontSendNotification);
        return;
    }

    lastZ_ = interpolate(w);
    gen_.onBallReleased(lastZ_);
}

void DrumSpaceAudioProcessorEditor::regenerate() {
    if (lastZ_.empty())
        return;
    gen_.onBallReleased(lastZ_);
}

midispace::LatentVector DrumSpaceAudioProcessorEditor::interpolate(
    const std::vector<float>& weights) const {
    const auto& nodes = canvas_.getNodes();
    const size_t dim = nodes.empty() ? 0 : nodes[0].latent.size();
    midispace::LatentVector z(dim, 0.0f);
    for (size_t i = 0; i < nodes.size() && i < weights.size(); ++i)
        for (size_t d = 0; d < dim; ++d)
            z[d] += weights[i] * nodes[i].latent[d];
    return z;
}

void DrumSpaceAudioProcessorEditor::applyAndPlay() {
    if (lastRawResult_.empty())
        return;
    currentResult_ = controls_.apply(lastRawResult_);
    resultLabel_.setText(notesToString(currentResult_), juce::dontSendNotification);
    processor_.setResult(currentResult_, 120.0);
}

void DrumSpaceAudioProcessorEditor::addResultAsNode() {
    if (currentResult_.empty())
        return;

    const auto result = currentResult_;
    const juce::String name = nextNodeName();
    auto alive = alive_;

    statusLabel_.setText("Encoding node " + name + "...", juce::dontSendNotification);
    loadPool_.addJob([this, alive, result, name]() {
        try {
            auto latent = model_.encode(result);
            juce::MessageManager::callAsync([this, alive, result, latent, name]() {
                if (!*alive)
                    return;
                auto pos = canvas_.ballPosition() + juce::Point<float>(40.0f, -50.0f);
                canvas_.addNode(name, pos, result, latent);
                statusLabel_.setText("Added node " + name, juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Add node failed", juce::dontSendNotification);
                resultLabel_.setText(msg, juce::dontSendNotification);
            });
        }
    });
}

void DrumSpaceAudioProcessorEditor::addNewNode() {
    // A minimal blank pattern: one kick on the first beat.
    const std::vector<midispace::NoteEvent> blank = { midispace::d(midispace::kKick, 0.0f) };
    const juce::String name = nextNodeName();
    auto alive = alive_;

    statusLabel_.setText("Adding node " + name + "...", juce::dontSendNotification);
    loadPool_.addJob([this, alive, blank, name]() {
        try {
            auto latent = model_.encode(blank);
            juce::MessageManager::callAsync([this, alive, blank, latent, name]() {
                if (!*alive)
                    return;
                auto pos = canvas_.ballPosition() + juce::Point<float>(30.0f, -30.0f);
                canvas_.addNode(name, pos, blank, latent);
                statusLabel_.setText("Added node " + name + " - select it, then Capture to load drums",
                                     juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Add node failed", juce::dontSendNotification);
            });
        }
    });
}

void DrumSpaceAudioProcessorEditor::deleteSelectedNode() {
    const int idx = canvas_.getSelectedNodeIndex();
    if (idx >= 0) {
        canvas_.removeNode(idx);
        statusLabel_.setText("Deleted node", juce::dontSendNotification);
    } else {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
    }
}

void DrumSpaceAudioProcessorEditor::startCapture() {
    const auto& nodes = canvas_.getNodes();
    const int idx = canvas_.getSelectedNodeIndex();
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
        return;
    }

    captureNodeIndex_ = idx;
    processor_.startCapture();
    statusLabel_.setText(
        "Capturing " + nodes[idx].name +
        " - write drums in FL piano roll (F7), press Space for ~2 bars",
        juce::dontSendNotification);
}

void DrumSpaceAudioProcessorEditor::finishCapture() {
    if (captureNodeIndex_ < 0)
        return;

    const int idx = captureNodeIndex_;
    captureNodeIndex_ = -1;
    const auto notes = processor_.stopCapture();

    if (notes.empty()) {
        statusLabel_.setText("No MIDI received - press Space in FL first", juce::dontSendNotification);
        return;
    }

    canvas_.setNodeMotif(idx, notes);
    const auto& ns = canvas_.getNodes();
    const juce::String nodeName = (idx >= 0 && idx < static_cast<int>(ns.size())) ? ns[idx].name : juce::String();
    statusLabel_.setText("Captured into node " + nodeName + " - re-encoding...",
                         juce::dontSendNotification);

    auto alive = alive_;
    loadPool_.addJob([this, alive, idx, notes]() {
        try {
            auto latent = model_.encode(notes);
            juce::MessageManager::callAsync([this, alive, idx, latent]() {
                if (!*alive)
                    return;
                canvas_.setNodeLatent(idx, latent);
                pushNodeState();   // persist latent with the node
                statusLabel_.setText("Ready", juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Re-encode failed", juce::dontSendNotification);
            });
        }
    });
}

void DrumSpaceAudioProcessorEditor::pushNodeState() {
    processor_.setNodeStateXml(nodesToXml(canvas_.getNodes(), canvas_.ballPosition()));
}

void DrumSpaceAudioProcessorEditor::applyPreset() {
    const int idx = canvas_.getSelectedNodeIndex();
    const auto& nodes = canvas_.getNodes();
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
        return;
    }

    const auto presets = midispace::drumPresets();
    const int pi = juce::jlimit(0, static_cast<int>(presets.size()) - 1,
                                presetBox_.getSelectedId() - 1);
    const auto notes = presets[pi].pattern;

    canvas_.setNodeMotif(idx, notes);
    pushNodeState();
    statusLabel_.setText("Applied - re-encoding...", juce::dontSendNotification);

    auto alive = alive_;
    loadPool_.addJob([this, alive, idx, notes]() {
        try {
            auto latent = model_.encode(notes);
            juce::MessageManager::callAsync([this, alive, idx, latent]() {
                if (!*alive)
                    return;
                canvas_.setNodeLatent(idx, latent);
                pushNodeState();   // persist latent with the node
                statusLabel_.setText("Ready", juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Pattern re-encode failed", juce::dontSendNotification);
            });
        }
    });
}

void DrumSpaceAudioProcessorEditor::showDrumMapEditor() {
    auto alive = alive_;
    auto* content = new DrumMapEditorComponent(
        processor_.getDrumMap(),
        [this, alive](midispace::DrumMap m) {
            if (!*alive)
                return;
            processor_.setDrumMap(m);
            statusLabel_.setText("Drum key map updated", juce::dontSendNotification);
        });

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Drum Key Map";
    opts.dialogBackgroundColour = juce::Colours::darkgrey;
    opts.content.setOwned(content);
    opts.componentToCentreAround = this;
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}

void DrumSpaceAudioProcessorEditor::saveResult() {
    const int idx = canvas_.getSelectedNodeIndex();
    const auto& nodes = canvas_.getNodes();
    std::vector<midispace::NoteEvent> notes;
    if (idx >= 0 && idx < static_cast<int>(nodes.size()))
        notes = nodes[idx].motif;
    else
        notes = currentResult_;

    if (notes.empty())
        return;

    auto alive = alive_;
    auto* fc = new juce::FileChooser(
        "Save MIDI as...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mid");

    fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, alive, fc, notes](const juce::FileChooser& chooser) {
            const auto chosen = chooser.getResult();
            delete fc;
            if (chosen == juce::File() || !*alive)
                return;

            const auto f = chosen.withFileExtension(".mid");
            if (midispace::writeMidiFile(notes, f, 120.0))
                statusLabel_.setText("Saved: " + f.getFileName(), juce::dontSendNotification);
            else
                statusLabel_.setText("Save failed", juce::dontSendNotification);
        });
}

juce::String DrumSpaceAudioProcessorEditor::nextNodeName() const {
    const auto& nodes = canvas_.getNodes();
    for (int i = 1; i <= 999; ++i) {
        const juce::String num(i);
        bool used = false;
        for (const auto& n : nodes) {
            if (n.name == num) {
                used = true;
                break;
            }
        }
        if (!used)
            return num;
    }
    return juce::String(nodes.size() + 1);
}

juce::String DrumSpaceAudioProcessorEditor::notesToString(
    const std::vector<midispace::NoteEvent>& notes) {
    juce::String s;
    for (const auto& n : notes) {
        if (s.isNotEmpty())
            s += "  ";
        s += midispace::drumPitchName(n.pitch);
    }
    return s;
}

void DrumSpaceAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void DrumSpaceAudioProcessorEditor::resized() {
    auto a = getLocalBounds();
    const int bottomH = 180;
    canvas_.setBounds(a.removeFromTop(getHeight() - bottomH));

    statusLabel_.setBounds(a.removeFromTop(20).reduced(10, 1));
    resultLabel_.setBounds(a.removeFromTop(18).reduced(10, 0));

    auto btnRow1 = a.removeFromTop(34);
    playButton_.setBounds(btnRow1.removeFromLeft(56).reduced(4, 4));
    stopButton_.setBounds(btnRow1.removeFromLeft(56).reduced(4, 4));
    regenButton_.setBounds(btnRow1.removeFromLeft(64).reduced(4, 4));
    saveButton_.setBounds(btnRow1.removeFromLeft(82).reduced(4, 4));

    auto btnRow2 = a.removeFromTop(34);
    newButton_.setBounds(btnRow2.removeFromLeft(70).reduced(4, 4));
    deleteButton_.setBounds(btnRow2.removeFromLeft(70).reduced(4, 4));
    addResultButton_.setBounds(btnRow2.removeFromLeft(90).reduced(4, 4));
    captureButton_.setBounds(btnRow2.removeFromLeft(90).reduced(4, 4));

    auto presetRow = a.removeFromTop(34);
    presetCaption_.setBounds(presetRow.removeFromLeft(56).reduced(4, 9));
    presetBox_.setBounds(presetRow.removeFromLeft(160).reduced(3, 4));
    applyButton_.setBounds(presetRow.removeFromLeft(70).reduced(4, 4));

    auto row = a.removeFromTop(34);
    densityLabel_.setBounds(row.removeFromLeft(64).reduced(4, 8));
    densitySlider_.setBounds(row.removeFromLeft(220).reduced(4, 6));
    mapButton_.setBounds(row.reduced(4, 4));   // prominent, full remaining width
}
