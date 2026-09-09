#include "Engine.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <cctype>
namespace fs=std::filesystem;
void le(std::ostream& out,std::uint32_t value,int bytes) {for(int i=0;i<bytes;++i) out.put(static_cast<char>((value>>(8*i))&255));}
int main(int argc,char** argv) {
    try {
        const fs::path directory=argc>1 ? argv[1] : "audio-demos"; fs::create_directories(directory);
        constexpr int sr=48000,frames=sr*5;
        std::ofstream index(directory/"PRESETS.txt");
        for(std::size_t i=0;i<geiger::presetNames.size();++i) {
            geiger::Engine e; e.configure(geiger::preset(i)); e.prepare(sr); e.setGate(true);
            std::string name=geiger::presetNames[i];
            for(auto& c:name) if(!std::isalnum(static_cast<unsigned char>(c))) c='-';
            std::ostringstream number; number<<std::setw(2)<<std::setfill('0')<<(i+1);
            const auto file=directory/(number.str()+"-"+name+".wav");
            index<<file.filename().string()<<"\n"<<geiger::presetDescriptions[i]<<"\n\n";
            std::ofstream out(file,std::ios::binary); if(!out) throw std::runtime_error("Cannot write WAV");
            out.write("RIFF",4); le(out,36+frames*4,4); out.write("WAVEfmt ",8); le(out,16,4); le(out,1,2); le(out,2,2);
            le(out,sr,4); le(out,sr*4,4); le(out,4,2); le(out,16,2); out.write("data",4); le(out,frames*4,4);
            for(int n=0;n<frames;++n) {auto f=e.tick(); double fade=std::min(1.0,std::min(n,frames-1-n)/240.0); for(float s:{f.left,f.right}) le(out,static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(s*fade,-1.0,1.0)*32767)),2);}
            if(!out) throw std::runtime_error("Incomplete WAV write");
        }
        std::cout<<"Rendered "<<geiger::presetNames.size()<<" five-second named preset examples (48 kHz / stereo / 16-bit).\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n"; return 1;}
}
