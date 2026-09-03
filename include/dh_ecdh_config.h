/*
  Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef DH_KEYS_INCLUDED
#define DH_KEYS_INCLUDED

#ifdef MYSQL_SERVER
#include "my_dbug.h"
#endif /* MYSQL_SERVER */

#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {
[[maybe_unused]] constexpr bool tls_pqc_sign_supported() {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  return true;
#else
  return false;
#endif
}

/**
  Set DH paramenter for given SSL context

  @param [in] ctx SSL context

  @return status of operation
    @retval false Success
    @retval true  Failure
*/
[[maybe_unused]] bool set_dh(SSL_CTX *ctx) {
  int security_level = 2;
  security_level = SSL_CTX_get_security_level(ctx);
#ifdef MYSQL_SERVER
  assert(security_level <= 5);
#endif /* MYSQL_SERVER */
  if (security_level < 2) security_level = 2;
#ifdef MYSQL_SERVER
  DBUG_EXECUTE_IF("crypto_policy_3", security_level = 3;);
#endif /* MYSQL_SERVER */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (SSL_CTX_set_dh_auto(ctx, 1) == 0) {
    return true;
  }
#else /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

  DH *dh = nullptr;
  switch (security_level) {
    case 1:
      [[fallthrough]];
    case 2:
      dh = DH_new_by_nid(NID_ffdhe2048);
      break;
    case 3:
      dh = DH_new_by_nid(NID_ffdhe3072);
      break;
    case 4:
      dh = DH_new_by_nid(NID_ffdhe8192);
      break;
    case 5:
      /* there is no RFC7919 approved prime for sec level 5 */
      [[fallthrough]];
    default:
      break;
  };
  if (SSL_CTX_set_tmp_dh(ctx, dh) == 0) {
    if (dh) DH_free(dh);
    return true;
  }
  DH_free(dh);

#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  return false;
}

/**
  Filter a user-configured TLS key exchange list against the internal
  allowlist and optional PQC-only restriction. Duplicate groups are ignored.

  @param [in] tls_kex   Colon-delimited TLS key exchange group list
  @param [in] force_pqc Require only PQC-capable groups
  @param [out] filtered Sanitized TLS key exchange group list

  @returns status of operation
    @retval false Success
    @retval true  Rejected group supplied or no allowlisted groups remain
*/
bool sanitize_tls_kex_list(const char *tls_kex, bool force_pqc,
                           std::string *filtered) {
  filtered->clear();
  if (tls_kex == nullptr || tls_kex[0] == '\0') return false;

  static constexpr const char *classic_groups[] = {
#ifdef NID_X25519
      "X25519",
#endif
      "secp384r1", "secp256r1", "secp521r1"};
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  static constexpr const char *pqc_groups[] = {
      "X25519MLKEM768", "secp384r1MLKEM1024", "secp256r1MLKEM768", "MLKEM512",
      "MLKEM768"};
#endif

  auto is_allowed = [](std::string_view group, bool &is_pqc_capable) {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    for (const char *candidate : pqc_groups) {
      if (group == candidate) {
        is_pqc_capable = true;
        return true;
      }
    }
#endif
    is_pqc_capable = false;
    for (const char *candidate : classic_groups) {
      if (group == candidate) return true;
    }
    return false;
  };

  std::string groups(tls_kex);
  std::vector<std::string_view> seen_groups;
  size_t begin = 0;
  while (begin <= groups.length()) {
    const size_t end = groups.find(':', begin);
    const std::string_view token{
        groups.data() + begin,
        end == std::string::npos ? groups.length() - begin : end - begin};

    bool is_pqc_capable;
    if (token.empty() || !is_allowed(token, is_pqc_capable) ||
        (force_pqc && !is_pqc_capable))
      return true;

    if (std::find(seen_groups.begin(), seen_groups.end(), token) ==
        seen_groups.end()) {
      seen_groups.push_back(token);
      if (!filtered->empty()) filtered->append(":");
      filtered->append(token);
    }

    if (end == std::string::npos) break;
    begin = end + 1;
  }

  return false;
}

