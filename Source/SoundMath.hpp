#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace geiger {
struct ClickProfile { double f1, f2, pulse, ring, noise; };
inline constexpr std::array<ClickProfile,8> clickProfiles{{
    {2100,4300,0.8,0.42,0.12}, {3600,6700,0.35,0.9,0.05},
    {950,2300,1.0,0.50,0.2}, {650,3100,1.1,0.75,0.28},
    {1250,2800,0.4,0.28,0.18}, {2800,7210,0.18,1.05,0.03},
    {1450,3920,0.55,0.72,0.07}, {4700,8350,0.12,1.12,0.025}
}};
struct SpeakerProfile { double bandwidth, lowcut, frequency, decayMs; };
inline constexpr std::array<SpeakerProfile,5> speakerProfiles{{
    {18000,20,800,1.5}, {8500,220,1350,2.2}, {10500,850,3100,1.0},
    {6500,120,720,5.5}, {4100,450,1850,2.8}
}};
// First-order antiderivative antialiasing for f(x)=x/sqrt(1+x*x).
// Rationalizing the divided difference of F(x)=sqrt(1+x*x) avoids
// cancellation and the near-equal-input branch, including at silence.
// This reduces nonlinear aliasing; it does not make the signal bandlimited.
inline double softClip(double x) noexcept { return x/std::sqrt(1+x*x); }
inline double antialiasedClip(double x,double previous) noexcept {
    return (x+previous)/(std::sqrt(1+x*x)+std::sqrt(1+previous*previous));
}
inline double densityGain(double acceptedRate,double balance) noexcept {
    return 1/std::sqrt(1+std::max(0.0,acceptedRate)*0.005*balance*0.01);
}
}
