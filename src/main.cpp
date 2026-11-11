#include "executable.h"
#include "audio_interface.h"
#include "interface.h"
#include "none_audio_interface.h"
#include "none_interface.h"
#include "simulator.h"
#include "tty_interface.h"

#ifdef HAVE_ALSA
#include "alsa_audio_interface.h"
#endif // HAVE_ALSA

#ifdef HAVE_X11
#include "x11_interface.h"
#endif // HAVE_X11

#include <cstdio>
#include <cstdlib>
#include <print>
#include <memory>
#include <string_view>

int main(int argc, char** argv)
{
    using namespace imp_sim;

    std::string_view interfaceName;
#ifdef HAVE_X11
    interfaceName = "x11";
#else // HAVE_X11
    interfaceName = "none";
#endif // HAVE_X11

    std::string_view audioInterfaceName;
#ifdef HAVE_ALSA
    audioInterfaceName = "alsa";
#else // HAVE_ALSA
    audioInterfaceName = "none";
#endif // HAVE_ALSA

    std::string_view executableFile;
    bool realTime = true;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg.starts_with("--interface="))
        {
            interfaceName = arg.substr(12);
        }
        else if (arg.starts_with("--audio-interface="))
        {
            audioInterfaceName = arg.substr(18);
        }
        else if (arg == "--fast")
        {
            realTime = false;
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::string interfaceTypes = "none|tty";
#ifdef HAVE_X11
            interfaceTypes += "|x11";
#endif // HAVE_X11
            
            std::string audioInterfaceTypes = "none";
#ifdef HAVE_ALSA
            audioInterfaceTypes += "|alsa";
#endif // HAVE_ALSA

            std::println("Usage: imp-sim [--interface={}] [--audio-interface={}] [--fast] EXECUTABLE", interfaceTypes, audioInterfaceTypes);
        }
        else if (arg.starts_with("-"))
        {
            std::println(stderr, "Invalid argument: {}", arg);
            return EXIT_FAILURE;
        }
        else
        {
            executableFile = arg;
        }
    }

    if (executableFile.empty())
    {
        std::println(stderr, "Missing argument");
        return EXIT_FAILURE;
    }

    try
    {
        std::unique_ptr<Interface> interface;
        if (interfaceName == "none")
        {
            interface = std::make_unique<NoneInterface>();
        }
        else if (interfaceName == "tty")
        {
            interface = std::make_unique<TtyInterface>();
        }
#ifdef HAVE_X11
        else if (interfaceName == "x11")
        {
            interface = std::make_unique<X11Interface>();
        }
#endif // HAVE_X11
        else
        {
            throw std::invalid_argument(std::format("Unsupported interface: {}", interfaceName));
        }

        std::unique_ptr<AudioInterface> audioInterface;
        if (audioInterfaceName == "none")
        {
            audioInterface = std::make_unique<NoneAudioInterface>();
        }
#ifdef HAVE_ALSA
        else if (audioInterfaceName == "alsa")
        {
            audioInterface = std::make_unique<AlsaAudioInterface>();
        }
#endif // HAVE_ALSA
        else
        {
            throw std::invalid_argument(std::format("Unsupported audio interface: {}", audioInterfaceName));
        }


        Executable executable(executableFile);

        Simulator simulator(executable, *interface, *audioInterface);
        simulator.run(realTime);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
