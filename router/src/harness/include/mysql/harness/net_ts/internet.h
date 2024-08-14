/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_NET_TS_INTERNET_H_
#define MYSQL_HARNESS_NET_TS_INTERNET_H_

#include <algorithm>
#include <array>
#include <bit>  // endian
#include <bitset>
#include <cstddef>  // ptrdiff_t
#include <cstring>  // memset, memcpy
#include <forward_list>
#include <sstream>
#include <stdexcept>  // length_error
#include <system_error>

#ifdef _WIN32
#include <WS2tcpip.h>  // inet_ntop
#include <WinSock2.h>
#include <Windows.h>
#else
#include <arpa/inet.h>    // inet_ntop
#include <netdb.h>        // getaddrinfo
#include <netinet/in.h>   // in_addr_t
#include <netinet/ip6.h>  // in6_addr_t
#include <netinet/tcp.h>  // TCP_NODELAY
#include <sys/ioctl.h>    // ioctl
#endif

#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/bit.h"  // byteswap
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace ip {

/**
 * convert an integer from host-endianness into network endianness.
 *
 * constexpr version of htons()/htonl()
 */
template <class T>
constexpr T host_to_network(const T t) noexcept {
  // no need to swap, host has network byte-order
  if (std::endian::native == std::endian::big) return t;

  return stdx::byteswap(t);
}

/**
 * convert an integer from network-endianness into host endianness.
 *
 * constexpr version of ntohs()/ntohl()
 */
template <class T>
constexpr T network_to_host(const T t) noexcept {
  // no need to swap, host has network byte-order
  if (std::endian::native == std::endian::big) return t;

  return stdx::byteswap(t);
}

using v6_only = socket_option::boolean<IPPROTO_IPV6, IPV6_V6ONLY>;

using port_type = uint_least16_t;
using scope_id_type = uint_least32_t;

class address_v4 {
 public:
  using uint_type = uint_least32_t;
  struct bytes_type : std::array<unsigned char, 4> {
    template <class... T>
    explicit constexpr bytes_type(T... t)
        : array<unsigned char, 4>{{static_cast<unsigned char>(t)...}} {}
  };

  // create from int in host-byte-order
  constexpr address_v4() noexcept : addr_{} {}
  explicit constexpr address_v4(uint_type val)
      : addr_{
            (val >> 24) & 0xff,
            (val >> 16) & 0xff,
            (val >> 8) & 0xff,
            (val >> 0) & 0xff,
        } {}

  // create from bytes in network-byte-order
  constexpr address_v4(const bytes_type &b) : addr_{b} {}

  constexpr bool is_unspecified() const noexcept { return to_uint() == 0; }
  constexpr bool is_loopback() const noexcept {
    return (to_uint() & 0xff000000) == 0x7f000000;
  }
  constexpr bool is_multicast() const noexcept {
    return (to_uint() & 0xf0000000) == 0xe0000000;
  }

  template <class Allocator = std::allocator<char>>
  std::basic_string<char, std::char_traits<char>, Allocator> to_string(
      const Allocator &a = Allocator()) const {
    std::basic_string<char, std::char_traits<char>, Allocator> out(a);
    out.resize(INET_ADDRSTRLEN);

    if (nullptr != ::inet_ntop(AF_INET, &addr_, &out.front(), out.size())) {
      out.erase(out.find('\0'));
    } else {
      // it failed. return empty instead
      out.resize(0);
    }

    return out;
  }

  // network byte order
  constexpr bytes_type to_bytes() const noexcept { return addr_; }

  // host byte-order
  constexpr uint_type to_uint() const noexcept {
    return (addr_[0] << 24) | (addr_[1] << 16) | (addr_[2] << 8) | addr_[3];
  }

  static constexpr address_v4 any() noexcept { return address_v4{}; }
  static constexpr address_v4 loopback() noexcept {
    return address_v4{0x7f000001};
  }
  static constexpr address_v4 broadcast() noexcept {
    return address_v4{0xffffffff};
  }

 private:
  bytes_type addr_;  // network byte order
};

// 21.5.5 [internet.address.v4.comparisons]
constexpr bool operator==(const address_v4 &a, const address_v4 &b) noexcept {
  return a.to_uint() == b.to_uint();
}
constexpr bool operator!=(const address_v4 &a, const address_v4 &b) noexcept {
  return !(a == b);
}
constexpr bool operator<(const address_v4 &a, const address_v4 &b) noexcept {
  return a.to_uint() < b.to_uint();
}
constexpr bool operator>(const address_v4 &a, const address_v4 &b) noexcept {
  return b < a;
}
constexpr bool operator<=(const address_v4 &a, const address_v4 &b) noexcept {
  return !(b < a);
}
constexpr bool operator>=(const address_v4 &a, const address_v4 &b) noexcept {
  return !(a < b);
}

/**
 * IPv6 address with scope_id.
 */
class address_v6 {
 public:
  struct bytes_type : std::array<unsigned char, 16> {
    // create an array, from parameters.
    template <class... T>
    explicit constexpr bytes_type(T... t)
        : array<unsigned char, 16>{{static_cast<unsigned char>(t)...}} {}
  };

  constexpr address_v6() noexcept : bytes_{}, scope_id_{} {}
  constexpr address_v6(const bytes_type &bytes,
                       scope_id_type scope_id = 0) noexcept
      : bytes_{bytes}, scope_id_{scope_id} {}

  // address of IPv6 any
  static constexpr address_v6 any() { return {}; }

  // address of IPv6 loopback
  static constexpr address_v6 loopback() {
    return bytes_type{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
  }

  constexpr bool is_unspecified() const noexcept;
  constexpr bool is_loopback() const noexcept;
  constexpr bool is_multicast() const noexcept { return bytes_[0] == 0xff; }
  constexpr bool is_link_local() const noexcept {
    return (bytes_[0] == 0xfe) && (bytes_[1] & 0xc0) == 0x80;
  }
  constexpr bool is_site_local() const noexcept {
    return (bytes_[0] == 0xfe) && (bytes_[1] & 0xc0) == 0xc0;
  }
  constexpr bool is_v4_mapped() const noexcept {
    return bytes_[0] == 0 && bytes_[1] == 0 && bytes_[2] == 0 &&
           bytes_[3] == 0 && bytes_[4] == 0 && bytes_[5] == 0 &&
           bytes_[6] == 0 && bytes_[7] == 0 && bytes_[8] == 0 &&
           bytes_[9] == 0 && bytes_[10] == 0xff && bytes_[11] == 0xff;
  }
  constexpr bool is_multicast_node_local() const noexcept {
    return is_multicast() && (bytes_[1] & 0x0f) == 0x01;
  }
  constexpr bool is_multicast_link_local() const noexcept {
    return is_multicast() && (bytes_[1] & 0x0f) == 0x02;
  }
  constexpr bool is_multicast_site_local() const noexcept {
    return is_multicast() && (bytes_[1] & 0x0f) == 0x05;
  }
  constexpr bool is_multicast_org_local() const noexcept {
    return is_multicast() && (bytes_[1] & 0x0f) == 0x08;
  }
  constexpr bool is_multicast_global() const noexcept {
    return is_multicast() && (bytes_[1] & 0x0f) == 0x0e;
  }

  constexpr bytes_type to_bytes() const noexcept { return bytes_; }

  constexpr scope_id_type scope_id() const noexcept { return scope_id_; }

  /**
   * convert an address_v6 into a string.
   *
   * @returns empty string on error.
   */
  template <class Allocator = std::allocator<char>>
  std::basic_string<char, std::char_traits<char>, Allocator> to_string(
      const Allocator &a = Allocator()) const {
    std::basic_string<char, std::char_traits<char>, Allocator> out(a);
    out.resize(INET6_ADDRSTRLEN);

    if (nullptr !=
        ::inet_ntop(AF_INET6, bytes_.data(), &out.front(), out.size())) {
      out.erase(out.find('\0'));  // we don't want the trailing  \0
      if (scope_id() != 0) {
        out.append("%");
        out += std::to_string(scope_id());
      }
    } else {
      // it failed. return empty instead
      out.resize(0);
    }

    return out;
  }

 private:
  bytes_type bytes_;
  scope_id_type scope_id_;
};

class bad_address_cast : public std::bad_cast {
 public:
  bad_address_cast() noexcept = default;
};

// 21.6.5 [internet.address.v6.comparisons]
constexpr bool operator==(const address_v6 &a, const address_v6 &b) noexcept {
  auto const aa = a.to_bytes();
  auto const bb = b.to_bytes();

  size_t ndx{0};

  for (; ndx != aa.size() && aa[ndx] == bb[ndx]; ++ndx)
    ;

  return (ndx == aa.size()) ? a.scope_id() == b.scope_id() : false;
}
constexpr bool operator!=(const address_v6 &a, const address_v6 &b) noexcept {
  return !(a == b);
}
constexpr bool operator<(const address_v6 &a, const address_v6 &b) noexcept {
  auto const aa = a.to_bytes();
  auto const bb = b.to_bytes();

  size_t ndx{0};

  for (; ndx != aa.size() && aa[ndx] == bb[ndx]; ++ndx)
    ;

  return (ndx == aa.size()) ? a.scope_id() < b.scope_id() : aa[ndx] < bb[ndx];
}
constexpr bool operator>(const address_v6 &a, const address_v6 &b) noexcept {
  return b < a;
}
constexpr bool operator<=(const address_v6 &a, const address_v6 &b) noexcept {
  return !(b < a);
}
constexpr bool operator>=(const address_v6 &a, const address_v6 &b) noexcept {
  return !(a < b);
}

constexpr bool address_v6::is_unspecified() const noexcept {
  return *this == any();
}
constexpr bool address_v6::is_loopback() const noexcept {
  return *this == loopback();
}

class address {
 public:
  constexpr address() noexcept : v4_{}, is_v4_{true} {}
  constexpr address(const address_v4 &a) noexcept : v4_{a}, is_v4_{true} {}
  constexpr address(const address_v6 &a) noexcept : v6_{a}, is_v4_{false} {}

  constexpr bool is_v4() const noexcept { return is_v4_; }
  constexpr bool is_v6() const noexcept { return !is_v4_; }

  /**
   * get address_v4 part.
   *
   * @throws bad_address_cast if !is_v4()
   */
  constexpr address_v4 to_v4() const {
    return is_v4() ? v4_ : throw bad_address_cast();
  }
  /**
   * get address_v6 part.
   *
   * @throws bad_address_cast if !is_v6()
   */
  constexpr address_v6 to_v6() const {
    return is_v6() ? v6_ : throw bad_address_cast();
  }
  constexpr bool is_unspecified() const noexcept {
    return is_v4() ? v4_.is_unspecified() : v6_.is_unspecified();
  }
  constexpr bool is_loopback() const noexcept {
    return is_v4() ? v4_.is_loopback() : v6_.is_loopback();
  }
  constexpr bool is_multicast() const noexcept {
    return is_v4() ? v4_.is_multicast() : v6_.is_multicast();
  }

  /**
   * convert an address into a string.
   *
   * @returns empty string on error.
   */
  template <class Allocator = std::allocator<char>>
  std::basic_string<char, std::char_traits<char>, Allocator> to_string(
      const Allocator &a = Allocator()) const {
    return is_v4() ? v4_.to_string(a) : v6_.to_string(a);
  }

 private:
  union {
    address_v4 v4_;
    address_v6 v6_;
  };

  bool is_v4_;
};

/**
 * make address_v6 from a string.
 *
 * @param str address string.
 *
 * @returns a address_v6 on success, std::error_code otherwise
 */
inline stdx::expected<address_v6, std::error_code> make_address_v6(
    const char *str) {
  address_v6::bytes_type ipv6_addr;

  scope_id_type scope_id{0};
  int inet_pton_res;
  // parse the scope_id separately as inet_pton() doesn't know about it.
  //
  // only numeric IDs though. For named scope-ids like "lo", getifaddrs() is
  // needed
  if (const char *percent = strchr(str, '%')) {
    const char *after_percent = percent + 1;
    // empty and numerics with leading -|+ are invalid
    if (*after_percent == '\0' || *after_percent == '-' ||
        *after_percent == '+') {
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }

    char *err{nullptr};
    scope_id = ::strtoul(after_percent, &err, 10);
    if (*err != '\0') {
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }

    std::string before_percent(str, percent);

    inet_pton_res = ::inet_pton(AF_INET6, before_percent.c_str(), &ipv6_addr);
  } else {
    inet_pton_res = ::inet_pton(AF_INET6, str, &ipv6_addr);
  }
  if (inet_pton_res == 1) {
    using ret_type = stdx::expected<address_v6, std::error_code>;

    return ret_type{std::in_place, ipv6_addr, scope_id};
  } else if (inet_pton_res == 0) {
    // parse failed
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  } else {
    return stdx::unexpected(impl::socket::last_error_code());
  }
}

/**
 * make address_v4 from a string.
 *
 * @param str address string.
 *
 * @returns a address_v4 on success, std::error_code otherwise
 */
inline stdx::expected<address_v4, std::error_code> make_address_v4(
    const char *str) {
  address_v4::bytes_type ipv4_addr;

  int res = ::inet_pton(AF_INET, str, &ipv4_addr);
  if (res == 1) {
    return {ipv4_addr};
  } else if (res == 0) {
    // parse failed
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  } else {
    return stdx::unexpected(impl::socket::last_error_code());
  }
}

/**
 * make address from a c-string.
 *
 * @param str address string.
 *
 * @returns a address4 on success, std::error_code otherwise
 */
inline stdx::expected<address, std::error_code> make_address(const char *str) {
  auto v6_res = make_address_v6(str);
  if (v6_res) {
    return address{*v6_res};
  } else {
    auto v4_res = make_address_v4(str);

    if (v4_res) return address{*v4_res};
    return stdx::unexpected(v4_res.error());
  }
}

/**
 * make address from a string.
 *
 * @param str address string.
 *
 * @returns a address4 on success, std::error_code otherwise
 */
inline stdx::expected<address, std::error_code> make_address(
    const std::string &str) {
  return make_address(str.c_str());
}

template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &os, const address &addr) {
  os << addr.to_string().c_str();

  return os;
}

constexpr bool operator==(const address &a, const address &b) noexcept {
  if (a.is_v4() && b.is_v4()) return a.to_v4() == b.to_v4();
  if (a.is_v6() && b.is_v6()) return a.to_v6() == b.to_v6();

  return false;
}

constexpr bool operator!=(const address &a, const address &b) noexcept {
  return !(a == b);
}

constexpr bool operator<(const address &a, const address &b) noexcept {
  // v4 is "smaller" than v6
  if (a.is_v4() && !b.is_v4()) return true;
  if (!a.is_v4() && b.is_v4()) return false;

  // both are of the same type
  if (a.is_v4()) return a.to_v4() < b.to_v4();
  return a.to_v6() < b.to_v6();
}

/**
 * endpoint of IPv4/IPv6 based connection.
 *
 *
 */
template <typename InternetProtocol>
class basic_resolver_entry {
 public:
  using protocol_type = InternetProtocol;
  using endpoint_type = typename protocol_type::endpoint;

  basic_resolver_entry() = default;

  basic_resolver_entry(const endpoint_type &ep, std::string host_name,
                       std::string service_name)
      : ep_{ep},
        host_name_{std::move(host_name)},
        service_name_{std::move(service_name)} {}

  endpoint_type endpoint() const { return ep_; }

  std::string host_name() const { return host_name_; }
  std::string service_name() const { return service_name_; }

 private:
  endpoint_type ep_;
  std::string host_name_;
  std::string service_name_;
};

// forward-decl for the the friendship
template <typename InternetProtocol>
class basic_resolver;

template <typename InternetProtocol>
class basic_resolver_results {
 public:
  using protocol_type = InternetProtocol;
  using endpoint_type = typename protocol_type::endpoint;
  using value_type = basic_resolver_entry<protocol_type>;
  using const_reference = const value_type &;
  using reference = value_type &;
  using const_iterator = typename std::forward_list<value_type>::const_iterator;
  using iterator = const_iterator;
  using difference_type = ptrdiff_t;
  using size_type = size_t;

  basic_resolver_results() = default;

  size_type size() const noexcept { return size_; }
  size_type max_size() const noexcept { return results_.max_size(); }
  bool empty() const noexcept { return results_.empty(); }

  const_iterator begin() const { return results_.begin(); }
  const_iterator end() const { return results_.end(); }
  const_iterator cbegin() const { return results_.cbegin(); }
  const_iterator cend() const { return results_.cend(); }

 private:
  friend class basic_resolver<protocol_type>;

  basic_resolver_results(std::unique_ptr<addrinfo, void (*)(addrinfo *)> ainfo,
                         const std::string &host_name,
                         const std::string &service_name) {
    endpoint_type ep;

    auto tail = results_.before_begin();
    for (const auto *cur = ainfo.get(); cur != nullptr; cur = cur->ai_next) {
      std::memcpy(ep.data(), cur->ai_addr, cur->ai_addrlen);

      tail = results_.emplace_after(tail, ep, host_name, service_name);
      ++size_;
    }
  }

  basic_resolver_results(const endpoint_type &ep, const std::string &host_name,
                         const std::string &service_name) {
    auto tail = results_.before_begin();

    tail = results_.emplace_after(tail, ep, host_name, service_name);
    ++size_;
  }

  std::forward_list<value_type> results_;
  size_t size_{0};
};

class resolver_base {
 public:
  using flags = std::bitset<32>;

  static constexpr flags passive = AI_PASSIVE;
  static constexpr flags canonical_name = AI_CANONNAME;
  static constexpr flags numeric_host = AI_NUMERICHOST;
  static constexpr flags numeric_service = AI_NUMERICSERV;
  static constexpr flags v4_mapped = AI_V4MAPPED;
  static constexpr flags all_matching = AI_ALL;
  static constexpr flags address_configured = AI_ADDRCONFIG;
};

template <typename InternetProtocol>
class basic_resolver : public resolver_base {
 public:
  using executor_type = io_context::executor_type;
  using protocol_type = InternetProtocol;
  using endpoint_type = typename InternetProtocol::endpoint;
  using results_type = basic_resolver_results<InternetProtocol>;
  using error_type = impl::socket::error_type;

  explicit basic_resolver(io_context &io_ctx) : io_ctx_{io_ctx} {}

  stdx::expected<results_type, error_type> resolve(
      const std::string &host_name, const std::string &service_name, flags f) {
    const auto proto = endpoint_type{}.protocol();

    ::addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto.type();
    hints.ai_protocol = proto.protocol();

    // posix ulong, windows int
    hints.ai_flags = static_cast<decltype(hints.ai_flags)>(f.to_ulong());

    auto res = io_ctx_.socket_service()->getaddrinfo(
        host_name.empty() ? nullptr : host_name.c_str(),
        service_name.empty() ? nullptr : service_name.c_str(), &hints);

    if (!res) return stdx::unexpected(res.error());

    return results_type{std::move(res.value()), host_name, service_name};
  }
  stdx::expected<results_type, error_type> resolve(
      const std::string &host_name, const std::string &service_name) {
    return resolve(host_name, service_name, flags{});
  }

  stdx::expected<results_type, error_type> resolve(const endpoint_type &ep) {
    std::array<char, NI_MAXHOST> host_name;
    std::array<char, NI_MAXSERV> service_name;

    int nameinfo_flags{0};

    if (endpoint_type().protocol().type() == SOCK_DGRAM) {
      nameinfo_flags |= NI_DGRAM;
    }

    const auto nameinfo_res = net::impl::resolver::getnameinfo(
        static_cast<const struct sockaddr *>(ep.data()), ep.size(),
        host_name.data(), host_name.size(), service_name.data(),
        service_name.size(), nameinfo_flags);
    if (!nameinfo_res) {
      return stdx::unexpected(nameinfo_res.error());
    }

    // find \0 char in array. If \0 isn't found, end of array.
    const auto name_end = std::find(host_name.begin(), host_name.end(), '\0');
    const auto serv_end =
        std::find(service_name.begin(), service_name.end(), '\0');

    return results_type{
        ep,
        std::string{host_name.begin(), name_end},
        std::string{service_name.begin(), serv_end},
    };
  }

 private:
  io_context &io_ctx_;
};

template <typename InternetProtocol>
class basic_endpoint {
 public:
  using protocol_type = InternetProtocol;
  using size_type = size_t;

  /**
   * default constructor.
   *
   * protocol() is v4()
   */
  constexpr basic_endpoint() : data_{} {
    data_.v4.sin_family = protocol_type::v4().family();
  }

  /**
   * construct from protocol and port-number.
   *
   * basic_endpoint(v4(), 80).
   */
  constexpr basic_endpoint(const protocol_type &proto,
                           port_type port_num) noexcept
      : data_{} {
    // win32 has short for .sin_family, unix has int
    data_.v4.sin_family = decltype(data_.v4.sin_family)(proto.family());
    data_.v4.sin_port = host_to_network(port_num);
  }

  /**
   * construct from address and port-number.
   */
  constexpr basic_endpoint(const ip::address &addr,
                           port_type port_num) noexcept {
    if (addr.is_v4()) {
      data_.v4.sin_family = protocol_type::v4().family();
      data_.v4.sin_port = host_to_network(port_num);
      {
        auto addr_b = addr.to_v4().to_bytes();

        // cast, as .s_addr is an int and incrementing it would step way too far
        std::copy(addr_b.begin(), addr_b.end(),
                  reinterpret_cast<unsigned char *>(&data_.v4.sin_addr.s_addr));
      }
    } else {
      data_.v6.sin6_family = protocol_type::v6().family();
      data_.v6.sin6_port = host_to_network(port_num);
      {
        auto addr_b = addr.to_v6().to_bytes();
        std::copy(addr_b.begin(), addr_b.end(), data_.v6.sin6_addr.s6_addr);
      }
      data_.v6.sin6_scope_id = addr.to_v6().scope_id();
    }
  }

  /**
   * get protocol of the endpoint.
   */
  constexpr protocol_type protocol() const noexcept {
    return data_.v4.sin_family == AF_INET ? protocol_type::v4()
                                          : protocol_type::v6();
  }

  /**
   * get address of the endpoint.
   */
  constexpr ip::address address() const noexcept {
    if (protocol().family() == protocol_type::v4().family())
      return address_v4(network_to_host(data_.v4.sin_addr.s_addr));

    address_v6::bytes_type v6b;

    std::copy(data_.v6.sin6_addr.s6_addr, data_.v6.sin6_addr.s6_addr + 16,
              v6b.begin());
    return address_v6(v6b);
  }

  /**
   * get port of the endpoint.
   */
  constexpr port_type port() const noexcept {
    return data_.v4.sin_family == AF_INET ? network_to_host(data_.v4.sin_port)
                                          : network_to_host(data_.v6.sin6_port);
  }

  /**
   * const pointer to the underlying sockaddr.
   */
  const void *data() const noexcept { return &data_; }

  /**
   * pointer to the underlying sockaddr.
   */
  void *data() noexcept { return &data_; }

  /**
   * size of the underlying sockaddr.
   */
  constexpr size_type size() const noexcept {
    return data_.v4.sin_family == AF_INET ? sizeof(sockaddr_in)
                                          : sizeof(sockaddr_in6);
  }

  /**
   * get capacity of the underlying sockaddr.
   */
  constexpr size_t capacity() const noexcept { return sizeof(data_); }

  /**
   * set the size of valid data of the underlying sockaddr.
   *
   * ~~~{.cpp}
   * basic_endpoint<tcp> ep;
   *
   * if (addrlen < ep.capacity()) {
   *   memcpy(ep.data(), addr, addrlen);
   *   ep.resize(addrlen);
   * }
   * ~~~
   *
   * @param n new size
   *
   * @throws std::length_error if n > capacity
   */
  void resize(size_type n) {
    if (n > capacity()) throw std::length_error("n > capacity()");
  }

 private:
  union {
    sockaddr_in v4;
    sockaddr_in6 v6;
  } data_;
};

template <class CharT, class Traits, class InternetProtocol>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &os,
    const basic_endpoint<InternetProtocol> &ep) {
  std::basic_ostringstream<CharT, Traits> ss;
  if (ep.protocol() == basic_endpoint<InternetProtocol>::protocol_type::v6()) {
    ss << "[" << ep.address() << "]";
  } else {
    ss << ep.address();
  }
  ss << ":" << ep.port();

  os << ss.str();

  return os;
}

