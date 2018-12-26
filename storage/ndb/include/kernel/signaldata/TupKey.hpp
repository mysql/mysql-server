/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TUP_KEY_H
#define TUP_KEY_H

#include "SignalData.hpp"

#define JAM_FILE_ID 57


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
  STATIC_CONST( SignalLength = 21 );

  enum
  {
    OP_PRIMARY_REPLICA = 0,
    OP_BACKUP_REPLICA = 1,
    OP_NO_TRIGGERS = 2,
  };

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
  Uint32 triggers;
  Uint32 coordinatorTC;
  Uint32 tcOpIndex;
  Uint32 savePointId;
  Uint32 disk_page;
  Uint32 m_row_id_page_no;
  Uint32 m_row_id_page_idx;
  Uint32 attrInfoIVal;
  Uint32 deferred_constraints;
  Uint32 disable_fk_checks;

  static Uint32 getDirtyFlag(Uint32 const& requestInfo);
  static Uint32 getSimpleFlag(Uint32 const& requestInfo);
  static Uint32 getOperation(Uint32 const& requestInfo);
  static Uint32 getInterpretedFlag(Uint32 const& requestInfo);
  static Uint32 getRowidFlag(Uint32 const& requestInfo);
  static Uint32 getReorgFlag(Uint32 const& requestInfo);
  static Uint32 getPrioAFlag(Uint32 const& requestInfo);
  static Uint32 getNrCopyFlag(Uint32 const& requestInfo);
  static void setDirtyFlag(Uint32 & requestInfo, Uint32 value);
  static void setSimpleFlag(Uint32 & requestInfo, Uint32 value);
  static void setOperation(Uint32 & requestInfo, Uint32 value);
  static void setInterpretedFlag(Uint32 & requestInfo, Uint32 value);
  static void setRowidFlag(Uint32 & requestInfo, Uint32 value);
  static void setReorgFlag(Uint32 & requestInfo, Uint32 value);
  static void setPrioAFlag(Uint32 & requestInfo, Uint32 value);
  static void setNrCopyFlag(Uint32 & requestInfo, Uint32 value);

  /*
    Request Info

              111111 1111222222222233
    0123456789012345 6789012345678901
    ds....ooo.izrrac ................
  */

  enum RequestInfo {
    DIRTY_POS       =  0, DIRTY_MASK       = 1,
    SIMPLE_POS      =  1, SIMPLE_MASK      = 1,
    OPERATION_POS   =  6, OPERATION_MASK   = 7,
    INTERPRETED_POS = 10, INTERPRETED_MASK = 1,
    ROWID_POS       = 11, ROWID_MASK       = 1,
    REORG_POS       = 12, REORG_MASK       = 3,
    PRIO_A_POS      = 14, PRIO_A_MASK      = 1,
    NR_COPY_POS     = 15, NR_COPY_MASK     = 1
  };
};

inline Uint32
TupKeyReq::getDirtyFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> DIRTY_POS) & DIRTY_MASK;
}

inline Uint32
TupKeyReq::getSimpleFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> SIMPLE_POS) & SIMPLE_MASK;
}

inline Uint32
TupKeyReq::getOperation(Uint32 const& requestInfo)
{
  return (requestInfo >> OPERATION_POS) & OPERATION_MASK;
}

inline Uint32
TupKeyReq::getInterpretedFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> INTERPRETED_POS) & INTERPRETED_MASK;
}

inline Uint32
TupKeyReq::getRowidFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> ROWID_POS) & ROWID_MASK;
}

inline Uint32
TupKeyReq::getReorgFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> REORG_POS) & REORG_MASK;
}

inline Uint32
TupKeyReq::getPrioAFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> PRIO_A_POS) & PRIO_A_MASK;
}

inline Uint32
TupKeyReq::getNrCopyFlag(Uint32 const& requestInfo)
{
  return (requestInfo >> NR_COPY_POS) & NR_COPY_MASK;
}

inline void
TupKeyReq::setDirtyFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= DIRTY_MASK);
  assert((requestInfo & (DIRTY_MASK << DIRTY_POS)) == 0);
  requestInfo |= value << DIRTY_POS;
}

inline void
TupKeyReq::setSimpleFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= SIMPLE_MASK);
  assert((requestInfo & (SIMPLE_MASK << SIMPLE_POS)) == 0);
  requestInfo |= value << SIMPLE_POS;
}

inline void
TupKeyReq::setOperation(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= OPERATION_MASK);
  assert((requestInfo & (OPERATION_MASK << OPERATION_POS)) == 0);
  requestInfo |= value << OPERATION_POS;
}

inline void
TupKeyReq::setInterpretedFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= INTERPRETED_MASK);
  assert((requestInfo & (INTERPRETED_MASK << INTERPRETED_POS)) == 0);
  requestInfo |= value << INTERPRETED_POS;
}

inline void
TupKeyReq::setRowidFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= ROWID_MASK);
  assert((requestInfo & (ROWID_MASK << ROWID_POS)) == 0);
  requestInfo |= value << ROWID_POS;
}

inline void
TupKeyReq::setReorgFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= REORG_MASK);
  assert((requestInfo & (REORG_MASK << REORG_POS)) == 0);
  requestInfo |= value << REORG_POS;
}

inline void
TupKeyReq::setPrioAFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= PRIO_A_MASK);
  assert((requestInfo & (PRIO_A_MASK << PRIO_A_POS)) == 0);
  requestInfo |= value << PRIO_A_POS;
}

inline void
TupKeyReq::setNrCopyFlag(Uint32 & requestInfo, Uint32 value)
{
  assert(value <= NR_COPY_MASK);
  assert((requestInfo & (NR_COPY_MASK << NR_COPY_POS)) == 0);
  requestInfo |= value << NR_COPY_POS;
}

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
  STATIC_CONST( SignalLength = 7 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 userPtr;
  Uint32 readLength;  // Length in Uint32 words
  Uint32 writeLength;
  Uint32 numFiredTriggers;
  Uint32 lastRow;
  Uint32 rowid;
  // Number of interpreter instructions executed.
  Uint32 noExecInstructions;
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
  STATIC_CONST( SignalLength = 3 );

private:

  /**
   * DATA VARIABLES
   */
  Uint32 userRef;
  Uint32 errorCode;
  // Number of interpreter instructions executed.
  Uint32 noExecInstructions;
};


#undef JAM_FILE_ID

#endif
