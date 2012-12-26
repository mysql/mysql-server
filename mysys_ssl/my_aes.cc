/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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


#include <my_global.h>
#include <m_string.h>
#include <my_aes.h>

#if defined(HAVE_YASSL)
#include "aes.hpp"
#include "openssl/ssl.h"
#elif defined(HAVE_OPENSSL)
#include <openssl/aes.h>
#include <openssl/evp.h>

// Wrap C struct, to ensure resources are released.
struct MyCipherCtx
{
  MyCipherCtx() { memset(&ctx, 0, sizeof(ctx)); }
  ~MyCipherCtx() { EVP_CIPHER_CTX_cleanup(&ctx); }

  EVP_CIPHER_CTX ctx;
};
#endif

enum encrypt_dir { MY_AES_ENCRYPT, MY_AES_DECRYPT };

#define MY_AES_BLOCK_SIZE 16                    /* Block size in bytes */

/* If bad data discovered during decoding */
#define AES_BAD_DATA  -1

/**
  This is internal function just keeps joint code of Key generation

  SYNOPSIS
    my_aes_create_key()
    @param key        [in]       Key to use for real key creation
    @param key_length [in]       Length of the key
    @param rkey       [out]      Real key (used by OpenSSL/YaSSL)

  @return
    0         Ok
    -1        Error; Note: The current impementation never returns this
*/

static int my_aes_create_key(const char *key, int key_length, uint8 *rkey)
{
  uint8 *rkey_end= rkey + AES_KEY_LENGTH / 8;   /* Real key boundary */
  uint8 *ptr;                                   /* Start of the real key*/
  const char *sptr;                             /* Start of the working key */
  const char *key_end= key + key_length;        /* Working key boundary*/

  memset(rkey, 0, AES_KEY_LENGTH / 8);          /* Set initial key  */

  for (ptr= rkey, sptr= key; sptr < key_end; ptr ++, sptr ++)
  {
    if (ptr == rkey_end)
      /*  Just loop over tmp_key until we used all key */
      ptr= rkey;
    *ptr ^= (uint8) *sptr;
  }
#ifdef AES_USE_KEY_BITS
  /*
   This block is intended to allow more weak encryption if application
   build with libmysqld needs to correspond to export regulations
   It should be never used in normal distribution as does not give
   any speed improvement.
   To get worse security define AES_USE_KEY_BITS to number of bits
   you want key to be. It should be divisible by 8

   WARNING: Changing this value results in changing of enryption for
   all key lengths  so altering this value will result in impossibility
   to decrypt data encrypted with previous value
  */
#define AES_USE_KEY_BYTES (AES_USE_KEY_BITS/8)
  /*
   To get weaker key we use first AES_USE_KEY_BYTES bytes of created key
   and cyclically copy them until we created all required key length
  */
  for (ptr= rkey+AES_USE_KEY_BYTES, sptr=rkey ; ptr < rkey_end;
       ptr ++, sptr ++)
  {
    if (sptr == rkey + AES_USE_KEY_BYTES)
      sptr= rkey;
    *ptr= *sptr;
  }
#endif
  return 0;
}


/**
  Crypt buffer with AES encryption algorithm.

  SYNOPSIS
     my_aes_encrypt()
     @param source         [in]  Pointer to data for encryption
     @param source_length  [in]  Size of encryption data
     @param dest           [out] Buffer to place encrypted data (must be large enough)
     @param key            [in]  Key to be used for encryption
     @param key_length     [in]  Length of the key. Will handle keys of any length

  @return
    >= 0             Size of encrypted data
    < 0              Error
*/

int my_aes_encrypt(const char* source, int source_length, char* dest,
                   const char* key, int key_length)
{
#if defined(HAVE_YASSL)
  TaoCrypt::AES_ECB_Encryption enc;
  /* 128 bit block used for padding */
  uint8 block[MY_AES_BLOCK_SIZE];
  int num_blocks;                               /* number of complete blocks */
  int i;
#elif defined(HAVE_OPENSSL)
  MyCipherCtx ctx;
  int u_len, f_len;
#endif

  /* The real key to be used for encryption */
  uint8 rkey[AES_KEY_LENGTH / 8];
  int rc;                                       /* result codes */

  if ((rc= my_aes_create_key(key, key_length, rkey)))
    return rc;

#if defined(HAVE_YASSL)
  enc.SetKey((const TaoCrypt::byte *) rkey, MY_AES_BLOCK_SIZE);

  num_blocks = source_length / MY_AES_BLOCK_SIZE;

  for (i = num_blocks; i > 0; i--)              /* Encode complete blocks */
  {
    enc.Process((TaoCrypt::byte *) dest, (const TaoCrypt::byte *) source,
                MY_AES_BLOCK_SIZE);
    source += MY_AES_BLOCK_SIZE;
    dest += MY_AES_BLOCK_SIZE;
  }

  /* Encode the rest. We always have incomplete block */
  char pad_len = MY_AES_BLOCK_SIZE - (source_length -
                                      MY_AES_BLOCK_SIZE * num_blocks);
  memcpy(block, source, 16 - pad_len);
  memset(block + MY_AES_BLOCK_SIZE - pad_len, pad_len,  pad_len);

  enc.Process((TaoCrypt::byte *) dest, (const TaoCrypt::byte *) block,
              MY_AES_BLOCK_SIZE);

  return MY_AES_BLOCK_SIZE * (num_blocks + 1);
#elif defined(HAVE_OPENSSL)
  if (! EVP_EncryptInit(&ctx.ctx, EVP_aes_128_ecb(),
                        (const unsigned char *) rkey, NULL))
    return AES_BAD_DATA;                        /* Error */
  if (! EVP_EncryptUpdate(&ctx.ctx, (unsigned char *) dest, &u_len,
                          (unsigned const char *) source, source_length))
    return AES_BAD_DATA;                        /* Error */
  if (! EVP_EncryptFinal(&ctx.ctx, (unsigned char *) dest + u_len, &f_len))
    return AES_BAD_DATA;                        /* Error */

  return u_len + f_len;
#endif
}


