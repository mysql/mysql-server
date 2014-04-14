/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
#include "my_aes_impl.h"

#include "aes.hpp"
#include "openssl/ssl.h"
/** AES block size is fixed to be 128 bits for CBC and ECB */
#define MY_AES_BLOCK_SIZE 16


/* keep in sync with enum my_aes_opmode in my_aes.h */
const char *my_aes_opmode_names[]=
{
  "aes-128-ecb",
  "aes-192-ecb",
  "aes-256-ecb",
  "aes-128-cbc",
  "aes-192-cbc",
  "aes-256-cbc",
  NULL /* needed for the type enumeration */
};


/* keep in sync with enum my_aes_opmode in my_aes.h */
static uint my_aes_opmode_key_sizes_impl[]=
{
  128 /* aes-128-ecb */,
  192 /* aes-192-ecb */,
  256 /* aes-256-ecb */,
  128 /* aes-128-cbc */,
  192 /* aes-192-cbc */,
  256 /* aes-256-cbc */,
};

uint *my_aes_opmode_key_sizes= my_aes_opmode_key_sizes_impl;


template <TaoCrypt::CipherDir DIR>
class MyCipherCtx
{
public:
  MyCipherCtx(enum my_aes_opmode mode) : m_mode(mode)
  {
    switch (m_mode)
    {
    case my_aes_128_ecb:
    case my_aes_192_ecb:
    case my_aes_256_ecb:
      m_need_iv= false;
      break;
    default:
      m_need_iv= true;
      break;
    }
  }

  bool SetKey(const unsigned char *key, uint block_size,
              const unsigned char *iv)
  {
    if (m_need_iv)
    {
      if (!iv)
        return TRUE;
      cbc.SetKey(key, block_size, iv);
    }
    else
      ecb.SetKey(key, block_size);
    return false;
  }

  void Process(unsigned char *dest, const unsigned char * source,
               uint block_size)
  {
    if (m_need_iv)
      cbc.Process(dest, source, block_size);
    else
      ecb.Process(dest, source, block_size);
  }

  bool needs_iv() const
  {
    return m_need_iv;
  }

private:
  /* we initialize the two classes to avoid dynamic allocation */
  TaoCrypt::BlockCipher<DIR, TaoCrypt::AES, TaoCrypt::ECB> ecb;
  TaoCrypt::BlockCipher<DIR, TaoCrypt::AES, TaoCrypt::CBC> cbc;
  enum my_aes_opmode m_mode;
  bool m_need_iv;
};


int my_aes_encrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest,
                   const unsigned char *key, uint32 key_length,
                   enum my_aes_opmode mode, const unsigned char *iv)
{
  MyCipherCtx<TaoCrypt::ENCRYPTION> enc(mode);

  /* 128 bit block used for padding */
  unsigned char block[MY_AES_BLOCK_SIZE];
  uint num_blocks;                               /* number of complete blocks */
  uint i;
  /* predicted real key size */
  const uint key_size= my_aes_opmode_key_sizes[mode] / 8;
  /* The real key to be used for encryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  my_aes_create_key(key, key_length, rkey, mode);

  if (enc.SetKey(rkey, key_size, iv))
    return MY_AES_BAD_DATA;

  num_blocks= source_length / MY_AES_BLOCK_SIZE;

  /* Encode all complete blocks */
  for (i = num_blocks; i > 0;
       i--, source+= MY_AES_BLOCK_SIZE, dest+= MY_AES_BLOCK_SIZE)
       enc.Process(dest, source, MY_AES_BLOCK_SIZE);

  /*
  Re-implement standard PKCS padding for the last block.
  Pad the last incomplete data block (even if empty) with bytes
  equal to the size of extra padding stored into that last packet.
  This also means that there will always be one more block,
  even if the source data size is dividable by the AES block size.
  */
  unsigned char pad_len=
    MY_AES_BLOCK_SIZE - (source_length - MY_AES_BLOCK_SIZE * num_blocks);
  memcpy(block, source, MY_AES_BLOCK_SIZE - pad_len);
  memset(block + MY_AES_BLOCK_SIZE - pad_len, pad_len, pad_len);

  enc.Process(dest, block, MY_AES_BLOCK_SIZE);

  /* we've added a block */
  num_blocks+= 1;

  return (int) (MY_AES_BLOCK_SIZE * num_blocks);
}


int my_aes_decrypt(const unsigned char *source, uint32 source_length,
                   unsigned char *dest,
                   const unsigned char *key, uint32 key_length,
                   enum my_aes_opmode mode, const unsigned char *iv)
{
  MyCipherCtx<TaoCrypt::DECRYPTION> dec(mode);
  /* 128 bit block used for padding */
  uint8 block[MY_AES_BLOCK_SIZE];
  uint32 num_blocks;                               /* Number of complete blocks */
  int i;
  /* predicted real key size */
  const uint key_size= my_aes_opmode_key_sizes[mode] / 8;
  /* The real key to be used for decryption */
  unsigned char rkey[MAX_AES_KEY_LENGTH / 8];

  my_aes_create_key(key, key_length, rkey, mode);
  dec.SetKey(rkey, key_size, iv);

  num_blocks= source_length / MY_AES_BLOCK_SIZE;

  /*
  Input size has to be a multiple of the AES block size.
  And, due to the standard PKCS padding, at least one block long.
  */
  if ((source_length != num_blocks * MY_AES_BLOCK_SIZE) || num_blocks == 0)
    return MY_AES_BAD_DATA;

  /* Decode all but the last block */
  for (i= num_blocks - 1; i > 0;
       i--, source+= MY_AES_BLOCK_SIZE, dest+= MY_AES_BLOCK_SIZE)
       dec.Process(dest, source, MY_AES_BLOCK_SIZE);

  /* unwarp the standard PKCS padding */
  dec.Process(block, source, MY_AES_BLOCK_SIZE);

  /* Use last char in the block as size */
  uint8 pad_len = block[MY_AES_BLOCK_SIZE - 1];

  if (pad_len > MY_AES_BLOCK_SIZE)
    return MY_AES_BAD_DATA;
  /* We could also check whole padding but we do not really need this */

  memcpy(dest, block, MY_AES_BLOCK_SIZE - pad_len);
  return MY_AES_BLOCK_SIZE * num_blocks - pad_len;
}

/**
 Get size of buffer which will be large enough for encrypted data

 SYNOPSIS
  my_aes_get_size()
 @param source_length  [in] Length of data to be encrypted
 @param mode           encryption mode

 @return Size of buffer required to store encrypted data
*/

int my_aes_get_size(uint32 source_length, my_aes_opmode opmode)
{
  return MY_AES_BLOCK_SIZE * (source_length / MY_AES_BLOCK_SIZE)
    + MY_AES_BLOCK_SIZE;
}


/**
  Return true if the AES cipher and block mode requires an IV

  SYNOPSIS
  my_aes_needs_iv()
  @param mode           encryption mode

  @retval TRUE   IV needed
  @retval FALSE  IV not needed
*/

my_bool my_aes_needs_iv(my_aes_opmode opmode)
{
  MyCipherCtx<TaoCrypt::ENCRYPTION> enc(opmode);

  return enc.needs_iv() ? TRUE : FALSE;
}

