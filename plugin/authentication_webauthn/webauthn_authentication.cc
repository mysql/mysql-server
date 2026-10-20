/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "webauthn_authentication.h"
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_authentication_registration.h>
#include <mysql/psi/mysql_memory.h>
#include <openssl/rand.h>  // RAND_bytes

#include "base64.h" /* base64_decode */
#include "common.h"
#include "scope_guard.h"

#include "mysql/my_loglevel.h"
#include "mysql/plugin_auth.h"
#include "mysqld_error.h"

#include "option_usage.h"
#include "webauthn_assert_verification.h"
#include "webauthn_attestation.h"

#define MAX_KEY_HANDLE_LENGTH 89
#define MAX_SIGNED_CHALLENGE_LENGTH 255
#define MAX_RAW_MESSAGE_LENGTH 1024

char *authentication_webauthn_rp_id = nullptr;
constexpr int signature_algorithm = COSE_ES256;

PSI_memory_key key_memory_webauthn_authentication;
PSI_memory_key key_memory_webauthn_attestation;

EVP_MD *md_sha256 = nullptr;

namespace {
bool initialized = false;
}  // namespace

static PSI_memory_info all_webauthn_memory[] = {
    {&key_memory_webauthn_authentication, "webauthn_authentication", 0, 0,
     "Memory allocated during webauthn authentication."},
    {&key_memory_webauthn_attestation, "webauthn_attestation", 0, 0,
     "Memory allocated during webauthn attestation."}};

const char *my_authentication_registration_service_name{
    "mysql_authentication_registration.authentication_webauthn"};

SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
SERVICE_TYPE(registry_registration) *reg_reg = nullptr;
my_h_service h_reg_svc = nullptr;

/* COMPONENT SERVICES */

/**
  This method performs initiate registration step.

  During ALTER USER .. INITIATE REGISTRATION
  Initiate plugin capability flag.
  Generate random challenge which consists of 32 byte salt, relying party
  ID. Format of challenge is:
  <1 byte capability flag><length encoded random salt>
  <length encoded relying party ID>

  @param [out] buf           Buffer to hold challenge
  @param [in] buflen         Length of buffer

  @return init registration status
  @retval false success
  @retval true failure
*/
static DEFINE_BOOL_METHOD(init, (unsigned char **buf, unsigned int buflen)) {
  DBUG_TRACE;
  unsigned char salt[CHALLENGE_LENGTH + 1] = {0};

  /* The fido2 specification expects challenge to be 32 bytes */
  RAND_bytes((unsigned char *)&salt[0], CHALLENGE_LENGTH);
  unsigned char *pos = *buf;
  unsigned short capability = RESIDENT_KEYS;

  memcpy(pos, reinterpret_cast<char *>(&capability), sizeof(char));
  pos++;
  pos = net_store_length(pos, CHALLENGE_LENGTH);
  memcpy(pos, salt, CHALLENGE_LENGTH);
  pos += CHALLENGE_LENGTH;

  pos = net_store_length(pos, strlen(authentication_webauthn_rp_id));
  memcpy(pos, authentication_webauthn_rp_id,
         strlen(authentication_webauthn_rp_id));
  pos += strlen(authentication_webauthn_rp_id);

  if (buflen != (unsigned int)(pos - *buf)) return true;
  ++opt_option_tracker_usage_authentication_webauthn_plugin;
  return false;
}

/**
  This method performs finish registration step.

  During ALTER USER .. FINISH REGISTRATION
  Verify the signature present in buf against the challenge present in
  auth_str. After successfull verification the token device will generate
  key handle (credential ID and public key) which is saved in
  challenge_response. Credential ID is saved only for non fido2 devices.

  @param [in] buf            Buffer to hold signed challenge
  @param [in] buflen         Length of signed challenge
  @param [in]  challenge       Buffer to hold server challenge generated
  during initiate registration
  challenge format is:
    [1 byte capability flag] [length encoded random salt]
    [length encoded rp id] [length encoded user name]
  @param [in]  challenge_length   Length of server challenge.
  @param [out] challenge_response       Buffer to hold challenge response
  challenge_response format is:
    [1 byte capability] [length encoded authenticator data]
    [length encoded signature] [length encoded x509 certificate]
  @param [out] challenge_response_length    Length of challenge response

  @return registration status
  @retval false success
  @retval true failure
*/
static DEFINE_BOOL_METHOD(finish, (unsigned char *buf, unsigned int buflen,
                                   const unsigned char *challenge,
                                   unsigned int challenge_length,
                                   unsigned char *challenge_response,
                                   unsigned int *challenge_response_length)) {
  DBUG_TRACE;
  auto wa = std::make_unique<webauthn_attestation>(signature_algorithm);
  if (!wa) return true;
  wa->prepare_credentials(buf, buflen, challenge, challenge_length);
  if (wa->verify_signature()) {
    LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_REGISTRATION_FAILED);
    return true;
  }
  if (wa->get_key_handle(&challenge_response, challenge_response_length)) {
    assert(false);
    return true;
  }
  ++opt_option_tracker_usage_authentication_webauthn_plugin;
  return false;
}

