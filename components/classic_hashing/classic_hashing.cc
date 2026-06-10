/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

/**
  @brief

  classic_hashing component exposes the MD5, SHA1 and SHA UDFs that
  generates the respective hashes.
*/

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/opensslv.h>
#include <cassert>
#include <cstdio>
#include "my_compiler.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/bits/my_err_bits.h"
#include "mysql/components/services/udf_metadata.h"
#include "mysql/components/services/udf_registration.h"

#if OPENSSL_VERSION_NUMBER < 0x10002000L
#include <openssl/ec.h>
#endif /* OPENSSL_VERSION_NUMBER < 0x10002000L */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
// IWYU pragma: no_include <openssl/types.h>
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

#if OPENSSL_VERSION_NUMBER < 0x30000000L
#include <openssl/crypto.h>
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

static REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
static REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

static bool check_init_common(UDF_INIT *initd, UDF_ARGS *args, char *message) {
  if (args->arg_count != 1) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "Wrong number of arguments: %d, should be 1", args->arg_count);
    return true;
  }
  if (args->arg_type[0] != STRING_RESULT) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "Wrong argument type: %d, should be string", args->arg_type[0]);
    return true;
  }

  const char *name = "ascii";
  char *value = const_cast<char *>(name);
  SERVICE_PLACEHOLDER(mysql_udf_metadata)
      ->result_set(initd, "charset", static_cast<void *>(value));
  initd->maybe_null = true;
  args->maybe_null[0] = 1;
  return false;
}

static inline void local_array_to_hex(char *to, const unsigned char *str,
                                      unsigned len) {
  static const char *hex_lower = "0123456789abcdef";
  for (unsigned i = 0; i < len; ++i) {
    const unsigned offset = 2 * i;
    to[offset] = hex_lower[str[i] >> 4];
    to[offset + 1] = hex_lower[str[i] & 0x0F];
  }
}

namespace fips {
static int fips_mode = 0;

static void read_mode() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  fips_mode = EVP_default_properties_is_fips_enabled(nullptr) &&
              OSSL_PROVIDER_available(nullptr, "fips");
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  fips_mode = FIPS_mode();
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}
}  // namespace fips

namespace sha {

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static EVP_MD *md_sha1 = nullptr;
#endif

static const EVP_MD *my_EVP_sha1() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return md_sha1 ? md_sha1 : EVP_sha1();
#else
  return EVP_sha1();
#endif
}

static void init() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  // warn if cache loaded multiple times
  assert(md_sha1 == nullptr);

  md_sha1 = EVP_MD_fetch(nullptr, "sha1", nullptr);
  assert(md_sha1 != nullptr);
  // do not propagate possible errors
  ERR_clear_error();
#endif
}

static void deinit() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MD_free(md_sha1);
  md_sha1 = nullptr;
  // do not propagate possible errors
  ERR_clear_error();
#endif
}

#define SHA1_HASH_SIZE 20 /* Hash size in bytes */

/**
  Wrapper function to compute SHA1 message digest.

  @param [out] digest  Computed SHA1 digest
  @param [in] buf      Message to be computed
  @param [in] len      Length of the message
*/
static void compute_sha1_hash(unsigned char *digest, const char *buf,
                              size_t len) {
  EVP_MD_CTX *sha1_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(sha1_context, my_EVP_sha1(), nullptr);
  EVP_DigestUpdate(sha1_context, buf, len);
  EVP_DigestFinal_ex(sha1_context, digest, nullptr);
  EVP_MD_CTX_destroy(sha1_context);
  sha1_context = nullptr;
}

static bool check_init(UDF_INIT *initd, UDF_ARGS *args, char *message) {
  initd->max_length = SHA1_HASH_SIZE * 2;
  return check_init_common(initd, args, message);
}

