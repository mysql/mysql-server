/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_SSE2_HPP
#define BOOST_BLOOM_DETAIL_SSE2_HPP

#if defined(__SSE2__)|| \
    defined(_M_X64)||(defined(_M_IX86_FP)&&_M_IX86_FP>=2)
#define BOOST_BLOOM_SSE2
#endif

#if defined(BOOST_BLOOM_SSE2)
#include <emmintrin.h>
#endif

#endif
