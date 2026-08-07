#ifndef BOOST_UUID_INVALID_UUID_HPP_INCLUDED
#define BOOST_UUID_INVALID_UUID_HPP_INCLUDED

// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/from_chars_result.hpp>
#include <boost/config.hpp>
#include <stdexcept>
#include <cstdio>
#include <cstddef>

namespace boost {
namespace uuids {

namespace detail
{

BOOST_CXX14_CONSTEXPR inline char const* fc_error_to_string( from_chars_error err ) noexcept
{
    switch( err )
    {
    case from_chars_error::none: return "no error";
    case from_chars_error::unexpected_end_of_input: return "unexpected end of input";
    case from_chars_error::hex_digit_expected: return "hex digit expected";
    case from_chars_error::dash_expected: return "dash expected";
    case from_chars_error::closing_brace_expected: return "closing brace expected";
    case from_chars_error::unexpected_extra_input: return "unexpected extra input";
    default: return "unknown error";
    }
}

} // namespace detail

class BOOST_SYMBOL_VISIBLE invalid_uuid: public std::runtime_error
{
private:

    std::ptrdiff_t pos_;
    from_chars_error err_;

private:

    static std::runtime_error create_base( std::ptrdiff_t pos, from_chars_error err )
    {
        char buffer[ 128 ];
        std::snprintf( buffer, sizeof( buffer ), "Invalid UUID string at position %td: %s", pos, detail::fc_error_to_string( err ) );

        return std::runtime_error( buffer );
    }

public:

    invalid_uuid( std::ptrdiff_t pos, from_chars_error err ): std::runtime_error( create_base( pos, err ) ), pos_( pos ), err_( err )
    {
    }

    std::ptrdiff_t position() const noexcept
    {
        return pos_;
    }

    from_chars_error error() const noexcept
    {
        return err_;
    }
};

}} // namespace boost::uuids

#endif // BOOST_UUID_INVALID_UUID_HPP_INCLUDED
