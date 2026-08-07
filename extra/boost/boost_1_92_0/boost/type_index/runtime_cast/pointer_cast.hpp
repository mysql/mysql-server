//
// Copyright (c) Chris Glover, 2016.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_RUNTIME_CAST_POINTER_CAST_HPP
#define BOOST_TYPE_INDEX_RUNTIME_CAST_POINTER_CAST_HPP

/// \file pointer_class.hpp
/// \brief Contains the function overloads of boost::typeindex::runtime_cast for
/// pointer types.

#include <boost/type_index/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#include <boost/type_index.hpp>
#include <boost/type_index/runtime_cast/detail/runtime_cast_impl.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

namespace boost { namespace typeindex {

BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT

/// \brief Safely converts pointers to classes up, down, and sideways along the inheritance hierarchy.
/// \tparam T The desired target type. Like dynamic_cast, must be a pointer to complete class type.
/// \tparam U A complete class type of the source instance, u.
/// \return If there exists a valid conversion from U* to T, returns a T that points to
/// an address suitably offset from u. If no such conversion exists, returns nullptr.
template<typename T, typename U>
T runtime_cast(U* u) noexcept {
    using impl_type = typename std::remove_pointer<T>::type;
    return detail::runtime_cast_impl<impl_type>(u, std::is_base_of<T, U>());
}

/// \brief Safely converts pointers to classes up, down, and sideways along the inheritance hierarchy.
/// \tparam T The desired target type. Like dynamic_cast, must be a pointer to complete class type.
/// \tparam U A complete class type of the source instance, u.
/// \return If there exists a valid conversion from U* to T, returns a T that points to
/// an address suitably offset from u. If no such conversion exists, returns nullptr.
template<typename T, typename U>
T runtime_cast(U const* u) noexcept {
    using impl_type = typename std::remove_pointer<T>::type;
    return detail::runtime_cast_impl<impl_type>(u, std::is_base_of<T, U>());
}

/// \brief Safely converts pointers to classes up, down, and sideways along the inheritance
/// hierarchy.
/// \tparam T The desired target type to return a pointer to.
/// \tparam U A complete class type of the source instance, u.
/// \return If there exists a valid conversion from U const* to T*, returns a T*
/// that points to an address suitably offset from u.
/// If no such conversion exists, returns nullptr.
template<typename T, typename U>
T* runtime_pointer_cast(U* u) noexcept {
    return detail::runtime_cast_impl<T>(u, std::is_base_of<T, U>());
}

/// \brief Safely converts pointers to classes up, down, and sideways along the inheritance
/// hierarchy.
/// \tparam T The desired target type to return a pointer to.
/// \tparam U A complete class type of the source instance, u.
/// \return If there exists a valid conversion from U const* to T const*, returns a T const*
/// that points to an address suitably offset from u.
/// If no such conversion exists, returns nullptr.
template<typename T, typename U>
T const* runtime_pointer_cast(U const* u) noexcept {
    return detail::runtime_cast_impl<T>(u, std::is_base_of<T, U>());
}

BOOST_TYPE_INDEX_END_MODULE_EXPORT

}} // namespace boost::typeindex

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#endif // BOOST_TYPE_INDEX_RUNTIME_CAST_POINTER_CAST_HPP
