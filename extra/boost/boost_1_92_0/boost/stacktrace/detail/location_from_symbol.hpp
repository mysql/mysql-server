// Copyright Antony Polukhin, 2016-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_STACKTRACE_DETAIL_LOCATION_FROM_SYMBOL_HPP
#define BOOST_STACKTRACE_DETAIL_LOCATION_FROM_SYMBOL_HPP

#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#if !defined(BOOST_STACKTRACE_INTERFACE_UNIT)
#if !defined(BOOST_WINDOWS) && !defined(__CYGWIN__)
#   include <dlfcn.h>
#else
#   include <boost/winapi/dll.hpp>
#endif
#endif // !defined(BOOST_STACKTRACE_INTERFACE_UNIT)

#ifdef _AIX
/* AIX doesn't provide dladdr syscall.
   This provides a minimal implementation of dladdr which retrieves
   only files information.
   TODO: Implement the symbol name.  */

#include <sys/ldr.h>
#include <sys/debug.h>

#include <cstring>
#include <string>
#include <vector>

namespace boost { namespace stacktrace { namespace detail {

struct Dl_info {
  std::string fname_storage{};
  const char *dli_fname = nullptr;
  const char *dli_sname = nullptr;
};

int dladdr(const void* address_raw, Dl_info* info) noexcept {
  static constexpr std::size_t dl_buff_size = 0x1000;

  try {
    std::vector<struct ld_info> pld_info_storage;
    pld_info_storage.resize(
        (dl_buff_size + sizeof(struct ld_info) - 1) / sizeof(struct ld_info)
    );

    if (loadquery(L_GETINFO, pld_info_storage.data(), dl_buff_size) == -1) {
      return 0;
    }

    const auto* pld_info = pld_info_storage.data();
    const char* const address = static_cast<const char*>(address_raw);
    while (true) {
      const auto* const dataorg = static_cast<char*>(pld_info->ldinfo_dataorg);
      const auto* const textorg = static_cast<char*>(pld_info->ldinfo_textorg);
      if ((address >= dataorg && address < dataorg + pld_info->ldinfo_datasize )
          || (address >= textorg && address < textorg + pld_info->ldinfo_textsize )) {

        /* ldinfo_filename is the null-terminated path name followed
           by null-terminated member name.
           If the file is not an archive, then member name is null. */
        const auto size_filename = std::strlen(pld_info->ldinfo_filename);
        const auto size_member = std::strlen(pld_info->ldinfo_filename + size_filename + 1);

        /* If member is not null, '(' and ')' must be added to create a
           fname looking like "filename(membername)".  */
        info->fname_storage.reserve(size_filename + (size_member ? size_member  + 3 : 1));
        info->fname_storage = pld_info->ldinfo_filename;
        if (size_member) {
          info->fname_storage += "(";
          info->fname_storage += pld_info->ldinfo_filename + size_filename + 1;
          info->fname_storage += ")";
        }

        info->dli_fname = info->fname_storage.c_str();
        return 1;
      }

      if (!pld_info->ldinfo_next) {
        break;
      }

      pld_info = reinterpret_cast<const struct ld_info *>(
        reinterpret_cast<const char*>(pld_info) + pld_info->ldinfo_next
      );
    };
  } catch (...) {
    // ignore
  }

  return 0;
}

}}} // namespace boost::stacktrace::detail

#elif !defined(BOOST_WINDOWS) && !defined(__CYGWIN__)

namespace boost { namespace stacktrace { namespace detail {

using Dl_info = ::Dl_info;

inline int dladdr(const void* addr, Dl_info& dli) noexcept {
  // `dladdr` on Solaris accepts nonconst addresses
  return ::dladdr(const_cast<void*>(addr), &dli);
}

}}} // namespace boost::stacktrace::detail

#endif

namespace boost { namespace stacktrace { namespace detail {

#if !defined(BOOST_WINDOWS) && !defined(__CYGWIN__)
class location_from_symbol {
    boost::stacktrace::detail::Dl_info dli_;

public:
    explicit location_from_symbol(const void* addr) noexcept
        : dli_()
    {
        if (!boost::stacktrace::detail::dladdr(addr, dli_)) {
            dli_.dli_fname = 0;
        }
    }

    bool empty() const noexcept {
        return !dli_.dli_fname;
    }

    const char* name() const noexcept {
        return dli_.dli_fname;
    }
};

class program_location {
public:
    const char* name() const noexcept {
        return 0;
    }
};

#else

class location_from_symbol {
    BOOST_STATIC_CONSTEXPR boost::winapi::DWORD_ DEFAULT_PATH_SIZE_ = 260;
    char file_name_[DEFAULT_PATH_SIZE_];

public:
    explicit location_from_symbol(const void* addr) noexcept {
        file_name_[0] = '\0';

        boost::winapi::MEMORY_BASIC_INFORMATION_ mbi;
        if (!boost::winapi::VirtualQuery(addr, &mbi, sizeof(mbi))) {
            return;
        }

        boost::winapi::HMODULE_ handle = reinterpret_cast<boost::winapi::HMODULE_>(mbi.AllocationBase);
        if (!boost::winapi::GetModuleFileNameA(handle, file_name_, DEFAULT_PATH_SIZE_)) {
            file_name_[0] = '\0';
            return;
        }
    }

    bool empty() const noexcept {
        return file_name_[0] == '\0';
    }

    const char* name() const noexcept {
        return file_name_;
    }
};

class program_location {
    BOOST_STATIC_CONSTEXPR boost::winapi::DWORD_ DEFAULT_PATH_SIZE_ = 260;
    char file_name_[DEFAULT_PATH_SIZE_];

public:
    program_location() noexcept {
        file_name_[0] = '\0';

        const boost::winapi::HMODULE_ handle = 0;
        if (!boost::winapi::GetModuleFileNameA(handle, file_name_, DEFAULT_PATH_SIZE_)) {
            file_name_[0] = '\0';
        }
    }

    const char* name() const noexcept {
        return file_name_[0] ? file_name_ : 0;
    }
};
#endif

}}} // namespace boost::stacktrace::detail

#endif // BOOST_STACKTRACE_DETAIL_LOCATION_FROM_SYMBOL_HPP
