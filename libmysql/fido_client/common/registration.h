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

#ifndef FIDO_CLIENT_REGISTRATION_H_
#define FIDO_CLIENT_REGISTRATION_H_

#include <fido.h>
#include <string>

/**
  Capability bit to support resident keys(aka discoverable credentials)
*/
#define RESIDENT_KEYS 1

namespace client_registration {
/**
  This class is used to perform registration step on client side.
*/
class registration {
 public:
  registration();
  virtual ~registration();
  bool make_credentials(const char *challenge);
  /* set rp id */
  void set_rp_id(std::string rp_id);
  /* set user name */
  void set_user(std::string user);

  /* get authenticator data details */
  size_t get_authdata_len();
  const unsigned char *get_authdata_ptr();
  /* get signature details */
  size_t get_sig_len();
  const unsigned char *get_sig_ptr();
  /* get x509 certificate details */
  size_t get_x5c_len();
  const unsigned char *get_x5c_ptr();
  /* get rp id */
  const char *get_rp_id();
  /* check if authenticator has resident keys support */
  bool is_fido2();

  /* abstract methods to be implemented by specific client plugins */
  virtual bool parse_challenge(const char *challenge) = 0;
  virtual bool make_challenge_response(unsigned char *&buf) = 0;
  virtual void set_client_data(const unsigned char *, const char *) = 0;
  /*
    Helper method to open the device and request the device to
    generate a signature, authenticator data and x509 certificate.
  */
  virtual bool generate_signature() = 0;

 protected:
  fido_dev_info_t *discover_fido2_devices(size_t num_devices);
  /* An abstraction to hold FIDO credentials. */
  fido_cred_t *m_cred;
  bool m_is_fido2{false};
};
}  // namespace client_registration
#endif  // FIDO_CLIENT_REGISTRATION_H_
