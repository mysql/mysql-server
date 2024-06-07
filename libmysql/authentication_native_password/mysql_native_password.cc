/* Copyright (c) 2024, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  This file is included by both libmysql (the MySQL client C API)
  and the mysqld server to connect to another MYSQL server.
*/

#include "my_dbug.h"
#include "my_sys.h"
#include "sha1.h"
#include "sql_common.h"

#define MYSQL_NATIVE_PASSWORD_PLUGIN_NAME "mysql_native_password"

/*****************************************************************************
  The main idea is that no password are sent between client & server on
  connection and that no password are saved in mysql in a decodable form.

  On connection a random string is generated and sent to the client.
  The client takes the random string from server and password provided
  by the user as an input and generates a new 'check' string.
  This 'check' string is sent to the server where it is compared with
  a string generated from the stored hash_value of the password and the
  random string.

  The password is saved (in user.authentication_string).

  Example:
    SET PASSWORD for test = 'haha'
  This saves a hashed number as a string in the authentication_string field.

  The new authentication is performed in following manner:

  SERVER:  public_seed=generate_user_salt()
           send(public_seed)

  CLIENT:  recv(public_seed)
           hash_stage1=sha1("password")
           hash_stage2=sha1(hash_stage1)
           reply=xor(hash_stage1, sha1(public_seed,hash_stage2)

           // this three steps are done in scramble()

           send(reply)


  SERVER:  recv(reply)
           hash_stage1=xor(reply, sha1(public_seed,hash_stage2))
           candidate_hash2=sha1(hash_stage1)
           check(candidate_hash2==hash_stage2)

           // this three steps are done in check_scramble()

*****************************************************************************/

/*
    Encrypt/Decrypt function used for password encryption in authentication.
    Simple XOR is used here, but it is OK as we crypt random strings. Note
    that XOR(s1, XOR(s1, s2)) == s2, XOR(s1, s2) == XOR(s2, s1)
  SYNOPSIS
    my_crypt()
    to      OUT buffer to hold encrypted string; must be at least len bytes
                long; to and s1 (or s2) may be the same.
    s1, s2  IN  input strings (of equal length)
    len     IN  length of s1 and s2
*/

static void my_crypt(char *to, const uchar *s1, const uchar *s2, uint len) {
  const uint8 *s1_end = s1 + len;
  while (s1 < s1_end) *to++ = *s1++ ^ *s2++;
}

/**
  Compute two stage SHA1 hash of the password :

    hash_stage1=sha1("password")
    hash_stage2=sha1(hash_stage1)

  @param [in] password       Password string.
  @param [in] pass_len       Length of the password.
  @param [out] hash_stage1   sha1(password)
  @param [out] hash_stage2   sha1(hash_stage1)
*/

inline void compute_two_stage_sha1_hash(const char *password, size_t pass_len,
                                        uint8 *hash_stage1,
                                        uint8 *hash_stage2) {
  /* Stage 1: hash password */
  compute_sha1_hash(hash_stage1, password, pass_len);

  /* Stage 2 : hash first stage's output. */
  compute_sha1_hash(hash_stage2, (const char *)hash_stage1, SHA1_HASH_SIZE);
}

/**
    Produce an obscure octet sequence from password and random
    string, received from the server. This sequence corresponds to the
    password, but password can not be easily restored from it. The sequence
    is then sent to the server for validation. Trailing zero is not stored
    in the buf as it is not needed.
    This function is used by client to create authenticated reply to the
    server's greeting.

    @param[out] to   store scrambled string here. The buf must be at least
                     SHA1_HASH_SIZE bytes long.
    @param message   random message, must be exactly SCRAMBLE_LENGTH long and
                     NULL-terminated.
    @param password  users' password, NULL-terminated
*/

void scramble(char *to, const char *message, const char *password) {
  uint8 hash_stage1[SHA1_HASH_SIZE];
  uint8 hash_stage2[SHA1_HASH_SIZE];

  /* Two stage SHA1 hash of the password. */
  compute_two_stage_sha1_hash(password, strlen(password), hash_stage1,
                              hash_stage2);

  /* create crypt string as sha1(message, hash_stage2) */;
  compute_sha1_hash_multi((uint8 *)to, message, SCRAMBLE_LENGTH,
                          (const char *)hash_stage2, SHA1_HASH_SIZE);
  my_crypt(to, (const uchar *)to, hash_stage1, SCRAMBLE_LENGTH);
}

/**
  Client authentication plugin that does native MySQL authentication
   using a 20-byte (4.1+) scramble

   @param vio    the channel to operate on
   @param mysql  the MYSQL structure to operate on

   @retval -1    ::CR_OK : Success
   @retval 1     ::CR_ERROR : error reading
   @retval 2012  ::CR_SERVER_HANDSHAKE_ERR : malformed handshake data
*/
static int native_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  int pkt_len;
  uchar *pkt;

  DBUG_TRACE;

  /* read the scramble */
  if ((pkt_len = vio->read_packet(vio, &pkt)) < 0) return CR_ERROR;

  if (pkt_len != SCRAMBLE_LENGTH + 1) return CR_SERVER_HANDSHAKE_ERR;

  /* save it in MYSQL */
  memcpy(mysql->scramble, pkt, SCRAMBLE_LENGTH);
  mysql->scramble[SCRAMBLE_LENGTH] = 0;

  if (mysql->passwd[0]) {
    char scrambled[SCRAMBLE_LENGTH + 1];
    DBUG_PRINT("info", ("sending scramble"));
    scramble(scrambled, (char *)pkt, mysql->passwd);
    if (vio->write_packet(vio, (uchar *)scrambled, SCRAMBLE_LENGTH))
      return CR_ERROR;
  } else {
    DBUG_PRINT("info", ("no password"));
    if (vio->write_packet(vio, nullptr, 0)) /* no password */
      return CR_ERROR;
  }

  return CR_OK;
}

mysql_declare_client_plugin(AUTHENTICATION) MYSQL_NATIVE_PASSWORD_PLUGIN_NAME
    ,                                     /* name */
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE,    /* author */
    "MySQL Native Authentication Client", /* description */
    {0, 1, 0},                            /* version */
    "GPL",                                /* license */
    nullptr,                              /* mysql_api */
    nullptr,                              /* init */
    nullptr,                              /* deinit */
    nullptr,                              /* options */
    nullptr,                              /* get_options */
    native_password_auth_client,          /* authenticate_user */
    nullptr,                              /* authenticate_user_nonblocking */
    mysql_end_client_plugin;