// 21.13.3 basic_endpoint comparison

template <class InternetProtocol>
constexpr bool operator==(const basic_endpoint<InternetProtocol> &a,
                          const basic_endpoint<InternetProtocol> &b) noexcept {
  return a.port() == b.port() && a.address() == b.address();
}

template <class InternetProtocol>
constexpr bool operator!=(const basic_endpoint<InternetProtocol> &a,
                          const basic_endpoint<InternetProtocol> &b) noexcept {
  return !(a == b);
}

// 21.9 [internet.address.iter]
template <class Address>
class basic_address_iterator;

template <>
class basic_address_iterator<address_v4> {
 public:
  using value_type = address_v4;
  using difference_type = ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;
  using iterator_category = std::input_iterator_tag;

  basic_address_iterator(const value_type &a) noexcept : addr_{a} {}

  reference operator*() const noexcept { return addr_; }
  pointer operator->() const noexcept { return std::addressof(addr_); }
  basic_address_iterator &operator++() noexcept {
    // increment
    addr_ = value_type{addr_.to_uint() + 1};
    return *this;
  }
  basic_address_iterator operator++(int) noexcept {
    auto tmp = *this;
    ++*this;
    return tmp;
  }
  basic_address_iterator &operator--() noexcept {
    // increment
    addr_ = value_type{addr_.to_uint() - 1};
    return *this;
  }
  basic_address_iterator operator--(int) noexcept {
    auto tmp = *this;
    --*this;
    return tmp;
  }

