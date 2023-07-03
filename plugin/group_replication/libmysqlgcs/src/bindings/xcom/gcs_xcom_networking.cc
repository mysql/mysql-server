/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <errno.h>
#include <algorithm>
#include <bitset>
#include <cstring>
#include <set>

#ifndef _WIN32
#include <netdb.h>
#endif

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe.h"

#if defined(_WIN32)
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_win32.h"
#else
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_ix.h"
#endif

/**
 Check if it contains an attempt of having an IP v4 address.
 It does not check for its validity, but only if it contains all authorized
 characters: numbers and dots.

 @return true if it exclusively contains all authorized characters.
 */
bool is_ipv4_address(const std::string &possible_ip) {
  std::string::const_iterator it = possible_ip.begin();
  while (it != possible_ip.end() &&
         (isdigit(static_cast<unsigned char>(*it)) || (*it) == '.')) {
    ++it;
  }
  return !possible_ip.empty() && it == possible_ip.end();
}

/**
 Check if it contains an attempt of having an IP v6 address.
 It does not check for its validity, but it checks if it contains : character

 @return true if it contains the : character.
 */
bool is_ipv6_address(const std::string &possible_ip) {
  return !possible_ip.empty() &&
         possible_ip.find_first_of(':') != std::string::npos;
}

/**
 Determines if a given address is an IP localhost address

 @param[in] address a reference to a string containing the address to test

 @return true if localhost, false otherwise.
 */
static bool is_address_localhost(const std::string &address) {
  std::string lower_address(address);

  std::transform(lower_address.begin(), lower_address.end(),
                 lower_address.begin(), ::tolower);

  return (strcmp(lower_address.c_str(), "::ffff:127.0.0.1/128") == 0) ||
         (strcmp(lower_address.c_str(), "::1/128") == 0) ||
         (strcmp(lower_address.c_str(), "127.0.0.1/32") == 0) ||
         (strcmp(lower_address.c_str(), "localhost/32") == 0);
}

bool get_local_addresses(Gcs_sock_probe_interface &sock_probe_if,
                         std::map<std::string, int> &addr_to_cidr_bits,
                         bool filter_out_inactive) {
  sock_probe *s = (sock_probe *)calloc(1, sizeof(sock_probe));

  if (sock_probe_if.init_sock_probe(s) < 0) {
    free(s);
    return true;
  }

  if (sock_probe_if.number_of_interfaces(s) == 0) {
    MYSQL_GCS_LOG_WARN(
        "Unable to probe any network interface "
        "for IP and netmask information. No addresses "
        "collected!");

    sock_probe_if.close_sock_probe(s);
    return true;
  }

  for (int j = 0; j < sock_probe_if.number_of_interfaces(s); j++) {
    if (!filter_out_inactive || sock_probe_if.is_if_running(s, j)) {
      char sname[INET6_ADDRSTRLEN];
      char smask[INET6_ADDRSTRLEN];

      sockaddr *ip = nullptr, *netmask = nullptr;

      sock_probe_if.get_sockaddr_address(s, j, &ip);
      sock_probe_if.get_sockaddr_netmask(s, j, &netmask);

      if (ip == nullptr || netmask == nullptr) {
        char *if_name = sock_probe_if.get_if_name(s, j);

        MYSQL_GCS_LOG_INFO(
            "Unable to probe network interface \""
            << ((if_name && strlen(if_name) > 0) ? if_name : "<unknown>")
            << "\" for IP and netmask information. Skipping!");
        continue;
      }

      if (ip->sa_family == AF_INET) {
        struct in_addr *inaddr = nullptr, *inmask = nullptr;

        inaddr = &((struct sockaddr_in *)ip)->sin_addr;
        inmask = &((struct sockaddr_in *)netmask)->sin_addr;

        // byte order does not matter, only how many bits are set does
        std::bitset<sizeof(unsigned long) * 8> prefix(inmask->s_addr);

        sname[0] = smask[0] = '\0';

        if (!inet_ntop(AF_INET, inaddr, sname,
                       static_cast<socklen_t>(sizeof(sname))) ||
            !inet_ntop(AF_INET, inmask, smask,
                       static_cast<socklen_t>(sizeof(smask)))) {
          char *if_name = sock_probe_if.get_if_name(s, j);

          MYSQL_GCS_LOG_INFO(
              "Unable to probe network interface \""
              << ((if_name && strlen(if_name) > 0) ? if_name : "<unknown>")
              << "\" for IP and netmask information. Skipping!");
          continue;
        }

        addr_to_cidr_bits.insert(std::make_pair(sname, prefix.count()));
      } else if (ip->sa_family == AF_INET6) {
        struct in6_addr *inaddrv6 = nullptr, *inmaskv6 = nullptr;

        inaddrv6 = &((struct sockaddr_in6 *)ip)->sin6_addr;
        inmaskv6 = &((struct sockaddr_in6 *)netmask)->sin6_addr;

        // byte order does not matter, only how many bits are set does
        std::ostringstream binary_string;
        for (int ipv6_bytes = 0; ipv6_bytes < 16; ipv6_bytes++) {
          std::bitset<8> prefix_unit(inmaskv6->s6_addr[ipv6_bytes]);
          binary_string << prefix_unit.to_string();
        }

        std::bitset<(4 * sizeof(unsigned long) * 8)> prefix(
            binary_string.str());

        sname[0] = smask[0] = '\0';

        if (!inet_ntop(AF_INET6, inaddrv6, sname,
                       static_cast<socklen_t>(sizeof(sname))) ||
            !inet_ntop(AF_INET6, inmaskv6, smask,
                       static_cast<socklen_t>(sizeof(smask)))) {
          char *if_name = sock_probe_if.get_if_name(s, j);

          MYSQL_GCS_LOG_INFO(
              "Unable to probe network interface \""
              << ((if_name && strlen(if_name) > 0) ? if_name : "<unknown>")
              << "\" for IP and netmask information. Skipping!");
          continue;
        }
        addr_to_cidr_bits.insert(std::make_pair(sname, prefix.count()));
      }
    }
  }

  sock_probe_if.close_sock_probe(s);

  return addr_to_cidr_bits.empty();
}

