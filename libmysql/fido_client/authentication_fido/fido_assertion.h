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

#ifndef FIDO_ASSERTION_H_
#define FIDO_ASSERTION_H_

#include <assertion.h>
/**
   Class to initiate authentication(aka assertion in FIDO terminology) on
   client side by generating a signed signature by FIDO device which needs
   to be sent to server to be verified.
*/
class fido_assertion : public client_authentication::assertion {
 public:
  fido_assertion() = default;
  bool get_signed_challenge(unsigned char **challenge_res,
                            size_t &challenge_res_len) override;
  void set_client_data(const unsigned char *, const char *) override;
  bool sign_challenge() override;
  bool parse_challenge(const unsigned char *challenge) override;
};

#endif  // FIDO_ASSERTION_H_
