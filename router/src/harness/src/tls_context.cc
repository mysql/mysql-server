/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_context.h"

#include <array>
#include <shared_mutex>
#include <string>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "my_thread.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/tls_types.h"
#include "openssl_version.h"

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
#include <openssl/core_names.h>  // OSSL_PKEY_...
#include <openssl/decoder.h>     // OSSL_DECODER...
#endif

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 1, 0)
#define RSA_bits(rsa) BN_num_bits(rsa->n)
#define DH_bits(dh) BN_num_bits(dh->p)
#endif

// type == decltype(BN_num_bits())
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
constexpr int kMinRsaKeySize{2048};
#endif

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 1, 0)
namespace {
template <class T>
struct OsslDeleter;

template <class T>
using OsslUniquePtr = std::unique_ptr<T, OsslDeleter<T>>;

template <>
struct OsslDeleter<EVP_PKEY> {
  void operator()(EVP_PKEY *pkey) { EVP_PKEY_free(pkey); }
};

template <>
struct OsslDeleter<RSA> {
  void operator()(RSA *rsa) { RSA_free(rsa); }
};
}  // namespace
#endif

/*
  OpenSSL 1.1 supports native platform threads,
  so we don't need the following callback functions.
*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
using shared_lock_t = std::shared_mutex;

typedef struct CRYPTO_dynlock_value {
  shared_lock_t lock;
} openssl_lock_t;

/* Array of locks used by openssl internally for thread synchronization.
   The number of locks is equal to CRYPTO_num_locks.
*/
static openssl_lock_t *openssl_stdlocks;

/*OpenSSL callback functions for multithreading. We implement all the functions
  as we are using our own locking mechanism.
*/
static void openssl_lock(int mode, openssl_lock_t *lock,
                         const char *file [[maybe_unused]],
                         int line [[maybe_unused]]) {
  switch (mode) {
    case CRYPTO_LOCK | CRYPTO_READ:
      lock->lock.lock_shared();
      break;
    case CRYPTO_LOCK | CRYPTO_WRITE:
      lock->lock.lock();
      break;
    case CRYPTO_UNLOCK | CRYPTO_READ:
      lock->lock.unlock_shared();
      break;
    case CRYPTO_UNLOCK | CRYPTO_WRITE:
      lock->lock.unlock();
      break;
    default:
      fprintf(stderr, "Fatal: OpenSSL interface problem (mode=0x%x)", mode);
      fflush(stderr);
      abort();
  }
}

static void openssl_lock_function(int mode, int n,

                                  const char *file [[maybe_unused]],
                                  int line [[maybe_unused]]) {
  if (n < 0 || n > CRYPTO_num_locks()) {
    fprintf(stderr, "Fatal: OpenSSL interface problem (n = %d)", n);
    fflush(stderr);
    abort();
  }
  openssl_lock(mode, &openssl_stdlocks[n], file, line);
}

static openssl_lock_t *openssl_dynlock_create(const char *file [[maybe_unused]],
                                              int line [[maybe_unused]]) {
  openssl_lock_t *lock;
  lock = new openssl_lock_t{};
  return lock;
}

static void openssl_dynlock_destroy(openssl_lock_t *lock,
                                    const char *file [[maybe_unused]],
                                    int line [[maybe_unused]]) {
  delete lock;
}

static unsigned long openssl_id_function() {
  return (unsigned long)my_thread_self();
}

static void init_ssl_locks() {
  openssl_stdlocks = new openssl_lock_t[CRYPTO_num_locks()];
}

static void deinit_ssl_locks() { delete[] openssl_stdlocks; }

static void set_lock_callback_functions(bool init) {
  CRYPTO_set_locking_callback(init ? openssl_lock_function : NULL);
  CRYPTO_set_id_callback(init ? openssl_id_function : NULL);
  CRYPTO_set_dynlock_create_callback(init ? openssl_dynlock_create : NULL);
  CRYPTO_set_dynlock_destroy_callback(init ? openssl_dynlock_destroy : NULL);
  CRYPTO_set_dynlock_lock_callback(init ? openssl_lock : NULL);
}

static void init_lock_callback_functions() {
  set_lock_callback_functions(true);
}

static void deinit_lock_callback_functions() {
  set_lock_callback_functions(false);
}

#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

TlsLibraryContext::TlsLibraryContext() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
  init_ssl_locks();
  init_lock_callback_functions();
#else
  OPENSSL_init_ssl(0, nullptr);
#endif
  SSL_load_error_strings();
  ERR_load_crypto_strings();
}

TlsLibraryContext::~TlsLibraryContext() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  CRYPTO_set_locking_callback(nullptr);
  CRYPTO_set_id_callback(nullptr);
  deinit_lock_callback_functions();
  deinit_ssl_locks();
#endif
// in case any of this is needed for cleanup
#if 0
  FIPS_mode_set(0);

  SSL_COMP_free_compression_methods();

  ENGINE_cleanup();

  CONF_modules_free();
  CONF_modules_unload(1);

  COMP_zlib_cleanup();

  ERR_free_strings();
  EVP_cleanup();

  CRYPTO_cleanup_all_ex_data();