/**
 This function gets all private network addresses and their
 subnet masks as a string

 In the Internet addressing architecture, a private network is a network that
 uses private IP address space. Both, the IPv4 and the IPv6 specifications
 define private addressing ranges. These addresses are commonly used for
 local area networks (LANs) in residential, office, and enterprise environments.
 Private IP address spaces were originally defined in an effort to delay IPv4
 address exhaustion.
 */
bool get_local_private_addresses(std::map<std::string, int> &out,
                                 bool filter_out_inactive) {
  std::map<std::string, int> addr_to_cidr;
  std::map<std::string, int>::iterator it;

  Gcs_sock_probe_interface *sock_probe_if = new Gcs_sock_probe_interface_impl();
  get_local_addresses(*sock_probe_if, addr_to_cidr, filter_out_inactive);
  delete sock_probe_if;

  /* IP v4 local addresses are defined in the IP Standard:
  - Class A - 24-bit block	10.0.0.0 – 10.255.255.255
  - Class B - 20-bit block	172.16.0.0 – 172.31.255.255
  - Class C - 16-bit block	192.168.0.0 – 192.168.255.255
  */
  for (it = addr_to_cidr.begin(); it != addr_to_cidr.end(); it++) {
    std::string ip = it->first;
    int cidr = it->second;

    int part1, part2, part3, part4;
    sscanf(ip.c_str(), "%d.%d.%d.%d", &part1, &part2, &part3, &part4);

    if ((part1 == 192 && part2 == 168 && cidr >= 16) ||
        (part1 == 172 && part2 >= 16 && part2 <= 31 && cidr >= 12) ||
        (part1 == 10 && cidr >= 8) ||
        (part1 == 127 && part2 == 0 && part3 == 0 && part4 == 1)) {
      out.insert(std::make_pair(ip, cidr));
    }
  }

  /*
  IP v6 standard defines two type of Local Addresses:
  - ::1 is the localhost
  - fe80::/10 is reserved for IP address autoconfiguration
  - fd00::/8, designed for /48 routing blocks, in which users can create
  multiple subnets
 */
  for (it = addr_to_cidr.begin(); it != addr_to_cidr.end(); it++) {
    std::string ip = it->first;
    int cidr = it->second;

    if (ip.compare("::1") == 0 || ip.compare(0, 2, "fd") == 0 ||
        ip.compare(0, 4, "fe80") == 0) {
      out.insert(std::make_pair(ip, cidr));
    }
  }

  return false;
}

