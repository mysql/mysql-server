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

#include "webauthn_assertion.h"

#include <iostream>
#include <sstream>

#include <base64.h> /* base64_encode */
#include <common.h>
#include <mysql_com.h>   /* CHALLENGE_LENGTH */
#include <scope_guard.h> /* create_scope_guard */

#undef MYSQL_DYNAMIC_PLUGIN
#include <mysql/service_mysql_alloc.h>
#define MYSQL_DYNAMIC_PLUGIN

#include <fido/credman.h>

namespace {
const unsigned int PIN_BUFFER_SIZE = 256;
}  // namespace
/**
  This method will calculate length of the buffer
  required for challenge response.

  @returns length of the buffer required
*/
size_t webauthn_assertion::calculate_client_response_length() {
  /*
    The length consists of following elements

    1. Packet Identifier (0x02 in this case)
    2. Length encoded number of assertions
    For each assertion
        3. Length encoded auth data
        4. Length encoded signature
    5. Length of encoded client data JSON
  */
  size_t packet_identifier_length = 1;
  size_t num_assertions = get_num_assertions();
  size_t total_num_assertions_len = net_length_size(num_assertions);
  size_t total_authdata_len = 0;
  size_t total_signature_len = 0;
  for (size_t num = 0; num < num_assertions; ++num) {
    size_t authdata_len = get_authdata_len(num);
    total_authdata_len += net_length_size(authdata_len) + authdata_len;
    size_t signature_len = get_signature_len(num);
    total_signature_len += net_length_size(signature_len) + signature_len;
  }
  size_t client_data_json_len = get_client_data_json_len();
  size_t total_client_data_json_len =
      net_length_size(client_data_json_len) + client_data_json_len;
  return packet_identifier_length + total_num_assertions_len +
         total_authdata_len + total_signature_len + total_client_data_json_len;
}

/**
  This method will construct challenge response which is passed to server.
  Challenge response format is:
    [packet identifier 0x02]
    [length encoded authenticator data]
    [length encoded signature]
    [length encoded client data JSON]

  @param [out] challenge_res     buffer to challenge response
  @param [out] challenge_res_len length of challenge response

  @retval false successful.
  @retval true  failed.
*/
bool webauthn_assertion::get_signed_challenge(unsigned char **challenge_res,
                                              size_t &challenge_res_len) {
  challenge_res_len = calculate_client_response_length();
  *challenge_res = new (std::nothrow) unsigned char[challenge_res_len];
  if (!challenge_res) return true;
  unsigned char *pos = *challenge_res;

  /* Add tag */
  const unsigned char tag = '\2';
  *pos = tag;
  pos++;

  /* Length encoded num_assertions */
  size_t num_assertions = get_num_assertions();
  pos = net_store_length(pos, static_cast<unsigned long long>(num_assertions));

  /*
    For each assertion:
    - Length encoded auth data
    - Length encoded signature
  */
  for (size_t num = 0; num < num_assertions; ++num) {
    size_t authdata_len = get_authdata_len(num);
    pos = net_store_length(pos, static_cast<unsigned long long>(authdata_len));
    memcpy(pos, get_authdata_ptr(num), authdata_len);
    pos += authdata_len;
    size_t sig_len = get_signature_len(num);
    pos = net_store_length(pos, static_cast<unsigned long long>(sig_len));
    memcpy(pos, get_signature_ptr(num), sig_len);
    pos += sig_len;
  }

  /* Length encoded client data JSON */
  size_t client_data_json_len = get_client_data_json_len();
  pos = net_store_length(
      pos, +static_cast<unsigned long long>(client_data_json_len));
  memcpy(pos, get_client_data_json().c_str(), client_data_json_len);
  pos += client_data_json_len;

  return false;
}

/**
  Method to obtains an assertion from a FIDO device.

  @retval false assertion successful.
  @retval true  assertion failed.
*/
bool webauthn_assertion::sign_challenge() {
  bool ret_code = false;
  fido_dev_info_t *dev_info = discover_fido2_devices(libfido_device_id + 1);
  if (!dev_info) return true;
  const fido_dev_info_t *curr = fido_dev_info_ptr(dev_info, libfido_device_id);
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
    std::string s(
        "Please insert FIDO device and perform gesture action for"
        " authentication to complete.");
    get_plugin_messages(s, message_type::INFO);
    if (fido_dev_get_assert(dev, m_assert, nullptr) != FIDO_OK) {
      get_plugin_messages(
          "Assertion failed. Please check relying party ID of the server.",
          message_type::ERROR);
      ret_code = true;
      goto end;
    }
  }
