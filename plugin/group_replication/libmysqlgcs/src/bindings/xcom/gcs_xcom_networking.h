/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_NETWORKING_H
#define GCS_XCOM_NETWORKING_H

#include <map>
#include <string>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe.h"

#ifdef _WIN32
typedef u_short sa_family_t;
#endif

/**
 * @brief Interface to decouple XCom sock_probe implementation to allow
 * unit testing.
 */
class Gcs_sock_probe_interface {
 public:
  Gcs_sock_probe_interface() = default;
  virtual ~Gcs_sock_probe_interface() = default;

  virtual int init_sock_probe(sock_probe *s) = 0;
  virtual int number_of_interfaces(sock_probe *s) = 0;
  virtual void get_sockaddr_address(sock_probe *s, int count,
                                    struct sockaddr **out) = 0;
  virtual void get_sockaddr_netmask(sock_probe *s, int count,
                                    struct sockaddr **out) = 0;
  virtual char *get_if_name(sock_probe *s, int count) = 0;
  virtual bool_t is_if_running(sock_probe *s, int count) = 0;
  virtual void close_sock_probe(sock_probe *s) = 0;

  Gcs_sock_probe_interface(Gcs_sock_probe_interface &) = default;
  Gcs_sock_probe_interface(Gcs_sock_probe_interface &&) =
      default;  // move constructor must be explicit because copy is
  Gcs_sock_probe_interface &operator=(const Gcs_sock_probe_interface &) =
      default;
  Gcs_sock_probe_interface &operator=(Gcs_sock_probe_interface &&) =
      default;  // move assignment must be explicit because move is
};

/**
 * @brief Implementation of @class Gcs_sock_probe_interface
 */
class Gcs_sock_probe_interface_impl : public Gcs_sock_probe_interface {
 public:
  Gcs_sock_probe_interface_impl() : Gcs_sock_probe_interface() {}
  ~Gcs_sock_probe_interface_impl() override = default;

  int init_sock_probe(sock_probe *s) override;
  int number_of_interfaces(sock_probe *s) override;
  void get_sockaddr_address(sock_probe *s, int count,
                            struct sockaddr **out) override;
  void get_sockaddr_netmask(sock_probe *s, int count,
                            struct sockaddr **out) override;
  char *get_if_name(sock_probe *s, int count) override;
  bool_t is_if_running(sock_probe *s, int count) override;
  void close_sock_probe(sock_probe *s) override;

  Gcs_sock_probe_interface_impl(Gcs_sock_probe_interface_impl &) = default;
  Gcs_sock_probe_interface_impl(Gcs_sock_probe_interface_impl &&) =
      default;  // move constructor must be explicit because copy is
  Gcs_sock_probe_interface_impl &operator=(
      const Gcs_sock_probe_interface_impl &) = default;
  Gcs_sock_probe_interface_impl &operator=(Gcs_sock_probe_interface_impl &&) =
      default;  // move assignment must be explicit because move is
};

/**
  This function gets all network addresses on this host and their
  subnet masks as a string.

  @param[out] sock_probe Socket probe interface
  @param[out] out maps IP addresses to subnetmasks
  @param filter_out_inactive If set to true, only active interfaces will be
  added to out
  @return false on success, true otherwise.
 */
bool get_local_addresses(Gcs_sock_probe_interface &sock_probe,
                         std::map<std::string, int> &out,
                         bool filter_out_inactive = false);

/**
  This function gets all private network addresses and their
  subnet masks as a string. IPv4 only. (SOCK_STREAM only)
 @param[out] out maps IP addresses to subnetmasks
 @param filter_out_inactive If set to true, only active interfaces will be added
                            to out
 @return false on success, true otherwise.
 */
bool get_local_private_addresses(std::map<std::string, int> &out,
                                 bool filter_out_inactive = false);

