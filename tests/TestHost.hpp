#pragma once
#include <clap/clap.h>
#include <clap/ext/timer-support.h>
#include <cstring>
struct TestHost {
    clap_host_t host{};
    static const void* CLAP_ABI extension(const clap_host_t*,const char* id) {
        static const clap_host_timer_support_t timers{
            [](const clap_host_t*,std::uint32_t,clap_id* out)->bool {*out=7; return true;},
            [](const clap_host_t*,clap_id)->bool {return true;}
        };
        return std::strcmp(id,CLAP_EXT_TIMER_SUPPORT)==0 ? &timers : nullptr;
    }
    TestHost() {
        host.clap_version=CLAP_VERSION; host.host_data=this;
        host.name="Geiger regression host"; host.vendor="Null Exo"; host.url="https://github.com/001null100/geiger-generator"; host.version="1";
        host.get_extension=extension;
        host.request_restart=[](const clap_host_t*){};
        host.request_process=[](const clap_host_t*){};
        host.request_callback=[](const clap_host_t*){};
    }
};
