/*
Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <scope_guard.h>
#include <sstream>
#include <vector>

#include "client/include/user_registration.h"

#include "my_hostname.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"  // my_malloc
#include "mysqld_error.h"

#define QUERY_LENGTH 32768
#define MAX_QUERY_LENGTH 65536
#define ENCODING_LENGTH 4
#define CAPABILITY_BIT_LENGTH 1

/**
  This helper method parses --register-factor
  option values, and inserts the parsed values in list.

  @param [in]  what_factor      Comma separated list of values, which specifies
                                which factor requires registration.
                                Valid values are "2", "3", "2,3" or "3,2"
  @param [out] factors          container holding individual factors

  @return true failed
  @return false success
*/
bool parse_register_option(const char *what_factor,
                           std::vector<unsigned int> &factors) {
  std::string token;
  std::stringstream str(what_factor);
  while (getline(str, token, ',')) {
    unsigned int nth_factor = 0;
    try {
      nth_factor = std::stoul(token);
    } catch (std::invalid_argument &) {
      return true;
    } catch (std::out_of_range &) {
      return true;
    }
    /* nth_factor can be either 2 or 3 */
    if (nth_factor < 2 || nth_factor > 3) return true;
    factors.push_back(nth_factor);
  }
  return false;
}

/**
  This helper method is used to perform device registration against a user
  account.

  Please refer @ref sect_webauthn_info for more information.

  @param [in]  mysql_handle       mysql connection handle
  @param [in]  register_option    Comma separated list of values, which
  specifies which factor requires registration. Valid values are "2", "3", "2,3"
  or "3,2"
  @param [out] errmsg             Buffer to hold error message in case of error.

  @return true failed
  @return false success
*/
bool user_device_registration(MYSQL *mysql_handle, char *register_option,
                              char *errmsg) {
  char query[QUERY_LENGTH] = {0};
  char *query_ptr = nullptr;
  MYSQL_RES *result;
  MYSQL_ROW row;
  ulong *lengths;
  uchar *server_challenge = nullptr;
  uchar *server_challenge_response = nullptr;
  std::string client_plugin_name;
  struct st_mysql_client_plugin *plugin_handler = nullptr;
  std::stringstream err{};

  auto print_error = [&errmsg, &mysql_handle, &err](bool print_mysql_error) {
    if (print_mysql_error) {
      sprintf(errmsg, "%s: %d (%s): %s\n", err.str().c_str(),
              mysql_errno(mysql_handle), mysql_sqlstate(mysql_handle),
              mysql_error(mysql_handle));
    } else {
      sprintf(errmsg, "%s\n", err.str().c_str());
    }
  };

  std::vector<unsigned int> factors;
  if (parse_register_option(register_option, factors)) {
    err << "Incorrect value specified for "
           "--register-factor option. "
           "Correct values can be '2', '3', '2,3' or '3,2'.";
    print_error(false);
    return true;
  }
  for (auto f : factors) {
    sprintf(query, "ALTER USER USER() %d FACTOR INITIATE REGISTRATION", f);
    if (mysql_real_query(mysql_handle, query, (ulong)strlen(query))) {
      err << "Initiate registration for " << f << " factor: ALTER USER failed";
      print_error(true);
      return true;
    }
    if (!(result = mysql_store_result(mysql_handle))) {
      err << "Initiate registration for " << f
          << " factor: Cannot process result";
      print_error(true);
      return true;
    }
    if (mysql_num_rows(result) > 1) {
      err << "Initiate registration for " << f << " factor: Unexpected result";
      print_error(true);
      mysql_free_result(result);
      return true;
    }

    row = mysql_fetch_row(result);
    lengths = mysql_fetch_lengths(result);
    /*
      max length of challenge can be 32 (random challenge) +
      255 (relying party ID) + 255 (host name) + 32 (user name) + 4 byte for
      length encodings + 1 byte capability
    */
    if (lengths[0] >
        (CHALLENGE_LENGTH + RELYING_PARTY_ID_LENGTH + HOSTNAME_LENGTH +
         USERNAME_LENGTH + ENCODING_LENGTH + CAPABILITY_BIT_LENGTH)) {
      err << "Initiate registration for " << f
          << " factor: Received server challenge is corrupt. "
             "Please retry.";
      print_error(false);
      mysql_free_result(result);
      return true;
    }
    server_challenge = static_cast<uchar *>(my_malloc(
        PSI_NOT_INSTRUMENTED, lengths[0] + 1, MYF(MY_WME | MY_ZEROFILL)));
    memcpy(server_challenge, row[0], lengths[0]);

    auto cleanup_guard = create_scope_guard([&] {
      if (server_challenge_response) {
        delete[] server_challenge_response;
        server_challenge_response = nullptr;
      }
      if (query_ptr && query_ptr != query) {
        my_free(query_ptr);
        query_ptr = nullptr;
      }
      if (server_challenge) {
        my_free(server_challenge);
        server_challenge = nullptr;
      }
    });

    if (mysql_num_fields(result) >= 2) {
      if (!lengths[1] || !row[1]) {
        err << "Initiate registration for " << f
            << " factor: No client plugin name received. Please retry.";
        print_error(false);
        mysql_free_result(result);
        return true;
      }
      client_plugin_name.assign(row[1], lengths[1]);
    }
    mysql_free_result(result);

    plugin_handler =
        mysql_client_find_plugin(mysql_handle, client_plugin_name.c_str(),
                                 MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
    /* check if client plugin is loaded */
    if (!plugin_handler) {
      err << "Initiate registration for " << f
          << " factor: Loading client plugin '" << client_plugin_name
          << "'failed with error";
      print_error(true);
      return true;
    }
    /* set server challenge in plugin */
    if (mysql_plugin_options(plugin_handler, "registration_challenge",
                             server_challenge)) {
      err << "Finish registration for " << f
          << " factor: Failed to set plugin options \"registration_challenge\" "
             "for plugin '"
          << client_plugin_name << "'.";
      print_error(false);
      return true;
    }
    /* get challenge response from plugin, and release the memory */
    if (mysql_plugin_get_option(plugin_handler, "registration_response",
                                &server_challenge_response)) {
      err << "Finish registration for " << f
          << " factor: Failed to get plugin options \"registration_response\". "
             "for pugin '"
          << client_plugin_name << "'.";
      print_error(false);
      return true;
    }

    /* execute FINISH REGISTRATION sql */
    int n = snprintf(query, sizeof(query),
                     "ALTER USER USER() %d FACTOR FINISH REGISTRATION SET "
                     "CHALLENGE_RESPONSE AS ",
                     f);
    size_t tot_query_len =
        n + strlen(reinterpret_cast<char *>(server_challenge_response)) +
        2 /* quotes */;
    if (tot_query_len >= MAX_QUERY_LENGTH) {
      err << "Finish registration for " << f
          << " factor: registration_response length exceeds max "
             "supported length of "
          << MAX_QUERY_LENGTH << "\n";
      print_error(false);
      return true;
    }
    if (tot_query_len >= QUERY_LENGTH) {
      /* allocate required buffer to construct query */
      query_ptr = static_cast<char *>(my_malloc(
          PSI_NOT_INSTRUMENTED, tot_query_len + 1, MYF(MY_WME | MY_ZEROFILL)));
    }
    if (query_ptr == nullptr) query_ptr = query;
    sprintf(query_ptr,
            "ALTER USER USER() %d FACTOR FINISH REGISTRATION SET "
            "CHALLENGE_RESPONSE AS '%s'",
            f, server_challenge_response);
    if (mysql_real_query(mysql_handle, query, (ulong)strlen(query))) {
      err << "Finish registration for " << f << " factor failed";
      print_error(true);
      return true;
    }
  }
  return false;
}