/**
 This function translates hostnames to all possible IP addresses.

 @param[in] name The hostname to translate.
 @param[out] ips The IP addresses after translation.

 @return false on success, true otherwise.
 */
bool resolve_all_ip_addr_from_hostname(
    std::string name, std::vector<std::pair<sa_family_t, std::string>> &ips);

/**
 This function translates hostname to all possible IP addresses.

 @param[in] name The hostname to translate.
 @param[out] ip  The IP addresses after translation.

 @return false on success, true otherwise.
 */
bool resolve_ip_addr_from_hostname(std::string name,
                                   std::vector<std::string> &ip);

/**
 Converts an address in string format (X.X.X.X/XX) into network octet format

 @param[in]  addr     IP address in X.X.X.X format
 @param[in]  mask     Network mask associated with the address
 @param[out] out_pair IP address and netmask, in binary format (network byte
                      order)

 @return false on success, true otherwise.
 */
bool get_address_for_allowlist(std::string addr, std::string mask,
                               std::pair<std::vector<unsigned char>,
                                         std::vector<unsigned char>> &out_pair);

/**
 @class Gcs_ip_allowlist_entry
 @brief Base abstract class for the allowlist entries.

 This is the base class for the Allowlist entries. Any derived class must
 implement its two abstract methods:
 - init_value();
 - get_value();
 */
class Gcs_ip_allowlist_entry {
 public:
  /**
   Constructor

   @param[in] addr IP address or hostname of this entry
   @param[in] mask Network mask of this entry.
   */
  Gcs_ip_allowlist_entry(std::string addr, std::string mask);

  virtual ~Gcs_ip_allowlist_entry() = default;

  /**
   Entry initialization.

   If one needs to initialize internal values, it should be done in this
   method.

   @return false on success, true otherwise
   */
  virtual bool init_value() = 0;

  /**
   Virtual Method that implements value retrieval for this entry.

   The returned value must be a list of std::pairs that contains both the
   address and the mask in network octet value. This is in list format because
   in the case of allowlist names, we can have multiple value for the same
   entry

   @return an std::vector of std::pair with ip and mask in network octet form
   */
  virtual std::vector<
      std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
      *get_value() = 0;

  /** Getters */
  std::string get_addr() const { return m_addr; }
  std::string get_mask() const { return m_mask; }

 private:
  std::string m_addr;
  std::string m_mask;
};

struct Gcs_ip_allowlist_entry_pointer_comparator {
  bool operator()(const Gcs_ip_allowlist_entry *lhs,
                  const Gcs_ip_allowlist_entry *rhs) const {
    // Check if addresses are different in content
    if (lhs->get_addr() != rhs->get_addr()) {  // Then compare only the
                                               // addresses
      return lhs->get_addr() < rhs->get_addr();
    } else {  // If addresses are equal, then compare the masks to untie.
      return lhs->get_mask() < rhs->get_mask();
    }
  }
};

/**
 @class Gcs_ip_allowlist_entry_ip
 @brief Implementation of Gcs_ip_allowlist_entry to use with
        raw IP addresses in format X.X.X.X/XX
 */
class Gcs_ip_allowlist_entry_ip : public Gcs_ip_allowlist_entry {
 public:
  Gcs_ip_allowlist_entry_ip(std::string addr, std::string mask);

 public:
  bool init_value() override;
  std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
      *get_value() override;

 private:
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> m_value;
};

/**
 @class Gcs_ip_allowlist_entry_hostname
 @brief Implementation of Gcs_ip_allowlist_entry to use with
        hostnames
 */
class Gcs_ip_allowlist_entry_hostname : public Gcs_ip_allowlist_entry {
 public:
  Gcs_ip_allowlist_entry_hostname(std::string addr, std::string mask);
  Gcs_ip_allowlist_entry_hostname(std::string addr);

 public:
  bool init_value() override;
  std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
      *get_value() override;
};

class Gcs_ip_allowlist {
 public:
  static const std::string DEFAULT_ALLOWLIST;

