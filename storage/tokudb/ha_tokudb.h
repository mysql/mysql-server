/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#if !defined(HA_TOKUDB_H)
#define HA_TOKUDB_H

#include <db.h>
#include "hatoku_cmp.h"

#define HA_TOKU_ORIG_VERSION 4
#define HA_TOKU_VERSION 4
//
// no capabilities yet
//
#define HA_TOKU_CAP 0

class ha_tokudb;

typedef struct loader_context {
    THD* thd;
    char write_status_msg[200];
    ha_tokudb* ha;
} *LOADER_CONTEXT;

//
// This object stores table information that is to be shared
// among all ha_tokudb objects.
// There is one instance per table, shared among threads.
// Some of the variables here are the DB* pointers to indexes,
// and auto increment information.
//
class TOKUDB_SHARE {
public:
    void init(void);
    void destroy(void);

public:
    char *table_name;
    uint table_name_length, use_count;
    pthread_mutex_t mutex;
    THR_LOCK lock;

    ulonglong auto_ident;
    ulonglong last_auto_increment, auto_inc_create_value;
    //
    // estimate on number of rows in table
    //
    ha_rows rows;
    //
    // estimate on number of rows added in the process of a locked tables
    // this is so we can better estimate row count during a lock table
    //
    ha_rows rows_from_locked_table;
    DB *status_block;
    //
    // DB that is indexed on the primary key
    //
    DB *file;
    //
    // array of all DB's that make up table, includes DB that
    // is indexed on the primary key, add 1 in case primary
    // key is hidden
    //
    DB *key_file[MAX_KEY +1];
    rw_lock_t key_file_lock;
    uint status, version, capabilities;
    uint ref_length;
    //
    // whether table has an auto increment column
    //
    bool has_auto_inc;
    //
    // index of auto increment column in table->field, if auto_inc exists
    //
    uint ai_field_index;
    //
    // whether the primary key has a string
    //
    bool pk_has_string;

    KEY_AND_COL_INFO kc_info;
    
    // 
    // we want the following optimization for bulk loads, if the table is empty, 
    // attempt to grab a table lock. emptiness check can be expensive, 
    // so we try it once for a table. After that, we keep this variable around 
    // to tell us to not try it again. 
    // 
    bool try_table_lock; 

    bool has_unique_keys;
    bool replace_into_fast;
    rw_lock_t num_DBs_lock;
    uint32_t num_DBs;

    pthread_cond_t m_openclose_cond;
    enum { CLOSED, OPENING, OPENED, CLOSING, ERROR } m_state;
    int m_error;
    int m_initialize_count;

    uint n_rec_per_key;
    uint64_t *rec_per_key;
};

typedef struct st_filter_key_part_info {
    uint offset;
    uint part_index;
} FILTER_KEY_PART_INFO;

typedef enum {
    lock_read = 0,
    lock_write
} TABLE_LOCK_TYPE;

// the number of rows bulk fetched in one callback grows exponentially
// with the bulk fetch iteration, so the max iteration is the max number
// of shifts we can perform on a 64 bit integer.
#define HA_TOKU_BULK_FETCH_ITERATION_MAX 63

class ha_tokudb : public handler {
private:
    THR_LOCK_DATA lock;         ///< MySQL lock
    TOKUDB_SHARE *share;        ///< Shared lock info

#ifdef MARIADB_BASE_VERSION
    // MariaDB version of MRR
    DsMrr_impl ds_mrr;
#elif 50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699
    // MySQL version of MRR
    DsMrr_impl ds_mrr;
#endif

    // For ICP. Cache our own copies
    Item* toku_pushed_idx_cond;
    uint toku_pushed_idx_cond_keyno;  /* The index which the above condition is for */
    bool icp_went_out_of_range;

    //
    // last key returned by ha_tokudb's cursor
    //
    DBT last_key;
    //
    // pointer used for multi_alloc of key_buff, key_buff2, primary_key_buff
    //
    void *alloc_ptr;
    //
    // buffer used to temporarily store a "packed row" 
    // data pointer of a DBT will end up pointing to this
    // see pack_row for usage
    //
    uchar *rec_buff;
    //
    // number of bytes allocated in rec_buff
    //
    ulong alloced_rec_buff_length;
    //
    // same as above two, but for updates
    //
    uchar *rec_update_buff;
    ulong alloced_update_rec_buff_length;
    uint32_t max_key_length;

