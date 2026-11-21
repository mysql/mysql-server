/* Copyright (c) 2026, Amazon.com, Inc. or its affiliates. All rights reserved.

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

#ifndef VIO_SIGALGS_H
#define VIO_SIGALGS_H

/**
  @file vio/vio_sigalgs.h

  Header-only helpers for TLS signature-algorithm list filtering.

  OpenSSL's SSL_CTX_set1_sigalgs_list() is all-or-nothing: a single
  unrecognised token causes the entire call to fail with no indication
  of which token was rejected.  These helpers let the caller probe
  each token individually and reassemble the survivors while preserving
  the original preference order.

  The floor predicate enforces the mandatory-to-implement (MTI) set
  from RFC 8446 section 9.1 so that a server cannot silently degrade
  to a TLS context that lacks any viable signature algorithm.

  Design notes:
  - SHA-224 is absent from the TLS 1.3 signature-algorithm registry
    and from MySQL's own FIPS list (tls_sigalgs_fips omits SHA224).
    Dropping ECDSA+SHA224 / RSA+SHA224 tokens is therefore harmless.
  - SHA-1 signature algorithms are deprecated by RFC 9155.
  - The floor set covers both the IANA native names and their OpenSSL
    shorthand equivalents so that meets_floor() works regardless of
    which spelling the configured list uses.
*/

#include <openssl/ssl.h>  // SSL_OP_NO_TLSv1_3

#include <functional>
#include <string>
#include <vector>

namespace vio_sigalgs {

/**
  Split a colon-separated signature-algorithm list into tokens.
  Empty tokens (from leading/trailing/doubled colons) are skipped.

  @param list  Colon-separated algorithm list (e.g. "ECDSA+SHA256:RSA+SHA256")
  @return      Vector of individual tokens in original order
*/
inline std::vector<std::string> split(const std::string &list) {
  std::vector<std::string> tokens;
  std::string::size_type start = 0;
  while (start < list.size()) {
    auto end = list.find(':', start);
    if (end == std::string::npos) end = list.size();
    if (end > start) tokens.emplace_back(list, start, end - start);
    start = end + 1;
  }
  return tokens;
}

/**
  Filter a colon-separated signature-algorithm list, keeping only the
  tokens accepted by a caller-supplied predicate.  Order is preserved.

  @param list     Colon-separated input list
  @param accepts  Predicate returning true if a token is accepted
  @param[out] dropped  If non-null, receives the rejected tokens
  @return         Colon-joined survivors (empty string if none survived)
*/
inline std::string filter(
    const std::string &list,
    const std::function<bool(const std::string &)> &accepts,
    std::vector<std::string> *dropped) {
  auto tokens = split(list);
  std::string result;
  for (auto &tok : tokens) {
    if (accepts(tok)) {
      if (!result.empty()) result += ':';
      result += tok;
    } else {
      if (dropped) dropped->push_back(std::move(tok));
    }
  }
  return result;
}

/**
  Check whether a colon-separated signature-algorithm list meets the
  mandatory-to-implement (MTI) floor for the TLS 1.3 CertificateVerify
  message (RFC 8446 sections 4.4.3 and 9.1).

  The floor requires support for at least one algorithm that is valid
  for signing the CertificateVerify handshake message:
    - rsa_pss_rsae_sha256
    - ecdsa_secp256r1_sha256  (OpenSSL shorthand: ECDSA+SHA256)

  RSASSA-PKCS1-v1_5 (rsa_pkcs1_sha256 / RSA+SHA256) is deliberately
  excluded: RFC 8446 sections 4.4.3 and 9.1 permit it only for
  certificate signatures, never for CertificateVerify, so on its own it
  cannot keep a TLS 1.3 handshake alive.

  This predicate is applied only where floor_required() holds: server
  (acceptor) contexts that enable TLS 1.3.  The client connector and
  TLS 1.2-only servers are filtered but not floored.

  Both the IANA native name and the OpenSSL shorthand are accepted
  so that the predicate works regardless of which spelling appears
  in the configured list.

  @param list  Colon-separated algorithm list to check
  @return      true if the list contains at least one MTI algorithm
*/
inline bool meets_floor(const std::string &list) {
  /*
    MTI CertificateVerify algorithms and their OpenSSL shorthand
    equivalents.  Matching is exact and uses the canonical spelling of
    each token (the IANA native name plus the OpenSSL shorthand where one
    exists); there is no normalization or prefix matching.  The shipped
    preference lists (see shipped_list_*() below) are compile-time
    constants, so this set only needs to cover the spellings those
    constants actually use.  Any one match is sufficient.
  */
  static const char *const floor_names[] = {
      "rsa_pss_rsae_sha256",
      "ecdsa_secp256r1_sha256",
      "ECDSA+SHA256",
  };
  auto tokens = split(list);
  for (const auto &tok : tokens) {
    for (const auto *name : floor_names) {
      if (tok == name) return true;
    }
  }
  return false;
}

/// Floor applies to server contexts that enable TLS 1.3; a TLS 1.2-only context
/// may sign ServerKeyExchange with RSASSA-PKCS1-v1_5 (RFC 5246 §7.4.1.4.1).
inline bool floor_required(bool is_client, long ssl_ctx_flags) {
  return !is_client && !(ssl_ctx_flags & SSL_OP_NO_TLSv1_3);
}

/**
  Accessors for the signature-algorithm preference lists the server
  ships, so that tests can assert against the real constants instead of
  keeping private copies that could drift out of sync.  Each is defined
  in vio/viosslfactories.cc next to get_sigalgs_list() and returns a
  pointer to a file-local compile-time constant, or nullptr when that list
  is compiled out: the PQC list on OpenSSL < 3.5
  (OPENSSL_VERSION_NUMBER < 0x30500000L), mirroring get_sigalgs_list().
*/
const char *shipped_list_fips();
const char *shipped_list_non_fips();
const char *shipped_list_non_fips_pqc();

}  // namespace vio_sigalgs

#endif  // VIO_SIGALGS_H
