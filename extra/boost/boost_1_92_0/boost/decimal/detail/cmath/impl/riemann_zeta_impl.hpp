// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_RIEMANN_ZETA_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_RIEMANN_ZETA_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <cstddef>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace riemann_zeta_detail {

template <bool b, typename T>
struct prime_table_imp
{
    using decimal_type = T;

    using prime_table_t = std::array<decimal_type, 36>;

    static constexpr prime_table_t primes =
    {{
        // Table[Prime[n], {n, 1, 36, 1}]

        decimal_type {   2 }, decimal_type {   3 }, decimal_type {   5 }, decimal_type {   7 },
        decimal_type {  11 }, decimal_type {  13 }, decimal_type {  17 }, decimal_type {  19 },
        decimal_type {  23 }, decimal_type {  29 }, decimal_type {  31 }, decimal_type {  37 },
        decimal_type {  41 }, decimal_type {  43 }, decimal_type {  47 }, decimal_type {  53 },
        decimal_type {  59 }, decimal_type {  61 }, decimal_type {  67 }, decimal_type {  71 },
        decimal_type {  73 }, decimal_type {  79 }, decimal_type {  83 }, decimal_type {  89 },
        decimal_type {  97 }, decimal_type { 101 }, decimal_type { 103 }, decimal_type { 107 },
        decimal_type { 109 }, decimal_type { 113 }, decimal_type { 127 }, decimal_type { 131 },
        decimal_type { 137 }, decimal_type { 139 }, decimal_type { 149 }, decimal_type { 151 }
    }};
};

template <bool b>
struct riemann_zeta_table_imp
{
private:
    using d32_coeffs_t       = std::array<decimal32_t,        7>;
    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,   7>;
    using d64_coeffs_t       = std::array<decimal64_t,       10>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  10>;
    using d128_coeffs_t      = std::array<decimal128_t,      15>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 15>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
        // N[Series[Zeta[x], {x, 1, 6}], 19]

