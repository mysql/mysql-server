/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef FIDO_CLIENT_ASSERTION_H_
#define FIDO_CLIENT_ASSERTION_H_

#include <fido.h>

namespace client_authentication {
/**
   Class to initiate authentication(aka assertion in FIDO terminology) on
   client side by generating a signed signature by FIDO device which needs
   to be sent to server to be verified.
*/
class assertion {
 public:
  assertion();
  virtual ~assertion();
  /* set credential ID */
  void set_cred_id(const unsigned char *cred, size_t len);
  /* set relying party ID */
  void set_rp_id(const char *rp_id);
  /* Get relying party ID */
  const char *get_rp_id();

  /* get method to retrieve authenticator data */
  const unsigned char *get_authdata_ptr(size_t index = 0);
  /* get method to retrieve length of authenticator data */
  size_t get_authdata_len(size_t index = 0);
  /* get method to retrieve signature */
  const unsigned char *get_signature_ptr(size_t index = 0);
  /* get method to retrieve length of signature */
  size_t get_signature_len(size_t index = 0);
  /* Number of assertions */
  size_t get_num_assertions();

  /* abstract methods to be implemented by specific plugins. */
  virtual bool get_signed_challenge(unsigned char **challenge_res,
                                    size_t &challenge_res_len) = 0;
  virtual void set_client_data(const unsigned char *, const char *) = 0;
  /* method to sign the received server challenge during authentication */
  virtual bool sign_challenge() = 0;
  /* parse challenge received from server during authentication */
  virtual bool parse_challenge(const unsigned char *challenge) = 0;

 protected:
  fido_dev_info_t *discover_fido2_devices(size_t num_devices);
  /* Abstract type to hold information during authentication */
  fido_assert_t *m_assert;
};
}  // namespace client_authentication
#endif  // FIDO_CLIENT_ASSERTION_H_
