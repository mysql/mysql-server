/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
  true, 0, 0, ~0, ~0
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
  true, 0, 0, ~0, ~0
};

// NDB$IS_HEAD

static const Ndbcntr::SysColumn
column_NDBIS_HEAD[] = {
  // key must be first
  { 0, "INDEX_ID",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  { 1, "INDEX_VERSION",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  // table
  { 2, "TABLE_ID",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 3, "FRAG_COUNT",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 4, "VALUE_FORMAT",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  // current sample
  { 5, "SAMPLE_VERSION",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 6, "LOAD_TIME",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 7, "SAMPLE_COUNT",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  },
  { 8, "KEY_BYTES",
    DictTabInfo::ExtUnsigned, 1,
    false, false
  }
};

const Ndbcntr::SysTable
Ndbcntr::g_sysTable_NDBIS_HEAD = {
  "sys/def/NDB$IS_HEAD",
  arrayLength(column_NDBIS_HEAD), column_NDBIS_HEAD,
  DictTabInfo::SystemTable,
  DictTabInfo::HashMapPartition,
  true, 0, 0, ~0, ~0
};

// NDB$IS_SAMPLE

static const Ndbcntr::SysColumn
column_NDBIS_SAMPLE[] = {
  // key must be first
  { 0, "INDEX_ID",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  { 1, "INDEX_VERSION",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  { 2, "SAMPLE_VERSION",
    DictTabInfo::ExtUnsigned, 1,
    true, false
  },
  { 3, "STAT_KEY",
    DictTabInfo::ExtLongvarbinary, MAX_INDEX_STAT_KEY_SIZE * 4,
    true, false
  },
  // value
  { 4, "STAT_VALUE",
    DictTabInfo::ExtLongvarbinary, MAX_INDEX_STAT_VALUE_CSIZE * 4,
    false, false
  }
};

const Ndbcntr::SysIndex
Ndbcntr::g_sysIndex_NDBIS_SAMPLE_X1 = {
    "sys/def/%u/NDB$IS_SAMPLE_X1",
    3, { 0, 1, 2, ~0 },
    DictTabInfo::OrderedIndex,
    false, ~0, ~0, ~0
};

static const Ndbcntr::SysIndex*
index_NDBIS_SAMPLE[] = {
  &Ndbcntr::g_sysIndex_NDBIS_SAMPLE_X1
};

const Ndbcntr::SysTable
Ndbcntr::g_sysTable_NDBIS_SAMPLE = {
  "sys/def/NDB$IS_SAMPLE",
  arrayLength(column_NDBIS_SAMPLE), column_NDBIS_SAMPLE,
  DictTabInfo::SystemTable,
  DictTabInfo::HashMapPartition,
  true,
  arrayLength(index_NDBIS_SAMPLE), index_NDBIS_SAMPLE,
  ~0, ~0
};

// all

const Ndbcntr::SysTable*
Ndbcntr::g_sysTableList[] = {
  &g_sysTable_SYSTAB_0,
  &g_sysTable_NDBEVENTS_0,
  &g_sysTable_NDBIS_HEAD,
  &g_sysTable_NDBIS_SAMPLE
};

//TODO Backup needs this info to allocate appropriate number of records
//BackupInit.cpp
const unsigned
Ndbcntr::g_sysTableCount = arrayLength(Ndbcntr::g_sysTableList);
