#ifndef BOOST_LEAF_DETAIL_TYPE_NAME_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_TYPE_NAME_HPP_INCLUDED

// Copyright 2018-2026 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/demangle.hpp>

namespace boost { namespace leaf {

namespace detail
{
    struct type_name
    {
        char const * name_not_zero_terminated_at_length;
        std::size_t length;
        std::size_t hash;

        friend bool operator==(type_name const & a, type_name const & b) noexcept
        {
            BOOST_LEAF_ASSERT((a.hash == b.hash) == (a.length == b.length && std::memcmp(a.name_not_zero_terminated_at_length, b.name_not_zero_terminated_at_length, a.length) == 0));
            return a.hash == b.hash;
        }

        friend bool operator!=(type_name const & a, type_name const & b) noexcept
        {
            return !(a == b);
        }

        template <class CharT, class Traits>
        friend std::ostream & operator<<(std::basic_ostream<CharT, Traits> & os, type_name const & x)
        {
            return os.write(x.name_not_zero_terminated_at_length, x.length);
        }

        template <std::size_t S>
        friend char * to_zstr(char (&zstr)[S], type_name const & x) noexcept
        {
            std::size_t n = x.length < S - 1 ? x.length : S - 1;
            std::memcpy(zstr, x.name_not_zero_terminated_at_length, n);
            zstr[n] = 0;
            return zstr;
        }
    };

    template <class T>
    type_name get_type_name()
    {
        n::r parsed = n::p<T>();
        return { parsed.name_not_zero_terminated_at_length, parsed.length, parsed.hash };
    }
} // namespace detail

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_DETAIL_TYPE_NAME_HPP_INCLUDED
