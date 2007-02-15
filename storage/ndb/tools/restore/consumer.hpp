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

#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include "Restore.hpp"
#include "ndb_nodegroup_map.h"

#include "../../../../sql/ha_ndbcluster_tables.h"
extern const char *Ndb_apply_table;

class BackupConsumer {
public:
  BackupConsumer() {}
  virtual ~BackupConsumer() { }
  virtual bool init() { return true;}
  virtual bool object(Uint32 tableType, const void*) { return true;}
  virtual bool table(const TableS &){return true;}
  virtual bool endOfTables() { return true; }
  virtual void tuple(const TupleS &, Uint32 fragId){}
  virtual void tuple_free(){}
  virtual void endOfTuples(){}
  virtual void logEntry(const LogEntry &){}
  virtual void endOfLogEntrys(){}
  virtual bool finalize_table(const TableS &){return true;}
  virtual bool createSystable(const TableS &){ return true;}
  virtual bool update_apply_status(const RestoreMetaData &metaData){return true;}
  NODE_GROUP_MAP *m_nodegroup_map;
  uint            m_nodegroup_map_len;
  virtual bool has_temp_error() {return false;}
  virtual bool table_equal(const TableS &) {return true;}
};

#endif
