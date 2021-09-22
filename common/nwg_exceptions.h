#pragma once

/*
 * Exception classes for nwg-launchers
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <stdexcept>
#include <string>
#include <string_view>

// wraps strerror into std::string
std::string error_description(int err);

struct ErrnoException: public std::runtime_error {
    static inline std::string concat(std::string_view a, std::string b) {
        b.insert(0, a.data(), a.size());
        return b;
    }
    /* Make sure to use this constructor as follows:
     *
     *    int err = errno;
     *    ErrnoException{ "desc", err }
     *
     * and NOT
     *
     *    ErrnoException{ "desc", errno }
     *
     * because creating `string_view` can potentially change `errno` value,
     * and the initialization order is not guaranteed */
    ErrnoException(std::string_view desc, int err):
        std::runtime_error{ concat(desc, error_description(err)) }
    {
        // intentionally left blank
    }
    ErrnoException(int err): std::runtime_error{ error_description(err) } {
        // intentionally left blank
    }
};
