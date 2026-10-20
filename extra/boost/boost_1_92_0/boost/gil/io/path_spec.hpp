//
// Copyright 2007-2008 Andreas Pokorny, Christian Henning
// Copyright 2024 Dirk Stolle
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_IO_PATH_SPEC_HPP
#define BOOST_GIL_IO_PATH_SPEC_HPP

#include <boost/gil/io/detail/filesystem.hpp>

#include <cstdlib>
#include <cwchar>
#include <string>
#include <type_traits>

namespace boost { namespace gil { namespace detail {

template<typename P> struct is_supported_path_spec              : std::false_type {};
template<> struct is_supported_path_spec< std::string >         : std::true_type {};
template<> struct is_supported_path_spec< const std::string >   : std::true_type {};
template<> struct is_supported_path_spec< std::wstring >        : std::true_type {};
template<> struct is_supported_path_spec< const std::wstring >  : std::true_type {};
template<> struct is_supported_path_spec< char const* >         : std::true_type {};
template<> struct is_supported_path_spec< char* >               : std::true_type {};
template<> struct is_supported_path_spec< const wchar_t* >      : std::true_type {};
template<> struct is_supported_path_spec< wchar_t* >            : std::true_type {};

template<int i> struct is_supported_path_spec<const char [i]>       : std::true_type {};
template<int i> struct is_supported_path_spec<char [i]>             : std::true_type {};
template<int i> struct is_supported_path_spec<const wchar_t [i]>    : std::true_type {};
template<int i> struct is_supported_path_spec<wchar_t [i]>          : std::true_type {};

template<> struct is_supported_path_spec<filesystem::path> : std::true_type {};
template<> struct is_supported_path_spec<filesystem::path const> : std::true_type {};

inline std::string convert_to_string( std::string const& obj)
{
   return obj;
}

inline std::string convert_to_string( std::wstring const& s )
{
    std::mbstate_t state = std::mbstate_t();
    const wchar_t* str = s.c_str();
    const std::size_t len = std::wcsrtombs(nullptr, &str, 0, &state);
    std::string result(len, '\0');
    std::wcstombs( &result[0], s.c_str(), len );

    return result;
}

inline std::string convert_to_string( char const* str )
{
    return std::string( str );
}

inline std::string convert_to_string( char* str )
{
    return std::string( str );
}

inline std::string convert_to_string(filesystem::path const& path)
{
    return convert_to_string(path.string());
}

inline char const* convert_to_native_string( char* str )
{
    return str;
}

inline char const* convert_to_native_string( char const* str )
{
    return str;
}

inline char const* convert_to_native_string( const std::string& str )
{
   return str.c_str();
}

inline char const* convert_to_native_string( const wchar_t* str )
{
    std::mbstate_t state = std::mbstate_t();
    const std::size_t len = std::wcsrtombs(nullptr, &str, 0, &state) + 1;
    char* c = new char[len];
    std::wcstombs( c, str, len );

    return c;
}

inline char const* convert_to_native_string( std::wstring const& str )
{
    std::mbstate_t state = std::mbstate_t();
    const wchar_t* wstr = str.c_str();
    const std::size_t len = std::wcsrtombs(nullptr, &wstr, 0, &state) + 1;
    char* c = new char[len];
    std::wcstombs( c, str.c_str(), len );

    return c;
}

}}} // namespace boost::gil::detail

#endif
