#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace geiger {
enum Param : std::size_t {
    RunMode, Rate, Background, Randomness, DeadTime, Recovery, Afterpulse,
    Clusters, Wander, WanderSpeed, PulseWidth, ClickModel, Pitch, Decay,
    Sharpness, Noise, Body, Variation, Speaker, Resonance, Tone, Drive,
    Hiss, Hum, HumFrequency, Width, Distance, Output, Compensation, Seed,
    MidiPitch, Attack, Release, MotionDepth, MotionRate, Retrigger, Power, Count
};
struct Definition {
    const char* key;
    const char* name;
    const char* unit;
    double minimum, maximum, initial, midpoint;
    int page;
    const char* choices;
    const char* help;
};
// Native units and stable string IDs are a saved-project contract. Append; never reorder IDs.
inline constexpr std::array<Definition, Count> definitions {{
    {"geiger.run-mode", "Run mode", "", 0, 2, 0, 0, 0, "Free running|Transport|MIDI gate", "Free runs immediately. Transport follows Play. MIDI gate follows held notes and sustain."},
    {"geiger.rate", "Intensity", "cps", 0, 12000, 12, 80, -1, "", "Incoming synthetic events per second, before tube losses. More radiation means more events, not higher-energy clicks."},
    {"geiger.background", "Background", "cps", 0, 30, 0.25, 2, 0, "", "An additional steady event rate. Set both Background and Intensity to zero for no random clicks."},
    {"geiger.randomness", "Random timing", "%", 0, 100, 100, 50, 0, "", "100%: exponential waiting times (Poisson arrivals). 0%: evenly spaced, deliberately musical events."},
    {"geiger.dead-time", "Tube dead time", "us", 0, 2000, 190, 250, 0, "", "After a count, the tube ignores incoming events for this long. Non-extending dead-time model."},
    {"geiger.recovery", "Recovery", "ms", 0, 10, 0.35, 1, 0, "", "Partial pulse-amplitude recovery after dead time. Short gaps produce weaker electrical pulses."},
    {"geiger.afterpulse", "Afterpulses", "%", 0, 60, 0, 20, 0, "", "Creative imperfect-quenching model: chance of one delayed secondary discharge. No recursive cascades."},
    {"geiger.clusters", "Clusters", "%", 0, 100, 0, 35, 0, "", "Creative intermittent hot spots. Adds short dense clouds above the steady field; not ordinary radioactive decay."},
    {"geiger.wander", "Field wander", "%", 0, 100, 0, 30, 0, "", "Slow random changes to the source intensity, as though moving a probe through an uneven field."},
    {"geiger.wander-speed", "Wander speed", "Hz", 0.03, 12, 0.4, 1, 0, "", "Speed of smooth random field changes. Does not change the independent click-timing random stream."},
    {"geiger.pulse-width", "Pulse width", "ms", 0.03, 6, 0.22, 0.6, 1, "", "Electrical pulse shaping before the transducer. Very short pulses make crisp ticks; longer ones make knocks."},
    {"geiger.click-model", "Click model", "", 0, 7, 0, 0, -1, "Classic tube|Pocket piezo|Vintage counter|Relay snap|Soft tick|Glass particle|Ceramic tap|Wire ping", "Eight synthesized pulse/transducer recipes, not recordings or claims of exact commercial-device emulation."},
    {"geiger.pitch", "Click pitch", "st", -24, 24, 0, 0, 1, "", "Transposes the acoustic ringing, not particle energy. Zero is the model's natural pitch."},
    {"geiger.decay", "Ring decay", "ms", 0.5, 120, 7, 12, 1, "", "Time constant of the transducer's damped ringing. Long settings become percussive or glassy."},
    {"geiger.sharpness", "Sharpness", "%", 0, 100, 75, 50, 1, "", "Balances the electrical edge against the softer pulse body."},
    {"geiger.noise", "Crackle", "%", 0, 100, 8, 30, 1, "", "Short noise excited by each detection. Unlike Hiss, this is silent between pulse tails."},
    {"geiger.body", "Click body", "%", 0, 100, 50, 50, 1, "", "Amount of the two damped acoustic modes excited by every electrical pulse."},
    {"geiger.variation", "Click variation", "%", 0, 100, 3, 30, 1, "", "Per-click amplitude and modal-balance variation. Keep low for a uniform GM-counter pulse train."},
    {"geiger.speaker", "Transducer", "", 0, 4, 1, 0, 2, "Direct circuit|Small speaker|Piezo disc|Metal enclosure|Field radio", "Colors the pulse with different bandwidths and enclosure resonance. Direct circuit is the least filtered."},
    {"geiger.resonance", "Enclosure", "%", 0, 100, 35, 50, 2, "", "Additional resonant body in the speaker or enclosure model."},
    {"geiger.tone", "High cut", "Hz", 250, 18000, 9000, 3500, 2, "", "Upper bandwidth, also limited by the chosen transducer and Distance. Sample-rate-aware filtering."},
    {"geiger.drive", "Circuit drive", "dB", 0, 24, 0, 8, 2, "", "Smoothed, antialiased soft saturation after density balancing. Adds weight and bite without abrupt drive changes; output protection remains active."},
    {"geiger.hiss", "Circuit hiss", "dB", -90, -24, -90, -55, 2, "", "Continuous amplifier noise while the field is running. -90 dB is fully off."},
    {"geiger.hum", "Supply hum", "dB", -90, -24, -90, -55, 2, "", "Optional power-supply coloration while running. -90 dB is fully off; not an inherent radiation sound."},
    {"geiger.hum-frequency", "Mains frequency", "", 0, 1, 0, 0, 2, "50 Hz|60 Hz", "Frequency of the optional supply-hum layer."},
    {"geiger.width", "Stereo scatter", "%", 0, 100, 0, 40, 2, "", "Pans individual clicks around the stereo field. Zero keeps the physical single-speaker mono image."},
    {"geiger.distance", "Listening distance", "%", 0, 100, 0, 40, 2, "", "An artistic listening-distance macro: attenuates and darkens the sound. It does not alter the detector count rate."},
    {"geiger.output", "Output", "dB", -60, 0, -12, -18, -1, "", "Master level with smoothing. A bounded output stage protects against dense pulse pile-up; start listening quietly."},
    {"geiger.compensation", "Density balance", "%", 0, 100, 45, 40, 2, "", "Smooth pre-drive headroom based on detected density. Keeps dense fields open instead of flattening their clicks. Zero retains natural energy growth."},
    {"geiger.seed", "Random seed", "", 1, 999999, 137, 500000, 3, "", "Type an integer for reproducible timing. Reset/retrigger repeats a seed; timbre/noise use separate streams."},
    {"geiger.midi-pitch", "MIDI pitch", "", 0, 1, 0, 0, 3, "Off|On", "In MIDI-gate mode, transpose acoustic ringing from the most recently held note; C4 / MIDI 60 is neutral."},
    {"geiger.attack", "Field attack", "ms", 0, 500, 5, 40, 3, "", "How quickly the event density rises after enabling the field or pressing a note. This is not a click attack envelope."},
    {"geiger.release", "Field release", "ms", 1, 2000, 80, 200, 3, "", "How quickly the event density falls after a note or transport gate closes. Existing transducer ringing can finish."},
    {"geiger.motion-depth", "Breathing", "%", 0, 100, 0, 40, 0, "", "Creative sinusoidal movement of the source intensity. Zero is a stationary field."},
    {"geiger.motion-rate", "Breathing speed", "Hz", 0.03, 20, 0.25, 1, 0, "", "Free-running frequency of the breathing field. Not tempo synchronization."},
    {"geiger.retrigger", "Repeat on Play", "", 0, 1, 1, 0, 3, "Off|On", "In Transport mode, restart the seeded field on each stopped-to-playing transition. Host Reset also restarts it."},
    {"geiger.power", "Power", "", 0, 1, 1, 0, -1, "Off|On", "Smoothly powers the generator down, including noise layers. Test click obeys Power; it bypasses the run-mode gate."}
}};
using Values = std::array<double, Count>;
inline Values defaults() noexcept {
    Values v{};
    for (std::size_t i=0; i<Count; ++i) v[i]=definitions[i].initial;
    return v;
}
inline Values sanitize(Values v) noexcept {
    for (std::size_t i=0; i<Count; ++i) {
        const auto& d=definitions[i];
        v[i]=std::isfinite(v[i]) ? std::clamp(v[i],d.minimum,d.maximum) : d.initial;
        if (d.choices[0] || i==Seed) v[i]=std::round(v[i]);
    }
    return v;
}
constexpr std::uint32_t parameterId(std::size_t i) noexcept {
    std::uint32_t h=2166136261u;
    for (const char* p=definitions[i].key; *p; ++p) h=(h^static_cast<std::uint8_t>(*p))*16777619u;
    return h;
}
// Append presets only: the stored index is part of the editor-state contract.
inline constexpr std::array<const char*,20> presetNames{{
    "Bench / classic", "Pocket survey", "Vintage civil defense", "Hot zone", "Glass particles",
    "Relay rain", "Drifting field", "Rhythmic dosimeter", "Soft dust",
    "Velvet reactor", "Ion storm", "Ceramic drizzle", "Wire constellation", "Deep containment",
    "Distant fallout", "Faulty quench", "Pulse engine", "Glass tide", "Night survey", "Critical mass"
}};
inline constexpr std::array<const char*,20> presetDescriptions{{
    "A clean, steady reference counter with natural random spacing.",
    "Short, bright piezo ticks at an unhurried survey pace.",
    "A narrow radio speaker, softened noise and a little amplifier drive.",
    "A dense classic counter with balanced pre-drive headroom.",
    "Long, clear resonances scattered across the stereo field.",
    "Low mechanical snaps in a resonant metal enclosure.",
    "A wandering probe with occasional hot spots and secondary clicks.",
    "Deliberately regular arrivals with a gently breathing intensity.",
    "Small, dark grains with a soft stereo image.",
    "A smooth, full high-density field with soft-tick articulation.",
    "A bright, active storm with fast-moving pockets of density.",
    "Hollow ceramic taps with a short, tactile after-ring.",
    "Sparse metallic pings with wide, shimmering resonance.",
    "Low, resonant enclosure knocks from a slowly shifting field.",
    "A dark, distant rain of clicks with a subdued radio bandwidth.",
    "Imperfect quenching: clear ticks followed by clustered secondary discharges.",
    "An even pulse train that opens and closes with a rhythmic breath.",
    "A slow swell of ringing glass grains; density balance preserves space.",
    "A quiet handheld survey with a faint amplifier-noise floor.",
    "Maximum incoming activity, restrained bandwidth and controlled saturation."
}};
inline bool presetProtected(std::size_t i) noexcept {
    return i==Output || i==Power || i==RunMode || i==Seed;
}
inline Values preset(std::size_t n) noexcept {
    auto v=defaults();
    switch (n) {
        case 1: v[ClickModel]=1; v[Speaker]=2; v[Rate]=5; v[Decay]=3; v[Body]=65; v[PulseWidth]=0.09; break;
        case 2: v[ClickModel]=2; v[Speaker]=4; v[Rate]=18; v[Drive]=5; v[Tone]=4800; v[Noise]=20; v[Decay]=11; break;
        case 3: v[Rate]=2200; v[ClickModel]=0; v[DeadTime]=160; v[Recovery]=0.2; v[Compensation]=65; v[Noise]=5; break;
        case 4: v[Rate]=9; v[ClickModel]=5; v[Speaker]=0; v[Decay]=65; v[Body]=92; v[Width]=75; v[Variation]=30; break;
        case 5: v[Rate]=26; v[ClickModel]=3; v[Speaker]=3; v[PulseWidth]=1.1; v[Pitch]=-7; v[Decay]=20; v[Width]=35; break;
        case 6: v[Rate]=24; v[Wander]=70; v[WanderSpeed]=0.3; v[Clusters]=35; v[Afterpulse]=12; v[Width]=40; break;
        case 7: v[Rate]=8; v[Background]=0; v[Randomness]=0; v[ClickModel]=1; v[MotionDepth]=45; v[MotionRate]=0.5; break;
        case 8: v[Rate]=55; v[ClickModel]=4; v[Sharpness]=20; v[Tone]=3200; v[Decay]=4; v[Width]=65; v[Noise]=20; break;
        case 9: v[Rate]=3600; v[ClickModel]=4; v[DeadTime]=90; v[Recovery]=0.12; v[Sharpness]=32; v[Tone]=4700; v[Noise]=15; v[Body]=42; v[Compensation]=85; v[Width]=45; break;
        case 10: v[Rate]=1600; v[ClickModel]=1; v[DeadTime]=110; v[Recovery]=0.15; v[Decay]=2.5; v[Clusters]=50; v[Wander]=25; v[WanderSpeed]=2; v[Width]=75; v[Compensation]=75; break;
        case 11: v[Rate]=14; v[ClickModel]=6; v[Speaker]=0; v[Decay]=24; v[Sharpness]=48; v[Body]=82; v[Variation]=26; v[Width]=45; v[Noise]=4; break;
        case 12: v[Rate]=4; v[ClickModel]=7; v[Speaker]=0; v[Pitch]=-5; v[Decay]=100; v[Body]=95; v[Sharpness]=30; v[Width]=95; v[Variation]=40; v[Noise]=1; break;
        case 13: v[Rate]=32; v[ClickModel]=3; v[Speaker]=3; v[Pitch]=-19; v[PulseWidth]=1.8; v[Decay]=38; v[Resonance]=85; v[Tone]=2600; v[Wander]=45; v[Noise]=12; break;
        case 14: v[Rate]=420; v[ClickModel]=2; v[Speaker]=4; v[Distance]=64; v[Tone]=3600; v[Width]=55; v[Compensation]=60; v[Noise]=12; break;
        case 15: v[Rate]=16; v[ClickModel]=0; v[Afterpulse]=58; v[Recovery]=0.08; v[DeadTime]=110; v[PulseWidth]=0.11; v[Decay]=3; v[Width]=20; break;
        case 16: v[Rate]=28; v[Background]=0; v[Randomness]=0; v[ClickModel]=6; v[DeadTime]=0; v[Recovery]=0; v[Pitch]=-12; v[MotionDepth]=60; v[MotionRate]=2; v[Width]=25; break;
        case 17: v[Rate]=130; v[ClickModel]=5; v[Speaker]=0; v[Decay]=88; v[Body]=90; v[Width]=90; v[MotionDepth]=68; v[MotionRate]=0.13; v[Compensation]=85; v[Noise]=2; v[Tone]=7500; break;
        case 18: v[Rate]=3; v[ClickModel]=1; v[Speaker]=1; v[Decay]=2; v[Hiss]=-66; v[Tone]=6200; v[Distance]=15; v[Wander]=20; break;
        case 19: v[Rate]=12000; v[ClickModel]=0; v[DeadTime]=60; v[Recovery]=0.07; v[Decay]=2.8; v[PulseWidth]=0.10; v[Tone]=5800; v[Noise]=5; v[Compensation]=100; v[Drive]=4; v[Width]=55; break;
        default: break;
    }
    return v;
}
inline bool matchesPreset(const Values& values,std::size_t index) noexcept {
    if(index>=presetNames.size()) return false;
    const auto candidate=preset(index);
    for(std::size_t i=0;i<Count;++i)
        if(!presetProtected(i) && std::abs(values[i]-candidate[i])>1e-7*std::max(1.0,definitions[i].maximum-definitions[i].minimum)) return false;
    return true;
}
inline int matchingPreset(const Values& values) noexcept {
    for(std::size_t i=0;i<presetNames.size();++i) if(matchesPreset(values,i)) return static_cast<int>(i);
    return -1;
}
}
