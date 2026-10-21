/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_SRW_LOCK_HPP_INCLUDED_
#define BOOST_WINAPI_SRW_LOCK_HPP_INCLUDED_

#include <boost/winapi/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if BOOST_USE_WINAPI_VERSION < BOOST_WINAPI_VERSION_WIN6 \
    || (defined(_MSC_VER) && _MSC_VER < 1600)
// Windows SDK 6.0A, which is used by MSVC 9, does not have TryAcquireSRWLock* neither in headers nor in .lib files,
// although the functions are present in later SDKs since Windows API version 6.
#define BOOST_WINAPI_NO_TRY_ACQUIRE_SRWLOCK
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/detail/cast_ptr.hpp>
#include <boost/winapi/detail/header.hpp>

#if !defined(BOOST_USE_WINDOWS_H)
extern "C" {
#if !defined(BOOST_WINAPI_IS_MINGW)
struct _RTL_SRWLOCK;
namespace boost {
namespace winapi {
namespace detail {
typedef ::_RTL_SRWLOCK winsdk_srwlock;
}
}
}
#else
// Legacy MinGW does not define _RTL_SRWLOCK type and instead defines PSRWLOCK to PVOID
namespace boost {
namespace winapi {
namespace detail {
typedef VOID_ winsdk_srwlock;
}
}
}
#endif

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
InitializeSRWLock(boost::winapi::detail::winsdk_srwlock* SRWLock);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
ReleaseSRWLockExclusive(boost::winapi::detail::winsdk_srwlock* SRWLock);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
ReleaseSRWLockShared(boost::winapi::detail::winsdk_srwlock* SRWLock);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
AcquireSRWLockExclusive(boost::winapi::detail::winsdk_srwlock* SRWLock);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
AcquireSRWLockShared(boost::winapi::detail::winsdk_srwlock* SRWLock);

#if !defined( BOOST_WINAPI_NO_TRY_ACQUIRE_SRWLOCK )
BOOST_WINAPI_IMPORT boost::winapi::BOOLEAN_ BOOST_WINAPI_WINAPI_CC
TryAcquireSRWLockExclusive(boost::winapi::detail::winsdk_srwlock* SRWLock);

BOOST_WINAPI_IMPORT boost::winapi::BOOLEAN_ BOOST_WINAPI_WINAPI_CC
TryAcquireSRWLockShared(boost::winapi::detail::winsdk_srwlock* SRWLock);
#endif
} // extern "C"
#endif

namespace boost {
namespace winapi {

typedef struct BOOST_MAY_ALIAS _RTL_SRWLOCK {
    PVOID_ Ptr;
} SRWLOCK_, *PSRWLOCK_;

#if defined(BOOST_USE_WINDOWS_H)
#if !defined(BOOST_WINAPI_IS_MINGW)
#define BOOST_WINAPI_SRWLOCK_INIT SRWLOCK_INIT
#else
// Legacy MinGW does not define SRWLOCK_INIT
#define BOOST_WINAPI_SRWLOCK_INIT 0
#endif
#else
#define BOOST_WINAPI_SRWLOCK_INIT {0}
#endif

BOOST_FORCEINLINE VOID_ InitializeSRWLock(PSRWLOCK_ SRWLock)
{
    ::InitializeSRWLock(winapi::detail::cast_ptr(SRWLock));
}

BOOST_FORCEINLINE VOID_ ReleaseSRWLockExclusive(PSRWLOCK_ SRWLock)
{
    ::ReleaseSRWLockExclusive(winapi::detail::cast_ptr(SRWLock));
}

BOOST_FORCEINLINE VOID_ ReleaseSRWLockShared(PSRWLOCK_ SRWLock)
{
    ::ReleaseSRWLockShared(winapi::detail::cast_ptr(SRWLock));
}

BOOST_FORCEINLINE VOID_ AcquireSRWLockExclusive(PSRWLOCK_ SRWLock)
{
    ::AcquireSRWLockExclusive(winapi::detail::cast_ptr(SRWLock));
}

BOOST_FORCEINLINE VOID_ AcquireSRWLockShared(PSRWLOCK_ SRWLock)
{
    ::AcquireSRWLockShared(winapi::detail::cast_ptr(SRWLock));
}

#if !defined(BOOST_WINAPI_NO_TRY_ACQUIRE_SRWLOCK)
BOOST_FORCEINLINE BOOLEAN_ TryAcquireSRWLockExclusive(PSRWLOCK_ SRWLock)
{
    return ::TryAcquireSRWLockExclusive(winapi::detail::cast_ptr(SRWLock));
}

BOOST_FORCEINLINE BOOLEAN_ TryAcquireSRWLockShared(PSRWLOCK_ SRWLock)
{
    return ::TryAcquireSRWLockShared(winapi::detail::cast_ptr(SRWLock));
}
#endif

}
}

#include <boost/winapi/detail/footer.hpp>

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

#endif // BOOST_WINAPI_SRW_LOCK_HPP_INCLUDED_
