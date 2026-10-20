// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_EXPM1_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_EXPM1_IMPL_HPP

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

namespace expm1_detail {

template <bool b>
struct expm1_table_imp
{
private:
    using d32_coeffs_t  = std::array<decimal32_t,  10>;
    using d64_coeffs_t  = std::array<decimal64_t,  14>;
    using d128_coeffs_t = std::array<decimal128_t, 32>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  10>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  14>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 32>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
        // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
        //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
        //   N[%, 48]
        //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10}, x]

        ::boost::decimal::decimal32_t { UINT64_C(1000000000005449334), - 19 + 1 }, // * x
        ::boost::decimal::decimal32_t { UINT64_C(5000000000003881336), - 19 - 0 }, // * x^2
        ::boost::decimal::decimal32_t { UINT64_C(1666666664242981149), - 19 - 0 }, // * x^3
        ::boost::decimal::decimal32_t { UINT64_C(4166666665026072773), - 19 - 1 }, // * x^4
        ::boost::decimal::decimal32_t { UINT64_C(8333336317448167991), - 19 - 2 }, // * x^5
        ::boost::decimal::decimal32_t { UINT64_C(1388889096793935619), - 19 - 2 }, // * x^6
        ::boost::decimal::decimal32_t { UINT64_C(1983978347911205530), - 19 - 3 }, // * x^7
        ::boost::decimal::decimal32_t { UINT64_C(2480049494648544583), - 19 - 4 }, // * x^8
        ::boost::decimal::decimal32_t { UINT64_C(2787876201220259352), - 19 - 5 }, // * x^9
        ::boost::decimal::decimal32_t { UINT64_C(2780855729673643225), - 19 - 6 }, // * x^10
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
         //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
         //   N[%, 48]
         //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10}, x]

         ::boost::decimal::decimal_fast32_t { UINT64_C(1000000000005449334), - 19 + 1 }, // * x
         ::boost::decimal::decimal_fast32_t { UINT64_C(5000000000003881336), - 19 - 0 }, // * x^2
         ::boost::decimal::decimal_fast32_t { UINT64_C(1666666664242981149), - 19 - 0 }, // * x^3
         ::boost::decimal::decimal_fast32_t { UINT64_C(4166666665026072773), - 19 - 1 }, // * x^4
         ::boost::decimal::decimal_fast32_t { UINT64_C(8333336317448167991), - 19 - 2 }, // * x^5
         ::boost::decimal::decimal_fast32_t { UINT64_C(1388889096793935619), - 19 - 2 }, // * x^6
         ::boost::decimal::decimal_fast32_t { UINT64_C(1983978347911205530), - 19 - 3 }, // * x^7
         ::boost::decimal::decimal_fast32_t { UINT64_C(2480049494648544583), - 19 - 4 }, // * x^8
         ::boost::decimal::decimal_fast32_t { UINT64_C(2787876201220259352), - 19 - 5 }, // * x^9
         ::boost::decimal::decimal_fast32_t { UINT64_C(2780855729673643225), - 19 - 6 }, // * x^10
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
        // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
        //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
        //   N[%, 48]
        //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10, x^11, x^12, x^13, x^14}, x]

        ::boost::decimal::decimal64_t { UINT64_C(1000000000000000003), - 19 +  1 }, // * x
        ::boost::decimal::decimal64_t { UINT64_C(4999999999999999998), - 19 -  0 }, // * x^2
        ::boost::decimal::decimal64_t { UINT64_C(1666666666666664035), - 19 -  0 }, // * x^3
        ::boost::decimal::decimal64_t { UINT64_C(4166666666666666934), - 19 -  1 }, // * x^4
        ::boost::decimal::decimal64_t { UINT64_C(8333333333339521841), - 19 -  2 }, // * x^5
        ::boost::decimal::decimal64_t { UINT64_C(1388888888888953513), - 19 -  2 }, // * x^6
        ::boost::decimal::decimal64_t { UINT64_C(1984126983488689186), - 19 -  3 }, // * x^7
        ::boost::decimal::decimal64_t { UINT64_C(2480158730001499149), - 19 -  4 }, // * x^8
        ::boost::decimal::decimal64_t { UINT64_C(2755732258782898252), - 19 -  5 }, // * x^9
        ::boost::decimal::decimal64_t { UINT64_C(2755732043147979013), - 19 -  6 }, // * x^10
        ::boost::decimal::decimal64_t { UINT64_C(2505116286861719378), - 19 -  7 }, // * x^11
        ::boost::decimal::decimal64_t { UINT64_C(2087632598463662328), - 19 -  8 }, // * x^12
        ::boost::decimal::decimal64_t { UINT64_C(1619385892296180390), - 19 -  9 }, // * x^13
        ::boost::decimal::decimal64_t { UINT64_C(1154399218598221557), - 19 - 10 }  // * x^14
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
         //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
         //   N[%, 48]
         //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10, x^11, x^12, x^13, x^14}, x]

