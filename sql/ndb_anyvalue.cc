/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_anyvalue.h"

/*
   AnyValue carries ServerId or Reserved codes
   Bits from opt_server_id_bits to 30 may carry other data
   so we ignore them when reading/setting AnyValue.

   332        21        10        0
   10987654321098765432109876543210
   roooooooooooooooooooooooosssssss

   r = Reserved bit indicates whether
   bits 0-7+ have ServerId (0) or
   some special reserved code (1).
   o = Optional bits, depending on value
       of server-id-bits will be
       serverid bits or user-specific
       data
   s = Serverid bits or reserved codes
       At least 7 bits will be available
       for serverid or reserved codes

*/

#include <my_global.h>

extern ulong opt_server_id_mask;

#define NDB_ANYVALUE_RESERVED_BIT   0x80000000
#define NDB_ANYVALUE_RESERVED_MASK  0x8000007f

#define NDB_ANYVALUE_NOLOGGING_CODE 0x8000007f

#ifndef DBUG_OFF
void dbug_ndbcluster_anyvalue_set_userbits(Uint32& anyValue)
{
  /*
     Set userData part of AnyValue (if there is one) to
     all 1s to test that it is ignored
  */
  const Uint32 userDataMask = ~(opt_server_id_mask |
                                NDB_ANYVALUE_RESERVED_BIT);

  anyValue |= userDataMask;
}
#endif

bool ndbcluster_anyvalue_is_reserved(Uint32 anyValue)
{
  return ((anyValue & NDB_ANYVALUE_RESERVED_BIT) != 0);
}

bool ndbcluster_anyvalue_is_nologging(Uint32 anyValue)
{
  return ((anyValue & NDB_ANYVALUE_RESERVED_MASK) ==
          NDB_ANYVALUE_NOLOGGING_CODE);
}

void ndbcluster_anyvalue_set_nologging(Uint32& anyValue)
{
  anyValue |= NDB_ANYVALUE_NOLOGGING_CODE;
}

void ndbcluster_anyvalue_set_normal(Uint32& anyValue)
{
  /* Clear reserved bit and serverid bits */
  anyValue &= ~(NDB_ANYVALUE_RESERVED_BIT);
  anyValue &= ~(opt_server_id_mask);
}

bool ndbcluster_anyvalue_is_serverid_in_range(Uint32 serverId)
{
  return ((serverId & ~opt_server_id_mask) == 0);
}

Uint32 ndbcluster_anyvalue_get_serverid(Uint32 anyValue)
{
  assert(! (anyValue & NDB_ANYVALUE_RESERVED_BIT) );

  return (anyValue & opt_server_id_mask);
}

void ndbcluster_anyvalue_set_serverid(Uint32& anyValue, Uint32 serverId)
{
  assert(! (anyValue & NDB_ANYVALUE_RESERVED_BIT) );
  anyValue &= ~(opt_server_id_mask);
  anyValue |= (serverId & opt_server_id_mask);
}
