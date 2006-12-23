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

#ifndef NODE_BITMASK_HPP
#define NODE_BITMASK_HPP

#include <ndb_limits.h>
#include <kernel_types.h>
#include <Bitmask.hpp>

/**
 * No of 32 bits words needed to store a node bitmask
 *   containing all the nodes in the system
 *   Both NDB nodes and API, MGM... nodes
 *
 * Note that this is used in a lot of signals
 */
#define _NODE_BITMASK_SIZE 2

/**
 * No of 32 bits words needed to store a node bitmask
 *   containing all the ndb nodes in the system
 *
 * Note that this is used in a lot of signals
 */
#define _NDB_NODE_BITMASK_SIZE 2

/**
 * No of 32 bits word needed to store B bits for N nodes
 */
#define NODE_ARRAY_SIZE(N, B) (((N)*(B)+31) >> 5)

typedef Bitmask<(unsigned int)_NODE_BITMASK_SIZE> NodeBitmask;

typedef Bitmask<(unsigned int)_NDB_NODE_BITMASK_SIZE> NdbNodeBitmask;

#define __NBM_SZ  ((MAX_NODES >> 5) + ((MAX_NODES & 31) != 0))
#define __NNBM_SZ ((MAX_NDB_NODES >> 5) + ((MAX_NDB_NODES & 31) != 0))

#if ( __NBM_SZ > _NODE_BITMASK_SIZE)
#error "MAX_NODES can not fit into NODE_BITMASK_SIZE"
#endif

#if ( __NNBM_SZ > _NDB_NODE_BITMASK_SIZE)
#error "MAX_NDB_NODES can not fit into NDB_NODE_BITMASK_SIZE"
#endif

/**
 * General B Bits operations
 *
 * Get(x, A[], B)
 *   w = x >> S1
 *   s = (x & S2) << S3
 *   return (A[w] >> s) & S4
 *
 * Set(x, A[], v, B)
 *   w    = x >> S1
 *   s    = (x & S2) << S3
 *   m    = ~(S4 << s)
 *   t    = A[w] & m;
 *   A[w] = t | ((v & S4) << s)
 *
 * B(Bits)    S1    S2    S3     S4
 *    1        5    31     0      1
 *    2        4    15     1      3
 *    4        3     7     2     15
 *    8        2     3     3    255
 *   16        1     1     4  65535
 *
 * S1 = 5 - 2log(B)
 * S2 = 2^S1 - 1
 * S3 = 2log(B)
 * S4 = 2^B - 1
 */

#endif
