/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CRYPT_HASHGEN_IMPL_H
#define CRYPT_HASHGEN_IMPL_H
#define	ROUNDS_DEFAULT	5000
#define	ROUNDS_MIN	1000
#define	ROUNDS_MAX	ROUNDS_DEFAULT
#define	MIXCHARS	32
#define CRYPT_SALT_LENGTH  20
#define CRYPT_MAGIC_LENGTH  3
#define CRYPT_PARAM_LENGTH 13
#define SHA256_HASH_LENGTH 43
#define CRYPT_MAX_PASSWORD_SIZE (CRYPT_SALT_LENGTH + \
                                 SHA256_HASH_LENGTH + \
                                 CRYPT_MAGIC_LENGTH + \
                                 CRYPT_PARAM_LENGTH)

#define MAX_PLAINTEXT_LENGTH 256

#include <stddef.h>
#include <my_global.h>

int extract_user_salt(char **salt_begin,
                      char **salt_end);
C_MODE_START
char *
my_crypt_genhash(char *ctbuffer,
                 size_t ctbufflen,
                 const char *plaintext,
                 size_t plaintext_len,
                 const char *switchsalt,
                 const char **params);
void generate_user_salt(char *buffer, int buffer_len);
void xor_string(char *to, int to_len, char *pattern, int pattern_len);

C_MODE_END
#endif
