#pragma once
#include <iostream>

namespace Log {
    template <typename ... Ts>
    void write(std::ostream& out, Ts && ... ts) {
        ((out << ts), ...);
        out << '\n';
    }

    template <typename ... Ts>
    void info(Ts && ... ts) { write(std::cerr, "INFO: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void warn(Ts && ... ts) { write(std::cerr, "WARN: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void error(Ts && ... ts) { write(std::cerr, "ERROR: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void plain(Ts && ... ts) { write(std::cerr, std::forward<Ts>(ts)...); }
}
