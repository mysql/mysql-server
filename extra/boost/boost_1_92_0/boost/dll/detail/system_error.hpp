// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_SYSTEM_ERROR_HPP
#define BOOST_DLL_SYSTEM_ERROR_HPP

#include <boost/dll/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_DLL_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <boost/dll/config.hpp>

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/predef/os.h>
#include <boost/throw_exception.hpp>

#if !BOOST_OS_WINDOWS
#   include <dlfcn.h>
#endif

#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <system_error>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)

#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

BOOST_DLL_BEGIN_MODULE_EXPORT

namespace boost { namespace dll { namespace detail {

    inline void reset_dlerror() noexcept {
#if !BOOST_OS_WINDOWS
        const char* const error_txt = dlerror();
        (void)error_txt;
#endif
    }

    inline void report_error(const std::error_code& ec, const char* message) {
#if !BOOST_OS_WINDOWS
        const char* const error_txt = dlerror();
        if (error_txt) {
            boost::throw_exception(
                boost::dll::fs::system_error(
                    ec,
                    message + std::string(" (dlerror system message: ") + error_txt + std::string(")")
                )
            );
        }
#endif

        boost::throw_exception(
            boost::dll::fs::system_error(
                ec, message
            )
        );
    }

}}} // boost::dll::detail

BOOST_DLL_END_MODULE_EXPORT

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_DLL_INTERFACE_UNIT)

#endif // BOOST_DLL_SYSTEM_ERROR_HPP

