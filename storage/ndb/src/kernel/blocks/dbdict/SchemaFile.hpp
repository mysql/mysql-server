/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DBDICT_SCHEMA_FILE_HPP
#define DBDICT_SCHEMA_FILE_HPP

#include <ndb_types.h>
#include <ndb_version.h>
#include <string.h>

#define NDB_SF_MAGIC                    "NDBSCHMA"

// page size 4k
#define NDB_SF_PAGE_SIZE_IN_WORDS_LOG2  10
#define NDB_SF_PAGE_SIZE_IN_WORDS       (1 << NDB_SF_PAGE_SIZE_IN_WORDS_LOG2)
#define NDB_SF_PAGE_SIZE                (NDB_SF_PAGE_SIZE_IN_WORDS << 2)

// 4k = (1 + 127) * 32
#define NDB_SF_PAGE_ENTRIES             127

// 160 pages = 20320 objects
#define NDB_SF_MAX_PAGES                160

// versions where format changed
#define NDB_SF_VERSION_5_0_6            MAKE_VERSION(5, 0, 6)

// One page in schema file.
struct SchemaFile {
  // header size 32 bytes
  char Magic[8];
  Uint32 ByteOrder;
  Uint32 NdbVersion;
  Uint32 FileSize; // In bytes
  Uint32 PageNumber;
  Uint32 CheckSum; // Of this page
  Uint32 NoOfTableEntries; // On this page (NDB_SF_PAGE_ENTRIES)
  
  enum TableState {
    INIT = 0,
    ADD_STARTED = 1,
    TABLE_ADD_COMMITTED = 2,
    DROP_TABLE_STARTED = 3,
    DROP_TABLE_COMMITTED = 4,
    ALTER_TABLE_COMMITTED = 5,
    TEMPORARY_TABLE_COMMITTED = 6
  };

  // entry size 32 bytes
  struct TableEntry {
    Uint32 m_tableState;
    Uint32 m_tableVersion;
    Uint32 m_tableType;
    Uint32 m_info_words;
    Uint32 m_gcp;
    Uint32 m_unused[3];
    
    bool operator==(const TableEntry& o) const { 
      return memcmp(this, &o, sizeof(* this))== 0;
    }
  };

  // pre-5.0.6
  struct TableEntry_old {
    Uint32 m_tableState;
    Uint32 m_tableVersion;
    Uint32 m_tableType;
    Uint32 m_noOfPages;
    Uint32 m_gcp;
  };
  
  union {
  TableEntry TableEntries[NDB_SF_PAGE_ENTRIES];
  TableEntry_old TableEntries_old[1];
  };
};

#endif
