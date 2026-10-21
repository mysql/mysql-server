/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_HANDLES_HPP_INCLUDED_
#define BOOST_WINAPI_HANDLES_HPP_INCLUDED_

#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if !defined(BOOST_USE_WINDOWS_H)
extern "C" {
BOOST_WINAPI_IMPORT_EXCEPT_WM boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
CloseHandle(boost::winapi::HANDLE_ handle);

BOOST_WINAPI_IMPORT_EXCEPT_WM boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
DuplicateHandle(
    boost::winapi::HANDLE_ hSourceProcessHandle,
    boost::winapi::HANDLE_ hSourceHandle,
    boost::winapi::HANDLE_ hTargetProcessHandle,
    boost::winapi::HANDLE_* lpTargetHandle,
    boost::winapi::DWORD_ dwDesiredAccess,
    boost::winapi::BOOL_ bInheritHandle,
    boost::winapi::DWORD_ dwOptions);
} // extern "C"
#endif

#if (!defined(BOOST_USE_WINDOWS_H) || (defined(BOOST_WINAPI_IS_MINGW_W64) && (__MINGW64_VERSION_MAJOR >= 6) && (__MINGW64_VERSION_MAJOR < 9))) && \
    (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN10 && (BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM))
extern "C" {
// Older MinGW-w64 do not have this declaration. It is present when __MINGW64_VERSION_MAJOR is 9 or greater, which is the case
// in 8.0.1 (specifically, not earlier or later 8.x) and 9.0.0 and onwards. Library exports seem to be present since 6.0.0.
BOOST_WINAPI_IMPORT boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
CompareObjectHandles(
    boost::winapi::HANDLE_ hFirstObjectHandle,
    boost::winapi::HANDLE_ hSecondObjectHandle);
} // extern "C"
#endif

namespace boost {
namespace winapi {

using ::CloseHandle;
using ::DuplicateHandle;

#if (!defined(BOOST_WINAPI_IS_MINGW_W64) || (__MINGW64_VERSION_MAJOR >= 6)) && \
    (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN10 && (BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM))
using ::CompareObjectHandles;
#endif

// Note: MSVC-14.1 does not interpret INVALID_HANDLE_VALUE_ initializer as a constant expression
#if defined(BOOST_USE_WINDOWS_H)
BOOST_CONSTEXPR_OR_CONST DWORD_ DUPLICATE_CLOSE_SOURCE_ = DUPLICATE_CLOSE_SOURCE;
BOOST_CONSTEXPR_OR_CONST DWORD_ DUPLICATE_SAME_ACCESS_ = DUPLICATE_SAME_ACCESS;
const HANDLE_ INVALID_HANDLE_VALUE_ = INVALID_HANDLE_VALUE;
#else
BOOST_CONSTEXPR_OR_CONST DWORD_ DUPLICATE_CLOSE_SOURCE_ = 1;
BOOST_CONSTEXPR_OR_CONST DWORD_ DUPLICATE_SAME_ACCESS_ = 2;
const HANDLE_ INVALID_HANDLE_VALUE_ = (HANDLE_)(-1);
#endif

BOOST_CONSTEXPR_OR_CONST DWORD_ duplicate_close_source = DUPLICATE_CLOSE_SOURCE_;
BOOST_CONSTEXPR_OR_CONST DWORD_ duplicate_same_access = DUPLICATE_SAME_ACCESS_;
// Note: The "unused" attribute here should not be necessary because the variable is a constant.
//       However, MinGW gcc 5.3 spams warnings about this particular constant.
const HANDLE_ invalid_handle_value BOOST_ATTRIBUTE_UNUSED = INVALID_HANDLE_VALUE_;

}
}

#include <boost/winapi/detail/footer.hpp>

#endif // BOOST_WINAPI_HANDLES_HPP_INCLUDED_
