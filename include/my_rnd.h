#ifndef MY_RANDOM_INCLUDED
#define MY_RANDOM_INCLUDED

/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

/*
  A wrapper to use OpenSSL/YaSSL PRNGs.
*/

#include <my_global.h>
#include <mysql_com.h>

#ifdef __cplusplus
extern "C" {
#endif

double my_rnd_ssl(struct rand_struct *rand_st);
int my_rand_buffer(unsigned char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* MY_RANDOM_INCLUDED */
