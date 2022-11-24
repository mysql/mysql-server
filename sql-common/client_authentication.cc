/* Copyright (c) 2011, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <stdarg.h>
#include <string.h>

#include "my_dbug.h"
#include "my_inttypes.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include "crypt_genhash_impl.h"
#include "errmsg.h"
#include "m_ctype.h"
#include "mysql/client_authentication.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysys_err.h"
#include "sql_common.h"
#include "sql_string.h"
#if defined(_WIN32) && !defined(_OPENSSL_Applink) && \
    defined(HAVE_OPENSSL_APPLINK_C)
#include <openssl/applink.c>
#endif
#include "client_async_authentication.h"
#include "mysql/plugin.h"
#include "sha2.h"
#include "violite.h"

#define MAX_CIPHER_LENGTH 1024
#define PASSWORD_SCRAMBLE_LENGTH 512
#define SHA2_SCRAMBLE_LENGTH SHA256_DIGEST_LENGTH

mysql_mutex_t g_public_key_mutex;

int sha256_password_init(char *, size_t, int, va_list) {
  mysql_mutex_init(0, &g_public_key_mutex, MY_MUTEX_INIT_SLOW);
  return 0;
}

int sha256_password_deinit(void) {
  mysql_reset_server_public_key();
  mysql_mutex_destroy(&g_public_key_mutex);
  return 0;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static EVP_PKEY *g_public_key = nullptr;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
static RSA *g_public_key = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

/**
  Reads and parse RSA public key data from a file.

  @param mysql connection handle with file path data

  @return Pointer to the RSA public key storage buffer
*/
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static EVP_PKEY *rsa_init(MYSQL *mysql) {
  EVP_PKEY *key = nullptr;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
static RSA *rsa_init(MYSQL *mysql) {
  RSA *key = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

  mysql_mutex_lock(&g_public_key_mutex);
  key = g_public_key;
  mysql_mutex_unlock(&g_public_key_mutex);

  if (key != nullptr) return key;

  FILE *pub_key_file = nullptr;

  if (mysql->options.extension != nullptr &&
      mysql->options.extension->server_public_key_path != nullptr &&
      mysql->options.extension->server_public_key_path[0] != '\0') {
    pub_key_file =
        fopen(mysql->options.extension->server_public_key_path, "rb");
  }
  /* No public key is used; return 0 without errors to indicate this. */
  else
    return nullptr;

  if (pub_key_file == nullptr) {
    /*
      If a key path was submitted but no key located then we print an error
      message. Else we just report that there is no public key.
    */
    my_message_local(WARNING_LEVEL, EE_FAILED_TO_LOCATE_SERVER_PUBLIC_KEY,
                     mysql->options.extension->server_public_key_path);

    return nullptr;
  }

  mysql_mutex_lock(&g_public_key_mutex);
  key = g_public_key =
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      PEM_read_PUBKEY(pub_key_file, nullptr, nullptr, nullptr);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      PEM_read_RSA_PUBKEY(pub_key_file, nullptr, nullptr, nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  mysql_mutex_unlock(&g_public_key_mutex);
  fclose(pub_key_file);
  if (g_public_key == nullptr) {
    ERR_clear_error();
    my_message_local(WARNING_LEVEL, EE_PUBLIC_KEY_NOT_IN_PEM_FORMAT,
                     mysql->options.extension->server_public_key_path);
    return nullptr;
  }

  return key;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static bool encrypt_RSA_public_key(const unsigned char *password,
                                   int password_len, unsigned char *to,
                                   size_t *to_len, EVP_PKEY *public_key) {
  EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new(public_key, nullptr);
  if (!key_ctx) return true;
  if (EVP_PKEY_encrypt_init(key_ctx) <= 0 ||
      EVP_PKEY_CTX_set_rsa_padding(key_ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
      EVP_PKEY_encrypt(key_ctx, to, to_len, password, password_len) <= 0) {
    EVP_PKEY_CTX_free(key_ctx);
    return true;
  }
  EVP_PKEY_CTX_free(key_ctx);
  return false;
}
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
static bool encrypt_RSA_public_key(const unsigned char *password,
                                   int password_len, unsigned char *to,
                                   RSA *public_key) {
  if (RSA_public_encrypt(password_len, password, to, public_key,
                         RSA_PKCS1_OAEP_PADDING) == -1)
    return true;
  return false;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

/**
  Authenticate the client using the RSA or TLS and a SHA256 salted password.

  @param vio Provides plugin access to communication channel
  @param mysql Client connection handler

  @return Error status
    @retval CR_ERROR An error occurred.
    @retval CR_OK Authentication succeeded.
*/

int sha256_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  bool uses_password = mysql->passwd[0] != 0;
  unsigned char encrypted_password[MAX_CIPHER_LENGTH];
  static char request_public_key = '\1';
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY *public_key = nullptr;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  RSA *public_key = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  bool got_public_key_from_server = false;
  bool connection_is_secure = false;
  unsigned char scramble_pkt[SCRAMBLE_LENGTH]{};
  unsigned char *pkt;

  DBUG_TRACE;

  /*
    Get the scramble from the server because we need it when sending encrypted
    password.
  */
  if (vio->read_packet(vio, &pkt) != SCRAMBLE_LENGTH + 1) {
    DBUG_PRINT("info", ("Scramble is not of correct length."));
    return CR_ERROR;
  }
  if (pkt[SCRAMBLE_LENGTH] != '\0') {
    DBUG_PRINT("info", ("Missing protocol token in scramble data."));
    return CR_ERROR;
  }
  /*
    Copy the scramble to the stack or it will be lost on the next use of the
    net buffer.
  */
  memcpy(scramble_pkt, pkt, SCRAMBLE_LENGTH);

  if (mysql_get_ssl_cipher(mysql) != nullptr) connection_is_secure = true;

  /* If connection isn't secure attempt to get the RSA public key file */
  if (!connection_is_secure) {
    public_key = rsa_init(mysql);
  }

  if (!uses_password) {
    /* We're not using a password */
    static const unsigned char zero_byte = '\0';
    if (vio->write_packet(vio, &zero_byte, 1)) return CR_ERROR;
  } else {
    /* Password is a 0-terminated byte array ('\0' character included) */
    unsigned int passwd_len =
        static_cast<unsigned int>(strlen(mysql->passwd) + 1);
    if (!connection_is_secure) {
      /*
        If no public key; request one from the server.
      */
      if (public_key == nullptr) {
        if (vio->write_packet(vio, (const unsigned char *)&request_public_key,
                              1))
          return CR_ERROR;

        int packet_len = 0;
        unsigned char *packet;
        if ((packet_len = vio->read_packet(vio, &packet)) == -1)
          return CR_ERROR;
        BIO *bio = BIO_new_mem_buf(packet, packet_len);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        public_key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        public_key = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        BIO_free(bio);
        if (public_key == nullptr) {
          ERR_clear_error();
          return CR_ERROR;
        }
        got_public_key_from_server = true;
      }

      /*
        An arbitrary limitation based on the assumption that passwords
        larger than e.g. 15 symbols don't contribute to security.
        Note also that it's further restricted to RSA_size() - 41 down
        below, so this leaves 471 bytes of possible RSA key sizes which
        should be reasonably future-proof.
        We avoid heap allocation for speed reasons.
      */
      char passwd_scramble[PASSWORD_SCRAMBLE_LENGTH];

      if (passwd_len > sizeof(passwd_scramble)) {
        /* password too long for the buffer */
        goto err;
      }
      memmove(passwd_scramble, mysql->passwd, passwd_len);

      /* Obfuscate the plain text password with the session scramble */
      xor_string(passwd_scramble, passwd_len - 1, (char *)scramble_pkt,
                 SCRAMBLE_LENGTH);
      /* Encrypt the password and send it to the server */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      int cipher_length = EVP_PKEY_get_size(public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      int cipher_length = RSA_size(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      /*
        When using RSA_PKCS1_OAEP_PADDING the password length must be less
        than RSA_size(rsa) - 41.
      */
      if (passwd_len + 41 >= (unsigned)cipher_length) {
        /* password message is to long */
        goto err;
      }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      {
        size_t encrypted_password_len = sizeof(encrypted_password);

        if (encrypt_RSA_public_key((unsigned char *)passwd_scramble, passwd_len,
                                   encrypted_password, &encrypted_password_len,
                                   public_key))
          goto err;

        if (got_public_key_from_server) EVP_PKEY_free(public_key);
      }
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      if (encrypt_RSA_public_key((unsigned char *)passwd_scramble, passwd_len,
                                 encrypted_password, public_key))
        goto err;

      if (got_public_key_from_server) RSA_free(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      if (vio->write_packet(vio, (uchar *)encrypted_password, cipher_length))
        return CR_ERROR;
    } else {
      /* The vio is encrypted already; just send the plain text passwd */
      if (vio->write_packet(vio, (uchar *)mysql->passwd, passwd_len))
        return CR_ERROR;
    }
  }

  return CR_OK;

err:
  if (got_public_key_from_server)
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY_free(public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
    RSA_free(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  return CR_ERROR;
}

/**
  Read RSA public key sent by server - used by nonblocking
  version of caching_sha2_password and sha256_password
  plugins

  @param [in]      vio       VIO handle to read data from server
  @param [in, out] ctx       Async authentication context to store data
  @param [out]     result    Authentication process result
  @param [out]     got_public_key_from_server Flag to be used for cleanup
  @param [out]     status    Async status

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
static bool read_public_key_nonblocking(MYSQL_PLUGIN_VIO *vio,
                                        mysql_async_auth *ctx, int *result,
                                        bool &got_public_key_from_server,
                                        net_async_status &status) {
  unsigned char *pkt = nullptr;
  int io_result;
  status = vio->read_packet_nonblocking(vio, &pkt, &io_result);
  if (status == NET_ASYNC_NOT_READY) return true;

  if (io_result <= 0) {
    *result = CR_ERROR;
    status = NET_ASYNC_COMPLETE;
    return true;
  }

  BIO *bio = BIO_new_mem_buf(pkt, io_result);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  ctx->sha2_auth.public_key =
      PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  ctx->sha2_auth.public_key =
      PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  BIO_free(bio);
  if (ctx->sha2_auth.public_key == nullptr) {
    ERR_clear_error();
    DBUG_PRINT("info", ("Failed to parse public key"));
    *result = CR_ERROR;
    status = NET_ASYNC_COMPLETE;
    return true;
  }
  got_public_key_from_server = true;
  return false;
}

/** Helper function to free RSA key */
void free_rsa_key(mysql_async_auth *ctx) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY_free(ctx->sha2_auth.public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  RSA_free(ctx->sha2_auth.public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}

/**
  Prepare public key and password for encryption

  @param [in]  ctx              Async authentication context to retrieve data
  @param [out] passwd_scramble  Buffer to store scramble. Must be allocated
  @param [in]  scramble_length  Length of the out buffer
  @param [in]  passwd           Password
  @param [in]  passwd_len       Length of password

  @returns Result of the processing
    @retval false Success
    @retval true  Failure
*/
static bool process_public_key_and_prepare_scramble_nonblocking(
    mysql_async_auth *ctx, char *passwd_scramble, size_t scramble_length,
    const char *passwd, unsigned int passwd_len) {
  if (passwd_len > scramble_length) {
    /* password too long for the buffer */
    DBUG_PRINT("info", ("Password is too long."));
    return true;
  }

  memmove(passwd_scramble, passwd, passwd_len);
  /* Obfuscate the plain text password with the session scramble */
  xor_string(passwd_scramble, passwd_len - 1,
             (char *)ctx->sha2_auth.scramble_pkt,
             sizeof(ctx->sha2_auth.scramble_pkt));

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  ctx->sha2_auth.cipher_length = EVP_PKEY_get_size(ctx->sha2_auth.public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  ctx->sha2_auth.cipher_length = RSA_size(ctx->sha2_auth.public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

  /*
    When using RSA_PKCS1_OAEP_PADDING the password length
    must be less than RSA_size(rsa) - 41.
  */
  if (passwd_len + 41 >= (unsigned)ctx->sha2_auth.cipher_length) {
    /* password message is to long */
    DBUG_PRINT("info", ("Password is too long to be encrypted using "
                        "given public key."));
    return true;
  }
  return false;
}

/**
  Non blocking version of sha256_password_auth_client
*/
net_async_status sha256_password_auth_client_nonblocking(MYSQL_PLUGIN_VIO *vio,
                                                         MYSQL *mysql,
                                                         int *result) {
  DBUG_TRACE;
  net_async_status status = NET_ASYNC_NOT_READY;
  bool uses_password = mysql->passwd[0] != 0;
  static char request_public_key = '\1';
  bool got_public_key_from_server = false;
  int io_result;
  bool connection_is_secure = (mysql_get_ssl_cipher(mysql) != nullptr);
  unsigned char *pkt;
  unsigned int passwd_len =
      static_cast<unsigned int>(strlen(mysql->passwd) + 1);

  mysql_async_auth *ctx = ASYNC_DATA(mysql)->connect_context->auth_context;
  switch (static_cast<client_auth_sha256_password_plugin_status>(
      ctx->client_auth_plugin_state)) {
    case client_auth_sha256_password_plugin_status::SHA256_READING_PASSWORD:
      status = vio->read_packet_nonblocking(vio, &pkt, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result != SCRAMBLE_LENGTH + 1) {
        DBUG_PRINT("info", ("Scramble is not of correct length."));
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      if (pkt[SCRAMBLE_LENGTH] != '\0') {
        DBUG_PRINT("info", ("Missing protocol token in scramble data."));
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      memcpy(ctx->sha2_auth.scramble_pkt, pkt,
             sizeof(ctx->sha2_auth.scramble_pkt));
      if (connection_is_secure)
        ctx->client_auth_plugin_state =
            client_auth_sha256_password_plugin_status::
                SHA256_SEND_PLAIN_PASSWORD;
      else
        ctx->client_auth_plugin_state =
            client_auth_sha256_password_plugin_status::
                SHA256_REQUEST_PUBLIC_KEY;
      return NET_ASYNC_NOT_READY;
    case client_auth_sha256_password_plugin_status::SHA256_REQUEST_PUBLIC_KEY:
      ctx->sha2_auth.public_key = rsa_init(mysql);
      /* If no public key; request one from the server. */
      if (ctx->sha2_auth.public_key == nullptr) {
        status = vio->write_packet_nonblocking(
            vio, (const unsigned char *)&request_public_key, 1, &io_result);
        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }
        if (io_result) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
      }
      ctx->client_auth_plugin_state =
          client_auth_sha256_password_plugin_status::SHA256_READ_PUBLIC_KEY;
      [[fallthrough]];
    case client_auth_sha256_password_plugin_status::SHA256_READ_PUBLIC_KEY:
      if (ctx->sha2_auth.public_key == nullptr) {
        if (read_public_key_nonblocking(vio, ctx, result,
                                        got_public_key_from_server, status))
          return status;
      }
      if (ctx->sha2_auth.public_key) {
        char passwd_scramble[PASSWORD_SCRAMBLE_LENGTH];
        if (process_public_key_and_prepare_scramble_nonblocking(
                ctx, passwd_scramble, sizeof(passwd_scramble), mysql->passwd,
                passwd_len))
          goto err;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        {
          size_t encrypted_password_len =
              sizeof(ctx->sha2_auth.encrypted_password);
          if (encrypt_RSA_public_key(
                  (unsigned char *)passwd_scramble, passwd_len,
                  ctx->sha2_auth.encrypted_password, &encrypted_password_len,
                  ctx->sha2_auth.public_key))
            goto err;
          if (got_public_key_from_server)
            EVP_PKEY_free(ctx->sha2_auth.public_key);
        }
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        if (encrypt_RSA_public_key((unsigned char *)passwd_scramble, passwd_len,
                                   ctx->sha2_auth.encrypted_password,
                                   ctx->sha2_auth.public_key))
          goto err;
        if (got_public_key_from_server) RSA_free(ctx->sha2_auth.public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      } else {
        set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                 ER_CLIENT(CR_AUTH_PLUGIN_ERR),
                                 "sha256_password",
                                 "Authentication requires SSL encryption");
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      ctx->client_auth_plugin_state =
          client_auth_sha256_password_plugin_status::
              SHA256_SEND_ENCRYPTED_PASSWORD;
      [[fallthrough]];
    case client_auth_sha256_password_plugin_status::
        SHA256_SEND_ENCRYPTED_PASSWORD: {
      if (uses_password) {
        status = vio->write_packet_nonblocking(
            vio, (uchar *)ctx->sha2_auth.encrypted_password,
            ctx->sha2_auth.cipher_length, &io_result);
      } else {
        /* We're not using a password */
        static const unsigned char zero_byte = '\0';
        status = vio->write_packet_nonblocking(vio, &zero_byte, 1, &io_result);
      }
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result < 0) {
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
    } break;
    case client_auth_sha256_password_plugin_status::
        SHA256_SEND_PLAIN_PASSWORD: {
      status = vio->write_packet_nonblocking(vio, (uchar *)mysql->passwd,
                                             passwd_len, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result < 0) {
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
    } break;
    default:
      assert(0);
  }
  *result = CR_OK;
  return NET_ASYNC_COMPLETE;

err:
  if (got_public_key_from_server) free_rsa_key(ctx);
  result = CR_ERROR;
  return NET_ASYNC_COMPLETE;
}
/* caching_sha2_password */

int caching_sha2_password_init(char *, size_t, int, va_list) { return 0; }

int caching_sha2_password_deinit(void) { return 0; }

static bool is_secure_transport(MYSQL *mysql) {
  if (!mysql || !mysql->net.vio) return false;
  switch (mysql->net.vio->type) {
    case VIO_TYPE_SSL: {
      if (mysql_get_ssl_cipher(mysql) == nullptr) return false;
    }
      [[fallthrough]];
    case VIO_TYPE_SHARED_MEMORY:
      [[fallthrough]];
    case VIO_TYPE_SOCKET:
      return true;
    default:
      return false;
  }
  return false;
}

static char request_public_key = '\2';
static char fast_auth_success = '\3';
static char perform_full_authentication = '\4';

/**
  Authenticate the client using the RSA or TLS and a SHA2 salted password.

  @param vio Provides plugin access to communication channel
  @param mysql Client connection handler

  @return Error status
    @retval CR_ERROR An error occurred.
    @retval CR_OK Authentication succeeded.
*/
int caching_sha2_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  bool uses_password = mysql->passwd[0] != 0;
  unsigned char encrypted_password[MAX_CIPHER_LENGTH];
  // static char request_public_key= '\1';
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY *public_key = nullptr;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  RSA *public_key = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  bool got_public_key_from_server = false;
  bool connection_is_secure = false;
  unsigned char scramble_pkt[SCRAMBLE_LENGTH]{};
  unsigned char *pkt;

  DBUG_TRACE;

  /*
    Get the scramble from the server because we need it when sending encrypted
    password.
  */
  if (vio->read_packet(vio, &pkt) != SCRAMBLE_LENGTH + 1) {
    DBUG_PRINT("info", ("Scramble is not of correct length."));
    return CR_ERROR;
  }
  if (pkt[SCRAMBLE_LENGTH] != '\0') {
    DBUG_PRINT("info", ("Missing protocol token in scramble data."));
    return CR_ERROR;
  }

  /*
    Copy the scramble to the stack or it will be lost on the next use of the
    net buffer.
  */
  memcpy(scramble_pkt, pkt, SCRAMBLE_LENGTH);

  connection_is_secure = is_secure_transport(mysql);

  if (!uses_password) {
    /* We're not using a password */
    static const unsigned char zero_byte = '\0';
    if (vio->write_packet(vio, &zero_byte, 1)) return CR_ERROR;
    return CR_OK;
  } else {
    /* Password is a 0-terminated byte array ('\0' character included) */
    unsigned int passwd_len =
        static_cast<unsigned int>(strlen(mysql->passwd) + 1);
    int pkt_len = 0;
    {
      /* First try with SHA2 scramble */
      unsigned char sha2_scramble[SHA2_SCRAMBLE_LENGTH];
      if (generate_sha256_scramble(sha2_scramble, SHA2_SCRAMBLE_LENGTH,
                                   mysql->passwd, passwd_len - 1,
                                   (char *)scramble_pkt, SCRAMBLE_LENGTH)) {
        set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                 ER_CLIENT(CR_AUTH_PLUGIN_ERR),
                                 "caching_sha2_password",
                                 "Failed to generate scramble");
        return CR_ERROR;
      }

      if (vio->write_packet(vio, sha2_scramble, SHA2_SCRAMBLE_LENGTH))
        return CR_ERROR;

      if ((pkt_len = vio->read_packet(vio, &pkt)) == -1) return CR_ERROR;

      if (pkt_len == 1 && *pkt == fast_auth_success) return CR_OK;

      /* An OK packet would follow */
    }

    if (pkt_len != 1 || *pkt != perform_full_authentication) {
      DBUG_PRINT("info", ("Unexpected reply from server."));
      return CR_ERROR;
    }

    /* If connection isn't secure attempt to get the RSA public key file */
    if (!connection_is_secure) {
      public_key = rsa_init(mysql);

      if (public_key == nullptr && mysql->options.extension &&
          mysql->options.extension->get_server_public_key) {
        // If no public key; request one from the server.
        if (vio->write_packet(vio, (const unsigned char *)&request_public_key,
                              1))
          return CR_ERROR;

        if ((pkt_len = vio->read_packet(vio, &pkt)) <= 0) return CR_ERROR;
        BIO *bio = BIO_new_mem_buf(pkt, pkt_len);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        public_key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        public_key = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        BIO_free(bio);
        if (public_key == nullptr) {
          ERR_clear_error();
          DBUG_PRINT("info", ("Failed to parse public key"));
          return CR_ERROR;
        }
        got_public_key_from_server = true;
      }

      if (public_key) {
        /*
           An arbitrary limitation based on the assumption that passwords
           larger than e.g. 15 symbols don't contribute to security.
           Note also that it's further restricted to RSA_size() - 11 down
           below, so this leaves 471 bytes of possible RSA key sizes which
           should be reasonably future-proof.
           We avoid heap allocation for speed reasons.
         */
        char passwd_scramble[PASSWORD_SCRAMBLE_LENGTH];

        if (passwd_len > sizeof(passwd_scramble)) {
          /* password too long for the buffer */
          DBUG_PRINT("info", ("Password is too long."));
          goto err;
        }
        memmove(passwd_scramble, mysql->passwd, passwd_len);

        /* Obfuscate the plain text password with the session scramble */
        xor_string(passwd_scramble, passwd_len - 1, (char *)scramble_pkt,
                   SCRAMBLE_LENGTH);
        /* Encrypt the password and send it to the server */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        int cipher_length = EVP_PKEY_get_size(public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        int cipher_length = RSA_size(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        /*
           When using RSA_PKCS1_OAEP_PADDING the password length must be less
           than RSA_size(rsa) - 41.
         */
        if (passwd_len + 41 >= (unsigned)cipher_length) {
          /* password message is to long */
          DBUG_PRINT("info", ("Password is too long to be encrypted using "
                              "given public key."));
          goto err;
        }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        {
          size_t encrypted_password_len = sizeof(encrypted_password);
          if (encrypt_RSA_public_key((unsigned char *)passwd_scramble,
                                     passwd_len, encrypted_password,
                                     &encrypted_password_len, public_key))
            goto err;

          if (got_public_key_from_server) EVP_PKEY_free(public_key);
        }
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        if (encrypt_RSA_public_key((unsigned char *)passwd_scramble, passwd_len,
                                   encrypted_password, public_key))
          goto err;

        if (got_public_key_from_server) RSA_free(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

        if (vio->write_packet(vio, (uchar *)encrypted_password, cipher_length))
          return CR_ERROR;
      } else {
        set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                 ER_CLIENT(CR_AUTH_PLUGIN_ERR),
                                 "caching_sha2_password",
                                 "Authentication requires secure connection.");
        return CR_ERROR;
      }
    } else {
      /* The vio is encrypted already; just send the plain text passwd */
      if (vio->write_packet(vio, (uchar *)mysql->passwd, passwd_len))
        return CR_ERROR;
    }
  }

  return CR_OK;

err:
  if (got_public_key_from_server)
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY_free(public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
    RSA_free(public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  return CR_ERROR;
}

/**
  non blocking version of caching_sha2_password_auth_client
*/
net_async_status caching_sha2_password_auth_client_nonblocking(
    MYSQL_PLUGIN_VIO *vio, MYSQL *mysql, int *result) {
  DBUG_TRACE;
  bool uses_password = mysql->passwd[0] != 0;
  int io_result;
  net_async_status status = NET_ASYNC_NOT_READY;
  bool connection_is_secure = is_secure_transport(mysql);
  bool got_public_key_from_server = false;
  unsigned int passwd_len =
      static_cast<unsigned int>(strlen(mysql->passwd) + 1);
  unsigned char *pkt;
  mysql_async_auth *ctx = ASYNC_DATA(mysql)->connect_context->auth_context;

  switch (static_cast<client_auth_caching_sha2_password_plugin_status>(
      ctx->client_auth_plugin_state)) {
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_READING_PASSWORD:
      /*
        Get the scramble from the server because we need it when sending
        encrypted password.
      */
      status = vio->read_packet_nonblocking(vio, &pkt, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result != SCRAMBLE_LENGTH + 1) {
        DBUG_PRINT("info", ("Scramble is not of correct length."));
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      if (pkt[SCRAMBLE_LENGTH] != '\0') {
        DBUG_PRINT("info", ("Missing protocol token in scramble data."));
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      /*
        Copy the scramble to the stack or it will be lost on the next use
        of the net buffer.
      */
      memcpy(ctx->sha2_auth.scramble_pkt, pkt,
             sizeof(ctx->sha2_auth.scramble_pkt));
      ctx->client_auth_plugin_state =
          client_auth_caching_sha2_password_plugin_status::
              CACHING_SHA2_WRITING_RESPONSE;
      [[fallthrough]];
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_WRITING_RESPONSE:
      if (!uses_password) {
        /* We're not using a password */
        static const unsigned char zero_byte = '\0';
        status = vio->write_packet_nonblocking(vio, &zero_byte, 1, &io_result);
        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }
        if (io_result) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
        *result = CR_OK;
        return NET_ASYNC_COMPLETE;
      } else {
        /* First try with SHA2 scramble */
        unsigned char sha2_scramble[SHA2_SCRAMBLE_LENGTH];
        if (generate_sha256_scramble(sha2_scramble, SHA2_SCRAMBLE_LENGTH,
                                     mysql->passwd, passwd_len - 1,
                                     (char *)ctx->sha2_auth.scramble_pkt,
                                     sizeof(ctx->sha2_auth.scramble_pkt))) {
          set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                   ER_CLIENT(CR_AUTH_PLUGIN_ERR),
                                   "caching_sha2_password",
                                   "Failed to generate scramble");
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
        status = vio->write_packet_nonblocking(
            vio, sha2_scramble, SHA2_SCRAMBLE_LENGTH, &io_result);
        if (status == NET_ASYNC_NOT_READY) {
          return NET_ASYNC_NOT_READY;
        }
        if (io_result) {
          *result = CR_ERROR;
          return NET_ASYNC_COMPLETE;
        }
      }
      ctx->client_auth_plugin_state =
          client_auth_caching_sha2_password_plugin_status::
              CACHING_SHA2_CHALLENGE_RESPONSE;
      [[fallthrough]];
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_CHALLENGE_RESPONSE:
      status = vio->read_packet_nonblocking(vio, &pkt, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result == -1) {
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      if (io_result == 1 && *pkt == fast_auth_success) {
        *result = CR_OK;
        return NET_ASYNC_COMPLETE;
      }
      if (io_result != 1 || *pkt != perform_full_authentication) {
        DBUG_PRINT("info", ("Unexpected reply from server."));
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
      if (!connection_is_secure)
        ctx->client_auth_plugin_state =
            client_auth_caching_sha2_password_plugin_status::
                CACHING_SHA2_REQUEST_PUBLIC_KEY;
      else
        ctx->client_auth_plugin_state =
            client_auth_caching_sha2_password_plugin_status::
                CACHING_SHA2_SEND_PLAIN_PASSWORD;
      return NET_ASYNC_NOT_READY;
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_REQUEST_PUBLIC_KEY:
      /* If connection isn't secure attempt to get the RSA public key file */
      {
        ctx->sha2_auth.public_key = rsa_init(mysql);

        if (ctx->sha2_auth.public_key == nullptr && mysql->options.extension &&
            mysql->options.extension->get_server_public_key) {
          status = vio->write_packet_nonblocking(
              vio, (const unsigned char *)&request_public_key, 1, &io_result);
          if (status == NET_ASYNC_NOT_READY) {
            return NET_ASYNC_NOT_READY;
          }
          if (io_result) {
            *result = CR_ERROR;
            return NET_ASYNC_COMPLETE;
          }
        }
      }
      ctx->client_auth_plugin_state =
          client_auth_caching_sha2_password_plugin_status::
              CACHING_SHA2_READ_PUBLIC_KEY;
      [[fallthrough]];
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_READ_PUBLIC_KEY: {
      if (ctx->sha2_auth.public_key == nullptr && mysql->options.extension &&
          mysql->options.extension->get_server_public_key) {
        if (read_public_key_nonblocking(vio, ctx, result,
                                        got_public_key_from_server, status))
          return status;
      }
      if (ctx->sha2_auth.public_key) {
        char passwd_scramble[PASSWORD_SCRAMBLE_LENGTH];
        if (process_public_key_and_prepare_scramble_nonblocking(
                ctx, passwd_scramble, sizeof(passwd_scramble), mysql->passwd,
                passwd_len))
          goto err;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        {
          size_t encrypted_password_len =
              sizeof(ctx->sha2_auth.encrypted_password);
          EVP_PKEY_CTX *key_ctx =
              EVP_PKEY_CTX_new(ctx->sha2_auth.public_key, nullptr);
          if (!key_ctx) goto err;
          if (EVP_PKEY_encrypt_init(key_ctx) <= 0 ||
              EVP_PKEY_CTX_set_rsa_padding(key_ctx, RSA_PKCS1_OAEP_PADDING) <=
                  0 ||
              EVP_PKEY_encrypt(key_ctx, ctx->sha2_auth.encrypted_password,
                               &encrypted_password_len,
                               (unsigned char *)passwd_scramble,
                               passwd_len) <= 0) {
            EVP_PKEY_CTX_free(key_ctx);
            goto err;
          }
          EVP_PKEY_CTX_free(key_ctx);
          if (got_public_key_from_server)
            EVP_PKEY_free(ctx->sha2_auth.public_key);
        }
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
        RSA_public_encrypt(passwd_len, (unsigned char *)passwd_scramble,
                           ctx->sha2_auth.encrypted_password,
                           ctx->sha2_auth.public_key, RSA_PKCS1_OAEP_PADDING);
        if (got_public_key_from_server) RSA_free(ctx->sha2_auth.public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
      } else {
        set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                 ER_CLIENT(CR_AUTH_PLUGIN_ERR),
                                 "caching_sha2_password",
                                 "Authentication requires secure connection.");
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
    }
      ctx->client_auth_plugin_state =
          client_auth_caching_sha2_password_plugin_status::
              CACHING_SHA2_SEND_ENCRYPTED_PASSWORD;
      [[fallthrough]];
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_SEND_ENCRYPTED_PASSWORD: {
      status = vio->write_packet_nonblocking(
          vio, (uchar *)ctx->sha2_auth.encrypted_password,
          ctx->sha2_auth.cipher_length, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result < 0) {
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
    } break;
    case client_auth_caching_sha2_password_plugin_status::
        CACHING_SHA2_SEND_PLAIN_PASSWORD: {
      status = vio->write_packet_nonblocking(vio, (uchar *)mysql->passwd,
                                             passwd_len, &io_result);
      if (status == NET_ASYNC_NOT_READY) {
        return NET_ASYNC_NOT_READY;
      }
      if (io_result < 0) {
        *result = CR_ERROR;
        return NET_ASYNC_COMPLETE;
      }
    } break;
    default:
      assert(0);
  }
  *result = CR_OK;
  return NET_ASYNC_COMPLETE;

err:
  if (got_public_key_from_server) free_rsa_key(ctx);
  result = CR_ERROR;
  return NET_ASYNC_COMPLETE;
}

void STDCALL mysql_reset_server_public_key(void) {
  DBUG_TRACE;
  mysql_mutex_lock(&g_public_key_mutex);
  if (g_public_key)
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY_free(g_public_key);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
    RSA_free(g_public_key);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  g_public_key = nullptr;
  mysql_mutex_unlock(&g_public_key_mutex);
}
