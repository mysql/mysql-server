// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_COSH_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_COSH_IMPL_HPP

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

namespace cosh_detail {

template <bool b>
struct cosh_table_imp
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
         // Series[Cosh[x], {x, 0, 12}]
         //            (1),                                                        // * 1
         ::boost::decimal::decimal32_t { 5, -1 },                                    // * x^2
         ::boost::decimal::decimal32_t { UINT64_C(4166666666666666667), - 19 -  1 }, // * x^6
         ::boost::decimal::decimal32_t { UINT64_C(1388888888888888889), - 19 -  2 }, // * x^8
         ::boost::decimal::decimal32_t { UINT64_C(2480158730158730159), - 19 -  4 }, // * x^10
         ::boost::decimal::decimal32_t { UINT64_C(2755731922398589065), - 19 -  6 }, // * x^12
         ::boost::decimal::decimal32_t { UINT64_C(2087675698786809898), - 19 -  8 }, // * x^12
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Series[Cosh[x], {x, 0, 12}]
         //            (1),                                                        // * 1
         ::boost::decimal::decimal_fast32_t { 5, -1 },                                    // * x^2
         ::boost::decimal::decimal_fast32_t { UINT64_C(4166666666666666667), - 19 -  1 }, // * x^6
         ::boost::decimal::decimal_fast32_t { UINT64_C(1388888888888888889), - 19 -  2 }, // * x^8
         ::boost::decimal::decimal_fast32_t { UINT64_C(2480158730158730159), - 19 -  4 }, // * x^10
         ::boost::decimal::decimal_fast32_t { UINT64_C(2755731922398589065), - 19 -  6 }, // * x^12
         ::boost::decimal::decimal_fast32_t { UINT64_C(2087675698786809898), - 19 -  8 }, // * x^12
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // Series[Cosh[x], {x, 0, 18}]
         //            (1),                                                        // * 1
         ::boost::decimal::decimal64_t { 5, -1 },                                    // * x^2
         ::boost::decimal::decimal64_t { UINT64_C(4166666666666666667), - 19 -  1 }, // * x^4
         ::boost::decimal::decimal64_t { UINT64_C(1388888888888888889), - 19 -  2 }, // * x^6
         ::boost::decimal::decimal64_t { UINT64_C(2480158730158730159), - 19 -  4 }, // * x^8
         ::boost::decimal::decimal64_t { UINT64_C(2755731922398589065), - 19 -  6 }, // * x^10
         ::boost::decimal::decimal64_t { UINT64_C(2087675698786809898), - 19 -  8 }, // * x^12
         ::boost::decimal::decimal64_t { UINT64_C(1147074559772972471), - 19 - 10 }, // * x^14
         ::boost::decimal::decimal64_t { UINT64_C(4779477332387385297), - 19 - 13 }, // * x^16
         ::boost::decimal::decimal64_t { UINT64_C(1561920696858622646), - 19 - 15 }  // * x^18
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Series[Cosh[x], {x, 0, 18}]
         //            (1),                                                             // * 1
         ::boost::decimal::decimal_fast64_t { 5, -1 },                                    // * x^2
         ::boost::decimal::decimal_fast64_t { UINT64_C(4166666666666666667), - 19 -  1 }, // * x^4
         ::boost::decimal::decimal_fast64_t { UINT64_C(1388888888888888889), - 19 -  2 }, // * x^6
         ::boost::decimal::decimal_fast64_t { UINT64_C(2480158730158730159), - 19 -  4 }, // * x^8
         ::boost::decimal::decimal_fast64_t { UINT64_C(2755731922398589065), - 19 -  6 }, // * x^10
         ::boost::decimal::decimal_fast64_t { UINT64_C(2087675698786809898), - 19 -  8 }, // * x^12
         ::boost::decimal::decimal_fast64_t { UINT64_C(1147074559772972471), - 19 - 10 }, // * x^14
         ::boost::decimal::decimal_fast64_t { UINT64_C(4779477332387385297), - 19 - 13 }, // * x^16
         ::boost::decimal::decimal_fast64_t { UINT64_C(1561920696858622646), - 19 - 15 }  // * x^18
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
         // Series[Cosh[x], {x, 0, 34}]
         //            (1),                                                                                                                   // * 1
         ::boost::decimal::decimal128_t { 5, -1 },                                                                                              // * x^2
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * x^4
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(75291817533715),  UINT64_C(10804165069276155440) }, -36 }, // * x^6
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(134449674167349), UINT64_C(4799281565792772746)  }, -38 }, // * x^8
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -40 }, // * x^10
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(113173126403492), UINT64_C(11865690723015477068) }, -42 }, // * x^12
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(62183036485435),  UINT64_C(9560282387433155251)  }, -44 }, // * x^14
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(259095985355981), UINT64_C(6015479145837302244)  }, -47 }, // * x^16
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(84671890639209),  UINT64_C(10767230553416093986) }, -49 }, // * x^18
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(222820764840025), UINT64_C(4062785569898205740)  }, -52 }, // * x^20
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(482296027792262), UINT64_C(7037075391028107068)  }, -55 }, // * x^22
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(87372468802946),  UINT64_C(1542176615384940434)  }, -57 }, // * x^24
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(134419182773763), UINT64_C(3791559721646796942)  }, -60 }, // * x^26
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(177803151817147), UINT64_C(1794430560736952558)  }, -63 }, // * x^28
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(204371438870284), UINT64_C(366311534299067156)   }, -66 }, // * x^30
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(206019595635366), UINT64_C(17625897212400736954) }, -69 }, // * x^32
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(183618177928134), UINT64_C(9987905770721758456)  }, -72 }, // * x^34
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // Series[Cosh[x], {x, 0, 34}]
        //            (1),                                                                                                                   // * 1
        ::boost::decimal::decimal_fast128_t { 5, -1 },                                                                                              // * x^2
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * x^4
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(75291817533715),  UINT64_C(10804165069276155440) }, -36 }, // * x^6
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(134449674167349), UINT64_C(4799281565792772746)  }, -38 }, // * x^8
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -40 }, // * x^10
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(113173126403492), UINT64_C(11865690723015477068) }, -42 }, // * x^12
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(62183036485435),  UINT64_C(9560282387433155251)  }, -44 }, // * x^14
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(259095985355981), UINT64_C(6015479145837302244)  }, -47 }, // * x^16
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(84671890639209),  UINT64_C(10767230553416093986) }, -49 }, // * x^18
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(222820764840025), UINT64_C(4062785569898205740)  }, -52 }, // * x^20
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(482296027792262), UINT64_C(7037075391028107068)  }, -55 }, // * x^22
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(87372468802946),  UINT64_C(1542176615384940434)  }, -57 }, // * x^24
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(134419182773763), UINT64_C(3791559721646796942)  }, -60 }, // * x^26
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(177803151817147), UINT64_C(1794430560736952558)  }, -63 }, // * x^28
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(204371438870284), UINT64_C(366311534299067156)   }, -66 }, // * x^30
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(206019595635366), UINT64_C(17625897212400736954) }, -69 }, // * x^32
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(183618177928134), UINT64_C(9987905770721758456)  }, -72 }, // * x^34
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename cosh_table_imp<b>::d32_coeffs_t cosh_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename cosh_table_imp<b>::d64_coeffs_t cosh_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename cosh_table_imp<b>::d128_coeffs_t cosh_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename cosh_table_imp<b>::d32_fast_coeffs_t cosh_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename cosh_table_imp<b>::d64_fast_coeffs_t cosh_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename cosh_table_imp<b>::d128_fast_coeffs_t cosh_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace cosh_detail

using cosh_table = cosh_detail::cosh_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto cosh_series_expansion(T z2) noexcept;

template <>
constexpr auto cosh_series_expansion<decimal32_t>(decimal32_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d32_coeffs);
}

template <>
constexpr auto cosh_series_expansion<decimal64_t>(decimal64_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d64_coeffs);
}

template <>
constexpr auto cosh_series_expansion<decimal128_t>(decimal128_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d128_coeffs);
}

template <>
constexpr auto cosh_series_expansion<decimal_fast32_t>(decimal_fast32_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d32_fast_coeffs);
}

template <>
constexpr auto cosh_series_expansion<decimal_fast64_t>(decimal_fast64_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d64_fast_coeffs);
}

template <>
constexpr auto cosh_series_expansion<decimal_fast128_t>(decimal_fast128_t z2) noexcept
{
    return taylor_series_result(z2, cosh_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_COSH_IMPL_HPP
