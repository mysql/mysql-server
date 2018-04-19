/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_anyvalue.h"

/*
   AnyValue carries ServerId or Reserved codes
   Bits from opt_server_id_bits to 30 may carry other data
   so we ignore them when reading/setting AnyValue.

   The idea with supporting 'other data' is to allow NdbApi 
   users to tag their NdbApi operations in some way that can
   be picked up at NdbApi event receivers, *without* interacting
   badly with / disabling normal binlogging and replication.
   
   To achieve this, we have a variable sized mask of bits in the
   *middle* of the AnyValue word which can be used to mask out
   the user data for the purpose of the MySQL Server.
   
   A better future approach would be to support > 1 tag word
   per operation.


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

   Implications : 
     Reserved codes can use values between
     0x80000000 and 0x8000007f inclusive
     (256 values).
     0x8000007f was always the 'nologging'
     code, so the others have started 
     'counting' down from there

   Examples :
     opt_server_id_bits= 31
      - ServerIds can be up to 2^31-1
      - No user-specific data supported
      - Reserved codes look like :
        0x8000007f etc...

     opt_server_id_bits= 7
      - ServerIds can be up to 2^7-1
      - User specific data can be up to 2^24-1
      - ServerIds have 0 top bit, 24 user bits, then 
        the serverid
      - Reserved codes have 1 top bit, 24 user bits (prob
        not used much), then the bottom lsbs of the 
        reserved code.
*/

#include <assert.h>

#include "my_inttypes.h"

extern ulong opt_server_id_mask;

#define NDB_ANYVALUE_RESERVED_BIT   0x80000000
#define NDB_ANYVALUE_RESERVED_MASK  0x8000007f

#define NDB_ANYVALUE_NOLOGGING_CODE 0x8000007f

#define NDB_ANYVALUE_REFRESH_OP_CODE 0x8000007e
#define NDB_ANYVALUE_REFLECT_OP_CODE 0x8000007d
#define NDB_ANYVALUE_READ_OP_CODE    0x8000007c

/* Next reserved code : 0x8000007c */


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

bool ndbcluster_anyvalue_is_refresh_op(Uint32 anyValue)
{
  return ((anyValue & NDB_ANYVALUE_RESERVED_MASK) ==
          NDB_ANYVALUE_REFRESH_OP_CODE);
}

void ndbcluster_anyvalue_set_refresh_op(Uint32& anyValue)
{
  anyValue &= ~NDB_ANYVALUE_RESERVED_MASK;
  anyValue |= NDB_ANYVALUE_REFRESH_OP_CODE;
}

bool ndbcluster_anyvalue_is_read_op(Uint32 anyValue)
{
  return ((anyValue & NDB_ANYVALUE_RESERVED_MASK) ==
          NDB_ANYVALUE_READ_OP_CODE);
}

void ndbcluster_anyvalue_set_read_op(Uint32& anyValue)
{
  anyValue &= ~NDB_ANYVALUE_RESERVED_MASK;
  anyValue |= NDB_ANYVALUE_READ_OP_CODE;
}

bool ndbcluster_anyvalue_is_reflect_op(Uint32 anyValue)
{
  return ((anyValue & NDB_ANYVALUE_RESERVED_MASK) ==
          NDB_ANYVALUE_REFLECT_OP_CODE);
}

void ndbcluster_anyvalue_set_reflect_op(Uint32& anyValue)
{
  anyValue &= ~NDB_ANYVALUE_RESERVED_MASK;
  anyValue |= NDB_ANYVALUE_REFLECT_OP_CODE;
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
