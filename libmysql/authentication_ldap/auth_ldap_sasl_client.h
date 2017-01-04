/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */

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
#include "log.h"

#define SASL_MAX_STR_SIZE 1024
#define SASL_BUFFER_SIZE 9000
#define SASL_SERVICE_NAME "ldap"

static const sasl_callback_t callbacks[] = {
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

sasl_security_properties_t security_properties = {
  0,
  1,
  0,
  0,
  NULL,
  NULL,
};

class Sasl_client {
public:
  Sasl_client();
  int Initilize();
  int UnInitilize();
  void SetPluginInfo(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
  void Interact(sasl_interact_t *ilist);
  int ReadMethodNameFromServer();
  int SaslStart(char **client_output, int* client_output_length);
  int SaslStep(char* server_in, int server_in_length, char** client_out, int* client_out_length);
  int SendSaslRequestToServer(const unsigned char *request, int request_len, unsigned char** reponse, int* response_len);
  void SetUserInfo(std::string name, std::string pwd);
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
