// Copyright 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_TGAMMA_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_TGAMMA_IMPL_HPP

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

namespace tgamma_detail {

template <bool b>
struct tgamma_table_imp
{
    using d32_coeffs_t  = std::array<decimal32_t,  15>;
    using d64_coeffs_t  = std::array<decimal64_t,  36>;
    using d128_coeffs_t = std::array<decimal128_t, 45>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,  15>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  36>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 45>;

    using d32_coeffs_asymp_t  = std::array<decimal32_t,  10>;
    using d64_coeffs_asymp_t  = std::array<decimal64_t,  15>;
    using d128_coeffs_asymp_t = std::array<decimal128_t, 22>;

    using d32_fast_coeffs_asymp_t  = std::array<decimal_fast32_t,  10>;
    using d64_fast_coeffs_asymp_t  = std::array<decimal_fast64_t,  15>;
    using d128_fast_coeffs_asymp_t = std::array<decimal_fast128_t, 22>;

    static constexpr d32_coeffs_t d32_coeffs =
    {{
        // N[Series[1/Gamma[z], {z, 0, 16}], 19]
        +::boost::decimal::decimal32_t { UINT64_C(5'772'156'649'015'328'606), - 19 - 0 }, // * z^2
        -::boost::decimal::decimal32_t { UINT64_C(6'558'780'715'202'538'811), - 19 - 0 }, // * z^3
        -::boost::decimal::decimal32_t { UINT64_C(4'200'263'503'409'523'553), - 19 - 1 }, // * z^4
        +::boost::decimal::decimal32_t { UINT64_C(1'665'386'113'822'914'895), - 19 - 0 }, // * z^5
        -::boost::decimal::decimal32_t { UINT64_C(4'219'773'455'554'433'675), - 19 - 1 }, // * z^6
        -::boost::decimal::decimal32_t { UINT64_C(9'621'971'527'876'973'562), - 19 - 2 }, // * z^7
        +::boost::decimal::decimal32_t { UINT64_C(7'218'943'246'663'099'542), - 19 - 2 }, // * z^8
        -::boost::decimal::decimal32_t { UINT64_C(1'165'167'591'859'065'112), - 19 - 2 }, // * z^9
        -::boost::decimal::decimal32_t { UINT64_C(2'152'416'741'149'509'728), - 19 - 3 }, // * z^10
        +::boost::decimal::decimal32_t { UINT64_C(1'280'502'823'881'161'862), - 19 - 3 }, // * z^11
        -::boost::decimal::decimal32_t { UINT64_C(2'013'485'478'078'823'866), - 19 - 4 }, // * z^12
        -::boost::decimal::decimal32_t { UINT64_C(1'250'493'482'142'670'657), - 19 - 5 }, // * z^13
        +::boost::decimal::decimal32_t { UINT64_C(1'133'027'231'981'695'882), - 19 - 5 }, // * z^14
        -::boost::decimal::decimal32_t { UINT64_C(2'056'338'416'977'607'103), - 19 - 6 }, // * z^15
        +::boost::decimal::decimal32_t { UINT64_C(6'116'095'104'481'415'818), - 19 - 8 }, // * z^16
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // N[Series[1/Gamma[z], {z, 0, 16}], 19]
         +::boost::decimal::decimal_fast32_t { UINT64_C(5'772'156'649'015'328'606), - 19 - 0 }, // * z^2
         -::boost::decimal::decimal_fast32_t { UINT64_C(6'558'780'715'202'538'811), - 19 - 0 }, // * z^3
         -::boost::decimal::decimal_fast32_t { UINT64_C(4'200'263'503'409'523'553), - 19 - 1 }, // * z^4
         +::boost::decimal::decimal_fast32_t { UINT64_C(1'665'386'113'822'914'895), - 19 - 0 }, // * z^5
         -::boost::decimal::decimal_fast32_t { UINT64_C(4'219'773'455'554'433'675), - 19 - 1 }, // * z^6
         -::boost::decimal::decimal_fast32_t { UINT64_C(9'621'971'527'876'973'562), - 19 - 2 }, // * z^7
         +::boost::decimal::decimal_fast32_t { UINT64_C(7'218'943'246'663'099'542), - 19 - 2 }, // * z^8
         -::boost::decimal::decimal_fast32_t { UINT64_C(1'165'167'591'859'065'112), - 19 - 2 }, // * z^9
         -::boost::decimal::decimal_fast32_t { UINT64_C(2'152'416'741'149'509'728), - 19 - 3 }, // * z^10
         +::boost::decimal::decimal_fast32_t { UINT64_C(1'280'502'823'881'161'862), - 19 - 3 }, // * z^11
         -::boost::decimal::decimal_fast32_t { UINT64_C(2'013'485'478'078'823'866), - 19 - 4 }, // * z^12
         -::boost::decimal::decimal_fast32_t { UINT64_C(1'250'493'482'142'670'657), - 19 - 5 }, // * z^13
         +::boost::decimal::decimal_fast32_t { UINT64_C(1'133'027'231'981'695'882), - 19 - 5 }, // * z^14
         -::boost::decimal::decimal_fast32_t { UINT64_C(2'056'338'416'977'607'103), - 19 - 6 }, // * z^15
         +::boost::decimal::decimal_fast32_t { UINT64_C(6'116'095'104'481'415'818), - 19 - 8 }, // * z^16
     }};

