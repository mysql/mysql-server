/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#if BOOST_WORKAROUND(BOOST_GCC_VERSION,>=40900)||\
    BOOST_WORKAROUND(BOOST_CLANG,>=1)&&\
    (__clang_major__>3 || __clang_major__==3 && __clang_minor__ >= 8)
/* https://github.com/boostorg/poly_collection/issues/15 */
  
#define BOOST_POLY_COLLECTION_INSIDE_NO_SANITIZE
#define BOOST_POLY_COLLECTION_NO_SANITIZE \
__attribute__((no_sanitize("undefined")))

/* UBSan seems not to be supported in some environments */
#if defined(BOOST_GCC_VERSION)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#elif defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wattributes"
#endif

#else

#define BOOST_POLY_COLLECTION_NO_SANITIZE

#endif