#endif
}

TlsContext::TlsContext(const SSL_METHOD *method)
    : ssl_ctx_{SSL_CTX_new(const_cast<SSL_METHOD *>(method)), &SSL_CTX_free} {
  // SSL_CTX_new may fail if ciphers aren't loaded.
}

stdx::expected<void, std::error_code> TlsContext::ssl_ca(
    const std::string &ca_file, const std::string &ca_path) {
  if (!ssl_ctx_) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  if (1 != SSL_CTX_load_verify_locations(
               ssl_ctx_.get(), ca_file.empty() ? nullptr : ca_file.c_str(),
               ca_path.empty() ? nullptr : ca_path.c_str())) {
    return stdx::unexpected(make_tls_error());
  }
  return {};
}

stdx::expected<void, std::error_code> TlsContext::crl(
    const std::string &crl_file, const std::string &crl_path) {
  if (!ssl_ctx_) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  auto *store = SSL_CTX_get_cert_store(ssl_ctx_.get());

  if (1 != X509_STORE_load_locations(
               store, crl_file.empty() ? nullptr : crl_file.c_str(),
               crl_path.empty() ? nullptr : crl_path.c_str())) {
    return stdx::unexpected(make_tls_error());
  }

  if (1 != X509_STORE_set_flags(
               store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL)) {
    return stdx::unexpected(make_tls_error());
  }

  return {};
}

stdx::expected<void, std::error_code> TlsContext::curves_list(
    const std::string &curves) {
  if (curves.empty()) return {};

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  if (1 != SSL_CTX_set1_curves_list(ssl_ctx_.get(),
                                    const_cast<char *>(curves.c_str()))) {
    return stdx::unexpected(make_tls_error());
  }
  return {};
#else
  return stdx::unexpected(make_error_code(std::errc::function_not_supported));
#endif
}

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
static int o11x_version(TlsVersion version) {
  switch (version) {
    case TlsVersion::AUTO:
      return 0;
    case TlsVersion::SSL_3:
      return SSL3_VERSION;
    case TlsVersion::TLS_1_0:
      return TLS1_VERSION;
    case TlsVersion::TLS_1_1:
      return TLS1_1_VERSION;
    case TlsVersion::TLS_1_2:
      return TLS1_2_VERSION;
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
    case TlsVersion::TLS_1_3:
      return TLS1_3_VERSION;
#endif
    default:
      throw std::invalid_argument("version out of range");
  }
}
#endif

