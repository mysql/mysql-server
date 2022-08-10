/*  Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#include <mysql/client_plugin.h>
#include <mysql/plugin_auth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
  first byte of the question string is the question "type".
  It can be a "ordinary" or a "password" question.
  The last bit set marks a last question in the authentication exchange.
*/
#define ORDINARY_QUESTION "\2"
#define LAST_QUESTION "\3"
#define LAST_PASSWORD "\4"
#define PASSWORD_QUESTION "\5"

/********************* SERVER SIDE ****************************************/

static int qa_auth_interface(MYSQL_PLUGIN_VIO *vio,
                             MYSQL_SERVER_AUTH_INFO *info) {
  unsigned char *pkt;
  int pkt_len, err = CR_OK;

  /* send a password question */
  if (vio->write_packet(vio, (const unsigned char *)PASSWORD_QUESTION, 1))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len = vio->read_packet(vio, &pkt)) < 0) return CR_ERROR;

  info->password_used = PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *)pkt, info->auth_string)) return CR_ERROR;

  /* Check the contents of components of info */
  if (strcmp(info->user_name, "qa_test_1_user") == 0) {
    if (info->user_name_length != 14) err = CR_ERROR;
    if (strcmp(info->auth_string, "qa_test_1_dest")) err = CR_ERROR;
    if (info->auth_string_length != 14) err = CR_ERROR;
    /*
      To be set by the plugin
        if (strcmp(info->authenticated_as, "qa_test_1_user"))
             err= CR_ERROR;
        if (strcmp(info->external_user, ""))
             err= CR_ERROR;
    */
    if (info->password_used != PASSWORD_USED_YES) err = CR_ERROR;
    if (strcmp(info->host_or_ip, "localhost")) err = CR_ERROR;
    if (info->host_or_ip_length != 9) err = CR_ERROR;
  }
  /* Assign values to the components of info even if not intended and watch the
     effect */
  else if (strcmp(info->user_name, "qa_test_2_user") == 0) {
    /* Overwriting not intended, but with effect on USER() */
    strcpy(info->user_name, "user_name");
    info->user_name_length = 9;
    /* Overwriting not intended, effect not visible */
    strcpy(const_cast<char *>(info->auth_string), "auth_string");
    info->auth_string_length = 11;
    /* Assign with account for authorization, effect on CURRENT_USER() */
    strcpy(info->authenticated_as, "authenticated_as");
    /* Assign with an external account, effect on @@local.EXTERNAL_USER */
    strcpy(info->external_user, "externaluser");
    /*
      Overwriting will cause a core dump
      strcpy(info->host_or_ip, "host_or_ip");
      info->host_or_ip_length= 10;
    */
  }
  /* Invalid, means too high values for length */
  else if (strcmp(info->user_name, "qa_test_3_user") == 0) {
    /* Original value is 14. Test runs also with higher value. Changes have no
     * effect.*/
    info->user_name_length = 28;
    strcpy(const_cast<char *>(info->auth_string), "qa_test_3_dest");
    /* Original value is 14. Test runs also with higher value. Changes have no
     * effect.*/
    info->auth_string_length = 28;
    strcpy(info->authenticated_as, info->auth_string);
    strcpy(info->external_user, info->auth_string);
  }
  /* Invalid, means too low values for length */
  else if (strcmp(info->user_name, "qa_test_4_user") == 0) {
    /* Original value is 14. Test runs also with lower value. Changes have no
     * effect.*/
    info->user_name_length = 8;
    strcpy(const_cast<char *>(info->auth_string), "qa_test_4_dest");
    /* Original value is 14. Test runs also with lower value. Changes have no
     * effect.*/
    info->auth_string_length = 8;
    strcpy(info->authenticated_as, info->auth_string);
    strcpy(info->external_user, info->auth_string);
  }
  /* Overwrite with empty values */
  else if (strcmp(info->user_name, "qa_test_5_user") == 0) {
    /* This assignment has no effect.*/
    strcpy(info->user_name, "");
    info->user_name_length = 0;
    /* This assignment has no effect.*/
    strcpy(const_cast<char *>(info->auth_string), "");
    info->auth_string_length = 0;
    /* This assignment caused an error or an "empty" user */
    strcpy(info->authenticated_as, "");
    /* This assignment has no effect.*/
    strcpy(info->external_user, "");
    /*
      Overwriting will cause a core dump
      strcpy(info->host_or_ip, "");
      info->host_or_ip_length= 0;
    */
  }
  /* Set to 'root' */
  else if (strcmp(info->user_name, "qa_test_6_user") == 0) {
    strcpy(info->authenticated_as, "root");
  } else {
    err = CR_ERROR;
  }
  return err;
}

