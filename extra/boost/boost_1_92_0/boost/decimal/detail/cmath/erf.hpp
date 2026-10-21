// Copyright John Maddock 2006.
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ERF_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ERF_HPP

#include <boost/decimal/detail/cmath/impl/kahan_sum.hpp>
#include <boost/decimal/detail/cmath/impl/evaluate_polynomial.hpp>
#include <boost/decimal/detail/cmath/exp.hpp>
#include <boost/decimal/detail/cmath/fabs.hpp>
#include <boost/decimal/detail/cmath/frexp.hpp>
#include <boost/decimal/detail/cmath/ldexp.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/numbers.hpp>
#include <boost/decimal/decimal128_t.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <array>
#endif

namespace boost {
namespace decimal {

namespace detail {

//
// Asymptotic series for large z:
//
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
class erf_asympt_series_t
{
private:
    T result;
    T xx;
    int tk {1};

public:
    using result_type = T;

    explicit constexpr erf_asympt_series_t(T z)
    {
        const auto neg_z_squared {-z * z};
        result = -exp(neg_z_squared) / sqrt(numbers::pi_v<T>);
        result /= z;
        xx = neg_z_squared * 2;
    }

    constexpr auto operator()() noexcept -> T
    {
        auto r {result};
        result *= tk / xx;
        tk += 2;
        if (fabs(r) < fabs(result))
        {
            result = 0;
        }

        return r;
    }

};

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
class erf_series_near_zero
{
private:
    T term;
    T zz;
    int k {};

public:
    using result_type = T;

    explicit constexpr erf_series_near_zero(const T &z) : term {z}, zz {-z * z} {}

