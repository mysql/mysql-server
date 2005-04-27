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

#ifndef DBDICT_SCHEMA_FILE_HPP
#define DBDICT_SCHEMA_FILE_HPP

#include <ndb_types.h>
#include <string.h>

struct SchemaFile {
  char Magic[8];
  Uint32 ByteOrder;
  Uint32 NdbVersion;
  Uint32 FileSize; // In bytes
  Uint32 Unused;
  
  Uint32 CheckSum;
  
  enum TableState {
    INIT = 0,
    ADD_STARTED = 1,
    TABLE_ADD_COMMITTED = 2,
    DROP_TABLE_STARTED = 3,
    DROP_TABLE_COMMITTED = 4,
    ALTER_TABLE_COMMITTED = 5
  };

  struct TableEntry {
    Uint32 m_tableState;
    Uint32 m_tableVersion;
    Uint32 m_tableType;
    Uint32 m_noOfPages;
    Uint32 m_gcp;
    
    bool operator==(const TableEntry& o) const { 
      return memcmp(this, &o, sizeof(* this))== 0;
    }
  };
  
  Uint32 NoOfTableEntries;
  TableEntry TableEntries[1];
};

#endif
