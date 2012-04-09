/*
   Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_RAND_H
#define NDB_RAND_H

#define NDB_RAND_MAX 32767

#ifdef __cplusplus
extern "C" {
#endif

int ndb_rand(void);
int ndb_rand_r(unsigned * seed);
void ndb_srand(unsigned seed);

#ifdef NDB_WIN
#define srandom(s) srand(s)
#endif

#ifdef __cplusplus
}
#endif

#endif
