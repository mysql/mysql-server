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

#include "assert_verification.h"
#include "my_dbug.h"

using namespace server_authentication;
/**
  Construcutor to allocate memory for new assertion.
*/
assert_verification::assert_verification() {
  m_assert = fido_assert_new();
  /* set number of assertionObjects to 1 */
  fido_assert_set_count(m_assert, 1);
}

/**
  Standard destructor
*/
assert_verification::~assert_verification() {
  /* releases the memory held by m_assert */
  fido_assert_free(&m_assert);
}

/**
  Method to set client data hash

  @param [in] cd    buffer holding client data hash
  @param [in] len   length of client data hash

  @retval false on success.
  @retval true on failure.
*/
bool assert_verification::set_client_data(const unsigned char *cd, size_t len) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* sets the client data hash in m_assert */
  int res = fido_assert_set_clientdata_hash(m_assert, cd, len);
  return (res != FIDO_OK);
}

/**
  Method to set authenticator data.

  @param [in] auth_data   buffer holding authenticator data
  @param [in] len         length of authenticator data
  @param [in] index       Assertion index

  @retval false on success.
  @retval true on failure.
*/
bool assert_verification::set_authdata(const unsigned char *auth_data,
                                       size_t len, size_t index /* =0 */) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* set authenticator data parts of the assertionObject */
  int res = fido_assert_set_authdata(m_assert, index, auth_data, len);
  return (res != FIDO_OK);
}

/**
  Method to set signature

  @param [in] sig   buffer holding signature
  @param [in] len   length of signature
  @param [in] index Assertion index

  @retval false on success.
  @retval true on failure.
*/
bool assert_verification::set_signature(const unsigned char *sig, size_t len,
                                        size_t index /* =0 */) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* set signature parts of the assertionObject */
  int res = fido_assert_set_sig(m_assert, index, sig, len);
  return (res != FIDO_OK);
}

/**
  Method to set the relying party name or id.

  @param [in] rp_id   buffer holding relying party name

  @retval false on success.
  @retval true on failure.
*/
bool assert_verification::set_rp_id(const char *rp_id) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* sets the relying party id in m_assert */
  int res = fido_assert_set_rp(m_assert, rp_id);
  return (res != FIDO_OK);
}

/**
  Method to set assertion count

  @param [in] count  Assertion count
*/
bool assert_verification::set_assertion_count(size_t count) {
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /* Set number of assertions in m_assert */
  int res = fido_assert_set_count(m_assert, count);
  return (res != FIDO_OK);
}

/**
  This method performs verification of signature.

  @param [in] alg      Algorithm used for assertion verification
  @param [in] pk       public key

  @retval false signature was valid.
  @retval true  signature was invalid.
*/
bool assert_verification::verify_signature(int alg, const unsigned char *pk) {
  DBUG_TRACE;
  DBUG_EXECUTE_IF("simulate_fido_testing", { return false; });
  /*
    verifies whether the client data hash, relying party ID, user
    presence/verification attributes of m_assert have been attested by the
    holder of the private counterpart of the public key
  */
  size_t num_assertion = fido_assert_count(m_assert);
  for (size_t num = 0; num < num_assertion; ++num) {
    int res = fido_assert_verify(m_assert, num, alg, pk);
    if (res == FIDO_OK) return false;
  }
  return true;
}
