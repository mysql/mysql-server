/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "my_openssl_fips.h"
#include <assert.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER < 0x10002000L
#include <openssl/ec.h>
#endif /* OPENSSL_VERSION_NUMBER < 0x10002000L */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/evp.h>
#include <openssl/provider.h>
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
/**
  Get fips mode from openssl library,

  @returns openssl current fips mode
*/
int get_fips_mode() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return EVP_default_properties_is_fips_enabled(nullptr) &&
         OSSL_PROVIDER_available(nullptr, "fips");
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  return FIPS_mode();
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static OSSL_PROVIDER *ossl_provider_fips = nullptr;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

/**
  Sets fips mode. On error the error is in the openssl error stack

  @retval 0 failure
  @retval non-0 success
*/
static int set_fips_mode_inner(int fips_mode) {
  int rc = -1;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  /* Load FIPS provider when needed. */
  if (fips_mode > 0 && nullptr == ossl_provider_fips) {
    ossl_provider_fips = OSSL_PROVIDER_load(nullptr, "fips");
    if (ossl_provider_fips == nullptr) rc = 0;
  }
  if (rc) rc = EVP_default_properties_enable_fips(nullptr, fips_mode);
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  rc = FIPS_mode_set(fips_mode);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  return rc;
}

/**
  Turns FIPs mode on or off

  @param [in]  fips_mode     0 for fips mode off, non-zero for fips mode ON
  @param [out] err_string    If fips mode set fails, err_string will have detail
                             failure reason.

  @returns openssl set fips mode errors
    @retval true     for error
    @retval false    for success
*/
bool set_fips_mode(const int fips_mode, char err_string[OPENSSL_ERROR_LENGTH]) {
  int rc = -1;
  int fips_mode_old = -1;

  if (fips_mode > 2) return true;

  fips_mode_old = get_fips_mode();
  if (fips_mode_old == fips_mode) return false;
  rc = set_fips_mode_inner(fips_mode);
  if (rc == 0) {
    /*
      For openssl libraries prior to 3.0 if OS doesn't have FIPS enabled openss
      l library and user sets FIPS mode ON, It fails with proper error.
      But in the same time it doesn't allow to
      perform any cryptographic operation. Now if FIPS mode set fails with
      error, setting old working FIPS mode value in the OpenSSL library. It will
      allow successful cryptographic operation and will not abort the server.
      For openssl 3.0 we turn the FIPs mode off for good measure.
    */
    unsigned long err_library = ERR_get_error();
    set_fips_mode_inner(fips_mode_old);
    ERR_error_string_n(err_library, err_string, OPENSSL_ERROR_LENGTH - 1);
    err_string[OPENSSL_ERROR_LENGTH - 1] = '\0';

    /* clear the rest of the error stack */
    ERR_clear_error();

    return true;
  }
  return false;
}

/**
  Toggle FIPS mode, to see whether it is available with the current SSL library.
  @retval 0 FIPS is not supported.
  @retval non-zero: FIPS is supported.
*/
int test_ssl_fips_mode(char err_string[OPENSSL_ERROR_LENGTH]) {
  unsigned test_fips_mode = get_fips_mode() == 0 ? 1 : 0;
  int ret = set_fips_mode_inner(test_fips_mode);
  unsigned long err = (ret == 0) ? ERR_get_error() : 0;

  if (err != 0) {
    ERR_error_string_n(err, err_string, OPENSSL_ERROR_LENGTH - 1);
    /* clear the rest of the error stack */
    ERR_clear_error();
  }
  return ret;
}

void fips_deinit() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (ossl_provider_fips) OSSL_PROVIDER_unload(ossl_provider_fips);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}

void fips_init() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  assert(ossl_provider_fips == nullptr);
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
}
