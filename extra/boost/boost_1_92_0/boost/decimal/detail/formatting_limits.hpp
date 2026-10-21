// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FORMATTING_LIMITS_HPP
#define BOOST_DECIMAL_DETAIL_FORMATTING_LIMITS_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/buffer_sizing.hpp>
#include <boost/decimal/detail/chars_format.hpp>
#include <boost/decimal/detail/attributes.hpp>

namespace boost {
namespace decimal {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision = -1>
class formatting_limits
{
private:

    // Class invariant
    static_assert(Precision > 0 || Precision == -1, "A specified precision must be greater than zero");

    static constexpr std::size_t required_characters() noexcept
    {
        // Add an extra character for null terminator
        const auto local_precision {detail::get_real_precision<DecimalType>(Precision)};
        return detail::total_buffer_length<DecimalType>(local_precision, detail::emax_v<DecimalType>, true) + 1U;
    }

    static constexpr std::size_t fixed_characters() noexcept
    {
        // Maximum would be
        // sign + 0 + . + exponent number of zeros + precision + null terminator
        // e.g. -0.00...01
        return static_cast<std::size_t>(3U + detail::emax_v<DecimalType> + 1U);
    }

public:

    static constexpr std::size_t scientific_format_max_chars {required_characters()};

    static constexpr std::size_t fixed_format_max_chars { fixed_characters() };

    static constexpr std::size_t hex_format_max_chars { scientific_format_max_chars };

    static constexpr std::size_t cohort_preserving_scientific_max_chars { scientific_format_max_chars };

    static constexpr std::size_t general_format_max_chars { scientific_format_max_chars };

    static constexpr std::size_t max_chars { fixed_format_max_chars };
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L)

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision>
constexpr std::size_t formatting_limits<DecimalType, Precision>::scientific_format_max_chars;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision>
constexpr std::size_t formatting_limits<DecimalType, Precision>::fixed_format_max_chars;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision>
constexpr std::size_t formatting_limits<DecimalType, Precision>::hex_format_max_chars;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision>
constexpr std::size_t formatting_limits<DecimalType, Precision>::cohort_preserving_scientific_max_chars;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, int Precision>
constexpr std::size_t formatting_limits<DecimalType, Precision>::general_format_max_chars;

#endif

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_FORMATTING_LIMITS_HPP