        +::boost::decimal::decimal32_t { UINT64_C(5772156649015328606), - 19 -  0 }, // EulerGamma
        +::boost::decimal::decimal32_t { UINT64_C(7281584548367672486), - 19 -  1 }, // * (x - 1)
        -::boost::decimal::decimal32_t { UINT64_C(4845181596436159242), - 19 -  2 }, // * (x - 1)^2
        -::boost::decimal::decimal32_t { UINT64_C(3423057367172243110), - 19 -  3 }, // * (x - 1)^3
        +::boost::decimal::decimal32_t { UINT64_C(9689041939447083573), - 19 -  4 }, // * (x - 1)^4
        -::boost::decimal::decimal32_t { UINT64_C(6611031810842189181), - 19 -  5 }, // * (x - 1)^5
        -::boost::decimal::decimal32_t { UINT64_C(3316240908752772359), - 19 -  6 }, // * (x - 1)^6
     }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
        // N[Series[Zeta[x], {x, 1, 6}], 19]

        +::boost::decimal::decimal_fast32_t { UINT64_C(5772156649015328606), - 19 -  0 }, // EulerGamma
        +::boost::decimal::decimal_fast32_t { UINT64_C(7281584548367672486), - 19 -  1 }, // * (x - 1)
        -::boost::decimal::decimal_fast32_t { UINT64_C(4845181596436159242), - 19 -  2 }, // * (x - 1)^2
        -::boost::decimal::decimal_fast32_t { UINT64_C(3423057367172243110), - 19 -  3 }, // * (x - 1)^3
        +::boost::decimal::decimal_fast32_t { UINT64_C(9689041939447083573), - 19 -  4 }, // * (x - 1)^4
        -::boost::decimal::decimal_fast32_t { UINT64_C(6611031810842189181), - 19 -  5 }, // * (x - 1)^5
        -::boost::decimal::decimal_fast32_t { UINT64_C(3316240908752772359), - 19 -  6 }, // * (x - 1)^6
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
        // N[Series[Zeta[x], {x, 1, 9}], 19]

        +::boost::decimal::decimal64_t { UINT64_C(5772156649015328606), - 19 -  0 }, // EulerGamma
        +::boost::decimal::decimal64_t { UINT64_C(7281584548367672486), - 19 -  1 }, // * (x - 1)
        -::boost::decimal::decimal64_t { UINT64_C(4845181596436159242), - 19 -  2 }, // * (x - 1)^2
        -::boost::decimal::decimal64_t { UINT64_C(3423057367172243110), - 19 -  3 }, // * (x - 1)^3
        +::boost::decimal::decimal64_t { UINT64_C(9689041939447083573), - 19 -  4 }, // * (x - 1)^4
        -::boost::decimal::decimal64_t { UINT64_C(6611031810842189181), - 19 -  5 }, // * (x - 1)^5
        -::boost::decimal::decimal64_t { UINT64_C(3316240908752772359), - 19 -  6 }, // * (x - 1)^6
        +::boost::decimal::decimal64_t { UINT64_C(1046209458447918742), - 19 -  6 }, // * (x - 1)^7
        -::boost::decimal::decimal64_t { UINT64_C(8733218100273797361), - 19 -  8 }, // * (x - 1)^8
        +::boost::decimal::decimal64_t { UINT64_C(9478277782762358956), - 19 - 10 }, // * (x - 1)^9
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // N[Series[Zeta[x], {x, 1, 9}], 19]

         +::boost::decimal::decimal_fast64_t { UINT64_C(5772156649015328606), - 19 -  0 }, // EulerGamma
         +::boost::decimal::decimal_fast64_t { UINT64_C(7281584548367672486), - 19 -  1 }, // * (x - 1)
         -::boost::decimal::decimal_fast64_t { UINT64_C(4845181596436159242), - 19 -  2 }, // * (x - 1)^2
         -::boost::decimal::decimal_fast64_t { UINT64_C(3423057367172243110), - 19 -  3 }, // * (x - 1)^3
         +::boost::decimal::decimal_fast64_t { UINT64_C(9689041939447083573), - 19 -  4 }, // * (x - 1)^4
         -::boost::decimal::decimal_fast64_t { UINT64_C(6611031810842189181), - 19 -  5 }, // * (x - 1)^5
         -::boost::decimal::decimal_fast64_t { UINT64_C(3316240908752772359), - 19 -  6 }, // * (x - 1)^6
         +::boost::decimal::decimal_fast64_t { UINT64_C(1046209458447918742), - 19 -  6 }, // * (x - 1)^7
         -::boost::decimal::decimal_fast64_t { UINT64_C(8733218100273797361), - 19 -  8 }, // * (x - 1)^8
         +::boost::decimal::decimal_fast64_t { UINT64_C(9478277782762358956), - 19 - 10 }, // * (x - 1)^9
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
        // N[Series[Zeta[x], {x, 1, 14}], 36]

        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(312909238939453), UINT64_C(7916302232898517972) }, -34 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(394735489323855), UINT64_C(10282954930524890450) }, -35 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(262657820647143), UINT64_C(7801536535536173172) }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(185564311701532), UINT64_C(15687007158497646588) }, -37 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(525244016002584), UINT64_C(12277750447068982866) }, -38 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(358384752584293), UINT64_C(18370286456371002882) }, -39 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(179773779887752), UINT64_C(17772011513518515048) }, -40 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(56715128386205), UINT64_C(15292499466693711883) }, -40 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(473428701855329), UINT64_C(926484760170384186) }, -42 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(513818468174601), UINT64_C(18105240268308765734) }, -44 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(306743667337648), UINT64_C(15567754919026551912) }, -44 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(366931412745108), UINT64_C(2220247416524400302) }, -45 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(189307984255553), UINT64_C(8448217616480074192) }, -46 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(239089604329878), UINT64_C(14831803080673374292) }, -48 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(130092671757244), UINT64_C(16458215134170057406) }, -48 },
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
         // N[Series[Zeta[x], {x, 1, 14}], 36]

         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(312909238939453), UINT64_C(7916302232898517972) }, -34 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(394735489323855), UINT64_C(10282954930524890450) }, -35 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(262657820647143), UINT64_C(7801536535536173172) }, -36 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(185564311701532), UINT64_C(15687007158497646588) }, -37 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(525244016002584), UINT64_C(12277750447068982866) }, -38 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(358384752584293), UINT64_C(18370286456371002882) }, -39 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(179773779887752), UINT64_C(17772011513518515048) }, -40 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(56715128386205), UINT64_C(15292499466693711883) }, -40 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(473428701855329), UINT64_C(926484760170384186) }, -42 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(513818468174601), UINT64_C(18105240268308765734) }, -44 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(306743667337648), UINT64_C(15567754919026551912) }, -44 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(366931412745108), UINT64_C(2220247416524400302) }, -45 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(189307984255553), UINT64_C(8448217616480074192) }, -46 },
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(239089604329878), UINT64_C(14831803080673374292) }, -48 },
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(130092671757244), UINT64_C(16458215134170057406) }, -48 },
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d32_coeffs_t riemann_zeta_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d32_fast_coeffs_t riemann_zeta_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d64_coeffs_t riemann_zeta_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d64_fast_coeffs_t riemann_zeta_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d128_coeffs_t riemann_zeta_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename riemann_zeta_table_imp<b>::d128_fast_coeffs_t riemann_zeta_table_imp<b>::d128_fast_coeffs;

