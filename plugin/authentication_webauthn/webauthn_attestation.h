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

#ifndef WEBAUTHN_ATTESTATION_H_
#define WEBAUTHN_ATTESTATION_H_

#include <mysql/service_mysql_alloc.h>
#include <mysql_com.h> /* CHALLENGE_LENGTH* */
#include "attestation.h"

extern char *authentication_webauthn_rp_id;
extern PSI_memory_key key_memory_webauthn_attestation;
extern EVP_MD *md_sha256;
/**
  Interface to perform attestation for users created with
  authentitcation_webauthn plugin
*/
class webauthn_attestation final : public server_registration::attestation {
 public:
  explicit webauthn_attestation(int alg);
  ~webauthn_attestation() override = default;
  /* Helper method to parse challenge received from server */
  bool parse_challenge(const unsigned char *challenge,
                       unsigned int challenge_len) override;
  /* Helper method to parse challenge received from client */
  bool parse_challenge_response(const unsigned char *challenge_reponse,
                                unsigned int challenge_reponse_len) override;
  /* Helper method which constructs auth string after registration */
  bool get_key_handle(unsigned char **buf, unsigned int *buflen) override;
  void set_client_data(unsigned char *, size_t);

 private:
  unsigned char m_salt[CHALLENGE_LENGTH] = {0};
};

#endif  // WEBAUTHN_ATTESTATION_H_
