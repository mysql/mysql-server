/*
 Copyright (c) 2014 Google Inc.
 Copyright (c) 2014, 2017 MariaDB Corporation

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

#include "my_crypt.h"

#include <string.h>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <boost/core/noncopyable.hpp>

#include "my_dbug.h"

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define ERR_remove_state(X) ERR_clear_error()
#else
#define EVP_CIPHER_CTX_buf_noconst(ctx) ((ctx)->buf)
#define RAND_OpenSSL() RAND_SSLeay()
#endif

#define MAKE_AES_DISPATCHER(MODE)                         \
  static inline const EVP_CIPHER *aes_##MODE(uint klen) { \
    switch (klen) {                                       \
      case 16:                                            \
        return EVP_aes_128_##MODE();                      \
      case 24:                                            \
        return EVP_aes_192_##MODE();                      \
      case 32:                                            \
        return EVP_aes_256_##MODE();                      \
      default:                                            \
        return 0;                                         \
    }                                                     \
  }

MAKE_AES_DISPATCHER(ecb)
MAKE_AES_DISPATCHER(cbc)
#ifdef HAVE_EncryptAes128Ctr
MAKE_AES_DISPATCHER(ctr)
#endif /* HAVE_EncryptAes128Ctr */
#ifdef HAVE_EncryptAes128Gcm
MAKE_AES_DISPATCHER(gcm)
#endif

typedef const EVP_CIPHER *(*cipher_function)(uint);

static const cipher_function ciphers[] = {aes_ecb, aes_cbc
#ifdef HAVE_EncryptAes128Ctr
                                          ,
                                          aes_ctr
#ifdef HAVE_EncryptAes128Gcm
                                          ,
                                          aes_gcm
#endif
#endif
};

class MyEncryptionCTX : private boost::noncopyable {
 public:
  MyEncryptionCTX();
  virtual ~MyEncryptionCTX();

  virtual int init(const my_aes_mode mode, int encrypt, const uchar *key,
                   size_t klen, const uchar *iv, size_t ivlen) noexcept;
  virtual int update(const uchar *src, size_t slen, uchar *dst,
                     size_t *dlen) noexcept;
  virtual int finish(uchar *dst, size_t *dlen);

 protected:
  EVP_CIPHER_CTX *ctx;
};

MyEncryptionCTX::MyEncryptionCTX() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ctx = new EVP_CIPHER_CTX();
  EVP_CIPHER_CTX_init(ctx);
#else
  ctx = EVP_CIPHER_CTX_new();
#endif
}

MyEncryptionCTX::~MyEncryptionCTX() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup(ctx);
  delete ctx;
#else
  EVP_CIPHER_CTX_reset(ctx);
  EVP_CIPHER_CTX_free(ctx);
#endif
  ERR_remove_state(0);
}

int MyEncryptionCTX::init(const my_aes_mode mode, int encrypt, const uchar *key,
                          size_t klen, const uchar *iv,
                          size_t ivlen MY_ATTRIBUTE((unused))) noexcept {
  if (unlikely(!ciphers[static_cast<int>(mode)](klen)))
    return MY_AES_BAD_KEYSIZE;

  if (!EVP_CipherInit_ex(ctx, ciphers[static_cast<int>(mode)](klen), NULL, key,
                         iv, encrypt))
    return MY_AES_OPENSSL_ERROR;

  DBUG_ASSERT(EVP_CIPHER_CTX_key_length(ctx) == (int)klen);
  DBUG_ASSERT(EVP_CIPHER_CTX_iv_length(ctx) <= (int)ivlen);

  return MY_AES_OK;
}

int MyEncryptionCTX::update(const uchar *src, size_t slen, uchar *dst,
                            size_t *dlen) noexcept {
  if (!EVP_CipherUpdate(ctx, dst, (int *)dlen, src, slen))
    return MY_AES_OPENSSL_ERROR;
  return MY_AES_OK;
}

int MyEncryptionCTX::finish(uchar *dst, size_t *dlen) {
  if (!EVP_CipherFinal_ex(ctx, dst, (int *)dlen)) return MY_AES_BAD_DATA;
  return MY_AES_OK;
}

typedef MyEncryptionCTX MyEncryptionCTX_ctr;

class MyEncryptionCTX_nopad final : public MyEncryptionCTX {
 public:
  MyEncryptionCTX_nopad() : MyEncryptionCTX() {}
  virtual ~MyEncryptionCTX_nopad() {}

