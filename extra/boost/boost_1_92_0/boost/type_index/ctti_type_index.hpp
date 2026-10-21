//
// Copyright 2013-2026 Antony Polukhin.
//
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_TYPE_INDEX_CTTI_TYPE_INDEX_HPP
#define BOOST_TYPE_INDEX_CTTI_TYPE_INDEX_HPP

/// \file ctti_type_index.hpp
/// \brief Contains boost::typeindex::ctti_type_index class that is constexpr if C++14
/// constexpr is supported by compiler.
///
/// boost::typeindex::ctti_type_index class can be used as a drop-in replacement
/// for std::type_index or as a type index that works at compile time, can provide a
/// type name at compile time.
///
/// It is used as the boost::typeindex::type_index when `typeid()` is not 
/// available or BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY macro is defined.

#include <boost/type_index/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#include <boost/type_index/type_index_facade.hpp>
#include <boost/type_index/detail/compile_time_type_info.hpp>

#if !defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)
#ifndef BOOST_TYPE_INDEX_USE_STD_MODULE
#include <cstring>
#include <type_traits>
#endif

#include <boost/container_hash/hash.hpp>
#endif

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

namespace boost { namespace typeindex {

namespace detail {

// That's the most trickiest part of the TypeIndex library:
//      1) we do not want to give user ability to manually construct and compare `struct-that-represents-type`
//      2) we need to distinguish between `struct-that-represents-type` and `const char*`
//      3) we need a thread-safe way to have references to instances `struct-that-represents-type`
//      4) we need a compile-time control to make sure that user does not copy or
// default construct `struct-that-represents-type`
//
// Solution would be the following:

/// \class ctti_data
/// Standard-layout class with private constructors and assignment operators.
///
/// You can not work with this class directly. The  purpose of this class is to hold type info 
/// \b when \b RTTI \b is \b off and allow ctti_type_index construction from itself.
///
/// \b Example:
/// \code
/// const detail::ctti_data& foo();
/// ...
/// type_index ti = type_index(foo());
/// std::cout << ti.pretty_name();
/// \endcode
BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT
class ctti_data {
public:
    ctti_data() = delete;
    ctti_data(const ctti_data&) = delete;
    ctti_data& operator=(const ctti_data&) = delete;
};
BOOST_TYPE_INDEX_END_MODULE_EXPORT

} // namespace detail

BOOST_TYPE_INDEX_BEGIN_MODULE_EXPORT

/// Helper method for getting detail::ctti_data of a template parameter T.
template <class T>
inline const detail::ctti_data& ctti_construct() noexcept {
    // Standard C++11, 5.2.10 Reinterpret cast:
    // An object pointer can be explicitly converted to an object pointer of a different type. When a prvalue
    // v of type "pointer to T1" is converted to the type "pointer to cv T2", the result is static_cast<cv
    // T2*>(static_cast<cv void*>(v)) if both T1 and T2 are standard-layout types (3.9) and the alignment
    // requirements of T2 are no stricter than those of T1, or if either type is void. Converting a prvalue of type
    // "pointer to T1" to the type "pointer to T2" (where T1 and T2 are object types and where the alignment
    // requirements of T2 are no stricter than those of T1) and back to its original type yields the original pointer
    // value.
    //
    // Alignments are checked in `type_index_test_ctti_alignment.cpp` test.
    return *reinterpret_cast<const detail::ctti_data*>(boost::typeindex::detail::postprocessed_name<T>());
}

/// \class ctti_type_index
/// This class is a wrapper that pretends to work exactly like stl_type_index, but does
/// not require RTTI support. \b For \b description \b of \b functions \b see type_index_facade.
///
/// This class on C++14 compatible compilers can be used at compile time and has the following
/// functions marked as constexpr:
///     * default constructor
///     * copy constructors and assignment operations
///     * class methods: name(), before(const ctti_type_index& rhs), equal(const ctti_type_index& rhs)
///     * static methods type_id<T>(), type_id_with_cvr<T>()
///     * comparison operators
///
/// Moreover, starting from C++14 the name() function always outputs the pretty_name() of a
/// type. For example the following static assert holds:
/// \code
/// static_assert(
///     boost::typeindex::ctti_type_index:::type_id<int>().name()
///     == std::string_view{"int"}
/// );
/// \endcode
///
/// This class produces slightly longer type names in C++11 than stl_type_index, see
/// "Code Bloat" fore more info.
class ctti_type_index: public type_index_facade<ctti_type_index, detail::ctti_data> {
    const char* data_;

