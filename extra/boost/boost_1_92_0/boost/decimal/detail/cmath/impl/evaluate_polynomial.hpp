//  (C) Copyright John Maddock 2006.
//  (C) Copyright Matt Borland 2023-2024
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_HPP

#include <boost/decimal/detail/config.hpp>

namespace boost {
namespace decimal {
namespace tools {

//
// Polynomial evaluation with runtime size.
// This requires a for-loop which may be more expensive than
// the loop expanded versions above:
//
template <typename T, typename U>
constexpr U evaluate_polynomial(const T& poly, const U& z) noexcept
{
    const auto count {poly.size()};
    auto sum = static_cast<U>(poly[count - 1]);
    for(int i = static_cast<int>(count) - 2; i >= 0; --i)
    {
        sum *= z;
        sum += static_cast<U>(poly[static_cast<std::size_t>(i)]);
    }

    return sum;
}

} //namespace tools
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_HPP
