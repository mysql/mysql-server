// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_POW_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_POW_IMPL_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <type_traits>
#endif

namespace boost { namespace decimal { namespace detail {

template<typename T, typename UnsignedIntegralType>
constexpr auto pow_n_impl(T b, UnsignedIntegralType p) noexcept -> std::enable_if_t<(detail::is_decimal_floating_point_v<T> && std::is_integral<UnsignedIntegralType>::value && std::is_unsigned<UnsignedIntegralType>::value), T> // NOLINT(misc-no-recursion)
{
    using local_unsigned_integral_type = UnsignedIntegralType;

    constexpr T one { 1, 0 };

    T result { };

    if (p < static_cast<local_unsigned_integral_type>(UINT8_C(5)))
    {
        switch (p)
        {
            case static_cast<local_unsigned_integral_type>(UINT8_C(4)):
                result = b; result *= result; result *= result; break;
            case static_cast<local_unsigned_integral_type>(UINT8_C(3)):
                result = b; result *= result; result *= b; break;
            case static_cast<local_unsigned_integral_type>(UINT8_C(2)):
                result = b; result *= result; break;
            default: // LCOV_EXCL_LINE
            case static_cast<local_unsigned_integral_type>(UINT8_C(1)):
                result = b; break;
      }
    }
    else
    {
        // Calculate (b ^ p) using the ladder method for powers.

        result = one;

        T y(b);

        auto p_local = static_cast<local_unsigned_integral_type>(p);

        for(;;)
        {
            const auto do_power_multiply =
              (static_cast<std::uint_fast8_t>(p_local & static_cast<unsigned>(UINT8_C(1))) != static_cast<std::uint_fast8_t>(UINT8_C(0)));

            if(do_power_multiply)
            {
              result *= y;
            }

            p_local >>= static_cast<unsigned>(UINT8_C(1));

            if(p_local == static_cast<local_unsigned_integral_type>(UINT8_C(0)))
            {
                break;
            }

            y *= y;
        }
    }

    return result;
}

template<typename T>
constexpr auto pow_2_impl(int e2) noexcept -> std::enable_if_t<detail::is_decimal_floating_point_v<T>, T>
{
    constexpr T one { 1, 0 };

    T result { };

    if(e2 > 0)
    {
        if(e2 < 64)
        {
            result = T { UINT64_C(1) << e2, 0 };
        }
        else
        {
            constexpr T two { 2, 0 };

            result = detail::pow_n_impl(two, static_cast<unsigned>(e2));
        }
    }
    else if(e2 < 0)
    {
        const auto e2_neg = static_cast<unsigned>(~e2) + 1U;

        if(e2 > -64)
        {
            const T local_p2 { UINT64_C(1) << e2_neg, 0 };

            result = one / local_p2;
        }
        else
        {
            constexpr T half {5, -1};

            result = detail::pow_n_impl(half, e2_neg);
        }
    }
    else
    {
        // Excluded from LCOV since it's apparently optimized away or otherwise
        // missing from LCOV. Verified this line is covered in the unit tests.

        result = one; // LCOV_EXCL_LINE
    }

    return result;
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif
