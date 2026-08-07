// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_DETAIL_OSTDSTREAM_HPP
#define BOOST_OPENMETHOD_DETAIL_OSTDSTREAM_HPP

#include <array>
#include <cstdio>
#include <charconv>
#include <random>
#include <string_view>

namespace boost::openmethod {

namespace detail {

// -----------------------------------------------------------------------------
// lightweight ostream

struct ostdstream {
    FILE* stream = nullptr;

    ostdstream(FILE* s = nullptr) : stream(s) {
    }

    void on(FILE* s = stderr) {
        this->stream = s;
    }

    void off() {
        stream = nullptr;
    }

    auto is_on() const -> bool {
        return stream != nullptr;
    }
};

struct ostderr : ostdstream {
    ostderr() : ostdstream(stderr) {
    }
};

inline ostdstream cerr;

inline auto operator<<(ostdstream& os, const char* str) -> ostdstream& {
    if (os.stream) {
        fputs(str, os.stream);
    }

    return os;
}

inline auto
operator<<(ostdstream& os, const std::string_view& view) -> ostdstream& {
    if (os.stream) {
        fwrite(view.data(), sizeof(*view.data()), view.length(), os.stream);
    }

    return os;
}

inline auto operator<<(ostdstream& os, const void* value) -> ostdstream& {
    if (os.stream) {
        std::array<char, 20> str;
        auto end = std::to_chars(
                       str.data(), str.data() + str.size(),
                       reinterpret_cast<uintptr_t>(value), 16)
                       .ptr;
        os << std::string_view(str.data(), end - str.data());
    }

    return os;
}

inline auto operator<<(ostdstream& os, void (*value)()) -> ostdstream& {
    if (os.stream) {
        std::array<char, 20> str;
        auto end = std::to_chars(
                       str.data(), str.data() + str.size(),
                       reinterpret_cast<uintptr_t>(value), 16)
                       .ptr;
        os << std::string_view(str.data(), end - str.data());
    }

    return os;
}

inline auto operator<<(ostdstream& os, std::size_t value) -> ostdstream& {
    if (os.stream) {
        std::array<char, 20> str;
        auto end =
            std::to_chars(str.data(), str.data() + str.size(), value).ptr;
        os << std::string_view(str.data(), end - str.data());
    }

    return os;
}

} // namespace detail

} // namespace boost::openmethod

#endif
