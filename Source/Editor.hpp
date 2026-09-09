#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>
class GeigerPlugin;
class GeigerControl;
class GeigerMonitor;
class GeigerVisuals;
class GeigerLook;
class GeigerEditor final : public juce::Component, private juce::Timer {
public:
    explicit GeigerEditor(GeigerPlugin&);
    ~GeigerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void selectPage(int);
    bool layoutIsValid() const;
    void focusParameter(std::size_t);
    void refreshDisplay();
    juce::String displayedPresetName() const { return presets_.getText(); }
private:
    void timerCallback() override;
    void help(const char*);
    GeigerPlugin& plugin_;
    std::unique_ptr<GeigerLook> look_;
    std::vector<std::unique_ptr<GeigerControl>> controls_;
    std::unique_ptr<GeigerMonitor> monitor_;
    std::unique_ptr<GeigerVisuals> explorer_;
    std::array<juce::TextButton,4> tabs_;
    juce::ComboBox presets_;
    juce::TextButton previousPreset_{"<"},nextPreset_{">"};
    juce::TextButton power_{"POWER"},test_{"Test click"},seed_{"New seed"},panic_{"Silence"};
    juce::ToggleButton calm_{"Calm display"};
    juce::Label help_;
    juce::TooltipWindow tooltip_{this,600};
    int page_=0, previewTicks_=0;
    std::size_t focused_=1;
};
