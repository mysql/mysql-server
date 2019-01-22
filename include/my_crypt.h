/*
 Copyright (c) 2017 Percona
 Copyright (c) 2014 Google Inc.
 Copyright (c) 2014, 2015 MariaDB Corporation

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_CRYPT_INCLUDED
#define MY_CRYPT_INCLUDED

#include "my_aes.h"
#include "my_compiler.h"
#include "my_config.h"

#define MY_AES_OK 0
#define MY_AES_OPENSSL_ERROR -101
#define MY_AES_BAD_KEYSIZE -102

#define ENCRYPTION_FLAG_DECRYPT 0
#define ENCRYPTION_FLAG_ENCRYPT 1
#define ENCRYPTION_FLAG_NOPAD 2

enum class my_aes_mode {
  ECB,
  CBC
#ifdef HAVE_EncryptAes128Ctr
  ,
  CTR
#endif
#ifdef HAVE_EncryptAes128Gcm
  ,
  GCM
#endif
};

class MyEncryptionCTX;

int my_aes_crypt_init(MyEncryptionCTX *&ctx, enum my_aes_mode mode, int flags,
                      const unsigned char *key, size_t klen,
                      const unsigned char *iv, size_t ivlen)
    MY_ATTRIBUTE((warn_unused_result));
int my_aes_crypt_update(
    MyEncryptionCTX *ctx, const unsigned char *src, size_t slen,
    unsigned char *dst,
    size_t *dlen) noexcept MY_ATTRIBUTE((warn_unused_result));
int my_aes_crypt_finish(MyEncryptionCTX *&ctx, uchar *dst, size_t *dlen)
    MY_ATTRIBUTE((warn_unused_result));
void my_aes_crypt_free_ctx(MyEncryptionCTX *ctx) noexcept;

int my_aes_crypt(enum my_aes_mode mode, int flags, const unsigned char *src,
                 size_t slen, unsigned char *dst, size_t *dlen,
                 const unsigned char *key, size_t klen, const unsigned char *iv,
                 size_t ivlen) MY_ATTRIBUTE((warn_unused_result));

int my_random_bytes(unsigned char *buf,
                    int num) noexcept MY_ATTRIBUTE((warn_unused_result));
size_t my_aes_crypt_get_size(
    enum my_aes_mode mode,
    size_t source_length) noexcept MY_ATTRIBUTE((warn_unused_result));

#endif /* MY_CRYPT_INCLUDED */
