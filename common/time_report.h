/*
 * Classes for nwg-launchers
 * Copyright (c) 2021 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#pragma once

#include <time.h>
#include <common/log.h>

namespace ntime {

    struct Time {
        std::string_view name;
        timespec time;
        Time* next = nullptr;

        Time( std::string_view name ): name{ name } {
            if (clock_gettime(CLOCK_MONOTONIC, &time)) {
                throw std::runtime_error{ "clock_gettime(CLOCK_MONOTONIC, ...) failed" };
            }
        }

        Time( std::string_view name, Time& prev ): Time{ name } {
            if (prev.next) {
                throw std::logic_error{ "Time::Time(name, prev)" };
            }
            prev.next = this;
        }

        Time(Time&&) = delete;
        Time(const Time&) = delete;
    };

    namespace detail {

        inline size_t to_ms(timespec t) {
            return t.tv_sec * 1000 + t.tv_nsec / 1000000;
        }

        inline size_t diff_ms(const Time& t1, const Time& t2) {
            return to_ms(t2.time) - to_ms(t1.time);
        }

        const Time& report(const Time& t1, const Time& t2) {
            auto diff = diff_ms(t1, t2);

            Log::plain(t2.name, ": ", diff, "ms");
            if (t2.next) {
                return report(t2, *t2.next);
            }
            return t2;
        }
    }; // namespace detail

    void report(const Time& initial) {
        if (!initial.next) {
            throw std::logic_error{ "time::report(initial): inital.next is empty" };
        }

        auto& last = detail::report(initial, *initial.next);
        if (initial.next != &last) {
            auto diff = detail::diff_ms(initial, last);
            Log::plain( "Total time: ", diff, "ms");
        }
    }

} // namespace time_report