    uchar* range_query_buff; // range query buffer
    uint32_t size_range_query_buff; // size of the allocated range query buffer
    uint32_t bytes_used_in_range_query_buff; // number of bytes used in the range query buffer
    uint32_t curr_range_query_buff_offset; // current offset into the range query buffer for queries to read
    uint64_t bulk_fetch_iteration;
    uint64_t rows_fetched_using_bulk_fetch;
    bool doing_bulk_fetch;
    bool maybe_index_scan;

    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    //
    uchar *key_buff; 
    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    // This is used in functions that require the packing
    // of more than one key
    //
    uchar *key_buff2; 
    uchar *key_buff3; 
    uchar *key_buff4;
    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    // currently this is only used for a primary key in
    // the function update_row, hence the name. It 
    // does not carry any state throughout the class.
    //
    uchar *primary_key_buff;

    //
    // ranges of prelocked area, used to know how much to bulk fetch
    //
    uchar *prelocked_left_range; 
    uint32_t prelocked_left_range_size;
    uchar *prelocked_right_range; 
    uint32_t prelocked_right_range_size;


    //
    // individual DBTs for each index
    //
    DBT_ARRAY mult_key_dbt_array[2*(MAX_KEY + 1)];
    DBT_ARRAY mult_rec_dbt_array[MAX_KEY + 1];
    uint32_t mult_put_flags[MAX_KEY + 1];
    uint32_t mult_del_flags[MAX_KEY + 1];
    uint32_t mult_dbt_flags[MAX_KEY + 1];
    

    //
    // when unpacking blobs, we need to store it in a temporary
    // buffer that will persist because MySQL just gets a pointer to the 
    // blob data, a pointer we need to ensure is valid until the next
    // query
    //
    uchar* blob_buff;
    uint32_t num_blob_bytes;

    bool unpack_entire_row;

    //
    // buffers (and their sizes) that will hold the indexes
    // of fields that need to be read for a query
    //
    uint32_t* fixed_cols_for_query;
    uint32_t num_fixed_cols_for_query;
    uint32_t* var_cols_for_query;
    uint32_t num_var_cols_for_query;
    bool read_blobs;
    bool read_key;

    //
    // transaction used by ha_tokudb's cursor
    //
    DB_TXN *transaction;

    // external_lock will set this true for read operations that will be closely followed by write operations.
    bool use_write_locks; // use write locks for reads

    //
    // instance of cursor being used for init_xxx and rnd_xxx functions
    //
    DBC *cursor;
    uint32_t cursor_flags; // flags for cursor
    //
    // flags that are returned in table_flags()
    //
    ulonglong int_table_flags;
    // 
    // count on the number of rows that gets changed, such as when write_row occurs
    // this is meant to help keep estimate on number of elements in DB
    // 
    ulonglong added_rows;
    ulonglong deleted_rows;


    uint last_dup_key;
    //
    // if set to 0, then the primary key is not hidden
    // if non-zero (not necessarily 1), primary key is hidden
    //
    uint hidden_primary_key;
    bool key_read, using_ignore;
    bool using_ignore_no_key;

    //
    // After a cursor encounters an error, the cursor will be unusable
    // In case MySQL attempts to do a cursor operation (such as rnd_next
    // or index_prev), we will gracefully return this error instead of crashing
    //
    int last_cursor_error;

    //
    // For instances where we successfully prelock a range or a table,
    // we set this to true so that successive cursor calls can know
    // know to limit the locking overhead in a call to the fractal tree
    //
    bool range_lock_grabbed;
    bool range_lock_grabbed_null;

    //
    // For bulk inserts, we want option of not updating auto inc
    // until all inserts are done. By default, is false
    //
    bool delay_updating_ai_metadata; // if true, don't update auto-increment metadata until bulk load completes
    bool ai_metadata_update_required; // if true, autoincrement metadata must be updated 

    //
    // buffer for updating the status of long insert, delete, and update
    // statements. Right now, the the messages are 
    // "[inserted|updated|deleted] about %llu rows",
    // so a buffer of 200 is good enough.
    //
    char write_status_msg[200]; //buffer of 200 should be a good upper bound.
    struct loader_context lc;

    DB_LOADER* loader;
    bool abort_loader;
    int loader_error;

    bool num_DBs_locked_in_bulk;
    uint32_t lock_count;
    
    bool fix_rec_buff_for_blob(ulong length);
    bool fix_rec_update_buff_for_blob(ulong length);
    uchar current_ident[TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH];

