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

#ifndef TUP_ACCESS_HPP
#define TUP_ACCESS_HPP

#include "SignalData.hpp"

/*
 * Direct signals used by ACC and TUX to access the TUP block in the
 * same thread.
 *
 * NOTE: Caller must set errorCode to RNIL.  Signal printer uses this to
 * distinguish between input and output (no better way exists).
 */

/*
 * Read attributes from any table.
 */
class TupReadAttrs {
  friend class Dbtup;
  friend class Dbacc;
  friend class Dbtux;
  friend bool printTUP_READ_ATTRS(FILE*, const Uint32*, Uint32, Uint16);
public:
  enum Flag {
    /*
     * Read primary key attributes.  No input attribute ids are
     * specified.  Instead TUP fills in both input and output sections.
     * Tuple version is not used.
     */
    ReadKeys = (1 << 0)
  };
  STATIC_CONST( SignalLength = 10 );
private:
  /*
   * Error code set by TUP.  Zero means no error.
   */
  Uint32 errorCode;
  /*
   * Request info contains flags (see Flags above).
   */
  Uint32 requestInfo;
  /*
   * Table i-value.
   */
  Uint32 tableId;
  /*
   * Fragment is given by logical id within the table or by direct
   * i-value (faster).  Unknown values are given as RNIL.  On return TUP
   * fills in both values.
   */
  Uint32 fragId;
  Uint32 fragPtrI;
  /*
   * Logical address ("local key") of "original" tuple (the latest
   * version) consisting of logical fragment page id and tuple index
   * within the page (shifted left by 1).
   */
  Uint32 tupAddr;
  /*
   * Version of the tuple to read.  Not used if ReadKeys.
   */
  Uint32 tupVersion;
  /*
   * Real page id and offset of the "original" tuple.  Unknown page is
   * given as RNIL.  On return TUP fills in these.
   */
  Uint32 pageId;
  Uint32 pageOffset;
  /*
   * Shared buffer id.  Currently must be 0 which means to use rest of
   * signal data.
   */
  Uint32 bufferId;
  /*
   * Shared buffer 0 starts after signal class.  Input is number of
   * attributes and list of attribute ids in AttributeHeader format.
   * Output is placed after the input and consists of a list of entries
   * where each entry has an AttributeHeader followed by words of data.
   */
};

/*
 * Query status of tuple version.  Used by TUX to decide if a tuple
 * version found in index tree is visible to the transaction.
 */
class TupQueryTh {
  friend class Dbtup;
  friend class Dbtux;
  friend bool printTUP_QUERY_TH(FILE*, const Uint32*, Uint32, Uint16);
public:
  enum Flag {
  };
  STATIC_CONST( SignalLength = 7 );
private:
  /*
  TUX wants to check if tuple is visible to the scan query.
  Input data is tuple address (tableId, fragId, tupAddr, tupVersion),
  and transaction data so that TUP knows how to deduct if tuple is
  visible (transId1, transId2, savePointId).
  returnCode is set in return signal to indicate whether tuple is visible.
  */
  union {
    Uint32 returnCode; // 1 if tuple visible
    Uint32 tableId;
  };
  Uint32 fragId;
  Uint32 tupAddr;
  Uint32 tupVersion;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 savePointId;
};

/*
 * Operate on entire tuple.  Used by TUX where the table has a single
 * Uint32 array attribute representing an index tree node.
 *
 * XXX this signal is no longer used by TUX and can be removed
 */
class TupStoreTh {
  friend class Dbtup;
  friend class Dbtux;
  friend bool printTUP_STORE_TH(FILE*, const Uint32*, Uint32, Uint16);
public:
  enum OpCode {
    OpUndefined = 0,
    OpRead = 1,
    OpInsert = 2,
    OpUpdate = 3,
    OpDelete = 4
  };
  STATIC_CONST( SignalLength = 12 );
private:
  /*
   * These are as in TupReadAttrs (except opCode).  Version must be
   * zero.  Ordered index tuple (tree node) has only current version.
   */
  Uint32 errorCode;
  Uint32 opCode;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 fragPtrI;
  Uint32 tupAddr;
  Uint32 tupVersion;
  Uint32 pageId;
  Uint32 pageOffset;
  Uint32 bufferId;
  /*
   * Data offset and size in words.  Applies to both the buffer and the
   * tuple.  Used e.g. to read only node header.
   */
  Uint32 dataOffset;
  Uint32 dataSize;
  /*
   * Shared buffer 0 starts after signal class.
   */
};

#endif
