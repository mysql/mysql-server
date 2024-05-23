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

#include <assert.h>
#include <openssl/sha.h>

#include <common.h>

#include "webauthn_registration.h"

#include <base64.h>      /* base64_decode */
#include <my_hostname.h> /* HOSTNAME_LENGTH */
#include <mysql_com.h>   /* CHALLENGE_LENGTH */
#include <scope_guard.h> /* create_scope_guard */

#undef MYSQL_DYNAMIC_PLUGIN
#include <mysql/service_mysql_alloc.h>
#define MYSQL_DYNAMIC_PLUGIN

namespace {
const unsigned int PIN_BUFFER_SIZE = 256;
}  // namespace
/**
  Helper method to parse the challenge received from server during registration
  process. This method extracts 1 byte capability flag, salt, user name, relying
  party ID and set it in fido_cred_t.

  @param [in] challenge   buffer holding the server challenge

  @retval false success
  @retval true failure
*/
bool webauthn_registration::parse_challenge(const char *challenge) {
  /* decode received challenge from base64 */
  char *tmp_value;
  const char *end_ptr;
  int64 length = base64_needed_decoded_length((uint64)strlen(challenge));
  tmp_value = new (std::nothrow) char[length];
  if (!tmp_value) return true;
  length = base64_decode(challenge, (uint64)strlen(challenge), tmp_value,
                         &end_ptr, 0);
  if (length < 0) return true;
  unsigned char *to = reinterpret_cast<unsigned char *>(tmp_value);
  /* skip capability flag */
  to++;
  if (!to) return true;
  /* length of challenge should be 32 bytes */
  unsigned long len = net_field_length_ll(&to);
  if (len != CHALLENGE_LENGTH) return true;
  /* extract challenge */
  unsigned char salt[CHALLENGE_LENGTH + 1] = {0};
  char rp[RELYING_PARTY_ID_LENGTH + 1] = {0};

  memcpy(salt, to, CHALLENGE_LENGTH);
  to += len;
  if (!to) return true;
  /* length of relying party ID  */
  len = net_field_length_ll(&to);
  /* max length of relying party ID is 255 */
  if (len > 255) return true;
  /* extract relying party ID  */
  memcpy(rp, to, len);
  set_rp_id(rp);
  to += len;
  if (!to) return true;
  /* length of user name */
  len = net_field_length_ll(&to);
  /*
    user name includes 32 byte user name + 255 bytes hostname
    + 4 "`" + @
  */
  if (len > (USERNAME_LENGTH + HOSTNAME_LENGTH + 5)) return true;
  /* extract user name */
  char *user = new (std::nothrow) char[len + 1];
  memcpy(user, to, len);
  user[len] = 0;
  set_user(user);
  delete[] user;
  /* set client data context */
  set_client_data(salt, rp);

  delete[] tmp_value;
  return false;
}

/**
  This method will extract authenticator data, signature, certificate from
  fido_cred_t struct, construct a buffer holding this data which
  will be converted to base64 format before passing to server. Format of
  challenge response is:
  [1 byte capability]
  [length encoded authenticator data]
  [length encoded signature: not used if attestation present]
  [length encoded certificate: not used if attestation present]
  [length encoded serialized client data JSON]
  [length encoded serialized attestation statement CBOR]
  [length encoded format string]

  @param [out] challenge_response     buffer to hold challenge response

  @retval false success
  @retval true failure
*/
bool webauthn_registration::make_challenge_response(
    unsigned char *&challenge_response) {
  /* copy client response into buf */
  unsigned long authdata_len = get_authdata_len();
  unsigned long sig_len = get_sig_len();
  unsigned long cert_len = get_x5c_len();
  unsigned long client_data_json_len = get_client_data_json_len();
  unsigned short capability = 0;
  unsigned short capability_len = 1;
  unsigned long attstmt_len = get_attestation_statement_length();
  const char *fmt = get_fmt();
  unsigned long fmt_len = strlen(fmt);

  /* calculate total required buffer length */
  size_t len = capability_len + net_length_size(authdata_len) +
               net_length_size(sig_len) +
               (cert_len ? net_length_size(cert_len) + cert_len : 0) +
               authdata_len + sig_len + net_length_size(client_data_json_len) +
               client_data_json_len + attstmt_len +
               net_length_size(attstmt_len) + fmt_len +
               net_length_size(fmt_len);
  unsigned char *str = new (std::nothrow) unsigned char[len];
  if (!str) return true;
  unsigned char *pos = str;

  auto cleanup = create_scope_guard([&] {
    if (str) delete[] str;
  });
  if (is_fido2()) {
    capability |= RESIDENT_KEYS;
  }
  capability |= SEND_FULL_ATTESTATION_BLOB;

  memcpy(pos, reinterpret_cast<char *>(&capability), sizeof(char));
  pos++;
  pos = net_store_length(pos, authdata_len);
  /* copy authenticator data */
  memcpy(pos, get_authdata_ptr(), authdata_len);
  pos += authdata_len;
  pos = net_store_length(pos, sig_len);
  /* append signature */
  memcpy(pos, get_sig_ptr(), sig_len);
  pos += sig_len;
  /* append x509 certificate if present */
  if (cert_len) {
    pos = net_store_length(pos, cert_len);
    memcpy(pos, get_x5c_ptr(), cert_len);
    pos += cert_len;
  } else {
    get_plugin_messages("Registration failed. Certificate missing.",
                        message_type::ERROR);
    return true;
  }
  /* send client data JSON to server */
  pos = net_store_length(pos, client_data_json_len);
  memcpy(pos, get_client_data_json().c_str(), client_data_json_len);
  pos += client_data_json_len;

  /* send the full attestation ptr */
  pos = net_store_length(pos, attstmt_len);
  memcpy(pos, get_attestation_statement_ptr(), attstmt_len);
  pos += attstmt_len;

  /** send the fmt */
  pos = net_store_length(pos, fmt_len);
  memcpy(pos, fmt, fmt_len);
  pos += fmt_len;

  /* base64 encode the whole thing */
  assert(len == (size_t)(pos - str));
  uint64 needed = base64_needed_encoded_length((uint64)len);
  unsigned char *tmp_value = new unsigned char[needed];
  base64_encode(str, len, reinterpret_cast<char *>(tmp_value));
  /* Ensure caller will release this memory. */
  challenge_response = tmp_value;

  return false;
}

