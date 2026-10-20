// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_SHRINK_SIGNIFICAND_HPP
#define BOOST_DECIMAL_DETAIL_SHRINK_SIGNIFICAND_HPP

#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <boost/decimal/detail/power_tables.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {

template <typename TargetType = std::uint32_t, typename Integer, typename Exp>
BOOST_DECIMAL_CUDA_CONSTEXPR auto shrink_significand(Integer sig, Exp& exp) noexcept -> TargetType
{
    using Unsigned_Integer = make_unsigned_t<Integer>;
    constexpr auto max_digits {std::numeric_limits<TargetType>::digits10};

    auto unsigned_sig {make_positive_unsigned(sig)};
    const auto sig_dig {num_digits(unsigned_sig)};

    if (sig_dig > max_digits)
    {
        unsigned_sig /= pow10(static_cast<Unsigned_Integer>(sig_dig - max_digits));
        exp += sig_dig - max_digits;
    }

    return static_cast<TargetType>(unsigned_sig);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_SHRINK_SIGNIFICAND_HPP