    ulong max_row_length(const uchar * buf);
    int pack_row_in_buff(
        DBT * row, 
        const uchar* record,
        uint index,
        uchar* row_buff
        );
    int pack_row(
        DBT * row, 
        const uchar* record,
        uint index
        );
    int pack_old_row_for_update(
        DBT * row, 
        const uchar* record,
        uint index
        );
    uint32_t place_key_into_mysql_buff(KEY* key_info, uchar * record, uchar* data);
    void unpack_key(uchar * record, DBT const *key, uint index);
    uint32_t place_key_into_dbt_buff(KEY* key_info, uchar * buff, const uchar * record, bool* has_null, int key_length);
    DBT* create_dbt_key_from_key(DBT * key, KEY* key_info, uchar * buff, const uchar * record, bool* has_null, bool dont_pack_pk, int key_length, uint8_t inf_byte);
    DBT *create_dbt_key_from_table(DBT * key, uint keynr, uchar * buff, const uchar * record, bool* has_null, int key_length = MAX_KEY_LENGTH);
    DBT* create_dbt_key_for_lookup(DBT * key, KEY* key_info, uchar * buff, const uchar * record, bool* has_null, int key_length = MAX_KEY_LENGTH);
    DBT *pack_key(DBT * key, uint keynr, uchar * buff, const uchar * key_ptr, uint key_length, int8_t inf_byte);
#if TOKU_INCLUDE_EXTENDED_KEYS
    DBT *pack_ext_key(DBT * key, uint keynr, uchar * buff, const uchar * key_ptr, uint key_length, int8_t inf_byte);
#endif
    bool key_changed(uint keynr, const uchar * old_row, const uchar * new_row);
    int handle_cursor_error(int error, int err_to_return, uint keynr);
    DBT *get_pos(DBT * to, uchar * pos);
 
    int open_main_dictionary(const char* name, bool is_read_only, DB_TXN* txn);
    int open_secondary_dictionary(DB** ptr, KEY* key_info, const char* name, bool is_read_only, DB_TXN* txn);
    int acquire_table_lock (DB_TXN* trans, TABLE_LOCK_TYPE lt);
    int estimate_num_rows(DB* db, uint64_t* num_rows, DB_TXN* txn);
    bool has_auto_increment_flag(uint* index);

    int write_frm_data(DB* db, DB_TXN* txn, const char* frm_name);
    int verify_frm_data(const char* frm_name, DB_TXN* trans);
    int remove_frm_data(DB *db, DB_TXN *txn);

    int write_to_status(DB* db, HA_METADATA_KEY curr_key_data, void* data, uint size, DB_TXN* txn);
    int remove_from_status(DB* db, HA_METADATA_KEY curr_key_data, DB_TXN* txn);

    int write_metadata(DB* db, void* key, uint key_size, void* data, uint data_size, DB_TXN* txn);
    int remove_metadata(DB* db, void* key_data, uint key_size, DB_TXN* transaction);

    int update_max_auto_inc(DB* db, ulonglong val);
    int remove_key_name_from_status(DB* status_block, char* key_name, DB_TXN* txn);
    int write_key_name_to_status(DB* status_block, char* key_name, DB_TXN* txn);
    int write_auto_inc_create(DB* db, ulonglong val, DB_TXN* txn);
    void init_auto_increment();
    bool can_replace_into_be_fast(TABLE_SHARE* table_share, KEY_AND_COL_INFO* kc_info, uint pk);
    int initialize_share(const char* name, int mode);

    void set_query_columns(uint keynr);
    int prelock_range (const key_range *start_key, const key_range *end_key);
    int create_txn(THD* thd, tokudb_trx_data* trx);
    bool may_table_be_empty(DB_TXN *txn);
    int delete_or_rename_table (const char* from_name, const char* to_name, bool is_delete);
    int delete_or_rename_dictionary( const char* from_name, const char* to_name, const char* index_name, bool is_key, DB_TXN* txn, bool is_delete);
    int truncate_dictionary( uint keynr, DB_TXN* txn );
    int create_secondary_dictionary(
        const char* name, 
        TABLE* form, 
        KEY* key_info, 
        DB_TXN* txn, 
        KEY_AND_COL_INFO* kc_info, 
        uint32_t keynr, 
        bool is_hot_index,
        toku_compression_method compression_method
        );
    int create_main_dictionary(const char* name, TABLE* form, DB_TXN* txn, KEY_AND_COL_INFO* kc_info, toku_compression_method compression_method);
    void trace_create_table_info(const char *name, TABLE * form);
    int is_index_unique(bool* is_unique, DB_TXN* txn, DB* db, KEY* key_info, int lock_flags);
    int is_val_unique(bool* is_unique, uchar* record, KEY* key_info, uint dict_index, DB_TXN* txn);
    int do_uniqueness_checks(uchar* record, DB_TXN* txn, THD* thd);
    void set_main_dict_put_flags(THD* thd, bool opt_eligible, uint32_t* put_flags);
    int insert_row_to_main_dictionary(uchar* record, DBT* pk_key, DBT* pk_val, DB_TXN* txn);
    int insert_rows_to_dictionaries_mult(DBT* pk_key, DBT* pk_val, DB_TXN* txn, THD* thd);
    void test_row_packing(uchar* record, DBT* pk_key, DBT* pk_val);
    uint32_t fill_row_mutator(
        uchar* buf, 
        uint32_t* dropped_columns, 
        uint32_t num_dropped_columns,
        TABLE* altered_table,
        KEY_AND_COL_INFO* altered_kc_info,
        uint32_t keynr,
        bool is_add
        );