/**
  Helper method to set client data context.

  Client data format is:
  SHA256({
          "type": "webauthn.create",
          "challenge": url_safe_base64("32 byte random"),
          "origin": authentication_webauthn_rp_id,
          "crossOrigin": false
        })

  @param [in] salt   buffer holding random salt
  @param [in] rp     relying party name aka origin
*/
void webauthn_registration::set_client_data(const unsigned char *salt,
                                            const char *rp) {
  char base64_salt[BASE64_CHALLENGE_LENGTH] = {0};
  char url_compatible_salt[BASE64_CHALLENGE_LENGTH] = {0};

  /* convert salt to be base64 */
  base64_encode(salt, CHALLENGE_LENGTH, base64_salt);
  /* convert base64 encoded salt to be websafe base64 */
  url_compatible_base64(url_compatible_salt, BASE64_CHALLENGE_LENGTH,
                        base64_salt);
  unsigned char client_data_buf[512] = {0};
  /* construct client data JSON string */
  size_t client_data_len = snprintf(
      reinterpret_cast<char *>(client_data_buf), sizeof(client_data_buf),
      "{\"type\":\"webauthn.create\",\"challenge\":"
      "\"%s\",\"origin\":\"https://%s\",\"crossOrigin\":false}",
      url_compatible_salt, rp);

  fido_cred_set_clientdata(m_cred, client_data_buf, client_data_len);

  /* save clientdataJSON to be passed to server */
  m_client_data_json = reinterpret_cast<char *>(client_data_buf);
}

/**
  This method checks if a token device is available on client host.
  If device is present, device expects user to perform gesture action,
  upon which device generates credential details, which consists of
  authenticator data, signature and optional x509 certificate which is
  passed to server.

  @retval FIDO_OK(false) successful generation of credentials.
  @retval true           error occurred.
*/
bool webauthn_registration::generate_signature() {
  bool ret_code = false;
  fido_dev_info_t *dev_infos = discover_fido2_devices(libfido_device_id + 1);
  if (!dev_infos) return true;
  const fido_dev_info_t *curr = fido_dev_info_ptr(dev_infos, libfido_device_id);
  const char *path = fido_dev_info_path(curr);
  /* open the device */
  fido_dev_t *dev = fido_dev_new();
  if (fido_dev_open(dev, path) != FIDO_OK) {
    get_plugin_messages("Failed to open FIDO device.", message_type::ERROR);
    ret_code = true;
    goto end;
  } else {
    std::stringstream message;
    message << "Using device " << libfido_device_id << " Product=["
            << fido_dev_info_product_string(curr) << "] Manufacturer=["
            << fido_dev_info_manufacturer_string(curr) << "]\n";
    get_plugin_messages(message.str(), message_type::INFO);
    /* check if device supports CTAP 2.1 Credential Management */
    m_is_fido2 = fido_dev_supports_credman(dev);
    /*
      if authenticator does not support discoverable credentials, turn OFF
      resident_keys flag
    */
    if (m_is_fido2) fido_cred_set_rk(m_cred, FIDO_OPT_TRUE);

    std::string s(
        "Please insert FIDO device and follow the instruction."
        "Depending on the device, you may have to perform gesture action "
        "multiple times.");
    get_plugin_messages(s, message_type::INFO);
    s.assign(
        "1. Perform gesture action (Skip this step if you are prompted to "
        "enter device PIN).");
    get_plugin_messages(s, message_type::INFO);
    int res = fido_dev_make_cred(dev, m_cred, nullptr);
    if (res == FIDO_ERR_PIN_REQUIRED) {
      char pin[PIN_BUFFER_SIZE]{0};
      if (get_user_input("2. Enter PIN for token device: ",
                         input_type::PASSWORD, pin, &PIN_BUFFER_SIZE)) {
        get_plugin_messages("Failed to get device PIN", message_type::ERROR);
        ret_code = true;
        goto end;
      }
      s.assign("3. Perform gesture action for registration to complete.");
      get_plugin_messages(s, message_type::INFO);
      res = fido_dev_make_cred(dev, m_cred, pin);
      memset(pin, 1, sizeof(pin));
    }
    if (res != FIDO_OK) {
      get_plugin_messages(
          "Registration failed. Challenge received might be corrupt.",
          message_type::ERROR);
      ret_code = true;
      goto end;
    }
  }
end:
  fido_dev_close(dev);
  fido_dev_free(&dev);
  fido_dev_info_free(&dev_infos, libfido_device_id + 1);
  return ret_code;
}

size_t webauthn_registration::get_client_data_json_len() {
  return m_client_data_json.length();
}

std::string webauthn_registration::get_client_data_json() {
  return m_client_data_json;
}
