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

#ifndef FIDO_REGISTRATION_H_
#define FIDO_REGISTRATION_H_

#include <fido.h>

/**
   A wrapper class which abstracts all access to FIDO device.
*/
class fido_make_cred {
 public:
  fido_make_cred();
  ~fido_make_cred();
  /* prepare credential */
  bool make_credentials(const char *challenge);
  bool make_challenge_response(unsigned char *&challenge_response);

 private:
  void set_rp_id(std::string rp_id);
  void set_type(int type = COSE_ES256);
  void set_user(std::string user);
  void set_scramble(unsigned char *, size_t);

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

  /* Helper method to parse challenge receviced from server */
  bool parse_challenge(const char *challenge);
  /*
    Helper method to open the device and request the device to
    generate a signature, authenticator data and x509 certificate.
  */
  bool generate_signature();

 private:
  /* An abstraction to hold FIDO credentials. */
  fido_cred_t *m_cred;
};

/**
  This class is used to perform registration step on client side.
*/
class fido_registration {
 public:
  bool make_credentials(const char *challenge);
  bool make_challenge_response(unsigned char *&buf);

 private:
  fido_make_cred m_fido_make_cred;
};

#endif  // FIDO_REGISTRATION_H_