/**
  Get method to get length of challenge.

  @param [out] buflen         Length of buffer
*/
static DEFINE_METHOD(void, get_challenge_length, (unsigned int *buflen)) {
  DBUG_TRACE;
  /* calculate total buffer length */
  int capability = 1;
  *buflen = net_length_size(strlen(authentication_webauthn_rp_id)) +
            net_length_size(CHALLENGE_LENGTH) + CHALLENGE_LENGTH +
            strlen(authentication_webauthn_rp_id) + capability;
}

BEGIN_SERVICE_IMPLEMENTATION(authentication_webauthn,
                             mysql_authentication_registration)
init, finish, get_challenge_length, END_SERVICE_IMPLEMENTATION();

static bool register_services() {
  if (reg_srv->acquire("registry_registration", &h_reg_svc) || !h_reg_svc)
    return true;

  reg_reg = reinterpret_cast<SERVICE_TYPE(registry_registration) *>(h_reg_svc);

  using webauthn_authentication_t =
      SERVICE_TYPE_NO_CONST(mysql_authentication_registration);
  return reg_reg->register_service(
      my_authentication_registration_service_name,
      reinterpret_cast<my_h_service>(
          const_cast<webauthn_authentication_t *>(&SERVICE_IMPLEMENTATION(
              authentication_webauthn, mysql_authentication_registration))));
}

static bool unregister_services() {
  reg_reg = reinterpret_cast<SERVICE_TYPE(registry_registration) *>(h_reg_svc);
  reg_reg->unregister(my_authentication_registration_service_name);
  if (h_reg_svc) reg_srv->release(h_reg_svc);
  return false;
}

/**
  Initialize authentication_webauthn plugin
*/
int webauthn_authentication_init(MYSQL_PLUGIN plugin_ref [[maybe_unused]]) {
  DBUG_TRACE;
  initialized = false;
  if (!authentication_webauthn_rp_id || !*authentication_webauthn_rp_id)
    return 1;

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) {
    LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_LOG);
    return 1;
  }
  const char *category = "webauthn_authentication";
  int count = static_cast<int>(array_elements(all_webauthn_memory));
  mysql_memory_register(category, all_webauthn_memory, count);

  register_services();
  if (authentication_webauthn_plugin_option_usage_init()) return 1;
  fido_init(0);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  md_sha256 = EVP_MD_fetch(nullptr, "sha256", nullptr);
#endif
  initialized = true;
  return 0;
}

/**
  Deinitialize authentication_webauthn plugin

  @param [in] arg Unused

  @returns Always returns success
*/
int webauthn_authentication_deinit(void *arg [[maybe_unused]]) {
  DBUG_TRACE;
  if (initialized) {
    if (authentication_webauthn_plugin_option_usage_deinit()) return 1;
    unregister_services();
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_free(md_sha256);
#endif
  }
  initialized = false;
  return 0;
}

