/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef DES_KEY_FILE_INCLUDED
#define DES_KEY_FILE_INCLUDED

#ifdef HAVE_OPENSSL
#include <openssl/des.h>

#include "violite.h"                /* DES_cblock, DES_key_schedule */

struct st_des_keyblock
{
  DES_cblock key1, key2, key3;
};

struct st_des_keyschedule
{
  DES_key_schedule ks1, ks2, ks3;
};

extern struct st_des_keyschedule des_keyschedule[10];
extern uint des_default_key;

bool load_des_key_file(const char *file_name);
#endif /* HAVE_OPENSSL */

#endif /* DES_KEY_FILE_INCLUDED */
