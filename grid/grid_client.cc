
#include <fstream>
#include <string_view>
#include <vector>

#include "nwg_tools.h"
#include "nwgconfig.h"

const char* const HELP_MESSAGE = "\
Usage:\n\
    nwggrid -client      sends -SIGUSR1 to nwggrid-server, requires nwggrid-server running\n\
    nwggrid [ARGS...]    launches nwggrid -oneshot ARGS...\n\n";

int main(int argc, char* argv[]) {
    try {
        using namespace std::string_view_literals;
        // TODO: maybe use dbus if it is present?
        auto pid_file = get_runtime_dir();
        pid_file /= "nwggrid.pid";
        Log::info("Using pid file ", pid_file);

        if (argc >= 2 && std::string_view{ argv[1] } == "-h"sv) {
            // TODO: nice help message
            Log::plain(HELP_MESSAGE);
        }
        if (argc >= 2 && std::string_view{ argv[1] } == "-client"sv) {
            Log::info("Running in client mode");
            // send signal or fail
            // opening file worked - file exists
            if (std::ifstream pid_stream{ pid_file }) {
                // set to not throw exceptions
                pid_stream.exceptions(std::ifstream::goodbit);
                if (pid_t saved_pid; pid_stream >> saved_pid) {
                    if (saved_pid <= 0) {
                        throw std::runtime_error{ "Non-positive value stored in pid file" };
                    }
                    if (kill(saved_pid, 0) != 0) {
                        throw std::runtime_error{ "process with pid specified in .pid file does not exist" };
                    }
                    Log::info("Found running instance with pid '", saved_pid, "'");
                    if (kill(saved_pid, SIGUSR1) != 0) {
                        throw std::runtime_error{ "failed to send SIGUSR1 to pid specified in .pid file" };
                    }
                    Log::plain("OK");
                } else {
                    Log::error("nwggrid-server is not running");
                    return EXIT_FAILURE;
                }
            }
            return EXIT_SUCCESS;
        }
        std::vector<std::string> arguments;
        arguments.emplace_back(INSTALL_PREFIX_STR "/bin/nwggrid-server");
        arguments.emplace_back("-oneshot");
        for (int i = 1; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        Glib::spawn_async(
            Glib::get_current_dir(), // working dir
            arguments
        );
        return EXIT_SUCCESS;
    } catch (const Glib::Error& err) {
        // Glib::ustring performs conversion with respect to locale settings
        // it might throw (and it does [on my machine])
        // so let's try our best
        auto ustr = err.what();
        try {
            Log::error(ustr);
        } catch (const Glib::ConvertError& err) {
            Log::plain("[message conversion failed]");
            Log::error(std::string_view{ ustr.data(), ustr.bytes() });
        } catch (...) {
            Log::error("Failed to print error message due to unknown error");
        }
    } catch (const std::exception& err) {
        Log::error(err.what());
    }
    return EXIT_FAILURE;
}