/**
  Authentication method for authentication_webauthn plugin.

  @param [in] vio  Virtual I/O interface
  @param [in] info Connection information

  Below is the handshake process:
  1. Server side plugin: initiate authentication by sending a challenge.
     Challenge consists of 1 byte capability flag, 32 bytes random salt,
     relying party name. Send credential ID if client requests it.
  2. Client side plugin: read the server challenge, let the device sign
     the challenge. Device rejects authentication in case of invalid rp ID
     or salt not being 32 bytes in length. Client side plugin can request
     for credential ID if token is not CTAP2.1 protocol enabled.
     Signed challenge is sent to server. Signed challenge consists of 1 byte
     packet tag, authenticator data, a signature and clientdataJSON object.
  3. Server side plugin: read the signed challenge and pass it to FIDO
     device along with public key for the device to verify the signed challenge.
     On successful verification of signed challenge return CR_OK to server
     else return CR_ERROR.

  @return authentication status
  @retval CR_OK    Successful authentication
  @retval CR_ERROR or CR_AUTH_USER_CREDENTIALS Authentication failure
*/
static int webauthn_authenticate(MYSQL_PLUGIN_VIO *vio,
                                 MYSQL_SERVER_AUTH_INFO *info) {
  DBUG_TRACE;

  if (info->multi_factor_auth_info[info->current_auth_factor]
          .is_registration_required) {
    /* send an empty packet to inidicate registration is required */
    if (vio->write_packet(vio, nullptr, 0)) return CR_AUTH_HANDSHAKE;
    /*
      registration step is pending, thus for now allow connection, for
      user to perform registration step.
    */
    return CR_OK_AUTH_IN_SANDBOX_MODE;
  }
  DBUG_EXECUTE_IF("simulate_fido_testing", { return CR_OK; });
  /* auth string is encoded in base64 format which is decoded back. */
  unsigned char *tmp_value = nullptr;
  const char *end_ptr;
  const char *auth_str = info->auth_string;
  unsigned char *pk = nullptr;
  unsigned long auth_str_len = info->auth_string_length;

  auto cleanup = create_scope_guard([&] {
    my_free(tmp_value);
    if (pk) my_free(pk);
  });

  /* decode auth_string */
  int64 length = base64_needed_decoded_length((uint64)auth_str_len);
  tmp_value = (unsigned char *)my_malloc(key_memory_webauthn_authentication,
                                         length, MYF(MY_WME));
  if (!tmp_value) {
    LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_OOM);
    return CR_ERROR;
  }

  length =
      base64_decode(auth_str, (uint64)auth_str_len, tmp_value, &end_ptr, 0);
  if (length < 0) return CR_ERROR;

  /* extract public key from auth string */
  auto *auth_string_ptr = reinterpret_cast<unsigned char *>(tmp_value);
  if (!auth_string_ptr) return CR_ERROR;
  size_t pk_len = net_field_length_ll(&auth_string_ptr);
  pk = (uchar *)my_malloc(key_memory_webauthn_authentication, pk_len,
                          MYF(MY_WME));
  memcpy(pk, auth_string_ptr, pk_len);
  auth_string_ptr += pk_len;

  unsigned char challenge[MAX_RAW_MESSAGE_LENGTH];
  unsigned char *pos = challenge;
  unsigned char salt[CHALLENGE_LENGTH + 1] = {0};
  /* prepare server challenge */
  RAND_bytes((unsigned char *)&salt[0], CHALLENGE_LENGTH);

  unsigned short capability = RESIDENT_KEYS;
  *pos = *reinterpret_cast<unsigned char *>(&capability);
  pos++;
  /* copy salt length */
  pos = net_store_length(pos, CHALLENGE_LENGTH);
  memcpy(pos, salt, CHALLENGE_LENGTH);
  pos += CHALLENGE_LENGTH;
  /* copy relying party ID */
  size_t rp_id_len = strlen(authentication_webauthn_rp_id);
  pos = net_store_length(pos, rp_id_len);
  DBUG_EXECUTE_IF("webauthn_short_assertion_rp_id", { rp_id_len = 0; });
  memcpy(pos, authentication_webauthn_rp_id, rp_id_len);
  pos += rp_id_len;
  DBUG_EXECUTE_IF("webauthn_short_assertion_challenge",
                  { pos = challenge + 3; });

  /* send server challenge having capability, random salt and rp_id */
  if (vio->write_packet(vio, challenge, (int)(pos - challenge))) {
    return CR_AUTH_HANDSHAKE;
  }

  unsigned char *challenge_response = nullptr;
  int challenge_response_len = 0;
  /*
    Client can send:
    1. Request credential ID with packet identifier set to 0x01.
    2. Packet containing authenticator data, signature, clientdataJSON
       with packet identifier set to 0x02.
  */
  challenge_response_len = vio->read_packet(vio, &challenge_response);
  /** something wrong with the client */
  if (challenge_response_len <= 0) {
    return CR_ERROR;
  }

  if (*challenge_response == '\1') {
    memset(challenge, 0, MAX_RAW_MESSAGE_LENGTH);
    pos = challenge;
    /* client requested credential ID */
    if ((auth_string_ptr - tmp_value) < length) {
      if (!auth_string_ptr) return CR_ERROR;
      /* cred ID is present in auth string */
      size_t cred_id_len = net_field_length_ll(&auth_string_ptr);
      pos = net_store_length(pos, cred_id_len);
      memcpy(pos, auth_string_ptr, cred_id_len);
      auth_string_ptr += cred_id_len;
      pos += cred_id_len;
      /* send credential ID */
      if (vio->write_packet(vio, challenge, (int)(pos - challenge))) {
        return CR_AUTH_HANDSHAKE;
      }
    } else {
      /*
        authentication_string does not have credential ID.
        So, send an empty packet
      */
      if (vio->write_packet(vio, nullptr, 0)) {
        return CR_AUTH_HANDSHAKE;
      }
    }
    challenge_response_len = vio->read_packet(vio, &challenge_response);
    /** something wrong with the client */
    if (challenge_response_len <= 0) {
      return CR_ERROR;
    }
  }

  uint lenc_size = 0;
  auto max = static_cast<uint64_t>(challenge_response_len);
  auto wav = std::make_unique<webauthn_assert_verification>(salt);
  if (wav) {
    /* packet identifier must be 0x02 */
    if (*challenge_response != '\2') return CR_ERROR;
    unsigned char *lpos = challenge_response;
    lpos++;
    max--;
    if (!lpos) return CR_ERROR;
    lenc_size = net_field_length_size(lpos);
    if (lenc_size > max) return CR_ERROR;
    size_t num_assertions = net_field_length_ll(&lpos);
    wav->set_assertion_count(num_assertions);
    max -= lenc_size;

    for (size_t num = 0; num < num_assertions; ++num) {
      if (!lpos) return CR_ERROR;
      lenc_size = net_field_length_size(lpos);
      if (lenc_size > max) return CR_ERROR;
      /* extract authentication data from signed challenge */
      auto llength = net_field_length_ll(&lpos);
      max -= lenc_size;
      if (llength > MAX_SIGNED_CHALLENGE_LENGTH || llength > max) {
        LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_AUTH_DATA_CORRUPT);
        return CR_AUTH_USER_CREDENTIALS;
      }
      if (wav->set_authdata(lpos, llength, num)) return true;
      lpos += llength;
      max -= llength;
      if (!lpos) return CR_ERROR;
      lenc_size = net_field_length_size(lpos);
      if (lenc_size > max) return CR_ERROR;
      /* extract signature from signed challenge */
      llength = net_field_length_ll(&lpos);
      max -= lenc_size;
      if (llength > MAX_SIGNED_CHALLENGE_LENGTH || llength > max) {
        LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_SIGNATURE_CORRUPT);
        return CR_AUTH_USER_CREDENTIALS;
      }
      if (wav->set_signature(lpos, llength, num)) return true;
      lpos += llength;
      max -= llength;
    }
    /* set relying party ID */
    if (wav->set_rp_id(authentication_webauthn_rp_id)) return true;
    if (!lpos) return CR_ERROR;
    lenc_size = net_field_length_size(lpos);
    if (lenc_size > max) return CR_ERROR;
    /* extract client data hash */
    auto length2 = net_field_length_ll(&lpos);
    max -= lenc_size;

    if (length2 > max) {
      LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_AUTH_DATA_CORRUPT);
      return CR_AUTH_USER_CREDENTIALS;
    }

    wav->set_client_data(reinterpret_cast<const unsigned char *>(lpos),
                         length2);
    /* verfify signed challenge */
    if (wav->verify_signature(signature_algorithm, pk)) {
      LogPluginErr(ERROR_LEVEL,
                   ER_AUTHENTICATION_PLUGIN_VERIFY_SIGNATURE_FAILED);
      return CR_AUTH_USER_CREDENTIALS;
    }
  } else
    return CR_ERROR;
  ++opt_option_tracker_usage_authentication_webauthn_plugin;
  return CR_OK;
}