bool resolve_ip_addr_from_hostname(std::string name,
                                   std::vector<std::string> &ip) {
  int res = true;
  char cip[INET6_ADDRSTRLEN];
  socklen_t cip_len = static_cast<socklen_t>(sizeof(cip));
  struct addrinfo *addrinf = nullptr, *addrinf_cycle = nullptr, hints;
  struct sockaddr *sa = nullptr;
  void *in_addr = nullptr;

  memset(&hints, 0, sizeof(hints));
  checked_getaddrinfo(name.c_str(), nullptr, &hints, &addrinf);
  if (!addrinf) goto end;

  addrinf_cycle = addrinf;
  while (addrinf_cycle) {
    sa = (struct sockaddr *)addrinf_cycle->ai_addr;
    switch (sa->sa_family) {
      case AF_INET:
        in_addr = &((struct sockaddr_in *)sa)->sin_addr;
        break;
      case AF_INET6:
        in_addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
        break;
      default:
        continue;
    }
    memset(cip, '\0', cip_len);
    if (!inet_ntop(sa->sa_family, in_addr, cip, cip_len)) goto end;

    std::string resolved_ip(cip);
    ip.push_back(resolved_ip);

    addrinf_cycle = addrinf_cycle->ai_next;
  }
  res = false;

end:
  if (addrinf) freeaddrinfo(addrinf);

  return res;
}

bool resolve_all_ip_addr_from_hostname(
    std::string name, std::vector<std::pair<sa_family_t, std::string>> &ips) {
  int res = true;
  char cip[INET6_ADDRSTRLEN];
  socklen_t cip_len = static_cast<socklen_t>(sizeof(cip));
  struct addrinfo *addrinf = nullptr, *addrinfo_list = nullptr, hints;
  struct sockaddr *sa = nullptr;
  void *in_addr = nullptr;

  memset(&hints, 0, sizeof(hints));
  checked_getaddrinfo(name.c_str(), nullptr, &hints, &addrinf);
  if (!addrinf) goto end;

  addrinfo_list = addrinf;
  while (addrinfo_list) {
    sa = (struct sockaddr *)addrinfo_list->ai_addr;

    switch (sa->sa_family) {
      case AF_INET:
        in_addr = &((struct sockaddr_in *)sa)->sin_addr;
        break;

      case AF_INET6:
        in_addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
        break;

      default:
        addrinfo_list = addrinfo_list->ai_next;
        continue;
    }

    if (!inet_ntop(sa->sa_family, in_addr, cip, cip_len)) goto end;

    ips.push_back(std::make_pair(sa->sa_family, std::string(cip)));

    addrinfo_list = addrinfo_list->ai_next;
  }

  res = ips.empty();

end:
  if (addrinf) freeaddrinfo(addrinf);

  return res;
}

/**
  Given the address as a string, gets the IP encoded as
  an integer.
 */
bool string_to_sockaddr(const std::string &addr, struct sockaddr_storage *sa) {
  /**
    Try IPv4 first.
   */
  sa->ss_family = AF_INET;
  if (inet_pton(AF_INET, addr.c_str(),
                &(((struct sockaddr_in *)sa)->sin_addr)) == 1)
    return false;

  /**
    Try IPv6.
   */
  sa->ss_family = AF_INET6;
  if (inet_pton(AF_INET6, addr.c_str(),
                &(((struct sockaddr_in6 *)sa)->sin6_addr)) == 1)
    return false;

  return true;
}

/**
  Returns the address in unsigned integer.
 */
