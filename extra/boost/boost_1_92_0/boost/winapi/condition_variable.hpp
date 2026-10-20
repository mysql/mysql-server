/*
 * Copyright 2010 Vicente J. Botet Escriba
 * Copyright 2015 Andrey Semashev
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BOOST_WINAPI_CONDITION_VARIABLE_HPP_INCLUDED_
#define BOOST_WINAPI_CONDITION_VARIABLE_HPP_INCLUDED_

#include <boost/winapi/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/srw_lock.hpp>
#include <boost/winapi/critical_section.hpp>
#include <boost/winapi/detail/cast_ptr.hpp>
#include <boost/winapi/detail/header.hpp>

#if !defined(BOOST_USE_WINDOWS_H)
extern "C" {
#if !defined(BOOST_WINAPI_IS_MINGW)
struct _RTL_CONDITION_VARIABLE;
namespace boost {
namespace winapi {
namespace detail {
typedef ::_RTL_CONDITION_VARIABLE winsdk_condition_variable;
}
}
}
#else
// Legacy MinGW does not define _RTL_CONDITION_VARIABLE types and instead defines PCONDITION_VARIABLE to PVOID
namespace boost {
namespace winapi {
namespace detail {
typedef VOID_ winsdk_condition_variable;
}
}
}
#endif

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
InitializeConditionVariable(boost::winapi::detail::winsdk_condition_variable* ConditionVariable);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
WakeConditionVariable(boost::winapi::detail::winsdk_condition_variable* ConditionVariable);

BOOST_WINAPI_IMPORT boost::winapi::VOID_ BOOST_WINAPI_WINAPI_CC
WakeAllConditionVariable(boost::winapi::detail::winsdk_condition_variable* ConditionVariable);

BOOST_WINAPI_IMPORT boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
SleepConditionVariableCS(
    boost::winapi::detail::winsdk_condition_variable* ConditionVariable,
    boost::winapi::detail::winsdk_critical_section* CriticalSection,
    boost::winapi::DWORD_ dwMilliseconds);

BOOST_WINAPI_IMPORT boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
SleepConditionVariableSRW(
    boost::winapi::detail::winsdk_condition_variable* ConditionVariable,
    boost::winapi::detail::winsdk_srwlock* SRWLock,
    boost::winapi::DWORD_ dwMilliseconds,
    boost::winapi::ULONG_ Flags);
}
#endif

namespace boost {
namespace winapi {

typedef struct BOOST_MAY_ALIAS _RTL_CONDITION_VARIABLE {
    PVOID_ Ptr;
} CONDITION_VARIABLE_, *PCONDITION_VARIABLE_;

#if defined(BOOST_USE_WINDOWS_H)
#if !defined(BOOST_WINAPI_IS_MINGW)
#define BOOST_WINAPI_CONDITION_VARIABLE_INIT CONDITION_VARIABLE_INIT
#else
// Legacy MinGW does not define CONDITION_VARIABLE_INIT
#define BOOST_WINAPI_CONDITION_VARIABLE_INIT 0
#endif
#else
#define BOOST_WINAPI_CONDITION_VARIABLE_INIT {0}
#endif

struct _RTL_CRITICAL_SECTION;
struct _RTL_SRWLOCK;

BOOST_FORCEINLINE VOID_ InitializeConditionVariable(PCONDITION_VARIABLE_ ConditionVariable)
{
    ::InitializeConditionVariable(winapi::detail::cast_ptr(ConditionVariable));
}

BOOST_FORCEINLINE VOID_ WakeConditionVariable(PCONDITION_VARIABLE_ ConditionVariable)
{
    ::WakeConditionVariable(winapi::detail::cast_ptr(ConditionVariable));
}

BOOST_FORCEINLINE VOID_ WakeAllConditionVariable(PCONDITION_VARIABLE_ ConditionVariable)
{
    ::WakeAllConditionVariable(winapi::detail::cast_ptr(ConditionVariable));
}

BOOST_FORCEINLINE BOOL_ SleepConditionVariableCS(
    PCONDITION_VARIABLE_ ConditionVariable,
    _RTL_CRITICAL_SECTION* CriticalSection,
    DWORD_ dwMilliseconds)
{
    return ::SleepConditionVariableCS(
        winapi::detail::cast_ptr(ConditionVariable),
        winapi::detail::cast_ptr(CriticalSection),
        dwMilliseconds);
}

BOOST_FORCEINLINE BOOL_ SleepConditionVariableSRW(
    PCONDITION_VARIABLE_ ConditionVariable,
    _RTL_SRWLOCK* SRWLock,
    DWORD_ dwMilliseconds,
    ULONG_ Flags)
{
    return ::SleepConditionVariableSRW(
        winapi::detail::cast_ptr(ConditionVariable),
        winapi::detail::cast_ptr(SRWLock),
        dwMilliseconds,
        Flags);
}

// Legacy MinGW does not define CONDITION_VARIABLE_LOCKMODE_SHARED
#if defined(BOOST_USE_WINDOWS_H) && !defined(BOOST_WINAPI_IS_MINGW)
BOOST_CONSTEXPR_OR_CONST ULONG_ CONDITION_VARIABLE_LOCKMODE_SHARED_ = CONDITION_VARIABLE_LOCKMODE_SHARED;
#else
BOOST_CONSTEXPR_OR_CONST ULONG_ CONDITION_VARIABLE_LOCKMODE_SHARED_ = 0x00000001;
#endif

BOOST_CONSTEXPR_OR_CONST ULONG_ condition_variable_lockmode_shared = CONDITION_VARIABLE_LOCKMODE_SHARED_;

}
}

#include <boost/winapi/detail/footer.hpp>

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

#endif // BOOST_WINAPI_CONDITION_VARIABLE_HPP_INCLUDED_
