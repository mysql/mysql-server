/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_NETIF_H_
#define MYSQL_HARNESS_NET_TS_IMPL_NETIF_H_

#include <algorithm>  // find_if
#include <forward_list>
#include <list>
#include <string>
#include <string_view>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__) || \
    defined(__sun__)
#define HAVE_IFADDRS_H
#endif

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>  // getifaddr
#include <net/if.h>   // IFF_UP
// linux/if.h defines
//
// - IFF_LOWER_UP
// - IFF_DORMANT
// - IFF_ECHO
#include <netinet/in.h>  // sockaddr_in
#endif
#if defined(_WIN32)
// needs to be included before iphlpapi.h
#include <winsock2.h>

#pragma comment(lib, "iphlpapi.lib")
#include <iphlpapi.h>
#endif

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"

namespace net {

/**
 * flags of the network interface.
 */
class InterfaceFlag {
 public:
  // should be 'unsigned int' which should be least32
  //
  // ... on the other side there is SIOC[GS]LIFFFLAGS on solaris which makes the
  // flags 64bit
#ifdef _WIN32
  using value_type = decltype(IP_ADAPTER_ADDRESSES::Flags);
#else
  using value_type = decltype(ifaddrs::ifa_flags);
#endif

  constexpr InterfaceFlag(value_type v) noexcept : v_{v} {}

  constexpr value_type value() const { return v_; }

 private:
  const value_type v_;
};

/**
 * networks of a network interface.
 */
template <class NetworkT>
class NetworkInterfaceNetworks {
 public:
  using value_type = NetworkT;
  using container_type = std::list<value_type>;
  using const_reference = const value_type &;
  using reference = value_type &;
  using const_iterator = typename container_type::const_iterator;
  using iterator = const_iterator;
  using difference_type = ptrdiff_t;
  using size_type = size_t;

  size_type max_size() const noexcept { return nets_.max_size(); }
  bool empty() const noexcept { return nets_.empty(); }

  const_iterator begin() const { return nets_.begin(); }
  const_iterator end() const { return nets_.end(); }

  void push_back(const value_type &v) { nets_.push_back(v); }

  /**
   * emplace an object in the container.
   *
   * object is created directly inplace in the container.
   */
  template <class... Args>
  auto emplace_back(Args &&... args) {
    return nets_.emplace_back(std::forward<Args>(args)...);
  }

 private:
  container_type nets_;
};

/**
 * an entry in the network interface result.
 */
class NetworkInterfaceEntry {
 public:
  using flags_type = InterfaceFlag;

  NetworkInterfaceEntry(std::string id, std::string display_name,
                        flags_type::value_type flags)
      : id_{std::move(id)},
        display_name_{std::move(display_name)},
        flags_{flags} {}

  std::string id() const { return id_; }
  std::string display_name() const { return display_name_; }
  flags_type flags() const { return flags_; }

  NetworkInterfaceNetworks<net::ip::network_v4> &v4_networks() {
    return net_v4_s_;
  }

  const NetworkInterfaceNetworks<net::ip::network_v4> &v4_networks() const {
    return net_v4_s_;
  }

  NetworkInterfaceNetworks<net::ip::network_v6> &v6_networks() {
    return net_v6_s_;
  }

  const NetworkInterfaceNetworks<net::ip::network_v6> &v6_networks() const {
    return net_v6_s_;
  }

 private:
  std::string id_;
  std::string display_name_;
  flags_type flags_;
  NetworkInterfaceNetworks<net::ip::network_v4> net_v4_s_;
  NetworkInterfaceNetworks<net::ip::network_v6> net_v6_s_;
};

/**
 * results of a NetworkInterfaceResolver::query().
 */
class NetworkInterfaceResults {
 public:
  using value_type = NetworkInterfaceEntry;
  using const_reference = const value_type &;
  using reference = value_type &;
  using const_iterator = typename std::forward_list<value_type>::const_iterator;
  using iterator = const_iterator;
  using difference_type = ptrdiff_t;
  using size_type = size_t;

  NetworkInterfaceResults() = default;

  size_type size() const noexcept { return size_; }
  size_type max_size() const noexcept { return results_.max_size(); }
  bool empty() const noexcept { return results_.empty(); }

  const_iterator begin() const { return results_.begin(); }
  const_iterator end() const { return results_.end(); }
  const_iterator cbegin() const { return results_.cbegin(); }
  const_iterator cend() const { return results_.cend(); }

