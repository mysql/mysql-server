/* Copyright (c) 2003, 2006 MySQL AB
   Use is subject to license terms

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* 
  This is needed to be able to compile with original libwrap header
  files that don't have the prototypes
*/

#include <my_global.h>
#include <my_libwrap.h>

#ifdef HAVE_LIBWRAP

void my_fromhost(struct request_info *req)
{
  fromhost(req);
}

int my_hosts_access(struct request_info *req)
{
  return hosts_access(req);
}

char *my_eval_client(struct request_info *req)
{
  return eval_client(req);
}

#endif /* HAVE_LIBWRAP */