    static constexpr d32_coeffs_asymp_t d32_coeffs_asymp =
    {{
        // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 7}], 19]
        +::boost::decimal::decimal32_t { UINT64_C(2506628274631000502), - 19 + 1 },
        +::boost::decimal::decimal32_t { UINT64_C(2088856895525833752), - 19 - 0 }, // / x
        +::boost::decimal::decimal32_t { UINT64_C(8703570398024307300), - 19 - 2 }, // / x^2
        -::boost::decimal::decimal32_t { UINT64_C(6721090474029881748), - 19 - 2 }, // / x^3
        -::boost::decimal::decimal32_t { UINT64_C(5752012381101712348), - 19 - 3 }, // / x^4
        +::boost::decimal::decimal32_t { UINT64_C(1965294881583203064), - 19 - 2 }, // / x^5
        +::boost::decimal::decimal32_t { UINT64_C(1747825212045591212), - 19 - 3 }, // / x^6
        -::boost::decimal::decimal32_t { UINT64_C(1484341135158276145), - 19 - 2 }, // / x^7
        -::boost::decimal::decimal32_t { UINT64_C(1296375732112554321), - 19 - 3 }, // / x^8
        +::boost::decimal::decimal32_t { UINT64_C(2104311229753206373), - 19 - 2 }, // / x^9
    }};

