// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT32_HPP
#define BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT32_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/fast_float/compute_float64.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#include <cmath>
#endif

namespace boost {
namespace decimal {
namespace detail {
namespace fast_float {

BOOST_DECIMAL_CXX20_CONSTEXPR auto compute_float32(std::int64_t power, std::uint64_t i, bool negative, bool& success) noexcept -> float
{
    const double d = compute_float64(power, i, negative, success);
    float return_val {};

    if (BOOST_DECIMAL_LIKELY(success))
    {
        return_val = static_cast<float>(d);

        // Some compilers (e.g. Intel) will optimize std::isinf to always false depending on compiler flags
        //
        // From Intel(R) oneAPI DPC++/C++ Compiler 2023.0.0 (2023.0.0.20221201)
        // warning: comparison with infinity always evaluates to false in fast floating point modes [-Wtautological-constant-compare]
        // if (std::isinf(return_val))

        const auto abs_return_val = return_val < 0 ? -return_val : return_val;
        if (abs_return_val > (std::numeric_limits<float>::max)())
        {
            return_val = negative ? -HUGE_VALF : HUGE_VALF;
            success = true;
        }
    }

    return return_val;
}

} // namespace fast_float
} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT32_HPP
