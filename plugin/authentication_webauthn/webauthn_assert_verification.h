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

#ifndef WEBAUTHN_ASSERT_VERIFICATION_H
#define WEBAUTHN_ASSERT_VERIFICATION_H

#include <fido.h>

#include <mysql_com.h> /* CHALLENGE_LENGTH */
#include "assert_verification.h"

/**
   Class to perform authentication(aka assertion in FIDO terminology) by
   verifying the signed signature sent by FIDO device.
*/
class webauthn_assert_verification final
    : public server_authentication::assert_verification {
 public:
  explicit webauthn_assert_verification(const unsigned char *salt);
  ~webauthn_assert_verification() override = default;
  void set_client_data(const unsigned char *cd, size_t len);

 private:
  unsigned char m_salt[CHALLENGE_LENGTH] = {0};
};

#endif  // WEBAUTHN_ASSERT_VERIFICATION_H
