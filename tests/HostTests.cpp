#include "Plugin.hpp"
#include "TestHost.hpp"
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool v,const char* message){if(!v) throw std::runtime_error(message);}
struct Events {
    std::vector<const clap_event_header_t*> items;
    clap_input_events_t input{this,[](const clap_input_events_t* p){return static_cast<std::uint32_t>(static_cast<Events*>(p->ctx)->items.size());},
        [](const clap_input_events_t* p,std::uint32_t i){return static_cast<Events*>(p->ctx)->items.at(i);}};
};
clap_event_param_value_t parameter(geiger::Param id,double value,std::uint32_t time=0) {
    clap_event_param_value_t e{}; e.header={sizeof(e),time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};
    e.param_id=geiger::parameterId(id); e.note_id=-1; e.port_index=-1; e.channel=-1; e.key=-1; e.value=value; return e;
}
clap_event_midi_t note(std::uint8_t status,std::uint8_t key,std::uint8_t value,std::uint32_t time=0) {
    clap_event_midi_t e{}; e.header={sizeof(e),time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_MIDI,0}; e.port_index=0;
    e.data[0]=status; e.data[1]=key; e.data[2]=value; return e;
}
int main() {
    try {
        TestHost host; auto instance=std::make_unique<GeigerPlugin>(&host.host); const auto* plugin=instance->clapPlugin();
        require(plugin->init(plugin),"init failed");
        const auto* params=static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin,CLAP_EXT_PARAMS));
        const auto* state=static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin,CLAP_EXT_STATE));
        require(params && state && params->count(plugin)==geiger::Count,"Missing extensions or controls");
        for(std::uint32_t i=0;i<geiger::Count;++i) {clap_param_info_t info{}; require(params->get_info(plugin,i,&info),"Parameter info failed"); require(info.id==geiger::parameterId(i),"Parameter mapping changed");}
        auto mode=parameter(geiger::RunMode,2),rate=parameter(geiger::Rate,1000),bg=parameter(geiger::Background,0),attack=parameter(geiger::Attack,0),release=parameter(geiger::Release,1);
        Events configure; configure.items={&mode.header,&rate.header,&bg.header,&attack.header,&release.header};
        params->flush(plugin,&configure.input,nullptr);
        require(plugin->activate(plugin,48000,1,512),"activate failed"); require(plugin->start_processing(plugin),"start failed");
        std::array<float,512> l{},r{}; float* channels[]{l.data(),r.data()}; clap_audio_buffer_t output{}; output.channel_count=2; output.data32=channels;
        Events empty; clap_process_t process{}; process.frames_count=512; process.audio_outputs_count=1; process.audio_outputs=&output; process.in_events=&empty.input; process.steady_time=-1;
        plugin->process(plugin,&process); require(instance->telemetry().detected.load()==0,"MIDI gate starts open");
        auto on=note(0x90,60,127,128); Events notes; notes.items={&on.header}; process.in_events=&notes.input;
        plugin->process(plugin,&process); require(std::all_of(l.begin(),l.begin()+128,[](float x){return x==0;}),"MIDI trigger arrived before sample offset");
        process.in_events=&empty.input; for(int i=0;i<30;++i) plugin->process(plugin,&process);
        require(instance->telemetry().detected.load()>100,"MIDI gate produced no detections");
        auto pedal=note(0xb0,64,127),off=note(0x80,60,0); notes.items={&pedal.header,&off.header}; process.in_events=&notes.input; plugin->process(plugin,&process);
        auto before=instance->telemetry().detected.load(); process.in_events=&empty.input; for(int i=0;i<10;++i) plugin->process(plugin,&process);
        require(instance->telemetry().detected.load()>before,"Sustain did not hold the gate");
        auto pedalUp=note(0xb0,64,0); notes.items={&pedalUp.header}; process.in_events=&notes.input; plugin->process(plugin,&process);
        process.in_events=&empty.input; for(int i=0;i<10;++i) plugin->process(plugin,&process);
        before=instance->telemetry().detected.load(); for(int i=0;i<20;++i) plugin->process(plugin,&process);
        require(instance->telemetry().detected.load()==before,"MIDI gate stuck after sustain release");
        // Duplicate/unmatched note-off under a new sustain pedal must not resurrect an old note.
        notes.items={&pedal.header,&off.header}; process.in_events=&notes.input; plugin->process(plugin,&process);
        process.in_events=&empty.input; plugin->process(plugin,&process);
        require(!instance->telemetry().running.load(),"Stray note-off resurrected a sustained note");
        notes.items={&pedalUp.header}; process.in_events=&notes.input; plugin->process(plugin,&process);
        process.in_events=&empty.input;
        std::array<double,512> dl{},dr{}; double* doubles[]{dl.data(),dr.data()}; output.data32=nullptr; output.data64=doubles;
        instance->testClick(); plugin->process(plugin,&process);
        require(std::any_of(dl.begin(),dl.end(),[](double x){return std::abs(x)>1e-7;}),"64-bit audition output silent");
        for(double x:dl) require(std::isfinite(x),"64-bit output nonfinite");
        // Offset-zero transport events override the block's initial snapshot.
        clap_event_transport_t stopped{},playing{};
        stopped.header={sizeof(stopped),0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_TRANSPORT,0};
        playing=stopped; playing.flags=CLAP_TRANSPORT_IS_PLAYING;
        mode=parameter(geiger::RunMode,1); notes.items={&mode.header,&playing.header};
        process.transport=&stopped; process.in_events=&notes.input;
        plugin->process(plugin,&process);
        require(instance->telemetry().running.load(),"Block snapshot overwrote offset-zero Play event");
        // Mid-block Stop closes the gate at the original sample offset.
        process.transport=&playing; stopped.header.time=256; notes.items={&stopped.header};
        plugin->process(plugin,&process);
        require(!instance->telemetry().running.load(),"Mid-block Stop was ignored");
        process.transport=nullptr; process.in_events=&empty.input;
        plugin->stop_processing(plugin); plugin->deactivate(plugin);
        std::vector<std::byte> saved;
        clap_ostream_t stream{&saved,[](const clap_ostream_t* s,const void* p,std::uint64_t n)->std::int64_t {
            auto& bytes=*static_cast<std::vector<std::byte>*>(s->ctx); auto first=static_cast<const std::byte*>(p); bytes.insert(bytes.end(),first,first+n); return static_cast<std::int64_t>(n);
        }};
        require(state->save(plugin,&stream),"State save failed");
        auto changed=parameter(geiger::Rate,2); Events change; change.items={&changed.header}; params->flush(plugin,&change.input,nullptr);
        struct Reader {const std::vector<std::byte>* bytes; std::size_t offset=0;} reader{&saved};
        clap_istream_t input{&reader,[](const clap_istream_t* s,void* p,std::uint64_t n)->std::int64_t {
            auto& rd=*static_cast<Reader*>(s->ctx); auto count=std::min<std::uint64_t>(n,rd.bytes->size()-rd.offset); std::memcpy(p,rd.bytes->data()+rd.offset,static_cast<std::size_t>(count)); rd.offset+=static_cast<std::size_t>(count); return static_cast<std::int64_t>(count);
        }};
        require(state->load(plugin,&input),"State load failed"); double restored=0; params->get_value(plugin,geiger::parameterId(geiger::Rate),&restored); require(restored==1000,"State did not restore native value");
        for (double sr:{1000.,1000.25,8000.,44100.,48000.123,96000.,384000.,768000.}) {
            require(plugin->activate(plugin,sr,1,512),"Host sample-rate reactivation rejected");
            require(plugin->start_processing(plugin),"Host sample-rate processing failed");
            instance->testClick();
            for (int block=0;block<4;++block) {
                require(plugin->process(plugin,&process)!=CLAP_PROCESS_ERROR,"Host sample-rate process error");
                for (double x:dl) require(std::isfinite(x) && std::fpclassify(x)!=FP_SUBNORMAL,"Invalid sample after rate change");
                for (double x:dr) require(std::isfinite(x) && std::fpclassify(x)!=FP_SUBNORMAL,"Invalid right sample after rate change");
            }
            plugin->stop_processing(plugin); plugin->deactivate(plugin);
        }
        std::cout<<"PASS actual CLAP reactivation from 1 to 768 kHz, including fractional rates\n";
        instance.release(); plugin->destroy(plugin);
        std::cout<<"PASS CLAP metadata, sample-offset transport/MIDI, sustain edge cases, 64-bit audition and state round trip\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1;}
}
