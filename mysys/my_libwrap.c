/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
