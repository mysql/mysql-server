/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mysql_native_authentication_client.h"
#include "client_async_authentication.h"
#include "config.h"
#include "crypt_genhash_impl.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql.h"
#include "mysql/plugin_auth_common.h"
#include "sha1.h"
#include "sql_common.h"

/*****************************************************************************
  The main idea is that no password are sent between client & server on
  connection and that no password are saved in mysql in a decodable form.

  On connection a random string is generated and sent to the client.
  The client generates a new string with a random generator inited with
  the hash values from the password and the sent string.
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

#if defined(WITHOUT_MYSQL_NATIVE_PASSWORD) && WITHOUT_MYSQL_NATIVE_PASSWORD == 1
#error This should not be compiled with native turned off
#endif

static inline uint8 char_val(uint8 X) {
  return (uint)(X >= '0' && X <= '9'
                    ? X - '0'
                    : X >= 'A' && X <= 'Z' ? X - 'A' + 10 : X - 'a' + 10);
}

/* Character to use as version identifier for version 4.1 */

#define PVERSION41_CHAR '*'

/*
    Convert given asciiz string of hex (0..9 a..f) characters to octet
    sequence.
  SYNOPSIS
    hex2octet()
    to        OUT buffer to place result; must be at least len/2 bytes
    str, len  IN  begin, length for character string; str and to may not
                  overlap; len % 2 == 0
*/

static void hex2octet(uint8 *to, const char *str, uint len) {
  const char *str_end = str + len;
  while (str < str_end) {
    char tmp = char_val(*str++);
    *to++ = (tmp << 4) | char_val(*str++);
  }
}

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

inline static void compute_two_stage_sha1_hash(const char *password,
                                               size_t pass_len,
                                               uint8 *hash_stage1,
                                               uint8 *hash_stage2) {
  /* Stage 1: hash password */
  compute_sha1_hash(hash_stage1, password, pass_len);

  /* Stage 2 : hash first stage's output. */
  compute_sha1_hash(hash_stage2, (const char *)hash_stage1, SHA1_HASH_SIZE);
}

/*
    MySQL 4.1.1 password hashing: SHA conversion (see RFC 2289, 3174) twice
    applied to the password string, and then produced octet sequence is
    converted to hex string.
    The result of this function is stored in the database.
  SYNOPSIS
    my_make_scrambled_password_sha1()
    buf       OUT buffer of size 2*SHA1_HASH_SIZE + 2 to store hex string
    password  IN  password string
    pass_len  IN  length of password string
*/

void my_make_scrambled_password_sha1(char *to, const char *password,
                                     size_t pass_len) {
  uint8 hash_stage2[SHA1_HASH_SIZE];

  /* Two stage SHA1 hash of the password. */
  compute_two_stage_sha1_hash(password, pass_len, (uint8 *)to, hash_stage2);

  /* convert hash_stage2 to hex string */
  *to++ = PVERSION41_CHAR;
  octet2hex(to, (const char *)hash_stage2, SHA1_HASH_SIZE);
}

/*
  Wrapper around my_make_scrambled_password() to maintain client lib ABI
  compatibility.
  In server code usage of my_make_scrambled_password() is preferred to
  avoid strlen().
  SYNOPSIS
    make_scrambled_password()
    buf       OUT buffer of size 2*SHA1_HASH_SIZE + 2 to store hex string
    password  IN  NULL-terminated password string
*/