    static constexpr d32_fast_coeffs_asymp_t d32_fast_coeffs_asymp =
    {{
         // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 7}], 19]
         +::boost::decimal::decimal_fast32_t { UINT64_C(2506628274631000502), - 19 + 1 },
         +::boost::decimal::decimal_fast32_t { UINT64_C(2088856895525833752), - 19 - 0 }, // / x
         +::boost::decimal::decimal_fast32_t { UINT64_C(8703570398024307300), - 19 - 2 }, // / x^2
         -::boost::decimal::decimal_fast32_t { UINT64_C(6721090474029881748), - 19 - 2 }, // / x^3
         -::boost::decimal::decimal_fast32_t { UINT64_C(5752012381101712348), - 19 - 3 }, // / x^4
         +::boost::decimal::decimal_fast32_t { UINT64_C(1965294881583203064), - 19 - 2 }, // / x^5
         +::boost::decimal::decimal_fast32_t { UINT64_C(1747825212045591212), - 19 - 3 }, // / x^6
         -::boost::decimal::decimal_fast32_t { UINT64_C(1484341135158276145), - 19 - 2 }, // / x^7
         -::boost::decimal::decimal_fast32_t { UINT64_C(1296375732112554321), - 19 - 3 }, // / x^8
         +::boost::decimal::decimal_fast32_t { UINT64_C(2104311229753206373), - 19 - 2 }, // / x^9
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // N[Series[1/Gamma[z], {z, 0, 27}], 19]
         +::boost::decimal::decimal64_t { UINT64_C(5'772'156'649'015'328'606), - 19 - 0  }, // * z^2
         -::boost::decimal::decimal64_t { UINT64_C(6'558'780'715'202'538'811), - 19 - 0  }, // * z^3
         -::boost::decimal::decimal64_t { UINT64_C(4'200'263'503'409'523'553), - 19 - 1  }, // * z^4
         +::boost::decimal::decimal64_t { UINT64_C(1'665'386'113'822'914'895), - 19 - 0  }, // * z^5
         -::boost::decimal::decimal64_t { UINT64_C(4'219'773'455'554'433'675), - 19 - 1  }, // * z^6
         -::boost::decimal::decimal64_t { UINT64_C(9'621'971'527'876'973'562), - 19 - 2  }, // * z^7
         +::boost::decimal::decimal64_t { UINT64_C(7'218'943'246'663'099'542), - 19 - 2  }, // * z^8
         -::boost::decimal::decimal64_t { UINT64_C(1'165'167'591'859'065'112), - 19 - 2  }, // * z^9
         -::boost::decimal::decimal64_t { UINT64_C(2'152'416'741'149'509'728), - 19 - 3  }, // * z^10
         +::boost::decimal::decimal64_t { UINT64_C(1'280'502'823'881'161'862), - 19 - 3  }, // * z^11
         -::boost::decimal::decimal64_t { UINT64_C(2'013'485'478'078'823'866), - 19 - 4  }, // * z^12
         -::boost::decimal::decimal64_t { UINT64_C(1'250'493'482'142'670'657), - 19 - 5  }, // * z^13
         +::boost::decimal::decimal64_t { UINT64_C(1'133'027'231'981'695'882), - 19 - 5  }, // * z^14
         -::boost::decimal::decimal64_t { UINT64_C(2'056'338'416'977'607'103), - 19 - 6  }, // * z^15
         +::boost::decimal::decimal64_t { UINT64_C(6'116'095'104'481'415'818), - 19 - 8  }, // * z^16
         +::boost::decimal::decimal64_t { UINT64_C(5'002'007'644'469'222'930), - 19 - 8  }, // * z^17
         -::boost::decimal::decimal64_t { UINT64_C(1'181'274'570'487'020'145), - 19 - 8  }, // * z^18
         +::boost::decimal::decimal64_t { UINT64_C(1'043'426'711'691'100'510), - 19 - 9  }, // * z^19
         +::boost::decimal::decimal64_t { UINT64_C(7'782'263'439'905'071'254), - 19 - 11 }, // * z^20
         -::boost::decimal::decimal64_t { UINT64_C(3'696'805'618'642'205'708), - 19 - 11 }, // * z^21
         +::boost::decimal::decimal64_t { UINT64_C(5'100'370'287'454'475'979), - 19 - 12 }, // * z^22
         -::boost::decimal::decimal64_t { UINT64_C(2'058'326'053'566'506'783), - 19 - 13 }, // * z^23
         -::boost::decimal::decimal64_t { UINT64_C(5'348'122'539'423'017'982), - 19 - 14 }, // * z^24
         +::boost::decimal::decimal64_t { UINT64_C(1'226'778'628'238'260'790), - 19 - 14 }, // * z^25
         -::boost::decimal::decimal64_t { UINT64_C(1'181'259'301'697'458'770), - 19 - 15 }, // * z^26
         +::boost::decimal::decimal64_t { UINT64_C(1'186'692'254'751'600'333), - 19 - 17 }, // * z^27
    }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // N[Series[1/Gamma[z], {z, 0, 27}], 19]
         +::boost::decimal::decimal_fast64_t { UINT64_C(5'772'156'649'015'328'606), - 19 - 0  }, // * z^2
         -::boost::decimal::decimal_fast64_t { UINT64_C(6'558'780'715'202'538'811), - 19 - 0  }, // * z^3
         -::boost::decimal::decimal_fast64_t { UINT64_C(4'200'263'503'409'523'553), - 19 - 1  }, // * z^4
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'665'386'113'822'914'895), - 19 - 0  }, // * z^5
         -::boost::decimal::decimal_fast64_t { UINT64_C(4'219'773'455'554'433'675), - 19 - 1  }, // * z^6
         -::boost::decimal::decimal_fast64_t { UINT64_C(9'621'971'527'876'973'562), - 19 - 2  }, // * z^7
         +::boost::decimal::decimal_fast64_t { UINT64_C(7'218'943'246'663'099'542), - 19 - 2  }, // * z^8
         -::boost::decimal::decimal_fast64_t { UINT64_C(1'165'167'591'859'065'112), - 19 - 2  }, // * z^9
         -::boost::decimal::decimal_fast64_t { UINT64_C(2'152'416'741'149'509'728), - 19 - 3  }, // * z^10
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'280'502'823'881'161'862), - 19 - 3  }, // * z^11
         -::boost::decimal::decimal_fast64_t { UINT64_C(2'013'485'478'078'823'866), - 19 - 4  }, // * z^12
         -::boost::decimal::decimal_fast64_t { UINT64_C(1'250'493'482'142'670'657), - 19 - 5  }, // * z^13
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'133'027'231'981'695'882), - 19 - 5  }, // * z^14
         -::boost::decimal::decimal_fast64_t { UINT64_C(2'056'338'416'977'607'103), - 19 - 6  }, // * z^15
         +::boost::decimal::decimal_fast64_t { UINT64_C(6'116'095'104'481'415'818), - 19 - 8  }, // * z^16
         +::boost::decimal::decimal_fast64_t { UINT64_C(5'002'007'644'469'222'930), - 19 - 8  }, // * z^17
         -::boost::decimal::decimal_fast64_t { UINT64_C(1'181'274'570'487'020'145), - 19 - 8  }, // * z^18
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'043'426'711'691'100'510), - 19 - 9  }, // * z^19
         +::boost::decimal::decimal_fast64_t { UINT64_C(7'782'263'439'905'071'254), - 19 - 11 }, // * z^20
         -::boost::decimal::decimal_fast64_t { UINT64_C(3'696'805'618'642'205'708), - 19 - 11 }, // * z^21
         +::boost::decimal::decimal_fast64_t { UINT64_C(5'100'370'287'454'475'979), - 19 - 12 }, // * z^22
         -::boost::decimal::decimal_fast64_t { UINT64_C(2'058'326'053'566'506'783), - 19 - 13 }, // * z^23
         -::boost::decimal::decimal_fast64_t { UINT64_C(5'348'122'539'423'017'982), - 19 - 14 }, // * z^24
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'226'778'628'238'260'790), - 19 - 14 }, // * z^25
         -::boost::decimal::decimal_fast64_t { UINT64_C(1'181'259'301'697'458'770), - 19 - 15 }, // * z^26
         +::boost::decimal::decimal_fast64_t { UINT64_C(1'186'692'254'751'600'333), - 19 - 17 }, // * z^27
     }};

    static constexpr d64_coeffs_asymp_t d64_coeffs_asymp =
    {{
         // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 14}], 19]
         +::boost::decimal::decimal64_t { UINT64_C(2506628274631000502), - 19 + 1  },
         +::boost::decimal::decimal64_t { UINT64_C(2088856895525833752), - 19 - 0  }, // / x
         +::boost::decimal::decimal64_t { UINT64_C(8703570398024307300), - 19 - 2  }, // / x^2
         -::boost::decimal::decimal64_t { UINT64_C(6721090474029881748), - 19 - 2  }, // / x^3
         -::boost::decimal::decimal64_t { UINT64_C(5752012381101712348), - 19 - 3  }, // / x^4
         +::boost::decimal::decimal64_t { UINT64_C(1965294881583203064), - 19 - 2  }, // / x^5
         +::boost::decimal::decimal64_t { UINT64_C(1747825212045591212), - 19 - 3  }, // / x^6
         -::boost::decimal::decimal64_t { UINT64_C(1484341135158276145), - 19 - 2  }, // / x^7
         -::boost::decimal::decimal64_t { UINT64_C(1296375732112554321), - 19 - 3  }, // / x^8
         +::boost::decimal::decimal64_t { UINT64_C(2104311229753206373), - 19 - 2  }, // / x^9
         +::boost::decimal::decimal64_t { UINT64_C(1805999456555504364), - 19 - 3  }, // / x^10
         -::boost::decimal::decimal64_t { UINT64_C(4798785670546346063), - 19 - 2  }, // / x^11
         -::boost::decimal::decimal64_t { UINT64_C(4073678593815251825), - 19 - 3  }, // / x^12
         +::boost::decimal::decimal64_t { UINT64_C(1605085033194459600), - 19 - 1  }, // / x^13
         +::boost::decimal::decimal64_t { UINT64_C(1353992280159094113), - 19 - 2  }, // / x^14
    }};

    static constexpr d64_fast_coeffs_asymp_t d64_fast_coeffs_asymp =
    {{
         // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 14}], 19]
         +::boost::decimal::decimal_fast64_t { UINT64_C(2506628274631000502), - 19 + 1  },
         +::boost::decimal::decimal_fast64_t { UINT64_C(2088856895525833752), - 19 - 0  }, // / x
         +::boost::decimal::decimal_fast64_t { UINT64_C(8703570398024307300), - 19 - 2  }, // / x^2
         -::boost::decimal::decimal_fast64_t { UINT64_C(6721090474029881748), - 19 - 2  }, // / x^3
         -::boost::decimal::decimal_fast64_t { UINT64_C(5752012381101712348), - 19 - 3  }, // / x^4
         +::boost::decimal::decimal_fast64_t { UINT64_C(1965294881583203064), - 19 - 2  }, // / x^5
         +::boost::decimal::decimal_fast64_t { UINT64_C(1747825212045591212), - 19 - 3  }, // / x^6
         -::boost::decimal::decimal_fast64_t { UINT64_C(1484341135158276145), - 19 - 2  }, // / x^7
         -::boost::decimal::decimal_fast64_t { UINT64_C(1296375732112554321), - 19 - 3  }, // / x^8
         +::boost::decimal::decimal_fast64_t { UINT64_C(2104311229753206373), - 19 - 2  }, // / x^9
         +::boost::decimal::decimal_fast64_t { UINT64_C(1805999456555504364), - 19 - 3  }, // / x^10
         -::boost::decimal::decimal_fast64_t { UINT64_C(4798785670546346063), - 19 - 2  }, // / x^11
         -::boost::decimal::decimal_fast64_t { UINT64_C(4073678593815251825), - 19 - 3  }, // / x^12
         +::boost::decimal::decimal_fast64_t { UINT64_C(1605085033194459600), - 19 - 1  }, // / x^13
         +::boost::decimal::decimal_fast64_t { UINT64_C(1353992280159094113), - 19 - 2  }, // / x^14
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
        // N[Series[1/Gamma[z], {z, 0, 46}], 36]
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(312909238939453), UINT64_C(7916302232898517972)  }, -34 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(355552215013931), UINT64_C(2875353717947891404)  }, -34 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(227696740770409), UINT64_C(1287992959696612036)  }, -35 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(90280762131699),  UINT64_C(14660682722320745466) }, -34 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(228754377395439), UINT64_C(1086189775515439306)  }, -35 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(521608121705894), UINT64_C(2882773517907923486)  }, -36 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(391339697554084), UINT64_C(12203646426790846826) }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(63163861720165),  UINT64_C(1793625582468481749)  }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(116682745342423), UINT64_C(7466931387917530902)  }, -37 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(69416197176288),  UINT64_C(17486507952476000235) }, -37 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(109151266480053), UINT64_C(14157573701904186532) }, -38 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(67789387500902),  UINT64_C(6337242598258275460)  }, -39 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(61421529319989),  UINT64_C(11330812743044278521) }, -39 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(111474328952626), UINT64_C(4349913604764276954)  }, -40 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(331554179970335), UINT64_C(8536598537651543980)  }, -42 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(271159377746131), UINT64_C(11232450780359262294) }, -42 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(64037022781195),  UINT64_C(7729482665838775386)  }, -42 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(56564275382244),  UINT64_C(15921046388084405946) }, -43 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(421877346419979), UINT64_C(12114109382397224706) }, -45 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(200404234149424), UINT64_C(17191629897693416576) }, -45 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(276491627306932), UINT64_C(18075235341994261118) }, -46 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(111582078948016), UINT64_C(1315679057212061374)  }, -47 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(289922303798056), UINT64_C(8236273575746269444)  }, -48 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(66503802694735),  UINT64_C(8619931044472680662)  }, -48 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(64036195058454),  UINT64_C(13570784405336680634) }, -49 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(64330716033670),  UINT64_C(6228121739584017954)  }, -51 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(76565308743615),  UINT64_C(9665163337994634860)  }, -51 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(124615253252825), UINT64_C(5713012462345318490)  }, -52 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(92938152937825),  UINT64_C(2160517649493992050)  }, -53 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(72497982578925),  UINT64_C(10055707640313829460) }, -55 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(111360223980902), UINT64_C(528747408384118098)   }, -55 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(148320486134320), UINT64_C(12662323637555269860) }, -56 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(93911231108772),  UINT64_C(8663955293807189228)  }, -57 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(127969413738636), UINT64_C(17978922200959991754) }, -59 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(101100927852914), UINT64_C(16158702556622869636) }, -59 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(120243204727301), UINT64_C(13141135468649758444) }, -60 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(70352901832557),  UINT64_C(2975454173305568482)  }, -61 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(64005738370342),  UINT64_C(18063645830042937300) }, -63 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(60963839731470),  UINT64_C(14965217315129705920) }, -63 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(69230926066837),  UINT64_C(16656915204960392533) }, -64 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(400691370795862), UINT64_C(16972369904241895558) }, -66 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(61514934723438),  UINT64_C(5918930041313493498)  }, -68 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(251487992814431), UINT64_C(6680121266003781724)  }, -68 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(289879709778175), UINT64_C(4432551928123929090)  }, -69 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(173905807485311), UINT64_C(17752316546962770214) }, -70 },
    }};

    static constexpr d128_coeffs_asymp_t d128_coeffs_asymp =
    {{
        // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 29}], 36]
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(135884591048426), UINT64_C(2199768757482254624)  }, -33 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(113237159207021), UINT64_C(14130970013708246594) }, -34 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(471821496695924), UINT64_C(464352157037447386)   }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(364351044670741), UINT64_C(6097570099755222654)  }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(311817215987699), UINT64_C(14568946901511994136) }, -37 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(106538849009357), UINT64_C(10090636838411945598) }, -36 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(94749794601238),  UINT64_C(6866971493329372072)  }, -37 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(80466294172410),  UINT64_C(2547924282344488810)  }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(70276669255695),  UINT64_C(16334355597894868319) }, -37 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(114074940344203), UINT64_C(9044723431924593842)  }, -36 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(97903426715255),  UINT64_C(16799883086492113070) }, -37 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(260142692464932), UINT64_C(15263500517507471568) }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(220834559071109), UINT64_C(9975868582270637886)  }, -37 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(87011834000670),  UINT64_C(1012280154922930780)  }, -35 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(73400068583854),  UINT64_C(10697903424322046536) }, -36 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(401238402683293), UINT64_C(16385890397153029532) }, -35 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(337230714209057), UINT64_C(16967592325356259778) }, -36 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(243967353836524), UINT64_C(9499344852909361366)  }, -34 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(204589376322286), UINT64_C(11872292347365127784) }, -35 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(189124322379112), UINT64_C(14090568112327257998) }, -33 },
        -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(158368431339348), UINT64_C(2168574764773383622)  }, -34 },
        +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(182057977444481), UINT64_C(3733389993208297254)  }, -32 },
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // N[Series[1/Gamma[z], {z, 0, 46}], 36]
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(312909238939453), UINT64_C(7916302232898517972)  }, -34 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(355552215013931), UINT64_C(2875353717947891404)  }, -34 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(227696740770409), UINT64_C(1287992959696612036)  }, -35 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(90280762131699),  UINT64_C(14660682722320745466) }, -34 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(228754377395439), UINT64_C(1086189775515439306)  }, -35 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(521608121705894), UINT64_C(2882773517907923486)  }, -36 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(391339697554084), UINT64_C(12203646426790846826) }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(63163861720165),  UINT64_C(1793625582468481749)  }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(116682745342423), UINT64_C(7466931387917530902)  }, -37 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(69416197176288),  UINT64_C(17486507952476000235) }, -37 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(109151266480053), UINT64_C(14157573701904186532) }, -38 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(67789387500902),  UINT64_C(6337242598258275460)  }, -39 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(61421529319989),  UINT64_C(11330812743044278521) }, -39 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(111474328952626), UINT64_C(4349913604764276954)  }, -40 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(331554179970335), UINT64_C(8536598537651543980)  }, -42 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(271159377746131), UINT64_C(11232450780359262294) }, -42 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(64037022781195),  UINT64_C(7729482665838775386)  }, -42 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(56564275382244),  UINT64_C(15921046388084405946) }, -43 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(421877346419979), UINT64_C(12114109382397224706) }, -45 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(200404234149424), UINT64_C(17191629897693416576) }, -45 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(276491627306932), UINT64_C(18075235341994261118) }, -46 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(111582078948016), UINT64_C(1315679057212061374)  }, -47 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(289922303798056), UINT64_C(8236273575746269444)  }, -48 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(66503802694735),  UINT64_C(8619931044472680662)  }, -48 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(64036195058454),  UINT64_C(13570784405336680634) }, -49 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(64330716033670),  UINT64_C(6228121739584017954)  }, -51 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(76565308743615),  UINT64_C(9665163337994634860)  }, -51 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(124615253252825), UINT64_C(5713012462345318490)  }, -52 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(92938152937825),  UINT64_C(2160517649493992050)  }, -53 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(72497982578925),  UINT64_C(10055707640313829460) }, -55 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(111360223980902), UINT64_C(528747408384118098)   }, -55 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(148320486134320), UINT64_C(12662323637555269860) }, -56 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(93911231108772),  UINT64_C(8663955293807189228)  }, -57 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(127969413738636), UINT64_C(17978922200959991754) }, -59 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(101100927852914), UINT64_C(16158702556622869636) }, -59 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(120243204727301), UINT64_C(13141135468649758444) }, -60 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(70352901832557),  UINT64_C(2975454173305568482)  }, -61 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(64005738370342),  UINT64_C(18063645830042937300) }, -63 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(60963839731470),  UINT64_C(14965217315129705920) }, -63 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(69230926066837),  UINT64_C(16656915204960392533) }, -64 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(400691370795862), UINT64_C(16972369904241895558) }, -66 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(61514934723438),  UINT64_C(5918930041313493498)  }, -68 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(251487992814431), UINT64_C(6680121266003781724)  }, -68 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(289879709778175), UINT64_C(4432551928123929090)  }, -69 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(173905807485311), UINT64_C(17752316546962770214) }, -70 },
    }};

    static constexpr d128_fast_coeffs_asymp_t d128_fast_coeffs_asymp =
    {{
        // N[Series[Gamma[x] Sqrt[x], {x, Infinity, 29}], 36]
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(135884591048426), UINT64_C(2199768757482254624)  }, -33 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(113237159207021), UINT64_C(14130970013708246594) }, -34 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(471821496695924), UINT64_C(464352157037447386)   }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(364351044670741), UINT64_C(6097570099755222654)  }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(311817215987699), UINT64_C(14568946901511994136) }, -37 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(106538849009357), UINT64_C(10090636838411945598) }, -36 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(94749794601238),  UINT64_C(6866971493329372072)  }, -37 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(80466294172410),  UINT64_C(2547924282344488810)  }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(70276669255695),  UINT64_C(16334355597894868319) }, -37 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(114074940344203), UINT64_C(9044723431924593842)  }, -36 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(97903426715255),  UINT64_C(16799883086492113070) }, -37 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(260142692464932), UINT64_C(15263500517507471568) }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(220834559071109), UINT64_C(9975868582270637886)  }, -37 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(87011834000670),  UINT64_C(1012280154922930780)  }, -35 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(73400068583854),  UINT64_C(10697903424322046536) }, -36 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(401238402683293), UINT64_C(16385890397153029532) }, -35 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(337230714209057), UINT64_C(16967592325356259778) }, -36 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(243967353836524), UINT64_C(9499344852909361366)  }, -34 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(204589376322286), UINT64_C(11872292347365127784) }, -35 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(189124322379112), UINT64_C(14090568112327257998) }, -33 },
        -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(158368431339348), UINT64_C(2168574764773383622)  }, -34 },
        +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(182057977444481), UINT64_C(3733389993208297254)  }, -32 },
    }};

};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b> constexpr typename tgamma_table_imp<b>::d32_coeffs_t  tgamma_table_imp<b>::d32_coeffs;
template <bool b> constexpr typename tgamma_table_imp<b>::d64_coeffs_t  tgamma_table_imp<b>::d64_coeffs;
template <bool b> constexpr typename tgamma_table_imp<b>::d128_coeffs_t tgamma_table_imp<b>::d128_coeffs;

