#pragma once
#include <nullclap/Gui.hpp>
#include <nullclap/PhysicalPixelGuiSizing.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
class GeigerPlugin;
class GeigerEditor;
class GeigerGui final : public nullclap::GuiDelegate {
public:
    explicit GeigerGui(GeigerPlugin& p) noexcept;
    ~GeigerGui() override;
    bool isApiSupported(const char*,bool) const noexcept override;
    const char* preferredApi() const noexcept override;
    bool create(const char*,bool) noexcept override;
    void destroy() noexcept override;
    bool setScale(double) noexcept override;
    bool show() noexcept override;
    bool hide() noexcept override;
    bool getSize(std::uint32_t&,std::uint32_t&) noexcept override;
    bool getResizeHints(clap_gui_resize_hints_t&) noexcept override;
    bool adjustSize(std::uint32_t&,std::uint32_t&) noexcept override;
    bool setSize(std::uint32_t,std::uint32_t) noexcept override;
    bool setParent(const clap_window_t&) noexcept override;
private:
    void applySize() noexcept;
    GeigerPlugin& plugin_;
    nullclap::PhysicalPixelGuiSizing sizing_{1120,760,900,660,1900,1300};
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> initialiser_;
    std::unique_ptr<GeigerEditor> editor_;
};
