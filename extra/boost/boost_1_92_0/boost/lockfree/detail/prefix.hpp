//  Copyright (C) 2009, 2016 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_PREFIX_HPP_INCLUDED
#define BOOST_LOCKFREE_PREFIX_HPP_INCLUDED

#include <boost/predef.h>

/* this file defines the following macros:
   BOOST_LOCKFREE_PTR_COMPRESSION: use tag/pointer compression to utilize parts
                                   of the virtual address space as tag (at least 16bit)
*/

namespace boost { namespace lockfree { namespace detail {

#ifdef __cpp_inline_variables
#    define inline_constexpr inline
#else
#    define inline_constexpr
#endif

#if BOOST_ARCH_SYS390
inline_constexpr constexpr size_t cacheline_bytes = 256;
#elif BOOST_ARCH_PPC
inline_constexpr constexpr size_t cacheline_bytes = 128;
#elif BOOST_ARCH_ARM && ( BOOST_OS_MACOS || BOOST_OS_IOS )
// technically this is for apple's the M chips, but the A chip are probably similar
inline_constexpr constexpr size_t cacheline_bytes = 128;
#else
inline_constexpr constexpr size_t cacheline_bytes = 64;
#endif

}}} // namespace boost::lockfree::detail

#if BOOST_ARCH_X86_64 || ( ( BOOST_ARCH_ARM >= BOOST_VERSION_NUMBER( 8, 0, 0 ) ) && !BOOST_PLAT_ANDROID )
#    define BOOST_LOCKFREE_PTR_COMPRESSION 1
#endif

#undef inline_constexpr

#endif /* BOOST_LOCKFREE_PREFIX_HPP_INCLUDED */
