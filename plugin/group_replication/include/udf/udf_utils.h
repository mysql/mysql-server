/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_GR_INCLUDE_UDF_UTILS_H
#define PLUGIN_GR_INCLUDE_UDF_UTILS_H

#include <assert.h>
#include <mysql/components/services/registry.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/udf_registration_types.h>

#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/member_version.h"

const char *const member_offline_or_minority_str =
    "Member must be ONLINE and in the majority partition.";
const char *const unreachable_member_on_group_str =
    "All members in the group must be reachable.";
const char *const recovering_member_on_group_str =
    "A member is joining the group, wait for it to be ONLINE.";
const char *const server_uuid_not_present_str =
    "Wrong arguments: You need to specify a server uuid.";
const char *const server_uuid_not_valid_str =
    "Wrong arguments: The server uuid is not valid.";
const char *const server_uuid_not_on_group_str =
    "The requested uuid is not a member of the group.";

/**
 * Result data type for user_has_gr_admin_privilege.
 * There are three cases:
 *
 * error: There was an error fetching the user's privileges
 * ok: The user has the required privileges
 * no_privilege: The user does *not* have the required privileges
 *
 * In the no_privilege case, the result contains the user's name and host for
 * the caller to create an helpful error message.
 */
enum class privilege_status { ok, no_privilege, error };
class privilege_result {
 public:
  privilege_status status;
  char const *get_user() const {
    assert(status == privilege_status::no_privilege &&
           "get_user() can only be called if status == no_privilege");
    return user;
  }
  char const *get_host() const {
    assert(status == privilege_status::no_privilege &&
           "get_host() can only be called if status == no_privilege");
    return host;
  }
  static privilege_result success() {
    return privilege_result(privilege_status::ok);
  }
  static privilege_result error() {
    return privilege_result(privilege_status::error);
  }
  static privilege_result no_privilege(char const *user, char const *host) {
    return privilege_result(user, host);
  }

 private:
  char const *user;
  char const *host;
  privilege_result(privilege_status status)
      : status(status), user(nullptr), host(nullptr) {
    assert(status != privilege_status::no_privilege &&
           "privilege_result(status) can only be called if status != "
           "no_privilege");
  }
  privilege_result(char const *user, char const *host)
      : status(privilege_status::no_privilege), user(user), host(host) {}
};

/**
  @class UDF_counter
  Class used to increase an atomic value when UDF functions are being
  initialized. If initialization fails the value will be decreased.

  number_udfs_running works together with plugin_is_stopping so when group
  replication is stopping, all new udf will fail to start and server will
  wait for the running ones to finish.
*/

class UDF_counter {
 public:
  static std::atomic<int> number_udfs_running;
  static void terminated() { number_udfs_running--; }
  static bool is_zero() { return number_udfs_running == 0; }

  UDF_counter() : success(false) { number_udfs_running++; }
  ~UDF_counter() {
    if (!success) number_udfs_running--;
  }

  void succeeded() { success = true; }

 private:
  bool success;
};

/**
 * Checks whether the user has GROUP_REPLICATION_ADMIN privilege.
 *
 * @retval privilege_result::error if there was an error fetching the user's
 * privileges
 * @retval privilege_result::no_privilege if the user does not have the
 * privilege
 * @retval privilege_result::success if the user has the privilege
 */
privilege_result user_has_gr_admin_privilege();

/**
 * Logs the privilege status of @c privilege into @c message.
 *
 * @param privilege the result of @c user_has_gr_admin_privilege()
 * @param[out] message the buffer where the log message will be written
 */
void log_privilege_status_result(privilege_result const &privilege,
                                 char *message);
/**
 * Checks that `super_read_only` is disabled on the server.
 *
 * @returns std::pair<bool, std::string> where each element has the
 *          following meaning:
 *            first element of the pair is the function error value:
 *              false  Successful
 *              true   Error
 *            second element of the pair is the error message.
 */
std::pair<bool, std::string> check_super_read_only_is_disabled();

/**
 * Checks whether the server is ONLINE and belongs to the majority partition.
 *
 * @retval true if the member is online and in the majority partition
 * @retval false otherwise
 */
bool member_online_with_majority();