  int init(const my_aes_mode mode, int encrypt, const uchar *key, size_t klen,
           const uchar *iv, size_t ivlen) noexcept {
    this->key = key;
    this->klen = klen;
    this->buf_len = 0;
    if (iv) {
      memcpy(oiv, iv, ivlen);
      DBUG_ASSERT(ivlen == sizeof(oiv));
    } else {
      DBUG_ASSERT(ivlen == 0);
    }

    int res = MyEncryptionCTX::init(mode, encrypt, key, klen, iv, ivlen);
    if (res == MY_AES_OK) EVP_CIPHER_CTX_set_padding(ctx, 0);
    return res;
  }

  int update(const uchar *src, size_t slen, uchar *dst, size_t *dlen) noexcept {
    buf_len += slen;
    return MyEncryptionCTX::update(src, slen, dst, dlen);
  }

  int finish(uchar *dst, size_t *dlen) {
    buf_len %= MY_AES_BLOCK_SIZE;
    if (buf_len) {
      uchar *buf = EVP_CIPHER_CTX_buf_noconst(ctx);
      /*
        Not much we can do, block ciphers cannot encrypt data that aren't
        a multiple of the block length. At least not without padding.
        Let's do something CTR-like for the last partial block.

        NOTE this assumes that there are only buf_len bytes in the buf.
        If OpenSSL will change that, we'll need to change the implementation
        of this class too.
      */
      uchar mask[MY_AES_BLOCK_SIZE];
      size_t mlen;

      int result = my_aes_crypt(
          my_aes_mode::ECB, ENCRYPTION_FLAG_ENCRYPT | ENCRYPTION_FLAG_NOPAD,
          oiv, sizeof(mask), mask, &mlen, key, klen, nullptr, 0);
      if (result != MY_AES_OK) return result;
      DBUG_ASSERT(mlen == sizeof(mask));

      for (uint i = 0; i < buf_len; i++) dst[i] = buf[i] ^ mask[i];
    }
    *dlen = buf_len;
    return MY_AES_OK;
  }

 private:
  const uchar *key;
  uint klen, buf_len;
  uchar oiv[MY_AES_BLOCK_SIZE];
};

/*
  special implementation for GCM; to fit OpenSSL AES-GCM into the
  existing my_aes_* API it does the following:
    - IV tail (over 12 bytes) goes to AAD
    - the tag is appended to the ciphertext
*/
#ifdef HAVE_EncryptAes128Gcm

class MyEncryptionCTX_gcm final : public MyEncryptionCTX {
 public:
  MyEncryptionCTX_gcm() : MyEncryptionCTX() {}
  virtual ~MyEncryptionCTX_gcm() {}

  int init(const my_aes_mode mode, int encrypt, const uchar *key, size_t klen,
           const uchar *iv, size_t ivlen) noexcept {
    int res = MyEncryptionCTX::init(mode, encrypt, key, klen, iv, ivlen);
    int real_ivlen = EVP_CIPHER_CTX_iv_length(ctx);
    aad = iv + real_ivlen;
    aadlen = ivlen - real_ivlen;
    return res;
  }

  int update(const uchar *src, size_t slen, uchar *dst, size_t *dlen) noexcept {
    /*
      note that this GCM class cannot do streaming decryption, because
      it needs the tag (which is located at the end of encrypted data)
      before decrypting the data. it can encrypt data piecewise, like, first
      half, then the second half, but it must decrypt all at once
    */
    if (!EVP_CIPHER_CTX_encrypting(ctx)) {
      /* encrypted string must contain authenticaton tag (see MDEV-11174) */
      if (slen < MY_AES_BLOCK_SIZE) return MY_AES_BAD_DATA;
      slen -= MY_AES_BLOCK_SIZE;
      if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, MY_AES_BLOCK_SIZE,
                               (void *)(src + slen)))
        return MY_AES_OPENSSL_ERROR;
    }
    int unused;
    if (aadlen && !EVP_CipherUpdate(ctx, NULL, &unused, aad, aadlen))
      return MY_AES_OPENSSL_ERROR;
    aadlen = 0;
    return MyEncryptionCTX::update(src, slen, dst, dlen);
  }

  int finish(uchar *dst, size_t *dlen) noexcept {
    int fin;
    if (!EVP_CipherFinal_ex(ctx, dst, &fin)) return MY_AES_BAD_DATA;
    DBUG_ASSERT(fin == 0);

    if (EVP_CIPHER_CTX_encrypting(ctx)) {
      if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, MY_AES_BLOCK_SIZE,
                               dst))
        return MY_AES_OPENSSL_ERROR;
      *dlen = MY_AES_BLOCK_SIZE;
    } else
      *dlen = 0;
    return MY_AES_OK;
  }

 private:
  const uchar *aad;
  int aadlen;
};