/**
  Password generator API for authentication_webauthn plugin. This API encodes
  auth string specified by user, to base64 before storing in mysql.user table.

  @param[out]   auth_str_hash      A buffer provided by server which will hold
  the authentication string generated by plugin.
  @param[in,out] auth_str_hash_len  Length of server provided buffer as IN param
  and length of plugin generated string as OUT param.
  @param[in]    auth_str       auth string provided by user.
  @param[in]    auth_str_len    auth string length.

  @returns 0 success, 1 failure
*/
static int webauthn_password_generate(char *auth_str_hash,
                                      unsigned int *auth_str_hash_len,
                                      const char *auth_str,
                                      unsigned int auth_str_len) {
  DBUG_TRACE;

  ++opt_option_tracker_usage_authentication_webauthn_plugin;
  if (auth_str_len > MAX_RAW_MESSAGE_LENGTH) return 1;
  if (auth_str_len == 0) {
    *auth_str_hash_len = 0;
    return 0;
  }
  /*
   Encode auth string to base64 format, as this will be stored in
   json format which requires binary data to be in base64 format
  */
  uint64 needed = base64_needed_encoded_length((uint64)auth_str_len);
  base64_encode(auth_str, auth_str_len, auth_str_hash);
  /* drop null terminator */
  *auth_str_hash_len = needed - 1;
  return 0;
}

