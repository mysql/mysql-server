/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef AUTH_LDAP_SASL_CLIENT_H_
#define AUTH_LDAP_SASL_CLIENT_H_

#include "my_config.h"

#ifdef HAVE_SASL_SASL_H
#include <sys/types.h>
#endif

#include <assert.h>
#include <mysql/client_plugin.h>
#include <sasl/sasl.h>

#include "auth_ldap_sasl_mechanism.h"

#define SASL_MAX_STR_SIZE 1024
#define SASL_SERVICE_NAME "ldap"

namespace auth_ldap_sasl_client {

/**
  Class representing SASL client
*/
class Sasl_client {
 public:
  /**
   Constructor

   @param vio [in]    pointer to server communication channel
   @param mysql [in]  pointer to MYSQL structure
  */
  Sasl_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);

  /**
   Default constructor -not wanted.
  */
  Sasl_client() = delete;

  /**
   Destructor
  */
  ~Sasl_client();

  /**
   Perform preauthentication step if needed, specific to the SASL mechanism e.g.
   obtaining Kerberos ticket for GSSAPI.

   @retval true success
   @retval false failure
  */
  bool preauthenticate();

  /**
   Initializes SASL client exchange.

   @retval true success
   @retval false failure
  */
  bool initilize_connection();

  /**
   Perform SASL interaction, callled as SASL callback.

   @param ilist [in] list of interaction ids to be served
  */
  void interact(sasl_interact_t *ilist);

  /**
   Decides and sets SASL mechanism to be used for authentication.

   @retval true success
   @retval false failure
  */
  bool set_mechanism();

  /**
   Starts SASL client exchange.

   @param client_output [out]  buffer with the initial client message to be
                               sent to server
   @param client_output_length [out] length of client_output

   @return SASL result code
  */
  int sasl_start(const char **client_output, int *client_output_length);

  /**
   Perform a step of SASL client exchange.

   @param server_input [in]  buffer with message from the server
   @param server_input_length [in] length of server_input
   @param client_output [out]  buffer with the client message to be
                               sent to server
   @param client_output_length [out] length of client_output

   @return SASL result code
  */
  int sasl_step(char *server_input, int server_input_length,
                const char **client_output, int *client_output_length);

  /**
   Sends SASL message to server and receive an response.
   SASL message is wrapped in a MySQL packet before sending.

   @param request [in]       pointer to the SASL request
   @param request_len [in]   length of request
   @param reponse [out]      pointer to received SASL response
   @param response_len [out] length of reponse or 0 on reading failure

   @retval 1 write failed
   @retval 0 write succeeded
  */
  int send_sasl_request_to_server(const char *request, int request_len,
                                  char **reponse, int *response_len);

  /**
   Check if the authentication method requires conclusion message from the
   server.

   @retval true conclusion required
   @retval false conclusion not required
  */
  bool require_conclude_by_server() {
    assert(m_sasl_mechanism);
    return m_sasl_mechanism->require_conclude_by_server();
  }

 private:
  /**
   If an empty original user name was given as client parameter and passed to
   the plugin via MYSQL structure, this function is used to determine the name
   for authentication and set this user name to the MYSQL structure. For proper
   memory management (string allocated by the plugin should not be freed by the
   main client module and vice versa), the original user name from MYSQL is
   stored to m_mysql_user and on destructing the object the original name is
   set back to MYSQL and m_mysql_user is freed.

   @retval true success
   @retval false failure
  */
  bool set_user();

  /**
   Sets (copies) user name and password to the members.

   @param name [in] user name
   @param pwd [in]  user password
  */
  void set_user_info(const char *name, const char *pwd);

  /** user name used for authentication */
  char m_user_name[SASL_MAX_STR_SIZE];

  /** user password used for authentication */
  char m_user_pwd[SASL_MAX_STR_SIZE];

  /** SASL connection data */
  sasl_conn_t *m_connection;

  /** pointer to server communication channel */
  MYSQL_PLUGIN_VIO *m_vio;

  /** pointer to MYSQL structure */
  MYSQL *m_mysql;

  /** the original user name, @see set_user() */
  char *m_mysql_user;

  /** the SASL mechanism used for authentication */
  Sasl_mechanism *m_sasl_mechanism;
};
}  // namespace auth_ldap_sasl_client
#endif  // AUTH_LDAP_SASL_CLIENT_H_
