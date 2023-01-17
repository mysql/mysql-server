/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_HOSTNAME_VALIDATOR_INCLUDED
#define MYSQL_HARNESS_HOSTNAME_VALIDATOR_INCLUDED

#include "harness_export.h"

#include <string>

namespace mysql_harness {

bool HARNESS_EXPORT is_valid_ip_address(const std::string &address);

/**
 * check if address is a hostname.
 *
 * hostname is verified according to RFC 1123.
 *
 * - fully qualified domain names like "mysql.com." are not valid hostnames
 *   (trailing dot)
 * - service names like "_mysql.example.com" are not valid hostnames (leading
 *   underscore)
 *
 * @param address name of a host
 *
 * @retval false hostname is invalid
 * @retval true hostname is valid
 */
bool HARNESS_EXPORT is_valid_hostname(const std::string &address);

/**
 * check if address is a domainname.
 *
 * domainnames according to RFC 2181:
 *
 * - max size 255 chars
 * - labels are separated by dots
 * - each label is min 1, max 63 chars.
 *
 * That means
 *
 * - IPv4 addresses
 * - IPv6 addresses
 * - hostnames
 *
 * are domainnames.
 *
 * @param address address to check
 *
 * @retval true address is a domainname
 * @retval false address is not a domainname
 */
bool HARNESS_EXPORT is_valid_domainname(const std::string &address);

}  // namespace mysql_harness

#endif  // #define MYSQL_HARNESS_HOSTNAME_VALIDATOR_INCLUDED
