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

#include <iostream>  // cout
#include <ostream>
#include <utility>  // pair

#include "mysql/harness/net_ts/impl/netif.h"

/**
 * map flags to name for interfaceflags.
 *
 * a common subset exists on all unixes, but each OS has its own set of flags,
 * where some are shared
 */
static const std::pair<unsigned int, const char *> interface_flags[] = {
#ifdef HAVE_IFADDRS_H
    {IFF_UP, "UP"},                // interface is up
    {IFF_BROADCAST, "BROADCAST"},  // broadcast access is valid
    {IFF_DEBUG, "DEBUG"},          // turn on debugging
    {IFF_LOOPBACK, "LOOPBACK"},    // is a loopback net
    {IFF_POINTOPOINT, "P-to-P"},   // interface is point-to-point link
    {IFF_RUNNING, "RUNNING"},      // interface is RFC2864 OPER_UP
    {IFF_NOARP, "NOARP"},          // no ARP protocol
    {IFF_PROMISC, "PROMISC"},      // receives all packets
    {IFF_ALLMULTI, "ALLMULTI"},    // receives all multicast packets
    {IFF_MULTICAST, "MULTICAST"},  // supports multicast
#ifdef IFF_NOTRAILERS
    // linux, macosx, solaris
    {IFF_NOTRAILERS, "NOTRAILERS"},
#endif
#ifdef IFF_DYNAMIC
    // linux
    {IFF_DYNAMIC, "DYNAMIC"},
#endif
#ifdef IFF_MASTER
    // linux
    {IFF_MASTER, "MASTER"},
#endif
#ifdef IFF_SLAVE
    // linux
    {IFF_SLAVE, "SLAVE"},
#endif
#ifdef IFF_PORTSEL
    // linux
    {IFF_PORTSEL, "PORTSEL"},
#endif
#ifdef IFF_AUTOMEDIA
    // linux
    {IFF_AUTOMEDIA, "AUTOMEDIA"},
#endif
#ifdef IFF_LOWER_UP
    // linux (but not glibc)
    {IFF_LOWER_UP, "LOWER_UP"},
#endif
#ifdef IFF_DORMANT
    // linux (but not glibc)
    {IFF_DORMANT, "DORMANT"},
#endif
#ifdef IFF_ECHO
    // linux (but not glibc)
    {IFF_ECHO, "ECHO"},
#endif
#ifdef IFF_OACTIVE
    // freebsd
    {IFF_OACTIVE, "OACTIVE"},
#endif
#ifdef IFF_SIMPLEX
    // freebsd
    {IFF_SIMPLEX, "SIMPLEX"},
#endif
#ifdef IFF_CANTCONFIG
    // freebsd
    {IFF_CANTCONFIG, "CANTCONFIG"},
#endif
#ifdef IFF_PPROMISC
    // freebsd
    {IFF_PPROMISC, "PPROMISC"},
#endif
#ifdef IFF_MONITOR
    // freebsd
    {IFF_MONITOR, "MONITOR"},
#endif
#ifdef IFF_STATICARP
    // freebsd
    {IFF_STATICARP, "STATICARP"},
#endif
#ifdef IFF_DYING
    // freebsd
    {IFF_DYING, "DYING"},
#endif
#ifdef IFF_RENAMING
    // freebsd
    {IFF_DYING, "RENAMING"},
#endif
#ifdef IFF_NOGROUP
    // freebsd
    {IFF_NOGROUP, "NOGROUP"},
#endif
#ifdef IFF_MULTI_BCAST
    // freebsd, solaris
    {IFF_MULTI_BCAST, "MULTI_BCAST"},
#endif
#ifdef IFF_IPV4
    // freebsd, solaris
    {IFF_IPV4, "IPV4"},
#endif
#ifdef IFF_IPV6
    // freebsd, solaris
    {IFF_IPV6, "IPV6"},
#endif
#ifdef IFF_VIRTUAL
    // freebsd
    {IFF_VIRTUAL, "VIRTUAL"},
#endif
#ifdef IFF_PHYSRUNNING
    // freebsd, solaris
    {IFF_PHYSRUNNING, "PHYSRUNNING"},
#endif
#ifdef IFF_INTELLIGENT
    // solaris
    {IFF_INTELLIGENT, "INTELLIGENT"},
#endif
#endif
#if defined(_WIN32)
    {IP_ADAPTER_DDNS_ENABLED, "DDNS"},            // dynamic DNS enable
    {IP_ADAPTER_REGISTER_ADAPTER_SUFFIX, "SUF"},  // DNS suffix register
    {IP_ADAPTER_DHCP_ENABLED, "DHCPv4"},          // DHCP enabled
    {IP_ADAPTER_RECEIVE_ONLY, "RCV"},             // receive only
    {IP_ADAPTER_NO_MULTICAST, "NOMULTICAST"},     // no multicast
    {IP_ADAPTER_IPV6_OTHER_STATEFUL_CONFIG,
     "IPv6OTHERCONFIG"},  // other IPv6-specific statefile config
    {IP_ADAPTER_NETBIOS_OVER_TCPIP_ENABLED,
     "NETBIOSOverTCP"},                 // netbios over tcpip
    {IP_ADAPTER_IPV4_ENABLED, "IPV4"},  // ipv4 enabled
    {IP_ADAPTER_IPV6_ENABLED, "IPV6"},  // ipv6 enabled
    {IP_ADAPTER_IPV6_MANAGE_ADDRESS_CONFIG,
     "IPV6MANAGEDADDRESS"},  // ipv6 manage address config
#endif
};

