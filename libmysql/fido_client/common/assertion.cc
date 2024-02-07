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

#include <sstream>

#include "assertion.h"
#include "common.h"

#include <scope_guard.h>

using namespace client_authentication;

/**
  Construcutor to allocate memory for performing assertion (authentication)
*/
assertion::assertion() { m_assert = fido_assert_new(); }

/**
  Standard destructor
*/
assertion::~assertion() { fido_assert_free(&m_assert); }

/**
  Set method to set credential ID.

  @param [in] cred   buffer holding credential ID
  @param [in] len    length of credential ID
*/
void assertion::set_cred_id(const unsigned char *cred, size_t len) {
  fido_assert_allow_cred(m_assert, cred, len);
}

/**
  Method to set the relying party name or id.

  @param [in] rp_id   buffer holding relying party name
*/
void assertion::set_rp_id(const char *rp_id) {
  fido_assert_set_rp(m_assert, rp_id);
}

/**
  Method to get authenticator data
  @param [in] index  Assertion index

  @retval buffer holding authenticator data
*/
const unsigned char *assertion::get_authdata_ptr(size_t index /* = 0 */) {
  return fido_assert_authdata_ptr(m_assert, index);
}

/**
  Method to get length of authenticator data
  @param [in] index  Assertion index

  @retval length of authenticator data
*/
size_t assertion::get_authdata_len(size_t index /* = 0 */) {
  return fido_assert_authdata_len(m_assert, index);
}

/**
  Method to get signature
  @param [in] index  Assertion index

  @retval buffer holding signature data
*/
const unsigned char *assertion::get_signature_ptr(size_t index /* = 0 */) {
  return fido_assert_sig_ptr(m_assert, index);
}

/**
  Method to get length of signature
  @param [in] index  Assertion index

  @retval length of signature
*/
size_t assertion::get_signature_len(size_t index /* = 0 */) {
  return fido_assert_sig_len(m_assert, index);
}

/**
  Method to get number of assertions

  @retval Number of assertions
*/
size_t assertion::get_num_assertions() { return fido_assert_count(m_assert); }

/**
  Method to get rp id

  @retval buffer holding rp id
*/
const char *assertion::get_rp_id() { return fido_assert_rp_id(m_assert); }

/**
  Discover available devices

  Caller should always free num_devices + 1.

  @param [in] num_devices Number of devices to open

  @returns handle to fido_dev_info_t array on success. null otherwise.
*/
fido_dev_info_t *assertion::discover_fido2_devices(size_t num_devices) {
  fido_dev_info_t *dev_infos = fido_dev_info_new(num_devices + 1);
  if (!dev_infos) {
    get_plugin_messages("Failed to allocate memory for fido_dev_info_t",
                        message_type::ERROR);
    return nullptr;
  }
  auto cleanup_guard = create_scope_guard(
      [&] { fido_dev_info_free(&dev_infos, num_devices + 1); });
  size_t olen = 0;
  (void)fido_dev_info_manifest(dev_infos, num_devices + 1, &olen);
  if (olen == 0) {
    get_plugin_messages("No FIDO device available on client host.",
                        message_type::ERROR);
    return nullptr;
  }

  if (num_devices < olen) {
    std::stringstream error;
    error
        << "Expected maximum of '" << num_devices
        << "' FIDO device(s). Please unplug some of the devices and try again.";
    get_plugin_messages(error.str(), message_type::ERROR);
    return nullptr;
  }

  cleanup_guard.commit();
  return dev_infos;
}