/**
  DeCrypt buffer with AES encryption algorithm.

  SYNOPSIS
    my_aes_decrypt()
    @param source        [in]   Pointer to data for decryption
    @param source_length [in]   Size of encrypted data
    @param dest          [out]  Buffer to place decrypted data (must
                                be large enough)
    @param key           [in]   Key to be used for decryption
    @param key_length    [in]   Length of the key. Will handle keys of any length

  @return
    >= 0             Size of encrypted data
    < 0              Error
*/

int my_aes_decrypt(const char *source, int source_length, char *dest,
                   const char *key, int key_length)
{
#if defined(HAVE_YASSL)
  TaoCrypt::AES_ECB_Decryption dec;
  /* 128 bit block used for padding */
  uint8 block[MY_AES_BLOCK_SIZE];
  int num_blocks;                               /* Number of complete blocks */
  int i;
#elif defined(HAVE_OPENSSL)
  MyCipherCtx ctx;
  int u_len, f_len;
#endif

  /* The real key to be used for decryption */
  uint8 rkey[AES_KEY_LENGTH / 8];
  int rc;                                       /* Result codes */

  if ((rc= my_aes_create_key(key, key_length, rkey)))
    return rc;

#if defined(HAVE_YASSL)
  dec.SetKey((const TaoCrypt::byte *) rkey, MY_AES_BLOCK_SIZE);

  num_blocks = source_length / MY_AES_BLOCK_SIZE;

  if ((source_length != num_blocks * MY_AES_BLOCK_SIZE) || num_blocks == 0 )
    /* Input size has to be even and at least one block */
    return AES_BAD_DATA;

  /* Decode all but last blocks */
  for (i = num_blocks - 1; i > 0; i--)
  {
    dec.Process((TaoCrypt::byte *) dest, (const TaoCrypt::byte *) source,
                MY_AES_BLOCK_SIZE);
    source += MY_AES_BLOCK_SIZE;
    dest += MY_AES_BLOCK_SIZE;
  }

  dec.Process((TaoCrypt::byte *) block, (const TaoCrypt::byte *) source,
              MY_AES_BLOCK_SIZE);

  /* Use last char in the block as size */
  uint pad_len = (uint) (uchar) block[MY_AES_BLOCK_SIZE - 1];

  if (pad_len > MY_AES_BLOCK_SIZE)
    return AES_BAD_DATA;
  /* We could also check whole padding but we do not really need this */

  memcpy(dest, block, MY_AES_BLOCK_SIZE - pad_len);
  return MY_AES_BLOCK_SIZE * num_blocks - pad_len;
#elif defined(HAVE_OPENSSL)
  if (! EVP_DecryptInit(&ctx.ctx, EVP_aes_128_ecb(),
                        (const unsigned char *) rkey, NULL))
    return AES_BAD_DATA;                        /* Error */
  if (! EVP_DecryptUpdate(&ctx.ctx, (unsigned char *) dest, &u_len,
                          (unsigned const char *) source, source_length))
    return AES_BAD_DATA;                        /* Error */
  if (! EVP_DecryptFinal(&ctx.ctx, (unsigned char *) dest + u_len, &f_len))
    return AES_BAD_DATA;                        /* Error */
  return u_len + f_len;
#endif
}


/**
  Get size of buffer which will be large enough for encrypted data

  SYNOPSIS
    my_aes_get_size()
    @param source_length  [in] Length of data to be encrypted

  @return
    Size of buffer required to store encrypted data
*/

int my_aes_get_size(int source_length)
{
  return MY_AES_BLOCK_SIZE * (source_length / MY_AES_BLOCK_SIZE)
    + MY_AES_BLOCK_SIZE;
}

