/* Copyright (C) 2002 MySQL AB & MySQL Finland AB & TCX DataKonsult AB                                                              
                                                                                                                                    
 This program is free software; you can redistribute it and/or modify                                                             
 it under the terms of the GNU General Public License as published by                                                             
 the Free Software Foundation; either version 2 of the License, or                                                                
 (at your option) any later version.                                                                                              
                                                                                                                                
 This program is distributed in the hope that it will be useful,                                                                  
 but WITHOUT ANY WARRANTY; without even the implied warranty of                                                                   
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                                                                    
 GNU General Public License for more details.                                                                                     
			                                                                                                                                    
 You should have received a copy of the GNU General Public License                                                                
 along with this program; if not, write to the Free Software                                                                      
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */                                                     


/* MY_AES.C  Implementation of AES Encryption for MySQL */ 


#include "my_global.h"				                                                                                      
#include "m_string.h"
#include <stdio.h>
#include "my_aes.h"


#define AES_ENCRYPT 1
#define AES_DECRYPT 2

#define AES_BLOCK_SIZE 16 
           /* Block size in bytes */
	   
#define AES_BAD_DATA  -1
           /* If bad data discovered during decoding */
	  

/*  The structure for key information */
typedef struct {
  int   nr;                       /* Number of rounds */
  uint32   rk[4*(MAXNR + 1)];        /* key schedule */
} KEYINSTANCE;


 /* 
  This is internal function just keeps joint code of Key generation 
  rkey       -  Address of Key Instance to be created
  direction  -  Direction (are we encoding or decoding)
  key        -  key to use for real key creation 
  key_length -  length of the key
  
  returns    -  returns 0 on success and negative on error
 */ 
static int my_aes_create_key(KEYINSTANCE* aes_key,char direction, char* key,
                             int key_length) 
{ 
  char rkey[AES_KEY_LENGTH/8];   /* The real key to be used for encryption */
  char *ptr;                            /* Start of the real key*/   
  char *rkey_end=rkey+AES_KEY_LENGTH/8; /* Real key boundary */
  char *sptr;                           /* Start of the working key */
  char *key_end=key+key_length;         /* Working key boundary*/
   
  bzero(rkey,AES_KEY_LENGTH/8);      /* Set initial key  */
  
  for (ptr= rkey, sptr= key; sptr < key_end; ptr++,sptr++)
  {
    if (ptr == rkey_end)
      ptr= rkey;  /*  Just loop over tmp_key until we used all key */
    *ptr^= *sptr;  
  }
  if (direction==AES_DECRYPT)
     aes_key->nr = rijndaelKeySetupDec(aes_key->rk, rkey, AES_KEY_LENGTH);
  else 
     aes_key->nr = rijndaelKeySetupEnc(aes_key->rk, rkey, AES_KEY_LENGTH);
  return 0;     
}


int my_aes_encrypt(const char* source, int source_length, const char* dest,
                   const char* key, int key_length)
{
  KEYINSTANCE aes_key;
  char block[AES_BLOCK_SIZE]; /* 128 bit block used for padding */
  int rc;                    /* result codes */
  int num_blocks;            /* number of complete blocks */
  char pad_len;               /* pad size for the last block */
  int i;
  
  if ( (rc=my_aes_create_key(&aes_key,AES_ENCRYPT,key,key_length)) )
    return rc;
    
  num_blocks = source_length/AES_BLOCK_SIZE;    
  
  for (i = num_blocks; i > 0; i--)   /* Encode complete blocks */
  {
    rijndaelEncrypt(aes_key.rk, aes_key.nr, source, dest);
    source+= AES_BLOCK_SIZE;
    dest+= AES_BLOCK_SIZE;
  }
  
  /* Encode the rest. We always have incomplete block */
  pad_len = AES_BLOCK_SIZE - (source_length - AES_BLOCK_SIZE*num_blocks);   
  memcpy(block, source, 16 - pad_len);   
  bfill(block + AES_BLOCK_SIZE - pad_len, pad_len, pad_len);
  rijndaelEncrypt(aes_key.rk, aes_key.nr, block, dest);
  return AES_BLOCK_SIZE*(num_blocks + 1);    
}
 
int my_aes_decrypt(const char* source, int source_length, const char* dest,
                   const char* key, int key_length)
{
  KEYINSTANCE aes_key;
  char block[AES_BLOCK_SIZE]; /* 128 bit block used for padding */
  int rc;                    /* result codes */
  int num_blocks;            /* number of complete blocks */
  char pad_len;               /* pad size for the last block */
  int i;
  
  if ( (rc=my_aes_create_key(&aes_key,AES_DECRYPT,key,key_length)) )
    return rc;
    
  num_blocks = source_length/AES_BLOCK_SIZE;   

     
  if ( (source_length!=num_blocks*AES_BLOCK_SIZE) || num_blocks==0)
    return AES_BAD_DATA; /* Input size has to be even and at leas one block */
     
  
  for (i = num_blocks-1; i > 0; i--)   /* Decode all but last blocks */
  {
    rijndaelDecrypt(aes_key.rk, aes_key.nr, source, dest);
    source+= AES_BLOCK_SIZE;
    dest+= AES_BLOCK_SIZE;
  }
  
  rijndaelDecrypt(aes_key.rk, aes_key.nr, source, block);
  pad_len = block[AES_BLOCK_SIZE-1]; /* Just use last char in the block as size*/

  if (pad_len > AES_BLOCK_SIZE) 
    return AES_BAD_DATA;
  /* We could also check whole padding but we do not really need this */
  
  memcpy(dest, block, AES_BLOCK_SIZE - pad_len);
    
  return AES_BLOCK_SIZE*num_blocks - pad_len;	  
}
 
int my_aes_get_size(int source_length)
{  
  return AES_BLOCK_SIZE*(source_length/AES_BLOCK_SIZE)+AES_BLOCK_SIZE;
}



