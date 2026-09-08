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
            if(!editor.layoutIsValid()) {std::cerr<<"Out-of-bounds controls\n"; return 2;}
            auto image=editor.createComponentSnapshot(editor.getLocalBounds());
            const auto file=directory.getChildFile("geiger-"+juce::String(width)+"-page-"+juce::String(page+1)+".png");
            juce::FileOutputStream stream(file); if(!stream.openedOk()) return 3;
            juce::PNGImageFormat png; if(!png.writeImageToStream(image,stream)) return 4;
        }
    }
    instance.release(); clap->destroy(clap);
    std::cout<<"PASS four UI pages at three sizes; snapshots written\n";
}