static bool sock_descriptor_to_sockaddr(int fd, struct sockaddr_storage *sa) {
  int res = 0;
  memset(sa, 0, sizeof(struct sockaddr_storage));
  socklen_t addr_size = static_cast<socklen_t>(sizeof(struct sockaddr_storage));
  if (!(res = getpeername(fd, (struct sockaddr *)sa, &addr_size))) {
    if (sa->ss_family != AF_INET && sa->ss_family != AF_INET6) {
      MYSQL_GCS_LOG_DEBUG(
          "Connection is not from an IPv4 nor IPv6 address. "
          "This is not supported. Refusing the connection!");
      res = 1;
    }
  } else {
    int err = errno;
    switch (err) {
      case EBADF:
        MYSQL_GCS_LOG_DEBUG("The file descriptor fd=%d is not valid", fd);
        break;
      case EFAULT:
        MYSQL_GCS_LOG_DEBUG(
            "The sockaddr_storage pointer sa=%p points to memory not in a "
            "valid part of the process address space",
            sa);
        break;
      case EINVAL:
        MYSQL_GCS_LOG_DEBUG("The value of addr_size=%lu is invalid", addr_size);
        break;
      case ENOBUFS:
        MYSQL_GCS_LOG_DEBUG(
            "Insufficient resources were available in the system to perform "
            "the getpeername operation");
        break;
      case ENOTCONN:
        MYSQL_GCS_LOG_DEBUG("The socket fd=%d is not connected", fd);
        break;
      case ENOTSOCK:
        MYSQL_GCS_LOG_DEBUG(
            "The file descriptor fd=%d does not refer to a socket", fd);
        break;
      default:
        MYSQL_GCS_LOG_DEBUG(
            "Unable to perform getpeername, therefore refusing connection.");
        break;
    }
  }
  return res ? true : false;
}

/**
  This function is a frontend function to inet_ntop.
  */
static bool sock_descriptor_to_string(int fd, std::string &out) {
  struct sockaddr_storage sa;
  socklen_t addr_size = static_cast<socklen_t>(sizeof(struct sockaddr_storage));
  char saddr[INET6_ADDRSTRLEN];

  // get the sockaddr struct
  sock_descriptor_to_sockaddr(fd, &sa);

  // try IPv4
  if (sa.ss_family == AF_INET) {
    if (inet_ntop(AF_INET, &(((struct sockaddr_in *)&sa)->sin_addr), saddr,
                  addr_size)) {
      out = saddr;
      return false;
    }
  }

  // try IPv6
  if (sa.ss_family == AF_INET6) {
    if (inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&sa)->sin6_addr), saddr,
                  addr_size)) {
      out = saddr;
      return false;
    }
  }

  // no go, return error
  return true;
}

int Gcs_sock_probe_interface_impl::init_sock_probe(sock_probe *s) {
  return ::init_sock_probe(s);
}

int Gcs_sock_probe_interface_impl::number_of_interfaces(sock_probe *s) {
  return ::number_of_interfaces(s);
}

void Gcs_sock_probe_interface_impl::get_sockaddr_address(
    sock_probe *s, int count, struct sockaddr **out) {
  ::get_sockaddr_address(s, count, out);
}

void Gcs_sock_probe_interface_impl::get_sockaddr_netmask(
    sock_probe *s, int count, struct sockaddr **out) {
  ::get_sockaddr_netmask(s, count, out);
}
char *Gcs_sock_probe_interface_impl::get_if_name(sock_probe *s, int count) {
  return ::get_if_name(s, count);
}

void Gcs_sock_probe_interface_impl::close_sock_probe(sock_probe *s) {
  ::close_sock_probe(s);
}

bool_t Gcs_sock_probe_interface_impl::is_if_running(sock_probe *s, int count) {
  return ::is_if_running(s, count);
}

/*
  The default allowlist contains all locak-link and private address ranges.
  Please refer to the documentation of get_local_private_addresses() to a better
  understanding of this concept.
*/
const std::string Gcs_ip_allowlist::DEFAULT_ALLOWLIST =
    "127.0.0.1/32,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,::1/128,fe80::/"
    "10,fd00::/8";

Gcs_ip_allowlist_entry::Gcs_ip_allowlist_entry(std::string addr,
                                               std::string mask)
    : m_addr(addr), m_mask(mask) {}

Gcs_ip_allowlist_entry_ip::Gcs_ip_allowlist_entry_ip(std::string addr,
                                                     std::string mask)
    : Gcs_ip_allowlist_entry(addr, mask) {}

bool Gcs_ip_allowlist_entry_ip::init_value() {
  bool error = get_address_for_allowlist(get_addr(), get_mask(), m_value);

  return error;
}

std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
    *Gcs_ip_allowlist_entry_ip::get_value() {
  return new std::vector<
      std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>(
      {m_value});
}

Gcs_ip_allowlist_entry_hostname::Gcs_ip_allowlist_entry_hostname(
    std::string addr, std::string mask)
    : Gcs_ip_allowlist_entry(addr, mask) {}

