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

#include <assert.h>
#include <string>

#include "fido_common.h"
#include "fido_registration.h"

#include "base64.h"    /* base64_decode */
#include "mysql_com.h" /* CHALLENGE_LENGTH */

using namespace std;

/**
  Construcutor to allocate memory for performing attestation (registration)
*/
fido_make_cred::fido_make_cred() {
  m_cred = fido_cred_new();
  // always set default type algorithm to COSE_ES256
  set_type();
}

/**
  Standard destructor
*/
fido_make_cred::~fido_make_cred() { fido_cred_free(&m_cred); }

/**
   This method fills in all information required to initiate registration
   process. This method parses server challenge and generates challenge
   response.

  @param [in] challenge       buffer holding the server challenge

  @retval false   successful generation of credentials.
  @retval true    error occurred.
*/
bool fido_make_cred::make_credentials(const char *challenge) {
  parse_challenge(challenge);
  return generate_signature();
}

/**
  Helper method to parse the challenge received from server during registration
  process. This method extracts salt, user name, relying party name and set it
  in fido_cred_t.

  @param [in] challenge   buffer holding the server challenge

  @retval void
*/
bool fido_make_cred::parse_challenge(const char *challenge) {
  /* decode received challenge from base64 */
  char *tmp_value;
  const char *end_ptr;
  int64 length = base64_needed_decoded_length((uint64)strlen(challenge));
  tmp_value = new char[length];
  length = base64_decode(challenge, (uint64)strlen(challenge), tmp_value,
                         &end_ptr, 0);

  unsigned char *to = (unsigned char *)tmp_value;
  /* length of challenge should be 32 bytes */
  unsigned long len = net_field_length_ll(&to);
  if (len != CHALLENGE_LENGTH) return true;
  /* extract challenge */
  set_scramble(to, len);

  to += len;
  /* length of relying party ID  */
  len = net_field_length_ll(&to);
  /* max length of relying party ID is 255 */
  if (len > 255) return true;
  /* extract relying party ID  */
  char *rp = new (std::nothrow) char[len + 1];
  memcpy(rp, to, len);
  rp[len] = 0;
  set_rp_id(rp);
  delete[] rp;

  to += len;
  /* length of user name */
  len = net_field_length_ll(&to);
  /* user name includes 32 byte user name + 255 bytes hostname */
  if (len > 280) return true;
  /* extract user name */
  char *user = new (std::nothrow) char[len + 1];
  memcpy(user, to, len);
  user[len] = 0;
  set_user(user);
  delete[] user;
  delete[] tmp_value;
  return false;
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
bool fido_make_cred::generate_signature() {
  bool ret_code = false;
  fido_init(0);
  size_t dev_infos_len = 0;
  fido_dev_info_t *dev_infos = fido_dev_info_new(1);
  if (fido_dev_info_manifest(dev_infos, 1, &dev_infos_len) != FIDO_OK ||
      dev_infos_len == 0) {
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
        " registration to complete.");
    get_plugin_messages(s, message_type::INFO);
    if (fido_dev_make_cred(dev, m_cred, nullptr) != FIDO_OK) {
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
  return ret_code;
}

/**
  This method will extract authenticator data, signature, certificate and
  rp id from fido_cred_t struct, construct a buffer holding this data which
  will be converted to base64 format before passing to server.

  @param [out] challenge_response     buffer to hold challenge response

  @retval false success
  @retval true failure
*/
bool fido_make_cred::make_challenge_response(
    unsigned char *&challenge_response) {
  /* copy client response into buf */
  unsigned long authdata_len = get_authdata_len();
  unsigned long sig_len = get_sig_len();
  unsigned long cert_len = get_x5c_len();
  unsigned long rp_id_len = strlen(get_rp_id());

  /* calculate total required buffer length */
  size_t len = net_length_size(authdata_len) + net_length_size(sig_len) +
               (cert_len ? net_length_size(cert_len) + cert_len : 0) +
               net_length_size(rp_id_len) + authdata_len + sig_len + rp_id_len;
  unsigned char *str = new unsigned char[len];
  unsigned char *pos = str;
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
    delete[] str;
    return true;
  }
  pos = net_store_length(pos, rp_id_len);
  memcpy(pos, get_rp_id(), rp_id_len);
  pos += rp_id_len;
  assert(len == (size_t)(pos - str));

  uint64 needed = base64_needed_encoded_length((uint64)len);
  unsigned char *tmp_value = new unsigned char[needed];
  base64_encode(str, len, reinterpret_cast<char *>(tmp_value));
  /* Ensure caller will release this memory. */
  challenge_response = tmp_value;

  delete[] str;
  return false;
}

/**
  Set method to set 32 bit random salt.

  @param [in] scramble   buffer holding random salt
  @param [in] len        length of salt
*/
void fido_make_cred::set_scramble(unsigned char *scramble, size_t len) {
  fido_cred_set_clientdata_hash(m_cred, scramble, len);
}

/**
  Set method to set user name.

  @param [in] user   buffer holding user name
*/
void fido_make_cred::set_user(string user) {
  fido_cred_set_user(m_cred,
                     reinterpret_cast<const unsigned char *>(user.c_str()),
                     user.length(), user.c_str(), nullptr, nullptr);
}

/**
  Method to set the algorithm type

  @param [in] type   algorithm type
*/
void fido_make_cred::set_type(int type) { fido_cred_set_type(m_cred, type); }

/**
  Method to set the relying party name or id

  @param [in] rp_id   buffer holding relying party name
*/
void fido_make_cred::set_rp_id(string rp_id) {
  fido_cred_set_rp(m_cred, rp_id.c_str(), nullptr);
}

/**
  Method to get length of authenticator data

  @retval length of authenticator data.
*/
size_t fido_make_cred::get_authdata_len() {
  return fido_cred_authdata_len(m_cred);
}

/**
  Method to get authenticator data

  @retval buffer holding authenticator data
*/
const unsigned char *fido_make_cred::get_authdata_ptr() {
  return fido_cred_authdata_ptr(m_cred);
}

/**
  Method to get length of signature

  @retval length of signature
*/
size_t fido_make_cred::get_sig_len() { return fido_cred_sig_len(m_cred); }

/**
  Method to get signature data

  @retval buffer holding signature data
*/
const unsigned char *fido_make_cred::get_sig_ptr() {
  return fido_cred_sig_ptr(m_cred);
}

/**
  Method to get length of x509 certificate

  @retval length of x509 certificate
*/
size_t fido_make_cred::get_x5c_len() { return fido_cred_x5c_len(m_cred); }

/**
  Method to get x509 certificate

  @retval buffer holding x509 certificate
*/
const unsigned char *fido_make_cred::get_x5c_ptr() {
  return fido_cred_x5c_ptr(m_cred);
}

/**
  Method to get rp id

  @retval buffer holding rp id
*/
const char *fido_make_cred::get_rp_id() { return fido_cred_rp_id(m_cred); }

/**
  Helper method to initiate registration process
*/
bool fido_registration::make_credentials(const char *challenge) {
  return m_fido_make_cred.make_credentials(challenge);
}

/**
  Helper method to get challenge response
*/
bool fido_registration::make_challenge_response(
    unsigned char *&challenge_response) {
  return m_fido_make_cred.make_challenge_response(challenge_response);
}
