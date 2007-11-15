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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_rand.h>

static unsigned long next= 1;

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

