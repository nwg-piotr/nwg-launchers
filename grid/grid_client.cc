/*
 * GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <fstream>
#include <string_view>
#include <vector>

#include "nwg_tools.h"
#include "nwg_exceptions.h"
#include "nwgconfig.h"

const char* const HELP_MESSAGE = "\
GTK application grid: nwggrid " VERSION_STR " (c) 2021 Piotr Miller, Sergey Smirnykh & Contributors \n\n\
Usage:\n\
    nwggrid -client      sends -SIGUSR1 to nwggrid-server, requires nwggrid-server running\n\
    nwggrid [ARGS...]    launches nwggrid-server -oneshot ARGS...\n\n\
\
See also:\n\
    nwggrid-server -h\n";

int main(int argc, char* argv[]) {
    try {
        using namespace std::string_view_literals;

        if (argc >= 2) {
            std::string_view argv1{ argv[1] };

            if (argv1 == "-h"sv) {
                Log::plain(HELP_MESSAGE);
                return EXIT_SUCCESS;
            }

            if (argv1 == "-client"sv) {
                auto pid_file = get_pid_file("nwggrid-server.pid");
                Log::info("Using pid file ", pid_file);
                Log::info("Running in client mode");
                if (argc != 2) {
                    Log::warn("Arguments after '-client' must be passed to nwggrid-server");
                }
                auto pid = get_instance_pid(pid_file.c_str());
                if (!pid) {
                    throw std::runtime_error{ "nwggrid-server is not running" };
                }
                if (kill(*pid, SIGUSR1) != 0) {
                    throw std::runtime_error{ "failed to send SIGUSR1 to the pid" };
                }
                Log::plain("Success");
                return EXIT_SUCCESS;
            }
        }
        char path[] = INSTALL_PREFIX_STR "/bin/nwggrid-server";
        char oneshot[] = "-oneshot";
        auto arguments = new char*[argc + 2];
        arguments[0] = path;
        for (int i = 1; i < argc; ++i) {
            arguments[i] = strdup(argv[i]);
            if (!arguments[i]) {
                int err = errno;
                // totally unnecessary cleanup, but why not?
                for (int j = 0; j < i; ++j) {
                    free(arguments[j]);
                }
                throw std::runtime_error{ error_description(err) };
            }
        }
        arguments[argc] = oneshot;
        arguments[argc + 1] = (char*)NULL;

        auto r = execv(
            INSTALL_PREFIX_STR "/bin/nwggrid-server",
            arguments
        );
        if (r == -1) {
            throw ErrnoException{ errno };
        }
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