    // 0 <= active_index < table_share->keys || active_index == MAX_KEY
    // tokudb_active_index = active_index if active_index < table_share->keys, else tokudb_active_index = primary_key = table_share->keys
    uint tokudb_active_index;
 
public:
    ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg);
    ~ha_tokudb();

    const char *table_type() const;
    const char *index_type(uint inx);
    const char **bas_ext() const;

    //
    // Returns a bit mask of capabilities of storage engine. Capabilities 
    // defined in sql/handler.h
    //
    ulonglong table_flags(void) const;
    
    ulong index_flags(uint inx, uint part, bool all_parts) const;

    //
    // Returns limit on the number of keys imposed by tokudb.
    //
    uint max_supported_keys() const {
        return MAX_KEY;
    } 

    uint extra_rec_buf_length() const {
        return TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    } 
    ha_rows estimate_rows_upper_bound();

    //
    // Returns the limit on the key length imposed by tokudb.
    //
    uint max_supported_key_length() const {
        return UINT_MAX32;
    } 

    //
    // Returns limit on key part length imposed by tokudb.
    //
    uint max_supported_key_part_length() const {
        return UINT_MAX32;
    } 
    const key_map *keys_to_use_for_scanning() {
        return &key_map_full;
    }

    double scan_time();

    double read_time(uint index, uint ranges, ha_rows rows);
    
    // Defined in mariadb
    double keyread_time(uint index, uint ranges, ha_rows rows);

    // Defined in mysql 5.6
    double index_only_read_time(uint keynr, double records);

    int open(const char *name, int mode, uint test_if_locked);
    int close(void);
    void update_create_info(HA_CREATE_INFO* create_info);
    int create(const char *name, TABLE * form, HA_CREATE_INFO * create_info);
    int delete_table(const char *name);
    int rename_table(const char *from, const char *to);
    int optimize(THD * thd, HA_CHECK_OPT * check_opt);
    int analyze(THD * thd, HA_CHECK_OPT * check_opt);
    int write_row(uchar * buf);
    int update_row(const uchar * old_data, uchar * new_data);
    int delete_row(const uchar * buf);
#if MYSQL_VERSION_ID >= 100000
    void start_bulk_insert(ha_rows rows, uint flags);
#else
    void start_bulk_insert(ha_rows rows);