Gcs_ip_allowlist_entry_hostname::Gcs_ip_allowlist_entry_hostname(
    std::string addr)
    : Gcs_ip_allowlist_entry(addr, "") {}

bool Gcs_ip_allowlist_entry_hostname::init_value() { return false; }

std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
    *Gcs_ip_allowlist_entry_hostname::get_value() {
  bool error = false;
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> value;

  std::vector<std::pair<sa_family_t, std::string>> ips;
  if (resolve_all_ip_addr_from_hostname(get_addr(), ips)) {
    MYSQL_GCS_LOG_WARN("Hostname "
                       << get_addr().c_str() << " in Allowlist"
                       << " configuration was not resolvable. Please check your"
                       << " Allowlist configuration.");
    return nullptr;
  }

  auto has_v4_addresses_it =
      std::find_if(ips.begin(), ips.end(),
                   [](std::pair<sa_family_t, std::string> const &ip_entry) {
                     return ip_entry.first == AF_INET;
                   });
  bool has_v4_addresses = has_v4_addresses_it != ips.end();

  auto *retval = new std::vector<
      std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>();

  for (auto &ip : ips) {
    if (has_v4_addresses && ip.first == AF_INET6) continue;

    std::string mask = get_mask();
    // If mask is empty, lets hand out a default value.
    // 32 to IPv4 and 128 to IPv6
    if (mask.empty()) {
      if (is_ipv4_address(ip.second))
        mask.append("32");
      else
        mask.append("128");
    }

    error = get_address_for_allowlist(ip.second, mask, value);

    if (error) return nullptr;

    retval->push_back(make_pair(value.first, value.second));
  }

  return retval;
}

/* purecov: begin deadcode */
std::string Gcs_ip_allowlist::to_string() const {
  std::set<Gcs_ip_allowlist_entry *>::const_iterator wl_it;
  std::stringstream ss;

  for (wl_it = m_ip_allowlist.begin(); wl_it != m_ip_allowlist.end(); wl_it++) {
    ss << (*wl_it)->get_addr() << "/" << (*wl_it)->get_mask() << ",";
  }

  std::string res = ss.str();
  res.erase(res.end() - 1);
  return res;
}
/* purecov: end */

bool Gcs_ip_allowlist::is_valid(const std::string &the_list) {
  // lock the list
  Atomic_lock_guard guard{m_atomic_guard};

  // copy the string
  std::string allowlist = the_list;

  // remove trailing whitespaces
  allowlist.erase(std::remove(allowlist.begin(), allowlist.end(), ' '),
                  allowlist.end());

  std::stringstream list_ss(allowlist);
  std::string list_entry;

  // split list by commas
  while (std::getline(list_ss, list_entry, ',')) {
    bool is_valid_ip = false;
    struct sockaddr_storage sa;
    unsigned int imask;
    std::stringstream entry_ss(list_entry);
    std::string ip, mask;

    // get ip and netmasks
    std::getline(entry_ss, ip, '/');
    std::getline(entry_ss, mask, '/');

    // Verify that this is a valid IPv4 or IPv6 address
    if (is_ipv4_address(ip) || is_ipv6_address(ip)) {
      is_valid_ip = !string_to_sockaddr(ip, &sa);
    } else {  // We won't check for hostname validity here.
      continue;
    }

    // convert the netbits from the mask to integer
    imask = (unsigned int)atoi(mask.c_str());

    // check if everything is valid
    if ((!is_valid_ip) ||                       // check for valid IP
        (!mask.empty() && !is_number(mask)) ||  // check that mask is a number
        (sa.ss_family == AF_INET6 &&
         imask > 128) ||  // check that IPv6 mask is within range
        (sa.ss_family == AF_INET &&
         imask > 32))  // check that IPv4 mask is within range
    {
      MYSQL_GCS_LOG_ERROR("Invalid IP or subnet mask in the allowlist: "
                          << ip << (mask.empty() ? "" : "/")
                          << (mask.empty() ? "" : mask));
      return false;
    }
  }

  return true;
}

