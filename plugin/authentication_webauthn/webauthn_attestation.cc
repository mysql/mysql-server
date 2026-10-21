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

#include "webauthn_attestation.h"
#include "common.h"

#include <base64.h>      /* base64_decode */
#include <my_dbug.h>     /* DBUG_EXECUTE_IF */
#include <my_inttypes.h> /* uint64 */
#include <my_sys.h>      /* MY_WME */
#include <scope_guard.h> /* create_scope_guard */

/* fido2 protocol supports "packed" format" */
const std::string attestation_fido2_format{"packed"};
/* fido1 aka U2F protocol supports "fido-u2f" format" */
// const std::string attestation_u2f_format{"fido-u2f"};

/* default constructor */
webauthn_attestation::webauthn_attestation(int alg) : attestation(alg) {}

/**
  Helper method to parse the challenge received from server during registration
  process. This method decodes the received challenge which is in base64 and
  extracts capability flag and salt. Set salt in fido_cred_t.

  @param [in] challenge       buffer holding the server challenge
  @param [in] challenge_len   length of the server challenge

  @retval true  fail if challenge received from server is wrong
  @retval false success
*/
bool webauthn_attestation::parse_challenge(const unsigned char *challenge,
                                           unsigned int challenge_len) {
  /* decode challenge from base64 */
  char *tmp_value;
  const char *end_ptr;
  int64 length = base64_needed_decoded_length((uint64)challenge_len);
  tmp_value =
      (char *)my_malloc(key_memory_webauthn_attestation, length, MYF(MY_WME));

  auto cleanup = create_scope_guard([&] { my_free(tmp_value); });
  length = base64_decode(reinterpret_cast<const char *>(challenge),
                         (uint64)challenge_len, tmp_value, &end_ptr, 0);
  if (length < 0) return true;
  auto *to = reinterpret_cast<unsigned char *>(tmp_value);
  uint64_t max = length > 0 ? length : 0;
  /* skip capability flag */
  to++;
  max--;
  /* invalid data */
  if (!to) return true;
  auto lenc_size = net_field_length_size(to);
  if (lenc_size > max) return true;
  /* length of salt should be 32 bytes */
  auto len = net_field_length_ll(&to);
  max -= lenc_size;
  if (len != CHALLENGE_LENGTH || len > max) {
    return true;
  }
  /* save the random salt */
  memcpy(m_salt, to, CHALLENGE_LENGTH);
  return false;
}

/**
  Helper method to parse the challenge response received from fido device via
  client. This method extracts capability flag, authenticator data, signature,
  x509 certificate, client data JSON object and set it in fido_cred_t.

  @param [in] challenge_res        buffer holding the challenge response
  @param [in] challenge_res_length length of buffer holding the challenge
  response

  @retval true  fail if challenge response is wrong or corrupt
  @retval false success
*/
bool webauthn_attestation::parse_challenge_response(
    const unsigned char *challenge_res, unsigned int challenge_res_length) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* decode challenge response from base64 */
  const char *end_ptr;
  int64 length = base64_needed_decoded_length((uint64)challenge_res_length);
  char *tmp_value =
      (char *)my_malloc(key_memory_webauthn_attestation, length, MYF(MY_WME));
  auto cleanup = create_scope_guard([&] { my_free(tmp_value); });
  length = base64_decode(reinterpret_cast<const char *>(challenge_res),
                         (uint64)challenge_res_length, tmp_value, &end_ptr, 0);
  if (length < 0) return true;

  auto *to = reinterpret_cast<unsigned char *>(tmp_value);
  uint64_t max = length > 0 ? length : 0;
  uint lenc_size = 0;
  /* read token capability sent by client */
  unsigned short capability = *to;
  to++;
  max--;
  if (capability & RESIDENT_KEYS) {
    /*
      client host is having fido2 token which supports CTAP1.2 protocol (aka
      resident keys or discoverable credentials), thus attestation statement
      format will be in "packed" format.
    */
    set_is_fido2(true);
    if (set_authenticator_data_format(attestation_fido2_format)) return true;
  }
  set_is_full_attestation_blob(0 != (capability & SEND_FULL_ATTESTATION_BLOB));

  lenc_size = net_field_length_size(to);
  if (lenc_size > max) return true;
  /* length of authenticator data */
  unsigned int len = net_field_length_ll(&to);
  max -= lenc_size;
  if ((len > MAX_AUTH_DATA_LENGTH) ||
      (to + len) >= reinterpret_cast<unsigned char *>(tmp_value + length)) {
    return true;
  }
  /* extract authenticator data */
  if (set_authenticator_data(to, len)) return true;
  to += len;
  max -= len;
  if (!to) return true;
  lenc_size = net_field_length_size(to);
  if (lenc_size > max) return true;

  /* length of signature  */
  len = net_field_length_ll(&to);
  max -= lenc_size;
  if ((len > MAX_SIGNATURE_LENGTH) ||
      (to + len) >= reinterpret_cast<unsigned char *>(tmp_value + length)) {
    return true;
  }
  /* extract signature  */
  if (set_signature(to, len)) return true;
  to += len;
  max -= len;
  if (!to) return true;
  lenc_size = net_field_length_size(to);
  if (lenc_size > max) return true;
  /* length of x509 certificate */
  len = net_field_length_ll(&to);
  max -= lenc_size;
  /*
    ensure certificate is within limits of buffer holding the
    challenge response
  */
  if ((to + len) >= reinterpret_cast<unsigned char *>(tmp_value + length))
    return true;
  /* extract x509 certificate */
  if (set_x509_certificate(to, len)) return true;
  to += len;
  max -= len;
  /* set relying party ID  */
  if (set_rp_id(authentication_webauthn_rp_id)) return true;
  if (!to) return true;
  lenc_size = net_field_length_size(to);
  if (lenc_size > max) return true;
  /* length of client data hash */
  len = net_field_length_ll(&to);
  if ((to + len) > reinterpret_cast<unsigned char *>(tmp_value + length))
    return true;
  /* extract client data */
  set_client_data(to, len);
  to += len;
  max -= len;

  if (get_is_full_attestation_blob()) {
    /* retrieve and set the attestation statement */
    lenc_size = net_field_length_size(to);
    if (lenc_size > max) return true;
    /* length of the attestation blob */
    len = net_field_length_ll(&to);
    max -= lenc_size;
    if ((len > MAX_ATTESTATION_BLOB_LENGTH) ||
        (to + len) > reinterpret_cast<unsigned char *>(tmp_value + length)) {
      return true;
    }
    if (set_attestation_statement(to, len)) return true;
    to += len;
    max -= len;

    /* retrieve and set the format */
    lenc_size = net_field_length_size(to);
    if (lenc_size > max) return true;
    /* length of the format string */
    len = net_field_length_ll(&to);
    max -= lenc_size;
    if ((len > MAX_FMT_STRING_LENGTH) ||
        (to + len) > reinterpret_cast<unsigned char *>(tmp_value + length)) {
      return true;
    }
    if (set_fmt(to, len)) return true;
    to += len;
    max -= len;
  }

  return false;
}

/**
  This method will construct authentication_string after successfull
  registration is performed. Format of authentication_string is: [length encoded
  public key][optional length encoded credential ID] This authentication_string
  is saved in mysql.user table.

  @param [out] buf        buffer to hold key handle
  @param [in, out] buflen length of key handle

  @retval false success
  @retval true  error
*/
bool webauthn_attestation::get_key_handle(unsigned char **buf,
                                          unsigned int *buflen) {
  DBUG_EXECUTE_IF("simulate_fido_testing", {
    /* dummy auth string */
    const char *dummy = "\nCREDIDLEN \nPUBKEYLEN";
    size_t sz = strlen(dummy);
    memcpy(*buf, dummy, sz);
    *buflen = sz;
    return false;
  });
  bool is_fido2 = get_is_fido2();
  unsigned long long cred_id_len = get_cred_id_len();
  unsigned long long pk_len = get_cred_pubkey_len();
  /* total length of key handle */
  unsigned int length = net_length_size(pk_len) + pk_len;
  if (!is_fido2) length += (net_length_size(cred_id_len) + cred_id_len);
  /* if length of buffer provided by server is not enough report error */
  if (length > *buflen) return true;
  unsigned char *pos = *buf;
  pos = net_store_length(pos, pk_len);
  /* copy public key */
  if (get_cred_pubkey_ptr()) memcpy(pos, get_cred_pubkey_ptr(), pk_len);
  pos += pk_len;
  if (!is_fido2) {
    pos = net_store_length(pos, cred_id_len);
    /* save credential ID only if it is U2F protocol. */
    if (get_cred_id_ptr()) memcpy(pos, get_cred_id_ptr(), cred_id_len);
    pos += cred_id_len;
  }
  if (length != (size_t)(pos - *buf)) return true;
  *buflen = length;
  return false;
}

/**
  Set method to set client data hash. This method will read client data JSON
  replace the random value received from client with what server has generated.
  Then generate SHA256 hash of the JSON object and set the hash in library
  context.

  @param [in] cd         buffer holding client data JSON object
  @param [in] len        length of buffer holding client data JSON object
*/
void webauthn_attestation::set_client_data(unsigned char *cd, size_t len) {
  char base64_salt[BASE64_SALT_LENGTH] = {0};
  char url_compatible_salt[BASE64_SALT_LENGTH] = {0};

  /* convert salt to be base64 */
  base64_encode(m_salt, CHALLENGE_LENGTH, base64_salt);
  /* convert base64 encoded salt to be websafe base64 */
  url_compatible_base64(url_compatible_salt, BASE64_SALT_LENGTH, base64_salt);

  std::string client_data_json = reinterpret_cast<char *>(cd);
  const std::string key{"\"challenge\":\""};
  auto p = client_data_json.find(key);
  client_data_json.replace(p + key.length(), strlen(url_compatible_salt),
                           url_compatible_salt);
  attestation::set_client_data(
      reinterpret_cast<const unsigned char *>(client_data_json.c_str()), len);
}