/**
 * Checks if an unreachable member exists in the group
 *
 * @retval true if an unreachable member exists
 * @retval false otherwise
 */
bool group_contains_unreachable_member();

/**
 * Checks if a member in recovery exists in the group
 *
 * @retval true if a recovering member exists
 * @retval false otherwise
 */
bool group_contains_recovering_member();

/**
 * Checks if the uuid is valid to use in a function
 * It checks:
 *   1. It is not empty
 *   2. It is a valid uuid
 *   3. It belongs to the group
 *
 * @param      uuid    the uuid string
 * @param      ulength the length of the uuid string
 * @param[out] error_message the returned error message
 *
 * @retval true if uuid is not valid
 * @retval false otherwise
 */
bool validate_uuid_parameter(std::string &uuid, size_t ulength,
                             const char **error_message);

/**
 * Throw a error on a UDF function with mysql_error_service_printf
 *
 * @param  action_name   the action name when the error occurred
 * @param  error_message the error message to print
 * @param  log_error     should the error also go to the log (default = false)
 *
 * @retval true the function failed to use the mysql_runtime_error service to
 *              throw the error
 * @retval false everything went OK
 */
bool throw_udf_error(const char *action_name, const char *error_message,
                     bool log_error = false);

/**
 * Logs the group action @c action_name result from @c result_area into
 * @c result_message.
 *
 * @param result_area describes the log message level
 * @param action_name group action name
 * @param[out] result_message buffer where the log message will be written
 * @param[out] length size of the log message written to @c result_message
 *
 * @retval true the group action failed and this function threw/logged the group
 *              action's error
 * @retval false everything went OK
 */
bool log_group_action_result_message(Group_action_diagnostics *result_area,
                                     const char *action_name,
                                     char *result_message,
                                     unsigned long *length);

/**
 * Checks if tables are locked, and logs to @c message if so.
 *
 * @param[out] message buffer where the log message will be written to
 * @retval true if tables are not locked
 * @retval false if tables are locked (@c message is written to)
 */
bool check_locked_tables(char *message);

/**
 * Checks whether the group contains a member older than the specified version.
 *
 * @param min_required_version Minimum version required
 * @returns true if there is some older member, false otherwise
 */
bool group_contains_member_older_than(
    Member_version const &min_required_version);
/**
 @class Charset_service

 Class that acquire/release the udf_metadata_service from registry service.
 It provides the APIs to set the character set of return value and arguments
 of UDFs using the udf_metadata service.
*/
class Charset_service {
 public:
  /**
    Acquires the udf_metadata_service from the registry  service.
    @param[in]  reg_srv Registry service from which udf_metadata service
                        will be acquired

    @retval true if service could not be acquired
    @retval false Otherwise
  */
  static bool init(SERVICE_TYPE(registry) * reg_srv);

  /**
    Release the udf_metadata service

    @param[in]  reg_srv Registry service from which the udf_metadata
                        service will be released.

    @retval true if service could not be released
    @retval false Otherwise
  */
  static bool deinit(SERVICE_TYPE(registry) * reg_srv);

  /**
    Set the specified character set of UDF return value

    @param[in] initid  UDF_INIT structure
    @param[in] charset_name Character set that has to be set.
               The default charset is set to 'latin1'

    @retval true Could not set the character set of return value
    @retval false Otherwise
  */
  static bool set_return_value_charset(
      UDF_INIT *initid, const std::string &charset_name = "latin1");
  /**
    Set the specified character set of all UDF arguments

    @param[in] args UDF_ARGS structure
    @param[in] charset_name Character set that has to be set.
               The default charset is set to 'latin1'

    @retval true Could not set the character set of any of the argument
    @retval false Otherwise
  */
  static bool set_args_charset(UDF_ARGS *args,
                               const std::string &charset_name = "latin1");

 private:
  /* Argument type to specify in the metadata service methods */
  static const char *arg_type;
  /* udf_metadata service name */
  static const char *service_name;
  /* Handle of udf_metadata_service */
  static SERVICE_TYPE(mysql_udf_metadata) * udf_metadata_service;
};

#endif /* PLUGIN_GR_INCLUDE_UDF_UTILS_H */