bool Gcs_ip_allowlist::configure(const std::string &the_list) {
  // lock the list
  Atomic_lock_guard guard{m_atomic_guard};

  // copy the list
  std::string allowlist = the_list;
  m_original_list.assign(allowlist);

  // clear the list
  this->clear();

  // remove whitespaces
  allowlist.erase(std::remove(allowlist.begin(), allowlist.end(), ' '),
                  allowlist.end());

  std::stringstream list_ss(allowlist);
  std::string list_entry;

  // parse commas
  bool found_localhost_entry = false;
  while (std::getline(list_ss, list_entry, ',')) {
    std::stringstream entry_ss(list_entry);
    std::string ip, mask;

    /**
      Check if the address is a localhost ipv4 address.
      Add it after if necessary.
    */
    if (!found_localhost_entry) {
      found_localhost_entry = is_address_localhost(entry_ss.str());
    }

    std::getline(entry_ss, ip, '/');
    std::getline(entry_ss, mask, '/');

    add_address(ip, mask);
  }

  // make sure that we always allow connections from localhost
  // so that we are able to connect to our embedded xcom

  // add IPv4 localhost addresses if needed
  if (!found_localhost_entry) {
    if (!add_address("127.0.0.1", "32")) {
      MYSQL_GCS_LOG_WARN(
          "Automatically adding IPv4 localhost address to the "
          "allowlist. It is mandatory that it is added.");
    } else {
      MYSQL_GCS_LOG_ERROR(
          "Error adding IPv4 localhost address automatically"
          " to the allowlist");
    }

    if (!add_address("::1", "128")) {
      MYSQL_GCS_LOG_WARN(
          "Automatically adding IPv6 localhost address to the "
          "allowlist. It is mandatory that it is added.");
    } else {
      MYSQL_GCS_LOG_ERROR(
          "Error adding IPv6 localhost address automatically"
          " to the allowlist");
    }
  }

  return false;
}

bool get_address_for_allowlist(
    std::string addr, std::string mask,
    std::pair<std::vector<unsigned char>, std::vector<unsigned char>>
        &out_pair) {
  struct sockaddr_storage sa;
  unsigned char *sock;
  size_t netmask_len = 0;
  int netbits = 0;
  std::vector<unsigned char> ssock;

  // zero the memory area
  memset(&sa, 0, sizeof(struct sockaddr_storage));

  // fill in the struct sockaddr
  if (string_to_sockaddr(addr, &sa)) return true;

  switch (sa.ss_family) {
    case AF_INET:
      sock = (unsigned char *)&((struct sockaddr_in *)&sa)->sin_addr;
      ssock.assign(sock, sock + sizeof(struct in_addr));
      netmask_len = sizeof(struct in_addr);
      netbits = mask.empty() ? 32 : atoi(mask.c_str());
      break;

      /* purecov: begin deadcode */
    case AF_INET6:
      sock = (unsigned char *)&((struct sockaddr_in6 *)&sa)->sin6_addr;
      ssock.assign(sock, sock + sizeof(struct in6_addr));
      netmask_len = sizeof(struct in6_addr);
      netbits = mask.empty() ? 128 : atoi(mask.c_str());
      break;
      /* purecov: end */
    default:
      return true;
  }

  std::vector<unsigned char> smask;

  // Set the first netbits/8 BYTEs to 255.
  smask.resize(static_cast<size_t>(netbits / 8), 0xff);

  if (smask.size() < netmask_len) {
    // Set the following netbits%8 BITs to 1.
    smask.push_back(static_cast<unsigned char>(0xff << (8 - netbits % 8)));
    // Set non-net part to 0
    smask.resize(netmask_len, 0);
  }

  out_pair = std::make_pair(ssock, smask);

  return false;
}

void Gcs_ip_allowlist::clear() {
  std::set<Gcs_ip_allowlist_entry *>::const_iterator wl_it =
      m_ip_allowlist.begin();
  while (wl_it != m_ip_allowlist.end()) {
    delete (*wl_it);
    m_ip_allowlist.erase(wl_it++);
  }
}

Gcs_ip_allowlist::~Gcs_ip_allowlist() { this->clear(); }

