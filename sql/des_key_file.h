/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
