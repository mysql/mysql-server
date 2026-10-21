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

#include "attestation.h"
#include <openssl/err.h>
#include "my_dbug.h"   /* DBUG_EXECUTE_IF */
#include "mysql_com.h" /* net_* */

using namespace server_registration;

/**
  Construcutor to allocate memory for new registration.
*/
attestation::attestation(int alg, std::string format /* = "fido-u2f" */) {
  m_cred = fido_cred_new();
  /* always set default type algorithm to COSE_ES256 */
  fido_cred_set_type(m_cred, alg);
  /* set default format as fido-u2f */
  fido_cred_set_fmt(m_cred, format.c_str());
}

/**
  Standard destructor
*/
attestation::~attestation() { fido_cred_free(&m_cred); }

/**
  Method to set the relying party name or id.

  @param [in] rp_id   buffer holding relying party name
*/
bool attestation::set_rp_id(std::string rp_id) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  int res = fido_cred_set_rp(m_cred, rp_id.c_str(), nullptr);
  return (res != FIDO_OK);
}

/**
  Set method to set client data.

  @param [in] cd         buffer holding client data
  @param [in] len        length of client data
*/
bool attestation::set_client_data(const unsigned char *cd, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  int res = fido_cred_set_clientdata(m_cred, cd, len);
  return (res != FIDO_OK);
}

/**
  Method to set authenticator data.

  @param [in] ptr   buffer holding authenticator data
  @param [in] len   length of authenticator data
*/
bool attestation::set_authenticator_data(const unsigned char *ptr, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  int res = fido_cred_set_authdata(m_cred, ptr, len);
  return (res != FIDO_OK);
}

/**
  Method to set signature

  @param [in] ptr   buffer holding signature
  @param [in] len   length of signature
*/
bool attestation::set_signature(const unsigned char *ptr, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  if (m_is_full_attestation_blob) return false;

  int res = fido_cred_set_sig(m_cred, ptr, len);
  return (res != FIDO_OK);
}

/**
  Method to set certificate

  @param [in] ptr   buffer holding certificate
  @param [in] len   length of certificate
*/
bool attestation::set_x509_certificate(const unsigned char *ptr, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  if (m_is_full_attestation_blob) return false;

  int res = fido_cred_set_x509(m_cred, ptr, len);
  return (res != FIDO_OK);
}

/**
  @brief Set the full attestation statement blob, as obtained by
  fido_cred_attstmt_ptr()

  If this one is called there's no need to set the X509 cert and signature,
  as they are contained into the attestation statement's CBOR format.
  See libfido's fido_cred_set_attstmt() for detals.

  * @param ptr The CBOR formatted blob to set
  * @param len Length of the CBOR blob in bytes
  * @retval false success
  * @retval true failure
 */
bool attestation::set_attestation_statement(const unsigned char *ptr,
                                            size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  if (m_is_full_attestation_blob) {
    int res = fido_cred_set_attstmt(m_cred, ptr, len);
    return (res != FIDO_OK);
  }
  return false;
}

/**
  @brief Set the format of the attestation statement.

  It currently is one of: [packed, fido-u2f, none or TPM].
  See libfido's fido_cred_set_fmt() and fido_cred_fmt() for more details.

 * @param ptr The format string
 * @param len The length of the format string
 * @retval true failure
 * @retval false success
 */
bool attestation::set_fmt(const unsigned char *ptr, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  if (m_is_full_attestation_blob) {
    std::string str(reinterpret_cast<const char *>(ptr), len);
    int res = fido_cred_set_fmt(m_cred, str.c_str());
    return res != FIDO_OK;
  }
  return false;
}
/**
  Method to get length of credential id.

  @retval length of credential id
*/
size_t attestation::get_cred_id_len() { return fido_cred_id_len(m_cred); }

/**
  Method to get credential id which will be stored in mysql.user table.

  @retval buffer holding credential id
*/
const unsigned char *attestation::get_cred_id_ptr() {
  return fido_cred_id_ptr(m_cred);
}

/**
  Method to get length of public key

  @retval length of public key
*/
size_t attestation::get_cred_pubkey_len() {
  return fido_cred_pubkey_len(m_cred);
}

/**
  Method to get public key which will be stored in mysql.user table.

  @retval buffer holding public key
*/
const unsigned char *attestation::get_cred_pubkey_ptr() {
  return fido_cred_pubkey_ptr(m_cred);
}

/**
  Method to set authenticator data format.

  @param [in] format   authenticator data format can be "u2f" or "packed"
*/
bool attestation::set_authenticator_data_format(const std::string format) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  int res = fido_cred_set_fmt(m_cred, format.c_str());
  return (res != FIDO_OK);
}

/**
  Method to set flag based on RESIDENT_KEYS capability.
*/
void attestation::set_is_fido2(bool val) { m_is_fido2 = val; }

/**
  @brief Set a flag indicating the full attestation blob is available

  * @param val the new value
*/
void attestation::set_is_full_attestation_blob(bool val) {
  m_is_full_attestation_blob = val;
}

/**
  @brief gets the flag indicating that the full attestation blob is available

  * @return true if the full attestation blob is available
*/
bool attestation::get_is_full_attestation_blob() {
  return m_is_full_attestation_blob;
}

/**
  Method to check if RESIDENT_KEYS is supported or not.

  @retval false   authenticator does not support CTAP2.1 protocol
  @retval true    authenticator supports CTAP2.1 protocol
*/
bool attestation::get_is_fido2() { return m_is_fido2; }

/**
   This method fills in all information required to perform verification of
   signature. This method parses server challenge and challenge response.

  @param [in] challenge_reponse       buffer holding the challenge response
  @param [in] challenge_reponse_len   length of the  challenge response
  @param [in] challenge       buffer holding the server challenge
  @param [in] challenge_len   length of the server challenge

  @retval true  fail if packet received from client side plugin is wrong
  @retval false success
*/
bool attestation::prepare_credentials(const unsigned char *challenge_reponse,
                                      unsigned int challenge_reponse_len,
                                      const unsigned char *challenge,
                                      unsigned int challenge_len) {
  if (challenge_len == 0 || challenge_reponse_len == 0) return true;
  if (parse_challenge(challenge, challenge_len)) return true;
  if (parse_challenge_response(challenge_reponse, challenge_reponse_len))
    return true;
  return false;
}

/**
  Helper method to verify challenge response received from fido device via
  client.

  @retval false signature was valid.
  @retval true  invalid signature
*/
bool attestation::verify_signature() {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });

  bool ret = true;
  /*
    In case x509 certificate is not available invalidate verfication
  */
  if (fido_cred_x5c_ptr(m_cred) == nullptr)
    ret = true;
  else
    ret = fido_cred_verify(m_cred) != 0;

  // clear the openssl error stack after FIDO interactions
  if (ret) ERR_clear_error();
  return ret;
}
