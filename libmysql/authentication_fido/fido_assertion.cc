/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "fido_assertion.h"
#include "fido_common.h"

#include "mysql_com.h" /* CHALLENGE_LENGTH */

using namespace std;

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

  @retval false received challenge was valid
  @retval true  received challenge was corrupt
*/
bool fido_prepare_assert::parse_challenge(const unsigned char *challenge) {
  char *str = nullptr;
  unsigned char *to = const_cast<unsigned char *>(challenge);
  /* length of challenge should be 32 bytes */
  unsigned long len = net_field_length_ll(&to);
  if (len != CHALLENGE_LENGTH) goto err;
  /* extract challenge */
  set_scramble(to, len);

  to += len;
  /* length of relying party ID */
  len = net_field_length_ll(&to);
  /* Length of relying party ID should not be > 255 */
  if (len > 255) goto err;
  /* extract relying party ID */
  str = new (std::nothrow) char[len + 1];
  memcpy(str, to, len);
  str[len] = 0;
  set_rp_id(str);
  delete[] str;

  to += len;
  /* length of cred ID */
  len = net_field_length_ll(&to);
  /* extract cred ID */
  set_cred_id(to, len);
  to += len;

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
*/
void fido_prepare_assert::get_signed_challenge(unsigned char **challenge_res,
                                               size_t &challenge_res_len) {
  unsigned long authdata_len = get_authdata_len();
  unsigned long sig_len = get_signature_len();
  challenge_res_len = net_length_size(authdata_len) + net_length_size(sig_len) +
                      authdata_len + sig_len;
  *challenge_res = new (std::nothrow) unsigned char[challenge_res_len];
  unsigned char *pos = *challenge_res;
  pos = net_store_length(pos, authdata_len);
  memcpy(pos, get_authdata_ptr(), authdata_len);
  pos += authdata_len;
  pos = net_store_length(pos, sig_len);
  memcpy(pos, get_signature_ptr(), sig_len);
}

/**
  Set method to set 32 bit random salt.

  @param [in] scramble   buffer holding random salt
  @param [in] len        length of salt
*/
void fido_prepare_assert::set_scramble(unsigned char *scramble, size_t len) {
  fido_assert_set_clientdata_hash(m_assert, scramble, len);
}

/**
  Set method to set credential ID.

  @param [in] cred   buffer holding credential ID
  @param [in] len    length of credential ID
*/
void fido_prepare_assert::set_cred_id(unsigned char *cred, size_t len) {
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
bool fido_assertion::prepare_assert(const unsigned char *challenge) {
  return m_fido_prepare_assert.parse_challenge(challenge);
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
void fido_assertion::get_signed_challenge(unsigned char **challenge_res,
                                          size_t &challenge_res_len) {
  m_fido_prepare_assert.get_signed_challenge(challenge_res, challenge_res_len);
}
