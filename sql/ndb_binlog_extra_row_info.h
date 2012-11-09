/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_BINLOG_EXTRA_ROW_INFO_H
#define NDB_BINLOG_EXTRA_ROW_INFO_H

#include <my_global.h>
#include <ndb_types.h>
#include <rpl_constants.h>

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
  static const Uint32 MaxLen =
    EXTRA_ROW_INFO_HDR_BYTES +
    FLAGS_SIZE +
    TRANSID_SIZE;

  static const Uint64 InvalidTransactionId = ~Uint64(0);

  enum Flags
  {
    NDB_ERIF_TRANSID = 0x1
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
  uchar* getBuffPtr()
  { return buff; };
  uchar* generateBuffer();
private:
  uchar buff[MaxLen];
  Uint16 flags;
  Uint64 transactionId;
};


#endif
