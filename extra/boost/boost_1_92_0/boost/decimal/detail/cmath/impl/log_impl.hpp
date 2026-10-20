// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG_IMPL_HPP

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

namespace log_detail {

template <bool b>
struct log_table_imp
{
private:
    using d32_coeffs_t  = std::array<decimal32_t,   8>;
    using d64_coeffs_t  = std::array<decimal64_t,  11>;
    using d128_coeffs_t = std::array<decimal128_t, 21>;

    using d32_fast_coeffs_t  = std::array<decimal_fast32_t,   8>;
    using d64_fast_coeffs_t  = std::array<decimal_fast64_t,  11>;
    using d128_fast_coeffs_t = std::array<decimal_fast128_t, 21>;

public:
    static constexpr d32_coeffs_t d32_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 17}]
         //            (1),                                                       // * z
         ::boost::decimal::decimal32_t { UINT64_C(8333333333333333333), - 19 - 1 }, // * z^3
         ::boost::decimal::decimal32_t { UINT64_C(1250000000000000000), - 19 - 1 }, // * z^5
         ::boost::decimal::decimal32_t { UINT64_C(2232142857142857143), - 19 - 2 }, // * z^7
         ::boost::decimal::decimal32_t { UINT64_C(4340277777777777778), - 19 - 3 }, // * z^9
         ::boost::decimal::decimal32_t { UINT64_C(8877840909090909091), - 19 - 4 }, // * z^11
         ::boost::decimal::decimal32_t { UINT64_C(1878004807692307692), - 19 - 4 }, // * z^13
         ::boost::decimal::decimal32_t { UINT64_C(4069010416666666667), - 19 - 5 }, // * z^15
         ::boost::decimal::decimal32_t { UINT64_C(8975758272058823529), - 19 - 6 }, // * z^17
    }};

    static constexpr d32_fast_coeffs_t d32_fast_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 17}]
         //            (1),                                                       // * z
         ::boost::decimal::decimal_fast32_t { UINT64_C(8333333333333333333), - 19 - 1 }, // * z^3
         ::boost::decimal::decimal_fast32_t { UINT64_C(1250000000000000000), - 19 - 1 }, // * z^5
         ::boost::decimal::decimal_fast32_t { UINT64_C(2232142857142857143), - 19 - 2 }, // * z^7
         ::boost::decimal::decimal_fast32_t { UINT64_C(4340277777777777778), - 19 - 3 }, // * z^9
         ::boost::decimal::decimal_fast32_t { UINT64_C(8877840909090909091), - 19 - 4 }, // * z^11
         ::boost::decimal::decimal_fast32_t { UINT64_C(1878004807692307692), - 19 - 4 }, // * z^13
         ::boost::decimal::decimal_fast32_t { UINT64_C(4069010416666666667), - 19 - 5 }, // * z^15
         ::boost::decimal::decimal_fast32_t { UINT64_C(8975758272058823529), - 19 - 6 }, // * z^17
     }};

    static constexpr d64_coeffs_t d64_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 23}]
         //            (1),                                                       // * z
         ::boost::decimal::decimal64_t { UINT64_C(8333333333333333333), - 19 - 1 }, // * z^3
         ::boost::decimal::decimal64_t { UINT64_C(1250000000000000000), - 19 - 1 }, // * z^5
         ::boost::decimal::decimal64_t { UINT64_C(2232142857142857143), - 19 - 2 }, // * z^7
         ::boost::decimal::decimal64_t { UINT64_C(4340277777777777778), - 19 - 3 }, // * z^9
         ::boost::decimal::decimal64_t { UINT64_C(8877840909090909091), - 19 - 4 }, // * z^11
         ::boost::decimal::decimal64_t { UINT64_C(1878004807692307692), - 19 - 4 }, // * z^13
         ::boost::decimal::decimal64_t { UINT64_C(4069010416666666667), - 19 - 5 }, // * z^15
         ::boost::decimal::decimal64_t { UINT64_C(8975758272058823529), - 19 - 6 }, // * z^17
         ::boost::decimal::decimal64_t { UINT64_C(2007735402960526316), - 19 - 6 }, // * z^19
         ::boost::decimal::decimal64_t { UINT64_C(4541306268601190476), - 19 - 7 }, // * z^21
         ::boost::decimal::decimal64_t { UINT64_C(1036602517832880435), - 19 - 7 }, // * z^23
     }};

    static constexpr d64_fast_coeffs_t d64_fast_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 23}]
         //            (1),                                                       // * z
         ::boost::decimal::decimal_fast64_t { UINT64_C(8333333333333333333), - 19 - 1 }, // * z^3
         ::boost::decimal::decimal_fast64_t { UINT64_C(1250000000000000000), - 19 - 1 }, // * z^5
         ::boost::decimal::decimal_fast64_t { UINT64_C(2232142857142857143), - 19 - 2 }, // * z^7
         ::boost::decimal::decimal_fast64_t { UINT64_C(4340277777777777778), - 19 - 3 }, // * z^9
         ::boost::decimal::decimal_fast64_t { UINT64_C(8877840909090909091), - 19 - 4 }, // * z^11
         ::boost::decimal::decimal_fast64_t { UINT64_C(1878004807692307692), - 19 - 4 }, // * z^13
         ::boost::decimal::decimal_fast64_t { UINT64_C(4069010416666666667), - 19 - 5 }, // * z^15
         ::boost::decimal::decimal_fast64_t { UINT64_C(8975758272058823529), - 19 - 6 }, // * z^17
         ::boost::decimal::decimal_fast64_t { UINT64_C(2007735402960526316), - 19 - 6 }, // * z^19
         ::boost::decimal::decimal_fast64_t { UINT64_C(4541306268601190476), - 19 - 7 }, // * z^21
         ::boost::decimal::decimal_fast64_t { UINT64_C(1036602517832880435), - 19 - 7 }, // * z^23
     }};

    static constexpr d128_coeffs_t d128_coeffs =
    {{
         // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 43}]
         //            (1),                                                                                                                   // * z
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -35 }, // * z^3
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(67762635780344),  UINT64_C(500376525493764096)   }, -35 }, // * z^5
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(121004706750614), UINT64_C(6164027816584450626)  }, -36 }, // * z^7
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(235286929792861), UINT64_C(3787056721709964394)  }, -37 }, // * z^9
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(481268720030852), UINT64_C(8584740752302634068)  }, -38 }, // * z^11
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(101806844621911), UINT64_C(1816002851448634124)  }, -38 }, // * z^13
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(220581496680807), UINT64_C(7009130190423632548)  }, -39 }, // * z^15
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(486576830913545), UINT64_C(12748560115094843630) }, -40 }, // * z^17
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(108839554283293), UINT64_C(2123490654414259032)  }, -40 }, // * z^19
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(246184706116972), UINT64_C(9634423737622849438)  }, -41 }, // * z^21
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(56194335091917),  UINT64_C(11823550152479764302) }, -41 }, // * z^23
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(129246970711410), UINT64_C(10592095684364861440) }, -42 }, // * z^25
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(299182802572709), UINT64_C(12220910627630811506) }, -43 }, // * z^27
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(69637376460889),  UINT64_C(5865971761607874066)  }, -43 }, // * z^29
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(162861606239176), UINT64_C(11636108014793143194) }, -44 }, // * z^31
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(382478014652611), UINT64_C(14470401740943906374) }, -45 }, // * z^33
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(90155532025258),  UINT64_C(9076666090147568782)  }, -45 }, // * z^35
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(213205650059732), UINT64_C(16978042870933143358) }, -46 }, // * z^37
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(505680067449366), UINT64_C(9996854995997550184)  }, -47 }, // * z^39
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(120253186771495), UINT64_C(12950434681540257980) }, -47 }, // * z^41
         ::boost::decimal::decimal128_t { boost::int128::uint128_t { UINT64_C(286650038234379), UINT64_C(5345076336561816786)  }, -48 }, // * z^43
    }};

    static constexpr d128_fast_coeffs_t d128_fast_coeffs =
    {{
        // Series[Log[(1 + (z/2))/(1 - (z/2))], {z, 0, 43}]
        //            (1),                                                                                                                   // * z
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(451750905202293), UINT64_C(9484758194528277842)  }, -35 }, // * z^3
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(67762635780344),  UINT64_C(500376525493764096)   }, -35 }, // * z^5
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(121004706750614), UINT64_C(6164027816584450626)  }, -36 }, // * z^7
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(235286929792861), UINT64_C(3787056721709964394)  }, -37 }, // * z^9
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(481268720030852), UINT64_C(8584740752302634068)  }, -38 }, // * z^11
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(101806844621911), UINT64_C(1816002851448634124)  }, -38 }, // * z^13
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(220581496680807), UINT64_C(7009130190423632548)  }, -39 }, // * z^15
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(486576830913545), UINT64_C(12748560115094843630) }, -40 }, // * z^17
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(108839554283293), UINT64_C(2123490654414259032)  }, -40 }, // * z^19
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(246184706116972), UINT64_C(9634423737622849438)  }, -41 }, // * z^21
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(56194335091917),  UINT64_C(11823550152479764302) }, -41 }, // * z^23
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(129246970711410), UINT64_C(10592095684364861440) }, -42 }, // * z^25
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(299182802572709), UINT64_C(12220910627630811506) }, -43 }, // * z^27
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(69637376460889),  UINT64_C(5865971761607874066)  }, -43 }, // * z^29
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(162861606239176), UINT64_C(11636108014793143194) }, -44 }, // * z^31
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(382478014652611), UINT64_C(14470401740943906374) }, -45 }, // * z^33
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(90155532025258),  UINT64_C(9076666090147568782)  }, -45 }, // * z^35
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(213205650059732), UINT64_C(16978042870933143358) }, -46 }, // * z^37
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(505680067449366), UINT64_C(9996854995997550184)  }, -47 }, // * z^39
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(120253186771495), UINT64_C(12950434681540257980) }, -47 }, // * z^41
        ::boost::decimal::decimal_fast128_t { boost::int128::uint128_t { UINT64_C(286650038234379), UINT64_C(5345076336561816786)  }, -48 }, // * z^43
    }};
};