    constexpr auto operator()() noexcept -> T
    {
        auto result {term / (2 * k + 1)};
        term *= zz / ++k;
        return result;
    }
};

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto erf_series_near_zero_sum(const T &x) noexcept -> T
{
    //
    // We need Kahan summation here, otherwise the errors grow fairly quickly.
    // This method is *much* faster than the alternatives even so.
    //
    constexpr T two_div_root_pi {2 / sqrt(numbers::pi_v<T>)};

    erf_series_near_zero<T> sum{x};
    return two_div_root_pi * tools::kahan_sum_series(sum, std::numeric_limits<T>::digits);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto erf_calc_impl(const T z, bool invert) noexcept -> T
{
    constexpr T zero {0, 0};

    if (z < zero)
    {
        if (!invert)
        {
            return -erf_calc_impl(-z, invert);
        }
        else if (z < T{-5, -1})
        {
            return 2 - erf_calc_impl(-z, invert);
        }
        else
        {
            return 1 + erf_calc_impl(-z, false);
        }
    }

    T result {};

    //
    // Big bunch of selection statements now to pick which
    // implementation to use, try to put most likely options
    // first:
    //
    if (z < T{5, -1})
    {
        //
        // We're going to calculate erf:
        //
        if (z == zero)
        {
            result = zero;
        }
        else if (z < T{1, -10})
        {
            constexpr T c {UINT64_C(3379167095512573896), -21};
            result = z * T{UINT64_C(1125), -3} + z * c;
        }
        else
        {
            // Max Error found at long double precision =   1.623299e-20
            // Maximum Deviation Found:                     4.326e-22
            // Expected Error Term:                         -4.326e-22
            // Maximum Relative Change in Control Points:   1.474e-04
            constexpr T Y {UINT64_C(1044948577880859375), -18};
            constexpr std::array<T, 6> P = {
                T{UINT64_C(8343058921465319890), -20},
                T{UINT64_C(3380972830755654137), -19, construction_sign::negative},
                T{UINT64_C(5096027344060672046), -20, construction_sign::negative},
                T{UINT64_C(9049063461585377944), -21, construction_sign::negative},
                T{UINT64_C(4894686514647986692), -22, construction_sign::negative},
                T{UINT64_C(2003056263661518778), -23, construction_sign::negative}
            };
            constexpr std::array<T, 6> Q = {
                T{UINT64_C(1), 0},
                T{UINT64_C(4558173005158751724), -19},
                T{UINT64_C(9165373543562417920), -20},
                T{UINT64_C(1027226526759100312), -20},
                T{UINT64_C(6505117526878515487), -22},
                T{UINT64_C(1895325191056554968), -23}
            };

            result = z * (Y + tools::evaluate_polynomial(P, T(z * z)) / tools::evaluate_polynomial(Q, T(z * z)));
        }
    }
    else if (invert ? (z < 110) : (z < T{66, -1}))
    {
        //
        // We'll be calculating erfc:
        //
        invert = !invert;

        if (z < T{15, -1})
        {
            // Max Error found at long double precision =   3.239590e-20
            // Maximum Deviation Found:                     2.241e-20
            // Expected Error Term:                         -2.241e-20
            // Maximum Relative Change in Control Points:   5.110e-03
            constexpr T Y {UINT64_C(4059357643127441406), -19};
            constexpr std::array<T, 8> P = {
                T{UINT64_C(9809059221628120317), -20, construction_sign::negative},
                T{UINT64_C(1599890899229691413), -19},
                T{UINT64_C(2223598216199357124), -19},
                T{UINT64_C(1273039217035773623), -19},
                T{UINT64_C(3840575303427624003), -20},
                T{UINT64_C(6284311608511567193), -21},
                T{UINT64_C(4412666545143917464), -22},
                T{UINT64_C(2666890683362956426), -26}
            };
            constexpr std::array<T, 7> Q = {
                T{UINT64_C(1), 0},
                T{UINT64_C(2032374749854694693), -18},
                T{UINT64_C(1783554549549694052), -18},
                T{UINT64_C(8679403262937605782), -19},
                T{UINT64_C(2480256069900216984), -19},
                T{UINT64_C(3966496318330022699), -20},
                T{UINT64_C(2792202373094490268), -21}
            };

            constexpr T half {5, -1};
            result = Y + tools::evaluate_polynomial(P, T(z - half)) / tools::evaluate_polynomial(Q, T(z - half));
        }
        else if (z < T{25, -1})
        {
            // Max Error found at long double precision =   3.686211e-21
            // Maximum Deviation Found:                     1.495e-21
            // Expected Error Term:                         -1.494e-21
            // Maximum Relative Change in Control Points:   1.793e-04

            constexpr T Y {UINT64_C(5067281723022460937), -19};
            constexpr std::array<T, 7> P = {
                T{UINT64_C(2435004762076984022), -20, construction_sign::negative},
                T{UINT64_C(3435226879356714513), -20},
                T{UINT64_C(5054208243055449495), -20},
                T{UINT64_C(2574793259177573882), -20},
                T{UINT64_C(6693498441903543561), -21},
                T{UINT64_C(9080791441609952444), -22},
                T{UINT64_C(5159172666980500279), -23}
            };
            constexpr std::array<T, 7> Q = {
                T{UINT64_C(1), 0},
                T{UINT64_C(1716578616719303363), -18},
                T{UINT64_C(1264096348242803662), -18},
                T{UINT64_C(5123714378389690159), -19},
                T{UINT64_C(1209026230511209509), -19},
                T{UINT64_C(1580271978318874853), -20},
                T{UINT64_C(8978713707780316114), -22}
            };

            constexpr T one_and_half {15, -1};
            result = Y + tools::evaluate_polynomial(P, T(z - one_and_half)) / tools::evaluate_polynomial(Q, T(z - one_and_half));
        }
        else if (z < T{45, -1})
        {
            // Maximum Deviation Found:                     1.107e-20
            // Expected Error Term:                         -1.106e-20
            // Maximum Relative Change in Control Points:   1.709e-04
            // Max Error found at long double precision =   1.446908e-20

            constexpr T Y {UINT64_C(5405750274658203125), -19};
            constexpr std::array<T, 7> P = {
                T{UINT64_C(2952767165309728403), -21},
                T{UINT64_C(1418532458954956041), -20},
                T{UINT64_C(1049595846264322939), -20},
                T{UINT64_C(3439637959761000776), -21},
                T{UINT64_C(5906544119487763790), -22},
                T{UINT64_C(5234353806361740087), -23},
                T{UINT64_C(1898960430503312573), -24}
            };
            constexpr std::array<T, 7> Q = {
                T{UINT64_C(1), 0},
                T{UINT64_C(1193521601852856426), -18},
                T{UINT64_C(6032569643634543929), -19},
                T{UINT64_C(1654111424585405858), -19},
                T{UINT64_C(2597298709462031665), -20},
                T{UINT64_C(2216575682928936992), -21},
                T{UINT64_C(8041494641903097998), -23}
            };

            constexpr T three_and_half {35, -1};
            result = Y + tools::evaluate_polynomial(P, T(z - three_and_half)) / tools::evaluate_polynomial(Q, T(z - three_and_half));
        }
        else
        {
            // Max Error found at long double precision =   7.961166e-21
            // Maximum Deviation Found:                     6.677e-21
            // Expected Error Term:                         6.676e-21
            // Maximum Relative Change in Control Points:   2.319e-05

            constexpr T Y {UINT64_C(5582551956176757812), -19};

            constexpr std::array<T, 9> P = {
                T{UINT64_C(5934387930080502141), -21},
                T{UINT64_C(2806662310090897139), -20},
                T{UINT64_C(1415978352045830500), -19, construction_sign::negative},
                T{UINT64_C(9780882011543005488), -19, construction_sign::negative},
                T{UINT64_C(5473515277960120494), -18, construction_sign::negative},
                T{UINT64_C(1386773046602453266), -17, construction_sign::negative},
                T{UINT64_C(2712749487205398217), -17, construction_sign::negative},
                T{UINT64_C(2925451527470094615), -17, construction_sign::negative},
                T{UINT64_C(1688657744997996769), -17, construction_sign::negative}
            };
            constexpr std::array<T, 9> Q = {
                T{UINT64_C(1), 0},
                T{UINT64_C(4729489111866453945), -18},
                T{UINT64_C(2367505431476957492), -17},
                T{UINT64_C(6000215173356931867), -17},
                T{UINT64_C(1317662516451495229), -16},
                T{UINT64_C(1781679249712834825), -16},
                T{UINT64_C(1824993905059152227), -16},
                T{UINT64_C(1043652514795785780), -16},
                T{UINT64_C(3083655118912242917), -17}
            };

            const auto inv_z {1 / z};
            result = Y + tools::evaluate_polynomial(P, inv_z) / tools::evaluate_polynomial(Q, inv_z);
        }

        int expon {};
        auto hi {floor(ldexp(frexp(z, &expon), 32))};
        hi = ldexp(hi, expon - 32);
        auto lo {z - hi};
        auto sq {z * z};
        auto err_sqr {((hi * hi - sq) + T{2, 0} * hi * lo) + lo * lo};

        result *= exp(-sq) * exp(-err_sqr) / z;
    }
    else
    {
        //
        // Any value of z larger than 110 will underflow to zero:
        //
        result = zero;
        invert = !invert;
    }

    if (invert)
    {
        result = T{1, 0} - result;
    }

    return result;
}

template <>
constexpr auto erf_calc_impl<decimal128_t>(const decimal128_t z, bool invert) noexcept -> decimal128_t
{
    constexpr decimal128_t zero {0, 0};
    constexpr decimal128_t half {5, -1};
    constexpr decimal128_t one {1, 0};

    if (z < zero)
    {
        if (!invert)
        {
            return -erf_calc_impl(-z, invert);
        }
        else if (z < -half)
        {
            return 2 - erf_calc_impl(-z, invert);
        }
        else
        {
            return 1 + erf_calc_impl(-z, false);
        }
    }

    decimal128_t result {};

    //
    // Big bunch of selection statements now to pick which
    // implementation to use, try to put most likely options
    // first:
    //
    if (z < half)
    {
        //
        // We're going to calculate erf:
        //
        if (z == zero)
        {
            result = zero;
        }
        else if (z < decimal128_t{1, -20})
        {
            constexpr decimal128_t c {int128::uint128_t{UINT64_C(183185015307313), UINT64_C(4316214765445777362)}, -36};
            result = z * decimal128_t{UINT64_C(1125), -3} + z * c;
        }
        else
        {
            // Max Error found at long double precision =   2.342380e-35
            // Maximum Deviation Found:                     6.124e-36
            // Expected Error Term:                         -6.124e-36
            // Maximum Relative Change in Control Points:   3.492e-10
            constexpr decimal128_t Y {UINT64_C(10841522216796875), -16};
            constexpr std::array<decimal128_t, 8> P = {
                decimal128_t{int128::uint128_t{UINT64_C(239754751511176), UINT64_C(15346977608939294094)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(192712955706190), UINT64_C(2786476198819993080)}, -34, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(315600174339923), UINT64_C(3061015393610667132)}, -35, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(61091917605891), UINT64_C(1019303663574361383)}, -35, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(436787460032112), UINT64_C(1788731756814597798)}, -37, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(306994537534154), UINT64_C(5857517254794866796)}, -38, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(91970165438019), UINT64_C(5861580289485811316)}, -39, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(186725770436288), UINT64_C(13306862545778890572)}, -41, construction_sign::negative}
            };
            constexpr std::array<decimal128_t, 8> Q = {
                decimal128_t{UINT64_C(1)},
                decimal128_t{int128::uint128_t{UINT64_C(252912975277071), UINT64_C(16234303672316163784)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(54212866299291), UINT64_C(9947708872772716820)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(69574086016095), UINT64_C(17436381122513081906)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(58086374505287), UINT64_C(2736284848178772790)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(317762509029661), UINT64_C(14901341870138001204)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(106376826023067), UINT64_C(57314722672041808)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(169888257966113), UINT64_C(17571764770326690292)}, -41}
            };