/**
 * stringify a interfaceflag.
 */
static std::ostream &operator<<(std::ostream &os,
                                const net::InterfaceFlag &flag) {
  bool need_sep{false};

  for (size_t bit_pos = 0, bit_value = 1;
       bit_pos < sizeof(net::InterfaceFlag::value_type) * 8;
       ++bit_pos, bit_value <<= 1) {
    if (flag.value() & bit_value) {
      if (need_sep) {
        os << ",";
      } else {
        need_sep = true;
      }

      const char *name{};
      for (size_t ndx{};
           ndx < sizeof(interface_flags) / sizeof interface_flags[0]; ++ndx) {
        auto const &f = interface_flags[ndx];
        if (f.first == bit_value) {
          name = f.second;
          break;
        }
      }

      if (name)
        os << name;
      else
        // print flags without a name as numeric value
        os << bit_value;
    }
  }
  return os;
}

int main() {
  net::impl::socket::init();

  net::NetworkInterfaceResolver netif_resolver;

  auto res = netif_resolver.query();
  if (!res) return EXIT_FAILURE;

  for (const auto &netif : *res) {
    std::cout << netif.display_name() << ": "
              << "flags=" << netif.flags().value() << " <" << netif.flags()
              << ">" << std::endl;
    for (auto &nif : netif.v4_networks()) {
      std::cout << "\tinet " << nif << std::endl;
    }

    for (auto &nif : netif.v6_networks()) {
      std::cout << "\tinet6 " << nif;
      std::cout << (nif.address().is_link_local() ? " (link-local)" : "");
      std::cout << (nif.address().is_site_local() ? " (site-local)" : "");
      std::cout << (nif.address().is_v4_mapped() ? " (v4-mapped)" : "");
      std::cout << (nif.address().is_multicast() ? " (multicast)" : "");
      std::cout << (nif.address().is_multicast_node_local()
                        ? " (multicast-node-local)"
                        : "");
      std::cout << (nif.address().is_multicast_link_local()
                        ? " (multicast-link-local)"
                        : "");
      std::cout << (nif.address().is_multicast_site_local()
                        ? " (multicast-site-local)"
                        : "");
      std::cout << (nif.address().is_multicast_org_local()
                        ? " (multicast-org-local)"
                        : "");
      std::cout << (nif.address().is_multicast_global() ? " (global)" : "");
      std::cout << (nif.address().is_loopback() ? " (loopback)" : "");
      std::cout << (nif.address().is_unspecified() ? " (unspec)" : "");

      std::cout << std::endl;
    }
  }

  return 0;
}
