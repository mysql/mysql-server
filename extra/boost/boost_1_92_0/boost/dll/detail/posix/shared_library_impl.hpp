// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_SHARED_LIBRARY_IMPL_HPP
#define BOOST_DLL_SHARED_LIBRARY_IMPL_HPP

#include <boost/dll/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/core/invoke_swap.hpp>
#include <boost/predef/os.h>

#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <utility>  // std::move
#include <cstring> // strncmp
#endif // !defined(BOOST_DLL_USE_STD_MODULE)

#include <dlfcn.h>
#if !BOOST_OS_MACOS && !BOOST_OS_IOS && !BOOST_OS_QNX && !BOOST_OS_CYGWIN
#   include <link.h>
#elif BOOST_OS_QNX
// QNX's copy of <elf.h> and <link.h> reside in sys folder
#   include <sys/link.h>
#endif
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#include <boost/dll/shared_library_load_mode.hpp>
#include <boost/dll/detail/posix/path_from_handle.hpp>
#include <boost/dll/detail/posix/program_location_impl.hpp>

namespace boost { namespace dll { namespace detail {

class shared_library_impl {
public:
    typedef void* native_handle_t;

    shared_library_impl() noexcept
        : handle_(nullptr)
    {}

    ~shared_library_impl() noexcept {
        unload();
    }

    shared_library_impl(shared_library_impl&& sl) noexcept
        : handle_(sl.handle_)
    {
        sl.handle_ = nullptr;
    }

    explicit shared_library_impl(native_handle_t handle) noexcept
        : handle_(handle)
    {}

    shared_library_impl & operator=(shared_library_impl&& sl) noexcept {
        swap(sl);
        return *this;
    }


    static boost::dll::fs::path decorate(const boost::dll::fs::path & sl) {
        boost::dll::fs::path actual_path = (
            std::strncmp(sl.filename().string().c_str(), "lib", 3)
            ? boost::dll::fs::path((sl.has_parent_path() ? sl.parent_path() / "lib" : "lib").native() + sl.filename().native())
            : sl
        );
        actual_path += suffix();
        return actual_path;
    }

    void load(boost::dll::fs::path sl, load_mode::type portable_mode, std::error_code &ec) {
        typedef int native_mode_t;
        native_mode_t native_mode = static_cast<native_mode_t>(portable_mode);
        unload();

        // Do not allow opening NULL paths. User must use program_location() instead
        if (sl.empty()) {
            boost::dll::detail::reset_dlerror();
            ec = std::make_error_code(
                std::errc::bad_file_descriptor
            );

            return;
        }

        // Fixing modes
        if (!(native_mode & load_mode::rtld_now)) {
            native_mode |= load_mode::rtld_lazy;
        }

        if (!(native_mode & load_mode::rtld_global)) {
            native_mode |= load_mode::rtld_local;
        }

#if BOOST_OS_LINUX || BOOST_OS_ANDROID
        if (!sl.has_parent_path() && !(native_mode & load_mode::search_system_folders)) {
            sl = "." / sl;
        }
#else
        if (!sl.is_absolute() && !(native_mode & load_mode::search_system_folders)) {
            boost::dll::fs::error_code current_path_ec;
            boost::dll::fs::path prog_loc = boost::dll::fs::current_path(current_path_ec);
            if (!current_path_ec) {
                prog_loc /= sl;
                sl.swap(prog_loc);
            }
        }
#endif

        native_mode = static_cast<unsigned>(native_mode) & ~static_cast<unsigned>(load_mode::search_system_folders);

        // Trying to open with appended decorations
        if (!!(native_mode & load_mode::append_decorations)) {
            native_mode = static_cast<unsigned>(native_mode) & ~static_cast<unsigned>(load_mode::append_decorations);

            boost::dll::fs::path actual_path = decorate(sl);
            handle_ = dlopen(actual_path.c_str(), native_mode);
            if (handle_) {
                boost::dll::detail::reset_dlerror();
                return;
            }
            boost::dll::fs::error_code prog_loc_err;
            boost::dll::fs::path loc = boost::dll::detail::program_location_impl(prog_loc_err);
            if (boost::dll::fs::exists(actual_path) && !boost::dll::fs::equivalent(sl, loc, prog_loc_err)) {
                // decorated path exists : current error is not a bad file descriptor and we are not trying to load the executable itself
                ec = std::make_error_code(
                    std::errc::executable_format_error
                );
                return;
            }
        }

        // Opening by exactly specified path
        handle_ = dlopen(sl.c_str(), native_mode);
        if (handle_) {
            boost::dll::detail::reset_dlerror();
            return;
        }

        ec = std::make_error_code(
            std::errc::bad_file_descriptor
        );

        // Maybe user wanted to load the executable itself? Checking...
        // We assume that usually user wants to load a dynamic library not the executable itself, that's why
        // we try this only after traditional load fails.
        boost::dll::fs::error_code prog_loc_err;
        boost::dll::fs::path loc = boost::dll::detail::program_location_impl(prog_loc_err);
        if (!prog_loc_err && boost::dll::fs::equivalent(sl, loc, prog_loc_err) && !prog_loc_err) {
            // As is known the function dlopen() loads the dynamic library file
            // named by the null-terminated string filename and returns an opaque
            // "handle" for the dynamic library. If filename is NULL, then the
            // returned handle is for the main program.
            ec.clear();
            boost::dll::detail::reset_dlerror();
            handle_ = dlopen(nullptr, native_mode);
            if (!handle_) {
                ec = std::make_error_code(
                    std::errc::bad_file_descriptor
                );
            }
        }
    }

    bool is_loaded() const noexcept {
        return (handle_ != 0);
    }

    void unload() noexcept {
        if (!is_loaded()) {
            return;
        }

        dlclose(handle_);
        handle_ = 0;
    }

    void swap(shared_library_impl& rhs) noexcept {
        boost::core::invoke_swap(handle_, rhs.handle_);
    }

    boost::dll::fs::path full_module_path(std::error_code &ec) const {
        return boost::dll::detail::path_from_handle(handle_, ec);
    }

    static boost::dll::fs::path suffix() {
        // https://sourceforge.net/p/predef/wiki/OperatingSystems/
#if BOOST_OS_MACOS || BOOST_OS_IOS
        return ".dylib";
#else
        return ".so";
#endif
    }

    void* symbol_addr(const char* sb, std::error_code &ec) const noexcept {
        // dlsym - obtain the address of a symbol from a dlopen object
        void* const symbol = dlsym(handle_, sb);
        if (symbol == nullptr) {
            ec = std::make_error_code(
                std::errc::invalid_seek
            );
        }

        // If handle does not refer to a valid object opened by dlopen(),
        // or if the named symbol cannot be found within any of the objects
        // associated with handle, dlsym() shall return NULL.
        // More detailed diagnostic information shall be available through dlerror().

        return symbol;
    }

    native_handle_t native() const noexcept {
        return handle_;
    }

private:
    native_handle_t         handle_;
};

}}} // boost::dll::detail

#endif // BOOST_DLL_SHARED_LIBRARY_IMPL_HPP