            const auto z_squared {z * z};
            result = z * (Y + tools::evaluate_polynomial(P, z_squared) / tools::evaluate_polynomial(Q, z_squared));
        }
    }
    else if (invert ? (z < decimal128_t{110}) : (z < decimal128_t{UINT64_C(865), -2}))
    {
        //
        // We'll be calculating erfc:
        //
        invert = !invert;

        if (z < one)
        {
            // Max Error found at long double precision =   3.246278e-35
            // Maximum Deviation Found:                     1.388e-35
            // Expected Error Term:                         1.387e-35
            // Maximum Relative Change in Control Points:   6.127e-05
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(201595030518654), UINT64_C(473630177736155136)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(347118283305744), UINT64_C(13376242280388530596)}, -35, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(108837567018829), UINT64_C(8949668339020089396)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(205156638136972), UINT64_C(8479374702376111038)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(165456838044201), UINT64_C(8069456678105518694)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(79629242873361), UINT64_C(2204766815466333204)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(251989150980866), UINT64_C(8451275733071948234)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(535539364059100), UINT64_C(16183076954934542620)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(74543006006681), UINT64_C(16874855259041196514)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(62057810663588), UINT64_C(4225016817461922308)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(236651445527996), UINT64_C(10163568610288357464)}, -39}
            };
            constexpr std::array<decimal128_t, 11> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(134251975244461), UINT64_C(12266621785705425304)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(151087088804865), UINT64_C(7783954991533043640)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(101533324186242), UINT64_C(5983365784156864228)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(449605535730502), UINT64_C(10426028039653281378)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(136248852536558), UINT64_C(5020121607011525382)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(283036543896270), UINT64_C(9880778004342474900)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(389408639476240), UINT64_C(2248582422915465180)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(322701424296268), UINT64_C(7168735379570594832)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(123050804282587), UINT64_C(10903520715667482668)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(146728458516852), UINT64_C(13607062250089259428)}, -44}
            };

            result = Y + tools::evaluate_polynomial(P, z - half) / tools::evaluate_polynomial(Q, z - half);
        }
        else if (z < decimal128_t{UINT64_C(15), -1})
        {
            // Max Error found at long double precision =   2.215785e-35
            // Maximum Deviation Found:                     1.539e-35
            // Expected Error Term:                         1.538e-35
            // Maximum Relative Change in Control Points:   6.104e-05
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(247512601803296), UINT64_C(15871045498809073664)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(157190807096733), UINT64_C(3137315625382477952)}, -35, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(470641968793799), UINT64_C(4414359042974488606)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(91817523159857), UINT64_C(7399250419088684648)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(72372915581218), UINT64_C(10309284290091665052)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(334719143293246), UINT64_C(12410907560623277594)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(100623987889980), UINT64_C(3812727289885689320)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(201634177286597), UINT64_C(799217504105204558)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(262985005296582), UINT64_C(13926270613440862488)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(204098189489188), UINT64_C(7062163629122386192)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(72189464720907), UINT64_C(6671367611770889188)}, -39}
            };
            constexpr std::array<decimal128_t, 10> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(126293469034752), UINT64_C(6450544005567922118)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(133533437898934), UINT64_C(4515443098870771936)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(84192571838248), UINT64_C(9806577921514899802)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(349261792374621), UINT64_C(8697853943046476554)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(98992586623193), UINT64_C(17173790472898309662)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(191996836870529), UINT64_C(4384468307423291196)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(246146619886387), UINT64_C(5406971225063089448)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(189665960731903), UINT64_C(5272013552808781312)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(67084640707228), UINT64_C(2876771981794530406)}, -38}
            };

            result = Y + tools::evaluate_polynomial(P, z - one) / tools::evaluate_polynomial(Q, z - one);
        }
        else if (z < decimal128_t{UINT64_C(225), -2})
        {
            // Maximum Deviation Found:                     1.418e-35
            // Expected Error Term:                         1.418e-35
            // Maximum Relative Change in Control Points:   1.316e-04
            // Max Error found at long double precision =   1.998462e-35
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(272406602338080), UINT64_C(4210402105957662720)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(109088969685101), UINT64_C(16218967400415836944)}, -35, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(179904028726584), UINT64_C(15631322379863663306)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(388449429341863), UINT64_C(3427022958736033442)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(295897921010371), UINT64_C(1587344243601439264)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(128311334641994), UINT64_C(878517591583687586)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(356144639692578), UINT64_C(1960158824930269962)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(65205351588585), UINT64_C(16368106670938658990)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(77045156856563), UINT64_C(153253981163960422)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(537510269782597), UINT64_C(7864628744107903138)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(169600143262140), UINT64_C(3983972277722912520)}, -40}
            };
            constexpr std::array<decimal128_t, 11> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(115741879193406), UINT64_C(8849667838245590984)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(111889261564439), UINT64_C(9272201090524384636)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(64335733615491), UINT64_C(10424160044837123455)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(242716646826127), UINT64_C(5037354530739296758)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(62372416290286), UINT64_C(9576972220148352039)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(109309228510036), UINT64_C(2873272329067173224)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(126151786412974), UINT64_C(12619763573277031316)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(87133643197794), UINT64_C(6231683340705747136)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(274931293980236), UINT64_C(1071170154394471314)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(101089787635740), UINT64_C(3144907365133271220)}, -46}
            };

            constexpr decimal128_t one_point_five {UINT64_C(15), -1};
            result = Y + tools::evaluate_polynomial(P, z - one_point_five) / tools::evaluate_polynomial(Q, z - one_point_five);
        }
        else if (z < decimal128_t{UINT64_C(3), 0})
        {
            // Maximum Deviation Found:                     3.575e-36
            // Expected Error Term:                         3.575e-36
            // Maximum Relative Change in Control Points:   7.103e-05
            // Max Error found at long double precision =   5.794737e-36
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(286754050062812), UINT64_C(9099170110843895808)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(489057861995043), UINT64_C(13133699014237994112)}, -36, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(78716949829450), UINT64_C(16506161309933484600)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(163541727676567), UINT64_C(6172848388919604508)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(116849098118354), UINT64_C(5575376344146644276)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(468745851741019), UINT64_C(5310956418198470786)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(119095866461710), UINT64_C(1828946576302487130)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(197846101027371), UINT64_C(399196491668317404)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(210429683133053), UINT64_C(13307826345816323382)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(131144025181426), UINT64_C(8089821001624051524)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(366778344605918), UINT64_C(17527236426819373002)}, -41}
            };
            constexpr std::array<decimal128_t, 10> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(104988268168107), UINT64_C(15957835969636138288)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(91869045001594), UINT64_C(3343713105315737866)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(477061739171983), UINT64_C(17142859933886225322)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(162141950642440), UINT64_C(14211750507196794040)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(374371737149964), UINT64_C(8759731599645491996)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(58778403347531), UINT64_C(12075093728068093506)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(60578304096118), UINT64_C(5572830439296116489)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(372338447915402), UINT64_C(18110338228551247378)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(104134117466942), UINT64_C(5843491151628340768)}, -39}
            };

            constexpr decimal128_t offset {UINT64_C(225), -2};
            result = Y + tools::evaluate_polynomial(P, z - offset) / tools::evaluate_polynomial(Q, z - offset);
        }
        else if (z < decimal128_t{UINT64_C(35), -1})
        {
            // Maximum Deviation Found:                     8.126e-37
            // Expected Error Term:                         -8.126e-37
            // Maximum Relative Change in Control Points:   1.363e-04
            // Max Error found at long double precision =   1.747062e-36
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(292937225141646), UINT64_C(6920050031251800064)}, -34};
            constexpr std::array<decimal128_t, 9> P = {
                decimal128_t{int128::uint128_t{UINT64_C(182706965924257), UINT64_C(1687510779571187718)}, -36, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(56892448168985), UINT64_C(572440462241151398)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(80518338580783), UINT64_C(5160816315849708842)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(442730178280838), UINT64_C(9281603077550627672)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(135371629264938), UINT64_C(7268401433168016132)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(252380094364866), UINT64_C(3735236004636993191)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(287925910284089), UINT64_C(6157066008997322426)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(186226232526489), UINT64_C(2794677292908361186)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(526445427809093), UINT64_C(11759659595468142822)}, -41}
            };
            constexpr std::array<decimal128_t, 9> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(86688065670866), UINT64_C(11737797169918939734)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(61583053693636), UINT64_C(8177778869190231158)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(254010066013673), UINT64_C(14314255351052138662)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(66581844722135), UINT64_C(10035464857808786462)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(113662830747969), UINT64_C(10480615872240633506)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(123515411355391), UINT64_C(5270626324694473614)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(78194463948513), UINT64_C(4344969105995523842)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(221048990718863), UINT64_C(13286283565256558792)}, -39}
            };

            constexpr decimal128_t offset {UINT64_C(3), 0};
            result = Y + tools::evaluate_polynomial(P, z - offset) / tools::evaluate_polynomial(Q, z - offset);
        }
        else if (z < decimal128_t{UINT64_C(55), -1})
        {
            // Maximum Deviation Found:                     5.804e-36
            // Expected Error Term:                         -5.803e-36
            // Maximum Relative Change in Control Points:   2.475e-05
            // Max Error found at long double precision =   1.349545e-35
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(298155700831090), UINT64_C(5321526117547458560)}, -34};
            constexpr std::array<decimal128_t, 11> P = {
                decimal128_t{int128::uint128_t{UINT64_C(64045367177120), UINT64_C(7126526946326712216)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(391406866150465), UINT64_C(13877902186207036830)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(427079575463624), UINT64_C(13820156213019431766)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(226722485297785), UINT64_C(7367271055178592050)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(72749098730669), UINT64_C(12561100216305275316)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(153528658470216), UINT64_C(5879898154264305294)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(219923398120484), UINT64_C(14096269023324246296)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(213307473906181), UINT64_C(12198158514880462324)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(134873959287144), UINT64_C(3092403745659621536)}, -40},
                decimal128_t{int128::uint128_t{UINT64_C(503884309615476), UINT64_C(12990957617719465524)}, -42},
                decimal128_t{int128::uint128_t{UINT64_C(84655302336436), UINT64_C(13284173252492882164)}, -43}
            };
            constexpr std::array<decimal128_t, 11> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(82917204517225), UINT64_C(1063181960067981490)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(57605799915412), UINT64_C(13799772320923268379)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(239437708311408), UINT64_C(11868061651166147832)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(65954868750830), UINT64_C(5000461927831447784)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(125840371312782), UINT64_C(6087785675919187498)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(168473189330587), UINT64_C(10282225561684677028)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(156321229157805), UINT64_C(14905032956529638730)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(96238765217732), UINT64_C(16749337409586968088)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(355113369717463), UINT64_C(1147037262655638552)}, -40},
                decimal128_t{int128::uint128_t{UINT64_C(59660975952017), UINT64_C(9288316767064383273)}, -41}
            };

            constexpr decimal128_t offset {UINT64_C(45), -1};
            result = Y + tools::evaluate_polynomial(P, z - offset) / tools::evaluate_polynomial(Q, z - offset);
        }
        else if (z < decimal128_t{UINT64_C(75), -1})
        {
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(302190791256700), UINT64_C(9714184389844172800)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(158964046028465), UINT64_C(11438909756407891630)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(122032765584843), UINT64_C(16892011538683858512)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(103532882203150), UINT64_C(1945910839355703890)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(405360279555144), UINT64_C(17738257215223704096)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(92516692701440), UINT64_C(2848453183816212240)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(133596036229094), UINT64_C(11924473305888059236)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(124584587904639), UINT64_C(291610981922610256)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(73122377133008), UINT64_C(17327362738533036112)}, -40},
                decimal128_t{int128::uint128_t{UINT64_C(246528390085209), UINT64_C(3924280155277418766)}, -42},
                decimal128_t{int128::uint128_t{UINT64_C(364835518629564), UINT64_C(5571555254170192776)}, -44}
            };
            constexpr std::array<decimal128_t, 10> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(61172687098579), UINT64_C(11482353827453039470)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(308943765243564), UINT64_C(387347066952065946)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(91666260198101), UINT64_C(11910461875258093924)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(176121839055192), UINT64_C(13669190067175731198)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(227277440268902), UINT64_C(12960685819537436558)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(197021940733251), UINT64_C(7957691617384334234)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(110655785712500), UINT64_C(17625392428371128500)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(365445712168932), UINT64_C(1180958078663840968)}, -40},
                decimal128_t{int128::uint128_t{UINT64_C(540820373195725), UINT64_C(8782966917686320850)}, -42}
            };

            constexpr decimal128_t offset {UINT64_C(65), -1};
            result = Y + tools::evaluate_polynomial(P, z - offset) / tools::evaluate_polynomial(Q, z - offset);
        }
        else if (z < decimal128_t{UINT64_C(115), -1})
        {
            // Maximum Deviation Found:                     8.380e-36
            // Expected Error Term:                         8.380e-36
            // Maximum Relative Change in Control Points:   2.632e-06
            // Max Error found at long double precision =   9.849522e-36

            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(304027649204451), UINT64_C(1728229377678557184)}, -34};
            constexpr std::array<decimal128_t, 10> P = {
                decimal128_t{int128::uint128_t{UINT64_C(153100583833654), UINT64_C(14327035843678029036)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(95077518459187), UINT64_C(9942820403827655058)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(115711360658550), UINT64_C(6169858550655575230)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(67302585249761), UINT64_C(14717008898158466981)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(229634417799869), UINT64_C(3537386017055083086)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(495496840641844), UINT64_C(10464545210085775186)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(68846798464464), UINT64_C(6483309338720405631)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(59962321481173), UINT64_C(3516818090677968196)}, -39},
                decimal128_t{int128::uint128_t{UINT64_C(298562604094816), UINT64_C(10053434321960440794)}, -41},
                decimal128_t{int128::uint128_t{UINT64_C(64908849789679), UINT64_C(2483175171918744174)}, -42}
            };
            constexpr std::array<decimal128_t, 10> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(92097343963423), UINT64_C(10172380546965853922)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(69835981275607), UINT64_C(1604553196623401915)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(310243256559610), UINT64_C(7192679776206411020)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(88990066089511), UINT64_C(8751850376241707954)}, -34},
                decimal128_t{int128::uint128_t{UINT64_C(170930514330698), UINT64_C(15386692183241008522)}, -35},
                decimal128_t{int128::uint128_t{UINT64_C(219870195268595), UINT64_C(7096478956361471750)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(182649595150853), UINT64_C(10228274995118757972)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(88922494481931), UINT64_C(16755190767040428824)}, -38},
                decimal128_t{int128::uint128_t{UINT64_C(193321492983025), UINT64_C(15015034683123628540)}, -40}
            };

            constexpr decimal128_t offset {UINT64_C(475), -2};
            const auto half_z {z / 2};
            result = Y + tools::evaluate_polynomial(P, half_z - offset) / tools::evaluate_polynomial(Q, half_z - offset);
        }
        else
        {
            // Maximum Deviation Found:                     1.132e-35
            // Expected Error Term:                         -1.132e-35
            // Maximum Relative Change in Control Points:   4.674e-04
            // Max Error found at long double precision =   1.162590e-35
            constexpr decimal128_t Y {int128::uint128_t{UINT64_C(305348553245121), UINT64_C(13092683829350334464)}, -34};
            constexpr std::array<decimal128_t, 12> P = {
                decimal128_t{int128::uint128_t{UINT64_C(499232842962978), UINT64_C(8830380466473645912)}, -37},
                decimal128_t{int128::uint128_t{UINT64_C(174252455201786), UINT64_C(2479322227425103044)}, -36},
                decimal128_t{int128::uint128_t{UINT64_C(135772070143446), UINT64_C(11134505343181509494)}, -34, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(491581404144094), UINT64_C(17408157071053090076)}, -34, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(483680789016642), UINT64_C(16561429108077906378)}, -33, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(118068225278210), UINT64_C(1054524085213991420)}, -32, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(494099073316133), UINT64_C(5874532072246990782)}, -32, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(78131897092350), UINT64_C(2017084479481280073)}, -31, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(170135756926931), UINT64_C(10167058340138167254)}, -31, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(148055281207309), UINT64_C(5898340572591612296)}, -31, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(147262609119790), UINT64_C(18318474967693923790)}, -31, construction_sign::negative},
                decimal128_t{int128::uint128_t{UINT64_C(325548278155557), UINT64_C(11031073479106502338)}, -32, construction_sign::negative}
            };
            constexpr std::array<decimal128_t, 12> Q = {
                decimal128_t{1},
                decimal128_t{int128::uint128_t{UINT64_C(189215206044366), UINT64_C(7201146952646483464)}, -33},
                decimal128_t{int128::uint128_t{UINT64_C(186246196669912), UINT64_C(16899706591617305338)}, -32},
                decimal128_t{int128::uint128_t{UINT64_C(458071748953339), UINT64_C(14530259322322392676)}, -32},
                decimal128_t{int128::uint128_t{UINT64_C(203833188002588), UINT64_C(3875604001885821522)}, -31},
                decimal128_t{int128::uint128_t{UINT64_C(341498582010851), UINT64_C(13646636761762590294)}, -31},
                decimal128_t{int128::uint128_t{UINT64_C(85020842353993), UINT64_C(17169196599155840002)}, -30},
                decimal128_t{int128::uint128_t{UINT64_C(89231168049555), UINT64_C(9824611763952834930)}, -30},
                decimal128_t{int128::uint128_t{UINT64_C(124681547981702), UINT64_C(8512281060063414408)}, -30},
                decimal128_t{int128::uint128_t{UINT64_C(66284436919305), UINT64_C(10410142805477852184)}, -30},
                decimal128_t{int128::uint128_t{UINT64_C(433333814418414), UINT64_C(10618654391229754076)}, -31},
                decimal128_t{int128::uint128_t{UINT64_C(394332605728132), UINT64_C(16714436905006754448)}, -32}
            };

            const auto inv_z {1 / z};
            result = Y + tools::evaluate_polynomial(P, inv_z) / tools::evaluate_polynomial(Q, inv_z);
        }

        decimal128_t hi {};
        decimal128_t lo {};
        int expon {};
        hi = floor(ldexp(frexp(z, &expon), 56));
        hi = ldexp(hi, expon - 56);
        lo = z - hi;
        auto sq = z * z;
        auto err_sqr = ((hi * hi - sq) + 2 * hi * lo) + lo * lo;
        result *= exp(-sq) * exp(-err_sqr) / z;
    }
    else
    {
        //
        // Any value of z larger than 110 will underflow to zero:
        //
        result = zero;
        invert = !invert;
    }

    if (invert)
    {
        result = one - result;
    }

    return result;
}

template <typename T>
constexpr auto erf_impl(const T z) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    // Edge cases
    const auto fp {fpclassify(z)};

    if (fp == FP_ZERO || fp == FP_NAN)
    {
        return z;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fp == FP_INFINITE)
    {
        return z < T{0} ? T{-1} : T{1};
    }
    #endif

    return detail::erf_calc_impl(z, false);
}

template <typename T>
constexpr auto erfc_impl(const T z) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    // Edge cases
    const auto fp {fpclassify(z)};

    if (fp == FP_NAN)
    {
        return z;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fp == FP_INFINITE)
    {
        return z < T{0} ? T{2} : T{0};
    }
    #endif

    return detail::erf_calc_impl(z, true);
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto erf(const T z) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::erf_impl(static_cast<evaluation_type>(z)));
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto erfc(const T z) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::erfc_impl(static_cast<evaluation_type>(z)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ERF_HPP