    inline std::size_t get_raw_name_length() const noexcept;

    BOOST_CXX14_CONSTEXPR inline explicit ctti_type_index(const char* data) noexcept
        : data_(data)
    {}

public:
    using type_info_t = detail::ctti_data;

    BOOST_CXX14_CONSTEXPR inline ctti_type_index() noexcept
        : data_(boost::typeindex::detail::postprocessed_name<void>())
    {}

    inline ctti_type_index(const type_info_t& data) noexcept
        : data_(reinterpret_cast<const char*>(&data))
    {}

    inline const type_info_t& type_info() const noexcept;
    BOOST_CXX14_CONSTEXPR inline const char* raw_name() const noexcept;
    BOOST_CXX14_CONSTEXPR inline const char* name() const noexcept;
    inline std::string  pretty_name() const;
    inline std::size_t  hash_code() const noexcept;

    BOOST_CXX14_CONSTEXPR inline bool equal(const ctti_type_index& rhs) const noexcept;
    BOOST_CXX14_CONSTEXPR inline bool before(const ctti_type_index& rhs) const noexcept;

    template <class T>
    BOOST_CXX14_CONSTEXPR inline static ctti_type_index type_id() noexcept;

    template <class T>
    BOOST_CXX14_CONSTEXPR inline static ctti_type_index type_id_with_cvr() noexcept;

    template <class T>
    inline static ctti_type_index type_id_runtime(const T& variable) noexcept;
};

BOOST_TYPE_INDEX_END_MODULE_EXPORT


inline const ctti_type_index::type_info_t& ctti_type_index::type_info() const noexcept {
    return *reinterpret_cast<const detail::ctti_data*>(data_);
}


BOOST_CXX14_CONSTEXPR inline bool ctti_type_index::equal(const ctti_type_index& rhs) const noexcept {
    const char* const left = raw_name();
    const char* const right = rhs.raw_name();
#ifdef __cpp_lib_is_constant_evaluated
    if (!std::is_constant_evaluated()) {
        if (left == right) {
            return true;
        }
    }
#endif
    return !boost::typeindex::detail::constexpr_strcmp(left, right);
}

BOOST_CXX14_CONSTEXPR inline bool ctti_type_index::before(const ctti_type_index& rhs) const noexcept {
    const char* const left = raw_name();
    const char* const right = rhs.raw_name();
#ifdef __cpp_lib_is_constant_evaluated
    if (!std::is_constant_evaluated()) {
        if (left == right) {
            return false;
        }
    }
#endif
    return boost::typeindex::detail::constexpr_strcmp(left, right) < 0;
}


template <class T>
BOOST_CXX14_CONSTEXPR inline ctti_type_index ctti_type_index::type_id() noexcept {
    using no_ref_t = typename std::remove_reference<T>::type;
    using no_cvr_t = typename std::remove_cv<no_ref_t>::type;
    return ctti_type_index(boost::typeindex::detail::postprocessed_name<no_cvr_t>());
}



template <class T>
BOOST_CXX14_CONSTEXPR inline ctti_type_index ctti_type_index::type_id_with_cvr() noexcept {
    return ctti_type_index(boost::typeindex::detail::postprocessed_name<T>());
}


template <class T>
inline ctti_type_index ctti_type_index::type_id_runtime(const T& variable) noexcept {
    return variable.boost_type_index_type_id_runtime_();
}


BOOST_CXX14_CONSTEXPR inline const char* ctti_type_index::raw_name() const noexcept {
    return data_;
}


BOOST_CXX14_CONSTEXPR inline const char* ctti_type_index::name() const noexcept {
    return data_;
}

inline std::size_t ctti_type_index::get_raw_name_length() const noexcept {
#if defined(BOOST_NO_CXX14_CONSTEXPR)
    return detail::constexpr_significant_part_length(raw_name());
#else
    return std::strlen(raw_name());
#endif
}


inline std::string ctti_type_index::pretty_name() const {
    std::size_t len = get_raw_name_length();
    return std::string(raw_name(), len);
}


inline std::size_t ctti_type_index::hash_code() const noexcept {
    return boost::hash_range(raw_name(), raw_name() + get_raw_name_length());
}

}} // namespace boost::typeindex

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_TYPE_INDEX_INTERFACE_UNIT)

#endif // BOOST_TYPE_INDEX_CTTI_TYPE_INDEX_HPP

