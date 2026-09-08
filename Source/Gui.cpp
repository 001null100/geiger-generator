#include "Gui.hpp"
#include "Plugin.hpp"
#include "Editor.hpp"
#include <cstring>
GeigerGui::GeigerGui(GeigerPlugin& p) noexcept:plugin_(p) {}
GeigerGui::~GeigerGui(){destroy();}
const char* GeigerGui::preferredApi() const noexcept {
#if JUCE_WINDOWS
    return CLAP_WINDOW_API_WIN32;
#elif JUCE_LINUX
    return plugin_.guiTimerAvailable() ? CLAP_WINDOW_API_X11 : nullptr;
#else
    return nullptr;
#endif
}
bool GeigerGui::isApiSupported(const char* api,bool floating) const noexcept {
    const auto* preferred=preferredApi(); return !floating && api && preferred && std::strcmp(api,preferred)==0;
}
bool GeigerGui::create(const char* api,bool floating) noexcept {
    if(editor_ || !isApiSupported(api,floating)) return false;
    try {
        initialiser_=std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        editor_=std::make_unique<GeigerEditor>(plugin_); applySize(); editor_->setVisible(false);
        if(!plugin_.startGuiTimer()) {destroy(); return false;}
        return true;
    } catch(...) {destroy(); return false;}
}
void GeigerGui::destroy() noexcept {
    plugin_.stopGuiTimer();
    if(editor_) {editor_->setVisible(false); if(editor_->isOnDesktop()) editor_->removeFromDesktop(); editor_.reset();}
    initialiser_.reset();
}
void GeigerGui::applySize() noexcept {if(editor_) editor_->setSize(static_cast<int>(sizing_.logicalWidth()),static_cast<int>(sizing_.logicalHeight()));}
bool GeigerGui::setScale(double scale) noexcept {if(!sizing_.setScale(scale)) return false; applySize(); return true;}
bool GeigerGui::show() noexcept {if(!editor_) return false; editor_->setVisible(true); return true;}
bool GeigerGui::hide() noexcept {if(!editor_) return false; editor_->setVisible(false); return true;}
bool GeigerGui::getSize(std::uint32_t& w,std::uint32_t& h) noexcept {sizing_.getPhysicalSize(w,h); return editor_!=nullptr;}
bool GeigerGui::getResizeHints(clap_gui_resize_hints_t& h) noexcept {h={}; h.can_resize_horizontally=true; h.can_resize_vertically=true; return true;}
bool GeigerGui::adjustSize(std::uint32_t& w,std::uint32_t& h) noexcept {sizing_.adjustPhysicalSize(w,h); return true;}
bool GeigerGui::setSize(std::uint32_t w,std::uint32_t h) noexcept {if(!w || !h) return false; sizing_.setPhysicalSize(w,h); applySize(); return true;}
bool GeigerGui::setParent(const clap_window_t& window) noexcept {
    if(!editor_ || !isApiSupported(window.api,false)) return false;
    void* parent=nullptr;
#if JUCE_WINDOWS
    parent=window.win32;
#elif JUCE_LINUX
    parent=reinterpret_cast<void*>(static_cast<std::uintptr_t>(window.x11));
#endif
    if(!parent) return false;
    if(editor_->isOnDesktop()) editor_->removeFromDesktop();
    editor_->addToDesktop(0,parent); applySize(); return editor_->isOnDesktop();
}