/**
  Password valiadtor API for authentication_webauthn plugin

  @param[in] auth_str   Buffer containing authentication string
  @param[in] len        Length of authentication string

  @returns 0 success, 1 failure
*/
static int webauthn_password_validate(char *const auth_str, unsigned int len) {
  DBUG_TRACE;
  if (!len) return 1;
  /*
    fido auth string aka key handle comprises of public key (65 bytes) and
    optional credential info (variable length). This auth string is in base64
    format. Considering only public key part minimum required length will be 89:
    select char_length(to_base64(repeat('1', 65))); gives 89
  */
  if (len < MAX_KEY_HANDLE_LENGTH) return 1;
  unsigned char *tmp_value = nullptr;
  const char *end_ptr;
  auto cleanup = create_scope_guard([&] { my_free(tmp_value); });
  /* decode auth_string */
  int64 length = base64_needed_decoded_length((uint64)len);
  tmp_value = (unsigned char *)my_malloc(key_memory_webauthn_authentication,
                                         length, MYF(MY_WME));
  if (!tmp_value) {
    LogPluginErr(ERROR_LEVEL, ER_AUTHENTICATION_PLUGIN_OOM);
    return 1;
  }
  length = base64_decode(auth_str, (uint64)len, tmp_value, &end_ptr, 0);
  if (length < 0) return 1;
  ++opt_option_tracker_usage_authentication_webauthn_plugin;
  return 0;
}

/**
  authentication_webauthn plugin API to generate random salt

  @returns Always returns success
*/
static int webauthn_password_salt(const char *, unsigned int, unsigned char *,
                                  unsigned char *) {
  DBUG_TRACE;
  return 0;
}

/** st_mysql_auth for authentication_webauthn plugin */
static struct st_mysql_auth webauthn_authentication_handler {
  MYSQL_AUTHENTICATION_INTERFACE_VERSION, "authentication_webauthn_client",
      webauthn_authenticate, webauthn_password_generate,
      webauthn_password_validate, webauthn_password_salt,
      AUTH_FLAG_REQUIRES_REGISTRATION, nullptr
};

static void update_webauthn_authentication_rp_id(THD *thd [[maybe_unused]],
                                                 SYS_VAR *var [[maybe_unused]],
                                                 void *var_ptr,
                                                 const void *save) {
  *static_cast<const char **>(var_ptr) = *static_cast<char *const *>(save);
}

static MYSQL_SYSVAR_STR(rp_id,                         /* name       */
                        authentication_webauthn_rp_id, /* value      */
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* flags */
                        "Server name that FIDO protocol uses to directly "
                        "authenticate users.",                /* comment    */
                        nullptr,                              /* check()    */
                        update_webauthn_authentication_rp_id, /* update()   */
                        ""                                    /* default    */
);

/** Array of system variables. Used in plugin declaration. */
static SYS_VAR *webauthn_authentication_sysvars[] = {MYSQL_SYSVAR(rp_id),
                                                     nullptr};

static SHOW_VAR webauthn_authentication_statusvars[] = {
    {"option_tracker_usage:WebauthN authentication",
     reinterpret_cast<char *>(
         &opt_option_tracker_usage_authentication_webauthn_plugin),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

/*
  authentication_webauthn plugin declaration
*/
mysql_declare_plugin(authentication_webauthn){
    MYSQL_AUTHENTICATION_PLUGIN,        /* plugin type                   */
    &webauthn_authentication_handler,   /* type specific descriptor      */
    "authentication_webauthn",          /* plugin name                   */
    PLUGIN_AUTHOR_ORACLE,               /* author                        */
    "webauthn authentication",          /* description                   */
    PLUGIN_LICENSE_GPL,                 /* license                       */
    webauthn_authentication_init,       /* plugin initializer            */
    nullptr,                            /* Uninstall notifier            */
    webauthn_authentication_deinit,     /* plugin deinitializer          */
    0x0100,                             /* version (1.0)                 */
    webauthn_authentication_statusvars, /* status variables              */
    webauthn_authentication_sysvars,    /* system variables              */
    nullptr,                            /* reserverd                     */
    0,                                  /* flags                         */
} mysql_declare_plugin_end;
