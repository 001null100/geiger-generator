#include "Engine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace geiger {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
double db(double v) noexcept { return std::pow(10.0,v/20.0); }
double clean(double v) noexcept { return std::abs(v)<1e-18 ? 0.0 : v; }
struct Model { double f1,f2,pulse,ring,noise; };
constexpr Model models[]={{2100,4300,0.8,0.42,0.12},{3600,6700,0.35,0.9,0.05},
    {950,2300,1.0,0.50,0.2},{650,3100,1.1,0.75,0.28},{1250,2800,0.4,0.28,0.18},{2800,7210,0.18,1.05,0.03}};
}
Engine::Engine() noexcept { prepare(48000); }
bool Engine::prepare(double sr) noexcept {
    if (!std::isfinite(sr) || sr<8000 || sr>384000) return false;
    sr_=sr; updateCoefficients(); reset(); return true;
}
void Engine::configure(Values v) noexcept {
    v=sanitize(v);
    if (v==values_) return;
    const bool newSeed=v[Seed]!=values_[Seed];
    values_=v; updateCoefficients();
    if (newSeed) reseed();
}
void Engine::reseed() noexcept {
    const auto s=static_cast<std::uint64_t>(values_[Seed]);
    timing_.seed(s); color_.seed(s^0xa45b83ULL); field_.seed(s^0x382ae5ULL); noise_.seed(s^0xf41237ULL);
    hazard_=nextHazard();
    afterTimes_.fill(std::numeric_limits<double>::infinity());
    wander_=wanderTarget_=wanderLeft_=cluster_=motionPhase_=0;
}
void Engine::reset() noexcept {
    channels_={}; pending_={}; modalPending_={};
    time_=readyAt_=0; lastAccepted_=-1e9;
    gate_=0; measured_=incoming_=pendingMeter_=0; accepted_=missed_=0; maintenance_=0; humPhase_=0;
    outputGain_=gainTarget_; powerGain_=values_[Power]; hissGain_=humGain_=0; distanceGain_=distanceTarget_;
    reseed();
}
void Engine::setGate(bool open,double velocity,double semitones) noexcept {
    gateTarget_=open ? 1.0 : 0.0;
    velocity_=std::isfinite(velocity) ? std::clamp(velocity,0.0,1.0) : 1.0;
    const double p=std::isfinite(semitones) ? std::clamp(semitones,-60.0,60.0) : 0.0;
    if (p!=midiPitch_) { midiPitch_=p; updateCoefficients(); }
}
void Engine::updateCoefficients() noexcept {
    const auto& v=values_;
    const auto m=models[static_cast<int>(v[ClickModel])];
    const double ratio=std::exp2((v[Pitch]+midiPitch_)/12.0);
    const double f1=std::min(sr_*0.4,m.f1*ratio),f2=std::min(sr_*0.43,m.f2*ratio);
    fastA_=std::exp(-1.0/(sr_*v[PulseWidth]*0.001));
    slowA_=std::exp(-1.0/(sr_*v[PulseWidth]*0.0035));
    noiseA_=std::exp(-1.0/(sr_*std::min(v[Decay],12.0)*0.0006));
    r1_=std::exp(-1.0/(sr_*v[Decay]*0.001)); r2_=std::exp(-1.0/(sr_*v[Decay]*0.00053));
    c1_=std::cos(2*pi*f1/sr_); s1_=std::sin(2*pi*f1/sr_);
    c2_=std::cos(2*pi*f2/sr_); s2_=std::sin(2*pi*f2/sr_);
    constexpr double bandwidth[]={18000,8500,10500,6500,4100};
    constexpr double lowcut[]={20,220,850,120,450};
    constexpr double boxFreq[]={800,1350,3100,720,1850};
    const auto sp=static_cast<int>(v[Speaker]);
    const double cutoff=std::clamp(std::min(v[Tone],bandwidth[sp])*std::exp2(-2.0*v[Distance]/100.0),80.0,sr_*0.42);
    lpA_=std::exp(-2*pi*cutoff/sr_); hpA_=std::exp(-2*pi*std::min(lowcut[sp],sr_*0.1)/sr_);
    const double bf=std::min(boxFreq[sp],sr_*0.35);
    boxR_=std::exp(-1.0/(sr_*0.0015)); boxC_=std::cos(2*pi*bf/sr_); boxS_=std::sin(2*pi*bf/sr_);
    smoothA_=std::exp(-1.0/(sr_*0.012)); meterA_=std::exp(-1.0/(sr_*0.75));
    gateAttack_=v[Attack]<=0 ? 0 : std::exp(-1.0/(sr_*v[Attack]*0.001));
    gateRelease_=std::exp(-1.0/(sr_*v[Release]*0.001));
    wanderA_=std::exp(-2*pi*v[WanderSpeed]/sr_); clusterA_=std::exp(-1.0/(sr_*0.065));
    pulseMix_=m.pulse; ringMix_=m.ring; noiseMix_=m.noise;
    gainTarget_=db(v[Output]); drive_=db(v[Drive]);
    dcA_=std::exp(-2*pi*12/sr_);
    hissTarget_=v[Hiss]<=-89.9 ? 0 : db(v[Hiss]); humTarget_=v[Hum]<=-89.9 ? 0 : db(v[Hum]);
    distanceTarget_=std::exp2(-2.5*v[Distance]*0.01);
}
double Engine::nextHazard() noexcept {
    const double random=values_[Randomness]*0.01;
    return std::max(1e-9,(1.0-random)+random*(-std::log(timing_.uniform())));
}
void Engine::excite(double amplitude) noexcept {
    // Equal mono amplitude at zero width; constant-power panning for scattered clicks.
    const double pan=color_.bipolar()*values_[Width]*0.01;
    const double variant=color_.bipolar()*values_[Variation]*0.01;
    amplitude*=1.0+0.4*variant;
    const double l=std::sqrt(1.0-pan),r=std::sqrt(1.0+pan);
    pending_[0]+=amplitude*l; pending_[1]+=amplitude*r;
    modalPending_[0]+=amplitude*l*(1.0+0.35*variant);
    modalPending_[1]+=amplitude*r*(1.0+0.35*variant);
}
bool Engine::detect(double time,bool secondary) noexcept {
    if (time<readyAt_) { ++missed_; return false; }
    const double gap=std::max(0.0,time-readyAt_);
    const double recovery=values_[Recovery]*0.001;
    const double strength=(lastAccepted_<0 || recovery<=0) ? 1.0 : 0.25+0.75*(-std::expm1(-gap/recovery));
    readyAt_=time+values_[DeadTime]*1e-6; lastAccepted_=time;
    ++accepted_; pendingMeter_+=1.0;
    excite(strength*(secondary ? 0.65 : 1.0));
    // Secondary discharge timing is independent of the primary arrival RNG.
    if (!secondary && field_.uniform()<values_[Afterpulse]*0.01) {
        for (auto& at:afterTimes_) if (!std::isfinite(at)) {
            at=readyAt_+0.0004+field_.uniform()*0.0036; break;
        }
    }
    return true;
}
void Engine::testClick() noexcept { if (values_[Power]>0.5) excite(1.0); }
double Engine::render(Channel& ch,double injection,double variation,double n,double hum) noexcept {
    ch.fast+=injection; ch.slow+=injection;
    ch.x1+=variation; ch.x2+=injection; ch.noise=std::min(ch.noise+injection,12.0);
    const double edge=values_[Sharpness]*0.01;
    const double pulse=pulseMix_*(ch.fast-(0.18+0.5*edge)*ch.slow);
    const double ring=ringMix_*(0.72*ch.x1+0.28*ch.x2)*values_[Body]*0.01;
    double x=0.22*((0.3+0.7*edge)*pulse+ring+(noiseMix_+0.8)*values_[Noise]*0.01*ch.noise*n);
    x+=gate_*(hissGain_*n+humGain_*hum);
    ch.fast*=fastA_; ch.slow*=slowA_; ch.noise*=noiseA_;
    const double x1=ch.x1,x2=ch.x2;
    ch.x1=r1_*(c1_*x1-s1_*ch.y1); ch.y1=r1_*(s1_*x1+c1_*ch.y1);
    ch.x2=r2_*(c2_*x2-s2_*ch.y2); ch.y2=r2_*(s2_*x2+c2_*ch.y2);
    const double high=x-ch.dcIn+hpA_*ch.dcOut; ch.dcIn=x; ch.dcOut=high;
    ch.lp1=(1-lpA_)*high+lpA_*ch.lp1; ch.lp2=(1-lpA_)*ch.lp1+lpA_*ch.lp2;
    const double bx=ch.boxX;
    ch.boxX=boxR_*(boxC_*bx-boxS_*ch.boxY)+(1-boxR_)*ch.lp2;
    ch.boxY=boxR_*(boxS_*bx+boxC_*ch.boxY);
    x=ch.lp2+ch.boxX*values_[Resonance]*0.05;
    x=std::tanh(x*drive_)/std::sqrt(drive_);
    // Remove any saturation-induced DC before the final bounded safety stage.
    const double y=x-ch.outIn+dcA_*ch.outDc;
    ch.outIn=x; ch.outDc=y;
    const double comp=1.0/std::sqrt(1.0+incoming_*0.005*values_[Compensation]*0.01);
    return 0.97*std::tanh(y/0.97)*outputGain_*powerGain_*comp*distanceGain_;
}
Frame Engine::tick() noexcept {
    const auto& v=values_;
    const double target=v[Power]>0.5 ? gateTarget_*velocity_ : 0.0;
    const double ga=target>gate_ ? gateAttack_ : gateRelease_;
    gate_=target+(gate_-target)*ga;
    if (gate_<1e-8) gate_=0;
    outputGain_=gainTarget_+(outputGain_-gainTarget_)*smoothA_;
    powerGain_=v[Power]+(powerGain_-v[Power])*smoothA_;
    hissGain_=hissTarget_+(hissGain_-hissTarget_)*smoothA_; humGain_=humTarget_+(humGain_-humTarget_)*smoothA_;
    distanceGain_=distanceTarget_+(distanceGain_-distanceTarget_)*smoothA_;
    if (--wanderLeft_<=0) {
        wanderLeft_=sr_/v[WanderSpeed]; wanderTarget_=field_.bipolar();
    }
    wander_=wanderTarget_+(wander_-wanderTarget_)*wanderA_;
    cluster_*=clusterA_;
    if ((maintenance_&63u)==0 && field_.uniform()<(-std::expm1(-32.0/sr_)))
        cluster_=20*v[Clusters]*0.01;
    motionPhase_+=2*pi*v[MotionRate]/sr_; if (motionPhase_>=2*pi) motionPhase_-=2*pi;
    const double modulation=(v[Wander]==0 && v[MotionDepth]==0) ? 1.0 : std::exp2(4*(v[Wander]*0.01)*wander_+3*(v[MotionDepth]*0.01)*std::sin(motionPhase_));
    incoming_=gate_*(v[Background]+v[Rate]*modulation*(1+cluster_));
    // Cap the artistic field before it can demand unbounded work at extreme combinations.
    incoming_=std::min(incoming_,120000.0);
    double consumed=0;
    const double perSample=incoming_/sr_;
    // Merge primary arrivals and delayed secondary discharges in chronological order.
    // Integrated hazard keeps automation immediate without redrawing the pending wait.
    for (int iterations=0; iterations<64; ++iterations) {
        const double primary=(perSample>0 && hazard_<=perSample-consumed)
            ? time_+(consumed+hazard_)/incoming_ : std::numeric_limits<double>::infinity();
        std::size_t secondary=afterTimes_.size();
        double after=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<afterTimes_.size();++i)
            if(afterTimes_[i]<=time_+1.0/sr_ && afterTimes_[i]<after) {after=afterTimes_[i]; secondary=i;}
        if(!std::isfinite(primary) && !std::isfinite(after)) break;
        if(primary<=after) {
            consumed+=hazard_; detect(primary,false); hazard_=nextHazard();
        } else {
            afterTimes_[secondary]=std::numeric_limits<double>::infinity();
            if(v[Power]>0.5) detect(std::max(after,time_),true);
        }
    }
    hazard_-=std::max(0.0,perSample-consumed);
    if(hazard_<=0) hazard_=nextHazard();
    measured_=meterA_*measured_+(1-meterA_)*pendingMeter_*sr_; pendingMeter_=0;
    const double n=noise_.bipolar();
    humPhase_+=2*pi*(v[HumFrequency]<0.5 ? 50.0 : 60.0)/sr_; if (humPhase_>=2*pi) humPhase_-=2*pi;
    const double hum=humGain_<1e-12 ? 0.0 : std::sin(humPhase_)+0.15*std::sin(2*humPhase_);
    Frame result;
    result.left=static_cast<float>(render(channels_[0],pending_[0],modalPending_[0],n,hum));
    result.right=static_cast<float>(render(channels_[1],pending_[1],modalPending_[1],n,hum));
    pending_={}; modalPending_={}; time_+=1/sr_;
    if ((++maintenance_&255u)==0) {
        for (auto& c:channels_) {
            c.fast=clean(c.fast); c.slow=clean(c.slow); c.x1=clean(c.x1); c.y1=clean(c.y1);
            c.x2=clean(c.x2); c.y2=clean(c.y2); c.noise=clean(c.noise); c.dcIn=clean(c.dcIn);
            c.dcOut=clean(c.dcOut); c.lp1=clean(c.lp1); c.lp2=clean(c.lp2); c.boxX=clean(c.boxX);
            c.boxY=clean(c.boxY); c.outIn=clean(c.outIn); c.outDc=clean(c.outDc);
        }
    }
    return result;
}
}
