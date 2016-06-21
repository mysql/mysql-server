/*  Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file

  Test driver for the mysql-test/t/plugin_auth.test

  This is a set of test plugins used to test the external authentication 
  implementation.
  See the above test file for more details.
  This test plugin is based on the dialog plugin example.
*/

#include <my_global.h>
#include <mysql/plugin_auth.h>
#include <mysql/client_plugin.h>
#include <mysql/service_my_plugin_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
  first byte of the question string is the question "type".
  It can be a "ordinary" or a "password" question.
  The last bit set marks a last question in the authentication exchange.
*/
#define ORDINARY_QUESTION       "\2"
#define LAST_QUESTION           "\3"
#define LAST_PASSWORD           "\4"
#define PASSWORD_QUESTION       "\5"

/********************* SERVER SIDE ****************************************/

/**
 Handle assigned when loading the plugin. 
 Used with the error reporting functions. 
*/
static MYSQL_PLUGIN plugin_info_ptr; 

static int
test_plugin_init (MYSQL_PLUGIN plugin_info)
{
  plugin_info_ptr= plugin_info;
  return 0;
}


/**
  dialog test plugin mimicking the ordinary auth mechanism. Used to test the auth plugin API
*/
static int auth_test_plugin(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len;

  /* send a password question */
  if (vio->write_packet(vio, (const unsigned char *) PASSWORD_QUESTION, 1))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *) pkt, info->auth_string))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, 
                          "Wrong password supplied for %s", 
                          info->auth_string);
    return CR_ERROR;
  }

  /* copy auth string as a destination name to check it */
  strcpy (info->authenticated_as, info->auth_string);

  /* copy something into the external user name */
  strcpy (info->external_user, info->auth_string);

  my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, 
                        "successfully authenticated user %s", info->authenticated_as);
  return CR_OK;
}

int generate_auth_string_hash(char *outbuf, unsigned int *buflen,
                              const char *inbuf, unsigned int inbuflen)
{
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < inbuflen)
    return 1;
  strncpy(outbuf, inbuf, inbuflen);
  *buflen= strlen(inbuf);
  return 0;
}

int validate_auth_string_hash(char* const inbuf  MY_ATTRIBUTE((unused)),
                              unsigned int buflen  MY_ATTRIBUTE((unused)))
{
  return 0;
}

int set_salt(const char* password MY_ATTRIBUTE((unused)),
             unsigned int password_len MY_ATTRIBUTE((unused)),
             unsigned char* salt MY_ATTRIBUTE((unused)),
             unsigned char* salt_len)
{
  *salt_len= 0;
  return 0;
}

static struct st_mysql_auth auth_test_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "auth_test_plugin", /* requires test_plugin client's plugin */
  auth_test_plugin,
  generate_auth_string_hash,
  validate_auth_string_hash,
  set_salt,
  AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE
};

/**
  dialog test plugin mimicking the ordinary auth mechanism. Used to test the clear text plugin API
*/
static int auth_cleartext_plugin(MYSQL_PLUGIN_VIO *vio, 
                                 MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len;

  /* read the password */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *) pkt, info->auth_string))
    return CR_ERROR;

  return CR_OK;
}


static struct st_mysql_auth auth_cleartext_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "mysql_clear_password", /* requires the clear text plugin */
  auth_cleartext_plugin,
  generate_auth_string_hash,
  validate_auth_string_hash,
  set_salt,
  AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE
};

mysql_declare_plugin(test_plugin)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &auth_test_handler,
  "test_plugin_server",
  "Georgi Kodinov",
  "plugin API test plugin",
  PLUGIN_LICENSE_GPL,
  test_plugin_init,
  NULL,
  0x0101,
  NULL,
  NULL,
  NULL,
  0,
},
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &auth_cleartext_handler,
  "cleartext_plugin_server",
  "Georgi Kodinov",
  "cleartext plugin API test plugin",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0101,
  NULL,
  NULL,
  NULL,
  0,
}
mysql_declare_plugin_end;


/********************* CLIENT SIDE ***************************************/
/*
  client plugin used for testing the plugin API
*/
#include <mysql.h>

/**
  The main function of the test plugin.

  Reads the prompt, check if the handshake is done and if the prompt is a
  password request and returns the password. Otherwise return error.

  @note
   1. this plugin shows how a client authentication plugin
      may read a MySQL protocol OK packet internally - which is important
      where a number of packets is not known in advance.
   2. the first byte of the prompt is special. it is not
      shown to the user, but signals whether it is the last question
      (prompt[0] & 1 == 1) or not last (prompt[0] & 1 == 0),
      and whether the input is a password (not echoed).
   3. the prompt is expected to be sent zero-terminated
*/
static int test_plugin_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  unsigned char *pkt, cmd= 0;
  int pkt_len, res;
  char *reply;

  do
  {
    /* read the prompt */
    pkt_len= vio->read_packet(vio, &pkt);
    if (pkt_len < 0)
      return CR_ERROR;

    if (pkt == 0)
    {
      /*
        in mysql_change_user() the client sends the first packet, so
        the first vio->read_packet() does nothing (pkt == 0).

        We send the "password", assuming the client knows what it's doing.
        (in other words, the dialog plugin should be only set as a default
        authentication plugin on the client if the first question
        asks for a password - which will be sent in clear text, by the way)
      */
      reply= mysql->passwd;
    }
    else
    {
      cmd= *pkt++;

      /* is it MySQL protocol (0=OK or 254=need old password) packet ? */
      if (cmd == 0 || cmd == 254)
        return CR_OK_HANDSHAKE_COMPLETE; /* yes. we're done */

      /*
        asking for a password with an empty prompt means mysql->password
        otherwise return an error
      */
      if ((cmd == LAST_PASSWORD[0] || cmd == PASSWORD_QUESTION[0]) && *pkt == 0)
        reply= mysql->passwd;
      else
        return CR_ERROR;
    }
    if (!reply)
      return CR_ERROR;
    /* send the reply to the server */
    res= vio->write_packet(vio, (const unsigned char *) reply, 
                           (int)strlen(reply) + 1);

    if (res)
      return CR_ERROR;

    /* repeat unless it was the last question */
  } while (cmd != LAST_QUESTION[0] && cmd != PASSWORD_QUESTION[0]);

  /* the job of reading the ok/error packet is left to the server */
  return CR_OK;
}


mysql_declare_client_plugin(AUTHENTICATION)
  "auth_test_plugin",
  "Georgi Kodinov",
  "Dialog Client Authentication Plugin",
  {0,1,0},
  "GPL",
  NULL,
  NULL,
  NULL,
  NULL,
  test_plugin_client
mysql_end_client_plugin;
