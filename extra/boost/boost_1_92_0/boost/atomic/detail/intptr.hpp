/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/intptr.hpp
 *
 * This header defines (u)intptr_t types.
 */

#ifndef BOOST_ATOMIC_DETAIL_INTPTR_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_INTPTR_HPP_INCLUDED_

#include <cstdint>
#if !defined(UINTPTR_MAX)
#include <cstddef>
#endif
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

#if defined(UINTPTR_MAX)
using std::uintptr_t;
using std::intptr_t;
#else
using uintptr_t = std::size_t;
using intptr_t = std::ptrdiff_t;
#endif

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_INTPTR_HPP_INCLUDED_
