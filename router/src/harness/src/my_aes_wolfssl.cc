/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  This is copy of mysys_ssl/my_aes_wolfssl.cc with some parts that
  we do not need removed.
  It's copied because the original file includes global my_aes_opmode_names
  which symbol is currently exposed from libmysqlclient. That is causing ODR
  violations.
  On the other hand we do not want to depend on my_aes_* functions
  being accessible from libmysqlclient, as this can change in the future.
*/

#include <aes.h>
#include <my_aes.h>
#include <sys/types.h>
#include <cstring>  // for memset
#include "my_aes_impl.h"
#include "my_inttypes.h"
#include "mysys_ssl/my_aes_impl.h"

/** AES block size is fixed to be 128 bits for CBC and ECB */
#define MY_AES_BLOCK_SIZE 16

/* keep in sync with enum my_aes_opmode in my_aes.h */
static uint my_aes_opmode_key_sizes_impl[] = {
    128 /* aes-128-ecb */, 192 /* aes-192-ecb */, 256 /* aes-256-ecb */,
    128 /* aes-128-cbc */, 192 /* aes-192-cbc */, 256 /* aes-256-cbc */,
};

bool needs_iv(enum my_aes_opmode mode) {
  switch (mode) {
    case my_aes_128_ecb:
    case my_aes_192_ecb:
    case my_aes_256_ecb:
      return false;
      break;
    default:
      return true;
      break;
  }
}

bool EncryptSetKey(Aes *ctx, const unsigned char *key, uint block_size,
                   const unsigned char *iv, my_aes_opmode mode) {
  const unsigned char nullIV[16] = {0};
  if (needs_iv(mode)) {
    if (!iv) return true;
    wc_AesSetKey(ctx, key, block_size, iv, AES_ENCRYPTION);
  } else {
    /*
     * NOT RECOMMENDED!
     * No chaining between blocks so iv is irrelavant and set to all 0's
     */
    wc_AesSetKeyDirect(ctx, key, block_size, nullIV, AES_ENCRYPTION);
  }
  return false;
}

bool DecryptSetKey(Aes *ctx, const unsigned char *key, uint block_size,
                   const unsigned char *iv, my_aes_opmode mode) {
  const unsigned char nullIV[16] = {0};
  if (needs_iv(mode)) {
    if (!iv) return true;
    wc_AesSetKey(ctx, key, block_size, iv, AES_DECRYPTION);
  } else {
    /*
     * NOT RECOMMENDED!
     * No chaining between blocks so iv is irrelavant and set to all 0's
     */
    wc_AesSetKeyDirect(ctx, key, block_size, nullIV, AES_DECRYPTION);
  }
  return false;
}

/* is called with only one block size at a time */
void EncryptProcess(Aes *ctx, unsigned char *dest, const unsigned char *source,
                    uint block_size, my_aes_opmode mode) {
  if (needs_iv(mode))
    wc_AesCbcEncrypt(ctx, dest, source, block_size);
  else
    wc_AesEncryptDirect(ctx, dest, source);
}

/* is called with only one block size at a time */
void DecryptProcess(Aes *ctx, unsigned char *dest, const unsigned char *source,
                    uint block_size, my_aes_opmode mode) {
  if (needs_iv(mode))
    wc_AesCbcDecrypt(ctx, dest, source, block_size);
  else
    wc_AesDecryptDirect(ctx, dest, source);
}

/**
  Transforms an arbitrary long key into a fixed length AES key

  AES keys are of fixed length. This routine takes an arbitrary long key
  iterates over it in AES key length increment and XORs the bytes with the
  AES key buffer being prepared.
  The bytes from the last incomplete iteration are XORed to the start
  of the key until their depletion.
  Needed since crypto function routines expect a fixed length key.

  @param [in] key               Key to use for real key creation
  @param [in] key_length        Length of the key
  @param [out] rkey             Real key (used by OpenSSL/WolfSSL)
  @param [out] opmode           encryption mode
*/

void my_aes_create_key(const unsigned char *key, uint key_length, uint8 *rkey,
                       enum my_aes_opmode opmode) {
  const uint key_size = my_aes_opmode_key_sizes_impl[opmode] / 8;
  uint8 *rkey_end;                              /* Real key boundary */
  uint8 *ptr;                                   /* Start of the real key*/
  uint8 *sptr;                                  /* Start of the working key */
  uint8 *key_end = ((uint8 *)key) + key_length; /* Working key boundary*/

  rkey_end = rkey + key_size;

  memset(rkey, 0, key_size); /* Set initial key  */

  for (ptr = rkey, sptr = (uint8 *)key; sptr < key_end; ptr++, sptr++) {
    if (ptr == rkey_end) /*  Just loop over tmp_key until we used all key */
      ptr = rkey;
    *ptr ^= *sptr;
  }
}

