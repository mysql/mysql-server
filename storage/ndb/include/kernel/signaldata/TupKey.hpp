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
  static constexpr Uint32 SignalLength = 10;

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
  Uint32 storedProcedure;
  Uint32 fragPtr;
  Uint32 disk_page;
  Uint32 m_row_id_page_no;
  Uint32 m_row_id_page_idx;
  Uint32 attrInfoIVal;

  static Uint32 getInterpretedFlag(Uint32 const& requestInfo);
  static Uint32 getRowidFlag(Uint32 const& requestInfo);
  static void setInterpretedFlag(Uint32 & requestInfo, Uint32 value);
  static void setRowidFlag(Uint32 & requestInfo, Uint32 value);

  /*
    Request Info

              111111 1111222222222233
    0123456789012345 6789012345678901
    ..........iz.... ................
  */

  enum RequestInfo {
    INTERPRETED_POS = 10, INTERPRETED_MASK = 1,
    ROWID_POS       = 11, ROWID_MASK       = 1
  };
};

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
  static constexpr Uint32 SignalLength = 7;

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
  static constexpr Uint32 SignalLength = 3;

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
