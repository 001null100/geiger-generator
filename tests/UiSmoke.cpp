#include "Plugin.hpp"
#include "Editor.hpp"
#include "TestHost.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    TestHost host; auto instance=std::make_unique<GeigerPlugin>(&host.host);
    const auto* clap=instance->clapPlugin(); if(!clap->init(clap)) return 1;
    {
        GeigerEditor editor(*instance);
        juce::File directory(argc>1 ? argv[1] : "ui-previews"); directory.createDirectory();
        for(int width:{900,1120,1600}) for(int page=0;page<4;++page) {
            editor.setSize(width,width==900 ? 660 : (width==1120 ? 760 : 1000)); editor.selectPage(page);
            if(!editor.layoutIsValid()) {std::cerr<<"Overlapping/out-of-bounds controls at "<<width<<" page "<<page<<"\n"; return 2;}
            auto image=editor.createComponentSnapshot(editor.getLocalBounds());
            const auto file=directory.getChildFile("geiger-"+juce::String(width)+"-page-"+juce::String(page+1)+".png");
            if(file.existsAsFile() && !file.deleteFile()) return 3;
            juce::FileOutputStream stream(file); if(!stream.openedOk()) return 3;
            juce::PNGImageFormat png; if(!png.writeImageToStream(image,stream)) return 4;
        }
        // Exercise the actual preset selector and editor-refresh path.
        juce::ComboBox* presets=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i) {
            auto* c=dynamic_cast<juce::ComboBox*>(editor.getChildComponent(i));
            if(c && c->getTitle()=="Current preset") presets=c;
        }
        if(!presets) return 5;
        presets->setSelectedId(10,juce::sendNotificationSync); editor.refreshDisplay();
        if(editor.displayedPresetName()!="Velvet reactor") return 6;
        instance->setParameterFromGui(geiger::parameterId(geiger::Decay),23); editor.refreshDisplay();
        if(editor.displayedPresetName()!="Velvet reactor *") return 7;
        editor.setSize(1120,760);
        for(std::size_t i=0;i<geiger::Count;++i) {
            editor.selectPage(std::max(0,geiger::definitions[i].page)); editor.focusParameter(i);
            instance->setParameterFromGui(geiger::parameterId(i),geiger::definitions[i].maximum);
            editor.refreshDisplay();
            if(!editor.layoutIsValid()) {std::cerr<<"Invalid parameter preview "<<i<<"\n";return 8;}
            instance->setParameterFromGui(geiger::parameterId(i),geiger::definitions[i].initial);
        }
        instance->applyPreset(11); editor.selectPage(1); editor.focusParameter(geiger::Decay); editor.refreshDisplay();
        auto image=editor.createComponentSnapshot(editor.getLocalBounds());
        const auto detailFile=directory.getChildFile("geiger-circuit-detail.png");
        if(detailFile.existsAsFile() && !detailFile.deleteFile()) return 9;
        juce::FileOutputStream detail(detailFile); if(!detail.openedOk()) return 9;
        juce::PNGImageFormat png; if(!png.writeImageToStream(image,detail)) return 9;
        // Reopening the editor retains the selected factory name.
        GeigerEditor reopened(*instance); reopened.refreshDisplay();
        if(reopened.displayedPresetName()!="Ceramic drizzle") return 10;
    }
    instance.release(); clap->destroy(clap);
    std::cout<<"PASS four UI pages at three sizes, non-overlap, all parameter previews, preset interaction and editor reopening; snapshots written\n";
}
