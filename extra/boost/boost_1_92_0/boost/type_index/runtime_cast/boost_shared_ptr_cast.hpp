//
// Copyright (c) Chris Glover, 2016.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_RUNTIME_CAST_BOOST_SHARED_PTR_CAST_HPP
#define BOOST_TYPE_INDEX_RUNTIME_CAST_BOOST_SHARED_PTR_CAST_HPP

/// \file boost_shared_ptr_cast.hpp
/// \brief Contains the overload of boost::typeindex::runtime_pointer_cast for
/// boost::shared_ptr types.

#include <boost/type_index/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#include <boost/type_index/runtime_cast/detail/runtime_cast_impl.hpp>

#if !defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)
#ifndef BOOST_TYPE_INDEX_USE_IMPORT_STD
#include <type_traits>
#endif
#endif

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

namespace boost { namespace typeindex {

BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT

/// \brief Creates a new instance of smart pointer whose stored pointer is obtained from u's
/// stored pointer using a runtime_cast.
///
/// The new shared_ptr will share ownership with u, except that it is empty if the runtime_cast
/// performed by runtime_pointer_cast returns a null pointer.
/// \tparam T The desired target type to return a pointer of.
/// \tparam U A complete class type of the source instance pointed to from u.
/// \return If there exists a valid conversion from U* to T*, returns a boost::shared_ptr<T>
/// that points to an address suitably offset from u.
/// If no such conversion exists, returns boost::shared_ptr<T>();
template<typename T, typename U,  template <class> class SmartPointer>
auto runtime_pointer_cast(SmartPointer<U> const& u) -> decltype(u.use_count(), SmartPointer<T>()) {
    T* value = detail::runtime_cast_impl<T>(u.get(), std::is_base_of<T, U>());
    if(value)
        return SmartPointer<T>(u, value);
    return SmartPointer<T>();
}

BOOST_TYPE_INDEX_END_MODULE_EXPORT

}} // namespace boost::typeindex

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#endif // BOOST_TYPE_INDEX_RUNTIME_CAST_BOOST_SHARED_PTR_CAST_HPP
