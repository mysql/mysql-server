// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_SINH_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_SINH_IMPL_HPP

#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/cmath/impl/taylor_series_result.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <cstddef>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace sinh_detail {

template <bool b>
struct sinh_table_imp
{
private:
    using d32_coeffs_t  = std::array<decimal32_t,   6>;
    using d64_coeffs_t  = std::array<decimal64_t,   9>;
    using d128_coeffs_t = std::array<decimal128_t, 17>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  6>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  9>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 17>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 13}]
         //            (1),                                                        // * x
         ::boost::decimal::decimal32_t { UINT64_C(1666666666666666667), - 19 -  0 }, // * x^3
         ::boost::decimal::decimal32_t { UINT64_C(8333333333333333333), - 19 -  2 }, // * x^5
         ::boost::decimal::decimal32_t { UINT64_C(1984126984126984127), - 19 -  3 }, // * x^7
         ::boost::decimal::decimal32_t { UINT64_C(2755731922398589065), - 19 -  5 }, // * x^9
         ::boost::decimal::decimal32_t { UINT64_C(2505210838544171878), - 19 -  7 }, // * x^11
         ::boost::decimal::decimal32_t { UINT64_C(1605904383682161460), - 19 -  9 }, // * x^13
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 13}]
         //            (1),                                                        // * x
         ::boost::decimal::decimal_fast32_t { UINT64_C(1666666666666666667), - 19 -  0 }, // * x^3
         ::boost::decimal::decimal_fast32_t { UINT64_C(8333333333333333333), - 19 -  2 }, // * x^5
         ::boost::decimal::decimal_fast32_t { UINT64_C(1984126984126984127), - 19 -  3 }, // * x^7
         ::boost::decimal::decimal_fast32_t { UINT64_C(2755731922398589065), - 19 -  5 }, // * x^9
         ::boost::decimal::decimal_fast32_t { UINT64_C(2505210838544171878), - 19 -  7 }, // * x^11
         ::boost::decimal::decimal_fast32_t { UINT64_C(1605904383682161460), - 19 -  9 }, // * x^13
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 19}]
         //            (1),                                                        // * x
         ::boost::decimal::decimal64_t { UINT64_C(1666666666666666667), - 19 -  0 }, // * x^3
         ::boost::decimal::decimal64_t { UINT64_C(8333333333333333333), - 19 -  2 }, // * x^5
         ::boost::decimal::decimal64_t { UINT64_C(1984126984126984127), - 19 -  3 }, // * x^7
         ::boost::decimal::decimal64_t { UINT64_C(2755731922398589065), - 19 -  5 }, // * x^9
         ::boost::decimal::decimal64_t { UINT64_C(2505210838544171878), - 19 -  7 }, // * x^11
         ::boost::decimal::decimal64_t { UINT64_C(1605904383682161460), - 19 -  9 }, // * x^13
         ::boost::decimal::decimal64_t { UINT64_C(7647163731819816476), - 19 - 12 }, // * x^15
         ::boost::decimal::decimal64_t { UINT64_C(2811457254345520763), - 19 - 14 }, // * x^17
         ::boost::decimal::decimal64_t { UINT64_C(8220635246624329717), - 19 - 17 }  // * x^19
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 19}]
         //            (1),                                                        // * x
         ::boost::decimal::decimal_fast64_t { UINT64_C(1666666666666666667), - 19 -  0 }, // * x^3
         ::boost::decimal::decimal_fast64_t { UINT64_C(8333333333333333333), - 19 -  2 }, // * x^5
         ::boost::decimal::decimal_fast64_t { UINT64_C(1984126984126984127), - 19 -  3 }, // * x^7
         ::boost::decimal::decimal_fast64_t { UINT64_C(2755731922398589065), - 19 -  5 }, // * x^9
         ::boost::decimal::decimal_fast64_t { UINT64_C(2505210838544171878), - 19 -  7 }, // * x^11
         ::boost::decimal::decimal_fast64_t { UINT64_C(1605904383682161460), - 19 -  9 }, // * x^13
         ::boost::decimal::decimal_fast64_t { UINT64_C(7647163731819816476), - 19 - 12 }, // * x^15
         ::boost::decimal::decimal_fast64_t { UINT64_C(2811457254345520763), - 19 - 14 }, // * x^17
         ::boost::decimal::decimal_fast64_t { UINT64_C(8220635246624329717), - 19 - 17 }  // * x^19
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 34}]
         //            (1),                                                                                                                   // * x
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * x^3
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -36 }, // * x^5
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(107559739333879), UINT64_C(7528774067376128516)  }, -37 }, // * x^7
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -39 }, // * x^9
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(135807751684191), UINT64_C(3170782423392841514)  }, -41 }, // * x^11
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(87056251079609),  UINT64_C(13384395342406417346) }, -43 }, // * x^13
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(414553576569570), UINT64_C(2246069003855862950)  }, -46 }, // * x^15
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(152409403150577), UINT64_C(4623619737181327888)  }, -48 }, // * x^17
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(445641529680050), UINT64_C(8125571139796411480)  }, -51 }, // * x^19
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(106105126114297), UINT64_C(13354072793200296588) }, -53 }, // * x^21
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(209693925127070), UINT64_C(11079921506407677690) }, -56 }, // * x^23
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(349489875211784), UINT64_C(6168706461539761736)  }, -59 }, // * x^25
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(497848825088011), UINT64_C(16092452014289198134) }, -62 }, // * x^27
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(61311431661085),  UINT64_C(3799242275031630472)  }, -64 }, // * x^29
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(65926270603317),  UINT64_C(7853896396813382020)  }, -67 }, // * x^31
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(62430180495565),  UINT64_C(13726064643322746782) }, -70 }, // * x^33
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(524623365508955), UINT64_C(15360627863698201590) }, -74 }, // * x^35
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
         // Series[Sinh[x], {x, 0, 34}]
         //            (1),                                                                                                                   // * x
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * x^3
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -36 }, // * x^5
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(107559739333879), UINT64_C(7528774067376128516)  }, -37 }, // * x^7
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -39 }, // * x^9
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(135807751684191), UINT64_C(3170782423392841514)  }, -41 }, // * x^11
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(87056251079609),  UINT64_C(13384395342406417346) }, -43 }, // * x^13
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(414553576569570), UINT64_C(2246069003855862950)  }, -46 }, // * x^15
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(152409403150577), UINT64_C(4623619737181327888)  }, -48 }, // * x^17
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(445641529680050), UINT64_C(8125571139796411480)  }, -51 }, // * x^19
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(106105126114297), UINT64_C(13354072793200296588) }, -53 }, // * x^21
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(209693925127070), UINT64_C(11079921506407677690) }, -56 }, // * x^23
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(349489875211784), UINT64_C(6168706461539761736)  }, -59 }, // * x^25
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(497848825088011), UINT64_C(16092452014289198134) }, -62 }, // * x^27
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(61311431661085),  UINT64_C(3799242275031630472)  }, -64 }, // * x^29
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(65926270603317),  UINT64_C(7853896396813382020)  }, -67 }, // * x^31
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(62430180495565),  UINT64_C(13726064643322746782) }, -70 }, // * x^33
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(524623365508955), UINT64_C(15360627863698201590) }, -74 }, // * x^35
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename sinh_table_imp<b>::d32_coeffs_t sinh_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename sinh_table_imp<b>::d64_coeffs_t sinh_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename sinh_table_imp<b>::d128_coeffs_t sinh_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename sinh_table_imp<b>::d32_fast_coeffs_t sinh_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename sinh_table_imp<b>::d64_fast_coeffs_t sinh_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename sinh_table_imp<b>::d128_fast_coeffs_t sinh_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace sinh_detail

using sinh_table = sinh_detail::sinh_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto sinh_series_expansion(T z2) noexcept;

template <>
constexpr auto sinh_series_expansion<decimal32_t>(decimal32_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d32_coeffs);
}

template <>
constexpr auto sinh_series_expansion<decimal_fast32_t>(decimal_fast32_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d32_fast_coeffs);
}

template <>
constexpr auto sinh_series_expansion<decimal64_t>(decimal64_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d64_coeffs);
}

template <>
constexpr auto sinh_series_expansion<decimal_fast64_t>(decimal_fast64_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d64_fast_coeffs);
}

template <>
constexpr auto sinh_series_expansion<decimal128_t>(decimal128_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d128_coeffs);
}

template <>
constexpr auto sinh_series_expansion<decimal_fast128_t>(decimal_fast128_t z2) noexcept
{
    return taylor_series_result(z2, sinh_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_SINH_IMPL_HPP
