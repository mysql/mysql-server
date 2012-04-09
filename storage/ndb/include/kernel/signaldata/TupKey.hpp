/*
   Copyright (C) 2003-2006, 2008 MySQL AB
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
  STATIC_CONST( SignalLength = 20 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 connectPtr;
  Uint32 request;
  Uint32 keyRef1;
  Uint32 keyRef2;
  Uint32 attrBufLen;
  Uint32 opRef;
  Uint32 applRef;
  Uint32 storedProcedure;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 fragPtr;
  Uint32 primaryReplica;
  Uint32 coordinatorTC;
  Uint32 tcOpIndex;
  Uint32 savePointId;
  Uint32 disk_page;
  Uint32 m_row_id_page_no;
  Uint32 m_row_id_page_idx;
  Uint32 attrInfoIVal;
  Uint32 deferred_constraints;
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
  STATIC_CONST( SignalLength = 6 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 userPtr;
  Uint32 readLength;
  Uint32 writeLength;
  Uint32 noFiredTriggers;
  Uint32 lastRow;
  Uint32 rowid;
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
