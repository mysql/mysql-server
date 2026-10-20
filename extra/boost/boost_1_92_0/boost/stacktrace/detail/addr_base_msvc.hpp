// Copyright Antony Polukhin, 2016-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_DETAIL_ADDR_BASE_MSVC_HPP
#define BOOST_STACKTRACE_DETAIL_ADDR_BASE_MSVC_HPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if !defined(BOOST_STACKTRACE_INTERFACE_UNIT)
#include <cstdio>
#include <cstdint>
#include <memory>

#ifdef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
// Prevent inclusion of extra Windows SDK headers which can cause conflict
// with other code using Windows SDK
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#undef WIN32_LEAN_AND_MEAN
#endif
#endif // !defined(BOOST_STACKTRACE_INTERFACE_UNIT)

namespace boost { namespace stacktrace { namespace detail {
  inline std::uintptr_t get_own_proc_addr_base(const void* addr) {
        // Try to avoid allocating memory for the modules array if possible.
        // The stack buffer should be large enough for most processes.
        HMODULE modules_stack[1024];
        std::unique_ptr<HMODULE[]> modules_allocated;
        HMODULE* modules = modules_stack;

        DWORD needed_bytes = 0;
        std::uintptr_t addr_base = 0;

        HANDLE process_handle = GetCurrentProcess();
        auto enum_process_is_ok = EnumProcessModules(process_handle, modules, sizeof(modules), &needed_bytes);

        // Check if the error is because the buffer is too small.
        if (!enum_process_is_ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            modules_allocated.reset(new HMODULE[needed_bytes / sizeof(HMODULE)]);
            modules = modules_allocated.get();
            enum_process_is_ok = EnumProcessModules(process_handle, modules, needed_bytes, &needed_bytes);
        }

        if (enum_process_is_ok) {
            for (std::size_t i = 0; i < (needed_bytes / sizeof(HMODULE)); ++i) {
                MODULEINFO module_info;

                // Get the module name
                if (GetModuleInformation(process_handle, modules[i], &module_info, sizeof(module_info))
                    && module_info.lpBaseOfDll <= addr && addr < LPBYTE(module_info.lpBaseOfDll) + module_info.SizeOfImage) {
                    // Module contains the address
                    addr_base = reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll);
                    break;
                }
            }
        }

        CloseHandle(process_handle);

        return addr_base;
    }

}}} // namespace boost::stacktrace::detail

#endif // BOOST_STACKTRACE_DETAIL_ADDR_BASE_MSVC_HPP
