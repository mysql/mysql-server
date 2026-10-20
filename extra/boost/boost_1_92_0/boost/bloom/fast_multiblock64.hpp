/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_FAST_MULTIBLOCK64_HPP
#define BOOST_BLOOM_FAST_MULTIBLOCK64_HPP

#include <boost/bloom/detail/avx2.hpp>

#if defined(BOOST_BLOOM_AVX2)
#include <boost/bloom/detail/fast_multiblock64_avx2.hpp>
#else /* fallback */
#include <boost/bloom/multiblock.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

template<std::size_t K>
using fast_multiblock64=multiblock<std::uint64_t,K>;

} /* namespace bloom */
} /* namespace boost */
#endif

#endif
