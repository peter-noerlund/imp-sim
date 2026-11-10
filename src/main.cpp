#include "executable.h"
#include "interface.h"
#include "none_interface.h"
#include "simulator.h"
#include "tty_interface.h"
#include "x11_interface.h"

#include <cstdio>
#include <cstdlib>
#include <print>
#include <memory>
#include <string_view>

int main(int argc, char** argv)
{
    using namespace imp_sim;

    std::unique_ptr<Interface> interface = std::make_unique<NoneInterface>();
    std::string_view executableFile;
    bool realTime = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg.starts_with("--interface="))
        {
            auto interfaceName = arg.substr(12);
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
                std::println(stderr, "Invalid interface type: {}", interfaceName);
                return EXIT_FAILURE;
            }
        }
        else if (arg == "--realtime")
        {
            realTime = true;
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::string interfaceTypes = "none|tty";
#ifdef HAVE_X11
            interfaceTypes += "|x11";
#endif // HAVE_X11

            std::println("Usage: imp-sim [--interface={}] [--realtime] EXECUTABLE", interfaceTypes);
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
        Executable executable(executableFile);

        Simulator simulator(executable, *interface);
        simulator.run(realTime);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