 private:
  class Atomic_lock_guard {
   private:
    /**
     * @brief When true, it is locked. When false it is not.
     */
    std::atomic_flag &m_guard;

   public:
    Atomic_lock_guard(std::atomic_flag &guard) : m_guard(guard) {
      // keep trying until it is unlocked, then lock
      while (m_guard.test_and_set()) {
        std::this_thread::yield();
      }
    }

    ~Atomic_lock_guard() { m_guard.clear(); }
  };

 private:
  /*
   The IP allowlist. It is a list of tuples Hexadecimal IP number
   and subnet mask also in Hexadecimal. E.g.: 192.168.1.2/24 or 127.0.0.1/32.

   This is for optimization purposes, so that we don't calculate the
   values each time we want to check.
   */
  std::set<Gcs_ip_allowlist_entry *, Gcs_ip_allowlist_entry_pointer_comparator>
      m_ip_allowlist;

  /**
   This is the list that originally submitted to be parsed and to configure
   the allowlist.
   */
  std::string m_original_list;

 public:
  Gcs_ip_allowlist() : m_ip_allowlist(), m_original_list() {
    m_atomic_guard.clear();
  }
  virtual ~Gcs_ip_allowlist();

  /**
   This member function shall be used to configure the allowlist.

   @param the_list The list with IP addresses. This list is a comma separated
                   list formatted only with IP addresses and/or in the form of
                   a subnet range, e.g., IP/netbits.
   @return true if the configuration failed, false otherwise.
   */
  bool configure(const std::string &the_list);

  /**
   This member function shall be used to validate the list that is used as
   input to the configure member function.

   @param the_list The list with IP addresses. This list is a comma separated
                   list formatted only with IP addresses and/or in the form of
                   a subnet range, e.g., IP/netbits.

   @return true if the configuration failed, false otherwise.
   */
  bool is_valid(const std::string &the_list);

  /**
   This member function SHALL return true if the given IP is to be blocked,
   false otherwise.

   @param ip_addr a string representation of an IPv4 address.
   @param xcom_config the latest XCom configuration.

   @return true if the ip should be blocked, false otherwise.
   */
  bool shall_block(const std::string &ip_addr,
                   site_def const *xcom_config = nullptr);

  /**
   This member function SHALL return true if the IP of the given file
   descriptor is to be blocked, false otherwise.

   @param fd the file descriptor of the accepted socket to check.
   @param xcom_config the latest XCom configuration.

   @return true if the ip should be blocked, false otherwise.
   */
  bool shall_block(int fd, site_def const *xcom_config = nullptr);

  /**
   This member function gets the textual representation of the list as
   provided to the configure member function.
   */
  const std::string get_configured_ip_allowlist() {
    // lock the list
    Atomic_lock_guard guard{m_atomic_guard};
    return m_original_list;
  }

  /**
   A string representation of the internal list of IP addresses. Can have
   more addresses than those submitted through the configure member
   function, since there are addresses that are implicitly added when
   configuring the list.
   */
  std::string to_string() const;

 private:
  bool do_check_block(struct sockaddr_storage *sa,
                      site_def const *xcom_config) const;
  bool do_check_block_allowlist(
      std::vector<unsigned char> const &incoming_octets) const;
  bool do_check_block_xcom(std::vector<unsigned char> const &incoming_octets,
                           site_def const *xcom_config) const;
  bool add_address(std::string addr, std::string mask);

  /**
   @brief Clears the contents of this Allowlist object.

   It deletes all entries and clears the internal set.
   */
  void clear();

  /**
   * @brief An atomic lock to guard the ip allowlist.
   */
  std::atomic_flag m_atomic_guard;

 private:
  Gcs_ip_allowlist(Gcs_ip_allowlist const &);
  Gcs_ip_allowlist &operator=(Gcs_ip_allowlist const &);
};

#endif /* GCS_XCOM_NETWORKING_H */
