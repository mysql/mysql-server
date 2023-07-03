/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DICT_TAKEOVER_HPP
#define DICT_TAKEOVER_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 68


// see comments in Dbdict.hpp

class DictTakeoverReq {
  friend class Dbdict;
public:
  static constexpr Uint32 SignalLength = 1;
  static constexpr Uint32 GSN = GSN_DICT_TAKEOVER_REQ;
private:
  Uint32 senderRef;
};

class DictTakeoverConf {
  friend class Dbdict;
public:
  static constexpr Uint32 SignalLength = 10;
  static constexpr Uint32 GSN = GSN_DICT_TAKEOVER_CONF;
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
  static constexpr Uint32 SignalLength = 3;
  static constexpr Uint32 GSN = GSN_DICT_TAKEOVER_REF;

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



#undef JAM_FILE_ID

#endif
