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

/* Enable NDB code to use OpenSSL 3 APIs unconditionally
   with any OpenSSL version starting from 1.0.2
*/

#ifndef NDB_PORTLIB_OPENSSL_COMPAT_H
#define NDB_PORTLIB_OPENSSL_COMPAT_H
#include <openssl/ssl.h>
#include "portlib/ndb_openssl_version.h"

#ifndef SSL_R_UNEXPECTED_EOF_WHILE_READING
// Macro not defined in OpenSSL 1.x headers (Brought in via ssl.h for OSSL3.0)
#define SSL_R_UNEXPECTED_EOF_WHILE_READING 294
#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L && OPENSSL_VERSION_NUMBER > 0x10002000L

EVP_PKEY *EVP_RSA_gen(unsigned int bits);
int EVP_PKEY_eq(const EVP_PKEY *a, const EVP_PKEY *b);
EVP_PKEY *EVP_EC_generate(const char *curve);

#else

#define EVP_EC_generate(curve) EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", curve)

#endif /* OPENSSL_VERSION_NUMBER */

/* These stub functions allow NDB TLS code to compile with OpenSSL 1.0.x */
#if OPENSSL_VERSION_NUMBER < NDB_TLS_MINIMUM_OPENSSL
#include <openssl/x509.h>
#include <openssl/x509v3.h>
const ASN1_INTEGER *X509_get0_serialNumber(const X509 *);

#ifndef X509_getm_notBefore

#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter
#define __NEED_STUB_ASN1_FUNCTIONS 1
int ASN1_INTEGER_get_uint64(uint64_t *, const ASN1_INTEGER *);
int ASN1_TIME_to_tm(const ASN1_TIME *, struct tm *);
const ASN1_TIME *X509_get0_notBefore(const X509 *);
const ASN1_TIME *X509_get0_notAfter(const X509 *);

#endif

EVP_PKEY *X509_get0_pubkey(X509 *);
EVP_PKEY *X509_REQ_get0_pubkey(X509_REQ *);
inline void X509_get0_signature(const ASN1_BIT_STRING **, const X509_ALGOR **,
                                const X509 *) {}
int X509_get_signature_info(X509 *, int *, int *, int *, uint32_t *);
X509_EXTENSION *X509V3_EXT_conf_nid(LHASH_OF(CONF_VALUE) *, X509V3_CTX *, int,
                                    const char *);
int EVP_PKEY_up_ref(EVP_PKEY *);
int X509_up_ref(X509 *);

inline SSL_METHOD *TLS_method() { return nullptr; }

#endif

#endif /* NDB_PORTLIB_OPENSSL_COMPAT_H */
