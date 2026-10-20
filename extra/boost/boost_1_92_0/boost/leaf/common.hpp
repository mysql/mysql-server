#ifndef BOOST_LEAF_COMMON_HPP_INCLUDED
#define BOOST_LEAF_COMMON_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>

#include <iosfwd>
#include <cerrno>
#include <cstring>

#if BOOST_LEAF_CFG_STD_STRING
#   include <string>
#endif

#if BOOST_LEAF_CFG_WIN32
#   include <windows.h>
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif

namespace boost { namespace leaf {

struct e_api_function { char const * value; };

#if BOOST_LEAF_CFG_STD_STRING

struct e_file_name
{
    std::string value;
};

#else

struct e_file_name
{
    char const * value = "<unavailable>";
    BOOST_LEAF_CONSTEXPR explicit e_file_name( char const * ) { }
};

#endif

struct e_errno
{
    int value;

    explicit e_errno(int val=errno): value(val) { }

    template <class CharT, class Traits>
    friend std::ostream & operator<<(std::basic_ostream<CharT, Traits> & os, e_errno const & err)
    {
        return os << err.value << ", \"" << std::strerror(err.value) << '"';
    }

    template <class Encoder>
    friend void output( Encoder & e, e_errno const & x )
    {
        output_at(e, x.value, "errno");
        output_at(e, std::strerror(x.value), "strerror");
    }
};

struct e_type_info_name { char const * value; };

struct e_at_line { int value; };

namespace windows
{
    struct e_LastError
    {
        unsigned value;

        explicit e_LastError(unsigned val): value(val) { }

#if BOOST_LEAF_CFG_WIN32
        e_LastError(): value(GetLastError()) { }

        template <class CharT, class Traits>
        friend std::ostream & operator<<(std::basic_ostream<CharT, Traits> & os, e_LastError const & err)
        {
            struct msg_buf
            {
                LPVOID * p;
                msg_buf(): p(nullptr) { }
                ~msg_buf() noexcept { if(p) LocalFree(p); }
            };
            msg_buf mb;
            if( FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                err.value,
                MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
                (LPSTR)&mb.p,
                0,
                nullptr) )
            {
                BOOST_LEAF_ASSERT(mb.p != nullptr);
                char * z = std::strchr((LPSTR)mb.p,0);
                if( z != (LPSTR)mb.p && z[-1] == '\n' )
                    *--z = 0;
                if( z != (LPSTR)mb.p && z[-1] == '\r' )
                    *--z = 0;
                return os << err.value << ", \"" << (LPCSTR)mb.p << '"';
            }
            return os;
        }
#endif // #if BOOST_LEAF_CFG_WIN32
    };
} // namespace windows

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_COMMON_HPP_INCLUDED
