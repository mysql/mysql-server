/*
   Copyright (c) 2004, 2019, Oracle and/or its affiliates. All rights reserved.

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

enum AttrConvType { ACT_UNSUPPORTED = 0, ACT_PRESERVING = 1, ACT_LOSSY =-1,
                    ACT_STAGING_PRESERVING = 2, ACT_STAGING_LOSSY = -2 };
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
  BackupRestore(Ndb_cluster_connection *conn,
                NODE_GROUP_MAP *ng_map,
                uint ng_map_len,
                const char *instance_name,
                Uint32 parallelism) :
    m_ndb(NULL),
    m_cluster_connection(conn)
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
    m_metadata_work_requested = 0;
    m_restore = false;
    m_restore_meta = false;
    m_no_restore_disk = false;
    m_restore_epoch_requested = false;
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
    snprintf(m_instance_name, BackupRestore::INSTANCE_ID_LEN, "%s", instance_name);
  }
  
  virtual ~BackupRestore();
  virtual bool init(Uint32 tableChangesMask);
  virtual void release();
  virtual bool object(Uint32 type, const void* ptr);
  virtual bool table(const TableS &);
  virtual bool fk(Uint32 type, const void* ptr);
  virtual bool endOfTables();
  virtual bool endOfTablesFK();
  virtual void tuple(const TupleS &, Uint32 fragId);
  virtual void tuple_free();
  virtual void tuple_a(restore_callback_t *cb);
  virtual void tuple_SYSTAB_0(restore_callback_t *cb, const TableS &);
  virtual void cback(int result, restore_callback_t *cb);
  virtual bool errorHandler(restore_callback_t *cb);
  virtual void exitHandler();
  virtual void endOfTuples();
  virtual void logEntry(const LogEntry &);
  virtual void endOfLogEntrys();
  virtual bool prepare_staging(const TableS &);
  virtual bool finalize_staging(const TableS &);
  virtual bool finalize_table(const TableS &);
  virtual bool rebuild_indexes(const TableS&);
  virtual bool has_temp_error();
  virtual bool createSystable(const TableS & table);
  virtual bool table_compatible_check(TableS & tableS);
  virtual bool check_blobs(TableS & tableS); 
  virtual bool column_compatible_check(const char* tableName,
                                       const NDBCOL* backupCol, 
                                       const NDBCOL* dbCol);
  virtual bool update_apply_status(const RestoreMetaData &metaData);
  virtual bool report_started(unsigned node_id, unsigned backup_id);
  virtual bool report_meta_data(unsigned node_id, unsigned backup_id);
  virtual bool report_data(unsigned node_id, unsigned backup_id);
  virtual bool report_log(unsigned node_id, unsigned backup_id);
  virtual bool report_completed(unsigned node_id, unsigned backup_id);
  bool map_in_frm(char *new_data, const char *data,
                  uint data_len, uint *new_data_len) const;
  bool search_replace(char *search_str, char **new_data,
                      const char **data, const char *end_data,
                      uint *new_data_len) const;
  bool map_nodegroups(Uint32 *ng_array, Uint32 no_parts) const;
  Uint32 map_ng(Uint32 ng) const;
  bool translate_frm(NdbDictionary::Table *table) const;
  bool isMissingTable(const TableS& table);

  static AttrConvType check_compat_sizes(const NDBCOL &old_col,
                                         const NDBCOL &new_col);
  static AttrConvType check_compat_precision(const NDBCOL &old_col,
                                             const NDBCOL &new_col);
  static AttrConvType check_compat_char_binary(const NDBCOL &old_col,
                                               const NDBCOL &new_col);
  static AttrConvType check_compat_char_to_text(const NDBCOL &old_col,
                                                const NDBCOL &new_col);
  static AttrConvType check_compat_text_to_char(const NDBCOL &old_col,
                                                const NDBCOL &new_col);
  static AttrConvType check_compat_text_to_text(const NDBCOL &old_col,
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

  void update_next_auto_val(Uint32 orig_table_id,
                            Uint64 next_val);


  Ndb * m_ndb;
  Ndb_cluster_connection * m_cluster_connection;

  bool m_restore;

  // flags set on all threads to indicate that metadata/epoch restore
  // was requested - will ensure all threads init data needed by later
  // stages of ndb_restore
  bool m_metadata_work_requested;
  bool m_restore_epoch_requested;

  // flags set only on thread 1 to indicate that thread 1 must
  // do restore work, like restoring epoch/rebuilding indices
  bool m_restore_meta;
  bool m_restore_epoch;
  bool m_disable_indexes;
  bool m_rebuild_indexes;

  bool m_no_upgrade; // for upgrade ArrayType from 5.0 backup file.
  bool m_no_restore_disk;
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

  static const Uint32 INSTANCE_ID_LEN = 20;
  char m_instance_name[INSTANCE_ID_LEN];

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
  const NdbDictionary::Table* get_table(const TableS &);

  Vector<Uint64> m_auto_values;
  Vector<const NdbDictionary::Table*> m_indexes;
  Vector<Vector<NdbDictionary::Index *> > m_index_per_table; //
  Vector<NdbDictionary::Tablespace*> m_tablespaces;    // Index by id
  Vector<NdbDictionary::LogfileGroup*> m_logfilegroups;// Index by id
  Vector<NdbDictionary::HashMap*> m_hashmaps;
  Vector<const NdbDictionary::ForeignKey*> m_fks;

  static const PromotionRules m_allowed_promotion_attrs[];
};

#endif