static int generate_auth_string_hash(char *outbuf, unsigned int *buflen,
                                     const char *inbuf, unsigned int inbuflen) {
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < inbuflen) return 1;
  strncpy(outbuf, inbuf, inbuflen);
  *buflen = strlen(inbuf);
  return 0;
}

static int validate_auth_string_hash(char *const inbuf [[maybe_unused]],
                                     unsigned int buflen [[maybe_unused]]) {
  return 0;
}

static int set_salt(const char *password [[maybe_unused]],
                    unsigned int password_len [[maybe_unused]],
                    unsigned char *salt [[maybe_unused]],
                    unsigned char *salt_len) {
  *salt_len = 0;
  return 0;
}

static struct st_mysql_auth qa_auth_test_handler = {
    MYSQL_AUTHENTICATION_INTERFACE_VERSION,
    "qa_auth_interface", /* requires test_plugin client's plugin */
    qa_auth_interface,
    generate_auth_string_hash,
    validate_auth_string_hash,
    set_salt,
    AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE,
    nullptr};

mysql_declare_plugin(test_plugin){
    MYSQL_AUTHENTICATION_PLUGIN,
    &qa_auth_test_handler,
    "qa_auth_interface",
    PLUGIN_AUTHOR_ORACLE,
    "plugin API test plugin",
    PLUGIN_LICENSE_GPL,
    nullptr, /* Init */
    nullptr, /* Check uninstall */
    nullptr, /* Deinit */
    0x0101,
    nullptr,
    nullptr,
    nullptr,
    0,
} mysql_declare_plugin_end;

/********************* CLIENT SIDE ***************************************/
/*
  client plugin used for testing the plugin API
*/
#include <mysql.h>

#include "my_compiler.h"

#ifdef __clang__
// Clang UBSAN false positive?
// Call to function through pointer to incorrect function type
static int test_plugin_client(MYSQL_PLUGIN_VIO *vio,
                              MYSQL *mysql) SUPPRESS_UBSAN;
#endif  // __clang__

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
static int test_plugin_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  unsigned char *pkt, cmd = 0;
  int pkt_len, res;
  char *reply;

  do {
    /* read the prompt */
    pkt_len = vio->read_packet(vio, &pkt);
    if (pkt_len < 0) return CR_ERROR;

    if (pkt == nullptr) {
      /*
        in mysql_change_user() the client sends the first packet, so
        the first vio->read_packet() does nothing (pkt == 0).

        We send the "password", assuming the client knows what its doing.
        (in other words, the dialog plugin should be only set as a default
        authentication plugin on the client if the first question
        asks for a password - which will be sent in cleat text, by the way)
      */
      reply = mysql->passwd;
    } else {
      cmd = *pkt++;

      /* is it MySQL protocol (0=OK or 254=need old password) packet ? */
      if (cmd == 0 || cmd == 254)
        return CR_OK_HANDSHAKE_COMPLETE; /* yes. we're done */

      /*
        asking for a password with an empty prompt means mysql->password
        otherwise return an error
      */
      if ((cmd == LAST_PASSWORD[0] || cmd == PASSWORD_QUESTION[0]) && *pkt == 0)
        reply = mysql->passwd;
      else
        return CR_ERROR;
    }
    if (!reply) return CR_ERROR;
    /* send the reply to the server */
    res = vio->write_packet(vio, (const unsigned char *)reply,
                            (int)strlen(reply) + 1);

    if (res) return CR_ERROR;

    /* repeat unless it was the last question */
  } while (cmd != LAST_QUESTION[0] && cmd != PASSWORD_QUESTION[0]);

  /* the job of reading the ok/error packet is left to the server */
  return CR_OK;
}

mysql_declare_client_plugin(AUTHENTICATION) "qa_auth_interface",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE, "Dialog Client Authentication Plugin",
    {0, 1, 0}, "GPL", nullptr, nullptr, nullptr, nullptr,
    nullptr, test_plugin_client, nullptr, mysql_end_client_plugin;
