/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "auth_ldap_sasl_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <lber.h>
#include <sasl/sasl.h>
#endif
#include <mysql/client_plugin.h>
#include <mysql.h>

Ldap_logger *g_logger_client;

void Sasl_client::interact(sasl_interact_t *ilist)
{
  while (ilist->id != SASL_CB_LIST_END)
  {
    switch(ilist->id)
    {
    /*
      the name of the user authenticating
    */
    case SASL_CB_USER:
      ilist->result= m_user_name;
      ilist->len= strlen((const char*)ilist->result);
      break;
    /* the name of the user acting for. (for example postman delivering mail for
       Martin might have an AUTHNAME of postman and a USER of Martin)
    */
    case SASL_CB_AUTHNAME:
      ilist->result= m_user_name;
      ilist->len= strlen((const char*)ilist->result);
      break;
    case SASL_CB_PASS:
      ilist->result= m_user_pwd;
      ilist->len= strlen((const char*)ilist->result);
      break;
    default:
      ilist->result= NULL;
      ilist->len= 0;
    }
    ilist++;
  }
}


void Sasl_client::set_plugin_info(MYSQL_PLUGIN_VIO *vio,  MYSQL *mysql)
{
  m_vio= vio;
  m_mysql= mysql;
}


/**
  SASL method is send from the Mysql server, and this is set by the client.
  SASL client and sasl server may support many sasl authentication methods
  and can negotiate in anyone.
  We want to enforce the SASL authentication set by the client.
*/
int Sasl_client::read_method_name_from_server()
{
  int rc_server_read= -1;
  unsigned char* packet= NULL;
  std::stringstream log_stream;
  /*
    We are assuming that there will be only one method name passed by
    server, and length of the method name will not exceed 256 chars.
  */
  const int max_method_name_len= 256;

  if (m_vio == NULL)
  {
    return rc_server_read;
  }
  /** Get authentication method from the server. */
  rc_server_read= m_vio->read_packet(m_vio, (unsigned char**)&packet);
  if (rc_server_read >= 0 && rc_server_read <= max_method_name_len)
  {
    strncpy(m_mechanism, (const char*)packet, rc_server_read);
    m_mechanism[rc_server_read]= '\0';
    log_stream << "Sasl_client::read_method_name_from_server : "
               << m_mechanism;
    log_dbg(log_stream.str());
  }
  else if (rc_server_read > max_method_name_len)
  {
    rc_server_read= -1;
    m_mechanism[0]= '\0';
    log_stream << "Sasl_client::read_method_name_from_server : Method name "
               << "is greater then allowed limit of 256 characters.";
    log_error(log_stream.str());
  }
  else
  {
    m_mechanism[0]= '\0';
    log_stream << "Sasl_client::read_method_name_from_server : Plugin has "
               << "failed to read the method name, make sure that default "
               << "authentication plugin and method name specified at "
               << "server are correct.";
    log_error(log_stream.str());
  }
  return rc_server_read;
}


 Sasl_client::Sasl_client()
{
  m_connection= NULL;
}


int Sasl_client::initilize()
{
  std::stringstream log_stream;
  int rc_sasl= SASL_FAIL;
  strncpy(m_service_name, SASL_SERVICE_NAME, sizeof(m_service_name)-1);
  m_service_name[sizeof(m_service_name)-1]= '\0';
  /** Initialize client-side of SASL. */
  rc_sasl= sasl_client_init(NULL);
  if (rc_sasl != SASL_OK)
  {
    goto EXIT;
  }

  /** Creating sasl connection. */
  rc_sasl= sasl_client_new(m_service_name, NULL, NULL, NULL, callbacks,
                           0, &m_connection);
  if (rc_sasl != SASL_OK)
    goto EXIT;

  /** Set security properties. */
  sasl_setprop(m_connection, SASL_SEC_PROPS, &security_properties);
  rc_sasl= SASL_OK;
EXIT:
  if (rc_sasl != SASL_OK)
  {
    log_stream << "Sasl_client::initilize failed rc: " << rc_sasl;
    log_error(log_stream.str());
  }
  return rc_sasl;
}


Sasl_client::~Sasl_client()
{
  if (m_connection)
  {
    sasl_dispose(&m_connection);
    m_connection=  NULL;
    sasl_client_done_wrapper();
  }
}


void Sasl_client::sasl_client_done_wrapper()
{
#if (SASL_VERSION_MAJOR >= 2) && \
    (SASL_VERSION_MINOR >= 1) && \
    (SASL_VERSION_STEP >= 24) && \
    (!defined __APPLE__) && (!defined __sun)
   sasl_client_done ();
#else
  sasl_done();
#endif
}


int Sasl_client::send_sasl_request_to_server(const unsigned char *request,
                                             int request_len,
                                             unsigned char** response,
                                             int* response_len)
{
  int rc_server= CR_ERROR;
  std::stringstream log_stream;

  if (m_vio == NULL)
  {
    goto EXIT;
  }
  /** Send the request to the MySQL server. */
  log_stream << "Sasl_client::SendSaslRequestToServer request:" << request;
  log_dbg(log_stream.str());
  rc_server= m_vio->write_packet(m_vio, request, request_len);
  if (rc_server)
  {
    log_error("Sasl_client::SendSaslRequestToServer: sasl request write failed");
    goto EXIT;
  }

  /** Get the sasl response from the MySQL server. */
  *response_len = m_vio->read_packet(m_vio, response);
  if ((*response_len) < 0 || (*response == NULL))
  {
    log_error("Sasl_client::SendSaslRequestToServer: sasl response read failed");
    goto EXIT;
  }
  log_stream.str("");
  log_stream << "Sasl_client::SendSaslRequestToServer response:" << *response;
  log_dbg(log_stream.str());
EXIT:
  return rc_server;
}


