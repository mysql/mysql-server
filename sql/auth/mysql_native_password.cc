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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define LOG_COMPONENT_TAG "mysql_native_password"

#include "sql/auth/sql_authentication.h"

#include "crypt_genhash_impl.h"  // generate_user_salt
#include "mysql/components/services/log_builtins.h"
#include "password.h"                 // my_make_scrambled_password
#include "sql/auth/sql_auth_cache.h"  // acl_cache
#include "sql/current_thd.h"          // current_thd
#include "sql/mysqld.h"               // global_system_variables
#include "sql/sql_class.h"            // THD

/* clang-format off */
/**
  @page page_protocol_connection_phase_authentication_methods_native_password_authentication Native Authentication

  Authentication::Native41:

  <ul>
  <li>
  The server name is *mysql_native_password*
  </li>
  <li>
  The client name is *mysql_native_password*
  </li>
  <li>
  Client side requires an 20-byte random challenge from server
  </li>
  <li>
  Client side sends a 20-byte response packet based on the algorithm described
  later.
  </li>
  </ul>

  @par "Requires" @ref CLIENT_RESERVED2 "CLIENT_SECURE_CONNECTION"

  @startuml
  Client<-Server: 20 byte random data
  Client->Server: 20 byte scrambled password
  @enduml

  This method fixes a 2 short-comings of the
  @ref page_protocol_connection_phase_authentication_methods_old_password_authentication

  1. using a tested, crypto-graphic hashing function (SHA1)
  2. knowing the content of the hash in the mysql.user table isn't enough
     to authenticate against the MySQL Server.

  The network packet content for the password is calculated by:
  ~~~~~
  SHA1( password ) XOR SHA1( "20-bytes random data from server" <concat> SHA1( SHA1( password ) ) )
  ~~~~~

  The following is stored into mysql.user.authentication_string
  ~~~~~
  SHA1( SHA1( password ) )
  ~~~~~

  @sa native_password_authenticate, native_password_auth_client,
  native_password_client_plugin, native_password_handler,
  check_scramble_sha1, compute_two_stage_sha1_hash, make_password_from_salt
*/
/* clang-format on */

static void native_password_authentication_deprecation_warning() {
  /*
    Deprecate message for mysql_native_password plugin.
  */
  LogPluginErr(WARNING_LEVEL, ER_SERVER_WARN_DEPRECATED,
               Cached_authentication_plugins::get_plugin_name(
                   PLUGIN_MYSQL_NATIVE_PASSWORD),
               Cached_authentication_plugins::get_plugin_name(
                   PLUGIN_CACHING_SHA2_PASSWORD));
}

static int generate_native_password(char *outbuf, unsigned int *buflen,
                                    const char *inbuf, unsigned int inbuflen) {
  THD *thd = current_thd;

  native_password_authentication_deprecation_warning();

  if (!thd->m_disable_password_validation) {
    if (my_validate_password_policy(inbuf, inbuflen)) return 1;
  }
  /* for empty passwords */
  if (inbuflen == 0) {
    *buflen = 0;
    return 0;
  }
  char *buffer = (char *)my_malloc(PSI_NOT_INSTRUMENTED,
                                   SCRAMBLED_PASSWORD_CHAR_LENGTH + 1, MYF(0));
  if (buffer == nullptr) return 1;
  my_make_scrambled_password_sha1(buffer, inbuf, inbuflen);
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < strlen(buffer)) {
    my_free(buffer);
    return 1;
  }
  *buflen = SCRAMBLED_PASSWORD_CHAR_LENGTH;
  memcpy(outbuf, buffer, *buflen);
  my_free(buffer);
  return 0;
}

static int validate_native_password_hash(char *const inbuf,
                                         unsigned int buflen) {
  /* empty password is also valid */
  if ((buflen && buflen == SCRAMBLED_PASSWORD_CHAR_LENGTH && inbuf[0] == '*') ||
      buflen == 0)
    return 0;
  return 1;
}

static int set_native_salt(const char *password, unsigned int password_len,
                           unsigned char *salt, unsigned char *salt_len) {
  /* for empty passwords salt_len is 0 */
  if (password_len == 0)
    *salt_len = 0;
  else {
    if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH) {
      get_salt_from_password(salt, password);
      *salt_len = SCRAMBLE_LENGTH;
    }
  }
  return 0;
}

/**
  Compare a clear text password with a stored hash for
  the native password plugin

  If the password is non-empty it calculates a hash from
  the cleartext and compares it with the supplied hash.

  if the password is empty checks if the hash is empty too.

  @arg hash              pointer to the hashed data
  @arg hash_length       length of the hashed data
  @arg cleartext         pointer to the clear text password
  @arg cleartext_length  length of the cleat text password
  @arg[out] is_error     non-zero in case of error extracting the salt
  @retval 0              the hash was created with that password
  @retval non-zero       the hash was created with a different password
*/
static int compare_native_password_with_hash(const char *hash,
                                             unsigned long hash_length,
                                             const char *cleartext,
                                             unsigned long cleartext_length,
                                             int *is_error) {
  DBUG_TRACE;

  char buffer[SCRAMBLED_PASSWORD_CHAR_LENGTH + 1];

  /** empty password results in an empty hash */
  if (!hash_length && !cleartext_length) return 0;

  assert(hash_length <= SCRAMBLED_PASSWORD_CHAR_LENGTH);

  /* calculate the hash from the clear text */
  my_make_scrambled_password_sha1(buffer, cleartext, cleartext_length);

  *is_error = 0;
  const int result = memcmp(hash, buffer, SCRAMBLED_PASSWORD_CHAR_LENGTH);

  return result;
}

