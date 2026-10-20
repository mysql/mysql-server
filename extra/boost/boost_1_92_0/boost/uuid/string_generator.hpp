#ifndef BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED
#define BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED

// Copyright 2010 Andy Tompkins
// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/from_chars.hpp>
#include <boost/uuid/detail/throw_invalid_uuid.hpp>
#include <boost/uuid/detail/cstring.hpp>
#include <boost/config.hpp>
#include <string>
#include <cstddef>

namespace boost {
namespace uuids {

// Generates a UUID from a string
//
// Accepts the following forms:
//
// 0123456789abcdef0123456789abcdef
// 01234567-89ab-cdef-0123-456789abcdef
// {01234567-89ab-cdef-0123-456789abcdef}
// {0123456789abcdef0123456789abcdef}

struct string_generator
{
public:

    using result_type = uuid;

    template<class CharIterator>
    BOOST_CXX14_CONSTEXPR uuid operator()( CharIterator first, CharIterator last, std::ptrdiff_t& pos, from_chars_error& err ) const noexcept
    {
        uuid u;

        pos = 0;

        if( first == last )
        {
            err = from_chars_error::unexpected_end_of_input;
            return u;
        }

        // check open brace

        bool has_open_brace = detail::from_chars_is_opening_brace( *first );

        if( has_open_brace )
        {
            ++first, ++pos;
        }

        bool has_dashes = false;

        for( int i = 0; i < 16; ++i )
        {
            if( first == last )
            {
                err = from_chars_error::unexpected_end_of_input;
                return u;
            }

            unsigned char v1 = detail::from_chars_digit_value( *first );

            if( v1 == 255 )
            {
                err = from_chars_error::hex_digit_expected;
                return u;
            }

            ++first, ++pos;

            if( first == last )
            {
                err = from_chars_error::unexpected_end_of_input;
                return u;
            }

            unsigned char v2 = detail::from_chars_digit_value( *first );

            if( v2 == 255 )
            {
                err = from_chars_error::hex_digit_expected;
                return u;
            }

            ++first, ++pos;

            u.data()[ i ] = static_cast<unsigned char>( ( v1 << 4 ) + v2 );

            if( i == 3 )
            {
                if( first == last )
                {
                    err = from_chars_error::unexpected_end_of_input;
                    return u;
                }

                has_dashes = detail::from_chars_is_dash( *first );

                if( has_dashes )
                {
                    ++first, ++pos;
                }
            }
            else if( i == 5 || i == 7 || i == 9 )
            {
                // if there are dashes, they must be in every slot

                if( has_dashes )
                {
                    if( first == last )
                    {
                        err = from_chars_error::unexpected_end_of_input;
                        return u;
                    }

                    if( detail::from_chars_is_dash( *first ) )
                    {
                        ++first, ++pos;
                    }
                    else
                    {
                        err = from_chars_error::dash_expected;
                        return u;
                    }
                }
            }
        }

        // check close brace

        if( has_open_brace )
        {
            if( first == last )
            {
                err = from_chars_error::unexpected_end_of_input;
                return u;
            }

            if( detail::from_chars_is_closing_brace( *first ) )
            {
                ++first, ++pos;
            }
            else
            {
                err = from_chars_error::closing_brace_expected;
                return u;
            }
        }

        // check end of string - any additional data is an invalid uuid

        if( first != last )
        {
            err = from_chars_error::unexpected_extra_input;
        }
        else
        {
            err = from_chars_error::none;
        }

        return u;
    }

    template<class CharIterator>
    BOOST_CXX14_CONSTEXPR uuid operator()( CharIterator first, CharIterator last ) const
    {
        std::ptrdiff_t pos = 0;
        from_chars_error err = {};

        uuid r = operator()( first, last, pos, err );

        if( err != from_chars_error::none )
        {
            detail::throw_invalid_uuid( pos, err );
        }

        return r;
    }

    template<class Str, class Ch = typename Str::value_type, class Tr = typename Str::traits_type>
    BOOST_CXX14_CONSTEXPR
    uuid operator()( Str const& str ) const
    {
        Ch const* first = str.data();
        Ch const* last = str.data() + str.size();

        return operator()( first, last );
    }

    template<class Ch>
    BOOST_CXX14_CONSTEXPR
    uuid operator()( Ch const* str ) const
    {
        Ch const* first = str;
        Ch const* last = str + detail::strlen_cx( str );

        return operator()( first, last );
    }
};

}} // namespace boost::uuids

#endif // BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED
