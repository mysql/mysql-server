/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "fido_assertion.h"
#include "fido_common.h"

#include <cstdint>
#include <limits>

#include "mysql_com.h" /* CHALLENGE_LENGTH */

using namespace std;

namespace {
size_t fido_net_field_length_size(const unsigned char *pos) {
  if (*pos <= 251) return 1;
  if (*pos == 252) return 3;
  if (*pos == 253) return 4;
  return 9;
}

bool read_length_encoded_data(const unsigned char **pos,
                              const unsigned char *end,
                              const unsigned char **data, size_t *len,
                              size_t min_len, size_t max_len) {
  if (*pos >= end) return true;

  const size_t length_size = fido_net_field_length_size(*pos);
  if (static_cast<size_t>(end - *pos) < length_size) return true;

  auto *mutable_pos = const_cast<unsigned char *>(*pos);
  const uint64_t decoded_len = net_field_length_ll(&mutable_pos);
  if (decoded_len == static_cast<uint64_t>(NULL_LENGTH) ||
      decoded_len < min_len || decoded_len > max_len)
    return true;

  if (static_cast<uint64_t>(end - mutable_pos) < decoded_len) return true;

  *data = mutable_pos;
  *len = static_cast<size_t>(decoded_len);
  mutable_pos += *len;
  *pos = mutable_pos;
  return false;
}
}  // namespace

/**
  Construcutor to allocate memory for performing assertion (authentication)
*/
fido_prepare_assert::fido_prepare_assert() { m_assert = fido_assert_new(); }

/**
  Standard destructor
*/
fido_prepare_assert::~fido_prepare_assert() { fido_assert_free(&m_assert); }

/**
  Helper method to parse the challenge received from server during
  authentication process. This method extracts salt, relying party
  name and set it in fido_assert_t.

  @param [in] challenge       buffer holding the server challenge
  @param [in] challenge_len   length of the server challenge

  @retval false received challenge was valid
  @retval true  received challenge was corrupt
*/
bool fido_prepare_assert::parse_challenge(const unsigned char *challenge,
                                          size_t challenge_len) {
  if (!challenge) return true;

  const unsigned char *to = challenge;
  const unsigned char *end = challenge + challenge_len;
  const unsigned char *data = nullptr;
  size_t len = 0;

  if (read_length_encoded_data(&to, end, &data, &len, CHALLENGE_LENGTH,
                               CHALLENGE_LENGTH))
    goto err;
  set_scramble(data, len);

  if (read_length_encoded_data(&to, end, &data, &len, 0,
                               RELYING_PARTY_ID_LENGTH))
    goto err;
  {
    string str(reinterpret_cast<const char *>(data), len);
    set_rp_id(str.c_str());
  }

  if (read_length_encoded_data(&to, end, &data, &len, 0,
                               numeric_limits<size_t>::max()))
    goto err;
  set_cred_id(data, len);

  return false;

err:
  get_plugin_messages("Challange recevied is corrupt.", message_type::ERROR);
  return true;
}

/**
  Method to obtains an assertion from a FIDO device.

  @retval false assertion successful.
  @retval true  assertion failed.
*/
bool fido_prepare_assert::sign_challenge() {
  bool ret_code = false;
  fido_init(0);
  size_t dev_infos_len = 0;
  fido_dev_info_t *dev_infos = fido_dev_info_new(1);
  if (fido_dev_info_manifest(dev_infos, 1, &dev_infos_len) != FIDO_OK) {
    fido_dev_info_free(&dev_infos, 1);
    get_plugin_messages("No FIDO device available on client host.",
                        message_type::ERROR);
    return true;
  }
  const fido_dev_info_t *curr = fido_dev_info_ptr(dev_infos, 0);
  const char *path = fido_dev_info_path(curr);
  /* open the device */
  fido_dev_t *dev = fido_dev_new();
  if (fido_dev_open(dev, path) != FIDO_OK) {
    get_plugin_messages("Failed to open FIDO device.", message_type::ERROR);
    ret_code = true;
    goto end;
  } else {
    std::string s(
        "Please insert FIDO device and perform gesture action for"
        " authentication to complete.");
    get_plugin_messages(s, message_type::INFO);
    if (fido_dev_get_assert(dev, m_assert, nullptr) != FIDO_OK) {
      get_plugin_messages(
          "Assertion failed.Please check relying party ID "
          "(@@global.authentication_fido_rp_id) of server.",
          message_type::ERROR);
      ret_code = true;
      goto end;
    }
  }
end:
  fido_dev_close(dev);
  fido_dev_free(&dev);
  fido_dev_info_free(&dev_infos, 1);
  return ret_code;
}

