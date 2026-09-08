#pragma once
#include "Parameters.hpp"
#include <array>
#include <cstdint>

namespace geiger {
struct Random {
    std::uint64_t state=1;
    void seed(std::uint64_t value) noexcept { state=value+0x9e3779b97f4a7c15ULL; next(); }
    std::uint32_t next() noexcept {
        // SplitMix64; no platform-dependent standard-library distribution state.
        std::uint64_t z=(state+=0x9e3779b97f4a7c15ULL);
        z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
        z=(z^(z>>27))*0x94d049bb133111ebULL;
        return static_cast<std::uint32_t>((z^(z>>31))>>32);
    }
    double uniform() noexcept { return (static_cast<double>(next())+0.5)/4294967296.0; }
    double bipolar() noexcept { return 2.0*uniform()-1.0; }
};
struct Frame { float left=0, right=0; };
class Engine {
public:
    Engine() noexcept;
    bool prepare(double sampleRate) noexcept;
    void configure(Values values) noexcept;
    void reset() noexcept;
    void setGate(bool open, double velocity=1.0, double semitones=0.0) noexcept;
    void testClick() noexcept;
    Frame tick() noexcept;
    std::uint64_t detected() const noexcept { return accepted_; }
    std::uint64_t missed() const noexcept { return missed_; }
    double measuredCps() const noexcept { return measured_; }
    double incomingCps() const noexcept { return incoming_; }
    double lastDetectionTime() const noexcept { return lastAccepted_; }
    const Values& values() const noexcept { return values_; }
private:
    struct Channel {
        double fast=0, slow=0, x1=0, y1=0, x2=0, y2=0, noise=0;
        double dcIn=0, dcOut=0, lp1=0, lp2=0, boxX=0, boxY=0;
        double outIn=0, outDc=0;
    };
    void updateCoefficients() noexcept;
    void reseed() noexcept;
    double nextHazard() noexcept;
    bool detect(double time, bool secondary) noexcept;
    void excite(double amplitude) noexcept;
    double render(Channel& ch, double injection, double variation, double noise, double hum) noexcept;
    Values values_=defaults();
    Random timing_, color_, field_, noise_;
    std::array<Channel,2> channels_{};
    std::array<double,16> afterTimes_{};
    std::array<double,2> pending_{}, modalPending_{};
    double sr_=48000, time_=0, hazard_=1, readyAt_=0, lastAccepted_=-1e9;
    double gate_=0, gateTarget_=1, velocity_=1, midiPitch_=0;
    double measured_=0, incoming_=0, wander_=0, wanderTarget_=0, wanderLeft_=0, cluster_=0;
    double motionPhase_=0, humPhase_=0, outputGain_=0, powerGain_=1, hissGain_=0, humGain_=0;
    double fastA_=0, slowA_=0, noiseA_=0, r1_=0, r2_=0, c1_=0, s1_=0, c2_=0, s2_=0;
    double lpA_=0, hpA_=0, boxR_=0, boxC_=0, boxS_=0, smoothA_=0, meterA_=0;
    double gateAttack_=0, gateRelease_=0, wanderA_=0, clusterA_=0;
    double pulseMix_=1, ringMix_=1, noiseMix_=0.1, gainTarget_=0.25, drive_=1;
    double pendingMeter_=0, dcA_=0, hissTarget_=0, humTarget_=0, distanceTarget_=1, distanceGain_=1;
    std::uint64_t accepted_=0, missed_=0;
    std::uint32_t maintenance_=0;
};
}