end:
  fido_dev_close(dev);
  fido_dev_free(&dev);
  fido_dev_info_free(&dev_info, libfido_device_id + 1);
  return ret_code;
}

/**
  Helper method to set client data context.

  Client data format is:
  SHA256({
          "type": "webauthn.get",
          "challenge": url_safe_base64("32 byte random"),
          "origin": authentication_webauthn_rp_id,
          "crossOrigin": false
        })

  @param [in] salt   buffer holding 32 byte random
  @param [in] rp         relying party name aka origin
*/
void webauthn_assertion::set_client_data(const unsigned char *salt,
                                         const char *rp) {
  unsigned char client_data_buf[512] = {0};
  char base64_salt[BASE64_CHALLENGE_LENGTH] = {0};
  char url_compatible_salt[BASE64_CHALLENGE_LENGTH] = {0};

  /* convert salt to be base64 */
  base64_encode(salt, CHALLENGE_LENGTH, base64_salt);
  /* convert salt to be websafe base64 */
  url_compatible_base64(url_compatible_salt, BASE64_CHALLENGE_LENGTH,
                        base64_salt);

  /* construct client data JSON object */
  size_t client_data_len = snprintf(
      reinterpret_cast<char *>(client_data_buf), sizeof(client_data_buf),
      "{\"type\":\"webauthn.get\",\"challenge\":"
      "\"%s\",\"origin\":\"https://%s\",\"crossOrigin\":false}",
      url_compatible_salt, rp);

  fido_assert_set_clientdata(m_assert, client_data_buf, client_data_len);
  /* save clientdataJSON */
  m_client_data_json = reinterpret_cast<char *>(client_data_buf);
}

/**
  Helper method to parse the challenge received from server during
  authentication process. This method extracts salt, relying party
  name and credential ID.

  @param [in] challenge       buffer holding the server challenge

  @retval false received challenge was valid
  @retval true  received challenge was corrupt
*/
bool webauthn_assertion::parse_challenge(const unsigned char *challenge) {
  char rp[RELYING_PARTY_ID_LENGTH + 1] = {0};
  unsigned char salt[CHALLENGE_LENGTH + 1] = {0};
  unsigned char *to = const_cast<unsigned char *>(challenge);
  if (!to) return true;
  /* skip reading capability flag */
  to++;
  if (!to) return true;
  /* length of challenge should be 32 bytes */
  unsigned long len = net_field_length_ll(&to);
  if (len != CHALLENGE_LENGTH) goto err;
  /* extract challenge */
  memcpy(salt, to, CHALLENGE_LENGTH);
  to += len;
  if (!to) return true;
  /* length of relying party ID */
  len = net_field_length_ll(&to);
  /* Length of relying party ID should not be > 255 */
  if (len > 255) goto err;
  /* extract relying party ID */
  memcpy(rp, to, len);
  set_rp_id(rp);
  to += len;
  /* set client data context */
  set_client_data(salt, rp);
  return false;

err:
  get_plugin_messages("Challange recevied is corrupt.", message_type::ERROR);
  return true;
}

/**
  This method is called by webauthn_authentication_client plugin to check
  if the token device present on current host does support resident keys(aka
  discoverable credentials or credential management) or not.

  @param [out] is_fido2     set to true if device supports resident keys

  @returns Status of check
    @retval false Success
    @retval true  Failure
*/
bool webauthn_assertion::check_fido2_device(bool &is_fido2) {
  fido_dev_info_t *dev_info = discover_fido2_devices(libfido_device_id + 1);
  if (!dev_info) return true;
  const fido_dev_info_t *curr = fido_dev_info_ptr(dev_info, libfido_device_id);
  const char *path = fido_dev_info_path(curr);
  /* open the device */
  fido_dev_t *dev = fido_dev_new();
  auto cleanup = create_scope_guard([&] {
    fido_dev_close(dev);
    fido_dev_free(&dev);
    fido_dev_info_free(&dev_info, libfido_device_id + 1);
  });

  if (fido_dev_open(dev, path) != FIDO_OK) {
    get_plugin_messages("Failed to open FIDO device.", message_type::ERROR);
    return true;
  }
  is_fido2 = fido_dev_supports_credman(dev);
  return false;
}

