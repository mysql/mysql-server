//
// Copyright 2013-2026 Antony Polukhin.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_CTTI_REGISTER_CLASS_HPP
#define BOOST_TYPE_INDEX_CTTI_REGISTER_CLASS_HPP

/// \file ctti_register_class.hpp
/// \brief Contains BOOST_TYPE_INDEX_REGISTER_CLASS macro implementation that uses boost::typeindex::ctti_type_index.
/// Not intended for inclusion from user's code.

#include <boost/type_index/ctti_type_index.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

#if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

namespace boost { namespace typeindex { namespace detail {

BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT

template <class T>
inline const ctti_data& ctti_construct_typeid_ref(const T*) noexcept {
    return boost::typeindex::ctti_construct<T>();
}

BOOST_TYPE_INDEX_END_MODULE_EXPORT

}}} // namespace boost::typeindex::detail

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

/// @cond
#define BOOST_TYPE_INDEX_REGISTER_CLASS                                                                       \
    virtual const boost::typeindex::detail::ctti_data& boost_type_index_type_id_runtime_() const noexcept {   \
        return boost::typeindex::detail::ctti_construct_typeid_ref(this);                                     \
    }                                                                                                         \
/**/
/// @endcond

#endif // BOOST_TYPE_INDEX_CTTI_REGISTER_CLASS_HPP

