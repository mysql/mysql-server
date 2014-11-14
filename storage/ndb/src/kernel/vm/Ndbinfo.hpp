/*
   Copyright (c) 2009, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef KERNEL_NDBINFO_HPP
#define KERNEL_NDBINFO_HPP

#include <signaldata/DbinfoScan.hpp>

#define JAM_FILE_ID 230


class Ndbinfo {
public:

  enum ColumnType {
    String = 1,
    Number = 2,
    Number64 = 3
  };

  struct Column {
    const char* name;
    ColumnType coltype;
    const char* comment;
  };

  enum TableId {
    TABLES_TABLEID =             0,
    COLUMNS_TABLEID =            1,
    TEST_TABLEID =               2,
    POOLS_TABLEID =              3,
    TRANSPORTERS_TABLEID =       4,
    LOGSPACES_TABLEID =          5,
    LOGBUFFERS_TABLEID =         6,
    RESOURCES_TABLEID =          7,
    COUNTERS_TABLEID =           8,
    NODES_TABLEID =              9,
    DISKPAGEBUFFER_TABLEID =     10,
    THREADBLOCKS_TABLEID =       11,
    THREADSTAT_TABLEID =         12,
    TRANSACTIONS_TABLEID =       13,
    OPERATIONS_TABLEID =         14,
    MEMBERSHIP_TABLEID =         15,
    DICT_OBJ_INFO_TABLEID =      16,
    FRAG_MEM_USE_TABLEID =       17,
    DISK_WRITE_SPEED_BASE_TABLEID = 18,
    DISK_WRITE_SPEED_AGGREGATE_TABLEID = 19,
    FRAG_OPERATIONS_TABLEID =    20,
    RESTART_INFO_TABLEID =       21
  };

  struct Table {
    struct Members {
      const char* name;
      int ncols;
      int flags;
      const char* comment;
    } m;
    Column col[1];

    int columns(void) const {
      return m.ncols;
    }
  };
  static int getNumTables();
  static const Table& getTable(int i);
  static const Table& getTable(Uint32 i);

  class Row {
    friend class SimulatedBlock;
    Uint32* start;      // Start of row buffer
    Uint32* curr;       // Current position in row buffer
    Uint32* end;        // End of buffer
    int col_counter;    // Current column counter
    DbinfoScan& m_req;  // The scan parameters
    Row();              // Not impl
    Row(const Row&);    // Not impl
  public:

    Row(class Signal* signal, DbinfoScanReq& req);

    Uint32 getLength(void) const {
      return (Uint32)(curr - start);
    }

    Uint32* getDataPtr() const {
      return start;
    }

    void write_string(const char* col);
    void write_uint32(Uint32 value);
    void write_uint64(Uint64 value);

    int columns(void) const {
      return col_counter;
    }

  private:
    bool check_buffer_space(class AttributeHeader& ah) const;
    void check_attribute_type(class AttributeHeader& ah, ColumnType) const;
  };

  struct ScanCursor
  {
    Uint32 senderRef;
    Uint32 saveSenderRef;
    Uint32 currRef; // The current node, block and instance
    Uint32 saveCurrRef;
    /**
     * Flags
     *
     m = More data         - 1  Bit 1

               1111111111222222222233
     01234567890123456789012345678901
     m
    */
    Uint32 flags;
    Uint32 data[4];  // Cursor data

    Uint32 totalRows;
    Uint32 totalBytes;
    STATIC_CONST( Length = 10 );

    STATIC_CONST( MOREDATA_SHIFT = 0 );
    STATIC_CONST( MOREDATA_MASK = 1 );

    static bool getHasMoreData(const UintR & flags){
      return (bool)((flags >> MOREDATA_SHIFT) & MOREDATA_MASK);
    }
    static void setHasMoreData(UintR & flags, bool value){
      flags = (flags & ~(MOREDATA_MASK << MOREDATA_SHIFT)) |
              ((value & MOREDATA_MASK) << MOREDATA_SHIFT);
    }
  };

  class Ratelimit {
    friend class SimulatedBlock;
    Uint32 rows;
    Uint32 bytes;
    Ratelimit(const Ratelimit&);// Not impl
  public:
    Ratelimit() :
      rows(0),
      bytes(0){
    }

    bool need_break(const DbinfoScan& scan) const
    {
      const Uint32 MAX_ROWS = 256;

      // Upgrade zero to MAX_ROWS 
      Uint32 maxRows = scan.maxRows ? scan.maxRows : MAX_ROWS; 

      // Limit maxRows to MAX_ROWS
      if (maxRows > MAX_ROWS)
        maxRows = MAX_ROWS;

      if (maxRows != 0 && rows >= maxRows)
        return true; // More than max rows already sent
      if (scan.maxBytes != 0 &&  bytes >= scan.maxBytes)
        return true; // More than max bytes already sent
      return false;
    }
  };

  struct pool_entry {
    const char* poolname;
    Uint64 used;
    Uint64 total;
    Uint64 entry_size;
    Uint64 used_hi;
    Uint32 config_params[4];
  };

  enum counter_id {
    ATTRINFO_COUNTER = 1,
    TRANSACTIONS_COUNTER = 2,
    COMMITS_COUNTER = 3,
    READS_COUNTER = 4,
    SIMPLE_READS_COUNTER = 5,
    WRITES_COUNTER = 6,
    ABORTS_COUNTER = 7,
    TABLE_SCANS_COUNTER = 8,
    RANGE_SCANS_COUNTER = 9,
    OPERATIONS_COUNTER = 10,
    /* Counters fetched from the SPJ block.*/
    SPJ_READS_RECEIVED_COUNTER = 11,
    SPJ_LOCAL_READS_SENT_COUNTER = 12,
    SPJ_REMOTE_READS_SENT_COUNTER = 13,
    SPJ_READS_NOT_FOUND_COUNTER = 14,
    SPJ_TABLE_SCANS_RECEIVED_COUNTER = 15,
    SPJ_LOCAL_TABLE_SCANS_SENT_COUNTER = 16,
    SPJ_RANGE_SCANS_RECEIVED_COUNTER = 17,
    SPJ_LOCAL_RANGE_SCANS_SENT_COUNTER = 18,
    SPJ_REMOTE_RANGE_SCANS_SENT_COUNTER = 19,
    SPJ_SCAN_BATCHES_RETURNED_COUNTER = 20,
    SPJ_SCAN_ROWS_RETURNED_COUNTER = 21,
    SPJ_PRUNED_RANGE_SCANS_RECEIVED_COUNTER = 22,
    SPJ_CONST_PRUNED_RANGE_SCANS_RECEIVED_COUNTER = 23,
    LOCAL_READ_COUNTER = 24,
    LOCAL_WRITE_COUNTER = 25,
    LQHKEY_OVERLOAD = 26,
    LQHKEY_OVERLOAD_TC = 27,
    LQHKEY_OVERLOAD_READER = 28,
    LQHKEY_OVERLOAD_NODE_PEER = 29,
    LQHKEY_OVERLOAD_SUBSCRIBER = 30,
    LQHSCAN_SLOWDOWN = 31
  };

  struct counter_entry {
    counter_id id;
    Uint64 val;
  };
};


#undef JAM_FILE_ID

#endif
