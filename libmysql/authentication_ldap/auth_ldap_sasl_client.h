/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef AUTH_LDAP_SASL_CLIENT_H_
#define AUTH_LDAP_SASL_CLIENT_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sasl/sasl.h>
#include <mysql/client_plugin.h>
#include <mysql/plugin.h>
#include <mysql/plugin_auth_common.h>
#include <mysql.h>
#include "log_client.h"

#define SASL_MAX_STR_SIZE 1024
#define SASL_BUFFER_SIZE 9000
#define SASL_SERVICE_NAME "ldap"

static const sasl_callback_t callbacks[]=
{
#ifdef SASL_CB_GETREALM
  {SASL_CB_GETREALM, NULL, NULL},
#endif
  {SASL_CB_USER, NULL, NULL},
  {SASL_CB_AUTHNAME, NULL, NULL},
  {SASL_CB_PASS, NULL, NULL},
  {SASL_CB_ECHOPROMPT, NULL, NULL},
  {SASL_CB_NOECHOPROMPT, NULL, NULL},
  {SASL_CB_LIST_END, NULL, NULL}
};


sasl_security_properties_t security_properties=
{
  /** Minimum acceptable final level. */
  0,
  /** Maximum acceptable final level. */
  1,
  /** Maximum security layer receive buffer size. */
  0,
  /** security flags */
  0,
  /** Property names. */
  NULL,
  /** Property values. */
  NULL,
};


class Sasl_client
{
public:
  Sasl_client();
  ~Sasl_client();
  int initilize();
  void set_plugin_info(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
  void interact(sasl_interact_t *ilist);
  int read_method_name_from_server();
  int sasl_start(char **client_output, int* client_output_length);
  int sasl_step(char* server_in, int server_in_length, char** client_out, int* client_out_length);
  int send_sasl_request_to_server(const unsigned char *request, int request_len, unsigned char** reponse, int* response_len);
  void set_user_info(std::string name, std::string pwd);
  void sasl_client_done_wrapper();

protected:
  char m_user_name[SASL_MAX_STR_SIZE];
  char m_user_pwd[SASL_MAX_STR_SIZE];
  char m_mechanism[SASL_MAX_STR_SIZE];
  char m_service_name[SASL_MAX_STR_SIZE];
  sasl_conn_t *m_connection;
  MYSQL_PLUGIN_VIO *m_vio;
  MYSQL *m_mysql;
};

#endif //AUTH_LDAP_SASL_CLIENT_H_