#endif
    int end_bulk_insert();
    int end_bulk_insert(bool abort);

    int prepare_index_scan();
    int prepare_index_key_scan( const uchar * key, uint key_len );
    int prepare_range_scan( const key_range *start_key, const key_range *end_key);
    void column_bitmaps_signal();
    int index_init(uint index, bool sorted);
    int index_end();
    int index_next_same(uchar * buf, const uchar * key, uint keylen); 
    int index_read(uchar * buf, const uchar * key, uint key_len, enum ha_rkey_function find_flag);
    int index_read_last(uchar * buf, const uchar * key, uint key_len);
    int index_next(uchar * buf);
    int index_prev(uchar * buf);
    int index_first(uchar * buf);
    int index_last(uchar * buf);

    int rnd_init(bool scan);
    int rnd_end();
    int rnd_next(uchar * buf);
    int rnd_pos(uchar * buf, uchar * pos);

    int read_range_first(const key_range *start_key,
                                 const key_range *end_key,
                                 bool eq_range, bool sorted);
    int read_range_next();


    void position(const uchar * record);
    int info(uint);
    int extra(enum ha_extra_function operation);
    int reset(void);
    int external_lock(THD * thd, int lock_type);
    int start_stmt(THD * thd, thr_lock_type lock_type);

    ha_rows records_in_range(uint inx, key_range * min_key, key_range * max_key);

    uint32_t get_cursor_isolation_flags(enum thr_lock_type lock_type, THD* thd);
    THR_LOCK_DATA **store_lock(THD * thd, THR_LOCK_DATA ** to, enum thr_lock_type lock_type);

    int get_status(DB_TXN* trans);
    void init_hidden_prim_key_info(DB_TXN *txn);
    inline void get_auto_primary_key(uchar * to) {
        tokudb_pthread_mutex_lock(&share->mutex);
        share->auto_ident++;
        hpk_num_to_char(to, share->auto_ident);
        tokudb_pthread_mutex_unlock(&share->mutex);
    }
    virtual void get_auto_increment(ulonglong offset, ulonglong increment, ulonglong nb_desired_values, ulonglong * first_value, ulonglong * nb_reserved_values);
    bool is_optimize_blocking();
    bool is_auto_inc_singleton();
    void print_error(int error, myf errflag);
    uint8 table_cache_type() {
        return HA_CACHE_TBL_TRANSACT;
    }
    bool primary_key_is_clustered() {
        return true;
    }
    bool supports_clustered_keys() {
        return true;
    }
    int cmp_ref(const uchar * ref1, const uchar * ref2);
    bool check_if_incompatible_data(HA_CREATE_INFO * info, uint table_changes);

#ifdef MARIADB_BASE_VERSION

// MariaDB MRR introduced in 5.5, API changed in MariaDB 10.0
#if MYSQL_VERSION_ID >= 100000
#define COST_VECT Cost_estimate
#endif

    int multi_range_read_init(RANGE_SEQ_IF* seq,
                              void* seq_init_param,
                              uint n_ranges, uint mode,
                              HANDLER_BUFFER *buf);
    int multi_range_read_next(range_id_t *range_info);
    ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                        void *seq_init_param, 
                                        uint n_ranges, uint *bufsz,
                                        uint *flags, COST_VECT *cost);
    ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                  uint key_parts, uint *bufsz, 
                                  uint *flags, COST_VECT *cost);
    int multi_range_read_explain_info(uint mrr_mode, char *str, size_t size);

#else

// MySQL  MRR introduced in 5.6
#if 50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699
    int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                              uint n_ranges, uint mode, HANDLER_BUFFER *buf);
    int multi_range_read_next(char **range_info);
    ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                        void *seq_init_param, 
                                        uint n_ranges, uint *bufsz,
                                        uint *flags, Cost_estimate *cost);
    ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                  uint *bufsz, uint *flags, Cost_estimate *cost);
#endif

#endif

    // ICP introduced in MariaDB 5.5
    Item* idx_cond_push(uint keyno, class Item* idx_cond);


