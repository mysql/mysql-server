/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TUP_KEY_H
#define TUP_KEY_H

#include "SignalData.hpp"

class TupKeyReq {
  /**
   * Reciver(s)
   */
  friend class Dbtup;

  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * For printing
   */
  friend bool printTUPKEYREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 18 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 connectPtr;
  Uint32 request;
  Uint32 tableRef;
  Uint32 fragId;
  Uint32 keyRef1;
  Uint32 keyRef2;
  Uint32 attrBufLen;
  Uint32 opRef;
  Uint32 applRef;
  Uint32 schemaVersion;
  Uint32 storedProcedure;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 fragPtr;
  Uint32 primaryReplica;
  Uint32 coordinatorTC;
  Uint32 tcOpIndex;
  Uint32 savePointId;
};

class TupKeyConf {
  /**
   * Reciver(s)
   */
  friend class Dblqh;

  /**
   * Sender(s)
   */
  friend class Dbtup;

  /**
   * For printing
   */
  friend bool printTUPKEYCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 5 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 userPtr;
  Uint32 readLength;
  Uint32 writeLength;
  Uint32 noFiredTriggers;
  Uint32 lastRow;
};

class TupKeyRef {
  /**
   * Reciver(s)
   */
  friend class Dblqh;      

  /**
   * Sender(s)
   */
  friend class Dbtup;

  /**
   * For printing
   */
  friend bool printTUPKEYREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
  STATIC_CONST( SignalLength = 2 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 userRef;
  Uint32 errorCode;
};

#endif
