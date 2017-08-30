#ifndef HA_PARTITION_INCLUDED
#define HA_PARTITION_INCLUDED

/*
   Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "partitioning/partition_handler.h"  /* Partition_handler */


/** class where to save partitions Handler_share's */
class Parts_share_refs
{
public:
  uint num_parts;                              /**< Size of ha_share array */
  Handler_share **ha_shares;                   /**< Storage for each part */
  Parts_share_refs();
  ~Parts_share_refs();
  bool init(uint arg_num_parts);
};


/**
  Partition specific Handler_share.
*/
class Ha_partition_share : public Partition_share
{
public:
  /** Storage for each partitions Handler_share */
  Parts_share_refs *partitions_share_refs;
  Ha_partition_share();
  ~Ha_partition_share();
  bool init(uint num_parts);
};

class ha_partition :
	public handler,
	public Partition_helper,
	public Partition_handler
{
private:
  /* Data for the partition handler */
  int  m_mode;                          // Open mode
  uint m_open_test_lock;                // Open test_if_locked
  char *m_file_buffer;                  // Content of the .par file
  char *m_name_buffer_ptr;              // Pointer to first partition name
  plugin_ref *m_engine_array;           // Array of types of the handlers
  handler **m_file;                     // Array of references to handler inst.
  uint m_file_tot_parts;                // Debug
  /*
    Since the partition handler is a handler on top of other handlers, it
    is necessary to keep information about what the underlying handler
    characteristics is. It is not possible to keep any handler instances
    for this since the MySQL Server sometimes allocating the handler object
    without freeing them.
  */
  ulong m_low_byte_first;
  enum enum_handler_status
  {
    handler_not_initialized= 0,
    handler_initialized,
    handler_opened,
    handler_closed
  };
  enum_handler_status m_handler_status;

  uint m_num_locks;                       // For engines like ha_blackhole, which needs no locks

  /**
    Array of new partitions used during
    fast_alter_part_table() / ALTER TABLE ... ADD/DROP/REORGANIZE... PARTITION.
  */
  handler **m_new_file;
  /** Maximum of new partitions in m_new_file. */
  uint m_num_new_partitions;
  /** True if the new partitions should be created but not opened and locked. */
  bool m_new_parts_open_only;
  /** cached value of indexes_are_disabled(). */
  int m_indexes_are_disabled;
  /*
    If set, this object was created with ha_partition::clone and doesn't
    "own" the m_part_info structure.
  */
  ha_partition *m_is_clone_of;
  MEM_ROOT *m_clone_mem_root;

  /*
    We keep track if all underlying handlers are MyISAM since MyISAM has a
    great number of extra flags not needed by other handlers.
  */
  bool m_myisam;                         // Are all underlying handlers
                                         // MyISAM
  /*
    We keep track of InnoDB handlers below since it requires proper setting
    of query_id in fields at index_init and index_read calls.
  */
  bool m_innodb;                        // Are all underlying handlers
                                        // InnoDB
  /*
    When calling extra(HA_EXTRA_CACHE) we do not pass this to the underlying
    handlers immediately. Instead we cache it and call the underlying
    immediately before starting the scan on the partition. This is to
    prevent allocating a READ CACHE for each partition in parallel when
    performing a full table scan on MyISAM partitioned table.
    This state is cleared by extra(HA_EXTRA_NO_CACHE).
  */
  bool m_extra_cache;
  uint m_extra_cache_size;
  /* The same goes for HA_EXTRA_PREPARE_FOR_UPDATE */
  bool m_extra_prepare_for_update;
  /* Which partition has active cache */
  uint m_extra_cache_part_id;

  void init_handler_variables();
  /*
    Variables for lock structures.
  */
  THR_LOCK_DATA lock;                   /* MySQL lock */

  /** For optimizing ha_start_bulk_insert calls */
  MY_BITMAP m_bulk_insert_started;
  ha_rows   m_bulk_inserted_rows;
  /** used for prediction of start_bulk_insert rows */
  enum_monotonicity_info m_part_func_monotonicity_info;
  /** keep track of locked partitions */
  MY_BITMAP m_locked_partitions;
  /** Stores shared auto_increment etc. */
  Ha_partition_share *part_share;
  /** Temporary storage for new partitions Handler_shares during ALTER */
  List<Parts_share_refs> m_new_partitions_share_refs;
  /** Sorted array of partition ids in descending order of number of rows. */
  uint32 *m_part_ids_sorted_by_num_of_records;
  /* Compare function for my_qsort2, for reversed order. */
  static int compare_number_of_records(ha_partition *me,
                                       const uint32 *a,
                                       const uint32 *b);
  /** keep track of partitions to call ha_reset */
  MY_BITMAP m_partitions_to_reset;
public:
  handler *clone(const char *name, MEM_ROOT *mem_root);
  /*
    -------------------------------------------------------------------------
    MODULE create/delete handler object
    -------------------------------------------------------------------------
    Object create/delete method. The normal called when a table object
    exists. There is also a method to create the handler object with only
    partition information. This is used from mysql_create_table when the
    table is to be created and the engine type is deduced to be the
    partition handler.
    -------------------------------------------------------------------------
  */
    ha_partition(handlerton *hton, TABLE_SHARE * table);
    ha_partition(handlerton *hton, TABLE_SHARE *share,
                 partition_info *part_info_arg,
                 ha_partition *clone_arg,
                 MEM_ROOT *clone_mem_root_arg);
   ~ha_partition();
  /*
    A partition handler has no characteristics in itself. It only inherits
    those from the underlying handlers. Here we set-up those constants to
    enable later calls of the methods to retrieve constants from the under-
    lying handlers. Returns false if not successful.
  */
   bool initialize_partition(MEM_ROOT *mem_root);

  /*
    -------------------------------------------------------------------------
    MODULE meta data changes
    -------------------------------------------------------------------------
    Meta data routines to CREATE, DROP, RENAME table and often used at
    ALTER TABLE (update_create_info used from ALTER TABLE and SHOW ..).

    create_handler_files is called before opening a new handler object
    with openfrm to call create. It is used to create any local handler
    object needed in opening the object in openfrm
    -------------------------------------------------------------------------
  */
  virtual int delete_table(const char *from);
  virtual int rename_table(const char *from, const char *to);
  virtual int create(const char *name, TABLE *form,
                     HA_CREATE_INFO *create_info);
  virtual int create_handler_files(const char *name,
                                   const char *old_name, int action_flag,
                                   HA_CREATE_INFO *create_info);
  virtual void update_create_info(HA_CREATE_INFO *create_info);
  int change_partitions_low(HA_CREATE_INFO *create_info,
                            const char *path,
                            ulonglong * const copied,
                            ulonglong * const deleted)
  {
    return Partition_helper::change_partitions(create_info,
                                               path,
                                               copied,
                                               deleted);
  }
private:
  bool get_num_parts(const char *name, uint *num_parts)
  {
    DBUG_ENTER("ha_partition::get_num_parts");
    *num_parts= m_tot_parts;
    DBUG_RETURN(0);
  }
  virtual void change_table_ptr(TABLE *table_arg, TABLE_SHARE *share);
  virtual bool check_if_incompatible_data(HA_CREATE_INFO *create_info,
                                          uint table_changes);
  int prepare_for_new_partitions(uint num_partitions,
                                 bool only_create);
  int create_new_partition(TABLE *table,
                           HA_CREATE_INFO *create_info,
                           const char *part_name,
                           uint new_part_id,
                           partition_element *p_elem);
  int write_row_in_new_part(uint part_id);
  void close_new_partitions();
  /*
    delete_table and rename_table uses very similar logic which
    is packed into this routine.
  */
  int del_ren_table(const char *from, const char *to);
  /*
    One method to create the table_name.par file containing the names of the
    underlying partitions, their engine and the number of partitions.
    And one method to read it in.
  */
  bool create_handler_file(const char *name);
  bool setup_engine_array(MEM_ROOT *mem_root);
  bool read_par_file(const char *name);
  bool get_from_handler_file(const char *name, MEM_ROOT *mem_root,
                             bool is_clone);
  bool new_handlers_from_part_info(MEM_ROOT *mem_root);
  bool create_handlers(MEM_ROOT *mem_root);
  void clear_handler_file();
  partition_element *find_partition_element(uint part_id);
  bool populate_partition_name_hash();
  Ha_partition_share *get_share();
  bool set_ha_share_ref(Handler_share **ha_share);
  void fix_data_dir(char* path);
  bool init_partition_bitmaps();
  void free_partition_bitmaps();

public:

  /*
    -------------------------------------------------------------------------
    MODULE open/close object
    -------------------------------------------------------------------------
    Open and close handler object to ensure all underlying files and
    objects allocated and deallocated for query handling is handled
    properly.
    -------------------------------------------------------------------------

    A handler object is opened as part of its initialisation and before
    being used for normal queries (not before meta-data changes always.
    If the object was opened it will also be closed before being deleted.
  */
  virtual int open(const char *name, int mode, uint test_if_locked);
  virtual int close(void);

  /*
    -------------------------------------------------------------------------
    MODULE start/end statement
    -------------------------------------------------------------------------
    This module contains methods that are used to understand start/end of
    statements, transaction boundaries, and aid for proper concurrency
    control.
    The partition handler need not implement abort and commit since this
    will be handled by any underlying handlers implementing transactions.
    There is only one call to each handler type involved per transaction
    and these go directly to the handlers supporting transactions
    currently InnoDB, BDB and NDB).
    -------------------------------------------------------------------------
  */
  virtual THR_LOCK_DATA **store_lock(THD * thd, THR_LOCK_DATA ** to,
                                     enum thr_lock_type lock_type);
  virtual int external_lock(THD * thd, int lock_type);
  /*
    When table is locked a statement is started by calling start_stmt
    instead of external_lock
  */
  virtual int start_stmt(THD * thd, thr_lock_type lock_type);
  /*
    Lock count is number of locked underlying handlers (I assume)
  */
  virtual uint lock_count(void) const;
  /*
    Call to unlock rows not to be updated in transaction
  */
  virtual void unlock_row();
  /*
    Check if semi consistent read
  */
  virtual bool was_semi_consistent_read();
  /*
    Call to hint about semi consistent read
  */
  virtual void try_semi_consistent_read(bool);

  /*
    NOTE: due to performance and resource issues with many partitions,
    we only use the m_psi on the ha_partition handler, excluding all
    partitions m_psi.
  */
#ifdef HAVE_M_PSI_PER_PARTITION
  /*
    Bind the table/handler thread to track table i/o.
  */
  virtual void unbind_psi();
  virtual void rebind_psi();
#endif
  /*
    -------------------------------------------------------------------------
    MODULE change record
    -------------------------------------------------------------------------
    This part of the handler interface is used to change the records
    after INSERT, DELETE, UPDATE, REPLACE method calls but also other
    special meta-data operations as ALTER TABLE, LOAD DATA, TRUNCATE.
    -------------------------------------------------------------------------

    These methods are used for insert (write_row), update (update_row)
    and delete (delete_row). All methods to change data always work on
    one row at a time. update_row and delete_row also contains the old
    row.
    delete_all_rows will delete all rows in the table in one call as a
    special optimization for DELETE from table;

    Bulk inserts are supported if all underlying handlers support it.
    start_bulk_insert and end_bulk_insert is called before and after a
    number of calls to write_row.
  */
  int write_row(uchar *buf)
  {
    return Partition_helper::ph_write_row(buf);
  }
  int update_row(const uchar *old_data, uchar *new_data)
  {
    return Partition_helper::ph_update_row(old_data, new_data);
  }
  int delete_row(const uchar *buf)
  {
    return Partition_helper::ph_delete_row(buf);
  }
  virtual int delete_all_rows(void);
  virtual int truncate();
  virtual void start_bulk_insert(ha_rows rows);
  virtual int end_bulk_insert();
private:
  ha_rows guess_bulk_insert_rows();
  void start_part_bulk_insert(THD *thd, uint part_id);
  long estimate_read_buffer_size(long original_size);
public:

  /*
    Method for truncating a specific partition.
    (i.e. ALTER TABLE t1 TRUNCATE PARTITION p).

    @remark This method is a partitioning-specific hook
            and thus not a member of the general SE API.
  */
  int truncate_partition_low();

  virtual bool is_ignorable_error(int error)
  {
    if (handler::is_ignorable_error(error) ||
        error == HA_ERR_NO_PARTITION_FOUND ||
        error == HA_ERR_NOT_IN_LOCK_PARTITIONS)
      return true;
    return false;
  }


  /*
    -------------------------------------------------------------------------
    MODULE full table scan
    -------------------------------------------------------------------------
    This module is used for the most basic access method for any table
    handler. This is to fetch all data through a full table scan. No
    indexes are needed to implement this part.
    It contains one method to start the scan (rnd_init) that can also be
    called multiple times (typical in a nested loop join). Then proceeding
    to the next record (rnd_next) and closing the scan (rnd_end).
    To remember a record for later access there is a method (position)
    and there is a method used to retrieve the record based on the stored
    position.
    The position can be a file position, a primary key, a ROWID dependent
    on the handler below.
    -------------------------------------------------------------------------
  */
  /*
    unlike index_init(), rnd_init() can be called two times
    without rnd_end() in between (it only makes sense if scan=1).
    then the second call should prepare for the new table scan
    (e.g if rnd_init allocates the cursor, second call should
    position it to the start of the table, no need to deallocate
    and allocate it again
  */
  int rnd_init(bool scan)
  {
    return Partition_helper::ph_rnd_init(scan);
  }
  int rnd_end()
  {
    return Partition_helper::ph_rnd_end();
  }
  int rnd_next(uchar *buf)
  {
    return Partition_helper::ph_rnd_next(buf);
  }
  int rnd_pos(uchar *buf, uchar *pos)
  {
    return Partition_helper::ph_rnd_pos(buf, pos);
  }
  int rnd_pos_by_record(uchar *record)
  {
    if (unlikely(get_part_for_delete(record,
                                     m_table->record[0],
                                     m_part_info,
                                     &m_last_part))) {
      return(HA_ERR_INTERNAL_ERROR);
    }
    return(m_file[m_last_part]->rnd_pos_by_record(record));
  }
  void position(const uchar *record)
  {
    return Partition_helper::ph_position(record);
  }

  /*
    -------------------------------------------------------------------------
    MODULE index scan
    -------------------------------------------------------------------------
    This part of the handler interface is used to perform access through
    indexes. The interface is defined as a scan interface but the handler
    can also use key lookup if the index is a unique index or a primary
    key index.
    Index scans are mostly useful for SELECT queries but are an important
    part also of UPDATE, DELETE, REPLACE and CREATE TABLE table AS SELECT
    and so forth.
    Naturally an index is needed for an index scan and indexes can either
    be ordered, hash based. Some ordered indexes can return data in order
    but not necessarily all of them.
    There are many flags that define the behavior of indexes in the
    various handlers. These methods are found in the optimizer module.
    -------------------------------------------------------------------------

    index_read is called to start a scan of an index. The find_flag defines
    the semantics of the scan. These flags are defined in
    include/my_base.h
    index_read_idx is the same but also initializes index before calling doing
    the same thing as index_read. Thus it is similar to index_init followed
    by index_read. This is also how we implement it.

    index_read/index_read_idx does also return the first row. Thus for
    key lookups, the index_read will be the only call to the handler in
    the index scan.

    index_init initializes an index before using it and index_end does
    any end processing needed.
  */
  int index_read_map(uchar *buf,
                     const uchar *key,
                     key_part_map keypart_map,
                     enum ha_rkey_function find_flag)
  {
    return Partition_helper::ph_index_read_map(buf,
                                               key,
                                               keypart_map,
                                               find_flag);
  }
  virtual int index_init(uint idx, bool sorted)
  {
    return Partition_helper::ph_index_init(idx, sorted);
  }
  virtual int index_end()
  {
    return Partition_helper::ph_index_end();
  }

  /**
    @breif
    Positions an index cursor to the index specified in the hanlde. Fetches the
    row if available. If the key value is null, begin at first key of the
    index.
  */
  int index_read_idx_map(uchar *buf,
                         uint index,
                         const uchar *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag)
  {
    return Partition_helper::ph_index_read_idx_map(buf,
                                                   index,
                                                   key,
                                                   keypart_map,
                                                   find_flag);
  }
  /*
    These methods are used to jump to next or previous entry in the index
    scan. There are also methods to jump to first and last entry.
  */
  int index_next(uchar *buf)
  {
    return Partition_helper::ph_index_next(buf);
  }
  int index_prev(uchar *buf)
  {
    return Partition_helper::ph_index_prev(buf);
  }
  int index_first(uchar *buf)
  {
    return Partition_helper::ph_index_first(buf);
  }
  int index_last(uchar *buf)
  {
    return Partition_helper::ph_index_last(buf);
  }
  int index_next_same(uchar *buf, const uchar *key, uint keylen)
  {
    return Partition_helper::ph_index_next_same(buf, key, keylen);
  }
  int index_read_last_map(uchar *buf,
                          const uchar *key,
                          key_part_map keypart_map)
  {
    return Partition_helper::ph_index_read_last_map(buf, key, keypart_map);
  }

  /*
    read_first_row is virtual method but is only implemented by
    handler.cc, no storage engine has implemented it so neither
    will the partition handler.

    virtual int read_first_row(uchar *buf, uint primary_key);
  */

  /*
    We don't implement multi read range yet, will do later.
    virtual int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
    KEY_MULTI_RANGE *ranges, uint range_count,
    bool sorted, HANDLER_BUFFER *buffer);
    virtual int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
  */

  int read_range_first(const key_range *start_key,
                       const key_range *end_key,
                       bool eq_range,
                       bool sorted)
  {
    return Partition_helper::ph_read_range_first(start_key,
                                                 end_key,
                                                 eq_range,
                                                 sorted);
  }
  int read_range_next()
  {
    return Partition_helper::ph_read_range_next();
  }
public:
  /*
    -------------------------------------------------------------------------
    MODULE information calls
    -------------------------------------------------------------------------
    This calls are used to inform the handler of specifics of the ongoing
    scans and other actions. Most of these are used for optimisation
    purposes.
    -------------------------------------------------------------------------
  */
  virtual int info(uint);
  void get_dynamic_partition_info(ha_statistics *stat_info,
                                  ha_checksum *check_sum,
                                  uint part_id);
  virtual int extra(enum ha_extra_function operation);
  virtual int extra_opt(enum ha_extra_function operation, ulong cachesize);
  virtual int reset(void);
  /*
    Do not allow caching of partitioned tables, since we cannot return
    a callback or engine_data that would work for a generic engine.
  */
  virtual my_bool register_query_cache_table(THD *thd, char *table_key,
                                             size_t key_length,
                                             qc_engine_callback
                                               *engine_callback,
                                             ulonglong *engine_data)
  {
    *engine_callback= NULL;
    *engine_data= 0;
    return FALSE;
  }

private:
  static const uint NO_CURRENT_PART_ID;
  int loop_extra(enum ha_extra_function operation);
  void late_extra_cache(uint partition_id);
  void late_extra_no_cache(uint partition_id);
  void prepare_extra_cache(uint cachesize);
public:

  /*
    -------------------------------------------------------------------------
    MODULE optimizer support
    -------------------------------------------------------------------------
  */

  /*
    NOTE !!!!!!
     -------------------------------------------------------------------------
     -------------------------------------------------------------------------
     One important part of the public handler interface that is not depicted in
     the methods is the attribute records which is defined in the base class.
     This is looked upon directly and is set by calling info(HA_STATUS_INFO) ?
     -------------------------------------------------------------------------
  */

private:
  /* Helper functions for optimizer hints. */
  ha_rows min_rows_for_estimate();
  uint get_biggest_used_partition(uint *part_index);
public:

  /*
    keys_to_use_for_scanning can probably be implemented as the
    intersection of all underlying handlers if mixed handlers are used.
    This method is used to derive whether an index can be used for
    index-only scanning when performing an ORDER BY query.
    Only called from one place in sql_select.cc
  */
  virtual const key_map *keys_to_use_for_scanning();

  /*
    Called in test_quick_select to determine if indexes should be used.
  */
  virtual double scan_time();

  /*
    The next method will never be called if you do not implement indexes.
  */
  virtual double read_time(uint index, uint ranges, ha_rows rows);
  /*
    For the given range how many records are estimated to be in this range.
    Used by optimizer to calculate cost of using a particular index.
  */
  virtual ha_rows records_in_range(uint inx, key_range * min_key,
                                   key_range * max_key);

  /*
    Upper bound of number records returned in scan is sum of all
    underlying handlers.
  */
  virtual ha_rows estimate_rows_upper_bound();

  /*
    table_cache_type is implemented by the underlying handler but all
    underlying handlers must have the same implementation for it to work.
  */
  virtual uint8 table_cache_type();
  virtual int records(ha_rows *num_rows);

  /* Calculate hash value for PARTITION BY KEY tables. */
  uint32 calculate_key_hash_value(Field **field_array)
  {
    return ph_calculate_key_hash_value(field_array);
  }

  /*
    -------------------------------------------------------------------------
    MODULE print messages
    -------------------------------------------------------------------------
    This module contains various methods that returns text messages for
    table types, index type and error messages.
    -------------------------------------------------------------------------
  */
  /*
    The name of the index type that will be used for display
    Here we must ensure that all handlers use the same index type
    for each index created.
  */
  virtual const char *index_type(uint inx);

  /* The name of the table type that will be used for display purposes */
  virtual const char *table_type() const;

  /* The name of the row type used for the underlying tables. */
  virtual enum row_type get_row_type() const;

  /*
     Handler specific error messages
  */
  virtual void print_error(int error, myf errflag);
  virtual bool get_error_message(int error, String * buf);
  /*
   -------------------------------------------------------------------------
    MODULE handler characteristics
    -------------------------------------------------------------------------
    This module contains a number of methods defining limitations and
    characteristics of the handler. The partition handler will calculate
    this characteristics based on underlying handler characteristics.
    -------------------------------------------------------------------------

    This is a list of flags that says what the storage engine
    implements. The current table flags are documented in handler.h
    The partition handler will support whatever the underlying handlers
    support except when specifically mentioned below about exceptions
    to this rule.
    NOTE: This cannot be cached since it can depend on TRANSACTION ISOLATION
    LEVEL which is dynamic, see bug#39084.

    HA_TABLE_SCAN_ON_INDEX:
    Used to avoid scanning full tables on an index. If this flag is set then
    the handler always has a primary key (hidden if not defined) and this
    index is used for scanning rather than a full table scan in all
    situations.
    (InnoDB, BDB, Federated)

    HA_REC_NOT_IN_SEQ:
    This flag is set for handlers that cannot guarantee that the rows are
    returned accroding to incremental positions (0, 1, 2, 3...).
    This also means that rnd_next() should return HA_ERR_RECORD_DELETED
    if it finds a deleted row.
    (MyISAM (not fixed length row), BDB, HEAP, NDB, InooDB)

    HA_CAN_GEOMETRY:
    Can the storage engine handle spatial data.
    Used to check that no spatial attributes are declared unless
    the storage engine is capable of handling it.
    (MyISAM)

    HA_FAST_KEY_READ:
    Setting this flag indicates that the handler is equally fast in
    finding a row by key as by position.
    This flag is used in a very special situation in conjunction with
    filesort's. For further explanation see intro to init_read_record.
    (BDB, HEAP, InnoDB)

    HA_NULL_IN_KEY:
    Is NULL values allowed in indexes.
    If this is not allowed then it is not possible to use an index on a
    NULLable field.
    (BDB, HEAP, MyISAM, NDB, InnoDB)

    HA_DUPLICATE_POS:
    Tells that we can the position for the conflicting duplicate key
    record is stored in table->file->dupp_ref. (insert uses rnd_pos() on
    this to find the duplicated row)
    (MyISAM)

    HA_CAN_INDEX_BLOBS:
    Is the storage engine capable of defining an index of a prefix on
    a BLOB attribute.
    (BDB, Federated, MyISAM, InnoDB)

    HA_AUTO_PART_KEY:
    Auto increment fields can be part of a multi-part key. For second part
    auto-increment keys, the auto_incrementing is done in handler.cc
    (BDB, Federated, MyISAM, NDB)

    HA_REQUIRE_PRIMARY_KEY:
    Can't define a table without primary key (and cannot handle a table
    with hidden primary key)
    (No handler has this limitation currently)

    HA_STATS_RECORDS_IS_EXACT:
    Does the counter of records after the info call specify an exact
    value or not. If it does this flag is set.
    Only MyISAM and HEAP uses exact count.

    HA_PRIMARY_KEY_IN_READ_INDEX:
    This parameter is set when the handler will also return the primary key
    when doing read-only-key on another index.

    HA_NOT_DELETE_WITH_CACHE:
    Seems to be an old MyISAM feature that is no longer used. No handler
    has it defined but it is checked in init_read_record.
    Further investigation needed.
    (No handler defines it)

    HA_NO_PREFIX_CHAR_KEYS:
    Indexes on prefixes of character fields is not allowed.
    (NDB)

    HA_CAN_FULLTEXT:
    Does the storage engine support fulltext indexes
    The partition handler will start by not supporting fulltext indexes.
    (MyISAM)

    HA_CAN_SQL_HANDLER:
    Can the HANDLER interface in the MySQL API be used towards this
    storage engine.
    (MyISAM, InnoDB)

    HA_NO_AUTO_INCREMENT:
    Set if the storage engine does not support auto increment fields.
    (Currently not set by any handler)

    HA_HAS_CHECKSUM:
    Special MyISAM feature. Has special SQL support in CREATE TABLE.
    No special handling needed by partition handler.
    (MyISAM)

    HA_FILE_BASED:
    Should file names always be in lower case (used by engines
    that map table names to file names.
    Since partition handler has a local file this flag is set.
    (BDB, Federated, MyISAM)

    HA_CAN_BIT_FIELD:
    Is the storage engine capable of handling bit fields?
    (MyISAM, NDB)

    HA_PRIMARY_KEY_REQUIRED_FOR_POSITION:
    Does the storage engine need a PK for position?
    (InnoDB)

    HA_FILE_BASED is always set for partition handler since we use a
    special file for handling names of partitions, engine types.
    HA_REC_NOT_IN_SEQ is always set for partition handler since we cannot
    guarantee that the records will be returned in sequence.
    HA_CAN_GEOMETRY, HA_CAN_FULLTEXT, HA_DUPLICATE_POS,
    HA_PRIMARY_KEY_REQUIRED_FOR_POSITION is disabled
    until further investigated.
  */
  virtual Table_flags table_flags() const;

  /*
    This is a bitmap of flags that says how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero
    here.

    part is the key part to check. First key part is 0
    If all_parts it's set, MySQL want to know the flags for the combined
    index up to and including 'part'.

    HA_READ_NEXT:
    Does the index support read next, this is assumed in the server
    code and never checked so all indexes must support this.
    Note that the handler can be used even if it doesn't have any index.
    (BDB, HEAP, MyISAM, Federated, NDB, InnoDB)

    HA_READ_PREV:
    Can the index be used to scan backwards.
    (BDB, HEAP, MyISAM, NDB, InnoDB)

    HA_READ_ORDER:
    Can the index deliver its record in index order. Typically true for
    all ordered indexes and not true for hash indexes.
    In first step this is not true for partition handler until a merge
    sort has been implemented in partition handler.
    Used to set keymap part_of_sortkey
    This keymap is only used to find indexes usable for resolving an ORDER BY
    in the query. Thus in most cases index_read will work just fine without
    order in result production. When this flag is set it is however safe to
    order all output started by index_read since most engines do this. With
    read_multi_range calls there is a specific flag setting order or not
    order so in those cases ordering of index output can be avoided.
    (BDB, InnoDB, HEAP, MyISAM, NDB)

    HA_READ_RANGE:
    Specify whether index can handle ranges, typically true for all
    ordered indexes and not true for hash indexes.
    Used by optimiser to check if ranges (as key >= 5) can be optimised
    by index.
    (BDB, InnoDB, NDB, MyISAM, HEAP)

    HA_ONLY_WHOLE_INDEX:
    Can't use part key searches. This is typically true for hash indexes
    and typically not true for ordered indexes.
    (Federated, NDB, HEAP)

    HA_KEYREAD_ONLY:
    Does the storage engine support index-only scans on this index.
    Enables use of HA_EXTRA_KEYREAD and HA_EXTRA_NO_KEYREAD
    Used to set key_map keys_for_keyread and to check in optimiser for
    index-only scans.  When doing a read under HA_EXTRA_KEYREAD the handler
    only have to fill in the columns the key covers. If
    HA_PRIMARY_KEY_IN_READ_INDEX is set then also the PRIMARY KEY columns
    must be updated in the row.
    (BDB, InnoDB, MyISAM)
  */
  virtual ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return m_file[0]->index_flags(inx, part, all_parts);
  }
  /*
     extensions of table handler files
  */
  virtual const char **bas_ext() const;
  /*
    unireg.cc will call the following to make sure that the storage engine
    can handle the data it is about to send.

    The maximum supported values is the minimum of all handlers in the table
  */
  uint min_of_the_max_uint(uint (handler::*operator_func)(void) const) const;
  virtual uint max_supported_record_length() const;
  virtual uint max_supported_keys() const;
  virtual uint max_supported_key_parts() const;
  virtual uint max_supported_key_length() const;
  virtual uint max_supported_key_part_length() const;

  /*
    All handlers in a partitioned table must have the same low_byte_first
  */
  virtual bool low_byte_first() const
  { return m_low_byte_first; }

  /*
    The extra record buffer length is the maximum needed by all handlers.
    The minimum record length is the maximum of all involved handlers.
  */
  virtual uint extra_rec_buf_length() const;
  virtual uint min_record_length(uint options) const;

  /*
    Primary key is clustered can only be true if all underlying handlers have
    this feature.
  */
  virtual bool primary_key_is_clustered() const
  { return m_pkey_is_clustered; }

  /*
    -------------------------------------------------------------------------
    MODULE compare records
    -------------------------------------------------------------------------
    cmp_ref checks if two references are the same. For most handlers this is
    a simple memcmp of the reference. However some handlers use primary key
    as reference and this can be the same even if memcmp says they are
    different. This is due to character sets and end spaces and so forth.
    For the partition handler the reference is first two bytes providing the
    partition identity of the referred record and then the reference of the
    underlying handler.
    Thus cmp_ref for the partition handler always returns FALSE for records
    not in the same partition and uses cmp_ref on the underlying handler
    to check whether the rest of the reference part is also the same.
    -------------------------------------------------------------------------
  */
  virtual int cmp_ref(const uchar * ref1, const uchar * ref2);

  /*
    -------------------------------------------------------------------------
    MODULE condition pushdown
    -------------------------------------------------------------------------
    cond_push
    -------------------------------------------------------------------------
  */

  /* No support of engine condition pushdown yet! */
  //const Item *cond_push(const Item *cond);
  //void cond_pop();
  /* Only Index condition pushdown is supported currently. */
  Item *idx_cond_push(uint keyno, Item* idx_cond);
  void cancel_pushed_idx_cond();
  /* No support of pushed joins yet! */
  //uint number_of_pushed_joins()
  //virtual const TABLE* root_of_pushed_join() const
  //virtual const TABLE* parent_of_pushed_join() const
  //virtual int index_read_pushed(uchar * buf, const uchar * key,
                                //key_part_map keypart_map)
  //virtual int index_next_pushed(uchar * buf)


  /*
    -------------------------------------------------------------------------
    MODULE auto increment
    -------------------------------------------------------------------------
    This module is used to handle the support of auto increments.

    This variable in the handler is used as part of the handler interface
    It is maintained by the parent handler object and should not be
    touched by child handler objects (see handler.cc for its use).

    auto_increment_column_changed
     -------------------------------------------------------------------------
  */
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  void release_auto_increment()
  {
    Partition_helper::ph_release_auto_increment();
  }
  /** Release the auto increment for all underlying partitions. */
  void release_auto_increment_all_parts();