  bool operator==(const basic_address_iterator &rhs) const noexcept {
    return addr_ == rhs.addr_;
  }

  bool operator!=(const basic_address_iterator &rhs) const noexcept {
    return addr_ != rhs.addr_;
  }

 private:
  value_type addr_;
};

template <>
class basic_address_iterator<address_v6> {
 public:
  using value_type = address_v6;
  using difference_type = ptrdiff_t;
  using pointer = const value_type *;
  using reference = const value_type &;
  using iterator_category = std::input_iterator_tag;

  basic_address_iterator(const value_type &a) noexcept : addr_{a} {}

  reference operator*() const noexcept { return addr_; }
  pointer operator->() const noexcept { return std::addressof(addr_); }
  basic_address_iterator &operator++() noexcept;
  basic_address_iterator operator++(int) noexcept {
    auto tmp = *this;
    ++*this;
    return tmp;
  }
  basic_address_iterator &operator--() noexcept;
  // increment the bytes

  basic_address_iterator operator--(int) noexcept {
    auto tmp = *this;
    --*this;
    return tmp;
  }

 private:
  value_type addr_;
};

using address_v4_iterator = basic_address_iterator<address_v4>;
using address_v6_iterator = basic_address_iterator<address_v6>;

// 21.10 [internet.address.range]
//
template <class Address>
class basic_address_range;

