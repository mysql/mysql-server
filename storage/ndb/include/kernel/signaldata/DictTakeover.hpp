/* Copyright 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef DICT_TAKEOVER_HPP
#define DICT_TAKEOVER_HPP

#include "SignalData.hpp"

// see comments in Dbdict.hpp

class DictTakeoverReq {
  friend class Dbdict;
public:
  STATIC_CONST( SignalLength = 1 );
  STATIC_CONST( GSN = GSN_DICT_TAKEOVER_REQ );
private:
  Uint32 senderRef;
};

class DictTakeoverConf {
  friend class Dbdict;
public:
  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( GSN = GSN_DICT_TAKEOVER_CONF );
private:
  Uint32 senderRef;
  Uint32 clientRef;
  Uint32 trans_key;
  Uint32 trans_state;
  Uint32 op_count;
  union
  {
    Uint32 rollforward_op; // Preferred starting point for rollforward
    Uint32 lowest_op;      // Next operation to be ended (removed)
  };
  union {
    Uint32 rollforward_op_state;
    Uint32 lowest_op_state;
  };
  union {
    Uint32 rollback_op; // Preferred starting point for rollback
    Uint32 highest_op;  // Last parsed operation
  };
  union {
    Uint32 rollback_op_state;
    Uint32 highest_op_state;
  };
  // Highest/lowest op is needed if new master is missing one operation
  union {
    Uint32 lowest_op_impl_req_gsn;
    Uint32 highest_op_impl_req_gsn;
  };
};

struct DictTakeoverRef {
  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( GSN = GSN_DICT_TAKEOVER_REF );

  Uint32 senderRef;
  union { Uint32 masterRef, senderData; };
  Uint32 errorCode;
  enum ErrorCode 
  {
    NoError = 0,
    NoTransaction = 1,
    NF_FakeErrorREF = 2
  };
};


#endif