         ::boost::decimal::decimal_fast64_t { UINT64_C(1000000000000000003), - 19 +  1 }, // * x
         ::boost::decimal::decimal_fast64_t { UINT64_C(4999999999999999998), - 19 -  0 }, // * x^2
         ::boost::decimal::decimal_fast64_t { UINT64_C(1666666666666664035), - 19 -  0 }, // * x^3
         ::boost::decimal::decimal_fast64_t { UINT64_C(4166666666666666934), - 19 -  1 }, // * x^4
         ::boost::decimal::decimal_fast64_t { UINT64_C(8333333333339521841), - 19 -  2 }, // * x^5
         ::boost::decimal::decimal_fast64_t { UINT64_C(1388888888888953513), - 19 -  2 }, // * x^6
         ::boost::decimal::decimal_fast64_t { UINT64_C(1984126983488689186), - 19 -  3 }, // * x^7
         ::boost::decimal::decimal_fast64_t { UINT64_C(2480158730001499149), - 19 -  4 }, // * x^8
         ::boost::decimal::decimal_fast64_t { UINT64_C(2755732258782898252), - 19 -  5 }, // * x^9
         ::boost::decimal::decimal_fast64_t { UINT64_C(2755732043147979013), - 19 -  6 }, // * x^10
         ::boost::decimal::decimal_fast64_t { UINT64_C(2505116286861719378), - 19 -  7 }, // * x^11
         ::boost::decimal::decimal_fast64_t { UINT64_C(2087632598463662328), - 19 -  8 }, // * x^12
         ::boost::decimal::decimal_fast64_t { UINT64_C(1619385892296180390), - 19 -  9 }, // * x^13
         ::boost::decimal::decimal_fast64_t { UINT64_C(1154399218598221557), - 19 - 10 }  // * x^14
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
        // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
        //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
        //   N[%, 48]
        //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10, x^11, x^12, x^13, x^14, x^15, x^16, x^17, x^18, x^19, x^20, x^21, x^22, x^23, x^24, x^25, x^26, x^27, x^28, x^29, x^30, x^31, x^32 }, x]
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(54210108624275),  UINT64_C(4089650035136921600)  }, -33 }, // * x
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(271050543121376), UINT64_C(2001506101975056384)  }, -34 }, // * x^2
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * x^3
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * x^4
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -36 }, // * x^5
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(75291817533715),  UINT64_C(10804165069276155440) }, -36 }, // * x^6
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(107559739333879), UINT64_C(7528774067376128516)  }, -37 }, // * x^7
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(134449674167349), UINT64_C(4799281565792772746)  }, -38 }, // * x^8
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -39 }, // * x^9
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -40 }, // * x^10
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(135807751684191), UINT64_C(3170782423392841514)  }, -41 }, // * x^11
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(113173126403492), UINT64_C(11865690723015477068) }, -42 }, // * x^12
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(87056251079609),  UINT64_C(13384395342406416636) }, -43 }, // * x^13
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(62183036485435),  UINT64_C(9560282387433156335)  }, -44 }, // * x^14
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(414553576569570), UINT64_C(2246069003862680020)  }, -46 }, // * x^15
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(259095985355981), UINT64_C(6015479145828949264)  }, -47 }, // * x^16
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(152409403150577), UINT64_C(4623619732418095578)  }, -48 }, // * x^17
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(84671890639209),  UINT64_C(10767230558026320466) }, -49 }, // * x^18
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(445641529680050), UINT64_C(8125595620937745600)  }, -51 }, // * x^19
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(222820764840025), UINT64_C(4062767274683195140)  }, -52 }, // * x^20
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(106105126114297), UINT64_C(13344759429965740488) }, -53 }, // * x^21
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(482296027792262), UINT64_C(7088674266265745598)  }, -55 }, // * x^22
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(209693925127072), UINT64_C(336105452763225878)   }, -56 }, // * x^23
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(87372468802945),  UINT64_C(10013088901203012320) }, -57 }, // * x^24
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(349489875208886), UINT64_C(9445768661182748344)  }, -59 }, // * x^25
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(134419182774415), UINT64_C(9680981560342232810)  }, -60 }, // * x^26
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(497848829278818), UINT64_C(16288994997110182382) }, -62 }, // * x^27
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(177803151475355), UINT64_C(16680206430774781810) }, -63 }, // * x^28
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(61311025561137),  UINT64_C(7837795588749518446)  }, -64 }, // * x^29
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(204371229207757), UINT64_C(18366861741830034248) }, -66 }, // * x^30
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(66162682638108),  UINT64_C(6755035083974089930)  }, -67 }, // * x^31
        ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(206436477688751), UINT64_C(15666750779045089894) }, -69 }, // * x^32
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // Specifically derive a polynomial expansion for Exp[x] - 1 for this work.
        //   Table[{x, Exp[x] - 1}, {x, -Log[2], Log[2], 1/60}]
        //   N[%, 48]
        //   Fit[%, {x, x^2, x^3, x^4, x^5, x^6, x^7, x^8, x^9, x^10, x^11, x^12, x^13, x^14, x^15, x^16, x^17, x^18, x^19, x^20, x^21, x^22, x^23, x^24, x^25, x^26, x^27, x^28, x^29, x^30, x^31, x^32 }, x]
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(54210108624275),  UINT64_C(4089650035136921600)  }, -33 }, // * x
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(271050543121376), UINT64_C(2001506101975056384)  }, -34 }, // * x^2
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * x^3
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * x^4
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -36 }, // * x^5
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(75291817533715),  UINT64_C(10804165069276155440) }, -36 }, // * x^6
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(107559739333879), UINT64_C(7528774067376128516)  }, -37 }, // * x^7
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(134449674167349), UINT64_C(4799281565792772746)  }, -38 }, // * x^8
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -39 }, // * x^9
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(149388526852610), UINT64_C(5332535073103080820)  }, -40 }, // * x^10
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(135807751684191), UINT64_C(3170782423392841514)  }, -41 }, // * x^11
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(113173126403492), UINT64_C(11865690723015477068) }, -42 }, // * x^12
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(87056251079609),  UINT64_C(13384395342406416636) }, -43 }, // * x^13
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(62183036485435),  UINT64_C(9560282387433156335)  }, -44 }, // * x^14
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(414553576569570), UINT64_C(2246069003862680020)  }, -46 }, // * x^15
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(259095985355981), UINT64_C(6015479145828949264)  }, -47 }, // * x^16
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(152409403150577), UINT64_C(4623619732418095578)  }, -48 }, // * x^17
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(84671890639209),  UINT64_C(10767230558026320466) }, -49 }, // * x^18
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(445641529680050), UINT64_C(8125595620937745600)  }, -51 }, // * x^19
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(222820764840025), UINT64_C(4062767274683195140)  }, -52 }, // * x^20
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(106105126114297), UINT64_C(13344759429965740488) }, -53 }, // * x^21
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(482296027792262), UINT64_C(7088674266265745598)  }, -55 }, // * x^22
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(209693925127072), UINT64_C(336105452763225878)   }, -56 }, // * x^23
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(87372468802945),  UINT64_C(10013088901203012320) }, -57 }, // * x^24
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(349489875208886), UINT64_C(9445768661182748344)  }, -59 }, // * x^25
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(134419182774415), UINT64_C(9680981560342232810)  }, -60 }, // * x^26
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(497848829278818), UINT64_C(16288994997110182382) }, -62 }, // * x^27
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(177803151475355), UINT64_C(16680206430774781810) }, -63 }, // * x^28
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(61311025561137),  UINT64_C(7837795588749518446)  }, -64 }, // * x^29
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(204371229207757), UINT64_C(18366861741830034248) }, -66 }, // * x^30
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(66162682638108),  UINT64_C(6755035083974089930)  }, -67 }, // * x^31
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(206436477688751), UINT64_C(15666750779045089894) }, -69 }, // * x^32
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename expm1_table_imp<b>::d32_coeffs_t expm1_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename expm1_table_imp<b>::d64_coeffs_t expm1_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename expm1_table_imp<b>::d128_coeffs_t expm1_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename expm1_table_imp<b>::d32_fast_coeffs_t expm1_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename expm1_table_imp<b>::d64_fast_coeffs_t expm1_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename expm1_table_imp<b>::d128_fast_coeffs_t expm1_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace expm1_detail

using expm1_table = expm1_detail::expm1_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto expm1_series_expansion(T x) noexcept;

template <>
constexpr auto expm1_series_expansion<decimal32_t>(decimal32_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d32_coeffs);
}

template <>
constexpr auto expm1_series_expansion<decimal_fast32_t>(decimal_fast32_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d32_fast_coeffs);
}

template <>
constexpr auto expm1_series_expansion<decimal64_t>(decimal64_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d64_coeffs);
}

template <>
constexpr auto expm1_series_expansion<decimal_fast64_t>(decimal_fast64_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d64_fast_coeffs);
}

template <>
constexpr auto expm1_series_expansion<decimal128_t>(decimal128_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d128_coeffs);
}

template <>
constexpr auto expm1_series_expansion<decimal_fast128_t>(decimal_fast128_t x) noexcept
{
    return taylor_series_result(x, expm1_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_EXPM1_IMPL_HPP