template <bool b> constexpr typename tgamma_table_imp<b>::d32_fast_coeffs_t tgamma_table_imp<b>::d32_fast_coeffs;
template <bool b> constexpr typename tgamma_table_imp<b>::d64_fast_coeffs_t tgamma_table_imp<b>::d64_fast_coeffs;
template <bool b> constexpr typename tgamma_table_imp<b>::d128_fast_coeffs_t tgamma_table_imp<b>::d128_fast_coeffs;

template <bool b> constexpr typename tgamma_table_imp<b>::d32_coeffs_asymp_t  tgamma_table_imp<b>::d32_coeffs_asymp;
template <bool b> constexpr typename tgamma_table_imp<b>::d64_coeffs_asymp_t  tgamma_table_imp<b>::d64_coeffs_asymp;
template <bool b> constexpr typename tgamma_table_imp<b>::d128_coeffs_asymp_t tgamma_table_imp<b>::d128_coeffs_asymp;

template <bool b> constexpr typename tgamma_table_imp<b>::d32_fast_coeffs_asymp_t tgamma_table_imp<b>::d32_fast_coeffs_asymp;
template <bool b> constexpr typename tgamma_table_imp<b>::d64_fast_coeffs_asymp_t tgamma_table_imp<b>::d64_fast_coeffs_asymp;
template <bool b> constexpr typename tgamma_table_imp<b>::d128_fast_coeffs_asymp_t tgamma_table_imp<b>::d128_fast_coeffs_asymp;

