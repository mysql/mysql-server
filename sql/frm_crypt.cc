/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/*
** change the following to the output of password('our password')
** split into 2 parts of 8 characters each.
** This is done to make it impossible to search after a text string in the
** mysql binary.
*/

#include "sql_priv.h"
#include "frm_crypt.h"

#ifdef HAVE_CRYPTED_FRM

/* password('test') */
ulong password_seed[2]={0x378b243e, 0x220ca493};

SQL_CRYPT *get_crypt_for_frm(void)
{
  return new SQL_CRYPT(password_seed);
}

#endif
