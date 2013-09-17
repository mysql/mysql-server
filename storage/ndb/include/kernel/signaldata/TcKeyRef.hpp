/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TCKEYREF_HPP
#define TCKEYREF_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 177


class TcKeyRef {

  /**
   * Receiver(s)
   */
  friend class NdbOperation;
  friend class NdbQueryOperationImpl;
  friend class Ndbcntr;
  friend class DbUtil;

  /**
   * Sender(s) / Receiver(s)
   */
  friend class Dbtc;

  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class Dbspj;

  friend bool printTCKEYREF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 5 );

private:
  Uint32 connectPtr; /* Operation pointer */
  Uint32 transId[2];
  Uint32 errorCode;
  Uint32 errorData;
};


#undef JAM_FILE_ID

#endif
