/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <common.h>
#include <mysql_com.h> /* CHALLENGE_LENGTH */

/**
  This method will extract authenticator data, signature from fido_assert_t
  struct and serialize it.

  @param [out] challenge_res     buffer to signed challenge
  @param [out] challenge_res_len length of signed challenge

  @retval false successful.
  @retval true  failed.

*/
bool fido_assertion::get_signed_challenge(unsigned char **challenge_res,
                                          size_t &challenge_res_len) {
  unsigned long authdata_len = get_authdata_len();
  unsigned long sig_len = get_signature_len();
  challenge_res_len = net_length_size(authdata_len) + net_length_size(sig_len) +
                      authdata_len + sig_len;
  *challenge_res = new (std::nothrow) unsigned char[challenge_res_len];
  if (!challenge_res) return true;
  unsigned char *pos = *challenge_res;
  pos = net_store_length(pos, authdata_len);
  memcpy(pos, get_authdata_ptr(), authdata_len);
  pos += authdata_len;
  pos = net_store_length(pos, sig_len);
  memcpy(pos, get_signature_ptr(), sig_len);
  return false;
}

/**
  Method to obtains an assertion from a FIDO device.

  @retval false assertion successful.
  @retval true  assertion failed.
*/
bool fido_assertion::sign_challenge() {
  bool ret_code = false;
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
          "Assertion failed. Please check relying party ID of the server.",
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
  Set method to set 32 bytes random salt.

  @param [in] salt         buffer holding random salt
*/
void fido_assertion::set_client_data(const unsigned char *salt, const char *) {
  fido_assert_set_clientdata_hash(m_assert, salt, CHALLENGE_LENGTH);
}

/**
  Helper method to parse the challenge received from server during
  authentication process. This method extracts salt, relying party
  name and credential ID.

  @param [in] challenge       buffer holding the server challenge

  @retval false received challenge was valid
  @retval true  received challenge was corrupt
*/
bool fido_assertion::parse_challenge(const unsigned char *challenge) {
  char rp[RELYING_PARTY_ID_LENGTH] = {0};
  unsigned char scramble[CHALLENGE_LENGTH] = {0};
  unsigned char *to = const_cast<unsigned char *>(challenge);
  if (!to) return true;
  /* length of challenge should be 32 bytes */
  unsigned long len = net_field_length_ll(&to);
  if (len != CHALLENGE_LENGTH) goto err;
  /* extract challenge */
  memcpy(scramble, to, CHALLENGE_LENGTH);
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
  if (!to) return true;
  /* length of cred ID */
  len = net_field_length_ll(&to);
  /* extract cred ID */
  set_cred_id(to, len);
  to += len;
  /* set client data context */
  set_client_data(scramble, rp);
  return false;

err:
  get_plugin_messages("Challange recevied is corrupt.", message_type::ERROR);
  return true;
}
