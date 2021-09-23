
/*
 * Exception classes for nwg-launchers
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */


#include <cstring>
#include "nwg_exceptions.h"

std::string error_description(int err) {
    errno = 0;
    auto cstr = std::strerror(err);
    if (!cstr || errno) {
        throw std::runtime_error{ "failed to retrieve errno description: strerror return NULL" };
    }
    return { cstr };
}
