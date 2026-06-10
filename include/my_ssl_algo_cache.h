/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  @file include/my_ssl_algo_cache.h
*/

#ifndef INCLUDE_MY_SSL_ALGO_CACHE_H_
#define INCLUDE_MY_SSL_ALGO_CACHE_H_

#include <openssl/evp.h>  // IWYU pragma: export
// IWYU pragma: no_include <openssl/types.h>

// cache algorithm pointers to improve OpenSSL 3.x performance
// (only cached for server code, passthrough to method call on client)

const EVP_MD *my_EVP_sha1();
const EVP_MD *my_EVP_sha224();
const EVP_MD *my_EVP_sha256();
const EVP_MD *my_EVP_sha384();
const EVP_MD *my_EVP_sha512();

// DES-EDE3-CBC was deliberately not cached here
const EVP_CIPHER *my_EVP_aes_128_ecb();
const EVP_CIPHER *my_EVP_aes_128_cbc();
const EVP_CIPHER *my_EVP_aes_128_cfb1();
const EVP_CIPHER *my_EVP_aes_128_cfb8();
const EVP_CIPHER *my_EVP_aes_128_cfb128();
const EVP_CIPHER *my_EVP_aes_128_ofb();
const EVP_CIPHER *my_EVP_aes_192_ecb();
const EVP_CIPHER *my_EVP_aes_192_cbc();
const EVP_CIPHER *my_EVP_aes_192_cfb1();
const EVP_CIPHER *my_EVP_aes_192_cfb8();
const EVP_CIPHER *my_EVP_aes_192_cfb128();
const EVP_CIPHER *my_EVP_aes_192_ofb();
const EVP_CIPHER *my_EVP_aes_256_ecb();
const EVP_CIPHER *my_EVP_aes_256_cbc();
const EVP_CIPHER *my_EVP_aes_256_cfb1();
const EVP_CIPHER *my_EVP_aes_256_cfb8();
const EVP_CIPHER *my_EVP_aes_256_cfb128();
const EVP_CIPHER *my_EVP_aes_256_ofb();
const EVP_CIPHER *my_EVP_aes_256_xts();
const EVP_CIPHER *my_EVP_aes_256_wrap();
const EVP_CIPHER *my_EVP_aes_256_ctr();

void my_ssl_algorithm_cache_load();
void my_ssl_algorithm_cache_unload();
int my_get_fips_mode();

#endif  // INCLUDE_MY_SSL_ALGO_CACHE_H_