stdx::expected<void, std::error_code> TlsContext::version_range(
    TlsVersion min_version, TlsVersion max_version) {
// set min TLS version
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  if (1 != SSL_CTX_set_min_proto_version(ssl_ctx_.get(),
                                         o11x_version(min_version))) {
    return stdx::unexpected(make_tls_error());
  }
  if (1 != SSL_CTX_set_max_proto_version(ssl_ctx_.get(),
                                         o11x_version(max_version))) {
    return stdx::unexpected(make_tls_error());
  }
#else
  // disable all by default
  long opts = SSL_CTX_clear_options(
      ssl_ctx_.get(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                          SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2);
  switch (min_version) {
    default:
      // unknown, leave all disabled
      [[fallthrough]];
    case TlsVersion::TLS_1_3:
      opts |= SSL_OP_NO_TLSv1_2;
      [[fallthrough]];
    case TlsVersion::TLS_1_2:
      opts |= SSL_OP_NO_TLSv1_1;
      [[fallthrough]];
    case TlsVersion::TLS_1_1:
      opts |= SSL_OP_NO_TLSv1;
      [[fallthrough]];
    case TlsVersion::TLS_1_0:
      opts |= SSL_OP_NO_SSLv3;
      [[fallthrough]];
    case TlsVersion::AUTO:
    case TlsVersion::SSL_3:
      opts |= SSL_OP_NO_SSLv2;
      break;
  }

  switch (max_version) {
    case TlsVersion::SSL_3:
      opts |= SSL_OP_NO_TLSv1;
      [[fallthrough]];
    case TlsVersion::TLS_1_0:
      opts |= SSL_OP_NO_TLSv1_1;
      [[fallthrough]];
    case TlsVersion::TLS_1_1:
      opts |= SSL_OP_NO_TLSv1_2;
      [[fallthrough]];
    default:
      break;
  }

  // returns the updated options
  SSL_CTX_set_options(ssl_ctx_.get(), opts);

#endif
  return {};
}

TlsVersion TlsContext::min_version() const {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
  switch (auto v = SSL_CTX_get_min_proto_version(ssl_ctx_.get())) {
    case SSL3_VERSION:
      return TlsVersion::SSL_3;
    case TLS1_VERSION:
      return TlsVersion::TLS_1_0;
    case TLS1_1_VERSION:
      return TlsVersion::TLS_1_1;
    case TLS1_2_VERSION:
      return TlsVersion::TLS_1_2;
    case TLS1_3_VERSION:
      return TlsVersion::TLS_1_3;
    case 0:
      return TlsVersion::AUTO;
    default:
      throw std::invalid_argument("unknown min-proto-version: " +
                                  std::to_string(v));
  }
#else
  auto opts = SSL_CTX_get_options(ssl_ctx_.get());

  if (opts & SSL_OP_NO_SSLv3) {
    if (opts & SSL_OP_NO_TLSv1) {
      if (opts & SSL_OP_NO_TLSv1_1) {
        if (opts & SSL_OP_NO_TLSv1_2) {
          return TlsVersion::TLS_1_3;
        } else {
          return TlsVersion::TLS_1_2;
        }
      } else {
        return TlsVersion::TLS_1_1;
      }
    } else {
      return TlsVersion::TLS_1_0;
    }
  } else {
    return TlsVersion::SSL_3;
  }
#endif
}

std::vector<std::string> TlsContext::cipher_list() const {
  // dump the cipher-list we actually have - using the SSL_* functions
  mysql_harness::Ssl ssl{SSL_new(ssl_ctx_.get())};

  std::vector<std::string> out;
  int prio = 0;
  while (auto cipher = SSL_get_cipher_list(ssl.get(), prio++)) {
    out.emplace_back(cipher);
  }

  return out;
}

void TlsContext::info_callback(TlsContext::InfoCallback cb) {
  SSL_CTX_set_info_callback(ssl_ctx_.get(), cb);
}

TlsContext::InfoCallback TlsContext::info_callback() const {
  return SSL_CTX_get_info_callback(ssl_ctx_.get());
}

/**
 * get the key size of an RSA key.
 *
 * @param x509 a non-null pointer to RSA-key wrapped in a X509 struct.
 *
 * @returns a key-size of RSA key on success, a std::error_code on failure.
 */
[[maybe_unused]]  // unused with openssl == 1.0.1 (RHEL6)
static stdx::expected<int, std::error_code>
get_rsa_key_size(X509 *x509) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  EVP_PKEY *public_key = X509_get0_pubkey(x509);
#else
  // if X509_get0_pubkey() isn't available, fall back to X509_get_pubkey() which
  // increments the ref-count.
  OsslUniquePtr<EVP_PKEY> public_key_storage(X509_get_pubkey(x509));

  EVP_PKEY *public_key = public_key_storage.get();
#endif
  if (public_key == nullptr) {
    return stdx::unexpected(make_error_code(TlsCertErrc::kNotACertificate));
  }

  if (EVP_PKEY_base_id(public_key) != EVP_PKEY_RSA) {
    return stdx::unexpected(make_error_code(TlsCertErrc::kNoRSACert));
  }

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
  int key_bits;
  if (!EVP_PKEY_get_int_param(public_key, OSSL_PKEY_PARAM_BITS, &key_bits)) {
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  return key_bits;
#else
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  RSA *rsa_key = EVP_PKEY_get0_RSA(public_key);
#else
  // if EVP_PKEY_get0_RSA() isn't available, fall back to EVP_PKEY_get1_RSA()
  // which increments the ref-count.
  OsslUniquePtr<RSA> rsa_key_storage(EVP_PKEY_get1_RSA(public_key));

  RSA *rsa_key = rsa_key_storage.get();
#endif
  if (!rsa_key) {
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
  return RSA_bits(rsa_key);
#endif
}

stdx::expected<void, std::error_code> TlsContext::load_key_and_cert(
    const std::string &private_key_file, const std::string &cert_chain_file) {
  // load cert and key
  if (!cert_chain_file.empty()) {
    if (1 != SSL_CTX_use_certificate_chain_file(ssl_ctx_.get(),
                                                cert_chain_file.c_str())) {
      return stdx::unexpected(make_tls_error());
    }
  }
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  // openssl 1.0.1 has no SSL_CTX_get0_certificate() and doesn't allow
  // to access ctx->cert->key->x509 as cert_st is opaque to us.

  // internal pointer, don't free
  if (X509 *x509 = SSL_CTX_get0_certificate(ssl_ctx_.get())) {
    auto key_size_res = get_rsa_key_size(x509);
    if (!key_size_res) {
      auto ec = key_size_res.error();

      if (ec != TlsCertErrc::kNoRSACert) {
        return stdx::unexpected(key_size_res.error());
      }

      // if it isn't a RSA Key ... just continue.
    } else {
      const auto key_size = *key_size_res;

      if (key_size < kMinRsaKeySize) {
        return stdx::unexpected(
            make_error_code(TlsCertErrc::kRSAKeySizeToSmall));
      }
    }
  } else {
    // doesn't exist
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
#endif
  if (1 != SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(), private_key_file.c_str(),
                                       SSL_FILETYPE_PEM)) {
    return stdx::unexpected(make_tls_error());
  }
  if (1 != SSL_CTX_check_private_key(ssl_ctx_.get())) {
    return stdx::unexpected(make_tls_error());
  }

  return {};
}

int TlsContext::security_level() const {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  return SSL_CTX_get_security_level(ssl_ctx_.get());
#else
  return 0;
#endif
}

long TlsContext::session_cache_hits() const {
  return SSL_CTX_sess_hits(ssl_ctx_.get());
}