bool Gcs_ip_allowlist::add_address(std::string addr, std::string mask) {
  Gcs_ip_allowlist_entry *addr_for_wl;
  struct sockaddr_storage sa;
  if (!string_to_sockaddr(addr, &sa)) {
    addr_for_wl = new Gcs_ip_allowlist_entry_ip(addr, mask);
  } else {
    addr_for_wl = new Gcs_ip_allowlist_entry_hostname(addr, mask);
  }
  bool error = addr_for_wl->init_value();

  if (!error) {
    std::pair<std::set<Gcs_ip_allowlist_entry *,
                       Gcs_ip_allowlist_entry_pointer_comparator>::iterator,
              bool>
        result;
    result = m_ip_allowlist.insert(addr_for_wl);

    error = !result.second;
  }

  return error;
}

bool Gcs_ip_allowlist::do_check_block_allowlist(
    std::vector<unsigned char> const &incoming_octets) const {
  /*
    Check if the incoming IP matches any IP-mask combination in the allowlist.
    The check compares both IPs' bytes (octets) in network byte order.
  */
  bool block = true;
  for (auto &wl_it : m_ip_allowlist) {
    std::unique_ptr<std::vector<
        std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>>
        wl_value((*wl_it).get_value());

    if (wl_value == nullptr) continue;

    for (auto &wl_value_entry : *wl_value) {
      std::vector<unsigned char> const &wl_range_octets = wl_value_entry.first;
      std::vector<unsigned char> const &wl_netmask_octets =
          wl_value_entry.second;

      // no point in comparing different families, e.g. IPv4 with IPv6
      if (incoming_octets.size() != wl_range_octets.size()) continue;

      for (size_t octet = 0; octet < wl_range_octets.size(); octet++) {
        unsigned char const &oct_in_ip = incoming_octets[octet];
        unsigned char const &oct_range_ip = wl_range_octets[octet];
        unsigned char const &oct_mask_ip = wl_netmask_octets[octet];
        // bail out on the first octet mismatch -- try next IP
        if ((block = (oct_in_ip & oct_mask_ip) != (oct_range_ip & oct_mask_ip)))
          break;
      }

      if (!block) return block;  // This breaks the multiple entry cycle
    }
  }
  return block;
}

bool Gcs_ip_allowlist::do_check_block_xcom(
    std::vector<unsigned char> const &incoming_octets,
    site_def const *xcom_config) const {
  /*
    Check if the incoming IP matches the IP of any XCom member.
    The check compares both IPs' bytes (octets) in network byte order.
  */
  bool block = true;
  for (u_int i = 0; i < xcom_config->nodes.node_list_len && block; i++) {
    Gcs_xcom_node_address xcom_addr(
        std::string(xcom_config->nodes.node_list_val[i].address));
    struct sockaddr_storage xcom_sa;
    std::unique_ptr<Gcs_ip_allowlist_entry> xcom_addr_wl(nullptr);
    std::unique_ptr<std::vector<
        std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>>
        wl_value(nullptr);
    std::vector<unsigned char> const *xcom_octets = nullptr;

    /*
      Treat the XCom member as if it is in the allowlist.
      The XCom member can be an IP or hostname.
      The magic-number "32" for the netmask is tied to IPv4.

      TODO: CHANGE THIS 32!!!!
    */
    bool is_hostname = string_to_sockaddr(xcom_addr.get_member_ip(), &xcom_sa);
    if (is_hostname) {
      xcom_addr_wl.reset(
          new Gcs_ip_allowlist_entry_hostname(xcom_addr.get_member_ip()));
    } else {
      std::string xcom_entry_netmask;

      if (is_ipv4_address(xcom_addr.get_member_ip()))
        xcom_entry_netmask.append("32");
      else
        xcom_entry_netmask.append("128");

      xcom_addr_wl.reset(new Gcs_ip_allowlist_entry_ip(
          xcom_addr.get_member_ip(), xcom_entry_netmask));
    }

    bool error = xcom_addr_wl->init_value();
    if (error) {
      continue;
    }

    wl_value.reset(xcom_addr_wl->get_value());
    if (wl_value.get() == nullptr) {
      continue;
    }

    for (auto &wl_value_entry : *wl_value.get()) {
      xcom_octets = &wl_value_entry.first;

      // no point in comparing different families, e.g. IPv4 with IPv6
      if (incoming_octets.size() != xcom_octets->size()) continue;

      for (size_t octet = 0; octet < xcom_octets->size(); octet++) {
        unsigned char const &oct_incoming = incoming_octets[octet];
        unsigned char const &oct_xcom = (*xcom_octets)[octet];
        // bail out on the first octet mismatch -- try next IP
        if ((block = (oct_incoming != oct_xcom))) break;
      }
    }
  }
  return block;
}

