// Copyright 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_LGAMMA_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_LGAMMA_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
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

namespace lgamma_detail {

template <bool b>
struct lgamma_taylor_series_imp
{
    using d32_coeffs_t  = std::array<decimal32_t,  17>;
    using d64_coeffs_t  = std::array<decimal64_t,  27>;
    using d128_coeffs_t = std::array<decimal128_t, 44>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  17>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  27>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 44>;

    static constexpr d32_coeffs_t d32_coeffs =
    {{
        // Use a Taylor series expansion of the logarithm of the gamma function.
        // N[Series[Log[Gamma[x]], {x, 0, 18}], 19]
        //         log(1/x)
        //        -EulerGamma                                      // * x
        + decimal32_t { UINT64_C(8224670334241132182), - 19 - 0 },   // x^2
        - decimal32_t { UINT64_C(4006856343865314285), - 19 - 0 },   // x^3
        + decimal32_t { UINT64_C(2705808084277845479), - 19 - 0 },   // x^4
        - decimal32_t { UINT64_C(2073855510286739853), - 19 - 0 },   // x^5
        + decimal32_t { UINT64_C(1695571769974081900), - 19 - 0 },   // x^6
        - decimal32_t { UINT64_C(1440498967688461181), - 19 - 0 },   // x^7
        + decimal32_t { UINT64_C(1255096695247430424), - 19 - 0 },   // x^8
        - decimal32_t { UINT64_C(1113342658695646905), - 19 - 0 },   // x^9
        + decimal32_t { UINT64_C(1000994575127818085), - 19 - 0 },   // x^10
        - decimal32_t { UINT64_C(9095401714582904223), - 19 - 1 },   // x^11
        + decimal32_t { UINT64_C(8335384054610900402), - 19 - 1 },   // x^12
        - decimal32_t { UINT64_C(7693251641135219147), - 19 - 1 },   // x^13
        + decimal32_t { UINT64_C(7143294629536133606), - 19 - 1 },   // x^14
        - decimal32_t { UINT64_C(6666870588242046803), - 19 - 1 },   // x^15
        + decimal32_t { UINT64_C(6250095514121304074), - 19 - 1 },   // x^16
        - decimal32_t { UINT64_C(5882397865868458234), - 19 - 1 },   // x^17
        + decimal32_t { UINT64_C(5555576762740361110), - 19 - 1 },   // x^18
     }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Use a Taylor series expansion of the logarithm of the gamma function.
         // N[Series[Log[Gamma[x]], {x, 0, 18}], 19]
         //         log(1/x)
         //        -EulerGamma                                      // * x
         + decimal_fast32_t { UINT64_C(8224670334241132182), - 19 - 0 },   // x^2
         - decimal_fast32_t { UINT64_C(4006856343865314285), - 19 - 0 },   // x^3
         + decimal_fast32_t { UINT64_C(2705808084277845479), - 19 - 0 },   // x^4
         - decimal_fast32_t { UINT64_C(2073855510286739853), - 19 - 0 },   // x^5
         + decimal_fast32_t { UINT64_C(1695571769974081900), - 19 - 0 },   // x^6
         - decimal_fast32_t { UINT64_C(1440498967688461181), - 19 - 0 },   // x^7
         + decimal_fast32_t { UINT64_C(1255096695247430424), - 19 - 0 },   // x^8
         - decimal_fast32_t { UINT64_C(1113342658695646905), - 19 - 0 },   // x^9
         + decimal_fast32_t { UINT64_C(1000994575127818085), - 19 - 0 },   // x^10
         - decimal_fast32_t { UINT64_C(9095401714582904223), - 19 - 1 },   // x^11
         + decimal_fast32_t { UINT64_C(8335384054610900402), - 19 - 1 },   // x^12
         - decimal_fast32_t { UINT64_C(7693251641135219147), - 19 - 1 },   // x^13
         + decimal_fast32_t { UINT64_C(7143294629536133606), - 19 - 1 },   // x^14
         - decimal_fast32_t { UINT64_C(6666870588242046803), - 19 - 1 },   // x^15
         + decimal_fast32_t { UINT64_C(6250095514121304074), - 19 - 1 },   // x^16
         - decimal_fast32_t { UINT64_C(5882397865868458234), - 19 - 1 },   // x^17
         + decimal_fast32_t { UINT64_C(5555576762740361110), - 19 - 1 },   // x^18
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
        // Use a Taylor series expansion of the logarithm of the gamma function.
        // N[Series[Log[Gamma[x]], {x, 0, 28}], 19]
        //         log(1/x)
        //        -EulerGamma                                      // * x
        + decimal64_t { UINT64_C(8224670334241132182), - 19 - 0 },   // x^2
        - decimal64_t { UINT64_C(4006856343865314285), - 19 - 0 },   // x^3
        + decimal64_t { UINT64_C(2705808084277845479), - 19 - 0 },   // x^4
        - decimal64_t { UINT64_C(2073855510286739853), - 19 - 0 },   // x^5
        + decimal64_t { UINT64_C(1695571769974081900), - 19 - 0 },   // x^6
        - decimal64_t { UINT64_C(1440498967688461181), - 19 - 0 },   // x^7
        + decimal64_t { UINT64_C(1255096695247430424), - 19 - 0 },   // x^8
        - decimal64_t { UINT64_C(1113342658695646905), - 19 - 0 },   // x^9
        + decimal64_t { UINT64_C(1000994575127818085), - 19 - 0 },   // x^10
        - decimal64_t { UINT64_C(9095401714582904223), - 19 - 1 },   // x^11
        + decimal64_t { UINT64_C(8335384054610900402), - 19 - 1 },   // x^12
        - decimal64_t { UINT64_C(7693251641135219147), - 19 - 1 },   // x^13
        + decimal64_t { UINT64_C(7143294629536133606), - 19 - 1 },   // x^14
        - decimal64_t { UINT64_C(6666870588242046803), - 19 - 1 },   // x^15
        + decimal64_t { UINT64_C(6250095514121304074), - 19 - 1 },   // x^16
        - decimal64_t { UINT64_C(5882397865868458234), - 19 - 1 },   // x^17
        + decimal64_t { UINT64_C(5555576762740361110), - 19 - 1 },   // x^18
        - decimal64_t { UINT64_C(5263167937961666073), - 19 - 1 },   // x^19
        + decimal64_t { UINT64_C(5000004769810169364), - 19 - 1 },   // x^20
        - decimal64_t { UINT64_C(4761907033014222799), - 19 - 1 },   // x^21
        + decimal64_t { UINT64_C(4545455629320466944), - 19 - 1 },   // x^22
        - decimal64_t { UINT64_C(4347826605304025936), - 19 - 1 },   // x^23
        + decimal64_t { UINT64_C(4166666915034121047), - 19 - 1 },   // x^24
        - decimal64_t { UINT64_C(4000000119214014059), - 19 - 1 },   // x^25
        + decimal64_t { UINT64_C(3846153903467518571), - 19 - 1 },   // x^26
        - decimal64_t { UINT64_C(3703703731298932555), - 19 - 1 },   // x^27
        + decimal64_t { UINT64_C(3571428584733335803), - 19 - 1 },   // x^28
    }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Use a Taylor series expansion of the logarithm of the gamma function.
         // N[Series[Log[Gamma[x]], {x, 0, 28}], 19]
         //         log(1/x)
         //        -EulerGamma                                      // * x
         + decimal_fast64_t { UINT64_C(8224670334241132182), - 19 - 0 },   // x^2
         - decimal_fast64_t { UINT64_C(4006856343865314285), - 19 - 0 },   // x^3
         + decimal_fast64_t { UINT64_C(2705808084277845479), - 19 - 0 },   // x^4
         - decimal_fast64_t { UINT64_C(2073855510286739853), - 19 - 0 },   // x^5
         + decimal_fast64_t { UINT64_C(1695571769974081900), - 19 - 0 },   // x^6
         - decimal_fast64_t { UINT64_C(1440498967688461181), - 19 - 0 },   // x^7
         + decimal_fast64_t { UINT64_C(1255096695247430424), - 19 - 0 },   // x^8
         - decimal_fast64_t { UINT64_C(1113342658695646905), - 19 - 0 },   // x^9
         + decimal_fast64_t { UINT64_C(1000994575127818085), - 19 - 0 },   // x^10
         - decimal_fast64_t { UINT64_C(9095401714582904223), - 19 - 1 },   // x^11
         + decimal_fast64_t { UINT64_C(8335384054610900402), - 19 - 1 },   // x^12
         - decimal_fast64_t { UINT64_C(7693251641135219147), - 19 - 1 },   // x^13
         + decimal_fast64_t { UINT64_C(7143294629536133606), - 19 - 1 },   // x^14
         - decimal_fast64_t { UINT64_C(6666870588242046803), - 19 - 1 },   // x^15
         + decimal_fast64_t { UINT64_C(6250095514121304074), - 19 - 1 },   // x^16
         - decimal_fast64_t { UINT64_C(5882397865868458234), - 19 - 1 },   // x^17
         + decimal_fast64_t { UINT64_C(5555576762740361110), - 19 - 1 },   // x^18
         - decimal_fast64_t { UINT64_C(5263167937961666073), - 19 - 1 },   // x^19
         + decimal_fast64_t { UINT64_C(5000004769810169364), - 19 - 1 },   // x^20
         - decimal_fast64_t { UINT64_C(4761907033014222799), - 19 - 1 },   // x^21
         + decimal_fast64_t { UINT64_C(4545455629320466944), - 19 - 1 },   // x^22
         - decimal_fast64_t { UINT64_C(4347826605304025936), - 19 - 1 },   // x^23
         + decimal_fast64_t { UINT64_C(4166666915034121047), - 19 - 1 },   // x^24
         - decimal_fast64_t { UINT64_C(4000000119214014059), - 19 - 1 },   // x^25
         + decimal_fast64_t { UINT64_C(3846153903467518571), - 19 - 1 },   // x^26
         - decimal_fast64_t { UINT64_C(3703703731298932555), - 19 - 1 },   // x^27
         + decimal_fast64_t { UINT64_C(3571428584733335803), - 19 - 1 },   // x^28
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
        // Use a Taylor series expansion of the logarithm of the gamma function.
        // N[Series[Log[Gamma[x]], {x, 0, 46}], 36]
        //         log(1/x)
        //        -EulerGamma                                                                             // * x
        + decimal128_t { int128::uint128_t { UINT64_C(445860272218065), UINT64_C(14203420802908087080) }, -34 }, // x^2
        - decimal128_t { int128::uint128_t { UINT64_C(217212117642804), UINT64_C(17657476868182733566) }, -34 }, // x^3
        + decimal128_t { int128::uint128_t { UINT64_C(146682150165144), UINT64_C(868910464649280216)   }, -34 }, // x^4
        - decimal128_t { int128::uint128_t { UINT64_C(112423932483695), UINT64_C(16359302115292012940) }, -34 }, // x^5
        + decimal128_t { int128::uint128_t { UINT64_C(91917129830549),  UINT64_C(10691428771700534346) }, -34 }, // x^6
        - decimal128_t { int128::uint128_t { UINT64_C(78089605511547),  UINT64_C(14820105259104989758) }, -34 }, // x^7
        + decimal128_t { int128::uint128_t { UINT64_C(68038928183332),  UINT64_C(1064388729383271304)  }, -34 }, // x^8
        - decimal128_t { int128::uint128_t { UINT64_C(60354426463930),  UINT64_C(7248291600601936245)  }, -34 }, // x^9
        + decimal128_t { int128::uint128_t { UINT64_C(54264024649989),  UINT64_C(4473690885429568095)  }, -34 }, // x^10
        - decimal128_t { int128::uint128_t { UINT64_C(493062714928958), UINT64_C(6174527806367053599)  }, -35 }, // x^11
        + decimal128_t { int128::uint128_t { UINT64_C(451862075025508), UINT64_C(9914131103541110236)  }, -35 }, // x^12
        - decimal128_t { int128::uint128_t { UINT64_C(417052007139823), UINT64_C(15614141522487214162) }, -35 }, // x^13
        + decimal128_t { int128::uint128_t { UINT64_C(387238777802355), UINT64_C(11643663834403323860) }, -35 }, // x^14
        - decimal128_t { int128::uint128_t { UINT64_C(361411778772587), UINT64_C(34493970254387038)    }, -35 }, // x^15
        + decimal128_t { int128::uint128_t { UINT64_C(338818356732611), UINT64_C(3351583636956848354)  }, -35 }, // x^16
        - decimal128_t { int128::uint128_t { UINT64_C(318885427279933), UINT64_C(15984063291116028642) }, -35 }, // x^17
        + decimal128_t { int128::uint128_t { UINT64_C(301168419778654), UINT64_C(4924861572478909706)  }, -35 }, // x^18
        - decimal128_t { int128::uint128_t { UINT64_C(285316905624704), UINT64_C(10127525246402845676) }, -35 }, // x^19
        + decimal128_t { int128::uint128_t { UINT64_C(271050801693303), UINT64_C(9350577868080165772)  }, -35 }, // x^20
        - decimal128_t { int128::uint128_t { UINT64_C(258143497518401), UINT64_C(2808067049374616864)  }, -35 }, // x^21
        + decimal128_t { int128::uint128_t { UINT64_C(246409643412285), UINT64_C(14764422888076943740) }, -35 }, // x^22
        - decimal128_t { int128::uint128_t { UINT64_C(235696152553045), UINT64_C(678353819689262840)   }, -35 }, // x^23
        + decimal128_t { int128::uint128_t { UINT64_C(225875466065173), UINT64_C(8075483572721697962)  }, -35 }, // x^24
        - decimal128_t { int128::uint128_t { UINT64_C(216840440959705), UINT64_C(9932733544470621540)  }, -35 }, // x^25
        + decimal128_t { int128::uint128_t { UINT64_C(208500420892654), UINT64_C(6216313969171226926)  }, -35 }, // x^26
        - decimal128_t { int128::uint128_t { UINT64_C(200778181585848), UINT64_C(10737065351264354832) }, -35 }, // x^27
        + decimal128_t { int128::uint128_t { UINT64_C(193607531522235), UINT64_C(12111991390268743970) }, -35 }, // x^28
        - decimal128_t { int128::uint128_t { UINT64_C(186931409397414), UINT64_C(9391433164642506256)  }, -35 }, // x^29
        + decimal128_t { int128::uint128_t { UINT64_C(180700362249208), UINT64_C(11251075665678452422) }, -35 }, // x^30
        - decimal128_t { int128::uint128_t { UINT64_C(174871318224254), UINT64_C(7048097254153902066)  }, -35 }, // x^31
        + decimal128_t { int128::uint128_t { UINT64_C(169406589490303), UINT64_C(3772465895856270802)  }, -35 }, // x^32
        - decimal128_t { int128::uint128_t { UINT64_C(164273056456321), UINT64_C(10547878619739699804) }, -35 }, // x^33
        + decimal128_t { int128::uint128_t { UINT64_C(159441495963031), UINT64_C(6975690319727375544)  }, -35 }, // x^34
        - decimal128_t { int128::uint128_t { UINT64_C(154886024645294), UINT64_C(2350345999520845736)  }, -35 }, // x^35
        + decimal128_t { int128::uint128_t { UINT64_C(150583635069622), UINT64_C(8350573461615823988)  }, -35 }, // x^36
        - decimal128_t { int128::uint128_t { UINT64_C(146513807093701), UINT64_C(14073039848059635994) }, -35 }, // x^37
        + decimal128_t { int128::uint128_t { UINT64_C(142658180590716), UINT64_C(17328619387130735144) }, -35 }, // x^38
        - decimal128_t { int128::uint128_t { UINT64_C(139000278524035), UINT64_C(8482044796809236580)  }, -35 }, // x^39
        + decimal128_t { int128::uint128_t { UINT64_C(135525271560811), UINT64_C(5788192050685000054)  }, -35 }, // x^40
        - decimal128_t { int128::uint128_t { UINT64_C(132219777132438), UINT64_C(13209900018467184292) }, -35 }, // x^41
        + decimal128_t { int128::uint128_t { UINT64_C(129071687200684), UINT64_C(11755517601068760186) }, -35 }, // x^42
        - decimal128_t { int128::uint128_t { UINT64_C(126070020056468), UINT64_C(6206529728400242912)  }, -35 }, // x^43
        + decimal128_t { int128::uint128_t { UINT64_C(123204792327905), UINT64_C(4326111080777755730)  }, -35 }, // x^44
        - decimal128_t { int128::uint128_t { UINT64_C(120466908053948), UINT64_C(6659042857988127932)  }, -35 }, // x^45
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // Use a Taylor series expansion of the logarithm of the gamma function.
        // N[Series[Log[Gamma[x]], {x, 0, 46}], 36]
        //         log(1/x)
        //        -EulerGamma                                                                             // * x
        + decimal_fast128_t { int128::uint128_t { UINT64_C(445860272218065), UINT64_C(14203420802908087080) }, -34 }, // x^2
        - decimal_fast128_t { int128::uint128_t { UINT64_C(217212117642804), UINT64_C(17657476868182733566) }, -34 }, // x^3
        + decimal_fast128_t { int128::uint128_t { UINT64_C(146682150165144), UINT64_C(868910464649280216)   }, -34 }, // x^4
        - decimal_fast128_t { int128::uint128_t { UINT64_C(112423932483695), UINT64_C(16359302115292012940) }, -34 }, // x^5
        + decimal_fast128_t { int128::uint128_t { UINT64_C(91917129830549),  UINT64_C(10691428771700534346) }, -34 }, // x^6
        - decimal_fast128_t { int128::uint128_t { UINT64_C(78089605511547),  UINT64_C(14820105259104989758) }, -34 }, // x^7
        + decimal_fast128_t { int128::uint128_t { UINT64_C(68038928183332),  UINT64_C(1064388729383271304)  }, -34 }, // x^8
        - decimal_fast128_t { int128::uint128_t { UINT64_C(60354426463930),  UINT64_C(7248291600601936245)  }, -34 }, // x^9
        + decimal_fast128_t { int128::uint128_t { UINT64_C(54264024649989),  UINT64_C(4473690885429568095)  }, -34 }, // x^10
        - decimal_fast128_t { int128::uint128_t { UINT64_C(493062714928958), UINT64_C(6174527806367053599)  }, -35 }, // x^11
        + decimal_fast128_t { int128::uint128_t { UINT64_C(451862075025508), UINT64_C(9914131103541110236)  }, -35 }, // x^12
        - decimal_fast128_t { int128::uint128_t { UINT64_C(417052007139823), UINT64_C(15614141522487214162) }, -35 }, // x^13
        + decimal_fast128_t { int128::uint128_t { UINT64_C(387238777802355), UINT64_C(11643663834403323860) }, -35 }, // x^14
        - decimal_fast128_t { int128::uint128_t { UINT64_C(361411778772587), UINT64_C(34493970254387038)    }, -35 }, // x^15
        + decimal_fast128_t { int128::uint128_t { UINT64_C(338818356732611), UINT64_C(3351583636956848354)  }, -35 }, // x^16
        - decimal_fast128_t { int128::uint128_t { UINT64_C(318885427279933), UINT64_C(15984063291116028642) }, -35 }, // x^17
        + decimal_fast128_t { int128::uint128_t { UINT64_C(301168419778654), UINT64_C(4924861572478909706)  }, -35 }, // x^18
        - decimal_fast128_t { int128::uint128_t { UINT64_C(285316905624704), UINT64_C(10127525246402845676) }, -35 }, // x^19
        + decimal_fast128_t { int128::uint128_t { UINT64_C(271050801693303), UINT64_C(9350577868080165772)  }, -35 }, // x^20
        - decimal_fast128_t { int128::uint128_t { UINT64_C(258143497518401), UINT64_C(2808067049374616864)  }, -35 }, // x^21
        + decimal_fast128_t { int128::uint128_t { UINT64_C(246409643412285), UINT64_C(14764422888076943740) }, -35 }, // x^22
        - decimal_fast128_t { int128::uint128_t { UINT64_C(235696152553045), UINT64_C(678353819689262840)   }, -35 }, // x^23
        + decimal_fast128_t { int128::uint128_t { UINT64_C(225875466065173), UINT64_C(8075483572721697962)  }, -35 }, // x^24
        - decimal_fast128_t { int128::uint128_t { UINT64_C(216840440959705), UINT64_C(9932733544470621540)  }, -35 }, // x^25
        + decimal_fast128_t { int128::uint128_t { UINT64_C(208500420892654), UINT64_C(6216313969171226926)  }, -35 }, // x^26
        - decimal_fast128_t { int128::uint128_t { UINT64_C(200778181585848), UINT64_C(10737065351264354832) }, -35 }, // x^27
        + decimal_fast128_t { int128::uint128_t { UINT64_C(193607531522235), UINT64_C(12111991390268743970) }, -35 }, // x^28
        - decimal_fast128_t { int128::uint128_t { UINT64_C(186931409397414), UINT64_C(9391433164642506256)  }, -35 }, // x^29
        + decimal_fast128_t { int128::uint128_t { UINT64_C(180700362249208), UINT64_C(11251075665678452422) }, -35 }, // x^30
        - decimal_fast128_t { int128::uint128_t { UINT64_C(174871318224254), UINT64_C(7048097254153902066)  }, -35 }, // x^31
        + decimal_fast128_t { int128::uint128_t { UINT64_C(169406589490303), UINT64_C(3772465895856270802)  }, -35 }, // x^32
        - decimal_fast128_t { int128::uint128_t { UINT64_C(164273056456321), UINT64_C(10547878619739699804) }, -35 }, // x^33
        + decimal_fast128_t { int128::uint128_t { UINT64_C(159441495963031), UINT64_C(6975690319727375544)  }, -35 }, // x^34
        - decimal_fast128_t { int128::uint128_t { UINT64_C(154886024645294), UINT64_C(2350345999520845736)  }, -35 }, // x^35
        + decimal_fast128_t { int128::uint128_t { UINT64_C(150583635069622), UINT64_C(8350573461615823988)  }, -35 }, // x^36
        - decimal_fast128_t { int128::uint128_t { UINT64_C(146513807093701), UINT64_C(14073039848059635994) }, -35 }, // x^37
        + decimal_fast128_t { int128::uint128_t { UINT64_C(142658180590716), UINT64_C(17328619387130735144) }, -35 }, // x^38
        - decimal_fast128_t { int128::uint128_t { UINT64_C(139000278524035), UINT64_C(8482044796809236580)  }, -35 }, // x^39
        + decimal_fast128_t { int128::uint128_t { UINT64_C(135525271560811), UINT64_C(5788192050685000054)  }, -35 }, // x^40
        - decimal_fast128_t { int128::uint128_t { UINT64_C(132219777132438), UINT64_C(13209900018467184292) }, -35 }, // x^41
        + decimal_fast128_t { int128::uint128_t { UINT64_C(129071687200684), UINT64_C(11755517601068760186) }, -35 }, // x^42
        - decimal_fast128_t { int128::uint128_t { UINT64_C(126070020056468), UINT64_C(6206529728400242912)  }, -35 }, // x^43
        + decimal_fast128_t { int128::uint128_t { UINT64_C(123204792327905), UINT64_C(4326111080777755730)  }, -35 }, // x^44
        - decimal_fast128_t { int128::uint128_t { UINT64_C(120466908053948), UINT64_C(6659042857988127932)  }, -35 }, // x^45
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d32_coeffs_t lgamma_taylor_series_imp<b>::d32_coeffs;

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d64_coeffs_t lgamma_taylor_series_imp<b>::d64_coeffs;

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d128_coeffs_t lgamma_taylor_series_imp<b>::d128_coeffs;

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d32_fast_coeffs_t lgamma_taylor_series_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d64_fast_coeffs_t lgamma_taylor_series_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename lgamma_taylor_series_imp<b>::d128_fast_coeffs_t lgamma_taylor_series_imp<b>::d128_fast_coeffs;

#endif

} //namespace lgamma_detail

using lgamma_taylor_series_table = lgamma_detail::lgamma_taylor_series_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto lgamma_taylor_series_expansion(T x) noexcept;

template <>
constexpr auto lgamma_taylor_series_expansion<decimal32_t>(decimal32_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d32_coeffs);
}

template <>
constexpr auto lgamma_taylor_series_expansion<decimal_fast32_t>(decimal_fast32_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d32_fast_coeffs);
}

template <>
constexpr auto lgamma_taylor_series_expansion<decimal64_t>(decimal64_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d64_coeffs);
}

template <>
constexpr auto lgamma_taylor_series_expansion<decimal_fast64_t>(decimal_fast64_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d64_fast_coeffs);
}

template <>
constexpr auto lgamma_taylor_series_expansion<decimal128_t>(decimal128_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d128_coeffs);
}

template <>
constexpr auto lgamma_taylor_series_expansion<decimal_fast128_t>(decimal_fast128_t x) noexcept
{
    return taylor_series_result(x, lgamma_taylor_series_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_TGAMMA_IMPL_HPP
