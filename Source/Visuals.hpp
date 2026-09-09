#pragma once
#include "Parameters.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
class GeigerPlugin;
// Main-thread-only, cached explanatory previews. Never touches the live engine.
class GeigerVisuals final : public juce::Component {
public:
    explicit GeigerVisuals(GeigerPlugin&);
    ~GeigerVisuals() override;
    void setPage(int);
    void focus(std::size_t);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool dataIsFinite() const noexcept;
private:
    enum class Kind { Probe, Wave, Spectrum, Field, Waiting, Throughput, Recovery, Afterpulse, Drive, Density, Stereo, Envelope, Pitch, Repeat };
    struct Data { std::array<float,256> low{},high{},curve{}; double peak=0; };
    Kind kind() const noexcept;
    Data generate(geiger::Values) const;
    juce::Rectangle<float> plot() const;
    void endGesture();
    void moveProbe(const juce::MouseEvent&);
    GeigerPlugin& plugin_;
    geiger::Values values_{}, reference_{};
    Data current_{}, default_{};
    std::size_t focused_=geiger::Rate;
    int page_=0;
    bool dirty_=true, dragging_=false;
    double duration_=1;
    juce::TextButton probe_{"Probe pad"};
};
