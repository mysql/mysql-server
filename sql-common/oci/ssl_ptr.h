/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#ifndef OCI_SSL_DELETERS_H
#define OCI_SSL_DELETERS_H

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>

namespace oci {
namespace ssl {
struct BIO_deleter {
  void operator()(BIO *p) const {
    if (p) BIO_free(p);
  }
};

struct X509_deleter {
  void operator()(X509 *p) const {
    if (p) X509_free(p);
  }
};

struct ASN1_TIME_deleter {
  void operator()(ASN1_TIME *p) const {
    if (p) ASN1_STRING_free(p);
  }
};
struct EVP_PKEY_deleter {
  void operator()(EVP_PKEY *p) const {
    if (p) EVP_PKEY_free(p);
  }
};

struct EVP_MD_CTX_deleter {
  void operator()(EVP_MD_CTX *p) const {
#if OPENSSL_VERSION_NUMBER > 0x10100000L
    if (p) EVP_MD_CTX_free(p);
#else
    // 1.0.x
    if (p) EVP_MD_CTX_destroy(p);
#endif
  }
};

using BIO_ptr = std::unique_ptr<BIO, BIO_deleter>;
using X509_ptr = std::unique_ptr<X509, X509_deleter>;
using ASN1_TIME_ptr = std::unique_ptr<ASN1_TIME, ASN1_TIME_deleter>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, EVP_PKEY_deleter>;
using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, EVP_MD_CTX_deleter>;

}  // namespace ssl
}  // namespace oci
#endif  // OCI_SSL_DELETERS_H