using address_v4_range = basic_address_range<address_v4>;
using address_v6_range = basic_address_range<address_v6>;

template <>
class basic_address_range<address_v4> {
 public:
  using value_type = address_v4;
  using iterator = basic_address_iterator<value_type>;

  basic_address_range() noexcept : begin_({}), end_({}) {}
  basic_address_range(const value_type &first, const value_type &last) noexcept
      : begin_{first}, end_{last} {}

  iterator begin() const noexcept { return begin_; }
  iterator end() const noexcept { return end_; }

  bool empty() const noexcept { return begin_ == end_; }
  size_t size() const noexcept { return end_->to_uint() - begin_->to_uint(); }

  iterator find(const value_type &addr) const noexcept {
    if (*begin_ <= addr && addr < *end_) return iterator{addr};

    return end();
  }

 private:
  iterator begin_;
  iterator end_;
};

template <>
class basic_address_range<address_v6> {
 public:
  using iterator = basic_address_iterator<address_v6>;
};

// 21.11 [internet.network.v4]

class network_v4 {
 public:
  constexpr network_v4() noexcept = default;
  constexpr network_v4(const address_v4 &addr, int prefix_len)
      : addr_{addr}, prefix_len_{prefix_len} {}
  constexpr network_v4(const address_v4 &addr, const address_v4 &mask)
      : addr_{addr} {
    auto m = mask.to_uint();
    uint32_t t{1U << 31U};

    size_t sh{0};
    for (; sh < 32; ++sh) {
      if ((m & t) == 0) break;

      t >>= 1U;
    }

    // TODO(jkneschk): check the remainder is all zero

    prefix_len_ = sh;
  }

