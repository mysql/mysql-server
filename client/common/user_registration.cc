
/*
Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <scope_guard.h>
#include <sstream>
#include <vector>

#include "user_registration.h"

#include "my_hostname.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"  // my_malloc
#include "mysqld_error.h"

#define QUERY_LENGTH 2048
#define MAX_QUERY_LENGTH 4096

/**
  This helper method parses --fido-register-factor option values, and
  inserts the parsed values in list.

  @param[in] what_factor      comma separated string containing what all factors
                              are to be registered
  @param[out] list            container holding individual factors

  @return true failed
  @return false success
*/
static bool parse_register_option(char *what_factor,
                                  std::vector<unsigned int> &list) {
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
    list.push_back(nth_factor);
  }
  return false;
}

/**
  This helper method is used to perform device registration against a user
  account.

  Please refer @ref sect_fido_info for more information.

  @param mysql              mysql connection handle
  @param register_option    Comma separated list of values, which specifies
  which factor requires registration. Valid values are "2", "3", "2,3" or "3,2"
  @param errmsg             Buffer tol hold error message in case of error.

  @return true failed
  @return false success
*/
bool user_device_registration(MYSQL *mysql, char *register_option,
                              char *errmsg) {
  char query[QUERY_LENGTH] = {0};
  char *query_ptr = nullptr;
  MYSQL_RES *result;
  MYSQL_ROW row;
  ulong *lengths;
  uchar *server_challenge = nullptr;
  uchar *server_challenge_response = nullptr;

  if (!mysql) {
    sprintf(errmsg, "MySQL internal error. ");
    return true;
  }

  std::vector<unsigned int> list;
  if (parse_register_option(register_option, list)) {
    sprintf(errmsg,
            "Incorrect value specified for --fido-register-factor option. "
            "Correct values can be '2', '3', '2,3' or '3,2'.");
    return true;
  }

  for (auto f : list) {
    sprintf(query, "ALTER USER USER() %d FACTOR INITIATE REGISTRATION", f);
    if (mysql_real_query(mysql, query, (ulong)strlen(query))) {
      sprintf(errmsg, "Initiate registration failed with error: %s. ",
              mysql_error(mysql));
      return true;
    }
    if (!(result = mysql_store_result(mysql))) {
      sprintf(errmsg, "Initiate registration failed with error: %s. ",
              mysql_error(mysql));
      return true;
    }
    if (mysql_num_rows(result) > 1) {
      sprintf(errmsg, "Initiate registration failed with error: %s. ",
              mysql_error(mysql));
      mysql_free_result(result);
      return true;
    }
    row = mysql_fetch_row(result);
    lengths = mysql_fetch_lengths(result);
    /*
      max length of challenge can be 32 (random challenge) +
      255 (relying party ID) + 255 (host name) + 32 (user name) + 4 byte for
      length encodings
    */
    if (lengths[0] > (CHALLENGE_LENGTH + RELYING_PARTY_ID_LENGTH +
                      HOSTNAME_LENGTH + USERNAME_LENGTH + 4)) {
      sprintf(errmsg, "Received server challenge is corrupt. Please retry.\n");
      mysql_free_result(result);
      return true;
    }
    server_challenge = static_cast<uchar *>(my_malloc(
        PSI_NOT_INSTRUMENTED, lengths[0] + 1, MYF(MY_WME | MY_ZEROFILL)));
    memcpy(server_challenge, row[0], lengths[0]);
    mysql_free_result(result);

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

    /* load fido client authentication plugin if required */
    struct st_mysql_client_plugin *p =
        mysql_client_find_plugin(mysql, "authentication_fido_client",
                                 MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
    if (!p) {
      sprintf(
          errmsg,
          "Loading authentication_fido_client plugin failed with error: %s. ",
          mysql_error(mysql));
      return true;
    }
    /* set server challenge in plugin */
    if (mysql_plugin_options(p, "registration_challenge", server_challenge)) {
      sprintf(errmsg,
              "Failed to set plugin options \"registration_challenge\".\n");
      return true;
    }
    /* get challenge response from plugin, and release the memory */
    if (mysql_plugin_get_option(p, "registration_response",
                                &server_challenge_response)) {
      sprintf(errmsg,
              "Failed to get plugin options \"registration_response\".\n");
      return true;
    }

    /* execute FINISH REGISTRATION sql */
    int n = snprintf(query, sizeof(query),
                     "ALTER USER USER() %d FACTOR FINISH REGISTRATION SET "
                     "CHALLENGE_RESPONSE AS ",
                     f);
    size_t tot_query_len =
        n + strlen(reinterpret_cast<char *>(server_challenge_response));
    if (tot_query_len >= MAX_QUERY_LENGTH) {
      sprintf(
          errmsg,
          "registration_response length exceeds max supported length of %d.\n",
          MAX_QUERY_LENGTH);
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
    if (mysql_real_query(mysql, query, (ulong)strlen(query))) {
      sprintf(errmsg, "Finish registration failed with error: %s.\n",
              mysql_error(mysql));
      return true;
    }
  }
  return false;
}
