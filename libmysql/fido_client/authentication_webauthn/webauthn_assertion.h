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

#ifndef WEBAUTHN_ASSERTION_H_
#define WEBAUTHN_ASSERTION_H_

#include <string>

#include <assertion.h>

/**
   Class to initiate authentication(aka assertion in FIDO terminology) on
   client side by generating a signature by FIDO device which needs to be
   sent to server to be verified using public key stored in auth_string.
*/
class webauthn_assertion : public client_authentication::assertion {
 public:
  webauthn_assertion(bool preserve_privacy)
      : m_client_data_json{}, m_preserve_privacy{preserve_privacy} {}
  bool get_signed_challenge(unsigned char **challenge_res,
                            size_t &challenge_res_len) override;
  void set_client_data(const unsigned char *, const char *) override;
  bool sign_challenge() override;
  bool parse_challenge(const unsigned char *challenge) override;
  bool check_fido2_device(bool &is_fido2);
  size_t get_client_data_json_len();
  std::string get_client_data_json();
  bool select_credential_id();

 private:
  size_t calculate_client_response_length();

 private:
  std::string m_client_data_json;
  bool m_preserve_privacy;
};

extern unsigned int libfido_device_id;

#endif  // WEBAUTHN_ASSERTION_H_
