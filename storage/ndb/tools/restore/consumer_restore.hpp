/*
   Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONSUMER_RESTORE_HPP
#define CONSUMER_RESTORE_HPP

#include "consumer.hpp"

bool map_nodegroups(Uint32 *ng_array, Uint32 no_parts);

struct restore_callback_t {
  class BackupRestore *restore;
  class TupleS tup;
  class NdbTransaction *connection;
  int    retries;
  int error_code;
  Uint32 fragId;
  Uint32 n_bytes;
  restore_callback_t *next;
};

struct char_n_padding_struct {
Uint32 n_old; // used also for time precision
Uint32 n_new;
char new_row[1];
};

enum AttrConvType { ACT_UNSUPPORTED = 0, ACT_PRESERVING = 1, ACT_LOSSY =-1 };
typedef  AttrConvType (*AttrCheckCompatFunc)(const NDBCOL &old_col,
                                             const NDBCOL &new_col);

struct PromotionRules {
  NDBCOL::Type old_type;
  NDBCOL::Type new_type;
  AttrCheckCompatFunc  attr_check_compatability;
  AttrConvertFunc attr_convert;
};

class BackupRestore : public BackupConsumer 
{
public:
  BackupRestore(const char* ndb_connectstring,
                int ndb_nodeid,
                NODE_GROUP_MAP *ng_map,
                uint ng_map_len,
                Uint32 parallelism=1) :
    m_ndb(NULL),
    m_cluster_connection(NULL),
    m_ndb_connectstring(ndb_connectstring),
    m_ndb_nodeid(ndb_nodeid)
  {
    m_nodegroup_map = ng_map;
    m_nodegroup_map_len = ng_map_len;
    m_n_tablespace = 0;
    m_n_logfilegroup = 0;
    m_n_datafile = 0;
    m_n_undofile = 0;
    m_n_tables = 0;
    m_logBytes = m_dataBytes = 0;
    m_logCount = m_dataCount = 0;
    m_restore = false;
    m_restore_meta = false;
    m_no_restore_disk = false;
    m_restore_epoch = false;
    m_parallelism = parallelism;
    m_callback = 0;
    m_free_callback = 0;
    m_temp_error = false;
    m_no_upgrade = false;
    m_tableChangesMask = 0;
    m_preserve_trailing_spaces = false;
    m_transactions = 0;
    m_cache.m_old_table = 0;
    m_disable_indexes = false;
    m_rebuild_indexes = false;
  }
  
  virtual ~BackupRestore();
  virtual bool init(Uint32 tableChangesMask);
  virtual void release();
  virtual bool object(Uint32 type, const void* ptr);
  virtual bool table(const TableS &);
  virtual bool endOfTables();
  virtual void tuple(const TupleS &, Uint32 fragId);
  virtual void tuple_free();
  virtual void tuple_a(restore_callback_t *cb);
  virtual void tuple_SYSTAB_0(restore_callback_t *cb, const TableS &);
  virtual int restoreAutoIncrement(restore_callback_t *cb,
                                    Uint32 tableId, Uint64 value);
  virtual void cback(int result, restore_callback_t *cb);
  virtual bool errorHandler(restore_callback_t *cb);
  virtual void exitHandler();
  virtual void endOfTuples();
  virtual void logEntry(const LogEntry &);
  virtual void endOfLogEntrys();
  virtual bool finalize_table(const TableS &);
  virtual bool rebuild_indexes(const TableS&);
  virtual bool has_temp_error();
  virtual bool createSystable(const TableS & table);
  virtual bool table_compatible_check(const TableS & tableS);
  virtual bool column_compatible_check(const char* tableName,
                                       const NDBCOL* backupCol, 
                                       const NDBCOL* dbCol);
  virtual bool update_apply_status(const RestoreMetaData &metaData);
  virtual bool report_started(unsigned node_id, unsigned backup_id);
  virtual bool report_meta_data(unsigned node_id, unsigned backup_id);
  virtual bool report_data(unsigned node_id, unsigned backup_id);
  virtual bool report_log(unsigned node_id, unsigned backup_id);
  virtual bool report_completed(unsigned node_id, unsigned backup_id);
  void connectToMysql();
  bool map_in_frm(char *new_data, const char *data,
                  uint data_len, uint *new_data_len);
  bool search_replace(char *search_str, char **new_data,
                      const char **data, const char *end_data,
                      uint *new_data_len);
  bool map_nodegroups(Uint32 *ng_array, Uint32 no_parts);
  Uint32 map_ng(Uint32 ng);
  bool translate_frm(NdbDictionary::Table *table);

  static AttrConvType check_compat_sizes(const NDBCOL &old_col,
                                         const NDBCOL &new_col);
  static AttrConvType check_compat_precision(const NDBCOL &old_col,
                                             const NDBCOL &new_col);
  static AttrConvType check_compat_promotion(const NDBCOL &old_col,
                                             const NDBCOL &new_col);
  static AttrConvType check_compat_lossy(const NDBCOL &old_col,
                                         const NDBCOL &new_col);

  // bitset conversion handler
  static void*
  convert_bitset(const void * source, void * target, bool &truncated);

  // char/binary array conversion handler
  template< typename S, typename T >
  static void *
  convert_array(const void * source, void * target, bool & truncated);

  // integral type conversion handler
  // (follows MySQL replication semantics truncating to nearest legal value)
  template< typename T, typename S >
  static void *
  convert_integral(const void * source, void * target, bool & truncated);

  // times with fractional seconds, see wl#946
  static void*
  convert_time_time2(const void * source, void * target, bool &);
  static void*
  convert_time2_time(const void * source, void * target, bool &);
  static void*
  convert_time2_time2(const void * source, void * target, bool &);
  static void*
  convert_datetime_datetime2(const void * source, void * target, bool &);
  static void*
  convert_datetime2_datetime(const void * source, void * target, bool &);
  static void*
  convert_datetime2_datetime2(const void * source, void * target, bool &);
  static void*
  convert_timestamp_timestamp2(const void * source, void * target, bool &);
  static void*
  convert_timestamp2_timestamp(const void * source, void * target, bool &);
  static void*
  convert_timestamp2_timestamp2(const void * source, void * target, bool &);

  // returns the handler function checking type conversion compatibility
  AttrCheckCompatFunc 
  get_attr_check_compatability(const NDBCOL::Type &old_type,
                               const NDBCOL::Type &new_type);

  // returns the handler function converting a value
  AttrConvertFunc
  get_convert_func(const NDBCOL::Type &old_type,
                   const NDBCOL::Type &new_type);

  Ndb * m_ndb;
  Ndb_cluster_connection * m_cluster_connection;
  const char* m_ndb_connectstring;
  int m_ndb_nodeid;
  bool m_restore;
  bool m_restore_meta;
  bool m_no_restore_disk;
  bool m_restore_epoch;
  bool m_no_upgrade; // for upgrade ArrayType from 5.0 backup file.
  bool m_disable_indexes;
  bool m_rebuild_indexes;
  Uint32 m_tableChangesMask;
  static bool m_preserve_trailing_spaces;

  Uint32 m_n_tablespace;
  Uint32 m_n_logfilegroup;
  Uint32 m_n_datafile;
  Uint32 m_n_undofile;
  Uint32 m_n_tables;

  Uint64 m_logBytes;
  Uint64 m_dataBytes;

  Uint32 m_logCount;
  Uint32 m_dataCount;

  Uint32 m_parallelism;
  volatile Uint32 m_transactions;

  restore_callback_t *m_callback;
  restore_callback_t *m_free_callback;
  bool m_temp_error;

  /**
   * m_new_table_ids[X] = Y;
   *   X - old table id
   *   Y != 0  - new table
   */
  Vector<const NdbDictionary::Table*> m_new_tables;
  struct {
    const NdbDictionary::Table* m_old_table;
    const NdbDictionary::Table* m_new_table;
  } m_cache;
  const NdbDictionary::Table* get_table(const NdbDictionary::Table* );

  Vector<const NdbDictionary::Table*> m_indexes;
  Vector<Vector<NdbDictionary::Index *> > m_index_per_table; //
  Vector<NdbDictionary::Tablespace*> m_tablespaces;    // Index by id
  Vector<NdbDictionary::LogfileGroup*> m_logfilegroups;// Index by id
  Vector<NdbDictionary::HashMap*> m_hashmaps;

  static const PromotionRules m_allowed_promotion_attrs[];
};

#endif
