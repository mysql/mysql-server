/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_AVX2_HPP
#define BOOST_BLOOM_DETAIL_AVX2_HPP

#if defined(__AVX2__)
#define BOOST_BLOOM_AVX2
#endif

#if defined(BOOST_BLOOM_AVX2)
#include <immintrin.h>
#endif

#endif
