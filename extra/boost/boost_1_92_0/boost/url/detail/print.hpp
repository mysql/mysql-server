 //
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_PRINT_HPP
#define BOOST_URL_DETAIL_PRINT_HPP

#include <boost/core/detail/string_view.hpp>
#include <cstdint>
#include <type_traits>

namespace boost {
namespace urls {
namespace detail {

// std::uint64_t
// 18446744073709551615
//          1          2
template<class T>
struct printed
    : std::false_type
{
};

// 16-bit unsigned
template<>
class printed<std::uint16_t>
    : std::false_type
{
    char n_;
    char buf_[5];

public:
    printed(std::uint16_t n)
    {
        char* it =
            buf_ + sizeof(buf_);
        if(n == 0)
        {
            *--it = '0';
            n_ = 1;
        }
        else
        {
            while(n > 0)
            {
                *--it = '0' + (n % 10);
                n /= 10;
            }
            n_ = static_cast<char>(
                sizeof(buf_) - (
                    it - buf_));
        }
    }

    core::string_view
    string() const noexcept
    {
        return core::string_view(buf_ +
            sizeof(buf_) - n_, n_);
    }
};

template<class T>
printed<T>
make_printed(T t)
{
    return printed<T>(t);
}

} // detail
} // urls
} // boost

#endif
