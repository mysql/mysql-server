// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG1P_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG1P_IMPL_HPP

#include <boost/decimal/fwd.hpp>
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

namespace log1p_detail {

template <bool b>
struct log1p_table_imp
{
private:
    using d32_coeffs_t  = std::array<decimal32_t,  12>;
    using d64_coeffs_t  = std::array<decimal64_t,  20>;
    using d128_coeffs_t = std::array<decimal128_t, 36>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  12>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  20>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 36>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
         // Series[Log[1 + x], {x, 0, 13}]
         //            (1),                                                     // * z
         -boost::decimal::decimal32_t { 5, -1 },                                  // * z^2
          boost::decimal::decimal32_t { UINT64_C(3333333333333333333), -19 },     // * z^3
         -boost::decimal::decimal32_t { 25, -2 },                                 // * z^4
          boost::decimal::decimal32_t { 2, -1 },                                  // * z^5
         -boost::decimal::decimal32_t { UINT64_C(1666666666666666667), -19 },     // * z^6
          boost::decimal::decimal32_t { UINT64_C(1428571428571428571), -19 },     // * z^7
         -boost::decimal::decimal32_t { 125, -3 },                                // * z^8
          boost::decimal::decimal32_t { UINT64_C(1111111111111111111), -19 },     // * z^9
         -boost::decimal::decimal32_t { 1, -1 },                                  // * z^10
          boost::decimal::decimal32_t { UINT64_C(9090909090909090909), -19 - 1 }, // * z^11
         -boost::decimal::decimal32_t { UINT64_C(8333333333333333333), -19 - 1 }, // * z^12
          boost::decimal::decimal32_t { UINT64_C(7692307692307692308), -19 - 1 }, // * z^13
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Series[Log[1 + x], {x, 0, 13}]
         //            (1),                                                     // * z
         -boost::decimal::decimal_fast32_t { 5, -1 },                                  // * z^2
          boost::decimal::decimal_fast32_t { UINT64_C(3333333333333333333), -19 },     // * z^3
         -boost::decimal::decimal_fast32_t { 25, -2 },                                 // * z^4
          boost::decimal::decimal_fast32_t { 2, -1 },                                  // * z^5
         -boost::decimal::decimal_fast32_t { UINT64_C(1666666666666666667), -19 },     // * z^6
          boost::decimal::decimal_fast32_t { UINT64_C(1428571428571428571), -19 },     // * z^7
         -boost::decimal::decimal_fast32_t { 125, -3 },                                // * z^8
          boost::decimal::decimal_fast32_t { UINT64_C(1111111111111111111), -19 },     // * z^9
         -boost::decimal::decimal_fast32_t { 1, -1 },                                  // * z^10
          boost::decimal::decimal_fast32_t { UINT64_C(9090909090909090909), -19 - 1 }, // * z^11
         -boost::decimal::decimal_fast32_t { UINT64_C(8333333333333333333), -19 - 1 }, // * z^12
          boost::decimal::decimal_fast32_t { UINT64_C(7692307692307692308), -19 - 1 }, // * z^13
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // Series[Log[1 + x], {x, 0, 21}]
         //            (1),                                                     // * z
         -boost::decimal::decimal64_t { 5, -1 },                                  // * z^2
          boost::decimal::decimal64_t { UINT64_C(3333333333333333333), -19 },     // * z^3
         -boost::decimal::decimal64_t { 25, -2 },                                 // * z^4
          boost::decimal::decimal64_t { 2, -1 },                                  // * z^5
         -boost::decimal::decimal64_t { UINT64_C(1666666666666666667), -19 },     // * z^6
          boost::decimal::decimal64_t { UINT64_C(1428571428571428571), -19 },     // * z^7
         -boost::decimal::decimal64_t { 125, -3 },                                // * z^8
          boost::decimal::decimal64_t { UINT64_C(1111111111111111111), -19 },     // * z^9
         -boost::decimal::decimal64_t { 1, -1 },                                  // * z^10
          boost::decimal::decimal64_t { UINT64_C(9090909090909090909), -19 - 1 }, // * z^11
         -boost::decimal::decimal64_t { UINT64_C(8333333333333333333), -19 - 1 }, // * z^12
          boost::decimal::decimal64_t { UINT64_C(7692307692307692308), -19 - 1 }, // * z^13
         -boost::decimal::decimal64_t { UINT64_C(7142857142857142857), -19 - 1 }, // * z^14
          boost::decimal::decimal64_t { UINT64_C(6666666666666666667), -19 - 1 }, // * z^15
         -boost::decimal::decimal64_t { UINT64_C(6250000000000000000), -19 - 1 }, // * z^16
          boost::decimal::decimal64_t { UINT64_C(5882352941176470588), -19 - 1 }, // * z^17
         -boost::decimal::decimal64_t { UINT64_C(5555555555555555556), -19 - 1 }, // * z^18
          boost::decimal::decimal64_t { UINT64_C(5263157894736842105), -19 - 1 }, // * z^19
         -boost::decimal::decimal64_t { 5, -2 },                                  // * z^20
          boost::decimal::decimal64_t { UINT64_C(4761904761904761905), -19 - 1 }, // * z^21
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Series[Log[1 + x], {x, 0, 21}]
         //            (1),                                                     // * z
         -boost::decimal::decimal_fast64_t { 5, -1 },                                  // * z^2
          boost::decimal::decimal_fast64_t { UINT64_C(3333333333333333333), -19 },     // * z^3
         -boost::decimal::decimal_fast64_t { 25, -2 },                                 // * z^4
          boost::decimal::decimal_fast64_t { 2, -1 },                                  // * z^5
         -boost::decimal::decimal_fast64_t { UINT64_C(1666666666666666667), -19 },     // * z^6
          boost::decimal::decimal_fast64_t { UINT64_C(1428571428571428571), -19 },     // * z^7
         -boost::decimal::decimal_fast64_t { 125, -3 },                                // * z^8
          boost::decimal::decimal_fast64_t { UINT64_C(1111111111111111111), -19 },     // * z^9
         -boost::decimal::decimal_fast64_t { 1, -1 },                                  // * z^10
          boost::decimal::decimal_fast64_t { UINT64_C(9090909090909090909), -19 - 1 }, // * z^11
         -boost::decimal::decimal_fast64_t { UINT64_C(8333333333333333333), -19 - 1 }, // * z^12
          boost::decimal::decimal_fast64_t { UINT64_C(7692307692307692308), -19 - 1 }, // * z^13
         -boost::decimal::decimal_fast64_t { UINT64_C(7142857142857142857), -19 - 1 }, // * z^14
          boost::decimal::decimal_fast64_t { UINT64_C(6666666666666666667), -19 - 1 }, // * z^15
         -boost::decimal::decimal_fast64_t { UINT64_C(6250000000000000000), -19 - 1 }, // * z^16
          boost::decimal::decimal_fast64_t { UINT64_C(5882352941176470588), -19 - 1 }, // * z^17
         -boost::decimal::decimal_fast64_t { UINT64_C(5555555555555555556), -19 - 1 }, // * z^18
          boost::decimal::decimal_fast64_t { UINT64_C(5263157894736842105), -19 - 1 }, // * z^19
         -boost::decimal::decimal_fast64_t { 5, -2 },                                  // * z^20
          boost::decimal::decimal_fast64_t { UINT64_C(4761904761904761905), -19 - 1 }, // * z^21
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 43}]
         //            (1),                                                                                                                    // * z
         -::boost::decimal::decimal128_t { 5, -1 },                                                                                              // * z^2
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -34 }, // * z^3
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(135525271560688), UINT64_C(1000753050987528192)  }, -34 }, // * z^4
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(108420217248550), UINT64_C(8179300070273843200)  }, -34 }, // * z^5
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * z^6
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(77443012320393),  UINT64_C(3207108039665666332)  }, -34 }, // * z^7
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(67762635780344),  UINT64_C(500376525493764096)   }, -34 }, // * z^8
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(60233454026972),  UINT64_C(8643332055420924359)  }, -34 }, // * z^9
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(54210108624275),  UINT64_C(4089650035136921600)  }, -34 }, // * z^10
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(492819169311592), UINT64_C(17054915875379776418) }, -35 }, // * z^11
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -35 }, // * z^12
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(417000835571347), UINT64_C(15850062977145160938) }, -35 }, // * z^13
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(387215061601965), UINT64_C(16035540198328331700) }, -35 }, // * z^14
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(361400724161834), UINT64_C(14966504185106442916) }, -35 }, // * z^15
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(338813178901720), UINT64_C(2501882627468820480)  }, -35 }, // * z^16
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(318882991907501), UINT64_C(5610020838860575434)  }, -35 }, // * z^17
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(301167270134862), UINT64_C(6323172129685518558)  }, -35 }, // * z^18
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(285316361180395), UINT64_C(16670067533954968520) }, -35 }, // * z^19
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(271050543121376), UINT64_C(2001506101975056384)  }, -35 }, // * z^20
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(258143374401310), UINT64_C(10690360132218887800) }, -35 }, // * z^21
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(246409584655796), UINT64_C(8527457937689888204)  }, -35 }, // * z^22
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(235696124453370), UINT64_C(9760763598982462770)  }, -35 }, // * z^23
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * z^24
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(216840434497100), UINT64_C(16358600140547686400) }, -35 }, // * z^25
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(208500417785673), UINT64_C(17148403525427356272) }, -35 }, // * z^26
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(200778180089908), UINT64_C(4215448086457012372)  }, -35 }, // * z^27
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(193607530800982), UINT64_C(17241142136018941658) }, -35 }, // * z^28
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(186931409049224), UINT64_C(16646619993397598836) }, -35 }, // * z^29
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -35 }, // * z^30
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(174871318142823), UINT64_C(5456688082434451252)  }, -35 }, // * z^31
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(169406589450860), UINT64_C(1250941313734410240)  }, -35 }, // * z^32
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(164273056437197), UINT64_C(11833886649696442678) }, -35 }, // * z^33
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(159441495953750), UINT64_C(12028382456285063520) }, -35 }, // * z^34
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(154886024640786), UINT64_C(6414216079331332674)  }, -35 }, // * z^35
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(150583635067431), UINT64_C(3161586064842759274)  }, -35 }, // * z^36
          ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(146513807092635), UINT64_C(13545911456276754540) }, -35 }, // * z^37
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 43}]
        //            (1),                                                                                                                    // * z
        -::boost::decimal::decimal_fast128_t { 5, -1 },                                                                                              // * z^2
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -34 }, // * z^3
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(135525271560688), UINT64_C(1000753050987528192)  }, -34 }, // * z^4
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(108420217248550), UINT64_C(8179300070273843200)  }, -34 }, // * z^5
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(90350181040458),  UINT64_C(12964998083131386532) }, -34 }, // * z^6
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(77443012320393),  UINT64_C(3207108039665666332)  }, -34 }, // * z^7
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(67762635780344),  UINT64_C(500376525493764096)   }, -34 }, // * z^8
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(60233454026972),  UINT64_C(8643332055420924359)  }, -34 }, // * z^9
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(54210108624275),  UINT64_C(4089650035136921600)  }, -34 }, // * z^10
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(492819169311592), UINT64_C(17054915875379776418) }, -35 }, // * z^11
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -35 }, // * z^12
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(417000835571347), UINT64_C(15850062977145160938) }, -35 }, // * z^13
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(387215061601965), UINT64_C(16035540198328331700) }, -35 }, // * z^14
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(361400724161834), UINT64_C(14966504185106442916) }, -35 }, // * z^15
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(338813178901720), UINT64_C(2501882627468820480)  }, -35 }, // * z^16
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(318882991907501), UINT64_C(5610020838860575434)  }, -35 }, // * z^17
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(301167270134862), UINT64_C(6323172129685518558)  }, -35 }, // * z^18
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(285316361180395), UINT64_C(16670067533954968520) }, -35 }, // * z^19
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(271050543121376), UINT64_C(2001506101975056384)  }, -35 }, // * z^20
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(258143374401310), UINT64_C(10690360132218887800) }, -35 }, // * z^21
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(246409584655796), UINT64_C(8527457937689888204)  }, -35 }, // * z^22
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(235696124453370), UINT64_C(9760763598982462770)  }, -35 }, // * z^23
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(225875452601146), UINT64_C(13965751134118914724) }, -35 }, // * z^24
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(216840434497100), UINT64_C(16358600140547686400) }, -35 }, // * z^25
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(208500417785673), UINT64_C(17148403525427356272) }, -35 }, // * z^26
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(200778180089908), UINT64_C(4215448086457012372)  }, -35 }, // * z^27
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(193607530800982), UINT64_C(17241142136018941658) }, -35 }, // * z^28
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(186931409049224), UINT64_C(16646619993397598836) }, -35 }, // * z^29
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -35 }, // * z^30
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(174871318142823), UINT64_C(5456688082434451252)  }, -35 }, // * z^31
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(169406589450860), UINT64_C(1250941313734410240)  }, -35 }, // * z^32
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(164273056437197), UINT64_C(11833886649696442678) }, -35 }, // * z^33
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(159441495953750), UINT64_C(12028382456285063520) }, -35 }, // * z^34
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(154886024640786), UINT64_C(6414216079331332674)  }, -35 }, // * z^35
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(150583635067431), UINT64_C(3161586064842759274)  }, -35 }, // * z^36
         ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(146513807092635), UINT64_C(13545911456276754540) }, -35 }, // * z^37
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename log1p_table_imp<b>::d32_coeffs_t log1p_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename log1p_table_imp<b>::d64_coeffs_t log1p_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename log1p_table_imp<b>::d128_coeffs_t log1p_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename log1p_table_imp<b>::d32_fast_coeffs_t log1p_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename log1p_table_imp<b>::d64_fast_coeffs_t log1p_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename log1p_table_imp<b>::d128_fast_coeffs_t log1p_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace log1p_detail

using log1p_table = log1p_detail::log1p_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto log1p_series_expansion(T z2) noexcept;

template <>
constexpr auto log1p_series_expansion<decimal32_t>(decimal32_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d32_coeffs);
}

template <>
constexpr auto log1p_series_expansion<decimal_fast32_t>(decimal_fast32_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d32_fast_coeffs);
}

template <>
constexpr auto log1p_series_expansion<decimal64_t>(decimal64_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d64_coeffs);
}

template <>
constexpr auto log1p_series_expansion<decimal_fast64_t>(decimal_fast64_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d64_fast_coeffs);
}

template <>
constexpr auto log1p_series_expansion<decimal128_t>(decimal128_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d128_coeffs);
}

template <>
constexpr auto log1p_series_expansion<decimal_fast128_t>(decimal_fast128_t z2) noexcept
{
    return taylor_series_result(z2, log1p_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG1P_IMPL_HPP
