/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef NODE_BITMASK_HPP
#define NODE_BITMASK_HPP

#include "ndb_limits.h"
#include "kernel_types.h"
#include <Bitmask.hpp>
#include "../transporter/TransporterDefinitions.hpp"

#define JAM_FILE_ID 2


/**
 * No. of 32 bits words needed to store a node bitmask
 * containing all the nodes in the system
 *
 * - Data nodes, API and MGMD nodes
 *
 * Note that this is used in a lot of signals
 */
#define _NODE_BITMASK_SIZE 8

/**
 * No. of 32 bits words needed to store a node bitmask
 * containing all the nodes in the system prior to 8.0.15
 *
 * The maximum number of total nodes is 255 prior to 8.0.15
 *
 * - Data nodes, API and MGMD nodes
 * Note that this is used in a lot of signals
 */
#define _NODE_BITMASK_SIZE_255_NODES 8

/**
 * No of 32 bits words needed to store a transporter bitmask
 *   containing all the transporters in the system
 *   Both NDB nodes and API, MGM... nodes
 */
#define _TRP_BITMASK_SIZE 11

/**
 * No of 32 bits words needed to store a node bitmask
 *   containing all the ndb nodes in the system
 *
 * Note that this is used in a lot of signals
 */
#define _NDB_NODE_BITMASK_SIZE 5

/**
 * No of 32 bits words needed to store a node bitmask
 * containing all the data nodes in the system prior to 8.0.15
 *
 * The maximum number of data nodes is 48 prior to 8.0.15
 *
 * Note that this is used in a lot of signals_NDB_NODE_BITMASK_SIZE
 */
#define _NDB_NODE_BITMASK_SIZE_48_NODES 2

#define _NDB_NBM_DIFF_BYTES (_NDB_NODE_BITMASK_SIZE - _NDB_NODE_BITMASK_SIZE_48_NODES)\
  * sizeof(Uint32)
#define _NBM_DIFF_BYTES (_NODE_BITMASK_SIZE - _NODE_BITMASK_SIZE_255_NODES)\
  * sizeof(Uint32)
/**
 * No of 32 bits word needed to store B bits for N nodes
 */
#define NODE_ARRAY_SIZE(N, B) (((N)*(B)+31) >> 5)

typedef Bitmask<(unsigned int)_NODE_BITMASK_SIZE> NodeBitmask;
typedef BitmaskPOD<(unsigned int)_NODE_BITMASK_SIZE> NodeBitmaskPOD;

typedef Bitmask<(unsigned int)_TRP_BITMASK_SIZE> TrpBitmask;
typedef BitmaskPOD<(unsigned int)_TRP_BITMASK_SIZE> TrpBitmaskPOD;

typedef Bitmask<(unsigned int)_NDB_NODE_BITMASK_SIZE> NdbNodeBitmask;
typedef BitmaskPOD<(unsigned int)_NDB_NODE_BITMASK_SIZE> NdbNodeBitmaskPOD;

typedef Bitmask<(unsigned int)_NODE_BITMASK_SIZE_255_NODES> NodeBitmask255;
typedef Bitmask<(unsigned int)_NDB_NODE_BITMASK_SIZE_48_NODES> NdbNodeBitmask48;


#define __NBM_SZ  ((MAX_NODES >> 5) + ((MAX_NODES & 31) != 0))
#define __TBM_SZ  ((MAX_NTRANSPORTERS >> 5) + ((MAX_NTRANSPORTERS & 31) != 0))
#define __NNBM_SZ ((MAX_NDB_NODES >> 5) + ((MAX_NDB_NODES & 31) != 0))

#if ( __NBM_SZ > _NODE_BITMASK_SIZE)
#error "MAX_NODES can not fit into NODE_BITMASK_SIZE"
#endif

#if ( __TBM_SZ > _TRP_BITMASK_SIZE)
#error "MAX_NTRANSPORTERS can not fit into TRP_BITMASK_SIZE"
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


#undef JAM_FILE_ID

#endif