void make_scrambled_password(char *to, const char *password) {
  my_make_scrambled_password_sha1(to, password, strlen(password));
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
    Check that scrambled message corresponds to the password.

    The function is used by server to check that received reply is authentic.
    This function does not check lengths of given strings: message must be
    null-terminated, reply and hash_stage2 must be at least SHA1_HASH_SIZE
    long (if not, something fishy is going on).

    @param scramble_arg  clients' reply, presumably produced by scramble()
    @param message       original random string, previously sent to client
                         (presumably second argument of scramble()), must be
                         exactly SCRAMBLE_LENGTH long and NULL-terminated.
    @param hash_stage2   hex2octet-decoded database entry

    @retval false  password is correct
    Wretval true   password is invalid
*/

static bool check_scramble_sha1(const uchar *scramble_arg, const char *message,
                                const uint8 *hash_stage2) {
  uint8 buf[SHA1_HASH_SIZE];
  uint8 hash_stage2_reassured[SHA1_HASH_SIZE];

  /* create key to encrypt scramble */
  compute_sha1_hash_multi(buf, message, SCRAMBLE_LENGTH,
                          (const char *)hash_stage2, SHA1_HASH_SIZE);
  /* encrypt scramble */
  my_crypt((char *)buf, buf, scramble_arg, SCRAMBLE_LENGTH);

  /* now buf supposedly contains hash_stage1: so we can get hash_stage2 */
  compute_sha1_hash(hash_stage2_reassured, (const char *)buf, SHA1_HASH_SIZE);

  return (memcmp(hash_stage2, hash_stage2_reassured, SHA1_HASH_SIZE) != 0);
}

bool check_scramble(const uchar *scramble_arg, const char *message,
                    const uint8 *hash_stage2) {
  return check_scramble_sha1(scramble_arg, message, hash_stage2);
}

/*
  Convert scrambled password from asciiz hex string to binary form.

  SYNOPSIS
    get_salt_from_password()
    res       OUT buf to hold password. Must be at least SHA1_HASH_SIZE
                  bytes long.
    password  IN  4.1.1 version value of user.password
*/

void get_salt_from_password(uint8 *hash_stage2, const char *password) {
  hex2octet(hash_stage2, password + 1 /* skip '*' */, SHA1_HASH_SIZE * 2);
}
/**
  Convert scrambled password from binary form to asciiz hex string.

  @param [out] to     store resulting string here, 2*SHA1_HASH_SIZE+2 bytes
  @param hash_stage2  password in salt format
*/

void make_password_from_salt(char *to, const uint8 *hash_stage2) {
  *to++ = PVERSION41_CHAR;
  octet2hex(to, (const char *)hash_stage2, SHA1_HASH_SIZE);
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

/**
  Client authentication plugin that does native MySQL authentication
  in a nonblocking way.

   @param[in]    vio    the channel to operate on
   @param[in]    mysql  the MYSQL structure to operate on
   @param[out]   result CR_OK : Success, CR_ERROR : error reading,
                        CR_SERVER_HANDSHAKE_ERR : malformed handshake data

   @retval     NET_ASYNC_NOT_READY  authentication not yet complete
   @retval     NET_ASYNC_COMPLETE   authentication done
*/
static net_async_status native_password_auth_client_nonblocking(
    MYSQL_PLUGIN_VIO *vio, MYSQL *mysql, int *result) {
  DBUG_TRACE;
  int io_result;
  uchar *pkt;
  mysql_async_auth *ctx = ASYNC_DATA(mysql)->connect_context->auth_context;

  switch (static_cast<client_auth_native_password_plugin_status>(
      ctx->client_auth_plugin_state)) {
    case client_auth_native_password_plugin_status::NATIVE_READING_PASSWORD:
      if (((MCPVIO_EXT *)vio)->mysql_change_user) {
        /* mysql_change_user_nonblocking not implemented yet. */
        assert(false);
      } else {
        /* read the scramble */
        const net_async_status status =
            vio->read_packet_nonblocking(vio, &pkt, &io_result);
        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }

        if (io_result < 0) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }

        if (io_result != SCRAMBLE_LENGTH + 1) {
          *result = CR_SERVER_HANDSHAKE_ERR;
          return NET_ASYNC_COMPLETE;
        }

        /* save it in MYSQL */
        memcpy(mysql->scramble, pkt, SCRAMBLE_LENGTH);
        mysql->scramble[SCRAMBLE_LENGTH] = 0;
      }
      ctx->client_auth_plugin_state = (int)
          client_auth_native_password_plugin_status::NATIVE_WRITING_RESPONSE;

      [[fallthrough]];

    case client_auth_native_password_plugin_status::NATIVE_WRITING_RESPONSE:
      if (mysql->passwd[0]) {
        char scrambled[SCRAMBLE_LENGTH + 1];
        DBUG_PRINT("info", ("sending scramble"));
        scramble(scrambled, (char *)pkt, mysql->passwd);
        const net_async_status status = vio->write_packet_nonblocking(
            vio, (uchar *)scrambled, SCRAMBLE_LENGTH, &io_result);
        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }

        if (io_result < 0) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
      } else {
        DBUG_PRINT("info", ("no password"));
        const net_async_status status = vio->write_packet_nonblocking(
            vio, nullptr, 0, &io_result); /* no password */

        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }

        if (io_result < 0) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
      }
      break;
    default:
      assert(0);
  }

  *result = CR_OK;
  return NET_ASYNC_COMPLETE;
}

auth_plugin_t native_password_client_plugin = {
    MYSQL_CLIENT_AUTHENTICATION_PLUGIN,
    MYSQL_CLIENT_AUTHENTICATION_PLUGIN_INTERFACE_VERSION,
    "mysql_native_password",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE,
    "Native MySQL authentication",
    {1, 0, 0},
    "GPL",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    native_password_auth_client,
    native_password_auth_client_nonblocking};