  // 21.11.2, mmembers
  constexpr address_v4 address() const noexcept { return addr_; }
  constexpr int prefix_length() const noexcept { return prefix_len_; }
  constexpr address_v4 netmask() const noexcept {
    address_v4::uint_type v{0xffffffff};

    v <<= 32U - prefix_len_;

    return address_v4{v};
  }
  constexpr address_v4 network() const noexcept {
    return address_v4{address().to_uint() & netmask().to_uint()};
  }
  constexpr address_v4 broadcast() const noexcept {
    auto mask = netmask().to_uint();
    address_v4::uint_type v{0xffffffff};
    return address_v4{(address().to_uint() & mask) | (~(v & mask) & v)};
  }
  address_v4_range hosts() const noexcept;
  constexpr network_v4 canonical() const noexcept {
    return {network(), prefix_length()};
  }
  constexpr bool is_host() const noexcept { return prefix_length() == 32; }
  constexpr bool is_subnet_of(const network_v4 &other) const noexcept;

  template <class Allocator = std::allocator<char>>
  std::basic_string<char, std::char_traits<char>, Allocator> to_string(
      const Allocator &a = Allocator()) const {
    return this->address().to_string(a) + "/" + std::to_string(prefix_length());
  }

 private:
  address_v4 addr_{};
  int prefix_len_{0};
};