/**
  Set TLS supported group details

  @param [in] ctx  SSL Context
  @param force_pqc Force Post-Quantum Crypto KEX algorithms
  @param tls_kex   Explicit TLS key exchange group list

  @returns status of operation
    @retval false Success
    @retval true  Error
*/
[[maybe_unused]] bool set_ecdh(SSL_CTX *ctx, bool force_pqc = false,
                               const char *tls_kex = nullptr) {
  std::string filtered_tls_kex;
  if (sanitize_tls_kex_list(tls_kex, force_pqc, &filtered_tls_kex)) return true;

  if (!filtered_tls_kex.empty()) {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    if (SSL_CTX_set1_groups_list(
            ctx, const_cast<char *>(filtered_tls_kex.c_str())) == 0)
      return true;
#else /* OPENSSL_VERSION_NUMBER >= 0x30500000L */
    auto group_name_to_nid = [](const std::string &group) {
#ifdef NID_X25519
      if (group == "X25519") return NID_X25519;
#endif
      if (group == "secp384r1") return NID_secp384r1;
      if (group == "secp256r1") return NID_X9_62_prime256v1;
      if (group == "secp521r1") return NID_secp521r1;
      return NID_undef;
    };

    std::vector<int> groups;
    groups.reserve(4);
    size_t begin = 0;
    while (begin <= filtered_tls_kex.length()) {
      const size_t end = filtered_tls_kex.find(':', begin);
      const std::string token =
          end == std::string::npos
              ? filtered_tls_kex.substr(begin)
              : filtered_tls_kex.substr(begin, end - begin);
      const int nid = group_name_to_nid(token);
      if (nid != NID_undef) groups.push_back(nid);
      if (end == std::string::npos) break;
      begin = end + 1;
    }

    if (groups.empty()) return true;

    if (SSL_CTX_set1_groups(ctx, groups.data(),
                            static_cast<int>(groups.size())) == 0)
      return true;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30500000L */

    return false;
  }

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  /* Add support for Post-Quantum Cryptography (PQC) TLS 1.3
   * key exchange algorithms (introduced with OpenSSL >= 3.5.0).
   *
   * The configuration must work for both standard and FIPS mode
   * (FIPS provider does not implement PQC yet), so all entries
   * here are prefixed with ? to auto-ignore missing implementations.
   *
   * It must also work with TLS 1.2, i.e. PQC is currently optional,
   * we do not enforce strict compliance yet to make the server work
   * with legacy clients.
   * At some point, we may enforce strict PQC mode compliance, phasing
   * out the legacy, less secure protocols.
   *
   * The cryptographic groups list below (used for key exchange) favors
   * hybrid PQC groups (vs pure PQC) for better compatibility and
   * performance, with fallback to classical groups for legacy clients.
   * The '/' tuple separator keeps the classical fallback below the PQC
   * tuple, even when a TLS 1.3 client predicts only a classical key share. */
  static constexpr char force_pqc_groups[] =
      "X25519MLKEM768:"     /* hybrid X25519 + ML-KEM-768 */
      "secp384r1MLKEM1024:" /* hybrid P-384  + ML-KEM-1024 */
      "secp256r1MLKEM768:"  /* hybrid P-256  + ML-KEM-768 */
      "MLKEM512:MLKEM768";  /* pure ML-KEM */
  static constexpr char default_groups[] =
      "?X25519MLKEM768:"     /* hybrid X25519 + ML-KEM-768 (if available) */
      "?secp384r1MLKEM1024:" /* hybrid P-384  + ML-KEM-1024 */
      "?secp256r1MLKEM768:"  /* hybrid P-256  + ML-KEM-768 */
      "?MLKEM512:?MLKEM768/" /* pure ML-KEM */
#ifdef NID_X25519
      "?X25519:" /* classical groups as fallback */
#endif
      "?secp384r1:"
      "?secp256r1:"
      "?secp521r1";
  static constexpr char fallback_groups[] =
#ifdef NID_X25519
      "?X25519:"
#endif
      "?secp384r1:"
      "?secp256r1:"
      "?secp521r1";

  const char *const selected_groups =
      force_pqc ? force_pqc_groups : default_groups;
  const char *const selected_fallback_groups =
      force_pqc ? nullptr : fallback_groups;

  if (SSL_CTX_set1_groups_list(ctx, const_cast<char *>(selected_groups)) == 0) {
    if (selected_fallback_groups != nullptr) {
      ERR_clear_error();
      if (SSL_CTX_set1_groups_list(
              ctx, const_cast<char *>(selected_fallback_groups)) != 0)
        return false;
    }
    return true;
  }
#else /* OPENSSL_VERSION_NUMBER >= 0x30500000L */
  if (force_pqc) return true;

  /*
    SSL_CTX_set1_groups() configures all supported TLS groups, not only EC
    curves. Include FFDHE groups so DHE cipher suites remain negotiable, but
    fall back to EC-only groups for OpenSSL builds that reject FFDHE NIDs.
  */
  int groups[] = {
#ifdef NID_X25519
      NID_X25519,
#endif
      NID_X9_62_prime256v1, NID_secp384r1, NID_secp521r1};
  int group_size = sizeof(groups) / sizeof(int);
  if (SSL_CTX_set1_groups(ctx, groups, group_size) == 0) return true;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30500000L */

  return false;
}

}  // namespace

#endif /* DH_KEYS_INCLUDED */