/**
  MySQL Server Password Authentication Plugin

  In the MySQL authentication protocol:
  1. the server sends the random scramble to the client
  2. client sends the encrypted password back to the server
  3. the server checks the password.
*/
static int native_password_authenticate(MYSQL_PLUGIN_VIO *vio,
                                        MYSQL_SERVER_AUTH_INFO *info) {
  uchar *pkt;
  int pkt_len;
  MPVIO_EXT *mpvio = (MPVIO_EXT *)vio;

  DBUG_TRACE;

  native_password_authentication_deprecation_warning();

  /* generate the scramble, or reuse the old one */
  if (mpvio->scramble[SCRAMBLE_LENGTH])
    generate_user_salt(mpvio->scramble, SCRAMBLE_LENGTH + 1);

  /* send it to the client */
  if (mpvio->write_packet(mpvio, (uchar *)mpvio->scramble, SCRAMBLE_LENGTH + 1))
    return CR_AUTH_HANDSHAKE;

  /* reply and authenticate */

  /*
    <digression>
      This is more complex than it looks.

      The plugin (we) may be called right after the client was connected -
      and will need to send a scramble, read reply, authenticate.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply. If the client has used the correct client-plugin,
      we won't need to read anything here from the client, the client
      has already sent a reply with everything we need for authentication.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply, but the client has used the wrong client-plugin.
      We'll need to sent a "switch to another plugin" packet to the
      client and read the reply. "Use the short scramble" packet is a special
      case of "switch to another plugin" packet.

      Or, perhaps, the plugin may be called after another plugin has
      done the handshake but did not send a useful scramble. We'll need
      to send a scramble (and perhaps a "switch to another plugin" packet)
      and read the reply.

      Besides, a client may be an old one, that doesn't understand plugins.
      Or doesn't even understand 4.0 scramble.

      And we want to keep the same protocol on the wire  unless non-native
      plugins are involved.

      Anyway, it still looks simple from a plugin point of view:
      "send the scramble, read the reply and authenticate"
      All the magic is transparently handled by the server.
    </digression>
  */

  /* read the reply with the encrypted password */
  if ((pkt_len = mpvio->read_packet(mpvio, &pkt)) < 0) return CR_AUTH_HANDSHAKE;
  DBUG_PRINT("info", ("reply read : pkt_len=%d", pkt_len));

  DBUG_EXECUTE_IF("native_password_bad_reply", {
    /* This should cause a HANDSHAKE ERROR */
    pkt_len = 12;
  });
  if (mysql_native_password_proxy_users) {
    *info->authenticated_as = PROXY_FLAG;
    DBUG_PRINT("info", ("mysql_native_authentication_proxy_users is enabled, "
                        "setting authenticated_as to NULL"));
  }
  if (pkt_len == 0) {
    info->password_used = PASSWORD_USED_NO;
    return mpvio->acl_user->credentials[PRIMARY_CRED].m_salt_len != 0
               ? CR_AUTH_USER_CREDENTIALS
               : CR_OK;
  } else
    info->password_used = PASSWORD_USED_YES;
  bool second = false;
  if (pkt_len == SCRAMBLE_LENGTH) {
    if (!mpvio->acl_user->credentials[PRIMARY_CRED].m_salt_len ||
        check_scramble(pkt, mpvio->scramble,
                       mpvio->acl_user->credentials[PRIMARY_CRED].m_salt)) {
      second = true;
      if (!mpvio->acl_user->credentials[SECOND_CRED].m_salt_len ||
          check_scramble(pkt, mpvio->scramble,
                         mpvio->acl_user->credentials[SECOND_CRED].m_salt)) {
        return CR_AUTH_USER_CREDENTIALS;
      } else {
        if (second) {
          MPVIO_EXT *mpvio_second = pointer_cast<MPVIO_EXT *>(vio);
          const char *username =
              *info->authenticated_as ? info->authenticated_as : "";
          const char *hostname = mpvio_second->acl_user->host.get_host();
          LogPluginErr(
              INFORMATION_LEVEL,
              ER_MYSQL_NATIVE_PASSWORD_SECOND_PASSWORD_USED_INFORMATION,
              username, hostname ? hostname : "");
        }
        return CR_OK;
      }
    } else {
      return CR_OK;
    }
  }

  my_error(ER_HANDSHAKE_ERROR, MYF(0));
  return CR_AUTH_HANDSHAKE;
}

static struct st_mysql_auth native_password_handler = {
    MYSQL_AUTHENTICATION_INTERFACE_VERSION,
    Cached_authentication_plugins::get_plugin_name(
        PLUGIN_MYSQL_NATIVE_PASSWORD),
    native_password_authenticate,
    generate_native_password,
    validate_native_password_hash,
    set_native_salt,
    AUTH_FLAG_USES_INTERNAL_STORAGE,
    compare_native_password_with_hash,
};

mysql_declare_plugin(mysql_native_password){
    MYSQL_AUTHENTICATION_PLUGIN, /* type constant    */
    &native_password_handler,    /* type descriptor  */
    Cached_authentication_plugins::get_plugin_name(
        PLUGIN_MYSQL_NATIVE_PASSWORD), /* Name           */
    PLUGIN_AUTHOR_ORACLE,              /* Author           */
    "Native MySQL authentication",     /* Description      */
    PLUGIN_LICENSE_GPL,                /* License          */
    nullptr,                           /* Init function    */
    nullptr,                           /* Check uninstall  */
    nullptr,                           /* Deinit function  */
    0x0101,                            /* Version (1.0)    */
    nullptr,                           /* status variables */
    nullptr,                           /* system variables */
    nullptr,                           /* config options   */
    PLUGIN_OPT_DEFAULT_OFF,            /* flags            */
} mysql_declare_plugin_end;
