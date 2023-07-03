/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_binlog_extra_row_info.h"

#include <string.h>  // memcpy

#include "my_byteorder.h"

Ndb_binlog_extra_row_info::Ndb_binlog_extra_row_info() {
  flags = 0;
  transactionId = InvalidTransactionId;
  conflictFlags = UnsetConflictFlags;
  /* Prepare buffer with extra row info buffer bytes */
  buff[EXTRA_ROW_INFO_LEN_OFFSET] = 0;
  buff[EXTRA_ROW_INFO_FORMAT_OFFSET] = ERIF_NDB;
}

void Ndb_binlog_extra_row_info::setFlags(Uint16 _flags) { flags = _flags; }

void Ndb_binlog_extra_row_info::setTransactionId(Uint64 _transactionId) {
  assert(_transactionId != InvalidTransactionId);
  transactionId = _transactionId;
}

void Ndb_binlog_extra_row_info::setConflictFlags(Uint16 _conflictFlags) {
  conflictFlags = _conflictFlags;
}

int Ndb_binlog_extra_row_info::loadFromBuffer(const uchar *extra_row_info) {
  assert(extra_row_info);

  Uint8 length = extra_row_info[EXTRA_ROW_INFO_LEN_OFFSET];
  assert(length >= EXTRA_ROW_INFO_HEADER_LENGTH);
  Uint8 payload_length = length - EXTRA_ROW_INFO_HEADER_LENGTH;
  Uint8 format = extra_row_info[EXTRA_ROW_INFO_FORMAT_OFFSET];

  if (likely(format == ERIF_NDB)) {
    if (likely(payload_length >= FLAGS_SIZE)) {
      const unsigned char *data = &extra_row_info[EXTRA_ROW_INFO_HEADER_LENGTH];
      Uint8 nextPos = 0;

      /* Have flags at least */
      bool error = false;
      Uint16 netFlags;
      memcpy(&netFlags, &data[nextPos], FLAGS_SIZE);
      nextPos += FLAGS_SIZE;
      flags = uint2korr((const char *)&netFlags);

      if (flags & NDB_ERIF_TRANSID) {
        if (likely((nextPos + TRANSID_SIZE) <= payload_length)) {
          /*
            Correct length, retrieve transaction id, converting from
            little endian if necessary.
          */
          Uint64 netTransId;
          memcpy(&netTransId, &data[nextPos], TRANSID_SIZE);
          nextPos += TRANSID_SIZE;
          transactionId = uint8korr((const char *)&netTransId);
        } else {
          flags = 0; /* No more processing */
          error = true;
        }
      }

      if (flags & NDB_ERIF_CFT_FLAGS) {
        if (likely((nextPos + CFT_FLAGS_SIZE) <= payload_length)) {
          /**
           * Correct length, retrieve conflict flags, converting if
           * necessary
           */
          Uint16 netCftFlags;
          memcpy(&netCftFlags, &data[nextPos], CFT_FLAGS_SIZE);
          nextPos += CFT_FLAGS_SIZE;
          conflictFlags = uint2korr((const char *)&netCftFlags);
        } else {
          flags = 0; /* No more processing */
          error = true;
        }
      }

      if (likely(!error)) {
        return 0;
      } else {
        /* Error - malformed buffer, dump some debugging info */
        fprintf(stderr,
                "Ndb_binlog_extra_row_info::loadFromBuffer()"
                "malformed buffer - flags : %x nextPos %u "
                "payload_length %u\n",
                uint2korr((const char *)&netFlags), nextPos, payload_length);
        return -1;
      }
    }
  }

  /* We currently ignore other formats of extra binlog info, and
   * different lengths.
   */

  return 0;
}

uchar *Ndb_binlog_extra_row_info::generateBuffer() {
  /*
    Here we write out the buffer in network format,
    based on the current member settings.
  */
  Uint8 nextPos = EXTRA_ROW_INFO_HEADER_LENGTH;

  if (flags) {
    /* Write current flags into buff */
    Uint16 netFlags = uint2korr((const char *)&flags);
    memcpy(&buff[nextPos], &netFlags, FLAGS_SIZE);
    nextPos += FLAGS_SIZE;

    if (flags & NDB_ERIF_TRANSID) {
      Uint64 netTransactionId = uint8korr((const char *)&transactionId);
      memcpy(&buff[nextPos], &netTransactionId, TRANSID_SIZE);
      nextPos += TRANSID_SIZE;
    }

    if (flags & NDB_ERIF_CFT_FLAGS) {
      Uint16 netCftFlags = uint2korr((const char *)&conflictFlags);
      memcpy(&buff[nextPos], &netCftFlags, CFT_FLAGS_SIZE);
      nextPos += CFT_FLAGS_SIZE;
    }

    assert(nextPos <= MaxLen);
    /* Set length */
    assert(buff[EXTRA_ROW_INFO_FORMAT_OFFSET] == ERIF_NDB);
    buff[EXTRA_ROW_INFO_LEN_OFFSET] = nextPos;

    return buff;
  }
  return nullptr;
}
