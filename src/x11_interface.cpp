#include "x11_interface.h"

#include "input.h"
#include "simulator.h"
#include "video_decoder.h"

#include <fmt/printf.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <chrono>
#include <poll.h>

namespace imp_sim
{

void X11Interface::start(Simulator& simulator)
{
    std::unique_lock lock(m_stateMutex);
    while (m_state == State::Stopping)
    {
        m_stateCondition.wait(lock);
    }

    if (m_state == State::Started)
    {
        return;
    }

    if (m_state == State::Stopped)
    {
        m_state = State::Started;
        m_simulator = &simulator;
        m_thread = std::thread([this]()
        {
            interfaceThread();
        });
    }
}

void X11Interface::stop()
{
    std::unique_lock lock(m_stateMutex);
    while (m_state != State::Stopped)
    {    
        if (m_state == State::Started)
        {
            m_state = State::Stopping;
            m_stateCondition.notify_all();
        }

        while (m_state == State::Stopping)
        {
            m_stateCondition.wait(lock);
            m_thread.join();
        }
    }
    m_simulator = nullptr;
}

void X11Interface::interfaceThread()
{
    auto display = XOpenDisplay(nullptr);
    auto screen = DefaultScreen(display);
    auto blackPixel = BlackPixel(display, screen);
    auto whitePixel = WhitePixel(display, screen);
    auto window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0,
        0,
        1024,
        768,
        0,
        blackPixel,
        whitePixel
    );
    XSelectInput(display, window, StructureNotifyMask | KeyPressMask | KeyReleaseMask);
    XMapWindow(display, window);

    auto gc = XCreateGC(display, window, 0, nullptr);

    XSetForeground(display, gc, blackPixel);

    for (;;)
    {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == MapNotify)
        {
            break;
        }
    }

    std::string_view title = "TT DOOM Simulator";
    XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(title.data()), title.size());

    std::vector<std::uint32_t> bitmap;
    bitmap.resize(1024 * 768);

    auto image = XCreateImage(
        display,
        DefaultVisual(display, screen),
        DefaultDepth(display, screen),
        ZPixmap,
        0,
        reinterpret_cast<char*>(bitmap.data()),
        1024,
        768,
        32,
        0
    );

    auto fd = ConnectionNumber(display);
    
    auto nextPrint = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(17);

    std::unique_lock lock(m_stateMutex);
    while (m_state == State::Started)
    {
        lock.unlock();

        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == KeyPress || event.type == KeyRelease)
            {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif // __GNUC__
                KeySym symbol = XKeycodeToKeysym(display, event.xkey.keycode, 0);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC__

                auto mask = keySymToHardware(symbol);
                if (mask)
                {
                    if (event.type == KeyPress)
                    {
                        m_controllerState.store(m_controllerState.load(std::memory_order_relaxed) | mask, std::memory_order_release);
                    }
                    else
                    {
                        m_controllerState.store(m_controllerState.load(std::memory_order_relaxed) & ~mask, std::memory_order_release);
                    }
                }
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        if (now >= nextPrint)
        {
            render(bitmap);
            XPutImage(display, window, gc, image, 0, 0, 0, 0, 1024, 768);
            XFlush(display);

            nextPrint = now + std::chrono::milliseconds(17);
        }

        pollfd pollInfo;
        pollInfo.fd = fd;
        pollInfo.events = POLLIN;

        poll(&pollInfo, 1, std::chrono::duration_cast<std::chrono::milliseconds>(nextPrint - now).count());

        lock.lock();
    }

    m_state = State::Stopped;
    m_stateCondition.notify_all();
}

void X11Interface::render(std::span<std::uint32_t> bitmap)
{
    if (!m_videoEnabled.load(std::memory_order_acquire))
    {
        return;
    }

    auto address = m_videoAddress.load(std::memory_order_acquire);

    constexpr auto width = 128;
    constexpr auto height = 96;
    constexpr auto scale = 1024 / width;

    for (unsigned int y = 0; y != height; ++y)
    {
        for (unsigned int x = 0; x != width; x += 8)
        {
            auto chunk = m_simulator->load32(address);
            address += 4;

            auto pixels = VideoDecoder::decode(chunk);

            for (std::size_t p = 0; p != pixels.size(); ++p)
            {
                auto pixel = pixels[p];
                uint32_t color8888 =
                    (static_cast<std::uint32_t>(pixel & 0x30) << 18) |
                    (static_cast<std::uint32_t>(pixel & 0x0C) << 12) |
                    (static_cast<std::uint32_t>(pixel & 0x03) << 6);

                for (unsigned int cy = 0; cy != scale; ++cy)
                {
                    for (unsigned int cx = 0; cx != scale; ++cx)
                    {
                        bitmap[(y * scale + cy) * 1024 + (x + p) * scale + cx] = color8888;
                    }
                }
            }
        }
    }
}


void X11Interface::setVideoConfig(bool enabled, std::uint32_t address)
{
    if (!enabled)
    {
        m_videoEnabled.store(false, std::memory_order_release);
    }
    m_videoAddress.store(address, std::memory_order_release);
    if (enabled)
    {
        m_videoEnabled.store(true, std::memory_order_release);
    }
}

std::uint32_t X11Interface::keySymToHardware(KeySym symbol) noexcept
{
    switch (symbol)
    {
        case XK_Right:
            return INPUT_SNES1_RIGHT;

        case XK_Left:
            return INPUT_SNES1_LEFT;

        case XK_Up:
        case XK_W:
            return INPUT_SNES1_UP;

        case XK_Down:
        case XK_S:
            return INPUT_SNES1_DOWN;
        
        case XK_A:
            return INPUT_SNES1_L;

        case XK_D:
            return INPUT_SNES1_R;

        case XK_Return:
            return INPUT_SNES1_X;

        case XK_Escape:
            return INPUT_SNES1_Y;

        case XK_space:
            return INPUT_SNES1_B;

        case XK_Control_L:
        case XK_Control_R:
            return INPUT_SNES1_A;
    }

    return 0;    
}

std::uint32_t X11Interface::input()
{
    return m_controllerState.load(std::memory_order_acquire);
}

} // namespace imp_sim