// 21.11.3 network_v4 comp
constexpr bool operator==(const network_v4 &a, const network_v4 &b) noexcept {
  return a.address() == b.address() && a.prefix_length() == b.prefix_length();
}

constexpr bool network_v4::is_subnet_of(
    const network_v4 &other) const noexcept {
  return other.prefix_length() < prefix_length() &&
         (network_v4(this->address(), other.prefix_length()).canonical() ==
          other.canonical());
}

// 21.11.4 network_v4 creation
//
// 21.11.5 network_v4 io
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &os, const network_v4 &net) {
  os << net.to_string().c_str();

  return os;
}

// 21.12 [internet.network.v6]

class network_v6 {
 public:
  constexpr network_v6() noexcept = default;
  constexpr network_v6(const address_v6 &addr, int prefix_len)
      : addr_{addr}, prefix_len_{prefix_len} {}

  // 21.11.2, mmembers
  constexpr address_v6 address() const noexcept { return addr_; }
  constexpr int prefix_length() const noexcept { return prefix_len_; }
  constexpr address_v6 network() const noexcept {
    const auto address_bytes = address().to_bytes();
    address_v6::bytes_type netmask_bytes{
        networkbits(address_bytes, prefix_len_, 0),
        networkbits(address_bytes, prefix_len_, 1),
        networkbits(address_bytes, prefix_len_, 2),
        networkbits(address_bytes, prefix_len_, 3),
        networkbits(address_bytes, prefix_len_, 4),
        networkbits(address_bytes, prefix_len_, 5),
        networkbits(address_bytes, prefix_len_, 6),
        networkbits(address_bytes, prefix_len_, 7),
        networkbits(address_bytes, prefix_len_, 8),
        networkbits(address_bytes, prefix_len_, 9),
        networkbits(address_bytes, prefix_len_, 10),
        networkbits(address_bytes, prefix_len_, 11),
        networkbits(address_bytes, prefix_len_, 12),
        networkbits(address_bytes, prefix_len_, 13),
        networkbits(address_bytes, prefix_len_, 14),
        networkbits(address_bytes, prefix_len_, 15),
    };

    return address_v6{netmask_bytes};
  }
  address_v6_range hosts() const noexcept;
  constexpr network_v6 canonical() const noexcept {
    return {network(), prefix_length()};
  }
  constexpr bool is_host() const noexcept { return prefix_length() == 128; }
  constexpr bool is_subnet_of(const network_v6 &other) const noexcept;

