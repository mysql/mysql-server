/*
   Copyright (c) 2007, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_rand.h>

static unsigned long next= 1;

#define NDB_RAND_MAX 32767

/**
 * ndb_rand
 *
 * constant time, cheap, pseudo-random number generator.
 *
 * NDB_RAND_MAX assumed to be 32767
 *
 * This is the POSIX example for "generating the same sequence on
 * different machines". Although that is not one of our requirements.
 */
int ndb_rand(void)
{
  next= next * 1103515245 + 12345;
  return((unsigned)(next/65536) % 32768);
}

void ndb_srand(unsigned seed)
{
  next= seed;
}

int
ndb_rand_r(unsigned * seed)
{
  * seed = (* seed) * 1103515245 + 12345;
  return ((unsigned)(*seed / 65536) % 32768);
}
