/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include "Restore.hpp"
#include "ndb_nodegroup_map.h"
#include "sql/ha_ndbcluster_tables.h"

class BackupConsumer {
public:
  BackupConsumer() {}
  virtual ~BackupConsumer() { }
  virtual bool init(Uint32 tableCompabilityMask) { return true;}
  virtual bool object(Uint32 tableType, const void*) { return true;}
  virtual bool table(const TableS &){return true;}
  virtual bool fk(Uint32 tableType, const void*) { return true;}
  virtual bool endOfTables() { return true; }
  virtual bool endOfTablesFK() { return true; }
  virtual void tuple(const TupleS &, Uint32 fragId){}
  virtual void tuple_free(){}
  virtual void endOfTuples(){}
  virtual void logEntry(const LogEntry &){}
  virtual void endOfLogEntrys(){}
  virtual bool prepare_staging(const TableS &){return true;}
  virtual bool finalize_staging(const TableS &){return true;}
  virtual bool finalize_table(const TableS &){return true;}
  virtual bool rebuild_indexes(const TableS &) { return true;}
  virtual bool createSystable(const TableS &){ return true;}
  virtual bool update_apply_status(const RestoreMetaData &metaData){return true;}
  virtual bool report_started(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_meta_data(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_data(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_log(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_completed(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool isMissingTable(const TableS &){return false;}
  NODE_GROUP_MAP *m_nodegroup_map;
  uint            m_nodegroup_map_len;
  virtual bool has_temp_error() {return false;}
  virtual bool table_equal(const TableS &) { return true; }
  virtual bool table_compatible_check(TableS &) {return true;}
  virtual bool check_blobs(TableS &) {return true;}
};

#endif