  template <class Allocator = std::allocator<char>>
  std::basic_string<char, std::char_traits<char>, Allocator> to_string(
      const Allocator &a = Allocator()) const {
    return this->address().to_string(a) + "/" + std::to_string(prefix_length());
  }

 private:
  constexpr unsigned char networkbits(
      const address_v6::bytes_type &address_bytes, int prefix_len,
      int ndx) const {
    return address_bytes[ndx] & leftmostbits(ndx, prefix_len);
  }

  constexpr uint8_t leftmostbits(int ndx, int prefix_len) const {
    int bitndx = ndx * 8;

    if (bitndx > prefix_len) return 0;
    prefix_len -= bitndx;
    if (prefix_len >= 8) return 0xff;

    return 0xff << (8 - prefix_len);
  }

  address_v6 addr_{};
  int prefix_len_{0};
};

// 21.12.3 network_v6 comp
constexpr bool operator==(const network_v6 &a, const network_v6 &b) noexcept {
  return a.address() == b.address() && a.prefix_length() == b.prefix_length();
}

constexpr bool network_v6::is_subnet_of(
    const network_v6 &other) const noexcept {
  return other.prefix_length() < prefix_length() &&
         (network_v6(this->address(), other.prefix_length()).canonical() ==
          other.canonical());
}

// 21.12.4 network_v6 creation
//
// 21.12.5 network_v6 io
template <class CharT, class Traits>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &os, const network_v6 &net) {
  os << net.to_string().c_str();

  return os;
}

/**
 * TCP protocol.
 *
 * - endpoint
 * - socket options
 *
 * ~~~
 * net::ip::tcp::no_delay opt;
 * if (0 == setsockopt(sock, opt.level(), opt.name(), opt.data(), opt.size())) {
 *   opt.resize();
 * }
 * ~~~
 */
