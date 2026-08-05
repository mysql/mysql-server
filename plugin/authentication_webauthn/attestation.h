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

#ifndef ATTESTATION_H_
#define ATTESTATION_H_

#include <fido.h>
#include <string>

/**
  Authenticator data depends on algorithm(RSA or ECDSA) used to generate
  credentials. In theory there is no limit, however setting a limit after
  getting some inputs from fido2 library developers.
*/
#define MAX_AUTH_DATA_LENGTH 1024
/**
  Limit the signature length to something that would accomodate all methods
*/
#define MAX_SIGNATURE_LENGTH 512

/**
 The max length of the fido attestaton blob.
 It's arbitrary
*/
#define MAX_ATTESTATION_BLOB_LENGTH 65536

/**
  Can be one of "packed", "fido-u2f", "none" or "tpm"
*/
#define MAX_FMT_STRING_LENGTH 8

enum capacity_bits {
  /**
    Capability bit to support resident keys(aka discoverable credentials)
  */
  RESIDENT_KEYS = 0x1,
  /**
    Capacity bit is to support sending the full attestation blob
  */
  SEND_FULL_ATTESTATION_BLOB = 0x2
};

namespace server_registration {
/**
  Interface to perform attestation(aka registration). In FIDO terminolody,
  attestation means creating a new public/private key pair during registration
  and getting it signed by private key that was created within FIDO device.
*/
class attestation {
 public:
  attestation(int alg, std::string format = std::string("fido-u2f"));
  virtual ~attestation();

  /* SET methods to set all context needed to verify signature */
  bool set_rp_id(std::string rp_id);
  bool set_client_data(const unsigned char *, size_t);
  /* set authenticator data details */
  bool set_authenticator_data(const unsigned char *ptr, size_t len);
  /* set signature details */
  bool set_signature(const unsigned char *ptr, size_t len);
  /* set x509 certificate details */
  bool set_x509_certificate(const unsigned char *ptr, size_t len);
  /* set format of authetnicator data */
  bool set_authenticator_data_format(const std::string fmt);
  /* turn ON/OFF flag which defines authenticator capability */
  void set_is_fido2(bool val);

  bool set_attestation_statement(const unsigned char *ptr, size_t len);

  bool set_fmt(const unsigned char *ptr, size_t len);

  /* GET methods to extract credential details */
  size_t get_cred_id_len();
  const unsigned char *get_cred_id_ptr();
  size_t get_cred_pubkey_len();
  const unsigned char *get_cred_pubkey_ptr();
  bool get_is_fido2();

  void set_is_full_attestation_blob(bool val);
  bool get_is_full_attestation_blob();

  /* method to verify attestation signature */
  bool verify_signature();

  /* helper method to initialize all context to do attestation. */
  bool prepare_credentials(const unsigned char *challenge_reponse,
                           unsigned int challenge_reponse_len,
                           const unsigned char *challenge,
                           unsigned int challenge_len);

  /* Helper method to parse challenge received from server */
  virtual bool parse_challenge(const unsigned char *challenge,
                               unsigned int challenge_len) = 0;
  virtual bool parse_challenge_response(const unsigned char *challenge_reponse,
                                        unsigned int challenge_reponse_len) = 0;
  /* method to get public key and credential ID to be saved in server */
  virtual bool get_key_handle(unsigned char **buf, unsigned int *buflen) = 0;

 private:
  /* An abstraction to hold FIDO credentials. */
  fido_cred_t *m_cred;
  bool m_is_fido2{false};
  bool m_is_full_attestation_blob{false};
};
}  // namespace server_registration
#endif  // ATTESTATION_H_