 protected:
  friend class NetworkInterfaceResolver;

#ifdef HAVE_IFADDRS_H
  /**
   * get the prefix length of a netmask.
   *
   * - on IPv6 addresses the prefix length may be 128 bit
   * - on IPv4 addresses 32bit
   *
   * In '127.0.0.1/8', the /8 means:
   *
   * - number of consecutive bits set in the netmask starting from the MSB
   *
   * `/8` in IPv4: 255.0.0.0
   * `/8` in IPv6: :ff00:...
   */
  template <class BytesClass>
  static constexpr int get_prefix_len(const BytesClass &mask) {
    // count prefix-len
    int prefix_len{0};

    // can't use for-range-loop here as .begin() isn't constexpr
    for (size_t ndx{}; ndx < mask.size(); ++ndx) {
      uint8_t mask_byte = mask[ndx];

      for (uint8_t b = mask_byte; b & 0x80; b <<= 1, ++prefix_len)
        ;

      // if all bytes were set, check the next byte
      if (mask_byte != 0xff) break;
    }

    return prefix_len;
  }

  NetworkInterfaceResults(ifaddrs *ifs) {
    // cleanup the ifaddrs when done
    struct scoped_ifaddrs {
      constexpr scoped_ifaddrs(ifaddrs *captured_ifaddrs)
          : ifaddrs_{captured_ifaddrs} {}
      ~scoped_ifaddrs() {
        if (ifaddrs_) ::freeifaddrs(ifaddrs_);
      }
      ifaddrs *ifaddrs_;
    } sifs{ifs};

    auto tail = results_.before_begin();

    /*
     * ifaddrs is a list of:
     *
     * - AF_INET, lo0, 127.0.0.1
     * - AF_INET6, lo0, ::1
     *
     * the result we return is:
     *
     * lo0:
     *   - AF_INET, 127.0.0.1
     *   - AF_INET6, ::1
     */
    for (auto cur = ifs; cur != nullptr; cur = cur->ifa_next) {
      // if the interface-name isn't found yet, insert it.
      if ((results_.end() == std::find_if(results_.begin(), results_.end(),
                                          [&cur](const auto &v) {
                                            return v.id() == cur->ifa_name;
                                          }))) {
        // not found
        tail = results_.emplace_after(tail, cur->ifa_name, cur->ifa_name,
                                      cur->ifa_flags);
        ++size_;
      }

      auto cur_res_it = std::find_if(
          results_.begin(), results_.end(),
          [&cur](const auto &v) { return v.id() == cur->ifa_name; });

      if (cur->ifa_addr) {
        // if a address family is assigned, capture it.
        switch (cur->ifa_addr->sa_family) {
          case AF_INET: {
            auto *sa = reinterpret_cast<const sockaddr_in *>(cur->ifa_addr);
            net::ip::address_v4::bytes_type bytes;

            if (bytes.size() < sizeof(sa->sin_addr.s_addr)) std::terminate();
            std::memcpy(bytes.data(), &(sa->sin_addr), sizeof(sa->sin_addr));

            net::ip::address_v4 addr{bytes};

            sa = reinterpret_cast<const sockaddr_in *>(cur->ifa_netmask);
            if (bytes.size() < sizeof(sa->sin_addr.s_addr)) std::terminate();
            std::memcpy(bytes.data(), &(sa->sin_addr.s_addr),
                        sizeof(sa->sin_addr.s_addr));
            net::ip::address_v4 netmask{bytes};

            auto prefix_len = get_prefix_len(netmask.to_bytes());
            cur_res_it->v4_networks().emplace_back(addr, prefix_len);

            // check get_prefix_len works for v4-addresses
            static_assert(get_prefix_len(net::ip::address_v4::bytes_type(
                              0x80, 0x00, 0x00, 0x00)) == 1,
                          "");
            static_assert(get_prefix_len(net::ip::address_v4::bytes_type(
                              0xff, 0x00, 0x00, 0x00)) == 8,
                          "");
            static_assert(get_prefix_len(net::ip::address_v4::bytes_type(
                              0xff, 0x80, 0x00, 0x00)) == 9,
                          "");

            // invalid case.
            static_assert(get_prefix_len(net::ip::address_v4::bytes_type(
                              0x00, 0x80, 0x00, 0x00)) == 0,
                          "");

            break;
          }
          case AF_INET6: {
            auto *sa = reinterpret_cast<const sockaddr_in6 *>(cur->ifa_addr);
            net::ip::address_v6::bytes_type bytes;

            if (bytes.size() < sizeof(sa->sin6_addr.s6_addr)) std::terminate();

            std::memcpy(bytes.data(), &(sa->sin6_addr.s6_addr),
                        sizeof(sa->sin6_addr.s6_addr));
            net::ip::address_v6 addr{bytes, sa->sin6_scope_id};

            sa = reinterpret_cast<const sockaddr_in6 *>(cur->ifa_netmask);
            if (bytes.size() < sizeof(sa->sin6_addr.s6_addr)) std::terminate();
            std::memcpy(bytes.data(), &(sa->sin6_addr.s6_addr),
                        sizeof(sa->sin6_addr.s6_addr));
            net::ip::address_v6 netmask{bytes};

            auto prefix_len = get_prefix_len(netmask.to_bytes());
            cur_res_it->v6_networks().emplace_back(addr, prefix_len);

            break;
          }
          default:
            // ignore the other address-family types
            break;
        }
      }
    }
  }

#elif defined(_WIN32)
  static stdx::expected<std::string, std::error_code> convert_wstring_to_utf8(
      const std::wstring_view &ws) {
    std::string out;

    // first, call it with 0 to get the buffer length
    auto out_len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), ws.size(),
                                       nullptr, 0, nullptr, nullptr);

