// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_COMPONENTS_HPP
#define BOOST_DECIMAL_DETAIL_COMPONENTS_HPP

#include <boost/decimal/detail/config.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4324) // Structure was padded due to alignment specifier
#endif


// Internal use only, and changes based on the types
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpadded"
#endif

template <typename SigType, typename BiasedExpType>
struct decimal_components
{
    using significand_type = SigType;
    using biased_exponent_type = BiasedExpType;

    significand_type sig {};
    biased_exponent_type exp {};
    bool sign {};

    constexpr decimal_components() = default;
    constexpr decimal_components(const decimal_components& rhs) = default;
    constexpr decimal_components& operator=(const decimal_components& rhs) = default;
    BOOST_DECIMAL_CUDA_CONSTEXPR decimal_components(SigType sig_, BiasedExpType exp_, bool sign_) : sig{sig_}, exp{exp_}, sign{sign_} {}

    BOOST_DECIMAL_CUDA_CONSTEXPR auto full_significand() const -> significand_type
    {
        return sig;
    }

    BOOST_DECIMAL_CUDA_CONSTEXPR auto biased_exponent() const -> biased_exponent_type
    {
        return exp;
    }

    BOOST_DECIMAL_CUDA_CONSTEXPR auto isneg() const -> bool
    {
        return sign;
    }

    BOOST_DECIMAL_CUDA_CONSTEXPR auto to_components() const -> decimal_components
    {
        return *this;
    }

    template <typename T1, typename T2>
    explicit BOOST_DECIMAL_CUDA_CONSTEXPR operator decimal_components<T1, T2>() const
    {
        return decimal_components<T1, T2>{static_cast<T1>(sig), static_cast<T2>(exp), sign};
    }
};

} // namespace impl

using decimal32_t_components = impl::decimal_components<std::uint32_t, std::int32_t>;

using decimal_fast32_t_components = impl::decimal_components<std::uint32_t, std::int32_t>;

using decimal64_t_components = impl::decimal_components<std::uint64_t, std::int32_t>;

using decimal_fast64_t_components = impl::decimal_components<std::uint64_t, std::int32_t>;

using decimal128_t_components = impl::decimal_components<boost::int128::uint128_t, std::int32_t>;

using decimal_fast128_t_components = impl::decimal_components<boost::int128::uint128_t, std::int32_t>;

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_COMPONENTS_HPP