class tcp {
 public:
  using endpoint = basic_endpoint<tcp>;
  using resolver = basic_resolver<tcp>;
  using socket = basic_stream_socket<tcp>;
  using acceptor = basic_socket_acceptor<tcp>;

#ifdef TCP_CONGESTION
  // linux, freebsd, solaris
  // using congestion = socket_option::string<IPPROTO_TCP, TCP_CONGESTION>;
#endif
#ifdef TCP_CORK
  // linux, solaris
  using cork = socket_option::boolean<IPPROTO_TCP, TCP_CORK>;
#endif
#ifdef TCP_DEFER_ACCEPT
  // linux
  using defer_accept = socket_option::integer<IPPROTO_TCP, TCP_DEFER_ACCEPT>;
#endif
#ifdef TCP_EXPEDITED_1122
  // windows xp
  using expedited_rfc1122 =
      socket_option::boolean<IPPROTO_TCP, TCP_EXPEDITED_1122>;
#endif
#ifdef TCP_FASTOPEN
  // linux, windows 10 build 1607, freebsd 10
  // freebsd 12 -> struct tcp_fastopen
  using fast_open = socket_option::integer<IPPROTO_TCP, TCP_FASTOPEN>;
#endif
#ifdef TCP_FASTOPEN_CONNECT
  // linux 4.11
  using fast_open_connect =
      socket_option::integer<IPPROTO_TCP, TCP_FASTOPEN_CONNECT>;
#endif
#ifdef TCP_INFO
  // linux, freebsd, solaris
  //
  // windows has GetPerTcpConnectionEStats
  // using info = socket_option::tcp_info<IPPROTO_TCP, TCP_INFO>;
#endif
#ifdef TCP_KEEPINIT
  // freebsd
  using keep_init = socket_option::integer<IPPROTO_TCP, TCP_KEEPINIT>;
#endif
#ifdef TCP_KEEPCNT
  // linux, windows 10 build 1703, freebsd
  using keep_cnt = socket_option::integer<IPPROTO_TCP, TCP_KEEPCNT>;
#endif
#ifdef TCP_KEEPIDLE
  // linux, windows 10 build 1709, freebsd
  using keep_idle = socket_option::integer<IPPROTO_TCP, TCP_KEEPIDLE>;
#endif
#ifdef TCP_KEEPINTVL
  // linux, windows 10 build 1709, freebsd
  using keep_intvl = socket_option::integer<IPPROTO_TCP, TCP_KEEPINTVL>;
#endif
#ifdef TCP_LINGER2
  // linux
  using linger2 = socket_option::integer<IPPROTO_TCP, TCP_LINGER2>;
#endif
#ifdef TCP_MAXRT
  // windows vista
  using maxrt = socket_option::integer<IPPROTO_TCP, TCP_MAXRT>;
#endif
#ifdef TCP_MAXSEG
  // linux, freebsd, solaris, macosx
  using maxseg = socket_option::integer<IPPROTO_TCP, TCP_MAXSEG>;
#endif
#ifdef TCP_MD5SIG
  // linux, freebsd
  // using md5sig = socket_option::md5sig<IPPROTO_TCP, TCP_MD5SIG>;
#endif
#ifdef TCP_NODELAY
  // all
  using no_delay = socket_option::boolean<IPPROTO_TCP, TCP_NODELAY>;
#endif
#ifdef TCP_NOOPT
  // freebsd, macosx
  using noopt = socket_option::boolean<IPPROTO_TCP, TCP_NOOPT>;
#endif
#ifdef TCP_NOPUSH
  // freebsd, macosx
  using no_push = socket_option::boolean<IPPROTO_TCP, TCP_NOPUSH>;
#endif
#ifdef TCP_QUICKACK
  // linux
  using quickack = socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK>;
#endif
#ifdef TCP_SYNCNT
  // linux
  using syncnt = socket_option::integer<IPPROTO_TCP, TCP_SYNCNT>;
#endif
#ifdef TCP_USER_TIMEOUT
  // linux
  using user_timeout = socket_option::integer<IPPROTO_TCP, TCP_USER_TIMEOUT>;
#endif
#ifdef TCP_WINDOW_CLAMP
  // linux
  using window_clamp = socket_option::integer<IPPROTO_TCP, TCP_WINDOW_CLAMP>;
#endif
#ifdef TCP_TIMESTAMPS
  // windows vista
  using timestamps = socket_option::boolean<IPPROTO_TCP, TCP_TIMESTAMPS>;
#endif
#ifdef TCP_NOTSENT_LOWAT
  // linux, macosx
  using not_sent_lowat = socket_option::integer<IPPROTO_TCP, TCP_NOTSENT_LOWAT>;
#endif

  // ## linux
  //
  // TCP_CC_INFO - tcp_cc_info
  // TCP_COOKIE_TRANSACTIONS - struct tcp_cookie_transactions
  // TCP_FASTOPEN_CONNECT
  // TCP_MD5SIG_EXT
  // TCP_QUEUE_SEQ
  // TCP_REPAIR
  // TCP_REPAIR_QUEUE
  // TCP_REPAIR_OPTIONS - struct tcp_repair_opt
  // TCP_REPAIR_WINDOW - struct tcp_repair_window
  // TCP_THIN_LINEAR_TIMEOUTS
  // TCP_THIN_DUPACK
  // TCP_TIMESTAMP
  // TCP_SAVE_SYN
  // TCP_SAVED_SYN
  // TCP_ULP

  // ## solaris
  //
  // TCP_CONN_ABORT_THRESHOLD
  // TCP_ABORT_THRESHOLD
  // TCP_RTO_MIN
  // TCP_RTO_MAX
  // TCP_RTO_INITIAL
  // TCP_INIT_CWND
  //
  // ## macosx
  //
  // TCP_KEEPALIVE
  // TCP_CONNECTIONTIMEOUT
  // TCP_SENDMOREACKS
  // TCP_ENABLE_ENC
  // TCP_CONNECTION_INFO

  static constexpr tcp v4() noexcept { return tcp{AF_INET}; }
  static constexpr tcp v6() noexcept { return tcp{AF_INET6}; }

  constexpr int family() const noexcept { return family_; }
  constexpr int type() const noexcept { return SOCK_STREAM; }
  constexpr int protocol() const noexcept { return IPPROTO_TCP; }

  // tcp() = delete;

 private:
  constexpr explicit tcp(int family) : family_{family} {}

  int family_;
};

constexpr bool operator==(const tcp &a, const tcp &b) noexcept {
  return a.family() == b.family() && a.type() == b.type();
}

constexpr bool operator!=(const tcp &a, const tcp &b) noexcept {
  return !(a == b);
}

// 21.20 internet.udp
//
class udp {
 public:
  using endpoint = basic_endpoint<udp>;
  using resolver = basic_resolver<udp>;
  using socket = basic_datagram_socket<udp>;

  static constexpr udp v4() noexcept { return udp{AF_INET}; }
  static constexpr udp v6() noexcept { return udp{AF_INET6}; }

  constexpr int family() const noexcept { return family_; }
  constexpr int type() const noexcept { return SOCK_DGRAM; }
  constexpr int protocol() const noexcept { return IPPROTO_UDP; }

  // udp() = delete;

 private:
  constexpr explicit udp(int family) : family_{family} {}

  int family_;
};

// 21.20.1 internet.udp.comparisons

constexpr bool operator==(const udp &a, const udp &b) noexcept {
  return a.family() == b.family() && a.type() == b.type();
}

constexpr bool operator!=(const udp &a, const udp &b) noexcept {
  return !(a == b);
}

}  // namespace ip

}  // namespace net

#endif
