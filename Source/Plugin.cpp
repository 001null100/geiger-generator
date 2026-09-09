#include "Plugin.hpp"
#include "Gui.hpp"
#include <clap/plugin-features.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

const clap_plugin_descriptor_t& GeigerPlugin::descriptor() noexcept {
    static const char* const features[]{CLAP_PLUGIN_FEATURE_INSTRUMENT,CLAP_PLUGIN_FEATURE_SYNTHESIZER,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
    static const clap_plugin_descriptor_t d{CLAP_VERSION,"dev.nullexo.geiger-generator","Geiger Generator","Null Exo",
        "https://github.com/001null100/geiger-generator","","","1.0.0",
        "A radiation-inspired stochastic click instrument: detector, circuit and transducer.",features};
    return d;
}
GeigerPlugin::GeigerPlugin(const clap_host_t* host):Plugin(&descriptor(),host),host_(host) {
    for(std::size_t i=0;i<geiger::Count;++i) {
        const auto& d=geiger::definitions[i];
        static const char* modules[]{"Field","Circuit","Speaker","Behavior"};
        auto s=nullclap::ParameterSpec::continuous(geiger::parameterId(i),d.name,d.page<0 ? "Main" : modules[d.page],d.minimum,d.maximum,d.initial);
        if(d.choices[0]) {
            std::vector<std::string> labels;
            std::string text=d.choices;
            for(std::size_t from=0;;) {auto to=text.find('|',from); labels.push_back(text.substr(from,to-from)); if(to==std::string::npos) break; from=to+1;}
            s=nullclap::ParameterSpec::choice(geiger::parameterId(i),d.name,s.module,std::move(labels),static_cast<std::size_t>(d.initial));
        }
        s.flags|=CLAP_PARAM_REQUIRES_PROCESS;
        if(i==geiger::Seed) s.flags=(CLAP_PARAM_IS_AUTOMATABLE|CLAP_PARAM_IS_STEPPED|CLAP_PARAM_REQUIRES_PROCESS);
        s.unit=d.unit; s.displayPrecision=(i==geiger::Seed || d.choices[0]) ? 0 : (d.maximum<=20 ? 2 : 1);
        parameters().add(std::move(s));
    }
    auto output=nullclap::AudioPortSpec::stereo(nullclap::stableId("geiger.audio.stereo-out"),"Stereo output",true);
    output.flags|=CLAP_AUDIO_PORT_SUPPORTS_64BITS; audioPorts().addOutput(std::move(output));
    notePorts().addInput(nullclap::NotePortSpec::midi(nullclap::stableId("geiger.note.midi-in"),"Gate and pitch"));
    const std::array<std::array<geiger::Param,8>,3> pages{{
        {{geiger::Rate,geiger::Randomness,geiger::DeadTime,geiger::Wander,geiger::MotionDepth,geiger::ClickModel,geiger::Width,geiger::Output}},
        {{geiger::PulseWidth,geiger::Pitch,geiger::Decay,geiger::Sharpness,geiger::Noise,geiger::Body,geiger::Variation,geiger::Drive}},
        {{geiger::Speaker,geiger::Resonance,geiger::Tone,geiger::Distance,geiger::Hiss,geiger::Hum,geiger::Compensation,geiger::Output}}
    }};
    const char* names[]{"Performance","Click circuit","Transducer"};
    const char* ids[]{"geiger.remote.performance","geiger.remote.circuit","geiger.remote.transducer"};
    for(std::size_t n=0;n<pages.size();++n) {
        nullclap::RemoteControlPage page; page.id=nullclap::stableId(ids[n]); page.section="Geiger Generator"; page.name=names[n];
        for(std::size_t i=0;i<8;++i) page.parameters[i]=geiger::parameterId(pages[n][i]);
        remoteControls().add(std::move(page));
    }
    setGuiDelegate(std::make_unique<GeigerGui>(*this));
}
GeigerPlugin::~GeigerPlugin() {setGuiDelegate(nullptr); stopGuiTimer();}
bool GeigerPlugin::onInit() noexcept {
    timerHost_=host_->get_extension ? static_cast<const clap_host_timer_support_t*>(host_->get_extension(host_,CLAP_EXT_TIMER_SUPPORT)) : nullptr;
    return true;
}
geiger::Values GeigerPlugin::readValues() const noexcept {
    geiger::Values v{}; for(std::size_t i=0;i<geiger::Count;++i) v[i]=parameters().effectiveValue(geiger::parameterId(i)); return v;
}
bool GeigerPlugin::onActivate(double sr,std::uint32_t,std::uint32_t) noexcept {
    scopeStride_=static_cast<std::uint32_t>(std::max(1.0,sr/750.0));
    meterDecay_=static_cast<float>(std::exp(-1.0/(std::max(1.0,sr)*0.08)));
    engine_.configure(readValues()); const bool ok=engine_.prepare(sr); onReset(); return ok;
}
void GeigerPlugin::onReset() noexcept {
    engine_.reset(); notes_={}; sustain_={}; noteOrder_=0; playing_=false; restartTransport_=false; blockTransportReady_=false;
    scopePeak_=meterPeak_=scopeMin_=scopeMax_=leftPeak_=rightPeak_=0; scopeSamples_=scopeHead_=0;
    requestedClicks_.store(0,std::memory_order_relaxed);
    telemetry_.cps=0; telemetry_.incoming=0; telemetry_.peak=0; telemetry_.detected=0; telemetry_.missed=0; telemetry_.running=false;
    telemetry_.balance=1; telemetry_.leftPeak=0; telemetry_.rightPeak=0;
    for(auto& x:telemetry_.scopeMin) x.store(0,std::memory_order_relaxed);
    for(auto& x:telemetry_.scopeMax) x.store(0,std::memory_order_relaxed);
    for(auto& x:telemetry_.scope) x.store(0,std::memory_order_relaxed);
    telemetry_.scopeHead.store(0,std::memory_order_release);
}
void GeigerPlugin::testClick() noexcept {
    auto n=requestedClicks_.load(std::memory_order_relaxed);
    while(n<8 && !requestedClicks_.compare_exchange_weak(n,n+1,std::memory_order_relaxed)) {}
    if(host_->request_process) host_->request_process(host_);
}
void GeigerPlugin::panic() noexcept {
    requestedPanic_.store(true,std::memory_order_release);
    beginParameterGesture(geiger::parameterId(geiger::Power));
    setParameterFromGui(geiger::parameterId(geiger::Power),0);
    endParameterGesture(geiger::parameterId(geiger::Power));
    if(host_->request_process) host_->request_process(host_);
}
void GeigerPlugin::applyPreset(std::size_t index) noexcept {
    if(index>=geiger::presetNames.size()) return;
    const auto v=geiger::preset(index);
    for(std::size_t i=0;i<geiger::Count;++i) {
        // Presets must not raise the user's level, change gating, or unexpectedly power on.
        if(geiger::presetProtected(i)) continue;
        const auto id=geiger::parameterId(i); beginParameterGesture(id); setParameterFromGui(id,v[i]); endParameterGesture(id);
    }
    presetIndex_.store(static_cast<int>(index),std::memory_order_relaxed);
    markStateDirty();
}
geiger::Values GeigerPlugin::currentValues() const noexcept {
    geiger::Values v{};
    for(std::size_t i=0;i<geiger::Count;++i) v[i]=parameters().value(geiger::parameterId(i));
    return v;
}
int GeigerPlugin::presetIndex() const noexcept {
    const auto v=currentValues();
    const int anchor=presetIndex_.load(std::memory_order_relaxed);
    if(anchor>=0 && geiger::matchesPreset(v,static_cast<std::size_t>(anchor))) return anchor;
    const int match=geiger::matchingPreset(v);
    return match>=0 ? match : anchor;
}
bool GeigerPlugin::presetIsModified() const noexcept {
    const int index=presetIndex();
    return index<0 || !geiger::matchesPreset(currentValues(),static_cast<std::size_t>(index));
}
std::string GeigerPlugin::presetName() const {
    const int index=presetIndex();
    if(index<0) return "Custom sound";
    return std::string(geiger::presetNames[static_cast<std::size_t>(index)])+(presetIsModified() ? " *" : "");
}
std::vector<std::byte> GeigerPlugin::saveExtraState() const {
    const auto index=static_cast<std::uint32_t>(presetIndex()+1);
    std::vector<std::byte> bytes{std::byte{'G'},std::byte{'G'},std::byte{'P'},std::byte{1}};
    for(int shift=0;shift<32;shift+=8) bytes.push_back(static_cast<std::byte>((index>>shift)&255));
    return bytes;
}
bool GeigerPlugin::loadExtraState(std::span<const std::byte> bytes) {
    if(bytes.empty()) { // Preview 0.1 project: infer the name from its controls.
        presetIndex_.store(geiger::matchingPreset(currentValues()),std::memory_order_relaxed); return true;
    }
    if(bytes.size()!=8 || bytes[0]!=std::byte{'G'} || bytes[1]!=std::byte{'G'} || bytes[2]!=std::byte{'P'} || bytes[3]!=std::byte{1}) return false;
    std::uint32_t index=0;
    for(int i=0;i<4;++i) index|=static_cast<std::uint32_t>(bytes[4+i])<<(i*8);
    if(index>geiger::presetNames.size()) return false;
    presetIndex_.store(static_cast<int>(index)-1,std::memory_order_relaxed); return true;
}
void GeigerPlugin::initialiseBlockTransport() noexcept {
    // The block snapshot precedes sample-offset events, including events at zero.
    // Do not reapply the snapshot after an offset-zero transport event.
    if(!blockTransportReady_ && currentProcess()) {
        blockTransportReady_=true;
        if(currentProcess()->transport) updateTransport(*currentProcess()->transport);
    }
}
void GeigerPlugin::updateTransport(const clap_event_transport_t& t) noexcept {
    const bool now=(t.flags&CLAP_TRANSPORT_IS_PLAYING)!=0;
    if(now && !playing_) restartTransport_=true;
    playing_=now;
}
void GeigerPlugin::midi(const clap_event_midi_t& e) noexcept {
    if(e.port_index!=0) return;
    const auto type=e.data[0]&0xf0, ch=e.data[0]&15;
    const auto key=e.data[1]&127, value=e.data[2]&127;
    auto& n=notes_[static_cast<std::size_t>(ch)*128+key];
    if(type==0x90 && value>0) {
        if(n.held<255) ++n.held;
        n.sustained=false; n.velocity=value/127.0; n.order=++noteOrder_;
    } else if(type==0x80 || (type==0x90 && value==0)) {
        if(n.held>0) {
            --n.held;
            if(n.held==0) n.sustained=sustain_[ch];
        }
    } else if(type==0xb0) {
        if(key==64) {
            sustain_[ch]=value>=64;
            if(!sustain_[ch]) for(int k=0;k<128;++k) notes_[ch*128+k].sustained=false;
        } else if(key==120 || key==123) {
            for(int k=0;k<128;++k) {auto& x=notes_[ch*128+k]; x.sustained=key==123 && sustain_[ch] && (x.held || x.sustained); x.held=0;}
            if(key==120) {sustain_[ch]=false; engine_.reset();}
        } else if(key==121) {
            sustain_[ch]=false; for(int k=0;k<128;++k) notes_[ch*128+k].sustained=false;
        }
    }
}
void GeigerPlugin::onEvent(const clap_event_header_t& h) noexcept {
    initialiseBlockTransport();
    if(h.space_id!=CLAP_CORE_EVENT_SPACE_ID) return;
    if(h.type==CLAP_EVENT_MIDI && h.size>=sizeof(clap_event_midi_t)) midi(reinterpret_cast<const clap_event_midi_t&>(h));
    if(h.type==CLAP_EVENT_TRANSPORT && h.size>=sizeof(clap_event_transport_t)) updateTransport(reinterpret_cast<const clap_event_transport_t&>(h));
}
void GeigerPlugin::processAudio(const clap_process_t& process,std::uint32_t start,std::uint32_t end) noexcept {
    initialiseBlockTransport();
    const auto v=readValues(); engine_.configure(v);
    if(requestedPanic_.exchange(false,std::memory_order_acq_rel)) {engine_.reset(); notes_={}; sustain_={};}
    const int mode=static_cast<int>(v[geiger::RunMode]);
    if(restartTransport_) {if(mode==1 && v[geiger::Retrigger]>0.5) engine_.reset(); restartTransport_=false;}
    bool gate=mode==0 || (mode==1 && playing_); double velocity=1,pitch=0;
    if(mode==2) {
        const Note* newest=nullptr; int newestKey=60;
        for(std::size_t i=0;i<notes_.size();++i) {
            const auto& n=notes_[i]; if((n.held || n.sustained) && (!newest || n.order>newest->order)) {newest=&n; newestKey=static_cast<int>(i%128);}
        }
        gate=newest!=nullptr;
        if(newest) {velocity=newest->velocity; if(v[geiger::MidiPitch]>0.5) pitch=newestKey-60;}
    }
    engine_.setGate(gate,velocity,pitch);
    auto shots=requestedClicks_.exchange(0,std::memory_order_acq_rel);
    while(shots-->0) engine_.testClick();
    clap_audio_buffer_t* output=(process.audio_outputs_count>0 && process.audio_outputs) ? &process.audio_outputs[0] : nullptr;
    if(output) output->constant_mask=0;
    for(auto frame=start;frame<end;++frame) {
        const auto f=engine_.tick();
        if(output) for(std::uint32_t ch=0;ch<output->channel_count;++ch) {
            const auto sample=ch==0 ? f.left : (ch==1 ? f.right : 0.0f);
            if(output->data32 && output->data32[ch]) output->data32[ch][frame]=sample;
            if(output->data64 && output->data64[ch]) output->data64[ch][frame]=sample;
        }
        const auto peak=std::max(std::abs(f.left),std::abs(f.right));
        scopePeak_=std::max(scopePeak_,peak); meterPeak_=std::max(peak,meterPeak_*meterDecay_);
        leftPeak_=std::max(std::abs(f.left),leftPeak_*meterDecay_); rightPeak_=std::max(std::abs(f.right),rightPeak_*meterDecay_);
        scopeMin_=std::min(scopeMin_,std::min(f.left,f.right)); scopeMax_=std::max(scopeMax_,std::max(f.left,f.right));
        if(++scopeSamples_>=scopeStride_) {
            telemetry_.scope[scopeHead_%128].store(scopePeak_,std::memory_order_relaxed);
            telemetry_.scopeMin[scopeHead_%128].store(scopeMin_,std::memory_order_relaxed);
            telemetry_.scopeMax[scopeHead_%128].store(scopeMax_,std::memory_order_relaxed);
            ++scopeHead_; telemetry_.scopeHead.store(scopeHead_,std::memory_order_release);
            scopeSamples_=0; scopePeak_=scopeMin_=scopeMax_=0;
        }
    }
    telemetry_.cps.store(static_cast<float>(engine_.measuredCps()),std::memory_order_relaxed);
    telemetry_.incoming.store(static_cast<float>(engine_.incomingCps()),std::memory_order_relaxed);
    telemetry_.balance.store(static_cast<float>(engine_.balanceGain()),std::memory_order_relaxed);
    telemetry_.leftPeak.store(leftPeak_,std::memory_order_relaxed); telemetry_.rightPeak.store(rightPeak_,std::memory_order_relaxed);
    telemetry_.peak.store(meterPeak_,std::memory_order_relaxed);
    telemetry_.detected.store(engine_.detected(),std::memory_order_relaxed);
    telemetry_.missed.store(engine_.missed(),std::memory_order_relaxed);
    telemetry_.running.store(gate && v[geiger::Power]>0.5,std::memory_order_relaxed);
}
bool GeigerPlugin::guiTimerAvailable() const noexcept {
#if JUCE_LINUX
    return timerHost_ && timerHost_->register_timer && timerHost_->unregister_timer;
#else
    return true;
#endif
}
bool GeigerPlugin::startGuiTimer() noexcept {
#if JUCE_LINUX
    if(!guiTimerAvailable()) return false;
    if(timerId_!=CLAP_INVALID_ID) return true;
    return timerHost_->register_timer(host_,16,&timerId_);
#else
    return true;
#endif
}
void GeigerPlugin::stopGuiTimer() noexcept {
#if JUCE_LINUX
    if(timerHost_ && timerId_!=CLAP_INVALID_ID) timerHost_->unregister_timer(host_,timerId_);
#endif
    timerId_=CLAP_INVALID_ID;
}
void GeigerPlugin::onTimer(clap_id id) noexcept {
#if JUCE_LINUX
    // X11 hosts do not pump JUCE's event queue. Use their main-thread CLAP timer.
    if(id==timerId_ && !pumping_) {
        pumping_=true;
        if(auto* mm=juce::MessageManager::getInstanceWithoutCreating()) mm->runDispatchLoopUntil(1);
        pumping_=false;
    }
#else
    (void)id;
#endif
}
