/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef MYSQL_HARNESS_NET_TS_IMPL_RESOLVER_H_
#define MYSQL_HARNESS_NET_TS_IMPL_RESOLVER_H_

#include <algorithm>
#include <memory>  // unique_ptr
#include <string>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>  // addrinfo
#else
#include <arpa/inet.h>  // inet_ntop
#include <netdb.h>      // getaddrinfo
#include <unistd.h>     // gethostname
#endif

#include "mysql/harness/net_ts/impl/socket_error.h"  // socket::last_error_code
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace ip {
enum class resolver_errc {
  try_again = EAI_AGAIN,     //!< name could not be resolved at this time
  bad_flags = EAI_BADFLAGS,  //!< flags parameter had an invalid value
#ifdef EAI_BADHINTS
  // freebsd, macosx
  bad_hints = EAI_BADHINTS,  //!< invalid value for hints
#endif
#ifdef EAI_ADDRFAMILY
  // glibc, (removed in freebsd), solaris, macosx
  bad_address_family =
      EAI_ADDRFAMILY,  //!< address family for NAME not supported
#endif
  fail = EAI_FAIL,             //!< non recoverable failed on name resolution
  bad_family = EAI_FAMILY,     //!< ai_family not supported
  out_of_memory = EAI_MEMORY,  //!< memory allocation failed
#ifdef EAI_NODATA
  // glibc, (removed in freebsd), solaris, macosx
  no_data = EAI_NODATA,  //!< no address associated with NAME
#endif
  host_not_found = EAI_NONAME,  //!< NAME or SERVICE is unknown
#ifdef EAI_OVERFLOW
  // glibc, freebsd, solaris, macosx, musl
  overflow = EAI_OVERFLOW,  //!< argument buffer overflow
#endif
#ifdef EAI_PROTOCOL
  // glibc, freebsd, solaris, macosx
  bad_protocol = EAI_PROTOCOL,  //!< resolved protocol unknown
#endif
#ifdef EAI_CANCELED
  // glibc
  cancelled = EAI_CANCELED,  //!< request cancelled
#endif
#ifdef EAI_NOTCANCELED
  // glibc
  not_cancelled = EAI_NOTCANCELED,  //!< request not cancelled
#endif
#ifdef EAI_INPROGRESS
  // glibc
  in_progress = EAI_INPROGRESS,  //!< request in progress
#endif
#ifdef EAI_ALLDONE
  // glibc
  all_done = EAI_ALLDONE,  //!< all done
#endif
#ifdef EAI_INTR
  // glibc
  interrupted = EAI_INTR,  //!< interrupted
#endif
#ifdef EAI_IDN_ENCODE
  // glibc
  idn_encode_failed = EAI_IDN_ENCODE,  //!< IDN encode failed
#endif
  service_not_found = EAI_SERVICE,  //!< SERVICE not supported for ai_socktype
  bad_socktype = EAI_SOCKTYPE,      //!< ai_socktype not supported
};
}  // namespace ip
}  // namespace net

namespace std {
template <>
struct is_error_code_enum<net::ip::resolver_errc> : true_type {};
}  // namespace std

namespace net {
namespace ip {
inline const std::error_category &resolver_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "resolver"; }
    std::string message(int ev) const override { return gai_strerror(ev); }

    // MSDN says:
    //
    // EAI_AGAIN    == WSATRY_AGAIN
    // EAI_BADFLAGS == WSAEINVAL
    // EAI_FAIL     == WSANO_RECOVERY
    // EAI_FAMILY   == WSAEAFNOSUPPORT
    // EAI_MEMORY   == WSA_NOT_ENOUGH_MEMORY
    // EAI_NONAME   == WSAHOST_NOT_FOUND
    // EAI_SERVICE  == WSATYPE_NOT_FOUND
    // EAI_SOCKTYPE == WSAESOCKTNOTSUPPORT
  };

  static category_impl instance;
  return instance;
}

inline std::error_code make_error_code(resolver_errc ec) {
  return {static_cast<int>(ec), resolver_category()};
}
}  // namespace ip

