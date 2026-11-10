#pragma once

#include "interface.h"

#include <X11/Xlib.h>

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <span>

namespace imp_sim
{

class Simulator;

class X11Interface : public Interface
{
public:
    X11Interface() = default;

    void start(Simulator& simulator) override;
    void stop() override;
    void setVideoConfig(bool enabled, std::uint32_t address) override;
    std::uint32_t input() override;

private:
    void interfaceThread();
    void render(std::span<std::uint32_t> bitmap);
    static std::uint32_t keySymToHardware(KeySym symbol) noexcept;

    enum class State
    {
        Stopped,
        Started,
        Stopping
    };
    Simulator* m_simulator = nullptr;
    std::thread m_thread;
    std::atomic_bool m_videoEnabled = 0;
    std::atomic_uint32_t m_videoAddress = 0;
    std::atomic_uint32_t m_controllerState = 0;
    std::condition_variable m_stateCondition;
    std::mutex m_stateMutex;
    State m_state = State::Stopped;
};

} // namespace imp_sim
