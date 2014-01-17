/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "Ndbcntr.hpp"

#define JAM_FILE_ID 460


#define arrayLength(x) sizeof(x)/sizeof(x[0])

// SYSTAB_0

static const Ndbcntr::SysColumn
column_SYSTAB_0[] = {
  { 0, "SYSKEY_0",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  { 1, "NEXTID",
    DictTabInfo::ExtBigunsigned, 1,
    false, false
  }
};

const Ndbcntr::SysTable
Ndbcntr::g_sysTable_SYSTAB_0 = {
  "sys/def/SYSTAB_0",
  arrayLength(column_SYSTAB_0), column_SYSTAB_0,
  DictTabInfo::SystemTable,
  DictTabInfo::HashMapPartition,
  true, UINT_MAX32, UINT_MAX32
};

// NDB$EVENTS_0

static const Ndbcntr::SysColumn
column_NDBEVENTS_0[] = {
  { 0, "NAME",
    DictTabInfo::ExtBinary, MAX_TAB_NAME_SIZE,
    true, false
  },
  { 1, "EVENT_TYPE",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 2, "TABLEID",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 3, "TABLEVERSION",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 4, "TABLE_NAME",
    DictTabInfo::ExtBinary, MAX_TAB_NAME_SIZE,
    false, false
  },
  { 5, "ATTRIBUTE_MASK",
    DictTabInfo::ExtUnsigned, MAXNROFATTRIBUTESINWORDS_OLD,
    false, false
  },
  { 6, "SUBID",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 7, "SUBKEY",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 8, "ATTRIBUTE_MASK2",
    DictTabInfo::ExtLongvarbinary, MAX_ATTRIBUTES_IN_TABLE_NDB_EVENTS_0 / 8,
    false, true
  }
};

Ndbcntr::SysTable
Ndbcntr::g_sysTable_NDBEVENTS_0 = {
  "sys/def/NDB$EVENTS_0",
  arrayLength(column_NDBEVENTS_0), column_NDBEVENTS_0,
  DictTabInfo::SystemTable,
  DictTabInfo::HashMapPartition,
  true, UINT_MAX32, UINT_MAX32
};

// all

const Ndbcntr::SysTable*
Ndbcntr::g_sysTableList[] = {
  &g_sysTable_SYSTAB_0,
  &g_sysTable_NDBEVENTS_0
};

//TODO Backup needs this info to allocate appropriate number of records
//BackupInit.cpp
const unsigned
Ndbcntr::g_sysTableCount = arrayLength(Ndbcntr::g_sysTableList);