    if (0 == out_len) {
      return stdx::unexpected(
          std::error_code(GetLastError(), std::system_category()));
    }

    out.resize(out_len);

    out_len =
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), ws.size(), &out.front(),
                            out.capacity(), nullptr, nullptr);
    if (0 == out_len) {
      return stdx::unexpected(
          std::error_code(GetLastError(), std::system_category()));
    }

    out.resize(out_len);

    return out;
  }

  NetworkInterfaceResults(
      std::unique_ptr<IP_ADAPTER_ADDRESSES, decltype(&free)> &&ifs) {
    auto tail = results_.before_begin();

    for (auto cur = ifs.get(); cur; cur = cur->Next) {
      tail = results_.emplace_after(tail, std::string{cur->AdapterName},
                                    convert_wstring_to_utf8(cur->Description)
                                        .value_or("<invalid-wstring>"),
                                    cur->Flags);
      ++size_;

      auto cur_res_it = std::find_if(
          results_.begin(), results_.end(),
          [&cur](const auto &v) { return v.id() == cur->AdapterName; });

      for (auto cur_unicast_addr = cur->FirstUnicastAddress; cur_unicast_addr;
           cur_unicast_addr = cur_unicast_addr->Next) {
        if (cur_unicast_addr->Address.lpSockaddr->sa_family == AF_INET) {
          auto *sa = reinterpret_cast<const sockaddr_in *>(
              cur_unicast_addr->Address.lpSockaddr);
          net::ip::address_v4::bytes_type bytes;
          std::memcpy(bytes.data(), &(sa->sin_addr.s_addr),
                      sizeof(sa->sin_addr.s_addr));
          net::ip::address_v4 addr{bytes};

          cur_res_it->v4_networks().emplace_back(
              addr, cur_unicast_addr->OnLinkPrefixLength);
        } else {
          auto *sa = reinterpret_cast<const sockaddr_in6 *>(
              cur_unicast_addr->Address.lpSockaddr);
          net::ip::address_v6::bytes_type bytes;
          std::memcpy(bytes.data(), &(sa->sin6_addr.s6_addr),
                      sizeof(sa->sin6_addr.s6_addr));
          net::ip::address_v6 addr{bytes, sa->sin6_scope_id};
          cur_res_it->v6_networks().emplace_back(
              addr, cur_unicast_addr->OnLinkPrefixLength);
        }
      }
    }
  }
#endif

  std::forward_list<value_type> results_;
  size_t size_{0};
};

class NetworkInterfaceResolver {
 public:
  stdx::expected<NetworkInterfaceResults, std::error_code> query() {
#ifdef _WIN32
    unsigned long ifs_size{0};
    auto res =
        ::GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &ifs_size);

    if (res != ERROR_BUFFER_OVERFLOW) {
      return stdx::unexpected(
          std::error_code{static_cast<int>(res), std::system_category()});
    }

    std::unique_ptr<IP_ADAPTER_ADDRESSES, decltype(&free)> ifs(
        reinterpret_cast<IP_ADAPTER_ADDRESSES *>(malloc(ifs_size)), &free);

    res = ::GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, ifs.get(), &ifs_size);
    if (ERROR_SUCCESS != res) {
      return stdx::unexpected(
          std::error_code{static_cast<int>(res), std::system_category()});
    }

    return NetworkInterfaceResults{std::move(ifs)};
#else
    ifaddrs *ifs = nullptr;

    if (-1 == ::getifaddrs(&ifs)) {
      return stdx::unexpected(net::impl::socket::last_error_code());
    }

    return NetworkInterfaceResults{ifs};
#endif
  }
};
}  // namespace net

#endif