#if TOKU_INCLUDE_ALTER_56
 public:
    enum_alter_inplace_result check_if_supported_inplace_alter(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    bool prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    bool inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    bool commit_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit);
 private:
    int alter_table_add_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int alter_table_drop_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int alter_table_add_or_drop_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int alter_table_expand_varchar_offsets(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int alter_table_expand_columns(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int alter_table_expand_one_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info, int expand_field_num);
    int alter_table_expand_blobs(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    void print_alter_info(TABLE *altered_table, Alter_inplace_info *ha_alter_info);
    int setup_kc_info(TABLE *altered_table, KEY_AND_COL_INFO *kc_info);
    int new_row_descriptor(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, uint32_t idx, DBT *row_descriptor);

 public:
#endif
#if TOKU_INCLUDE_ALTER_55
public:
    // Returns true of the 5.6 inplace alter table interface is used.
    bool try_hot_alter_table();

    // Used by the partition storage engine to provide new frm data for the table.
    int new_alter_table_frm_data(const uchar *frm_data, size_t frm_len);
#endif

 private:
    int tokudb_add_index(
        TABLE *table_arg, 
        KEY *key_info, 
        uint num_of_keys, 
        DB_TXN* txn, 
        bool* inc_num_DBs,
        bool* modified_DB
        ); 
    void restore_add_index(TABLE* table_arg, uint num_of_keys, bool incremented_numDBs, bool modified_DBs);
    int drop_indexes(TABLE *table_arg, uint *key_num, uint num_of_keys, KEY *key_info, DB_TXN* txn);
    void restore_drop_indexes(TABLE *table_arg, uint *key_num, uint num_of_keys);

 public:
    // delete all rows from the table
    // effect: all dictionaries, including the main and indexes, should be empty
    int discard_or_import_tablespace(my_bool discard);
    int truncate();
    int delete_all_rows();
    void extract_hidden_primary_key(uint keynr, DBT const *found_key);
    void read_key_only(uchar * buf, uint keynr, DBT const *found_key);
    int read_row_callback (uchar * buf, uint keynr, DBT const *row, DBT const *found_key);
    int read_primary_key(uchar * buf, uint keynr, DBT const *row, DBT const *found_key);
    int unpack_blobs(
        uchar* record,
        const uchar* from_tokudb_blob,
        uint32_t num_blob_bytes,
        bool check_bitmap
        );
    int unpack_row(
        uchar* record, 
        DBT const *row, 
        DBT const *key,
        uint index
        );

    int prefix_cmp_dbts( uint keynr, const DBT* first_key, const DBT* second_key) {
        return tokudb_prefix_cmp_dbt_key(share->key_file[keynr], first_key, second_key);
    }

    void track_progress(THD* thd);
    void set_loader_error(int err);
    void set_dup_value_for_pk(DBT* key);


    //
    // index into key_file that holds DB* that is indexed on
    // the primary_key. this->key_file[primary_index] == this->file
    //
    uint primary_key;

    int check(THD *thd, HA_CHECK_OPT *check_opt);

    int fill_range_query_buf(
        bool need_val, 
        DBT const *key, 
        DBT  const *row, 
        int direction,
        THD* thd,
        uchar* buf,
        DBT* key_to_compare
        );
#if TOKU_INCLUDE_ROW_TYPE_COMPRESSION
    enum row_type get_row_type() const;
#endif
private:
    int read_full_row(uchar * buf);
    int __close();
    int get_next(uchar* buf, int direction, DBT* key_to_compare, bool do_key_read);
    int read_data_from_range_query_buff(uchar* buf, bool need_val, bool do_key_read);
    // for ICP, only in MariaDB and MySQL 5.6
#if defined(MARIADB_BASE_VERSION) || (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699)
    enum icp_result toku_handler_index_cond_check(Item* pushed_idx_cond);
#endif
    void invalidate_bulk_fetch();
    void invalidate_icp();
    int delete_all_rows_internal();
    void close_dsmrr();
    void reset_dsmrr();
    
#if TOKU_INCLUDE_WRITE_FRM_DATA
    int write_frm_data(const uchar *frm_data, size_t frm_len);
#endif
#if TOKU_INCLUDE_UPSERT
private:
    int fast_update(THD *thd, List<Item> &update_fields, List<Item> &update_values, Item *conds);
    bool check_fast_update(THD *thd, List<Item> &update_fields, List<Item> &update_values, Item *conds);
    int send_update_message(List<Item> &update_fields, List<Item> &update_values, Item *conds, DB_TXN *txn);
    int upsert(THD *thd, List<Item> &update_fields, List<Item> &update_values);
    bool check_upsert(THD *thd, List<Item> &update_fields, List<Item> &update_values);
    int send_upsert_message(THD *thd, List<Item> &update_fields, List<Item> &update_values, DB_TXN *txn);
#endif
public:
    // mysql sometimes retires a txn before a cursor that references the txn is closed.
    // for example, commit is sometimes called before index_end.  the following methods
    // put the handler on a list of handlers that get cleaned up when the txn is retired.
    void cleanup_txn(DB_TXN *txn);
private:
    LIST trx_handler_list;
    void add_to_trx_handler_list();
    void remove_from_trx_handler_list();

private:
    int do_optimize(THD *thd);
    int map_to_handler_error(int error);

public:
    void rpl_before_write_rows();
    void rpl_after_write_rows();
    void rpl_before_delete_rows();
    void rpl_after_delete_rows();
    void rpl_before_update_rows();
    void rpl_after_update_rows();
    bool rpl_lookup_rows();
private:
    bool in_rpl_write_rows;
    bool in_rpl_delete_rows;
    bool in_rpl_update_rows;
};

#if TOKU_INCLUDE_OPTION_STRUCTS
struct ha_table_option_struct {
    uint row_format;
};

struct ha_index_option_struct {
    bool clustering;
};

static inline bool key_is_clustering(const KEY *key) {
    return (key->flags & HA_CLUSTERING) || (key->option_struct && key->option_struct->clustering);
}

#else

static inline bool key_is_clustering(const KEY *key) {
    return key->flags & HA_CLUSTERING;
}
#endif

#endif

