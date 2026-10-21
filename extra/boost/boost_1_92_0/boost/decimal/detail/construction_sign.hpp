// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CONSTRUCTION_SIGN_HPP
#define BOOST_DECIMAL_DETAIL_CONSTRUCTION_SIGN_HPP

#include <boost/decimal/detail/config.hpp>

namespace boost {
namespace decimal {

enum class construction_sign : bool
{
    positive = false,
    negative = true
};

namespace detail {

class construction_sign_wrapper
{
private:

    construction_sign value_;

public:

    constexpr construction_sign_wrapper() noexcept = delete;

    BOOST_DECIMAL_CUDA_CONSTEXPR construction_sign_wrapper(const construction_sign value) noexcept : value_{value} {}

    BOOST_DECIMAL_CUDA_CONSTEXPR construction_sign_wrapper(const bool value) noexcept
        : value_{ value ? construction_sign::negative : construction_sign::positive } {}

    BOOST_DECIMAL_CUDA_CONSTEXPR operator bool() const noexcept { return static_cast<bool>(value_); }
};

} // namespace detail

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CONSTRUCTION_SIGN_HPP