/**
  This method will extract authenticator data, signature from fido_assert_t
  struct.

  @param [out] challenge_res     buffer to signed challenge
  @param [out] challenge_res_len length of signed challenge

  @retval false success
  @retval true  allocation failed
*/
bool fido_prepare_assert::get_signed_challenge(unsigned char **challenge_res,
                                               size_t &challenge_res_len) {
  unsigned long authdata_len = get_authdata_len();
  unsigned long sig_len = get_signature_len();
  challenge_res_len = net_length_size(authdata_len) + net_length_size(sig_len) +
                      authdata_len + sig_len;
  *challenge_res = new (std::nothrow) unsigned char[challenge_res_len];
  if (!*challenge_res) {
    challenge_res_len = 0;
    return true;
  }
  unsigned char *pos = *challenge_res;
  pos = net_store_length(pos, authdata_len);
  memcpy(pos, get_authdata_ptr(), authdata_len);
  pos += authdata_len;
  pos = net_store_length(pos, sig_len);
  memcpy(pos, get_signature_ptr(), sig_len);
  return false;
}

/**
  Set method to set 32 bit random salt.

  @param [in] scramble   buffer holding random salt
  @param [in] len        length of salt
*/
void fido_prepare_assert::set_scramble(const unsigned char *scramble,
                                       size_t len) {
  fido_assert_set_clientdata_hash(m_assert, scramble, len);
}

/**
  Set method to set credential ID.

  @param [in] cred   buffer holding credential ID
  @param [in] len    length of credential ID
*/
void fido_prepare_assert::set_cred_id(const unsigned char *cred, size_t len) {
  fido_assert_allow_cred(m_assert, cred, len);
}

/**
  Method to set the relying party name or id.

  @param [in] rp_id   buffer holding relying party name
*/
void fido_prepare_assert::set_rp_id(const char *rp_id) {
  fido_assert_set_rp(m_assert, rp_id);
}

/**
  Method to get authenticator data

  @retval buffer holding authenticator data
*/
const unsigned char *fido_prepare_assert::get_authdata_ptr() {
  return fido_assert_authdata_ptr(m_assert, 0);
}

/**
  Method to get length of authenticator data

  @retval length of authenticator data
*/
size_t fido_prepare_assert::get_authdata_len() {
  return fido_assert_authdata_len(m_assert, 0);
}

/**
  Method to get signature

  @retval buffer holding signature data
*/
const unsigned char *fido_prepare_assert::get_signature_ptr() {
  return fido_assert_sig_ptr(m_assert, 0);
}

/**
  Method to get length of signature

  @retval length of signature
*/
size_t fido_prepare_assert::get_signature_len() {
  return fido_assert_sig_len(m_assert, 0);
}

/**
  Helper method to prepare all context required to perform assertion.
*/
bool fido_assertion::prepare_assert(const unsigned char *challenge,
                                    size_t challenge_len) {
  return m_fido_prepare_assert.parse_challenge(challenge, challenge_len);
}

/**
  Helper method to sign the challenge received from server side FIDO
  plugin during authentication, and send signed challenge back to server
  side plugin, only if FIDO device successfully verifies the challenge,
  else report an error.
*/
bool fido_assertion::sign_challenge() {
  return m_fido_prepare_assert.sign_challenge();
}

/**
  Helper method to prepare challenge response to be passed to server
*/
bool fido_assertion::get_signed_challenge(unsigned char **challenge_res,
                                          size_t &challenge_res_len) {
  return m_fido_prepare_assert.get_signed_challenge(challenge_res,
                                                    challenge_res_len);
}