int Sasl_client::sasl_start(char **client_output, int* client_output_length)
{
  int rc_sasl= SASL_FAIL;
  const char *mechanism= NULL;
  char* sasl_client_output= NULL;
  sasl_interact_t *interactions= NULL;
  std::stringstream log_stream;

  if (m_connection == NULL)
  {
    log_error("Sasl_client::SaslStart: sasl connection is null");
    return rc_sasl;
  }
  do
  {
     rc_sasl= sasl_client_start(m_connection, m_mechanism, &interactions,
                                (const char**)&sasl_client_output,
                                (unsigned int *)client_output_length,
                                &mechanism);
     if(rc_sasl == SASL_INTERACT) interact(interactions);
  }
  while(rc_sasl == SASL_INTERACT);
  if (rc_sasl == SASL_NOMECH)
  {
    log_stream << "SASL method '" << m_mechanism << "' sent by server, "
               << "is not supported by the SASL client. Make sure that "
               << "cyrus SASL library is installed.";
    log_error(log_stream.str());
    goto EXIT;
  }
  if (client_output != NULL)
  {
    *client_output= sasl_client_output;
    log_stream << "Sasl_client::SaslStart sasl output: " << sasl_client_output;
    log_dbg(log_stream.str());
  }
EXIT:
  return rc_sasl;
}


int Sasl_client::sasl_step(char* server_in, int server_in_length,
                           char** client_out, int* client_out_length)
{
  int rc_sasl= SASL_FAIL;
  sasl_interact_t *interactions= NULL;

  if (m_connection == NULL)
  {
    return rc_sasl;
  }
  do
  {
    rc_sasl= sasl_client_step(m_connection,
                              server_in, server_in_length,
                              &interactions,
                              (const char**)client_out, (unsigned int *)client_out_length);
    if(rc_sasl == SASL_INTERACT)
      Sasl_client::interact(interactions);
  }
  while(rc_sasl == SASL_INTERACT);
  return rc_sasl;
}


void Sasl_client::set_user_info(std::string name, std::string pwd)
{
  strncpy(m_user_name, name.c_str(), sizeof(m_user_name)-1);
  m_user_name[sizeof(m_user_name)-1]= '\0';
  strncpy(m_user_pwd, pwd.c_str(), sizeof(m_user_pwd)-1);
  m_user_pwd[sizeof(m_user_pwd)-1]= '\0';
}


static int sasl_authenticate(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  int rc_sasl= SASL_FAIL;
  int rc_auth= CR_ERROR;
  unsigned char* server_packet= NULL;
  int server_packet_len= 0;
  char *sasl_client_output= NULL;
  int sasl_client_output_len=   0;
  const char *opt= getenv("AUTHENTICATION_LDAP_CLIENT_LOG");
  int opt_val= opt ? atoi(opt) : 0;
  std::stringstream log_stream;

  g_logger_client= new Ldap_logger();
  if (opt && opt_val > 0 && opt_val < 6)
  {
    g_logger_client->set_log_level((ldap_log_level)opt_val);
  }
  Sasl_client sasl_client;
  sasl_client.set_user_info(mysql->user, mysql->passwd);
  sasl_client.set_plugin_info(vio, mysql);
  server_packet_len= sasl_client.read_method_name_from_server();
  if (server_packet_len < 0)
  {
    // Callee has already logged the messages.
    goto EXIT;
  }

  rc_sasl= sasl_client.initilize();
  if (rc_sasl != SASL_OK)
  {
    log_error("sasl_authenticate: initialize failed");
    goto EXIT;
  }

  rc_sasl= sasl_client.sasl_start(&sasl_client_output,
                                  &sasl_client_output_len);
  if ((rc_sasl != SASL_OK) && (rc_sasl != SASL_CONTINUE))
  {
    log_error("sasl_authenticate: SaslStart failed");
    goto EXIT;
  }

  /**
    Running SASL authentication step till authentication process is concluded
    MySQL server plug-in working as proxy for SASL / LDAP server.
  */
  do
  {
    rc_auth= sasl_client.send_sasl_request_to_server((const unsigned char *)sasl_client_output,
                                                     sasl_client_output_len,
                                                     &server_packet,
                                                     &server_packet_len);
    if(rc_auth < 0)
    {
      goto EXIT;
    }

    rc_sasl= sasl_client.sasl_step((char*)server_packet,
                                   server_packet_len, &sasl_client_output,
                                   &sasl_client_output_len);
  }while (rc_sasl == SASL_CONTINUE);

  if (rc_sasl == SASL_OK)
  {
    rc_auth= CR_OK;
    log_dbg("sasl_authenticate authentication successful");
  }
  else
  {
    log_error("sasl_authenticate client failed");
  }

EXIT:
  if (rc_sasl != SASL_OK)
  {
    log_stream.str("");
    log_stream << "sasl_authenticate client failed rc: " << rc_sasl;
    log_error(log_stream.str());
  }
  if (g_logger_client)
  {
    delete g_logger_client;
    g_logger_client= NULL;
  }
  return rc_auth;
}


mysql_declare_client_plugin(AUTHENTICATION)
  "authentication_ldap_sasl_client",
  "Yashwant Sahu",
  "LDAP SASL Client Authentication Plugin",
  {0,1,0},
  "PROPRIETARY",
  NULL,
  NULL,
  NULL,
  NULL,
  sasl_authenticate
mysql_end_client_plugin;
