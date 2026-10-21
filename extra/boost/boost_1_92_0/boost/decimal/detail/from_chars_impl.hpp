// Copyright 2024 - 2035 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FROM_CHARS_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_FROM_CHARS_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/from_chars_result.hpp>
#include <boost/decimal/detail/from_chars_integer_impl.hpp>
#include <boost/decimal/detail/parser.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/write_payload.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4127)
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_chars_general_impl(const char* first, const char* last, TargetDecimalType& value, const chars_format fmt) noexcept -> from_chars_result
{
    using significand_type = std::conditional_t<(std::numeric_limits<typename TargetDecimalType::significand_type>::digits >
                                                 std::numeric_limits<std::uint64_t>::digits),
                                                 int128::uint128_t, std::uint64_t>;

    BOOST_DECIMAL_IF_CONSTEXPR (is_fast_type_v<TargetDecimalType>)
    {
        if (fmt == chars_format::cohort_preserving_scientific)
        {
            return {first, std::errc::invalid_argument};
        }
    }

    if (BOOST_DECIMAL_UNLIKELY(first >= last))
    {
        return {first, std::errc::invalid_argument};
    }

    bool sign {};
    significand_type significand {};
    std::int32_t expval {};

    auto r {detail::parser(first, last, sign, significand, expval, fmt)};

    if (!r)
    {
        if (r.ec == std::errc::not_supported)
        {
            using resultant_sig_type = typename TargetDecimalType::significand_type;

            resultant_sig_type payload_value {};
            if (significand < std::numeric_limits<resultant_sig_type>::max())
            {
                payload_value = static_cast<resultant_sig_type>(significand);
            }

            if (expval > 0)
            {
                value = write_payload<TargetDecimalType, true>(payload_value);
            }
            else
            {
                value = write_payload<TargetDecimalType, false>(payload_value);
            }

            if (sign)
            {
                value = -value;
            }

            r.ec = std::errc();
        }
        else if (r.ec == std::errc::value_too_large)
        {
            value = sign ? -std::numeric_limits<TargetDecimalType>::infinity() :
                            std::numeric_limits<TargetDecimalType>::infinity();
            r.ec = std::errc();
        }
        else
        {
            value = std::numeric_limits<TargetDecimalType>::signaling_NaN();
            errno = static_cast<int>(r.ec);
        }
    }
    else
    {
        BOOST_DECIMAL_IF_CONSTEXPR (!is_fast_type_v<TargetDecimalType>)
        {
            if (fmt == chars_format::cohort_preserving_scientific)
            {
                const auto sig_digs {detail::num_digits(significand)};
                if (sig_digs > precision_v<TargetDecimalType>)
                {
                    // If we are parsing more digits than are representable there's no concept of cohorts
                    return {last, std::errc::value_too_large};
                }
            }
        }

        value = TargetDecimalType(significand, expval, sign);
    }

    return r;
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#endif // !defined(BOOST_DECIMAL_DISABLE_CLIB)

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_FROM_CHARS_IMPL_HPP
