#ifndef MY_AES_INCLUDED
#define MY_AES_INCLUDED

/* Copyright (c) 2002, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

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


/* Header file for my_aes.c */
/* Wrapper to give simple interface for MySQL to AES standard encryption */

#include "rijndael.h"

C_MODE_START

#define AES_KEY_LENGTH 128		/* Must be 128 192 or 256 */

/*
  my_aes_encrypt	- Crypt buffer with AES encryption algorithm.
  source		- Pointer to data for encryption
  source_length		- size of encryption data
  dest			- buffer to place encrypted data (must be large enough)
  key			- Key to be used for encryption
  kel_length		- Length of the key. Will handle keys of any length

  returns  - size of encrypted data, or negative in case of error.
*/

int my_aes_encrypt(const char *source, int source_length, char *dest,
		   const char *key, int key_length);

/*
  my_aes_decrypt	- DeCrypt buffer with AES encryption algorithm.
  source		- Pointer to data for decryption
  source_length		- size of encrypted data
  dest			- buffer to place decrypted data (must be large enough)
  key			- Key to be used for decryption
  kel_length		- Length of the key. Will handle keys of any length

  returns  - size of original data, or negative in case of error.
*/


int my_aes_decrypt(const char *source, int source_length, char *dest,
		   const char *key, int key_length);

/*
  my_aes_get_size - get size of buffer which will be large enough for encrypted
		    data
  source_length   -  length of data to be encrypted

  returns  - size of buffer required to store encrypted data
*/

int my_aes_get_size(int source_length);

C_MODE_END

#endif /* MY_AES_INCLUDED */
