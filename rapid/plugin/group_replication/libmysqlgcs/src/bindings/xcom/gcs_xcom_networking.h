/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
#define	GCS_XCOM_NETWORKING_H

#include<vector>
#include<map>
#include<string>

/**
  This function gets all network addresses on this host and their
  subnet masks as a string. IPv4 only. (SOCK_STREAM only)
 @param[out] out maps IP addresses to subnetmasks
 @param filter_out_inactive If set to true, only active interfaces will be added
                            to out
 @return false on sucess, true otherwise.
 */
bool
get_ipv4_local_addresses(std::map<std::string, int>& out,
                         bool filter_out_inactive= false);

/**
  This function gets all private network addresses and their
  subnet masks as a string. IPv4 only. (SOCK_STREAM only)
 @param[out] out maps IP addresses to subnetmasks
 @param filter_out_inactive If set to true, only active interfaces will be added
                            to out
 @return false on sucess, true otherwise.
 */
bool
get_ipv4_local_private_addresses(std::map<std::string, int>& out,
                                 bool filter_out_inactive= false);

/**
 This function translates hostnames to IP addresses.

 @param[in] name The hostname to translate.
 @param[out] ip  The IP address after translation.

 @return false on success, true otherwise.
 */
bool
resolve_ip_addr_from_hostname(std::string name, std::string& ip);

/**
 Converts an address in string format (X.X.X.X/XX) into network octet format

 @param[in]  addr     IP address in X.X.X.X format
 @param[in]  mask     Network mask associated with the address
 @param[out] out_pair

 @return false on success, true otherwise.
 */
bool
get_address_for_whitelist(std::string addr, std::string mask,
                          std::pair<std::vector<unsigned char>,
                                    std::vector<unsigned char>> &out_pair);

/**
 @class Gcs_ip_whitelist_entry
 @brief Base abstract class for the whitelist entries.

 This is the base class for the Whitelist entries. Any derived class must
 implement its two abstract methods:
 - init_value();
 - get_value();
 */
class Gcs_ip_whitelist_entry
{
public:
  /**
   Constructor

   @param[in] addr IP address or hostname of this entry
   @param[in] mask Network mask of this entry.
   */
  Gcs_ip_whitelist_entry(std::string addr, std::string mask);

  virtual ~Gcs_ip_whitelist_entry() {}

  /**
   Entry initialization.

   If one needs to initialize internal values, it should be done in this
   method.

   @return false on success, true otherwise
   */
  virtual bool init_value() = 0;

  /**
   Virtual Method that implements value retrieval for this entry.

   The returned value must be an std::pair that contains both the address and the
   mask in network octet value.

   @return an std::pair with ip and mask in network octet form
   */
  virtual std::pair< std::vector<unsigned char>,
                     std::vector<unsigned char> > *get_value() = 0;

  /** Getters */
  std::string get_addr() const {return m_addr;};
  std::string get_mask() const {return m_mask;};

private:
  std::string m_addr;
  std::string m_mask;
};

struct Gcs_ip_whitelist_entry_pointer_comparator {
    bool operator() (const Gcs_ip_whitelist_entry* lhs,
                     const Gcs_ip_whitelist_entry* rhs) const {
      //Check if addresses are different in content
      if(lhs->get_addr() != rhs->get_addr())
      { //Then compare only the addresses
        return lhs->get_addr() < rhs->get_addr();
      }
      else
      { //If addresses are equal, then compare the masks to untie.
        return lhs->get_mask() < rhs->get_mask();
      }
    }
};

/**
 @class Gcs_ip_whitelist_entry_ip
 @brief Implementation of Gcs_ip_whitelist_entry to use with
        raw IP addresses in format X.X.X.X/XX
 */
class Gcs_ip_whitelist_entry_ip: public Gcs_ip_whitelist_entry
{
public:
  Gcs_ip_whitelist_entry_ip(std::string addr, std::string mask);

public:
  bool init_value();
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> *get_value();

private:
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> m_value;
};

/**
 @class Gcs_ip_whitelist_entry_hostname
 @brief Implementation of Gcs_ip_whitelist_entry to use with
        hostnames
 */
class Gcs_ip_whitelist_entry_hostname: public Gcs_ip_whitelist_entry
{
public:
  Gcs_ip_whitelist_entry_hostname(std::string addr, std::string mask);

public:
  bool init_value();
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> *get_value();
};

class Gcs_ip_whitelist
{
public:
  static const std::string DEFAULT_WHITELIST;

private:
  /*
   The IP whitelist. It is a list of tuples Hexadecimal IP number
   and subnet mask also in Hexadecimal. E.g.: 192.168.1.2/24 or 127.0.0.1/32.

   This is for optimization purposes, so that we don't calculate the
   values each time we want to check.
   */
  std::set< Gcs_ip_whitelist_entry*,
            Gcs_ip_whitelist_entry_pointer_comparator> m_ip_whitelist;

  /**
   This is the list that originally submitted to be parsed and to configure
   the whitelist.
   */
  std::string m_original_list;

public:
  Gcs_ip_whitelist() :
    m_ip_whitelist(),
    m_original_list() {}
  virtual ~Gcs_ip_whitelist();

  /**
   This member function shall be used to configure the whitelist.

   @param the_list The list with IP addresses. This list is a comma separated
                   list formatted only with IP addresses and/or in the form of
                   a subnet range, e.g., IP/netbits.
   @return true if the configuration failed, false otherwise.
   */
  bool configure(const std::string& the_list);

  /**
   This member function shall be used to validate the list that is used as input
   to the configure member function.

   @param the_list The list with IP addresses. This list is a comma separated
                   list formatted only with IP addresses and/or in the form of
                   a subnet range, e.g., IP/netbits.

   @return true if the configuration failed, false otherwise.
   */
  bool is_valid(const std::string& the_list) const;

  /**
   This member function SHALL return true if the given IP is to be blocked,
   false otherwise.

   @param ip_addr a string representation of an IPv4 address.

   @return true if the ip should be blocked, false otherwise.
   */
  bool shall_block(const std::string& ip_addr) const;

  /**
   This member function SHALL return true if the IP of the given file descriptor
   is to be blocked, false otherwise.

   @param fd the file descriptor of the accepted socket to check.

   @return true if the ip should be blocked, false otherwise.
   */
  bool shall_block(int fd) const;

  /**
   This member function gets the textual representation of the list as
   provided to the configure member function.
   */
  const std::string& get_configured_ip_whitelist() const { return m_original_list; }

  /**
   A string representation of the internal list of IP addresses. Can have
   more addresses than those submitted through the configure member
   function, since there are addresses that are implicitly added when
   configuring the list.
   */
  std::string to_string() const;

private:
  bool do_check_block(struct sockaddr_storage *sa) const;
  bool add_address(std::string addr, std::string mask);

private:
  Gcs_ip_whitelist(Gcs_ip_whitelist const&);
  Gcs_ip_whitelist& operator=(Gcs_ip_whitelist const&);
};


#endif	/* GCS_XCOM_NETWORKING_H */

