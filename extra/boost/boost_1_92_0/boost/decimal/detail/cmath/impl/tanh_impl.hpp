// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_TANH_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_TANH_IMPL_HPP

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

namespace tanh_detail {

template <bool b>
struct tanh_table_imp
{
private:
    using d32_coeffs_t  = std::array<decimal32_t,   7>;
    using d64_coeffs_t  = std::array<decimal64_t,  11>;
    using d128_coeffs_t = std::array<decimal128_t, 22>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,   7>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  11>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 22>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 15}]
         //            (1),                                                        // * x
         -::boost::decimal::decimal32_t { UINT64_C(3333333333333333333), - 19 - 0 }, // * x^3
         +::boost::decimal::decimal32_t { UINT64_C(1333333333333333333), - 19 - 0 }, // * x^5
         -::boost::decimal::decimal32_t { UINT64_C(5396825396825396825), - 19 - 1 }, // * x^7
         +::boost::decimal::decimal32_t { UINT64_C(2186948853615520282), - 19 - 1 }, // * x^9
         -::boost::decimal::decimal32_t { UINT64_C(8863235529902196569), - 19 - 2 }, // * x^11
         +::boost::decimal::decimal32_t { UINT64_C(3592128036572481017), - 19 - 2 }, // * x^13
         -::boost::decimal::decimal32_t { UINT64_C(1455834387051318268), - 19 - 2 }, // * x^15
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 15}]
         //            (1),                                                        // * x
         -::boost::decimal::decimal_fast32_t { UINT64_C(3333333333333333333), - 19 - 0 }, // * x^3
         +::boost::decimal::decimal_fast32_t { UINT64_C(1333333333333333333), - 19 - 0 }, // * x^5
         -::boost::decimal::decimal_fast32_t { UINT64_C(5396825396825396825), - 19 - 1 }, // * x^7
         +::boost::decimal::decimal_fast32_t { UINT64_C(2186948853615520282), - 19 - 1 }, // * x^9
         -::boost::decimal::decimal_fast32_t { UINT64_C(8863235529902196569), - 19 - 2 }, // * x^11
         +::boost::decimal::decimal_fast32_t { UINT64_C(3592128036572481017), - 19 - 2 }, // * x^13
         -::boost::decimal::decimal_fast32_t { UINT64_C(1455834387051318268), - 19 - 2 }, // * x^15
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 23}]
         //            (1),                                                        // * x
         -::boost::decimal::decimal64_t { UINT64_C(3333333333333333333), - 19 - 0 }, // * x^3
         +::boost::decimal::decimal64_t { UINT64_C(1333333333333333333), - 19 - 0 }, // * x^5
         -::boost::decimal::decimal64_t { UINT64_C(5396825396825396825), - 19 - 1 }, // * x^7
         +::boost::decimal::decimal64_t { UINT64_C(2186948853615520282), - 19 - 1 }, // * x^9
         -::boost::decimal::decimal64_t { UINT64_C(8863235529902196569), - 19 - 2 }, // * x^11
         +::boost::decimal::decimal64_t { UINT64_C(3592128036572481017), - 19 - 2 }, // * x^13
         -::boost::decimal::decimal64_t { UINT64_C(1455834387051318268), - 19 - 2 }, // * x^15
         +::boost::decimal::decimal64_t { UINT64_C(5900274409455859814), - 19 - 3 }, // * x^17
         -::boost::decimal::decimal64_t { UINT64_C(2391291142435524815), - 19 - 3 }  // * x^19
         +::boost::decimal::decimal64_t { UINT64_C(9691537956929450326), - 19 - 4 }  // * x^21
         -::boost::decimal::decimal64_t { UINT64_C(3927832388331683405), - 19 - 4 }  // * x^23
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 23}]
         //            (1),                                                        // * x
         -::boost::decimal::decimal_fast64_t { UINT64_C(3333333333333333333), - 19 - 0 }, // * x^3
         +::boost::decimal::decimal_fast64_t { UINT64_C(1333333333333333333), - 19 - 0 }, // * x^5
         -::boost::decimal::decimal_fast64_t { UINT64_C(5396825396825396825), - 19 - 1 }, // * x^7
         +::boost::decimal::decimal_fast64_t { UINT64_C(2186948853615520282), - 19 - 1 }, // * x^9
         -::boost::decimal::decimal_fast64_t { UINT64_C(8863235529902196569), - 19 - 2 }, // * x^11
         +::boost::decimal::decimal_fast64_t { UINT64_C(3592128036572481017), - 19 - 2 }, // * x^13
         -::boost::decimal::decimal_fast64_t { UINT64_C(1455834387051318268), - 19 - 2 }, // * x^15
         +::boost::decimal::decimal_fast64_t { UINT64_C(5900274409455859814), - 19 - 3 }, // * x^17
         -::boost::decimal::decimal_fast64_t { UINT64_C(2391291142435524815), - 19 - 3 }  // * x^19
         +::boost::decimal::decimal_fast64_t { UINT64_C(9691537956929450326), - 19 - 4 }  // * x^21
         -::boost::decimal::decimal_fast64_t { UINT64_C(3927832388331683405), - 19 - 4 }  // * x^23
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 45}]
         //            (1),                                                                                                                    // * x
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -34 }, // * x^3
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(72280144832366),  UINT64_C(17750696095988929874) }, -34 }, // * x^5
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(292562490988151), UINT64_C(18264656174417923374) }, -35 }, // * x^7
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(118554734910231), UINT64_C(9692136079832632224)  }, -35 }, // * x^9
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(480476960838533), UINT64_C(11637084576724682862) }, -36 }, // * x^11
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(194729651054898), UINT64_C(12399216788389290642) }, -36 }, // * x^13
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(78920940261007),  UINT64_C(1837289537202064798)  }, -36 }, // * x^15
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(319854516649633), UINT64_C(9164231904936043282)  }, -37 }, // * x^17
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(129632152583696), UINT64_C(18287072186516798384) }, -37 }, // * x^19
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(525379325381431), UINT64_C(15812750027817946304) }, -38 }, // * x^21
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(212928220429406), UINT64_C(17197028753568092234) }, -38 }, // * x^23
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(86296557298784),  UINT64_C(15970280607199622056) }, -38 }, // * x^25
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(349746773190745), UINT64_C(16747878178724308630) }, -39 }, // * x^27
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(141747028139092), UINT64_C(18150782588055037068) }, -39 }, // * x^29
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(57447906675346),  UINT64_C(7219349788452465642)  }, -39 }, // * x^31
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(232827596084826), UINT64_C(637032669719018454)   }, -40 }, // * x^33
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(94361470479658),  UINT64_C(14765552217602302942) }, -40 }, // * x^35
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(382432635169221), UINT64_C(13797141926350469414) }, -41 }, // * x^37
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(154994109035215), UINT64_C(9538263982529984200)  }, -41 }, // * x^39
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(62816746340150),  UINT64_C(7041711327684726779)  }, -41 }, // * x^41
         -::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(254586683669781), UINT64_C(685051056553551574)   }, -42 }, // * x^43
         +::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(103180096515998), UINT64_C(10260917890244709612) }, -42 }, // * x^45
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
         // Series[Tanh[x], {x, 0, 45}]
         //            (1),                                                                                                                    // * x
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(180700362080917), UINT64_C(7483252092553221458)  }, -34 }, // * x^3
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(72280144832366),  UINT64_C(17750696095988929874) }, -34 }, // * x^5
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(292562490988151), UINT64_C(18264656174417923374) }, -35 }, // * x^7
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(118554734910231), UINT64_C(9692136079832632224)  }, -35 }, // * x^9
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(480476960838533), UINT64_C(11637084576724682862) }, -36 }, // * x^11
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(194729651054898), UINT64_C(12399216788389290642) }, -36 }, // * x^13
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(78920940261007),  UINT64_C(1837289537202064798)  }, -36 }, // * x^15
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(319854516649633), UINT64_C(9164231904936043282)  }, -37 }, // * x^17
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(129632152583696), UINT64_C(18287072186516798384) }, -37 }, // * x^19
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(525379325381431), UINT64_C(15812750027817946304) }, -38 }, // * x^21
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(212928220429406), UINT64_C(17197028753568092234) }, -38 }, // * x^23
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(86296557298784),  UINT64_C(15970280607199622056) }, -38 }, // * x^25
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(349746773190745), UINT64_C(16747878178724308630) }, -39 }, // * x^27
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(141747028139092), UINT64_C(18150782588055037068) }, -39 }, // * x^29
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(57447906675346),  UINT64_C(7219349788452465642)  }, -39 }, // * x^31
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(232827596084826), UINT64_C(637032669719018454)   }, -40 }, // * x^33
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(94361470479658),  UINT64_C(14765552217602302942) }, -40 }, // * x^35
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(382432635169221), UINT64_C(13797141926350469414) }, -41 }, // * x^37
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(154994109035215), UINT64_C(9538263982529984200)  }, -41 }, // * x^39
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(62816746340150),  UINT64_C(7041711327684726779)  }, -41 }, // * x^41
         -::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(254586683669781), UINT64_C(685051056553551574)   }, -42 }, // * x^43
         +::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(103180096515998), UINT64_C(10260917890244709612) }, -42 }, // * x^45
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename tanh_table_imp<b>::d32_coeffs_t tanh_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename tanh_table_imp<b>::d64_coeffs_t tanh_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename tanh_table_imp<b>::d128_coeffs_t tanh_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename tanh_table_imp<b>::d32_fast_coeffs_t tanh_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename tanh_table_imp<b>::d64_fast_coeffs_t tanh_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename tanh_table_imp<b>::d128_fast_coeffs_t tanh_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace tanh_detail

using tanh_table = tanh_detail::tanh_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto tanh_series_expansion(T z2) noexcept;

template <>
constexpr auto tanh_series_expansion<decimal32_t>(decimal32_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d32_coeffs);
}

template <>
constexpr auto tanh_series_expansion<decimal_fast32_t>(decimal_fast32_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d32_fast_coeffs);
}

template <>
constexpr auto tanh_series_expansion<decimal64_t>(decimal64_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d64_coeffs);
}

template <>
constexpr auto tanh_series_expansion<decimal_fast64_t>(decimal_fast64_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d64_fast_coeffs);
}

template <>
constexpr auto tanh_series_expansion<decimal128_t>(decimal128_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d128_coeffs);
}

template <>
constexpr auto tanh_series_expansion<decimal_fast128_t>(decimal_fast128_t z2) noexcept
{
    return taylor_series_result(z2, tanh_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_TANH_IMPL_HPP
