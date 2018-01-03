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

#ifndef NDB_BINLOG_EXTRA_ROW_INFO_H
#define NDB_BINLOG_EXTRA_ROW_INFO_H

#include "my_byteorder.h"
#include "rows_event.h"
#include "sql/rpl_constants.h"
#include "storage/ndb/include/ndb_types.h"

/*
   Helper for reading/writing Binlog extra row info
   in Ndb format.
   It contains an internal buffer, which can be passed
   in the thd variable when writing binlog entries if
   the object stays in scope around the write.
*/
class Ndb_binlog_extra_row_info
{
public:
  static const Uint32 FLAGS_SIZE = sizeof(Uint16);
  static const Uint32 TRANSID_SIZE = sizeof(Uint64);
  static const Uint32 CFT_FLAGS_SIZE = sizeof(Uint16);
  static const Uint32 MaxLen =
    EXTRA_ROW_INFO_HDR_BYTES +
    FLAGS_SIZE +
    TRANSID_SIZE +
    CFT_FLAGS_SIZE;

  static const Uint64 InvalidTransactionId = ~Uint64(0);
  static const Uint16 UnsetConflictFlags = 0;

  enum Flags
  {
    NDB_ERIF_TRANSID   = 0x1,
    NDB_ERIF_CFT_FLAGS = 0x2
  };

  Ndb_binlog_extra_row_info();

  int loadFromBuffer(const uchar* extra_row_info_ptr);

  Uint16 getFlags() const
  {
    return flags;
  }
  void setFlags(Uint16 _flags);
  
  Uint64 getTransactionId() const
  { return transactionId; };
  void setTransactionId(Uint64 _transactionId);
  
  Uint16 getConflictFlags() const
  { return conflictFlags; };
  void setConflictFlags(Uint16 _conflictFlags);

  uchar* getBuffPtr()
  { return buff; };
  uchar* generateBuffer();
private:
  uchar buff[MaxLen];
  Uint16 flags;
  Uint64 transactionId;
  Uint16 conflictFlags;
};


#endif
