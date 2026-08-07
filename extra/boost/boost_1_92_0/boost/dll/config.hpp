// Copyright Antony Polukhin, 2018-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file boost/dll/config.hpp
/// \brief Imports filesystem, error_code, errc, system_error, make_error_code from Boost or C++17 into `boost::dll::fs` namespace.

#ifndef BOOST_DLL_CONFIG_HPP
#define BOOST_DLL_CONFIG_HPP

#include <boost/dll/detail/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#ifdef BOOST_DLL_DOXYGEN
/// Define this macro to make Boost.DLL use C++17's std::filesystem::path and std::system_error.
#define BOOST_DLL_USE_STD_FS BOOST_DLL_USE_STD_FS

/// Define this macro to make Boost.DLL use boost::shared_ptr instead of std::shared_ptr. This macro will be removed
/// after a few releases, consider migrating to std::shared_ptr. 
#define BOOST_DLL_USE_BOOST_SHARED_PTR BOOST_DLL_USE_BOOST_SHARED_PTR

/// This namespace contains aliases to the Boost or C++17 classes. Aliases are configured using BOOST_DLL_USE_STD_FS macro.
namespace boost { namespace dll { namespace fs {

/// Alias to `std::filesystem::path` if \forcedmacrolink{BOOST_DLL_USE_STD_FS} is defined by user.
/// Alias to `boost::filesystem::path` otherwise.
using path = std::conditional_t<BOOST_DLL_USE_STD_FS, std::filesystem::path, boost::filesystem::path>;

/// Alias to `std::error_code` if \forcedmacrolink{BOOST_DLL_USE_STD_FS} is defined by user.
/// boost::system::error_code otherwise.
using error_code = std::conditional_t<BOOST_DLL_USE_STD_FS, std::error_code, boost::system::error_code>;

/// Alias to `std::system_error` if \forcedmacrolink{BOOST_DLL_USE_STD_FS} is defined by user.
/// Alias to `boost::system::system_error` otherwise.
using system_error = std::conditional_t<BOOST_DLL_USE_STD_FS, std::system_error, boost::system::system_error>;

}}}

#endif


#ifdef BOOST_DLL_USE_STD_FS

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <filesystem>
#include <system_error>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace fs {

using namespace std::filesystem;
using std::error_code;
using std::system_error;

}}}

#else // BOOST_DLL_USE_STD_FS

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace fs {

using namespace boost::filesystem;
using boost::system::error_code;
using boost::system::system_error;

}}}

#endif // BOOST_DLL_USE_STD_FS


#ifdef BOOST_DLL_USE_BOOST_SHARED_PTR

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/make_shared.hpp>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    template <class T>
    using shared_ptr = boost::shared_ptr<T>;
    using boost::make_shared;
}}}

#else  // BOOST_DLL_USE_STD_FS

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <memory>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    template <class T>
    using shared_ptr = std::shared_ptr<T>;
    using std::make_shared;
}}}

#endif  // BOOST_DLL_USE_STD_FS

#endif // BOOST_DLL_CONFIG_HPP