template <bool b, typename T>
constexpr typename prime_table_imp<b, T>::prime_table_t prime_table_imp<b, T>::primes;

#endif

} //namespace riemann_zeta_detail

using riemann_zeta_table = riemann_zeta_detail::riemann_zeta_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto riemann_zeta_series_or_pade_expansion(T x) noexcept;

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal32_t>(decimal32_t x) noexcept
{
    constexpr decimal32_t one { 1 };

    const decimal32_t dx { x - one };

    if (fabs(dx) < decimal32_t { 5, - 2 })
    {
        return one / dx + taylor_series_result(dx, riemann_zeta_table::d32_coeffs);
    }
    else
    {
        const decimal32_t top =
                  (decimal32_t { UINT64_C(7025346442393055904), -19 + 1 }
            + x * (decimal32_t { UINT64_C(6331631438687936980), -19 + 1 }
            + x *  decimal32_t { UINT64_C(1671529107642800378), -19 + 1 }));

        const decimal32_t bot =
                  (decimal32_t { UINT64_C(1402850698872379326), -19 + 2, true }
            + x * (decimal32_t { UINT64_C(1302850698872379326), -19 + 2 }
            + x *  decimal32_t { 1 }));

        return top / bot;
    }
}

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal_fast32_t>(decimal_fast32_t x) noexcept
{
    constexpr decimal_fast32_t one { 1 };

    const decimal_fast32_t dx { x - one };

    if (fabs(dx) < decimal_fast32_t { 5, -2 })
    {
        return one / dx + taylor_series_result(dx, riemann_zeta_table::d32_fast_coeffs);
    }
    else
    {
        const decimal_fast32_t top =
                  (decimal_fast32_t { UINT64_C(7025346442393055904), -19 + 1 }
            + x * (decimal_fast32_t { UINT64_C(6331631438687936980), -19 + 1 }
            + x *  decimal_fast32_t { UINT64_C(1671529107642800378), -19 + 1 }));

        const decimal_fast32_t bot =
                  (decimal_fast32_t { UINT64_C(1402850698872379326), -19 + 2, true }
            + x * (decimal_fast32_t { UINT64_C(1302850698872379326), -19 + 2 }
            + x *  decimal_fast32_t { 1 }));

        return top / bot;
    }
}

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal64_t>(decimal64_t x) noexcept
{
    constexpr decimal64_t one { 1 };

    const decimal64_t dx { x - one };

    if (fabs(dx) < decimal64_t { 5, -2 })
    {
        return one / dx + taylor_series_result(dx, riemann_zeta_table::d64_coeffs);
    }
    else
    {
        constexpr decimal64_t c0 { UINT64_C(4124764818173475125), - 19 + 5 };
        constexpr decimal64_t c1 { UINT64_C(4582078064035558510), - 19 + 5 };
        constexpr decimal64_t c2 { UINT64_C(1806662427082674333), - 19 + 5 };
        constexpr decimal64_t c3 { UINT64_C(3281232347201801441), - 19 + 4 };
        constexpr decimal64_t c4 { UINT64_C(3092253262304078300), - 19 + 3 };
        constexpr decimal64_t c5 { UINT64_C(1985384224421766402), - 19 + 2 };
        constexpr decimal64_t c6 { UINT64_C(1016070109033501213), - 19 + 1 };

        constexpr decimal64_t d0 { UINT64_C(8249529636338921254), - 19 + 5, true };
        constexpr decimal64_t d1 { UINT64_C(5997465199121809585), - 19 + 5 };
        constexpr decimal64_t d2 { UINT64_C(1915568444415559307), - 19 + 5 };
        constexpr decimal64_t d3 { UINT64_C(3021354370625514285), - 19 + 4 };
        constexpr decimal64_t d4 { UINT64_C(3227310996533313801), - 19 + 3 };
        constexpr decimal64_t d5 { UINT64_C(1987445773667795184), - 19 + 2 };

        const decimal64_t top { c0 + x * (c1 + x * (c2 + x * (c3 + x * (c4 + x * (c5+ x * c6))))) };
        const decimal64_t bot { d0 + x * (d1 + x * (d2 + x * (d3 + x * (d4 + x * (d5 + x))))) };

        return top / bot;
    }
}

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal_fast64_t>(decimal_fast64_t x) noexcept
{
    constexpr decimal_fast64_t one { 1 };

    const decimal_fast64_t dx { x - one };

    if (fabs(dx) < decimal_fast64_t { 5, -2 })
    {
        return one / dx + taylor_series_result(dx, riemann_zeta_table::d64_fast_coeffs);
    }
    else
    {
        constexpr decimal_fast64_t c0 { UINT64_C(4124764818173475125), - 19 + 5 };
        constexpr decimal_fast64_t c1 { UINT64_C(4582078064035558510), - 19 + 5 };
        constexpr decimal_fast64_t c2 { UINT64_C(1806662427082674333), - 19 + 5 };
        constexpr decimal_fast64_t c3 { UINT64_C(3281232347201801441), - 19 + 4 };
        constexpr decimal_fast64_t c4 { UINT64_C(3092253262304078300), - 19 + 3 };
        constexpr decimal_fast64_t c5 { UINT64_C(1985384224421766402), - 19 + 2 };
        constexpr decimal_fast64_t c6 { UINT64_C(1016070109033501213), - 19 + 1 };

        constexpr decimal_fast64_t d0 { UINT64_C(8249529636338921254), - 19 + 5, true };
        constexpr decimal_fast64_t d1 { UINT64_C(5997465199121809585), - 19 + 5 };
        constexpr decimal_fast64_t d2 { UINT64_C(1915568444415559307), - 19 + 5 };
        constexpr decimal_fast64_t d3 { UINT64_C(3021354370625514285), - 19 + 4 };
        constexpr decimal_fast64_t d4 { UINT64_C(3227310996533313801), - 19 + 3 };
        constexpr decimal_fast64_t d5 { UINT64_C(1987445773667795184), - 19 + 2 };

        const decimal_fast64_t top { c0 + x * (c1 + x * (c2 + x * (c3 + x * (c4 + x * (c5+ x * c6))))) };
        const decimal_fast64_t bot { d0 + x * (d1 + x * (d2 + x * (d3 + x * (d4 + x * (d5 + x))))) };

        return top / bot;
    }
}

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal128_t>(decimal128_t x) noexcept
{
    constexpr decimal128_t one { 1 };

    const decimal128_t dx { x - one };

    return one / dx + taylor_series_result(dx, riemann_zeta_table::d128_coeffs);
}

template <>
constexpr auto riemann_zeta_series_or_pade_expansion<decimal_fast128_t>(decimal_fast128_t x) noexcept
{
    constexpr decimal_fast128_t one { 1 };

    const decimal_fast128_t dx { x - one };

    return one / dx + taylor_series_result(dx, riemann_zeta_table::d128_fast_coeffs);
}

template<typename T>
using prime_table_t = typename riemann_zeta_detail::prime_table_imp<true, T>::prime_table_t;

template<typename T>
using prime_table = riemann_zeta_detail::prime_table_imp<true, T>;

template <typename T>
constexpr auto riemann_zeta_decimal_order(T x) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, int)
{
    int n { };

    static_cast<void>(frexp10(x, &n));

    constexpr int order_bias
    {
          std::numeric_limits<T>::digits10 < 10 ?  6
        : std::numeric_limits<T>::digits10 < 20 ? 15
        :                                         33
    };

    return n + order_bias;
}

template <typename T>
constexpr auto riemann_zeta_factorial(int nf) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    return { (nf <= 1) ? T { 1 } : riemann_zeta_factorial<T>(nf - 1) * nf };
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_RIEMANN_ZETA_IMPL_HPP