int my_aes_encrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest, const unsigned char *key,
                   uint32 key_length, enum my_aes_opmode mode,
                   const unsigned char *iv, bool padding) {
  Aes enc;

  /* 128 bit block used for padding */
  unsigned char block[MY_AES_BLOCK_SIZE];
  uint num_blocks; /* number of complete blocks */
  uint i;
  /* predicted real key size */
  const uint key_size = my_aes_opmode_key_sizes_impl[mode] / 8;
  /* The real key to be used for encryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  my_aes_create_key(key, key_length, rkey, mode);

  if (EncryptSetKey(&enc, rkey, key_size, iv, mode)) return MY_AES_BAD_DATA;

  num_blocks = source_length / MY_AES_BLOCK_SIZE;

  /* Encode all complete blocks */
  for (i = num_blocks; i > 0;
       i--, source += MY_AES_BLOCK_SIZE, dest += MY_AES_BLOCK_SIZE)
    EncryptProcess(&enc, dest, source, MY_AES_BLOCK_SIZE, mode);

  /* If no padding, return here */
  if (!padding) return (int)(MY_AES_BLOCK_SIZE * num_blocks);
  /*
    Re-implement standard PKCS padding for the last block.
    Pad the last incomplete data block (even if empty) with bytes
    equal to the size of extra padding stored into that last packet.
    This also means that there will always be one more block,
    even if the source data size is dividable by the AES block size.
   */
  unsigned char pad_len =
      MY_AES_BLOCK_SIZE - (source_length - MY_AES_BLOCK_SIZE * num_blocks);
  memcpy(block, source, MY_AES_BLOCK_SIZE - pad_len);
  memset(block + MY_AES_BLOCK_SIZE - pad_len, pad_len, pad_len);

  EncryptProcess(&enc, dest, block, MY_AES_BLOCK_SIZE, mode);

  /* we've added a block */
  num_blocks += 1;

  return (int)(MY_AES_BLOCK_SIZE * num_blocks);
}

int my_aes_decrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest, const unsigned char *key,
                   uint32 key_length, enum my_aes_opmode mode,
                   const unsigned char *iv, bool padding) {
  Aes dec;
  /* 128 bit block used for padding */
  uint8 block[MY_AES_BLOCK_SIZE];
  uint32 num_blocks; /* Number of complete blocks */
  int i;
  /* predicted real key size */
  const uint key_size = my_aes_opmode_key_sizes_impl[mode] / 8;
  /* The real key to be used for decryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  my_aes_create_key(key, key_length, rkey, mode);
  DecryptSetKey(&dec, rkey, key_size, iv, mode);

  num_blocks = source_length / MY_AES_BLOCK_SIZE;

  /*
    Input size has to be a multiple of the AES block size.
    And, due to the standard PKCS padding, at least one block long.
   */
  if ((source_length != num_blocks * MY_AES_BLOCK_SIZE) || num_blocks == 0)
    return MY_AES_BAD_DATA;

  /* Decode all but the last block */
  for (i = padding ? num_blocks - 1 : num_blocks; i > 0;
       i--, source += MY_AES_BLOCK_SIZE, dest += MY_AES_BLOCK_SIZE)
    DecryptProcess(&dec, dest, source, MY_AES_BLOCK_SIZE, mode);

  /* If no padding, return here. */
  if (!padding) return MY_AES_BLOCK_SIZE * num_blocks;

  /* unwarp the standard PKCS padding */
  DecryptProcess(&dec, block, source, MY_AES_BLOCK_SIZE, mode);

  /* Use last char in the block as size */
  uint8 pad_len = block[MY_AES_BLOCK_SIZE - 1];

  if (pad_len > MY_AES_BLOCK_SIZE) return MY_AES_BAD_DATA;
  /* We could also check whole padding but we do not really need this */

  memcpy(dest, block, MY_AES_BLOCK_SIZE - pad_len);
  return MY_AES_BLOCK_SIZE * num_blocks - pad_len;
}

int my_aes_get_size(uint32 source_length, my_aes_opmode) {
  return MY_AES_BLOCK_SIZE * (source_length / MY_AES_BLOCK_SIZE) +
         MY_AES_BLOCK_SIZE;
}
