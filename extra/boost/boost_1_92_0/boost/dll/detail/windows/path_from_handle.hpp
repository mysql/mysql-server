// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_DETAIL_WINDOWS_PATH_FROM_HANDLE_HPP
#define BOOST_DLL_DETAIL_WINDOWS_PATH_FROM_HANDLE_HPP

#include <boost/dll/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/winapi/dll.hpp>
#include <boost/winapi/get_last_error.hpp>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#include <boost/dll/detail/system_error.hpp>

namespace boost { namespace dll { namespace detail {

    inline std::error_code last_error_code() noexcept {
        boost::winapi::DWORD_ err = boost::winapi::GetLastError();
        return std::error_code(
            static_cast<int>(err),
            std::system_category()
        );
    }

    inline boost::dll::fs::path path_from_handle(boost::winapi::HMODULE_ handle, std::error_code &ec) {
        constexpr boost::winapi::DWORD_ ERROR_INSUFFICIENT_BUFFER_ = 0x7A;
        constexpr boost::winapi::DWORD_ DEFAULT_PATH_SIZE_ = 260;

        ec.clear();
        {
            // If `handle` parameter is NULL, GetModuleFileName retrieves the path of the
            // executable file of the current process.
            boost::winapi::WCHAR_ path_hldr[DEFAULT_PATH_SIZE_];
            const boost::winapi::DWORD_ size = boost::winapi::GetModuleFileNameW(handle, path_hldr, DEFAULT_PATH_SIZE_);

            // If the function succeeds, the return value is the length of the string that is copied to the
            // buffer, in characters, not including the terminating null character.  If the buffer is too
            // small to hold the module name, the string is truncated to nSize characters including the
            // terminating null character, the function returns nSize, and the function sets the last
            // error to ERROR_INSUFFICIENT_BUFFER.
            if (size != 0 && size < DEFAULT_PATH_SIZE_) {
                // On success, GetModuleFileNameW() doesn't reset last error to ERROR_SUCCESS. Resetting it manually.
                return boost::dll::fs::path(path_hldr);
            }
            ec = boost::dll::detail::last_error_code();
        }

        for (boost::winapi::DWORD_ new_size = 1024; new_size < 1024 * 1024 && static_cast<boost::winapi::DWORD_>(ec.value()) == ERROR_INSUFFICIENT_BUFFER_; new_size *= 2) {
            std::wstring p(new_size, L'\0');
            const std::size_t size = boost::winapi::GetModuleFileNameW(handle, &p[0], static_cast<boost::winapi::DWORD_>(p.size()));
            if (size != 0 && size < p.size()) {
                // On success, GetModuleFileNameW() doesn't reset last error to ERROR_SUCCESS. Resetting it manually.
                ec.clear();
                p.resize(size);
                return boost::dll::fs::path(std::move(p));
            }

            ec = boost::dll::detail::last_error_code();
        }

        // Error other than ERROR_INSUFFICIENT_BUFFER_ occurred or failed to allocate buffer big enough.
        return boost::dll::fs::path();
    }

}}} // namespace boost::dll::detail

#endif // BOOST_DLL_DETAIL_WINDOWS_PATH_FROM_HANDLE_HPP

