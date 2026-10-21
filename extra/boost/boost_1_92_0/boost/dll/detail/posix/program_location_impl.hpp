// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_DETAIL_POSIX_PROGRAM_LOCATION_IMPL_HPP
#define BOOST_DLL_DETAIL_POSIX_PROGRAM_LOCATION_IMPL_HPP

#include <boost/dll/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/predef/os.h>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#include <boost/dll/detail/system_error.hpp>

#if BOOST_OS_MACOS || BOOST_OS_IOS

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <string>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#include <mach-o/dyld.h>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code &ec) {
        ec.clear();

        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0)
            return boost::dll::fs::path(path);

        std::string p;
        p.resize(size);
        if (_NSGetExecutablePath(&p[0], &size) != 0) {
            ec = std::make_error_code(
                std::errc::bad_file_descriptor
            );
        }

        return boost::dll::fs::path(std::move(p));
    }
}}} // namespace boost::dll::detail

#elif BOOST_OS_SOLARIS

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <stdlib.h>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code& ec) {
        ec.clear();

        return boost::dll::fs::path(getexecname());
    }
}}} // namespace boost::dll::detail

#elif BOOST_OS_BSD_FREE

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <string>
#include <stdlib.h>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code& ec) {
        ec.clear();

        int mib[4];
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PATHNAME;
        mib[3] = -1;
        char path[1024];
        size_t size = sizeof(path);
        if (sysctl(mib, 4, path, &size, nullptr, 0) == 0) {
            return boost::dll::fs::path(path);
        }

        const auto errno_snapshot = static_cast<std::errc>(errno);
        if (errno_snapshot != std::errc::not_enough_memory) {
            ec = std::make_error_code(
                errno_snapshot
            );
        }

        std::string p;
        p.resize(size);
        if (sysctl(mib, 4, p.data(), &size, nullptr, 0) != 0) {
            ec = std::make_error_code(
                static_cast<std::errc>(errno)
            );
        }

        return boost::dll::fs::path(std::move(p));
    }
}}} // namespace boost::dll::detail



#elif BOOST_OS_BSD_NET

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code &ec) {
        boost::dll::fs::error_code fs_errc;
        auto result = boost::dll::fs::read_symlink("/proc/curproc/exe", fs_errc);
        ec = fs_errc;
        return result;
    }
}}} // namespace boost::dll::detail

#elif BOOST_OS_BSD_DRAGONFLY


namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code &ec) {
        boost::dll::fs::error_code fs_errc;
        auto result = boost::dll::fs::read_symlink("/proc/curproc/file", fs_errc);
        ec = fs_errc;
        return result;
    }
}}} // namespace boost::dll::detail

#elif BOOST_OS_QNX

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <fstream>
#include <string> // for std::getline
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code &ec) {
        ec.clear();

        std::string path;
        std::ifstream ifs("/proc/self/exefile");
        std::getline(ifs, path);

        if (ifs.fail() || path.empty()) {
            ec = std::make_error_code(
                std::errc::bad_file_descriptor
            );
        }

        return boost::dll::fs::path(std::move(path));
    }
}}} // namespace boost::dll::detail

#else  // BOOST_OS_LINUX || BOOST_OS_UNIX || BOOST_OS_HPUX || BOOST_OS_ANDROID

namespace boost { namespace dll { namespace detail {
    inline boost::dll::fs::path program_location_impl(std::error_code &ec) {
        // We can not use
        // boost::dll::detail::path_from_handle(dlopen(NULL, RTLD_LAZY | RTLD_LOCAL), ignore);
        // because such code returns empty path.

        boost::dll::fs::error_code fs_errc;
        auto result = boost::dll::fs::read_symlink("/proc/self/exe", fs_errc);   // Linux specific
        ec = fs_errc;
        return result;
    }
}}} // namespace boost::dll::detail

#endif

#endif // BOOST_DLL_DETAIL_POSIX_PROGRAM_LOCATION_IMPL_HPP