#endif

int my_aes_crypt_init(MyEncryptionCTX *&ctx, const my_aes_mode mode, int flags,
                      const unsigned char *key, size_t klen,
                      const unsigned char *iv, size_t ivlen) {
#ifdef HAVE_EncryptAes128Ctr
#ifdef HAVE_EncryptAes128Gcm
  if (mode == MY_AES_GCM)
    if (flags & ENCRYPTION_FLAG_NOPAD)
      return MY_AES_OPENSSL_ERROR;
    else
      ctx = new MyEncryptionCTX_gcm();
  else
#endif
      if (mode == MY_AES_CTR)
    ctx = new MyEncryptionCTX_ctr();
  else
#endif
    ctx = (flags & ENCRYPTION_FLAG_NOPAD) ? new MyEncryptionCTX_nopad()
                                          : new MyEncryptionCTX();

  int ctx_init_result = ctx->init(mode, flags & 1, key, klen, iv, ivlen);
  if (ctx_init_result != MY_AES_OK) {
    delete ctx;
    ctx = NULL;
  }
  return ctx_init_result;
}

int my_aes_crypt_update(MyEncryptionCTX *ctx, const uchar *src, size_t slen,
                        uchar *dst, size_t *dlen) noexcept {
  return ctx->update(src, slen, dst, dlen);
}

void my_aes_crypt_free_ctx(MyEncryptionCTX *ctx) noexcept { delete ctx; }

int my_aes_crypt_finish(MyEncryptionCTX *&ctx, uchar *dst, size_t *dlen) {
  int res = ctx->finish(dst, dlen);
  delete ctx;
  ctx = NULL;
  return res;
}

int my_aes_crypt(const my_aes_mode mode, int flags, const uchar *src,
                 size_t slen, uchar *dst, size_t *dlen, const uchar *key,
                 size_t klen, const uchar *iv, size_t ivlen) {
  int res1, res2;
  size_t d1 = 0, d2;
  MyEncryptionCTX *ctx = NULL;
  if ((res1 = my_aes_crypt_init(ctx, mode, flags, key, klen, iv, ivlen))) {
    if (ctx != NULL) delete ctx;
    return res1;
  }
  res1 = my_aes_crypt_update(ctx, src, slen, dst, &d1);
  res2 = my_aes_crypt_finish(ctx, dst + d1, &d2);
  if (res1 || res2)
    ERR_remove_state(0); /* in case of failure clear error queue */
  else
    *dlen = d1 + d2;
  return res1 ? res1 : res2;
}

/*
  calculate the length of the cyphertext from the length of the plaintext
  for different AES encryption modes with padding enabled.
  Without padding (ENCRYPTION_FLAG_NOPAD) cyphertext has the same length
  as the plaintext
*/
size_t my_aes_crypt_get_size(enum my_aes_mode mode MY_ATTRIBUTE((unused)),
                             size_t source_length) noexcept {
#ifdef HAVE_EncryptAes128Ctr
  if (mode == MY_AES_CTR) return source_length;
#ifdef HAVE_EncryptAes128Gcm
  if (mode == MY_AES_GCM) return source_length + MY_AES_BLOCK_SIZE;
#endif
#endif
  return (source_length / MY_AES_BLOCK_SIZE + 1) * MY_AES_BLOCK_SIZE;
}

int my_random_bytes(uchar *buf, int num) noexcept {
  /*
    Unfortunately RAND_bytes manual page does not provide any guarantees
    in relation to blocking behavior. Here we explicitly use SSLeay random
    instead of whatever random engine is currently set in OpenSSL. That way
    we are guaranteed to have a non-blocking random.
  */
  RAND_METHOD *rand = RAND_OpenSSL();
  if (rand == nullptr || rand->bytes(buf, num) != 1)
    return MY_AES_OPENSSL_ERROR;
  return MY_AES_OK;
}
