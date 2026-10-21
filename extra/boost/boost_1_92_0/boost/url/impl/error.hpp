//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_ERROR_HPP
#define BOOST_URL_IMPL_ERROR_HPP

#include <boost/url/grammar/error.hpp>
#include <type_traits>

namespace boost {

//-----------------------------------------------
namespace system {
template<>
struct is_error_code_enum<::boost::urls::error>
{
    static bool const value = true;
};
} // system
//-----------------------------------------------

namespace urls {

namespace detail {

struct BOOST_SYMBOL_VISIBLE
    error_cat_type
    : system::error_category
{
    BOOST_URL_CXX20_CONSTEXPR
    const char* name(
        ) const noexcept override
    {
        return "boost.url";
    }

    std::string message(
        int code) const override
    {
        return message(code, nullptr, 0);
    }

    BOOST_URL_CXX20_CONSTEXPR
    char const* message(
        int code,
        char*,
        std::size_t) const noexcept override
    {
        switch(static_cast<error>(code))
        {
        case error::success: return "success";
        case error::illegal_null: return "illegal null";
        case error::illegal_reserved_char: return "illegal reserved char";
        case error::non_canonical: return "non canonical";
        case error::bad_pct_hexdig: return "bad hexdig in pct-encoding";
        case error::incomplete_encoding: return "incomplete pct-encoding";
        case error::missing_pct_hexdig: return "missing hexdig in pct-encoding";
        case error::no_space: return "no space";
        case error::not_a_base: return "not a base";
        }
        return "";
    }

    BOOST_URL_CXX20_CONSTEXPR
    system::error_condition
        default_error_condition(
            int ev) const noexcept override;

    BOOST_SYSTEM_CONSTEXPR error_cat_type() noexcept
        : error_category(0xbc15399d7a4ce829)
    {
    }
};

#if defined(BOOST_URL_HAS_CXX20_CONSTEXPR)
inline constexpr error_cat_type error_cat{};
#else
BOOST_URL_DECL extern error_cat_type error_cat;
#endif

} // detail

inline
BOOST_SYSTEM_CONSTEXPR
system::error_code
make_error_code(
    error ev) noexcept
{
    return system::error_code{
        static_cast<std::underlying_type<
            error>::type>(ev),
        detail::error_cat};
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::error_condition
detail::error_cat_type::
    default_error_condition(
        int ev) const noexcept
{
    switch(static_cast<error>(ev))
    {
    default:
        return {ev, *this};
    case error::bad_pct_hexdig:
    case error::incomplete_encoding:
    case error::missing_pct_hexdig:
        return grammar::condition::fatal;
    }
}

} // urls
} // boost

#endif
