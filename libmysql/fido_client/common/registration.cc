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

#include "registration.h"
#include "common.h"

#include <scope_guard.h>

using namespace client_registration;
/**
  Construcutor to allocate memory for performing attestation (registration)
*/
registration::registration() {
  m_cred = fido_cred_new();
  // always set defaut type algorithm to COSE_ES256
  fido_cred_set_type(m_cred, COSE_ES256);
}

/**
  Standard destructor
*/
registration::~registration() { fido_cred_free(&m_cred); }

/**
   This method fills in all information required to initiate registration
   process. This method parses server challenge and generates challenge
   response.

  @param [in] challenge       buffer holding the server challenge

  @retval false   successfull generation of credentials.
  @retval true    error occurred.
*/
bool registration::make_credentials(const char *challenge) {
  parse_challenge(challenge);
  return generate_signature();
}

/**
  Set method to set user name.

  @param [in] user   buffer holding user name
*/
void registration::set_user(std::string user) {
  /*
    1, 2 parameters refer to user ID/len, which should be unique for
    a given user, else same credentials is re used in authenticator.
  */
  fido_cred_set_user(m_cred,
                     reinterpret_cast<const unsigned char *>(user.c_str()),
                     user.length(), user.c_str(), nullptr, nullptr);
}

/**
  Method to set the relying party name or id

  @param [in] rp_id   buffer holding relying party name
*/
void registration::set_rp_id(std::string rp_id) {
  fido_cred_set_rp(m_cred, rp_id.c_str(), nullptr);
}

/**
  Method to get length of authenticator data

  @retval length of authenticator data.
*/
size_t registration::get_authdata_len() {
  return fido_cred_authdata_len(m_cred);
}

/**
  Method to get authenticator data

  @retval buffer holding authenticator data
*/
const unsigned char *registration::get_authdata_ptr() {
  return fido_cred_authdata_ptr(m_cred);
}

/**
  Method to get length of signature

  @retval length of signature
*/
size_t registration::get_sig_len() { return fido_cred_sig_len(m_cred); }

/**
  Method to get signature data

  @retval buffer holding signature data
*/
const unsigned char *registration::get_sig_ptr() {
  return fido_cred_sig_ptr(m_cred);
}

/**
  Gets the full attestation statement blob
*/
const unsigned char *registration::get_attestation_statement_ptr() {
  return fido_cred_attstmt_ptr(m_cred);
}

/**
  Gets the length of the full attestation statement blob
*/
size_t registration::get_attestation_statement_length() {
  return fido_cred_attstmt_len(m_cred);
}

const char *registration::get_fmt() { return fido_cred_fmt(m_cred); }
/**
  Method to get length of x509 certificate

  @retval length of x509 certificate
*/
size_t registration::get_x5c_len() { return fido_cred_x5c_len(m_cred); }

/**
  Method to get x509 certificate

  @retval buffer holding x509 certificate
*/
const unsigned char *registration::get_x5c_ptr() {
  return fido_cred_x5c_ptr(m_cred);
}

/**
  Method to get rp id

  @retval buffer holding rp id
*/
const char *registration::get_rp_id() { return fido_cred_rp_id(m_cred); }

/**
  Method to check if token device supports CTAP2.1 resident keys feature

  @retval false   authenticator does not support resident keys
  @retval true    authenticator supports resident keys

*/
bool registration::is_fido2() { return m_is_fido2; }

/**
  Discover available devices

  @param [in] num_devices Number of devices to open

  @returns handle to fido_dev_info_t array on success. null otherwise.
*/
fido_dev_info_t *registration::discover_fido2_devices(size_t num_devices) {
  fido_dev_info_t *dev_infos = fido_dev_info_new(num_devices);
  if (!dev_infos) {
    get_plugin_messages("Failed to allocate memory for fido_dev_info_t",
                        message_type::ERROR);
    return nullptr;
  }
  auto cleanup_guard =
      create_scope_guard([&] { fido_dev_info_free(&dev_infos, num_devices); });
  size_t olen = 0;
  (void)fido_dev_info_manifest(dev_infos, num_devices, &olen);
  if (olen == 0) {
    get_plugin_messages("No FIDO device available on client host.",
                        message_type::ERROR);
    return nullptr;
  }

  if (num_devices > olen) {
    std::stringstream error;
    error << "Requested FIDO device '" << num_devices - 1
          << "' not present. Please correct the device id supplied or make "
             "sure the device is present.";
    get_plugin_messages(error.str(), message_type::ERROR);
    return nullptr;
  }

  cleanup_guard.release();
  return dev_infos;
}