/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_UTIL_TLS_KEY_ERRORS_H
#define NDB_UTIL_TLS_KEY_ERRORS_H

class TlsKeyError {
 public:
  enum code {
    no_error = 0,
    negotiation_failure = 1,
    openssl_error = 2,
    no_local_cert = 3,
    authentication_failure = 4,
    auth2_bad_socket = 5,
    auth2_no_cert = 6,
    auth2_bad_common_name = 7,
    auth2_bad_hostname = 8,
    auth2_resolver_error = 9,
    /* space for 3 more auth errors */
    verification_error = 13,
    signing_error = 14,
    lifetime_error = 15,
    password_error = 16,
    cannot_store_ca_key = 17,
    cannot_store_ca_cert = 18,
    failed_to_init_ca = 19,
    ca_key_not_found = 20,
    ca_cert_not_found = 21,
    cannot_read_ca_key = 22,
    cannot_read_ca_cert = 23,
    /* space for 3 more generic errors */
    END_GENERIC_ERRORS = 27,
    cannot_store_pending_key = 28,
    pending_key_not_found = 29,
    active_key_not_found = 30,
    cannot_read_pending_key = 31,
    cannot_read_active_key = 32,
    cannot_promote_key = 33,
    cannot_store_signing_req = 34,
    cannot_read_signing_req = 35,
    cannot_remove_signing_req = 36,
    pending_cert_fails_auth = 37,
    cannot_store_pending_cert = 38,
    pending_cert_not_found = 39,
    active_cert_not_found = 40,
    cannot_read_pending_cert = 41,
    cannot_read_active_cert = 42,
    cannot_promote_cert = 43,
    active_cert_mismatch = 44,
    active_cert_invalid = 45,
    active_cert_expired = 46,
    no_writable_dir = 47,
    END_ERRORS
  };

  static const char *message(int n) {
    if (n > 0 && n < code::END_ERRORS) return _message[n];
    return "(unknown error code)";
  }

 private:
  static constexpr const char *_message[code::END_ERRORS] = {
      "(no error)",
      "protocol negotiation failure",
      "openssl error",
      "no certificate",
      "authentication failure",
      "authorization failure: socket error",
      "authorization failure: no peer certificate",
      "authorization failure: subject common name is non-conforming",
      "authorization failure: bad hostname",
      "authorization failure: resolver error",
      "(unused code 10)",
      "(unused code 11)",
      "(unused code 12)",

      "signature verification error",
      "signing error",
      "invalid certificate lifetime parameters",
      "password error",
      "failed to store CA key file",
      "failed to store CA certificate file",
      "failed to initialize Certification Authority",
      "CA key file not found",
      "CA certificate file not found",
      "cannot read CA key file",
      "cannot read CA certificate file",
      "(unused code 24)",
      "(unused code 25)",
      "(unused code 26)",
      "(unused code 27)",  // END_GENERIC_ERRORS

      "cannot store pending key file",
      "pending key not found",
      "active key not found",
      "cannot read pending key file",
      "cannot read active key file",
      "error promoting pending key to active",
      "cannot store signing request",
      "cannot read signing request",
      "cannot remove signing request",
      "pending certificate fails authentication against active CA chain",
      "cannot store pending certificate file",
      "pending certificate not found",
      "active certificate not found",
      "cannot read pending certificate file",
      "cannot read active certificate file",
      "error promoting pending certificate to active",
      "node certificate has wrong public key",
      "active node certificate is not valid",
      "active node certificate has expired",
      "no writable directory found in TLS search path"};
};

#endif
