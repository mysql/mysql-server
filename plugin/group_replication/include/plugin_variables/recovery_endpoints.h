/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef RECOVERY_ENDPOINTS_INCLUDE
#define RECOVERY_ENDPOINTS_INCLUDE

#include <set>
#include <string>
#include <vector>

#include <include/my_inttypes.h>
#include "plugin/group_replication/include/member_info.h"

/**
  @class Recovery_endpoints

  Validate recovery endpoints
*/
class Recovery_endpoints {
 protected:
  /**
   @enum enum_status

   This enumeration describes error status
  */
  enum class enum_status { OK = 0, INVALID, BADFORMAT, ERROR };

  /**
    Recovery_endpoints constructor
  */
  Recovery_endpoints();

  /**
    Recovery_endpoints destructor
  */
  virtual ~Recovery_endpoints();

  /**
    Validate recovery endpoints and log errors if it fails.

    @param endpoints advertised recovery endpoints

    @return the operation status
      @retval false   OK
      @retval true    Error
  */
  std::pair<enum_status, std::string> check(const char *endpoints);

  /**
    Return recovery endpoints

    @return list with recovery endpoints
  */
  std::vector<std::pair<std::string, uint>> get_endpoints();

  /**
    Set ports allowed on advertised recovery endpoints

    It shall be called when validating local recovery endpoints.

    @param mysqld_port mysqld port allowed on advertised recovery endpoints
    @param admin_port  mysqld admin port allowed on advertised recovery
    endpoints
  */
  void set_port_settings(uint mysqld_port, uint admin_port);

 private:
  /**
   Validate if recovery endpoint is a host name.

   @param host     hostname to be checked
   @param host_ips list of host IPs

   @return the operation status
     @retval 0      OK
     @retval !=0    Error
  */
  int hostname_check_and_log(std::string host, std::set<std::string> host_ips);

  /**
   Retrieve from host all ip address

   @param[out] local_ips     list of IPs present on host

   @return the operation status
     @retval 0      OK
     @retval !=0    Error
  */
  int local_interfaces_ips(std::set<std::string> &local_ips);

  /**
   Mysql bind port
  */
  uint m_mysqld_port;

  /**
   Mysql bind admin port
  */
  uint m_mysqld_admin_port;

  /**
    Advertised recovery valid endpoints
  */
  std::vector<std::pair<std::string, uint>> m_endpoints;

  /**
    Recovery endpoints are from donor
  */
  bool m_remote;
};

/**
  @class Advertised_recovery_endpoints

  Validate advertised recovery endpoints
*/
class Advertised_recovery_endpoints : Recovery_endpoints {
 public:
  /**
   @enum enum_log_context

   This enumeration describes which log context is being used.
  */
  enum class enum_log_context { ON_BOOT, ON_START, ON_SET };

  /**
    Advertised_recovery_endpoints constructor
  */
  Advertised_recovery_endpoints();

  /**
    Advertised_recovery_endpoints destructor
  */
  ~Advertised_recovery_endpoints() override;

  /**
    Validate recovery endpoints and log errors if it fails.

    @param endpoints advertised recovery endpoints
    @param where     context where being executed

    @return the operation status
      @retval false   OK
      @retval true    Error
  */
  bool check(const char *endpoints, enum_log_context where);
};

/**
  @class Donor_recovery_endpoints

  Validate donor recovery endpoints
*/
class Donor_recovery_endpoints : Recovery_endpoints {
 public:
  /**
    Donor_recovery_endpoints constructor
  */
  Donor_recovery_endpoints();

  /**
    Donor_recovery_endpoints destructor
  */
  ~Donor_recovery_endpoints() override;

  /**
    Get recovery endpoints

    @param donor group member info from donor

    @return endpoints
  */
  std::vector<std::pair<std::string, uint>> get_endpoints(
      Group_member_info *donor);
};

#endif /* RECOVERY_ENDPOINTS_INCLUDE */
