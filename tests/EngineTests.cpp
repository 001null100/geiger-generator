#include "Engine.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <chrono>
using namespace geiger;
constexpr std::array<double,10> sampleRates{{1000.,1000.25,8000.,22050.,44100.,48000.123,96000.,192000.,384000.,768000.}};
void checkSample(float v) {
    if (!std::isfinite(v) || std::fpclassify(v)==FP_SUBNORMAL)
        throw std::runtime_error("Non-finite or subnormal output sample");
}
void check(bool v,const char* msg) { if (!v) throw std::runtime_error(msg); }
Values quiet() { auto v=defaults(); v[Background]=0; v[DeadTime]=0; v[Recovery]=0; v[Attack]=0; return v; }
void run(Engine& e,int n) { for (int i=0;i<n;++i) e.tick(); }
int main() {
    try {
        constexpr std::array<std::uint32_t,37> legacyIds{{
            3689929747u, 2624747258u, 532751276u, 3309663248u, 3968804712u, 879334527u, 742440259u, 3218804779u, 4098958921u, 1757669703u, 2193741336u, 3316471812u, 3392211582u, 3637984318u, 541851959u, 1784850826u, 2782838276u, 144860093u, 3746995105u, 1052401384u, 4006486424u, 3249167216u, 1248056913u, 4220174518u, 3710180941u, 210355828u, 3236424531u, 146016693u, 3102311682u, 3750352205u, 3356903962u, 3346684004u, 2396866805u, 921264074u, 2603139783u, 229696167u, 3797350897u
        }};
        for(std::size_t i=0;i<legacyIds.size();++i) check(parameterId(i)==legacyIds[i],"Legacy parameter ID changed");
        for (std::size_t i=0;i<Count;++i) {
            check(parameterId(i)!=0xffffffffu,"Invalid parameter ID");
            for (std::size_t j=i+1;j<Count;++j) check(parameterId(i)!=parameterId(j),"ID collision");
        }
        for (std::size_t i=0;i<presetNames.size();++i) check(sanitize(preset(i))==preset(i),"Invalid preset");
        std::cout<<"PASS stable IDs and factory values\n";
        for (double sr:sampleRates) {
            Engine e; auto v=quiet(); v[Rate]=0; e.configure(v); check(e.prepare(sr),"Supported sample rate rejected"); e.setGate(true);
            for (int i=0;i<10000;++i) {auto f=e.tick(); check(f.left==0 && f.right==0,"Zero input not silent");}
            check(e.detected()==0,"Zero rate generated events");
        }
        std::cout<<"PASS activation and zero-rate silence at ten sample rates (including fractional and 1-768 kHz)\n";
        Engine a; auto v=quiet(); v[Rate]=100; a.configure(v); a.prepare(8000); a.setGate(true);
        std::vector<double> waits; double previous=-1;
        std::vector<double> bins;
        for (int w=0;w<100;++w) {
            auto start=a.detected();
            for (int i=0;i<8000;++i) {auto count=a.detected(); a.tick(); if(a.detected()!=count) {
                double t=a.lastDetectionTime(); if(previous>=0) waits.push_back(t-previous); previous=t;
            }}
            bins.push_back(static_cast<double>(a.detected()-start));
        }
        double mean=0,var=0; for(double x:bins) mean+=x/bins.size(); for(double x:bins) var+=(x-mean)*(x-mean)/bins.size();
        double wm=0,wv=0; for(double x:waits) wm+=x/waits.size(); for(double x:waits) wv+=(x-wm)*(x-wm)/waits.size();
        check(std::abs(mean-100)<4,"Poisson mean outside tolerance");
        check(var/mean>0.6 && var/mean<1.5,"Poisson Fano factor outside tolerance");
        check(std::abs(std::sqrt(wv)/wm-1)<0.07,"Exponential interval CV outside tolerance");
        std::cout<<"PASS Poisson statistics: mean="<<mean<<", Fano="<<var/mean<<", interval CV="<<std::sqrt(wv)/wm<<"\n";
        Engine d; v=quiet(); v[Rate]=1200; v[DeadTime]=190; d.configure(v); d.prepare(8000); d.setGate(true);
        previous=-1;
        for(int i=0;i<8000*60;++i) {auto n=d.detected(); d.tick(); if(d.detected()!=n) {
            double t=d.lastDetectionTime(); if(previous>=0) check(t-previous>=0.00019-1e-10,"Dead time violated"); previous=t;
        }}
        const double expected=1200/(1+1200*0.00019), actual=d.detected()/60.0;
        check(std::abs(actual-expected)/expected<0.025,"Non-paralyzable rate law failed");
        check(d.missed()>0,"Tube missed counter inactive");
        std::cout<<"PASS dead time: observed="<<actual<<", expected="<<expected<<"\n";
        Engine regular; v=quiet(); v[Rate]=8; v[Randomness]=0; regular.configure(v); regular.prepare(8000); regular.setGate(true);
        run(regular,80000); check(regular.detected()>=79 && regular.detected()<=80,"Regular timing failed");
        std::cout<<"PASS regular creative timing\n";
        Engine b,c; v=quiet(); v[Rate]=600; b.configure(v); c.configure(v); b.reset(); c.reset(); b.setGate(true); c.setGate(true);
        for (int i=0;i<100000;++i) {
            if(i%17==0) c.configure(v);
            auto x=b.tick(),y=c.tick(); check(x.left==y.left && x.right==y.right,"Span splitting changed rendering");
        }
        std::cout<<"PASS deterministic reset and span independence\n";
        b.reset(); c.reset(); auto alternate=v; alternate[ClickModel]=5; alternate[Noise]=100; alternate[Variation]=100; alternate[Width]=100; c.configure(alternate);
        for(int i=0;i<50000;++i) {b.tick(); c.tick(); check(b.detected()==c.detected(),"Timbre changed primary timing");}
        std::cout<<"PASS independent timing and sound random streams\n";
        std::array<double,clickProfiles.size()> energies{};
        for (int m=0;m<static_cast<int>(clickProfiles.size());++m) {
            Engine e; v=quiet(); v[Rate]=0; v[ClickModel]=m; e.configure(v); e.reset(); e.testClick();
            for(int i=0;i<24000;++i) {auto f=e.tick(); energies[m]+=f.left*f.left; check(f.left==f.right,"Mono image not mono");}
            check(energies[m]>0.00001,"Click model silent");
            for(int j=0;j<m;++j) check(std::abs(energies[m]-energies[j])>0.00001,"Identical click models");
        }
        std::cout<<"PASS eight distinct click recipes and mono coherence\n";
        const auto clock=std::chrono::steady_clock::now();
        for(double sr:sampleRates) {
            Engine e; auto extreme=defaults(); for(std::size_t i=0;i<Count;++i) extreme[i]=definitions[i].maximum;
            e.configure(extreme); check(e.prepare(sr),"Extreme sample rate rejected"); e.setGate(true,1,60);
            for(int i=0;i<static_cast<int>(sr*0.5);++i) {
                if(i%521==0) {extreme[ClickModel]=(i/521)%static_cast<int>(clickProfiles.size()); extreme[Speaker]=(i/521)%5; e.configure(extreme);}
                auto f=e.tick(); checkSample(f.left); checkSample(f.right);
                check(std::abs(f.left)<=0.97001f && std::abs(f.right)<=0.97001f,"Output safety bound violated");
            }
            extreme[Power]=0; e.configure(extreme); run(e,static_cast<int>(sr)); auto f=e.tick();
            check(std::abs(f.left)<1e-12 && std::abs(f.right)<1e-12,"Power-off did not fade silent");
        }
        std::cout<<"PASS extreme automation, finite output and power fade ("<<std::chrono::duration<double>(std::chrono::steady_clock::now()-clock).count()<<" s)\n";
        for (double sr:sampleRates) {
            Engine e; auto tail=quiet(); tail[Rate]=0; tail[Decay]=0.5; tail[Output]=0;
            e.configure(tail); check(e.prepare(sr),"Tail sample rate rejected"); e.testClick();
            for (int i=0;i<static_cast<int>(sr);++i) {auto f=e.tick(); checkSample(f.left); checkSample(f.right);}
            auto f=e.tick(); check(f.left==0 && f.right==0,"Pulse tail did not settle to exact zero");
        }
        std::cout<<"PASS finite, non-subnormal pulse tails without relying on host FTZ settings\n";
        for(double x:{-100.0,-1.0,-1e-12,0.0,1e-12,0.1,1.0,100.0}) {
            check(std::abs(antialiasedClip(x,x)-softClip(x))<1e-14,"ADAA equal-input instability");
            check(std::abs(antialiasedClip(x,-x))<1e-14,"ADAA sign symmetry failed");
            check(std::abs(antialiasedClip(x,0.5*x))<=1,"ADAA bound violated");
        }
        std::cout<<"PASS antialiased saturation limits, symmetry and cancellation stability\n";
        for(double sr:{44100.,48000.,96000.}) {
            Engine raw,balanced; auto dense=quiet(); dense[Rate]=12000; dense[Decay]=65;
            dense[Noise]=100; dense[Drive]=18; dense[Output]=0; dense[Compensation]=0;
            raw.configure(dense); check(raw.prepare(sr),"Dense activation failed"); raw.setGate(true);
            dense[Compensation]=100; balanced.configure(dense); check(balanced.prepare(sr),"Balanced activation failed"); balanced.setGate(true);
            double er=0,eb=0;
            for(int i=0;i<static_cast<int>(sr*3);++i) {
                const auto a=raw.tick(),b=balanced.tick(); checkSample(a.left); checkSample(b.left);
                check(raw.detected()==balanced.detected(),"Density balance changed the detector");
                if(i>sr) {er+=a.left*a.left; eb+=b.left*b.left;}
            }
            check(eb>1e-6 && eb<er*0.8,"Density balance failed to preserve headroom");
            check(balanced.balanceGain()>0 && balanced.balanceGain()<0.3,"Dense gain meter invalid");
        }
        std::cout<<"PASS high-intensity headroom and detector independence at 44.1/48/96 kHz\n";
        for(std::size_t index=0;index<presetNames.size();++index) {
            const auto factory=preset(index); check(matchesPreset(factory,index),"Preset matching failed");
            auto protectedEdit=factory; protectedEdit[Output]=-55; protectedEdit[Power]=0; protectedEdit[RunMode]=2; protectedEdit[Seed]=781;
            check(matchesPreset(protectedEdit,index),"Protected controls incorrectly mark a preset edited");
            auto changed=factory; changed[Decay]=factory[Decay]==2 ? 3 : 2;
            check(!matchesPreset(changed,index),"Edited preset incorrectly recognized as factory");
            Engine e; e.configure(factory); check(e.prepare(48000),"Preset activation failed"); e.setGate(true); e.testClick();
            double energy=0;
            for(int n=0;n<48000;++n) {const auto f=e.tick(); checkSample(f.left); checkSample(f.right); energy+=f.left*f.left+f.right*f.right;}
            check(energy>1e-7,"Factory preset silent");
        }
        std::cout<<"PASS all 20 factory sounds, edited detection and protected-control semantics\n";
        auto bad=defaults(); bad[Rate]=std::numeric_limits<double>::quiet_NaN(); bad[Decay]=std::numeric_limits<double>::infinity();
        a.configure(bad); check(a.values()==defaults(),"Non-finite parameter sanitation failed");
        for (double sr:{-1.,0.,999.,768001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            check(!a.prepare(sr),"Invalid or out-of-range sample rate accepted");
        std::cout<<"PASS invalid-input rejection\nAll DSP checks passed.\n";
    } catch (const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1;}
}
