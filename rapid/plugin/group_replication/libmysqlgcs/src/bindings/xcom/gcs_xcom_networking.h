/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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

 @param[in] host The hostname to translate.
 @param[out] ip  The IP address after translation.
 @return false on success, true otherwise.
 */
bool
get_ipv4_addr_from_hostname(const std::string& host, std::string& ip);

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
  std::map< std::vector<unsigned char>, std::vector<unsigned char> > m_ip_whitelist;

  /**
   This is the list that originally submitted to be parsed and to configure
   the whitelist.
   */
  std::string m_original_list;

public:
  Gcs_ip_whitelist() : m_ip_whitelist(), m_original_list() {}
  virtual ~Gcs_ip_whitelist() { }

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