size_t webauthn_assertion::get_client_data_json_len() {
  return m_client_data_json.length();
}

std::string webauthn_assertion::get_client_data_json() {
  return m_client_data_json;
}

/**
  Select credential ID from a list of resident keys and set it for assertion

  @returns status of operation
    @retval false Success
    @retval true  Error
*/
bool webauthn_assertion::select_credential_id() {
  if (!m_preserve_privacy) return false;
  fido_dev_info_t *dev_infos{nullptr};
  fido_dev_t *dev{nullptr};
  fido_credman_rk_t *rk{nullptr};
  const char *rp_id = get_rp_id();
  auto cleanup_guard = create_scope_guard([&] {
    if (dev_infos) fido_dev_info_free(&dev_infos, libfido_device_id + 1);
    if (rk) fido_credman_rk_free(&rk);
    if (dev) {
      fido_dev_close(dev);
      fido_dev_free(&dev);
    }
  });

  dev_infos = discover_fido2_devices(libfido_device_id + 1);
  if (!dev_infos) return true;

  const fido_dev_info_t *curr = fido_dev_info_ptr(dev_infos, libfido_device_id);
  const char *path = fido_dev_info_path(curr);
  /* open the device */
  if (!(dev = fido_dev_new())) {
    get_plugin_messages("Failed to allocate memory for fido_credman_rk_t",
                        message_type::ERROR);
    return true;
  }

  if (fido_dev_open(dev, path) != FIDO_OK) {
    get_plugin_messages("Failed to open FIDO device.", message_type::ERROR);
    return true;
  }
  if (!(rk = fido_credman_rk_new())) {
    get_plugin_messages("Failed to allocate memory for fido_dev_t",
                        message_type::ERROR);
    return true;
  }

  char pin[PIN_BUFFER_SIZE]{0};
  if (get_user_input("2. Enter PIN for token device: ", input_type::PASSWORD,
                     pin, &PIN_BUFFER_SIZE)) {
    get_plugin_messages("Failed to get device PIN", message_type::ERROR);
    return true;
  }

  if (fido_credman_get_dev_rk(dev, rp_id, rk, pin) != FIDO_OK) {
    get_plugin_messages(
        "Failed to get metadata for discoverable credentail from the device",
        message_type::ERROR);
    return true;
  }
  memset(pin, 1, strlen(pin));

  auto count{fido_credman_rk_count(rk)};
  if (count == 0) {
    std::stringstream message;
    message << "No credentials found for RP ID: " << rp_id << ".";
    get_plugin_messages(message.str(), message_type::ERROR);
    return true;
  }

  unsigned int input{1};
  if (count == 1) {
    std::stringstream message;
    message << "Found only one credential for RP ID: " << rp_id
            << ". Using the same for authentication. ";
    get_plugin_messages(message.str(), message_type::INFO);
  } else {
    std::string message("Found following credentials for RP ID: ");
    message.append(rp_id);
    get_plugin_messages(message, message_type::INFO);
    for (size_t index = 0; index < count; ++index) {
      const fido_cred_t *cred{nullptr};
      if (!(cred = fido_credman_rk(rk, index))) {
        std::string error{
            "Failed to get discoverable credential metadata for index: "};
        error.append(std::to_string(index));
        get_plugin_messages(error, message_type::INFO);
        return true;
      }

      std::stringstream ss;
      ss << "[" << index + 1 << "]"
         << std::string{
                reinterpret_cast<const char *>(fido_cred_user_id_ptr(cred)),
                fido_cred_user_id_len(cred)};
      get_plugin_messages(ss.str(), message_type::INFO);
    }
    message.assign("Please select one(1...N): ");
    if (get_user_input(message, input_type::UINT,
                       pointer_cast<void *>(&input))) {
      get_plugin_messages("Failed to get input", message_type::ERROR);
      return true;
    }
    if (input > count || input < 1) {
      get_plugin_messages("Invalid input", message_type::ERROR);
      return true;
    }
  }

  const fido_cred_t *cred{nullptr};
  if (!(cred = fido_credman_rk(rk, input - 1))) {
    std::string error{
        "Failed to get discoverable credential metadata for index: "};
    error.append(std::to_string(input - 1));
    get_plugin_messages(error, message_type::INFO);
    return true;
  }

  set_cred_id(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
  return false;
}