bool Gcs_ip_allowlist::do_check_block(struct sockaddr_storage *sa,
                                      site_def const *xcom_config) const {
  bool block = true;
  unsigned char *buf;
  std::vector<unsigned char> ip;

  if (sa->ss_family == AF_INET6) {
    unsigned int buff_offset = 0;
    unsigned int buff_lenght = sizeof(struct in6_addr);
    struct in6_addr *sa6 = &((struct sockaddr_in6 *)sa)->sin6_addr;

    /*
     This tests if we are in presence of an IPv4-mapped address.

     Since we expose XCom as a dual-stack application, all IPv4 connection will
     come as IPv4 mapped addresses. Those addresses have the following format:
     -  ::FFFF:129.144.52.38
     -- the first 80 bits are 0
     -- After those, we will have 16 bits set to 1
     -- The 32-bit IP address is at the end

     If the condition below marks true, it means that we are in presence of an
     IPv4 mapped address. We want to shift to the correct byte and treat it as
     a V4 address.
    */
    if ((sa6->s6_addr[0] == 0) && (sa6->s6_addr[1] == 0) &&
        (sa6->s6_addr[2] == 0) && (sa6->s6_addr[3] == 0) &&
        (sa6->s6_addr[4] == 0) && (sa6->s6_addr[5] == 0) &&
        (sa6->s6_addr[6] == 0) && (sa6->s6_addr[7] == 0) &&
        (sa6->s6_addr[8] == 0) && (sa6->s6_addr[9] == 0) &&
        (sa6->s6_addr[10] == 0xFF) && (sa6->s6_addr[11] == 0xFF)) {
      buff_offset = 12;
      buff_lenght = sizeof(struct in_addr);
    }

    buf =
        (unsigned char *)&((struct sockaddr_in6 *)sa)->sin6_addr + buff_offset;

    ip.assign(buf, buf + buff_lenght);

  } else if (sa->ss_family == AF_INET) {
    buf = (unsigned char *)&((struct sockaddr_in *)sa)->sin_addr;
    ip.assign(buf, buf + sizeof(struct in_addr));
  } else
    goto end;

  /*
    Allow the incoming IP if it is allowlisted *or* is an XCom member.
    XCom members are authorized by default so that XCom can create its
    all-to-all bidirectional network.
  */
  if (!m_ip_allowlist.empty()) block = do_check_block_allowlist(ip);
  if (block && xcom_config != nullptr)
    block = do_check_block_xcom(ip, xcom_config);

end:
  return block;
}

bool Gcs_ip_allowlist::shall_block(int fd, site_def const *xcom_config) {
  // lock the list
  Atomic_lock_guard guard{m_atomic_guard};

  bool ret = true;
  if (fd > 0) {
    struct sockaddr_storage sa;
    if (sock_descriptor_to_sockaddr(fd, &sa)) {
      MYSQL_GCS_LOG_WARN(
          "Invalid IPv4/IPv6 address. "
          "Refusing connection!");
      ret = true;
    } else
      ret = do_check_block(&sa, xcom_config);
  }

  if (ret) {
    std::string addr;
    sock_descriptor_to_string(fd, addr);
    MYSQL_GCS_LOG_WARN("Connection attempt from IP address "
                       << addr
                       << " refused. Address is not in the "
                          "IP allowlist.");
  }
  return ret;
}

bool Gcs_ip_allowlist::shall_block(const std::string &ip_addr,
                                   site_def const *xcom_config) {
  // lock the list
  Atomic_lock_guard guard{m_atomic_guard};

  bool ret = true;
  if (!ip_addr.empty()) {
    struct sockaddr_storage sa;
    if (string_to_sockaddr(ip_addr, &sa)) {
      MYSQL_GCS_LOG_WARN("Invalid IPv4/IPv6 address ("
                         << ip_addr
                         << "). "
                            "Refusing connection!");
      ret = true;
    } else
      ret = do_check_block(&sa, xcom_config);
  }

  if (ret) {
    MYSQL_GCS_LOG_WARN("Connection attempt from IP address "
                       << ip_addr
                       << " refused. Address is not in the "
                          "IP allowlist.");
  }
  return ret;
}