#endif

} //namespace tgamma_detail

using tgamma_table = tgamma_detail::tgamma_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto tgamma_series_expansion(T z) noexcept;

template <>
constexpr auto tgamma_series_expansion<decimal32_t>(decimal32_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d32_coeffs);
}

template <>
constexpr auto tgamma_series_expansion<decimal_fast32_t>(decimal_fast32_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d32_fast_coeffs);
}

template <>
constexpr auto tgamma_series_expansion<decimal64_t>(decimal64_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d64_coeffs);
}

template <>
constexpr auto tgamma_series_expansion<decimal_fast64_t>(decimal_fast64_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d64_fast_coeffs);
}

template <>
constexpr auto tgamma_series_expansion<decimal128_t>(decimal128_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d128_coeffs);
}

template <>
constexpr auto tgamma_series_expansion<decimal_fast128_t>(decimal_fast128_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d128_fast_coeffs);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto tgamma_series_expansion_asymp(T z) noexcept;

template <>
constexpr auto tgamma_series_expansion_asymp<decimal32_t>(decimal32_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d32_coeffs_asymp);
}

template <>
constexpr auto tgamma_series_expansion_asymp<decimal_fast32_t>(decimal_fast32_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d32_fast_coeffs_asymp);
}

template <>
constexpr auto tgamma_series_expansion_asymp<decimal64_t>(decimal64_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d64_coeffs_asymp);
}

template <>
constexpr auto tgamma_series_expansion_asymp<decimal_fast64_t>(decimal_fast64_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d64_fast_coeffs_asymp);
}

template <>
constexpr auto tgamma_series_expansion_asymp<decimal128_t>(decimal128_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d128_coeffs_asymp);
}

template <>
constexpr auto tgamma_series_expansion_asymp<decimal_fast128_t>(decimal_fast128_t z) noexcept
{
    return taylor_series_result(z, tgamma_table::d128_fast_coeffs_asymp);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_TGAMMA_IMPL_HPP