namespace impl {
namespace resolver {

/**
 * get hostname.
 *
 * @returns void on success, the native function's error-code on error
 * @retval std::errc::filename_too_long if the buffer is too small to contain
 * the hostname + nul-char
 *
 */
inline stdx::expected<void, std::error_code> gethostname(char *buf,
                                                         size_t buf_len) {
  if (0 != ::gethostname(buf, buf_len)) {
    return stdx::make_unexpected(net::impl::socket::last_error_code());
  }

  // POSIX says that it is unspecified if the returned string contains
  // a \0 char if truncation occurred.
  //
  // Looks like only Solaris doesn't add \0 and doesn't return an error.
  //
  const auto begin = buf;
  const auto end = buf + buf_len;

  if (end == std::find(begin, end, '\0')) {
    return stdx::make_unexpected(
        std::error_code(make_error_code(std::errc::filename_too_long)));
  }

  return {};
}

inline stdx::expected<void, std::error_code> getnameinfo(
    const struct sockaddr *saddr, socklen_t addrlen, char *host,
    socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
#if defined(__APPLE__)
  // macosx doesn't check the 'addrlen' parameter and reads garbage.

  if (addrlen < sizeof(*saddr)) {
    return stdx::make_unexpected(
        make_error_code(net::ip::resolver_errc::bad_family));
  }

  if ((saddr->sa_family == AF_INET && addrlen < sizeof(sockaddr_in)) ||
      (saddr->sa_family == AF_INET6 && addrlen < sizeof(sockaddr_in6))) {
    return stdx::make_unexpected(
        make_error_code(net::ip::resolver_errc::bad_family));
  }
#endif

  int ret = ::getnameinfo(saddr, addrlen, host, hostlen, serv, servlen, flags);
  if (ret != 0) {
#ifdef EAI_SYSTEM
    // linux, freebsd, solaris, macosx
    if (ret == EAI_SYSTEM) {
      return stdx::make_unexpected(net::impl::socket::last_error_code());
    } else {
      return stdx::make_unexpected(
          std::error_code{ret, net::ip::resolver_category()});
    }
#else
#if defined(_WIN32)
    switch (ret) {
      case EAI_AGAIN:
      case EAI_BADFLAGS:
      case EAI_FAIL:
      case EAI_FAMILY:
      case EAI_MEMORY:
      case EAI_NONAME:
      case EAI_SERVICE:
      case EAI_SOCKTYPE:
        break;
      default:
        return stdx::make_unexpected(
            std::error_code{ret, std::system_category()});
    }
#endif
    return stdx::make_unexpected(
        std::error_code{ret, net::ip::resolver_category()});
#endif
  }

  return {};
}

inline stdx::expected<
    std::unique_ptr<struct addrinfo, void (*)(struct addrinfo *)>,
    std::error_code>
getaddrinfo(const char *node, const char *service,
            const struct addrinfo *hints) {
  struct addrinfo *ainfo{nullptr};

  int ret = ::getaddrinfo(node, service, hints, &ainfo);
  if (ret != 0) {
#ifdef EAI_SYSTEM
    // linux, freebsd, solaris, macosx
    if (ret == EAI_SYSTEM) {
      return stdx::make_unexpected(net::impl::socket::last_error_code());
    } else {
      return stdx::make_unexpected(
          std::error_code{ret, net::ip::resolver_category()});
    }
#else
#if defined(_WIN32)
    switch (ret) {
      case EAI_AGAIN:
      case EAI_BADFLAGS:
      case EAI_FAIL:
      case EAI_FAMILY:
      case EAI_MEMORY:
      case EAI_NONAME:
      case EAI_SERVICE:
      case EAI_SOCKTYPE:
        break;
      default:
        return stdx::make_unexpected(
            std::error_code{ret, std::system_category()});
    }
#endif
    return stdx::make_unexpected(
        std::error_code{ret, net::ip::resolver_category()});
#endif
  }

  return {std::in_place, ainfo, &::freeaddrinfo};
}

inline stdx::expected<const char *, std::error_code> inetntop(int af,
                                                              const void *src,
                                                              char *out,
                                                              size_t out_len) {
  if (nullptr == ::inet_ntop(af, src, out, out_len)) {
    return stdx::make_unexpected(net::impl::socket::last_error_code());
  }
  return out;
}

// # async getaddrinfo
//
// windows has GetAddrInfoEx
// linux has getaddrinfo_a
// freebsd has getaddrinfo_async

}  // namespace resolver
}  // namespace impl

}  // namespace net

#endif
