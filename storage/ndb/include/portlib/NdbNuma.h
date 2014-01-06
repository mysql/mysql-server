/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ndb_numa_h
#define ndb_numa_h

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Set memory policy that memory allocation should be performed
 *   using interleaving over all numa-nodes
 *
 * NOTE: Not thread safe
 */
int NdbNuma_setInterleaved();

#if TODO
/**
 * Set memory policy that memory allocation should be performed
 *   using interleaving over numa-nodes corresponding to cpu's
 *
 * NOTE: Not thread safe
 */
int NdbNuma_setInterleavedOnCpus(unsigned cpu[], unsigned len);
#endif

#ifdef	__cplusplus
}
#endif

#endif
