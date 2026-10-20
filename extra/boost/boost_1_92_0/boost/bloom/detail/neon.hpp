/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_NEON_HPP
#define BOOST_BLOOM_DETAIL_NEON_HPP

#if defined(__ARM_NEON)&&!defined(__ARM_BIG_ENDIAN)
#define BOOST_BLOOM_LITTLE_ENDIAN_NEON
#endif

#if defined(BOOST_BLOOM_LITTLE_ENDIAN_NEON)
#include <arm_neon.h>
#endif

#endif
