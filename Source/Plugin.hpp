#pragma once
#include "Engine.hpp"
#include <nullclap/NullClap.hpp>
#include <array>
#include <atomic>
#include <memory>
#include <string>

class GeigerPlugin final : public nullclap::Plugin {
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept;
    explicit GeigerPlugin(const clap_host_t* host);
    ~GeigerPlugin() override;
    void testClick() noexcept;
    void panic() noexcept;
    void applyPreset(std::size_t index) noexcept;
    int presetIndex() const noexcept;
    bool presetIsModified() const noexcept;
    std::string presetName() const;
    geiger::Values currentValues() const noexcept;
    bool guiTimerAvailable() const noexcept;
    bool startGuiTimer() noexcept;
    void stopGuiTimer() noexcept;
    struct Telemetry {
        std::atomic<float> cps{0}, incoming{0}, peak{0}, balance{1}, leftPeak{0}, rightPeak{0};
        std::atomic<std::uint64_t> detected{0}, missed{0};
        std::atomic<bool> running{false};
        std::array<std::atomic<float>,128> scope{}, scopeMin{}, scopeMax{};
        std::atomic<std::uint32_t> scopeHead{0};
    };
    const Telemetry& telemetry() const noexcept {return telemetry_;}
private:
    std::vector<std::byte> saveExtraState() const override;
    bool loadExtraState(std::span<const std::byte>) override;
    bool onInit() noexcept override;
    bool onActivate(double,std::uint32_t,std::uint32_t) noexcept override;
    void onReset() noexcept override;
    void onEvent(const clap_event_header_t&) noexcept override;
    void processAudio(const clap_process_t&,std::uint32_t,std::uint32_t) noexcept override;
    bool implementsTimerSupport() const noexcept override {return true;}
    void onTimer(clap_id) noexcept override;
    void updateTransport(const clap_event_transport_t&) noexcept;
    void initialiseBlockTransport() noexcept;
    clap_process_status processFinished() noexcept override {blockTransportReady_=false; return CLAP_PROCESS_CONTINUE;}
    void midi(const clap_event_midi_t&) noexcept;
    geiger::Values readValues() const noexcept;
    struct Note {std::uint8_t held=0; bool sustained=false; double velocity=0; std::uint64_t order=0;};
    const clap_host_t* host_=nullptr;
    const clap_host_timer_support_t* timerHost_=nullptr;
    clap_id timerId_=CLAP_INVALID_ID;
    geiger::Engine engine_;
    Telemetry telemetry_;
    std::array<Note,16*128> notes_{};
    std::array<bool,16> sustain_{};
    std::uint64_t noteOrder_=0;
    std::atomic<std::uint32_t> requestedClicks_{0};
    std::atomic<bool> requestedPanic_{false};
    bool playing_=false, restartTransport_=false, pumping_=false, blockTransportReady_=false;
    std::atomic<int> presetIndex_{0};
    float scopePeak_=0, meterPeak_=0, scopeMin_=0, scopeMax_=0, leftPeak_=0, rightPeak_=0;
    std::uint32_t scopeStride_=64;
    float meterDecay_=0.9997f;
    std::uint32_t scopeSamples_=0, scopeHead_=0;
};