#if !(defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L) && (!defined(_MSC_VER) || _MSC_VER != 1900)

template <bool b>
constexpr typename log_table_imp<b>::d32_coeffs_t log_table_imp<b>::d32_coeffs;

template <bool b>
constexpr typename log_table_imp<b>::d64_coeffs_t log_table_imp<b>::d64_coeffs;

template <bool b>
constexpr typename log_table_imp<b>::d128_coeffs_t log_table_imp<b>::d128_coeffs;

template <bool b>
constexpr typename log_table_imp<b>::d32_fast_coeffs_t log_table_imp<b>::d32_fast_coeffs;

template <bool b>
constexpr typename log_table_imp<b>::d64_fast_coeffs_t log_table_imp<b>::d64_fast_coeffs;

template <bool b>
constexpr typename log_table_imp<b>::d128_fast_coeffs_t log_table_imp<b>::d128_fast_coeffs;

#endif

} //namespace log_detail

using log_table = log_detail::log_table_imp<true>;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto log_series_expansion(T z2) noexcept;

template <>
constexpr auto log_series_expansion<decimal32_t>(decimal32_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d32_coeffs);
}

template <>
constexpr auto log_series_expansion<decimal_fast32_t>(decimal_fast32_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d32_fast_coeffs);
}

template <>
constexpr auto log_series_expansion<decimal64_t>(decimal64_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d64_coeffs);
}

template <>
constexpr auto log_series_expansion<decimal_fast64_t>(decimal_fast64_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d64_fast_coeffs);
}

template <>
constexpr auto log_series_expansion<decimal128_t>(decimal128_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d128_coeffs);
}

template <>
constexpr auto log_series_expansion<decimal_fast128_t>(decimal_fast128_t z2) noexcept
{
    return taylor_series_result(z2, log_table::d128_fast_coeffs);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_LOG_IMPL_HPP
