// Copyright (C) 2026 Andrzej Krzemieński.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com
//
//
// This header provides definitions required by any specialization of
// optional<>.

#ifndef BOOST_OPTIONAL_DETAIL_OPTIONAL_SELECT_IMPLEMENTATION_01FEB2026_HPP
#define BOOST_OPTIONAL_DETAIL_OPTIONAL_SELECT_IMPLEMENTATION_01FEB2026_HPP

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_CONSTEXPR) &&             \
    !defined(BOOST_NO_CXX11_REF_QUALIFIERS) &&        \
    !defined(BOOST_NO_CXX11_TRAILING_RESULT_TYPES) && \
    !defined(BOOST_NO_CXX11_UNRESTRICTED_UNION) &&    \
    !defined(BOOST_NO_CXX11_NOEXCEPT) &&              \
    !defined(BOOST_NO_CXX11_DEFAULTED_MOVES) &&       \
    !defined(BOOST_OPTIONAL_CONFIG_DISABLE_UNION_OPTIONAL)
# define BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
#endif


// In C++20 we have `std::construct_at()` which is a constexpr equivalent of
// placement-new. We can then make more functions constexpr.
// TBD: This additional constexpr-ication is left for the future.
# define BOOST_OPTIONAL_CXX20_CONSTEXPR

#endif //BOOST_OPTIONAL_DETAIL_OPTIONAL_SELECT_IMPLEMENTATION_01FEB2026_HPP