public:

  /*
     -------------------------------------------------------------------------
     MODULE initialize handler for HANDLER call
     -------------------------------------------------------------------------
     This method is a special InnoDB method called before a HANDLER query.
     -------------------------------------------------------------------------
  */
  virtual void init_table_handle_for_HANDLER();

  /*
    The remainder of this file defines the handler methods not implemented
    by the partition handler
  */

  /*
    -------------------------------------------------------------------------
    MODULE foreign key support
    -------------------------------------------------------------------------
    The following methods are used to implement foreign keys as supported by
    InnoDB. Implement this ??
    get_foreign_key_create_info is used by SHOW CREATE TABLE to get a textual
    description of how the CREATE TABLE part to define FOREIGN KEY's is done.
    free_foreign_key_create_info is used to free the memory area that provided
    this description.
    can_switch_engines checks if it is ok to switch to a new engine based on
    the foreign key info in the table.
    -------------------------------------------------------------------------

    virtual char* get_foreign_key_create_info()
    virtual void free_foreign_key_create_info(char* str)

    virtual int get_foreign_key_list(THD *thd,
    List<FOREIGN_KEY_INFO> *f_key_list)
    virtual uint referenced_by_foreign_key()
  */
    virtual bool can_switch_engines();
  /*
    -------------------------------------------------------------------------
    MODULE fulltext index
    -------------------------------------------------------------------------
    Fulltext stuff not yet.
    -------------------------------------------------------------------------
    virtual int ft_init() { return HA_ERR_WRONG_COMMAND; }
    virtual FT_INFO *ft_init_ext(uint flags,uint inx,const uchar *key,
    uint keylen)
    { return NULL; }
    virtual int ft_read(uchar *buf) { return HA_ERR_WRONG_COMMAND; }
  */

  /*
    -------------------------------------------------------------------------
    MODULE in-place ALTER TABLE
    -------------------------------------------------------------------------
    These methods are in the handler interface. (used by innodb-plugin)
    They are used for in-place alter table:
    -------------------------------------------------------------------------
  */
    virtual enum_alter_inplace_result
      check_if_supported_inplace_alter(TABLE *altered_table,
                                       Alter_inplace_info *ha_alter_info);
    virtual bool prepare_inplace_alter_table(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info);
    virtual bool inplace_alter_table(TABLE *altered_table,
                                     Alter_inplace_info *ha_alter_info);
    virtual bool commit_inplace_alter_table(TABLE *altered_table,
                                            Alter_inplace_info *ha_alter_info,
                                            bool commit);
    virtual void notify_table_changed();

  /*
    -------------------------------------------------------------------------
    MODULE tablespace support
    -------------------------------------------------------------------------
  */
    virtual int discard_or_import_tablespace(my_bool discard);

  /*
    -------------------------------------------------------------------------
    MODULE admin MyISAM
    -------------------------------------------------------------------------

    -------------------------------------------------------------------------
      OPTIMIZE TABLE, CHECK TABLE, ANALYZE TABLE and REPAIR TABLE are
      mapped to a routine that handles looping over a given set of
      partitions and those routines send a flag indicating to execute on
      all partitions.
    -------------------------------------------------------------------------
  */
    virtual int optimize(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int analyze(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int check(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int repair(THD* thd, HA_CHECK_OPT *check_opt);
    virtual bool check_and_repair(THD *thd);
    virtual bool auto_repair() const;
    virtual bool is_crashed() const;
    virtual int check_for_upgrade(HA_CHECK_OPT *check_opt);

    private:
    int handle_opt_partitions(THD *thd,
                              HA_CHECK_OPT *check_opt,
                              enum_part_operation operation);
    int handle_opt_part(THD *thd, HA_CHECK_OPT *check_opt, uint part_id,
                        enum_part_operation operation);
    public:
  /*
    -------------------------------------------------------------------------
    Admin commands not supported currently (almost purely MyISAM routines)
    This means that the following methods are not implemented:
    -------------------------------------------------------------------------

    virtual int backup(TD* thd, HA_CHECK_OPT *check_opt);
    virtual int restore(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int dump(THD* thd, int fd = -1);
    virtual int net_read_dump(NET* net);
  */
  ha_checksum checksum() const
  {
    return Partition_helper::ph_checksum();
  }
  /* Enabled keycache for performance reasons, WL#4571 */
    virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int preload_keys(THD* thd, HA_CHECK_OPT* check_opt);

  /*
    -------------------------------------------------------------------------
    MODULE enable/disable indexes
    -------------------------------------------------------------------------
    Enable/Disable Indexes are only supported by HEAP and MyISAM.
    -------------------------------------------------------------------------
  */
    virtual int disable_indexes(uint mode);
    virtual int enable_indexes(uint mode);
    virtual int indexes_are_disabled(void);

  /*
    -------------------------------------------------------------------------
    MODULE append_create_info
    -------------------------------------------------------------------------
    append_create_info is only used by MyISAM MERGE tables and the partition
    handler will not support this handler as underlying handler.
    Implement this??
    -------------------------------------------------------------------------
    virtual void append_create_info(String *packet)
  */

  /*
    -------------------------------------------------------------------------
    MODULE partitioning specific handler API
    -------------------------------------------------------------------------
  */
  handler *get_handler()
  {
    return static_cast<handler*>(this);
  }
  Partition_handler *get_partition_handler()
  {
    return static_cast<Partition_handler*>(this);
  }
  void set_part_info(partition_info *part_info, bool early)
  {
    Partition_helper::set_part_info_low(part_info, early);
  }
  uint alter_flags(uint flags MY_ATTRIBUTE((unused))) const
  {
    return (HA_PARTITION_FUNCTION_SUPPORTED |
            HA_FAST_CHANGE_PARTITION);
  }

private:
  /* private support functions for Partition_helper: */
  int write_row_in_part(uint part_id, uchar *buf);
  int update_row_in_part(uint part_id, const uchar *old_data, uchar *new_data);
  int delete_row_in_part(uint part_id, const uchar *buf);
  int rnd_init_in_part(uint part_id, bool table_scan);
  int rnd_next_in_part(uint part_id, uchar *buf);
  int rnd_end_in_part(uint part_id, bool scan);
  void position_in_last_part(uchar *ref, const uchar *record);
  int rnd_pos_in_part(uint part_id, uchar *buf, uchar *pos);
  int index_init_in_part(uint part, uint keynr, bool sorted);
  int index_end_in_part(uint part);
  int index_last_in_part(uint part, uchar *buf);
  int index_first_in_part(uint part, uchar* buf);
  int index_prev_in_part(uint part, uchar *buf);
  int index_next_in_part(uint part, uchar *buf);
  int index_next_same_in_part(uint part,
                              uchar *buf,
                              const uchar *key,
                              uint length);
  int index_read_map_in_part(uint part,
                             uchar *buf,
                             const uchar *key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag);
  int index_read_idx_map_in_part(uint part,
                                 uchar *buf,
                                 uint index,
                                 const uchar *key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag);
  int index_read_last_map_in_part(uint part,
                                  uchar *buf,
                                  const uchar *key,
                                  key_part_map keypart_map);
  int read_range_first_in_part(uint part_id,
                               uchar *buf,
                               const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range_arg,
                               bool sorted);
  int read_range_next_in_part(uint part, uchar *buf);
  ha_checksum checksum_in_part(uint part_id) const;
  int initialize_auto_increment(bool no_lock);
  /*
    Access methods to protected areas in handler to avoid adding
    friend class Partition_helper in class handler.
  */
  THD *get_thd() const
  {
    return ha_thd();
  }
  TABLE *get_table() const
  {
    return table;
  }
  bool get_eq_range() const
  {
    return eq_range;
  }
  void set_eq_range(bool eq_range_arg)
  {
    eq_range= eq_range_arg;
  }
  void set_range_key_part(KEY_PART_INFO *key_part)
  {
    range_key_part= key_part;
  }
};

#endif /* HA_PARTITION_INCLUDED */