static char *udf(UDF_INIT *initid [[maybe_unused]], UDF_ARGS *args,
                 char *result, unsigned long *length, unsigned char *null_value,
                 unsigned char *error [[maybe_unused]]) {
  // if the argument is a null, return a NULL
  if (args->args[0] == nullptr) {
    *null_value = 1;
    return nullptr;
  }

  const char *arg = args->args[0];
  unsigned long arg_length = args->lengths[0];
  /* Temporary buffer to store 160bit digest */
  unsigned char digest[SHA1_HASH_SIZE];
  compute_sha1_hash(digest, arg, arg_length);
  /* Ensure that memory is free */
  local_array_to_hex(result, digest, SHA1_HASH_SIZE);
  *length = SHA1_HASH_SIZE * 2;
  *null_value = 0;
  return result;
}
}  // namespace sha

namespace md5 {

#define MD5_HASH_SIZE 16

// returns 1 for success and 0 for failure
static int md5_hash(unsigned char *digest, const char *buf, size_t len) {
  // OpenSSL3.x EVP API is 1.5x-4x slower than this (deprecated) API
  // (depending if caching algorithm pointer etc.)
  // Issue reported at: https://github.com/openssl/openssl/issues/25858
  MY_COMPILER_DIAGNOSTIC_PUSH()
  MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")
  MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")
  MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4996)

  if (::fips::fips_mode != 0) return 0;
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, buf, len);
  return MD5_Final(digest, &ctx);

  // restore clang/gcc checks for -Wdeprecated-declarations
  MY_COMPILER_DIAGNOSTIC_POP()
}

static char *udf(UDF_INIT *initid [[maybe_unused]], UDF_ARGS *args,
                 char *result, unsigned long *length, unsigned char *null_value,
                 unsigned char *error [[maybe_unused]]) {
  // if the argument is a null, return a NULL
  if (args->args[0] == nullptr) {
    *null_value = 1;
    return nullptr;
  }

  const char *arg = args->args[0];
  unsigned long arg_length = args->lengths[0];
  unsigned char digest[MD5_HASH_SIZE] = {0};

  if (0 == md5_hash(digest, arg, arg_length)) {
    *null_value = 1;
    return nullptr;
  }

  local_array_to_hex(result, digest, MD5_HASH_SIZE);
  *length = MD5_HASH_SIZE * 2;
  *null_value = 0;
  return result;
}

static bool check_init(UDF_INIT *initd, UDF_ARGS *args, char *message) {
  initd->max_length = MD5_HASH_SIZE * 2;
  return check_init_common(initd, args, message);
}

}  // namespace md5

/**
  Component initialization.
*/
static mysql_service_status_t init() {
  fips::read_mode();
  sha::init();
  return mysql_service_udf_registration->udf_register(
             "md5", STRING_RESULT, (Udf_func_any)md5::udf, md5::check_init,
             nullptr) != 0 ||
                 mysql_service_udf_registration->udf_register(
                     "sha", STRING_RESULT, (Udf_func_any)sha::udf,
                     sha::check_init, nullptr) != 0 ||
                 mysql_service_udf_registration->udf_register(
                     "sha1", STRING_RESULT, (Udf_func_any)sha::udf,
                     sha::check_init, nullptr) != 0
             ? 1
             : 0;
}

/**
  Component deinitialization.
*/
static mysql_service_status_t deinit() {
  int was_present = 0;
  if ((mysql_service_udf_registration->udf_unregister("md5", &was_present) +
       mysql_service_udf_registration->udf_unregister("sha", &was_present) +
       mysql_service_udf_registration->udf_unregister("sha1", &was_present)) !=
      0) {
    return 1;
  }
  sha::deinit();
  return 0;
}

BEGIN_COMPONENT_PROVIDES(classic_hashing)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(classic_hashing)
REQUIRES_SERVICE(udf_registration), REQUIRES_SERVICE(mysql_udf_metadata),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(classic_hashing)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

static DECLARE_COMPONENT(classic_hashing, "classic_hashing") init, deinit
    END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(classic_hashing)
    END_DECLARE_LIBRARY_COMPONENTS
