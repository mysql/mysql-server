// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_ASIN_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_ASIN_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include "../../int128.hpp"
#include <boost/decimal/detail/cmath/impl/remez_series_result.hpp>
#include <boost/decimal/detail/construction_sign.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace asin_detail {

// Use a struct of arrays so that we can have static constexpr arrays of coefficients.
// If we don't do this we will end up constructing the values in the arrays each time the function is called
// which leads to massive slowdowns.
//
// See https://github.com/boostorg/math/issues/923 for further information
template <bool b>
struct asin_table_imp
{
private:
    using d32_coeffs_t   = std::array<decimal32_t,  11>;
    using d64_coeffs_t   = std::array<decimal64_t,  21>;
    using d128_coeffs_t  = std::array<decimal128_t, 41>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  11>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  21>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 41>;

public:

    // 10th degree remez polynomial calculated from 0, 0.5
    // Estimated max error: 7.3651618860008751e-11
    static constexpr d32_coeffs_t d32_coeffs = {{
         decimal32_t {UINT64_C(263887099755925), -15},
         decimal32_t {UINT64_C(43491393212832818), -17, construction_sign::negative},
         decimal32_t {UINT64_C(38559884786102105), -17},
         decimal32_t {UINT64_C(13977130653211101), -17, construction_sign::negative},
         decimal32_t {UINT64_C(54573213517731915), -18},
         decimal32_t {UINT64_C(64851743877986187), -18},
         decimal32_t {UINT64_C(11606701725692841), -19},
         decimal32_t {UINT64_C(16658989049586517), -17},
         decimal32_t {UINT64_C(25906093603686159), -22},
         decimal32_t {UINT64_C(99999996600828589), -17},
         decimal32_t {UINT64_C(73651618860008751), -27}
     }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs = {{
        decimal_fast32_t {UINT64_C(263887099755925), -15},
        decimal_fast32_t {UINT64_C(43491393212832818), -17, construction_sign::negative},
        decimal_fast32_t {UINT64_C(38559884786102105), -17},
        decimal_fast32_t {UINT64_C(13977130653211101), -17, construction_sign::negative},
        decimal_fast32_t {UINT64_C(54573213517731915), -18},
        decimal_fast32_t {UINT64_C(64851743877986187), -18},
        decimal_fast32_t {UINT64_C(11606701725692841), -19},
        decimal_fast32_t {UINT64_C(16658989049586517), -17},
        decimal_fast32_t {UINT64_C(25906093603686159), -22},
        decimal_fast32_t {UINT64_C(99999996600828589), -17},
        decimal_fast32_t {UINT64_C(73651618860008751), -27}
    }};

    // 20th degree remez polynomial calculated from 0, 0.5
    // Estimated max error: 6.0872797932519911178133457751215133e-19
    static constexpr d64_coeffs_t d64_coeffs = {{
        decimal64_t {UINT64_C(2201841632531125594), -18},
        decimal64_t {UINT64_C(9319383818485265142), -18, construction_sign::negative},
        decimal64_t {UINT64_C(1876826158920611297), -17},
        decimal64_t {UINT64_C(2351630530022519158), -17, construction_sign::negative},
        decimal64_t {UINT64_C(2046603318375014621), -17},
        decimal64_t {UINT64_C(1304427904865204196), -17, construction_sign::negative},
        decimal64_t {UINT64_C(6308794339076719731), -18},
        decimal64_t {UINT64_C(2333806156857836980), -18, construction_sign::negative},
        decimal64_t {UINT64_C(6826985955727270693), -19},
        decimal64_t {UINT64_C(1326415745606167277), -19, construction_sign::negative},
        decimal64_t {UINT64_C(2747750823768175476), -20},
        decimal64_t {UINT64_C(2660509753516203115), -20},
        decimal64_t {UINT64_C(3977122944636320545), -22},
        decimal64_t {UINT64_C(4461135938842722307), -20},
        decimal64_t {UINT64_C(1826730778134521645), -24},
        decimal64_t {UINT64_C(7499992533825458566), -20},
        decimal64_t {UINT64_C(2034140780525051207), -27},
        decimal64_t {UINT64_C(1666666666327808185), -19},
        decimal64_t {UINT64_C(2987315928933390856), -31},
        decimal64_t {UINT64_C(9999999999999989542), -19},
        decimal64_t {UINT64_C(6087279793251991118), -37}
    }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs = {{
        decimal_fast64_t {UINT64_C(2201841632531125594), -18},
        decimal_fast64_t {UINT64_C(9319383818485265142), -18, construction_sign::negative},
        decimal_fast64_t {UINT64_C(1876826158920611297), -17},
        decimal_fast64_t {UINT64_C(2351630530022519158), -17, construction_sign::negative},
        decimal_fast64_t {UINT64_C(2046603318375014621), -17},
        decimal_fast64_t {UINT64_C(1304427904865204196), -17, construction_sign::negative},
        decimal_fast64_t {UINT64_C(6308794339076719731), -18},
        decimal_fast64_t {UINT64_C(2333806156857836980), -18, construction_sign::negative},
        decimal_fast64_t {UINT64_C(6826985955727270693), -19},
        decimal_fast64_t {UINT64_C(1326415745606167277), -19, construction_sign::negative},
        decimal_fast64_t {UINT64_C(2747750823768175476), -20},
        decimal_fast64_t {UINT64_C(2660509753516203115), -20},
        decimal_fast64_t {UINT64_C(3977122944636320545), -22},
        decimal_fast64_t {UINT64_C(4461135938842722307), -20},
        decimal_fast64_t {UINT64_C(1826730778134521645), -24},
        decimal_fast64_t {UINT64_C(7499992533825458566), -20},
        decimal_fast64_t {UINT64_C(2034140780525051207), -27},
        decimal_fast64_t {UINT64_C(1666666666327808185), -19},
        decimal_fast64_t {UINT64_C(2987315928933390856), -31},
        decimal_fast64_t {UINT64_C(9999999999999989542), -19},
        decimal_fast64_t {UINT64_C(6087279793251991118), -37}
    }};

    // 40th degree remez polynomial calculated from 0, 0.5
    // Estimated max error: 1.084502473818005718919720519483941e-34
    static constexpr d128_coeffs_t d128_coeffs = {{
        decimal128_t {int128::uint128_t{UINT64_C(236367828732266), UINT64_C(4865873281479238114)}, -31},
        decimal128_t {int128::uint128_t{UINT64_C(218966359248756), UINT64_C(1393338271545593644)}, -30, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(98104038983693), UINT64_C(4819646069944316372)}, -29},
        decimal128_t {int128::uint128_t{UINT64_C(282853615727310), UINT64_C(10104044375051504970)}, -29, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(58930987436658), UINT64_C(3829646337759276014)}, -28},
        decimal128_t {int128::uint128_t{UINT64_C(94467942291578), UINT64_C(14212526794757587650)}, -28, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(121156109355190), UINT64_C(6171523396929956760)}, -28},
        decimal128_t {int128::uint128_t{UINT64_C(127640043209581), UINT64_C(8369619306382995314)}, -28, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(112556984011870), UINT64_C(14401172681696800280)}, -28},
        decimal128_t {int128::uint128_t{UINT64_C(84240716950351), UINT64_C(10152945328926072964)}, -28, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(540724366020485), UINT64_C(8813105586620168570)}, -29},
        decimal128_t {int128::uint128_t{UINT64_C(300054630162323), UINT64_C(4862687399308912842)}, -29, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(144827005285082), UINT64_C(4790810090757542758)}, -29},
        decimal128_t {int128::uint128_t{UINT64_C(61085784025333), UINT64_C(3908625641731373429)}, -29, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(225929173229512), UINT64_C(18404095637827467688)}, -30},
        decimal128_t {int128::uint128_t{UINT64_C(73452862511516), UINT64_C(2655967943189644664)}, -30, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(210254502661653), UINT64_C(14174199201997297032)}, -31},
        decimal128_t {int128::uint128_t{UINT64_C(530269670900176), UINT64_C(3023877239296322874)}, -32, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(117870705400334), UINT64_C(8785618254907029456)}, -32},
        decimal128_t {int128::uint128_t{UINT64_C(230285265351731), UINT64_C(8107756519153341434)}, -33, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(397318429350031), UINT64_C(567549410172969484)}, -34},
        decimal128_t {int128::uint128_t{UINT64_C(54772616787306), UINT64_C(4168475956004989379)}, -34, construction_sign::negative},
        decimal128_t {int128::uint128_t{UINT64_C(79509164538790), UINT64_C(17928590725399689320)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(534376054761824), UINT64_C(1987644731805023176)}, -36},
        decimal128_t {int128::uint128_t{UINT64_C(92204817966183), UINT64_C(17576450582561384882)}, -37},
        decimal128_t {int128::uint128_t{UINT64_C(75623542590285), UINT64_C(990523592779300020)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(59680570668825), UINT64_C(14870623164911255928)}, -39},
        decimal128_t {int128::uint128_t{UINT64_C(94069144841714), UINT64_C(11353995396932754836)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(204081757431333), UINT64_C(1300964680833664202)}, -42},
        decimal128_t {int128::uint128_t{UINT64_C(121279716530202), UINT64_C(3054546075061258708)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(340541736068294), UINT64_C(674620373211314186)}, -45},
        decimal128_t {int128::uint128_t{UINT64_C(164700850853976), UINT64_C(1203142186405381614)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(246590930469756), UINT64_C(6088477928552847004)}, -48},
        decimal128_t {int128::uint128_t{UINT64_C(242009413501228), UINT64_C(3841246034215456962)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(64561634810301), UINT64_C(5259904364587721972)}, -51},
        decimal128_t {int128::uint128_t{UINT64_C(406575814682064), UINT64_C(3001055340328133406)}, -35},
        decimal128_t {int128::uint128_t{UINT64_C(447242814330412), UINT64_C(4234427805033793948)}, -56},
        decimal128_t {int128::uint128_t{UINT64_C(90350181040458), UINT64_C(12964998079628443792)}, -34},
        decimal128_t {int128::uint128_t{UINT64_C(430604756670586), UINT64_C(9888097447655546704)}, -61},
        decimal128_t {int128::uint128_t{UINT64_C(542101086242752), UINT64_C(4003012203950105568)}, -34},
        decimal128_t {int128::uint128_t{UINT64_C(58790996908969), UINT64_C(5250765973560640036)}, -67}
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs = {{
        decimal_fast128_t {int128::uint128_t{UINT64_C(236367828732266), UINT64_C(4865873281479238114)}, -31},
        decimal_fast128_t {int128::uint128_t{UINT64_C(218966359248756), UINT64_C(1393338271545593644)}, -30, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(98104038983693), UINT64_C(4819646069944316372)}, -29},
        decimal_fast128_t {int128::uint128_t{UINT64_C(282853615727310), UINT64_C(10104044375051504970)}, -29, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(58930987436658), UINT64_C(3829646337759276014)}, -28},
        decimal_fast128_t {int128::uint128_t{UINT64_C(94467942291578), UINT64_C(14212526794757587650)}, -28, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(121156109355190), UINT64_C(6171523396929956760)}, -28},
        decimal_fast128_t {int128::uint128_t{UINT64_C(127640043209581), UINT64_C(8369619306382995314)}, -28, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(112556984011870), UINT64_C(14401172681696800280)}, -28},
        decimal_fast128_t {int128::uint128_t{UINT64_C(84240716950351), UINT64_C(10152945328926072964)}, -28, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(540724366020485), UINT64_C(8813105586620168570)}, -29},
        decimal_fast128_t {int128::uint128_t{UINT64_C(300054630162323), UINT64_C(4862687399308912842)}, -29, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(144827005285082), UINT64_C(4790810090757542758)}, -29},
        decimal_fast128_t {int128::uint128_t{UINT64_C(61085784025333), UINT64_C(3908625641731373429)}, -29, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(225929173229512), UINT64_C(18404095637827467688)}, -30},
        decimal_fast128_t {int128::uint128_t{UINT64_C(73452862511516), UINT64_C(2655967943189644664)}, -30, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(210254502661653), UINT64_C(14174199201997297032)}, -31},
        decimal_fast128_t {int128::uint128_t{UINT64_C(530269670900176), UINT64_C(3023877239296322874)}, -32, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(117870705400334), UINT64_C(8785618254907029456)}, -32},
        decimal_fast128_t {int128::uint128_t{UINT64_C(230285265351731), UINT64_C(8107756519153341434)}, -33, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(397318429350031), UINT64_C(567549410172969484)}, -34},
        decimal_fast128_t {int128::uint128_t{UINT64_C(54772616787306), UINT64_C(4168475956004989379)}, -34, construction_sign::negative},
        decimal_fast128_t {int128::uint128_t{UINT64_C(79509164538790), UINT64_C(17928590725399689320)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(534376054761824), UINT64_C(1987644731805023176)}, -36},
        decimal_fast128_t {int128::uint128_t{UINT64_C(92204817966183), UINT64_C(17576450582561384882)}, -37},
        decimal_fast128_t {int128::uint128_t{UINT64_C(75623542590285), UINT64_C(990523592779300020)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(59680570668825), UINT64_C(14870623164911255928)}, -39},
        decimal_fast128_t {int128::uint128_t{UINT64_C(94069144841714), UINT64_C(11353995396932754836)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(204081757431333), UINT64_C(1300964680833664202)}, -42},
        decimal_fast128_t {int128::uint128_t{UINT64_C(121279716530202), UINT64_C(3054546075061258708)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(340541736068294), UINT64_C(674620373211314186)}, -45},
        decimal_fast128_t {int128::uint128_t{UINT64_C(164700850853976), UINT64_C(1203142186405381614)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(246590930469756), UINT64_C(6088477928552847004)}, -48},
        decimal_fast128_t {int128::uint128_t{UINT64_C(242009413501228), UINT64_C(3841246034215456962)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(64561634810301), UINT64_C(5259904364587721972)}, -51},
        decimal_fast128_t {int128::uint128_t{UINT64_C(406575814682064), UINT64_C(3001055340328133406)}, -35},
        decimal_fast128_t {int128::uint128_t{UINT64_C(447242814330412), UINT64_C(4234427805033793948)}, -56},
        decimal_fast128_t {int128::uint128_t{UINT64_C(90350181040458), UINT64_C(12964998079628443792)}, -34},
        decimal_fast128_t {int128::uint128_t{UINT64_C(430604756670586), UINT64_C(9888097447655546704)}, -61},
        decimal_fast128_t {int128::uint128_t{UINT64_C(542101086242752), UINT64_C(4003012203950105568)}, -34},
        decimal_fast128_t {int128::uint128_t{UINT64_C(58790996908969), UINT64_C(5250765973560640036)}, -67}
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename asin_table_imp<b>::d32_coeffs_t asin_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename asin_table_imp<b>::d64_coeffs_t asin_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename asin_table_imp<b>::d128_coeffs_t asin_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename asin_table_imp<b>::d32_fast_coeffs_t asin_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename asin_table_imp<b>::d64_fast_coeffs_t asin_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename asin_table_imp<b>::d128_fast_coeffs_t asin_table_imp<b>::d128_fast_coeffs;

#endif

using asin_table = asin_table_imp<true>;

} //namespace asin_detail

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto asin_series(T x) noexcept;

template <>
constexpr auto asin_series<decimal32_t>(decimal32_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d32_coeffs);
}

template <>
constexpr auto asin_series<decimal_fast32_t>(decimal_fast32_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d32_fast_coeffs);
}

template <>
constexpr auto asin_series<decimal64_t>(decimal64_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d64_coeffs);
}

template <>
constexpr auto asin_series<decimal_fast64_t>(decimal_fast64_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d64_fast_coeffs);
}

template <>
constexpr auto asin_series<decimal128_t>(decimal128_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d128_coeffs);
}

template <>
constexpr auto asin_series<decimal_fast128_t>(decimal_fast128_t x) noexcept
{
    return remez_series_result(x, asin_detail::asin_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_ASIN_IMPL_HPP
