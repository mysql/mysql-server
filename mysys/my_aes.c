/* Copyright (C) 2002 MySQL AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/*
  Implementation of AES Encryption for MySQL
  Initial version by Peter Zaitsev  June 2002
*/


#include <my_global.h>
#include <m_string.h>
#include "my_aes.h"

enum encrypt_dir { AES_ENCRYPT, AES_DECRYPT };

#define AES_BLOCK_SIZE 16	/* Block size in bytes */

#define AES_BAD_DATA  -1	/* If bad data discovered during decoding */


/* The structure for key information */
typedef struct {
  int	nr;				/* Number of rounds */
  uint32   rk[4*(AES_MAXNR + 1)];	/* key schedule */
} KEYINSTANCE;


/*
  This is internal function just keeps joint code of Key generation

  SYNOPSIS
    my_aes_create_key()
    aes_key		Address of Key Instance to be created
    direction		Direction (are we encoding or decoding)
    key			Key to use for real key creation
    key_length		Length of the key

  DESCRIPTION

  RESULT
    0	ok
    -1	Error		Note: The current impementation never returns this
*/

static int my_aes_create_key(KEYINSTANCE *aes_key,
			     enum encrypt_dir direction, const char *key,
			     int key_length)
{
  uint8 rkey[AES_KEY_LENGTH/8];	 /* The real key to be used for encryption */
  uint8 *rkey_end=rkey+AES_KEY_LENGTH/8; /* Real key boundary */
  uint8 *ptr;			/* Start of the real key*/
  const char *sptr;			/* Start of the working key */
  const char *key_end=key+key_length;	/* Working key boundary*/

  bzero((char*) rkey,AES_KEY_LENGTH/8);      /* Set initial key  */

  for (ptr= rkey, sptr= key; sptr < key_end; ptr++,sptr++)
  {
    if (ptr == rkey_end)
      ptr= rkey;  /*  Just loop over tmp_key until we used all key */
    *ptr^= (uint8) *sptr;
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
       ptr++,sptr++)
  {
    if (sptr == rkey+AES_USE_KEY_BYTES)
      sptr=rkey;
    *ptr=*sptr;   
  }      
#endif
  if (direction == AES_DECRYPT)
     aes_key->nr = rijndaelKeySetupDec(aes_key->rk, rkey, AES_KEY_LENGTH);
  else
     aes_key->nr = rijndaelKeySetupEnc(aes_key->rk, rkey, AES_KEY_LENGTH);
  return 0;
}


/*
  Crypt buffer with AES encryption algorithm.

  SYNOPSIS
     my_aes_encrypt()
     source		Pointer to data for encryption
     source_length	Size of encryption data
     dest		Buffer to place encrypted data (must be large enough)
     key		Key to be used for encryption
     key_length		Length of the key. Will handle keys of any length

  RETURN
    >= 0	Size of encrypted data
    < 0		Error
*/

int my_aes_encrypt(const char* source, int source_length, char* dest,
		   const char* key, int key_length)
{
  KEYINSTANCE aes_key;
  uint8 block[AES_BLOCK_SIZE];	/* 128 bit block used for padding */
  int rc;			/* result codes */
  int num_blocks;		/* number of complete blocks */
  char pad_len;			/* pad size for the last block */
  int i;

  if ((rc= my_aes_create_key(&aes_key,AES_ENCRYPT,key,key_length)))
    return rc;

  num_blocks = source_length/AES_BLOCK_SIZE;

  for (i = num_blocks; i > 0; i--)   /* Encode complete blocks */
  {
    rijndaelEncrypt(aes_key.rk, aes_key.nr, (const uint8*) source,
		    (uint8*) dest);
    source+= AES_BLOCK_SIZE;
    dest+= AES_BLOCK_SIZE;
  }

  /* Encode the rest. We always have incomplete block */
  pad_len = AES_BLOCK_SIZE - (source_length - AES_BLOCK_SIZE*num_blocks);
  memcpy(block, source, 16 - pad_len);
  bfill(block + AES_BLOCK_SIZE - pad_len, pad_len, pad_len);
  rijndaelEncrypt(aes_key.rk, aes_key.nr, block, (uint8*) dest);
  return AES_BLOCK_SIZE*(num_blocks + 1);
}


/*
  DeCrypt buffer with AES encryption algorithm.

  SYNOPSIS
    my_aes_decrypt()
    source		Pointer to data for decryption
    source_length	Size of encrypted data
    dest		Buffer to place decrypted data (must be large enough)
    key			Key to be used for decryption
    key_length		Length of the key. Will handle keys of any length

  RETURN
    >= 0	Size of encrypted data
    < 0		Error
*/

int my_aes_decrypt(const char *source, int source_length, char *dest,
		   const char *key, int key_length)
{
  KEYINSTANCE aes_key;
  uint8 block[AES_BLOCK_SIZE];	/* 128 bit block used for padding */
  int rc;			/* Result codes */
  int num_blocks;		/* Number of complete blocks */
  uint pad_len;			/* Pad size for the last block */
  int i;

  if ((rc=my_aes_create_key(&aes_key,AES_DECRYPT,key,key_length)))
    return rc;

  num_blocks = source_length/AES_BLOCK_SIZE;

  if ((source_length != num_blocks*AES_BLOCK_SIZE) || num_blocks ==0 )
    return AES_BAD_DATA; /* Input size has to be even and at least one block */

  for (i = num_blocks-1; i > 0; i--)   /* Decode all but last blocks */
  {
    rijndaelDecrypt(aes_key.rk, aes_key.nr, (const uint8*) source,
		    (uint8*) dest);
    source+= AES_BLOCK_SIZE;
    dest+= AES_BLOCK_SIZE;
  }

  rijndaelDecrypt(aes_key.rk, aes_key.nr, (const uint8*) source, block);
  /* Use last char in the block as size */
  pad_len = (uint) (uchar) block[AES_BLOCK_SIZE-1];

  if (pad_len > AES_BLOCK_SIZE)
    return AES_BAD_DATA;
  /* We could also check whole padding but we do not really need this */

  memcpy(dest, block, AES_BLOCK_SIZE - pad_len);
  return AES_BLOCK_SIZE*num_blocks - pad_len;
}


/*
  Get size of buffer which will be large enough for encrypted data

  SYNOPSIS
    my_aes_get_size()
    source_length		Length of data to be encrypted

 RETURN
   Size of buffer required to store encrypted data
*/

int my_aes_get_size(int source_length)
{
  return AES_BLOCK_SIZE*(source_length/AES_BLOCK_SIZE)+AES_BLOCK_SIZE;
}
