#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation          // gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "hatoku_defines.h"
#include "hatoku_cmp.h"
extern "C" {
#include "stdint.h"
#if defined(_WIN32)
#include "misc.h"
#endif
}

static inline void *thd_data_get(THD *thd, int slot) {
    return thd->ha_data[slot].ha_ptr;
}

static inline void thd_data_set(THD *thd, int slot, void *data) {
    thd->ha_data[slot].ha_ptr = data;
}

#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION

/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif

#include "ha_tokudb.h"
#include "hatoku_hton.h"
#include <mysql/plugin.h>

static const char *ha_tokudb_exts[] = {
    ha_tokudb_ext,
    NullS
};


static void share_key_file_lock_init(TOKUDB_SHARE * share)
{
#ifdef HA_TOKUDB_NEEDS_KEY_FILE_LOCK
    my_rwlock_init(&share->key_file_lock, 0);
#endif
}

static void share_key_file_lock_destroy(TOKUDB_SHARE * share)
{
#ifdef HA_TOKUDB_NEEDS_KEY_FILE_LOCK
    rwlock_destroy(&share->key_file_lock);
#endif
}

static void share_key_file_rdlock(TOKUDB_SHARE * share)
{
#ifdef HA_TOKUDB_NEEDS_KEY_FILE_LOCK
    rw_rdlock(&share->key_file_lock);
#endif
}

static void share_key_file_wrlock(TOKUDB_SHARE * share)
{
#ifdef HA_TOKUDB_NEEDS_KEY_FILE_LOCK
    rw_wrlock(&share->key_file_lock);
#endif
}

static void share_key_file_unlock(TOKUDB_SHARE * share)
{
#ifdef HA_TOKUDB_NEEDS_KEY_FILE_LOCK
    rw_unlock(&share->key_file_lock);
#endif
}

//
// This offset is calculated starting from AFTER the NULL bytes
//
static inline uint32_t get_fixed_field_size(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
    uint offset = 0;
    for (uint i = 0; i < table_share->fields; i++) {
        if (kc_info->field_lengths[i] && !bitmap_is_set(&kc_info->key_filters[keynr],i)) {
            offset += kc_info->field_lengths[i];
        }
    }
    return offset;
}


static inline uint32_t get_len_of_offsets(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
    uint len = 0;
    for (uint i = 0; i < table_share->fields; i++) {
        if (kc_info->length_bytes[i] && !bitmap_is_set(&kc_info->key_filters[keynr],i)) {
            len += kc_info->num_offset_bytes;
        }
    }
    return len;
}


static int allocate_key_and_col_info ( TABLE_SHARE* table_share, KEY_AND_COL_INFO* kc_info) {
    int error;
    //
    // initialize all of the bitmaps
    //
    for (uint i = 0; i < MAX_KEY + 1; i++) {
        error = bitmap_init(
            &kc_info->key_filters[i],
            NULL,
            table_share->fields,
            false
            );
        if (error) {
            goto exit;
        }
    }
    
    //
    // create the field lengths
    //
    kc_info->field_lengths = (uint16_t *)my_malloc(table_share->fields*sizeof(uint16_t), MYF(MY_WME | MY_ZEROFILL));
    kc_info->length_bytes= (uchar *)my_malloc(table_share->fields, MYF(MY_WME | MY_ZEROFILL));
    kc_info->blob_fields= (uint32_t *)my_malloc(table_share->fields*sizeof(uint32_t), MYF(MY_WME | MY_ZEROFILL));
    
    if (kc_info->field_lengths == NULL || 
        kc_info->length_bytes == NULL || 
        kc_info->blob_fields == NULL ) {
        error = ENOMEM;
        goto exit;
    }
exit:
    if (error) {
        for (uint i = 0; MAX_KEY + 1; i++) {
            bitmap_free(&kc_info->key_filters[i]);
        }
        my_free(kc_info->field_lengths, MYF(MY_ALLOW_ZERO_PTR));
        my_free(kc_info->length_bytes, MYF(MY_ALLOW_ZERO_PTR));
        my_free(kc_info->blob_fields, MYF(MY_ALLOW_ZERO_PTR));
    }
    return error;
}

/** @brief
    Simple lock controls. The "share" it creates is a structure we will
    pass to each tokudb handler. Do you have to have one of these? Well, you have
    pieces that are used for locking, and they are needed to function.
*/
static TOKUDB_SHARE *get_share(const char *table_name, TABLE_SHARE* table_share) {
    TOKUDB_SHARE *share = NULL;
    int error = 0;
    uint length;

    pthread_mutex_lock(&tokudb_mutex);
    length = (uint) strlen(table_name);

    if (!(share = (TOKUDB_SHARE *) my_hash_search(&tokudb_open_tables, (uchar *) table_name, length))) {
        char *tmp_name;

        //
        // create share and fill it with all zeroes
        // hence, all pointers are initialized to NULL
        //
        if (!(share = (TOKUDB_SHARE *) 
            my_multi_malloc(MYF(MY_WME | MY_ZEROFILL), 
                            &share, sizeof(*share),
                            &tmp_name, length + 1, 
                            NullS))) {
            pthread_mutex_unlock(&tokudb_mutex);
            return NULL;
        }
        share->use_count = 0;
        share->table_name_length = length;
        share->table_name = tmp_name;
        strmov(share->table_name, table_name);

        error = allocate_key_and_col_info(table_share, &share->kc_info);
        if (error) {
            goto exit;
        }

        memset((void *) share->key_file, 0, sizeof(share->key_file));

        error = my_hash_insert(&tokudb_open_tables, (uchar *) share);
        if (error) {
            goto exit;
        }
        thr_lock_init(&share->lock);
        pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
        share_key_file_lock_init(share);
        my_rwlock_init(&share->num_DBs_lock, 0);
    }

exit:
    if (error) {
        pthread_mutex_destroy(&share->mutex);
        my_free((uchar *) share, MYF(0));
        share = NULL;
    }
    pthread_mutex_unlock(&tokudb_mutex);
    return share;
}


static void free_key_and_col_info (KEY_AND_COL_INFO* kc_info) {
    for (uint i = 0; i < MAX_KEY+1; i++) {
        bitmap_free(&kc_info->key_filters[i]);
    }
    
    for (uint i = 0; i < MAX_KEY+1; i++) {
        my_free(kc_info->cp_info[i], MYF(MY_ALLOW_ZERO_PTR));
        kc_info->cp_info[i] = NULL; // 3144
    }
    
    my_free(kc_info->field_lengths, MYF(MY_ALLOW_ZERO_PTR));
    my_free(kc_info->length_bytes, MYF(MY_ALLOW_ZERO_PTR));
    my_free(kc_info->blob_fields, MYF(MY_ALLOW_ZERO_PTR));
}

static int free_share(TOKUDB_SHARE * share, bool mutex_is_locked) {
    int error, result = 0;

    pthread_mutex_lock(&tokudb_mutex);

    if (mutex_is_locked)
        pthread_mutex_unlock(&share->mutex);
    if (!--share->use_count) {
        DBUG_PRINT("info", ("share->use_count %u", share->use_count));

        //
        // number of open DB's may not be equal to number of keys we have because add_index
        // may have added some. So, we loop through entire array and close any non-NULL value
        // It is imperative that we reset a DB to NULL once we are done with it.
        //
        for (uint i = 0; i < sizeof(share->key_file)/sizeof(share->key_file[0]); i++) {
            if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
                TOKUDB_TRACE("dbclose:%p\n", share->key_file[i]);
            }
            if (share->key_file[i]) { 
                error = share->key_file[i]->close(share->key_file[i], 0);
                assert(error == 0);
                if (error) {
                    result = error;
                }
                share->key_file[i] = NULL;
            }
        }

        free_key_and_col_info(&share->kc_info);

        if (share->status_block && (error = share->status_block->close(share->status_block, 0))) {
            assert(error == 0);
            result = error;
        }
        

        my_hash_delete(&tokudb_open_tables, (uchar *) share);
        thr_lock_delete(&share->lock);
        pthread_mutex_destroy(&share->mutex);
        share_key_file_lock_destroy(share);
        rwlock_destroy(&share->num_DBs_lock);
        my_free((uchar *) share, MYF(0));
    }
    pthread_mutex_unlock(&tokudb_mutex);

    return result;
}


#define HANDLE_INVALID_CURSOR() \
    if (cursor == NULL) { \
        error = last_cursor_error; \
        goto cleanup; \
    }

const char *ha_tokudb::table_type() const {
    extern const char * const tokudb_hton_name;
    return tokudb_hton_name;
} 

const char *ha_tokudb::index_type(uint inx) {
    return "BTREE";
}

/* 
 *  returns NULL terminated file extension string
 */
const char **ha_tokudb::bas_ext() const {
    TOKUDB_DBUG_ENTER("ha_tokudb::bas_ext");
    DBUG_RETURN(ha_tokudb_exts);
}

static inline bool is_insert_ignore (THD* thd) {
    //
    // from http://lists.mysql.com/internals/37735
    //
    return thd->lex->ignore && thd->lex->duplicates == DUP_ERROR;
}

static inline bool is_replace_into(THD* thd) {
    return thd->lex->duplicates == DUP_REPLACE;
}

static inline bool do_ignore_flag_optimization(THD* thd, TABLE* table, bool opt_eligible) {
    uint pk_insert_mode = get_pk_insert_mode(thd);
    return ( 
        opt_eligible && 
        (is_replace_into(thd) || is_insert_ignore(thd)) && 
        ((!table->triggers && pk_insert_mode < 2) || pk_insert_mode == 0)
        );
}

ulonglong ha_tokudb::table_flags() const {
    return (table && do_ignore_flag_optimization(ha_thd(), table, share->replace_into_fast) ? 
        int_table_flags | HA_BINLOG_STMT_CAPABLE : 
#if defined(HA_GENERAL_ONLINE)
        int_table_flags | HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE | HA_ONLINE_ALTER);
#else
        int_table_flags | HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE);
#endif
}

//
// Returns a bit mask of capabilities of the key or its part specified by 
// the arguments. The capabilities are defined in sql/handler.h.
//
ulong ha_tokudb::index_flags(uint idx, uint part, bool all_parts) const {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_flags");
    assert(table_share);
    ulong flags = (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_KEYREAD_ONLY | HA_READ_RANGE);
    if (table_share->key_info[idx].flags & HA_CLUSTERING) {
        flags |= HA_CLUSTERED_INDEX;
    }
    DBUG_RETURN(flags);
}


//
// struct that will be used as a context for smart DBT callbacks
// contains parameters needed to complete the smart DBT cursor call
//
typedef struct smart_dbt_info {
    ha_tokudb* ha; //instance to ha_tokudb needed for reading the row
    uchar* buf; // output buffer where row will be written
    uint keynr; // index into share->key_file that represents DB we are currently operating on
} *SMART_DBT_INFO;

typedef struct smart_dbt_bf_info {
    ha_tokudb* ha;
    bool need_val;
    int direction;
    THD* thd;
} *SMART_DBT_BF_INFO;

typedef struct index_read_info {
    struct smart_dbt_info smart_dbt_info;
    int cmp;
    DBT* orig_key;
} *INDEX_READ_INFO;


static int ai_poll_fun(void *extra, float progress) {
    LOADER_CONTEXT context = (LOADER_CONTEXT)extra;
    if (context->thd->killed) {
        sprintf(context->write_status_msg, "The process has been killed, aborting add index.");
        return ER_ABORTING_CONNECTION;
    }
    float percentage = progress * 100;
    sprintf(context->write_status_msg, "Adding of indexes about %.1f%% done", percentage);
    thd_proc_info(context->thd, context->write_status_msg);
#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    thd_progress_report(context->thd, (unsigned long long) percentage, 100);
#endif
    return 0;
}

static int loader_poll_fun(void *extra, float progress) {
    LOADER_CONTEXT context = (LOADER_CONTEXT)extra;
    if (context->thd->killed) {
        sprintf(context->write_status_msg, "The process has been killed, aborting bulk load.");
        return ER_ABORTING_CONNECTION;
    }
    float percentage = progress * 100;
    sprintf(context->write_status_msg, "Loading of data about %.1f%% done", percentage);
    thd_proc_info(context->thd, context->write_status_msg);
#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    thd_progress_report(context->thd, (unsigned long long) percentage, 100);
#endif
    return 0;
}

static void loader_ai_err_fun(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra) {
    LOADER_CONTEXT context = (LOADER_CONTEXT)error_extra;
    assert(context->ha);
    context->ha->set_loader_error(err);
}

static void loader_dup_fun(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra) {
    LOADER_CONTEXT context = (LOADER_CONTEXT)error_extra;
    assert(context->ha);
    context->ha->set_loader_error(err);
    if (err == DB_KEYEXIST) {
        context->ha->set_dup_value_for_pk(key);
    }
}

//
// smart DBT callback function for optimize
// in optimize, we want to flatten DB by doing
// a full table scan. Therefore, we don't
// want to actually do anything with the data, hence
// callback does nothing
//
static int smart_dbt_do_nothing (DBT const *key, DBT  const *row, void *context) {
  return 0;
}

static int smart_dbt_metacallback (DBT const *key, DBT  const *row, void *context) {
    DBT* val = (DBT *)context;
    val->data = my_malloc(row->size, MYF(MY_WME|MY_ZEROFILL));
    if (val->data == NULL) return ENOMEM;
    memcpy(val->data, row->data, row->size);
    val->size = row->size;
    return 0;
}


static int
smart_dbt_callback_rowread_ptquery (DBT const *key, DBT  const *row, void *context) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->extract_hidden_primary_key(info->keynr, key);
    return info->ha->read_row_callback(info->buf,info->keynr,row,key);
}

//
// Smart DBT callback function in case where we have a covering index
//
static int
smart_dbt_callback_keyread(DBT const *key, DBT  const *row, void *context) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->extract_hidden_primary_key(info->keynr, key);
    info->ha->read_key_only(info->buf,info->keynr,key);
    return 0;
}

//
// Smart DBT callback function in case where we do NOT have a covering index
//
static int
smart_dbt_callback_rowread(DBT const *key, DBT  const *row, void *context) {
    int error = 0;
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->extract_hidden_primary_key(info->keynr, key);
    error = info->ha->read_primary_key(info->buf,info->keynr,row,key);
    return error;
}

//
// Smart DBT callback function in case where we have a covering index
//
static int
smart_dbt_callback_ir_keyread(DBT const *key, DBT  const *row, void *context) {
    INDEX_READ_INFO ir_info = (INDEX_READ_INFO)context;
    ir_info->cmp = ir_info->smart_dbt_info.ha->prefix_cmp_dbts(ir_info->smart_dbt_info.keynr, ir_info->orig_key, key);
    if (ir_info->cmp) {
        return 0;
    }
    return smart_dbt_callback_keyread(key, row, &ir_info->smart_dbt_info);
}

static int
smart_dbt_callback_lookup(DBT const *key, DBT  const *row, void *context) {
    INDEX_READ_INFO ir_info = (INDEX_READ_INFO)context;
    ir_info->cmp = ir_info->smart_dbt_info.ha->prefix_cmp_dbts(ir_info->smart_dbt_info.keynr, ir_info->orig_key, key);
    return 0;
}


//
// Smart DBT callback function in case where we do NOT have a covering index
//
static int
smart_dbt_callback_ir_rowread(DBT const *key, DBT  const *row, void *context) {
    INDEX_READ_INFO ir_info = (INDEX_READ_INFO)context;
    ir_info->cmp = ir_info->smart_dbt_info.ha->prefix_cmp_dbts(ir_info->smart_dbt_info.keynr, ir_info->orig_key, key);
    if (ir_info->cmp) {
        return 0;
    }
    return smart_dbt_callback_rowread(key, row, &ir_info->smart_dbt_info);
}

//
// macro for Smart DBT callback function, 
// so we do not need to put this long line of code in multiple places
//
#define SMART_DBT_CALLBACK ( this->key_read ? smart_dbt_callback_keyread : smart_dbt_callback_rowread ) 
#define SMART_DBT_IR_CALLBACK ( this->key_read ? smart_dbt_callback_ir_keyread : smart_dbt_callback_ir_rowread ) 


//
// macro that modifies read flag for cursor operations depending on whether
// we have preacquired lock or not
//
#define SET_PRELOCK_FLAG(flg) ((flg) | (range_lock_grabbed ? (use_write_locks ? DB_PRELOCKED_WRITE : DB_PRELOCKED) : 0))

//
// This method retrieves the value of the auto increment column of a record in MySQL format
// This was basically taken from MyISAM
// Parameters:
//              type - the type of the auto increment column (e.g. int, float, double...)
//              offset - offset into the record where the auto increment column is stored
//      [in]    record - MySQL row whose auto increment value we want to extract
// Returns:
//      The value of the auto increment column in record
//
ulonglong retrieve_auto_increment(uint16 type, uint32 offset,const uchar *record)
{
    const uchar *key;     /* Key */
    ulonglong   unsigned_autoinc = 0;  /* Unsigned auto-increment */
    longlong      signed_autoinc = 0;  /* Signed auto-increment */
    enum { unsigned_type, signed_type } autoinc_type;
    float float_tmp;   /* Temporary variable */
    double double_tmp; /* Temporary variable */

    key = ((uchar *) record) + offset;

    /* Set default autoincrement type */
    autoinc_type = unsigned_type;

    switch (type) {
    case HA_KEYTYPE_INT8:
        signed_autoinc   = (longlong) *(char*)key;
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_BINARY:
        unsigned_autoinc = (ulonglong) *(uchar*) key;
        break;

    case HA_KEYTYPE_SHORT_INT:
        signed_autoinc   = (longlong) sint2korr(key);
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_USHORT_INT:
        unsigned_autoinc = (ulonglong) uint2korr(key);
        break;

    case HA_KEYTYPE_LONG_INT:
        signed_autoinc   = (longlong) sint4korr(key);
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_ULONG_INT:
        unsigned_autoinc = (ulonglong) uint4korr(key);
        break;

    case HA_KEYTYPE_INT24:
        signed_autoinc   = (longlong) sint3korr(key);
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_UINT24:
        unsigned_autoinc = (ulonglong) uint3korr(key);
    break;

    case HA_KEYTYPE_LONGLONG:
        signed_autoinc   = sint8korr(key);
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_ULONGLONG:
        unsigned_autoinc = uint8korr(key);
        break;

    /* The remaining two cases should not be used but are included for 
       compatibility */
    case HA_KEYTYPE_FLOAT:                      
        float4get(float_tmp, key);  /* Note: float4get is a macro */
        signed_autoinc   = (longlong) float_tmp;
        autoinc_type     = signed_type;
        break;

    case HA_KEYTYPE_DOUBLE:
        float8get(double_tmp, key); /* Note: float8get is a macro */
        signed_autoinc   = (longlong) double_tmp;
        autoinc_type     = signed_type;
        break;

    default:
        DBUG_ASSERT(0);
        unsigned_autoinc = 0;
    }

    if (signed_autoinc < 0) {
        signed_autoinc = 0;
    }

    return autoinc_type == unsigned_type ?  
           unsigned_autoinc : (ulonglong) signed_autoinc;
}

static inline bool
is_null_field( TABLE* table, Field* field, const uchar* record) {
    uint null_offset;
    bool ret_val;
    if (!field->real_maybe_null()) {
        ret_val = false;
        goto exitpt;
    }
    null_offset = get_null_offset(table,field);
    ret_val = (record[null_offset] & field->null_bit) ? true: false;

exitpt:
    return ret_val;
}

static inline ulong field_offset(Field* field, TABLE* table) {
    return((ulong) (field->ptr - table->record[0]));
}

static inline HA_TOKU_ISO_LEVEL tx_to_toku_iso(ulong tx_isolation) {
    if (tx_isolation == ISO_READ_UNCOMMITTED) {
        return hatoku_iso_read_uncommitted;
    }
    else if (tx_isolation == ISO_READ_COMMITTED) {
        return hatoku_iso_read_committed;
    }
    else if (tx_isolation == ISO_REPEATABLE_READ) {
        return hatoku_iso_repeatable_read;
    }
    else {
        return hatoku_iso_serializable;
    }
}

static inline uint32_t toku_iso_to_txn_flag (HA_TOKU_ISO_LEVEL lvl) {
    if (lvl == hatoku_iso_read_uncommitted) {
        return DB_READ_UNCOMMITTED;
    }
    else if (lvl == hatoku_iso_read_committed) {
        return DB_READ_COMMITTED;
    }
    else if (lvl == hatoku_iso_repeatable_read) {
        return DB_TXN_SNAPSHOT;
    }
    else {
        return 0;
    }
}

static int filter_key_part_compare (const void* left, const void* right) {
    FILTER_KEY_PART_INFO* left_part= (FILTER_KEY_PART_INFO *)left;
    FILTER_KEY_PART_INFO* right_part = (FILTER_KEY_PART_INFO *)right;
    return left_part->offset - right_part->offset;
}

//
// Be very careful with parameters passed to this function. Who knows
// if key, table have proper info set. I had to verify by checking
// in the debugger.
//
void set_key_filter(MY_BITMAP* key_filter, KEY* key, TABLE* table, bool get_offset_from_keypart) {
    FILTER_KEY_PART_INFO parts[MAX_REF_PARTS];
    uint curr_skip_index = 0;

    for (uint i = 0; i < key->key_parts; i++) {
        //
        // horrendous hack due to bugs in mysql, basically
        // we cannot always reliably get the offset from the same source
        //
        parts[i].offset = get_offset_from_keypart ? key->key_part[i].offset : field_offset(key->key_part[i].field, table);
        parts[i].part_index = i;
    }
    qsort(
        parts, // start of array
        key->key_parts, //num elements
        sizeof(*parts), //size of each element
        filter_key_part_compare
        );

    for (uint i = 0; i < table->s->fields; i++) {
        Field* field = table->field[i];
        uint curr_field_offset = field_offset(field, table);
        if (curr_skip_index < key->key_parts) {
            uint curr_skip_offset = 0;
            curr_skip_offset = parts[curr_skip_index].offset;
            if (curr_skip_offset == curr_field_offset) {
                //
                // we have hit a field that is a portion of the primary key
                //
                uint curr_key_index = parts[curr_skip_index].part_index;
                curr_skip_index++;
                //
                // only choose to continue over the key if the key's length matches the field's length
                // otherwise, we may have a situation where the column is a varchar(10), the
                // key is only the first 3 characters, and we end up losing the last 7 bytes of the
                // column
                //
                TOKU_TYPE toku_type;
                toku_type = mysql_to_toku_type(field);
                switch(toku_type) {
                case(toku_type_blob):
                    break;
                case(toku_type_varbinary):
                case(toku_type_varstring):
                case(toku_type_fixbinary):
                case(toku_type_fixstring):
                    if (key->key_part[curr_key_index].length == field->field_length) {
                        bitmap_set_bit(key_filter,i);
                    }
                    break;
                default:
                    bitmap_set_bit(key_filter,i);
                    break;
                }
            }
        }
    }
}

static inline uchar* pack_fixed_field(
    uchar* to_tokudb,
    const uchar* from_mysql,
    uint32_t num_bytes
    )
{
    switch (num_bytes) {
    case (1):
        memcpy(to_tokudb, from_mysql, 1);
        break;
    case (2):
        memcpy(to_tokudb, from_mysql, 2);
        break;
    case (3):
        memcpy(to_tokudb, from_mysql, 3);
        break;
    case (4):
        memcpy(to_tokudb, from_mysql, 4);
        break;
    case (8):
        memcpy(to_tokudb, from_mysql, 8);
        break;
    default:
        memcpy(to_tokudb, from_mysql, num_bytes);
        break;
    }
    return to_tokudb+num_bytes;
}

static inline const uchar* unpack_fixed_field(
    uchar* to_mysql,
    const uchar* from_tokudb,
    uint32_t num_bytes
    )
{
    switch (num_bytes) {
    case (1):
        memcpy(to_mysql, from_tokudb, 1);
        break;
    case (2):
        memcpy(to_mysql, from_tokudb, 2);
        break;
    case (3):
        memcpy(to_mysql, from_tokudb, 3);
        break;
    case (4):
        memcpy(to_mysql, from_tokudb, 4);
        break;
    case (8):
        memcpy(to_mysql, from_tokudb, 8);
        break;
    default:
        memcpy(to_mysql, from_tokudb, num_bytes);
        break;
    }
    return from_tokudb+num_bytes;
}

static inline uchar* write_var_field(
    uchar* to_tokudb_offset_ptr, //location where offset data is going to be written
    uchar* to_tokudb_data, // location where data is going to be written
    uchar* to_tokudb_offset_start, //location where offset starts, IS THIS A BAD NAME????
    const uchar * data, // the data to write
    uint32_t data_length, // length of data to write
    uint32_t offset_bytes // number of offset bytes
    )
{
    memcpy(to_tokudb_data, data, data_length);
    //
    // for offset, we pack the offset where the data ENDS!
    //
    uint32_t offset = to_tokudb_data + data_length - to_tokudb_offset_start;
    switch(offset_bytes) {
    case (1):
        to_tokudb_offset_ptr[0] = (uchar)offset;
        break;
    case (2):
        int2store(to_tokudb_offset_ptr,offset);
        break;
    default:
        assert(false);
        break;
    }
    return to_tokudb_data + data_length;
}

static inline uint32_t get_var_data_length(
    const uchar * from_mysql, 
    uint32_t mysql_length_bytes 
    ) 
{
    uint32_t data_length;
    switch(mysql_length_bytes) {
    case(1):
        data_length = from_mysql[0];
        break;
    case(2):
        data_length = uint2korr(from_mysql);
        break;
    default:
        assert(false);
        break;
    }
    return data_length;
}

static inline uchar* pack_var_field(
    uchar* to_tokudb_offset_ptr, //location where offset data is going to be written
    uchar* to_tokudb_data, // pointer to where tokudb data should be written
    uchar* to_tokudb_offset_start, //location where data starts, IS THIS A BAD NAME????
    const uchar * from_mysql, // mysql data
    uint32_t mysql_length_bytes, //number of bytes used to store length in from_mysql
    uint32_t offset_bytes //number of offset_bytes used in tokudb row
    )
{
    uint data_length = get_var_data_length(from_mysql, mysql_length_bytes);    
    return write_var_field(
        to_tokudb_offset_ptr,
        to_tokudb_data,
        to_tokudb_offset_start,
        from_mysql + mysql_length_bytes,
        data_length,
        offset_bytes
        );
}

static inline void unpack_var_field(
    uchar* to_mysql,
    const uchar* from_tokudb_data,
    uint32_t from_tokudb_data_len,
    uint32_t mysql_length_bytes
    )
{
    //
    // store the length
    //
    switch (mysql_length_bytes) {
    case(1):
        to_mysql[0] = (uchar)from_tokudb_data_len;
        break;
    case(2):
        int2store(to_mysql, from_tokudb_data_len);
        break;
    default:
        assert(false);
        break;
    }
    //
    // store the data
    //
    memcpy(to_mysql+mysql_length_bytes, from_tokudb_data, from_tokudb_data_len);
}

static uchar* pack_toku_field_blob(
    uchar* to_tokudb,
    const uchar* from_mysql,
    Field* field
    )
{
    uint32_t len_bytes = field->row_pack_length();
    uint32_t length = 0;
    uchar* data_ptr = NULL;
    memcpy(to_tokudb, from_mysql, len_bytes);

    switch (len_bytes) {
    case (1):
        length = (uint32_t)(*from_mysql);
        break;
    case (2):
        length = uint2korr(from_mysql);
        break;
    case (3):
        length = uint3korr(from_mysql);
        break;
    case (4):
        length = uint4korr(from_mysql);
        break;
    default:
        assert(false);
    }

    if (length > 0) {
        memcpy_fixed((uchar *)(&data_ptr), from_mysql + len_bytes, sizeof(uchar*));
        memcpy(to_tokudb + len_bytes, data_ptr, length);
    }
    return (to_tokudb + len_bytes + length);
}


static int add_table_to_metadata(const char *name, TABLE* table, DB_TXN* txn) {
    int error = 0;
    DBT key;
    DBT val;
    uchar hidden_primary_key = (table->s->primary_key >= MAX_KEY);
    assert(txn);
    
    memset((void *)&key, 0, sizeof(key));
    memset((void *)&val, 0, sizeof(val));
    key.data = (void *)name;
    key.size = strlen(name) + 1;
    val.data = &hidden_primary_key;
    val.size = sizeof(hidden_primary_key);
    error = metadata_db->put(
        metadata_db,
        txn,
        &key,
        &val,
        0
        );
    return error;
}

static int drop_table_from_metadata(const char *name, DB_TXN* txn) {
    int error = 0;
    DBT key;
    DBT data;
    assert(txn);
    memset((void *)&key, 0, sizeof(key));
    memset((void *)&data, 0, sizeof(data));
    key.data = (void *)name;
    key.size = strlen(name) + 1;
    error = metadata_db->del(
        metadata_db, 
        txn, 
        &key , 
        DB_DELETE_ANY
        );
    return error;
}

static int rename_table_in_metadata(const char *from, const char *to, DB_TXN* txn) {
    int error = 0;
    DBT from_key;
    DBT to_key;
    DBT val;
    assert(txn);
    
    memset((void *)&from_key, 0, sizeof(from_key));
    memset((void *)&to_key, 0, sizeof(to_key));
    memset((void *)&val, 0, sizeof(val));
    from_key.data = (void *)from;
    from_key.size = strlen(from) + 1;
    to_key.data = (void *)to;
    to_key.size = strlen(to) + 1;
    
    error = metadata_db->getf_set(
        metadata_db, 
        txn, 
        0, 
        &from_key, 
        smart_dbt_metacallback, 
        &val
        );

    if (error) {
        goto cleanup;
    }

    error = metadata_db->put(
        metadata_db,
        txn,
        &to_key,
        &val,
        0
        );
    if (error) {
        goto cleanup;
    }

    error = metadata_db->del(
        metadata_db, 
        txn, 
        &from_key, 
        DB_DELETE_ANY
        );
    if (error) {
        goto cleanup;
    }

    error = 0;

cleanup:
    my_free(val.data, MYF(MY_ALLOW_ZERO_PTR));

    return error;
}


static int check_table_in_metadata(const char *name, bool* table_found, DB_TXN* txn) {
    int error = 0;
    DBT key;
    pthread_mutex_lock(&tokudb_meta_mutex);
    memset((void *)&key, 0, sizeof(key));
    key.data = (void *)name;
    key.size = strlen(name) + 1;
    
    error = metadata_db->getf_set(
        metadata_db, 
        txn, 
        0, 
        &key, 
        smart_dbt_do_nothing, 
        NULL
        );

    if (error == 0) {
        *table_found = true;
    }
    else if (error == DB_NOTFOUND){
        *table_found = false;
        error = 0;
    }

    pthread_mutex_unlock(&tokudb_meta_mutex);
    return error;
}

static int create_tokudb_trx_data_instance(tokudb_trx_data** out_trx) {
    int error;
    tokudb_trx_data* trx = NULL;
    trx = (tokudb_trx_data *) my_malloc(sizeof(*trx), MYF(MY_ZEROFILL));
    if (!trx) {
        error = ENOMEM;
        goto cleanup;
    }

    *out_trx = trx;
    error = 0;
cleanup:
    return error;
}


static inline int tokudb_generate_row(
    DB *dest_db, 
    DB *src_db,
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *src_key, 
    const DBT *src_val
    ) 
{
    int error;

    DB* curr_db = dest_db;
    uchar* row_desc = NULL;
    uint32_t desc_size;
    uchar* buff = NULL;
    uint32_t max_key_len = 0;
    
    row_desc = (uchar *)curr_db->descriptor->dbt.data;
    row_desc += (*(uint32_t *)row_desc);
    desc_size = (*(uint32_t *)row_desc) - 4;
    row_desc += 4;
    
    if (is_key_pk(row_desc, desc_size)) {
        if (dest_key->flags == DB_DBT_REALLOC && dest_key->data != NULL) {
            free(dest_key->data);
        }
        if (dest_val != NULL) {
            if (dest_val->flags == DB_DBT_REALLOC && dest_val->data != NULL) {
                free(dest_val->data);
            }
        }
        dest_key->data = src_key->data;
        dest_key->size = src_key->size;
        dest_key->flags = 0;
        if (dest_val != NULL) {
            dest_val->data = src_val->data;
            dest_val->size = src_val->size;
            dest_val->flags = 0;
        }
        error = 0;
        goto cleanup;
    }
    // at this point, we need to create the key/val and set it
    // in the DBTs
    if (dest_key->flags == 0) {
        dest_key->ulen = 0;
        dest_key->size = 0;
        dest_key->data = NULL;
        dest_key->flags = DB_DBT_REALLOC;
    }
    if (dest_key->flags == DB_DBT_REALLOC) {
        max_key_len = max_key_size_from_desc(row_desc, desc_size);
        max_key_len += src_key->size;
        
        if (max_key_len > dest_key->ulen) {
            void* old_ptr = dest_key->data;
            void* new_ptr = NULL;
            new_ptr = realloc(old_ptr, max_key_len);
            assert(new_ptr);
            dest_key->data = new_ptr;
            dest_key->ulen = max_key_len;
        }

        buff = (uchar *)dest_key->data;
        assert(buff != NULL && max_key_len > 0);
    }
    else {
        assert(false);
    }

    dest_key->size = pack_key_from_desc(
        buff,
        row_desc,
        desc_size,
        src_key,
        src_val
        );
    assert(dest_key->ulen >= dest_key->size);
    if (tokudb_debug & TOKUDB_DEBUG_CHECK_KEY && !max_key_len) {
        max_key_len = max_key_size_from_desc(row_desc, desc_size);
        max_key_len += src_key->size;
    }
    if (max_key_len) {
        assert(max_key_len >= dest_key->size);
    }

    row_desc += desc_size;
    desc_size = (*(uint32_t *)row_desc) - 4;
    row_desc += 4;
    if (dest_val != NULL) {
        if (!is_key_clustering(row_desc, desc_size) || src_val->size == 0) {
            dest_val->size = 0;
        }
        else {
            uchar* buff = NULL;
            if (dest_val->flags == 0) {
                dest_val->ulen = 0;
                dest_val->size = 0;
                dest_val->data = NULL;
                dest_val->flags = DB_DBT_REALLOC;
            }
            if (dest_val->flags == DB_DBT_REALLOC){
                if (dest_val->ulen < src_val->size) {
                    void* old_ptr = dest_val->data;
                    void* new_ptr = NULL;
                    new_ptr = realloc(old_ptr, src_val->size);
                    assert(new_ptr);
                    dest_val->data = new_ptr;
                    dest_val->ulen = src_val->size;
                }
                buff = (uchar *)dest_val->data;
                assert(buff != NULL);
            }
            else {
                assert(false);
            }
            dest_val->size = pack_clustering_val_from_desc(
                buff,
                row_desc,
                desc_size,
                src_val
                );
            assert(dest_val->ulen >= dest_val->size);
        }
    }
    error = 0;
cleanup:
    return error;
}

static int generate_row_for_del(
    DB *dest_db, 
    DB *src_db,
    DBT *dest_key,
    const DBT *src_key, 
    const DBT *src_val
    )
{
    return tokudb_generate_row(
        dest_db,
        src_db,
        dest_key,
        NULL,
        src_key,
        src_val
        );
}


static int generate_row_for_put(
    DB *dest_db, 
    DB *src_db,
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *src_key, 
    const DBT *src_val
    ) 
{
    return tokudb_generate_row(
        dest_db,
        src_db,
        dest_key,
        dest_val,
        src_key,
        src_val
        );
}


ha_tokudb::ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg):handler(hton, table_arg) 
    // flags defined in sql\handler.h
{
    share = NULL;
    int_table_flags = HA_REC_NOT_IN_SEQ  | HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_PRIMARY_KEY_IN_READ_INDEX | 
                    HA_FILE_BASED | HA_AUTO_PART_KEY | HA_TABLE_SCAN_ON_INDEX | HA_CAN_WRITE_DURING_OPTIMIZE;
    alloc_ptr = NULL;
    rec_buff = NULL;
    rec_update_buff = NULL;
    transaction = NULL;
    is_fast_alter_running = false;
    cursor = NULL;
    fixed_cols_for_query = NULL;
    var_cols_for_query = NULL;
    num_fixed_cols_for_query = 0;
    num_var_cols_for_query = 0;
    unpack_entire_row = true;
    read_blobs = false;
    read_key = false;
    added_rows = 0;
    deleted_rows = 0;
    last_dup_key = UINT_MAX;
    using_ignore = 0;
    last_cursor_error = 0;
    range_lock_grabbed = false;
    blob_buff = NULL;
    num_blob_bytes = 0;
    delay_updating_ai_metadata = false;
    ai_metadata_update_required = false;
    memset(mult_key_dbt, 0, sizeof(mult_key_dbt));
    memset(mult_rec_dbt, 0, sizeof(mult_rec_dbt));
    loader = NULL;
    abort_loader = false;
    memset(&lc, 0, sizeof(lc));
    lock.type = TL_IGNORE;
    for (uint32_t i = 0; i < MAX_KEY+1; i++) {
        mult_put_flags[i] = 0;
        mult_del_flags[i] = DB_DELETE_ANY;
        mult_dbt_flags[i] = DB_DBT_REALLOC;
    }
    num_DBs_locked_in_bulk = false;
    lock_count = 0;
    use_write_locks = false;
    range_query_buff = NULL;
    size_range_query_buff = 0;
    bytes_used_in_range_query_buff = 0;
    curr_range_query_buff_offset = 0;
    doing_bulk_fetch = false;
    prelocked_left_range_size = 0;
    prelocked_right_range_size = 0;
    tokudb_active_index = MAX_KEY;
}

ha_tokudb::~ha_tokudb() {
}

//
// states if table has an auto increment column, if so, sets index where auto inc column is to index
// Parameters:
//      [out]   index - if auto inc exists, then this param is set to where it exists in table, if not, then unchanged
// Returns:
//      true if auto inc column exists, false otherwise
//
bool ha_tokudb::has_auto_increment_flag(uint* index) {
    //
    // check to see if we have auto increment field
    //
    bool ai_found = false;
    uint ai_index = 0;
    for (uint i = 0; i < table_share->fields; i++, ai_index++) {
        Field* field = table->field[i];
        if (field->flags & AUTO_INCREMENT_FLAG) {
            ai_found = true;
            *index = ai_index;
            break;
        }
    }
    return ai_found;
}

#define status_dict_pagesize 1024

static int open_status_dictionary(DB** ptr, const char* name, DB_TXN* txn) {
    int error;
    char* newname = NULL;
    uint open_mode = DB_THREAD;
    uint32_t pagesize = 0;
    newname = (char *)my_malloc(
        get_max_dict_name_path_length(name), 
        MYF(MY_WME)
        );
    if (newname == NULL) {
        error = ENOMEM;
        goto cleanup;
    }
    make_name(newname, name, "status");
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("open:%s\n", newname);
    }
    error = db_create(ptr, db_env, 0);
    if (error) { goto cleanup; }
    
    error = (*ptr)->open((*ptr), txn, newname, NULL, DB_BTREE, open_mode, 0);
    if (error) { 
        goto cleanup; 
    }

    error = (*ptr)->get_pagesize(*ptr, &pagesize);
    if (error) { 
        goto cleanup; 
    }

    if (pagesize > status_dict_pagesize) {
        error = (*ptr)->change_pagesize(*ptr, status_dict_pagesize);
        if (error) { goto cleanup; }
    }
    
cleanup:
    if (error) {
        if (*ptr) {
            int r = (*ptr)->close(*ptr, 0);
            assert(r==0);
            *ptr = NULL;
        }
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

int ha_tokudb::open_main_dictionary(const char* name, bool is_read_only, DB_TXN* txn) {
    int error;    
    char* newname = NULL;
    uint open_flags = (is_read_only ? DB_RDONLY : 0) | DB_THREAD;

    assert(share->file == NULL);
    assert(share->key_file[primary_key] == NULL);

    newname = (char *)my_malloc(
        get_max_dict_name_path_length(name),
        MYF(MY_WME|MY_ZEROFILL)
        );
    if (newname == NULL) { 
        error = ENOMEM;
        goto exit;
    }
    make_name(newname, name, "main");

    error = db_create(&share->file, db_env, 0);
    if (error) {
        goto exit;
    }
    share->key_file[primary_key] = share->file;

    error = share->file->open(share->file, txn, newname, NULL, DB_BTREE, open_flags, 0);
    if (error) {
        goto exit;
    }
    
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("open:%s:file=%p\n", newname, share->file);
    }

    error = 0;
exit:
    if (error) {
        if (share->file) {
            int r = share->file->close(
                share->file,
                0
                );
            assert(r==0);
            share->file = NULL;
            share->key_file[primary_key] = NULL;
        }
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

//
// Open a secondary table, the key will be a secondary index, the data will be a primary key
//
int ha_tokudb::open_secondary_dictionary(DB** ptr, KEY* key_info, const char* name, bool is_read_only, DB_TXN* txn) {
    int error = ENOSYS;
    char dict_name[MAX_DICT_NAME_LEN];
    uint open_flags = (is_read_only ? DB_RDONLY : 0) | DB_THREAD;
    char* newname = NULL;
    uint newname_len = 0;
    
    sprintf(dict_name, "key-%s", key_info->name);

    newname_len = get_max_dict_name_path_length(name);
    newname = (char *)my_malloc(newname_len, MYF(MY_WME|MY_ZEROFILL));
    if (newname == NULL) {
        error = ENOMEM;
        goto cleanup;
    }
    make_name(newname, name, dict_name);


    if ((error = db_create(ptr, db_env, 0))) {
        my_errno = error;
        goto cleanup;
    }


    if ((error = (*ptr)->open(*ptr, txn, newname, NULL, DB_BTREE, open_flags, 0))) {
        my_errno = error;
        goto cleanup;
    }
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("open:%s:file=%p\n", newname, *ptr);
    }
cleanup:
    if (error) {
        if (*ptr) {
            int r = (*ptr)->close(*ptr, 0);
            assert(r==0);
            *ptr = NULL;
        }
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

static int initialize_col_pack_info(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
    int error = ENOSYS;
    //
    // set up the cp_info
    //
    assert(kc_info->cp_info[keynr] == NULL);
    kc_info->cp_info[keynr] = (COL_PACK_INFO *)my_malloc(
        table_share->fields*sizeof(COL_PACK_INFO), 
        MYF(MY_WME | MY_ZEROFILL)
        );
    if (kc_info->cp_info[keynr] == NULL) {
        error = ENOMEM;
        goto exit;
    }
    {
    uint32_t curr_fixed_offset = 0;
    uint32_t curr_var_index = 0;
    for (uint j = 0; j < table_share->fields; j++) {
        COL_PACK_INFO* curr = &kc_info->cp_info[keynr][j];
        //
        // need to set the offsets / indexes
        // offsets are calculated AFTER the NULL bytes
        //
        if (!bitmap_is_set(&kc_info->key_filters[keynr],j)) {
            if (kc_info->field_lengths[j]) {
                curr->col_pack_val = curr_fixed_offset;
                curr_fixed_offset += kc_info->field_lengths[j];
            }
            else if (kc_info->length_bytes[j]) {
                curr->col_pack_val = curr_var_index;
                curr_var_index++;
            }
        }
    }
    
    //
    // set up the mcp_info
    //
    kc_info->mcp_info[keynr].fixed_field_size = get_fixed_field_size(
        kc_info,
        table_share,
        keynr
        );
    kc_info->mcp_info[keynr].len_of_offsets = get_len_of_offsets(
        kc_info,
        table_share,
        keynr
        );

    error = 0;
    }
exit:
    return error;
}

// reset the kc_info state at keynr
static void reset_key_and_col_info(KEY_AND_COL_INFO *kc_info, uint keynr) {
    bitmap_clear_all(&kc_info->key_filters[keynr]);
    my_free(kc_info->cp_info[keynr], MYF(MY_ALLOW_ZERO_PTR));
    kc_info->cp_info[keynr] = NULL;
    kc_info->mcp_info[keynr] = (MULTI_COL_PACK_INFO) { 0, 0 };
}

static int initialize_key_and_col_info(TABLE_SHARE* table_share, TABLE* table, KEY_AND_COL_INFO* kc_info, uint hidden_primary_key, uint primary_key) {
    int error = 0;
    uint32_t curr_blob_field_index = 0;
    uint32_t max_var_bytes = 0;
    //
    // fill in the field lengths. 0 means it is a variable sized field length
    // fill in length_bytes, 0 means it is fixed or blob
    //
    for (uint i = 0; i < table_share->fields; i++) {
        Field* field = table_share->field[i];
        TOKU_TYPE toku_type = mysql_to_toku_type(field);
        uint32 pack_length = 0;
        switch (toku_type) {
        case toku_type_int:
        case toku_type_double:
        case toku_type_float:
        case toku_type_fixbinary:
        case toku_type_fixstring:
            pack_length = field->pack_length();
            assert(pack_length < 1<<16);
            kc_info->field_lengths[i] = (uint16_t)pack_length;
            kc_info->length_bytes[i] = 0;
            break;
        case toku_type_blob:
            kc_info->field_lengths[i] = 0;
            kc_info->length_bytes[i] = 0;
            kc_info->blob_fields[curr_blob_field_index] = i;
            curr_blob_field_index++;
            break;
        case toku_type_varstring:
        case toku_type_varbinary:
            //
            // meaning it is variable sized
            //
            kc_info->field_lengths[i] = 0;
            kc_info->length_bytes[i] = (uchar)((Field_varstring *)field)->length_bytes;
            max_var_bytes += field->field_length;
            break;
        default:
            assert(false);
        }
    }
    kc_info->num_blobs = curr_blob_field_index;

    //
    // initialize share->num_offset_bytes
    // because MAX_REF_LENGTH is 65536, we
    // can safely set num_offset_bytes to 1 or 2
    //
    if (max_var_bytes < 256) {
        kc_info->num_offset_bytes = 1;
    }
    else {
        kc_info->num_offset_bytes = 2;
    }


    for (uint i = 0; i < table_share->keys + test(hidden_primary_key); i++) {
        //
        // do the cluster/primary key filtering calculations
        //
        if (! (i==primary_key && hidden_primary_key) ){        
            if ( i == primary_key ) {
                set_key_filter(
                    &kc_info->key_filters[primary_key],
                    &table_share->key_info[primary_key],
                    table,
                    true
                    );
            }
            else {
                set_key_filter(
                    &kc_info->key_filters[i],
                    &table_share->key_info[i],
                    table,
                    true
                    );
                if (!hidden_primary_key) {
                    set_key_filter(
                        &kc_info->key_filters[i],
                        &table_share->key_info[primary_key],
                        table,
                        true
                        );
                }
            }
        }
        if (i == primary_key || table_share->key_info[i].flags & HA_CLUSTERING) {
            error = initialize_col_pack_info(kc_info,table_share,i);
            if (error) {
                goto exit;
            }
        }

    }
exit:
    return error;
}

bool ha_tokudb::can_replace_into_be_fast(TABLE_SHARE* table_share, KEY_AND_COL_INFO* kc_info, uint pk) {
    uint curr_num_DBs = table_share->keys + test(hidden_primary_key);
    bool ret_val;
    if (curr_num_DBs == 1) {
        ret_val = true;
        goto exit;
    }
    ret_val = true;
    for (uint curr_index = 0; curr_index < table_share->keys; curr_index++) {
        if (curr_index == pk) continue;
        KEY* curr_key_info = &table_share->key_info[curr_index];
        for (uint i = 0; i < curr_key_info->key_parts; i++) {
            uint16 curr_field_index = curr_key_info->key_part[i].field->field_index;
            if (!bitmap_is_set(&kc_info->key_filters[curr_index],curr_field_index)) {
                ret_val = false;
                goto exit;
            }
            if (bitmap_is_set(&kc_info->key_filters[curr_index], curr_field_index) &&
                !bitmap_is_set(&kc_info->key_filters[pk], curr_field_index)) {
                ret_val = false;
                goto exit;
            }
            
        }
    }
exit:
    return ret_val;
}

int ha_tokudb::initialize_share(
    const char* name,
    int mode
    )
{
    int error = 0;
    uint64_t num_rows = 0;
    bool table_exists;
    DB_TXN* txn = NULL;
    bool do_commit = false;
    THD* thd = ha_thd();
    tokudb_trx_data *trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(ha_thd(), tokudb_hton->slot);
    if (thd_sql_command(thd) == SQLCOM_CREATE_TABLE && trx && trx->sub_sp_level) {
        txn = trx->sub_sp_level;
    }
    else {
        do_commit = true;
        error = db_env->txn_begin(db_env, 0, &txn, 0);
        if (error) { goto exit; }
    }

    DBUG_PRINT("info", ("share->use_count %u", share->use_count));

    table_exists = true;
    error = check_table_in_metadata(name, &table_exists, txn);

    if (error) {
        goto exit;
    }
    if (!table_exists) {
        sql_print_error("table %s does not exist in metadata, was it moved from someplace else? Not opening table", name);
        error = HA_ADMIN_FAILED;
        goto exit;
    }

    error = get_status(txn);
    if (error) {
        goto exit;
    }
    if (share->version != HA_TOKU_VERSION) {
        error = ENOSYS;
        goto exit;
    }

    if (TOKU_PARTITION_WRITE_FRM_DATA) {
        // verify frm data for all tables
        error = verify_frm_data(table->s->path.str, txn);
        if (error)
            goto exit;
    } else {
        // verify frm data for non-partitioned tables
        if (table->part_info == NULL) {
            error = verify_frm_data(table->s->path.str, txn);
            if (error)
                goto exit;
        } else {
            // remove the frm data for partitions since we are not maintaining it
            error = remove_frm_data(share->status_block, txn);
            if (error)
                goto exit;
        }
    }

    error = initialize_key_and_col_info(
        table_share,
        table, 
        &share->kc_info,
        hidden_primary_key,
        primary_key
        );
    if (error) { goto exit; }
    
    error = open_main_dictionary(name, mode == O_RDONLY, txn);
    if (error) { goto exit; }

    share->has_unique_keys = false;
    /* Open other keys;  These are part of the share structure */
    for (uint i = 0; i < table_share->keys; i++) {
        if (table_share->key_info[i].flags & HA_NOSAME) {
            share->has_unique_keys = true;
        }
        if (i != primary_key) {
            error = open_secondary_dictionary(
                &share->key_file[i],
                &table_share->key_info[i],
                name,
                mode == O_RDONLY,
                txn
                );
            if (error) {
                goto exit;
            }
        }
    }
    share->replace_into_fast = can_replace_into_be_fast(
        table_share, 
        &share->kc_info, 
        primary_key
        );
        
    if (!hidden_primary_key) {
        //
        // We need to set the ref_length to start at 5, to account for
        // the "infinity byte" in keys, and for placing the DBT size in the first four bytes
        //
        ref_length = sizeof(uint32_t) + sizeof(uchar);
        KEY_PART_INFO *key_part = table->key_info[primary_key].key_part;
        KEY_PART_INFO *end = key_part + table->key_info[primary_key].key_parts;
        for (; key_part != end; key_part++) {
            ref_length += key_part->field->max_packed_col_length(key_part->length);
        }
        share->status |= STATUS_PRIMARY_KEY_INIT;
    }
    share->ref_length = ref_length;

    error = estimate_num_rows(share->file,&num_rows, txn);
    //
    // estimate_num_rows should not fail under normal conditions
    //
    if (error == 0) {
        share->rows = num_rows;
    }
    else {
        goto exit;
    }
    //
    // initialize auto increment data
    //
    share->has_auto_inc = has_auto_increment_flag(&share->ai_field_index);
    if (share->has_auto_inc) {
        init_auto_increment();
    }

    if (may_table_be_empty(NULL)) {
        share->try_table_lock = true;
    }
    else {
        share->try_table_lock = false;
    }

    share->num_DBs = table_share->keys + test(hidden_primary_key);

    error = 0;
exit:
    if (do_commit && txn) {
        commit_txn(txn,0);
    }
    return error;
}



//
// Creates and opens a handle to a table which already exists in a tokudb
// database.
// Parameters:
//      [in]   name - table name
//             mode - seems to specify if table is read only
//             test_if_locked - unused
// Returns:
//      0 on success
//      1 on error
//
int ha_tokudb::open(const char *name, int mode, uint test_if_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::open %p %s", this, name);
    THD* thd = ha_thd();

    int error = 0;
    int ret_val = 0;
    uint curr_num_DBs = 0;

    transaction = NULL;
    cursor = NULL;


    /* Open primary key */
    hidden_primary_key = 0;
    if ((primary_key = table_share->primary_key) >= MAX_KEY) {
        // No primary key
        primary_key = table_share->keys;
        key_used_on_scan = MAX_KEY;
        hidden_primary_key = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        ref_length = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH + sizeof(uint32_t);
    } 
    else {
        key_used_on_scan = primary_key;
    }
    curr_num_DBs = table_share->keys + test(hidden_primary_key);

    /* Need some extra memory in case of packed keys */
    // the "+ 1" is for the first byte that states +/- infinity
    // multiply everything by 2 to account for clustered keys having a key and primary key together
    max_key_length = 2*(table_share->max_key_length + MAX_REF_PARTS * 3 + sizeof(uchar));
    alloc_ptr = my_multi_malloc(MYF(MY_WME),
        &key_buff, max_key_length, 
        &key_buff2, max_key_length, 
        &key_buff3, max_key_length, 
        &prelocked_left_range, max_key_length, 
        &prelocked_right_range, max_key_length, 
        &primary_key_buff, (hidden_primary_key ? 0 : max_key_length),
        &fixed_cols_for_query, table_share->fields*sizeof(uint32_t),
        &var_cols_for_query, table_share->fields*sizeof(uint32_t),
        NullS
        );
    if (alloc_ptr == NULL) {
        ret_val = 1;
        goto exit;
    }

    size_range_query_buff = get_tokudb_read_buf_size(thd);
    range_query_buff = (uchar *)my_malloc(size_range_query_buff, MYF(MY_WME));
    if (range_query_buff == NULL) {
        ret_val = 1;
        goto exit;
    }

    alloced_rec_buff_length = table_share->rec_buff_length + table_share->fields;
    rec_buff = (uchar *) my_malloc(alloced_rec_buff_length, MYF(MY_WME));
    if (rec_buff == NULL) {
        ret_val = 1;
        goto exit;
    }

    alloced_update_rec_buff_length = alloced_rec_buff_length;
    rec_update_buff = (uchar *) my_malloc(alloced_update_rec_buff_length, MYF(MY_WME));
    if (rec_update_buff == NULL) {
        ret_val = 1;
        goto exit;
    }

    for (uint32_t i = 0; i < sizeof(mult_key_dbt)/sizeof(mult_key_dbt[0]); i++) {
        mult_key_dbt[i].flags = DB_DBT_REALLOC;
    }

    for (uint32_t i = 0; i < curr_num_DBs; i++) {
        mult_rec_dbt[i].flags = DB_DBT_REALLOC;
    }

    /* Init shared structure */
    share = get_share(name, table_share);
    if (share == NULL) {
        ret_val = 1;
        goto exit;
    }

    thr_lock_data_init(&share->lock, &lock, NULL);

    /* Fill in shared structure, if needed */
    pthread_mutex_lock(&share->mutex);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("tokudbopen:%p:share=%p:file=%p:table=%p:table->s=%p:%d\n", 
                     this, share, share->file, table, table->s, share->use_count);
    }
    if (!share->use_count++) {
        ret_val = initialize_share(
            name,
            mode
            );
        if (ret_val) {
            free_share(share, 1);
            goto exit;
        }
    }
    ref_length = share->ref_length;     // If second open
    pthread_mutex_unlock(&share->mutex);

    key_read = false;
    stats.block_size = 1<<20;    // QQQ Tokudb DB block size

    init_hidden_prim_key_info();

    info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

exit:
    if (ret_val) {
        my_free(range_query_buff, MYF(MY_ALLOW_ZERO_PTR));
        range_query_buff = NULL;
        my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
        alloc_ptr = NULL;
        my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
        rec_buff = NULL;
        my_free(rec_update_buff, MYF(MY_ALLOW_ZERO_PTR));
        rec_update_buff = NULL;
        
        if (error) {
            my_errno = error;
        }
    }
    TOKUDB_DBUG_RETURN(ret_val);
}

//
// estimate the number of rows in a DB
// Parameters:
//      [in]    db - DB whose number of rows will be estimated
//      [out]   num_rows - number of estimated rows in db
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::estimate_num_rows(DB* db, uint64_t* num_rows, DB_TXN* txn) {
    int error = ENOSYS;
    DBC* crsr = NULL;
    bool do_commit = false;
    DB_BTREE_STAT64 dict_stats;
    DB_TXN* txn_to_use = NULL;

    if (txn == NULL) {
        error = db_env->txn_begin(db_env, 0, &txn_to_use, DB_READ_UNCOMMITTED);
        if (error) goto cleanup;
        do_commit = true;
    }
    else {
        txn_to_use = txn;
    }

    error = db->stat64(
        share->file, 
        txn_to_use, 
        &dict_stats
        );
    if (error) { goto cleanup; }

    *num_rows = dict_stats.bt_ndata;
    error = 0;
cleanup:
    if (crsr != NULL) {
        int r = crsr->c_close(crsr);
        assert(r==0);
        crsr = NULL;
    }
    if (do_commit) {
        commit_txn(txn_to_use, 0);
        txn_to_use = NULL;
    }
    return error;
}


int ha_tokudb::write_to_status(DB* db, HA_METADATA_KEY curr_key_data, void* data, uint size, DB_TXN* txn ){
    return write_metadata(db, &curr_key_data, sizeof curr_key_data, data, size, txn);
}

int ha_tokudb::remove_from_status(DB *db, HA_METADATA_KEY curr_key_data, DB_TXN *txn) {
    return remove_metadata(db, &curr_key_data, sizeof curr_key_data, txn);
}

int ha_tokudb::remove_metadata(DB* db, void* key_data, uint key_size, DB_TXN* transaction){
    int error;
    DBT key;
    DB_TXN* txn = NULL;
    bool do_commit = false;
    //
    // transaction to be used for putting metadata into status.tokudb
    //
    if (transaction == NULL) {
        error = db_env->txn_begin(db_env, 0, &txn, 0);
        if (error) { 
            goto cleanup;
        }
        do_commit = true;
    }
    else {
        txn = transaction;
    }

    memset(&key, 0, sizeof(key));
    key.data = key_data;
    key.size = key_size;
    error = db->del(db, txn, &key, DB_DELETE_ANY);
    if (error) { 
        goto cleanup; 
    }
    
    error = 0;
cleanup:
    if (do_commit && txn) {
        if (!error) {
            commit_txn(txn, DB_TXN_NOSYNC);
        }
        else {
            abort_txn(txn);
        }
    }
    return error;
}

//
// helper function to write a piece of metadata in to status.tokudb
//
int ha_tokudb::write_metadata(DB* db, void* key_data, uint key_size, void* val_data, uint val_size, DB_TXN* transaction ){
    int error;
    DBT key;
    DBT value;
    DB_TXN* txn = NULL;
    bool do_commit = false;
    //
    // transaction to be used for putting metadata into status.tokudb
    //
    if (transaction == NULL) {
        error = db_env->txn_begin(db_env, 0, &txn, 0);
        if (error) { 
            goto cleanup;
        }
        do_commit = true;
    }
    else {
        txn = transaction;
    }

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    key.data = key_data;
    key.size = key_size;
    value.data = val_data;
    value.size = val_size;
    error = db->put(db, txn, &key, &value, 0);
    if (error) { 
        goto cleanup; 
    }
    
    error = 0;
cleanup:
    if (do_commit && txn) {
        if (!error) {
            commit_txn(txn, DB_TXN_NOSYNC);
        }
        else {
            abort_txn(txn);
        }
    }
    return error;
}

int ha_tokudb::write_frm_data(DB* db, DB_TXN* txn, const char* frm_name) {
    TOKUDB_DBUG_ENTER("ha_tokudb::write_frm_data %p %p %p %s", this, db, txn, frm_name);

    uchar* frm_data = NULL;
    size_t frm_len = 0;
    int error = 0;

    error = readfrm(frm_name,&frm_data,&frm_len);
    if (error) { goto cleanup; }
    
    error = write_to_status(db,hatoku_frm_data,frm_data,(uint)frm_len, txn);
    if (error) { goto cleanup; }

    error = 0;
cleanup:
    my_free(frm_data, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
}

int ha_tokudb::remove_frm_data(DB *db, DB_TXN *txn) {
    return remove_from_status(db, hatoku_frm_data, txn);
}

static int
smart_dbt_callback_verify_frm (DBT const *key, DBT  const *row, void *context) {
    DBT* stored_frm = (DBT *)context;
    stored_frm->size = row->size;
    stored_frm->data = (uchar *)my_malloc(row->size, MYF(MY_WME));
    assert(stored_frm->data);
    memcpy(stored_frm->data, row->data, row->size);
    return 0;
}

int ha_tokudb::verify_frm_data(const char* frm_name, DB_TXN* txn) {
    TOKUDB_DBUG_ENTER("ha_tokudb::verify_frm_data %s", frm_name);
    uchar* mysql_frm_data = NULL;
    size_t mysql_frm_len = 0;
    DBT key, stored_frm;
    int error = 0;
    HA_METADATA_KEY curr_key = hatoku_frm_data;

    memset(&key, 0, sizeof(key));
    memset(&stored_frm, 0, sizeof(&stored_frm));
    // get the frm data from MySQL
    error = readfrm(frm_name,&mysql_frm_data,&mysql_frm_len);
    if (error) { goto cleanup; }

    key.data = &curr_key;
    key.size = sizeof(curr_key);
    error = share->status_block->getf_set(
        share->status_block, 
        txn,
        0,
        &key, 
        smart_dbt_callback_verify_frm, 
        &stored_frm
        );
    if (error == DB_NOTFOUND) {
        // if not found, write it
        error = write_frm_data(
            share->status_block,
            txn,
            frm_name
            );
        goto cleanup;
    }
    else if (error) {
        goto cleanup;
    }

    if (stored_frm.size != mysql_frm_len || 
        memcmp(stored_frm.data, mysql_frm_data, stored_frm.size))
    {
        error = HA_ERR_TABLE_DEF_CHANGED;
        goto cleanup;
    }

    error = 0;
cleanup:
    my_free(mysql_frm_data, MYF(MY_ALLOW_ZERO_PTR));
    my_free(stored_frm.data, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
}

//
// Updates status.tokudb with a new max value used for the auto increment column
// Parameters:
//      [in]    db - this will always be status.tokudb
//              val - value to store
//  Returns:
//      0 on success, error otherwise
//
//
int ha_tokudb::update_max_auto_inc(DB* db, ulonglong val){
    return write_to_status(db,hatoku_max_ai,&val,sizeof(val), NULL);
}

//
// Writes the initial auto increment value, as specified by create table
// so if a user does "create table t1 (a int auto_increment, primary key (a)) auto_increment=100",
// then the value 100 will be stored here in val
// Parameters:
//      [in]    db - this will always be status.tokudb
//              val - value to store
//  Returns:
//      0 on success, error otherwise
//
//
int ha_tokudb::write_auto_inc_create(DB* db, ulonglong val, DB_TXN* txn){
    return write_to_status(db,hatoku_ai_create_value,&val,sizeof(val), txn);
}


//
// Closes a handle to a table. 
//
int ha_tokudb::close(void) {
    TOKUDB_DBUG_ENTER("ha_tokudb::close %p", this);
    TOKUDB_DBUG_RETURN(__close(0));
}

int ha_tokudb::__close(int mutex_is_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::__close %p", this);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) 
        TOKUDB_TRACE("close:%p\n", this);
    my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(rec_update_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(blob_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
    my_free(range_query_buff, MYF(MY_ALLOW_ZERO_PTR));
    for (uint32_t i = 0; i < sizeof(mult_rec_dbt)/sizeof(mult_rec_dbt[0]); i++) {
        if (mult_rec_dbt[i].flags == DB_DBT_REALLOC &&
                mult_rec_dbt[i].data != NULL) {
            free(mult_rec_dbt[i].data);
        }
    }
    for (uint32_t i = 0; i < sizeof(mult_key_dbt)/sizeof(mult_key_dbt[0]); i++) {
        if (mult_key_dbt[i].flags == DB_DBT_REALLOC &&
                mult_key_dbt[i].data != NULL) {
            free(mult_key_dbt[i].data);
        }
    }
    rec_buff = NULL;
    rec_update_buff = NULL;
    alloc_ptr = NULL;
    ha_tokudb::reset();
    TOKUDB_DBUG_RETURN(free_share(share, mutex_is_locked));
}

//
// Reallocate record buffer (rec_buff) if needed
// If not needed, does nothing
// Parameters:
//          length - size of buffer required for rec_buff
//
bool ha_tokudb::fix_rec_buff_for_blob(ulong length) {
    if (!rec_buff || (length > alloced_rec_buff_length)) {
        uchar *newptr;
        if (!(newptr = (uchar *) my_realloc((void *) rec_buff, length, MYF(MY_ALLOW_ZERO_PTR))))
            return 1;
        rec_buff = newptr;
        alloced_rec_buff_length = length;
    }
    return 0;
}

//
// Reallocate record buffer (rec_buff) if needed
// If not needed, does nothing
// Parameters:
//          length - size of buffer required for rec_buff
//
bool ha_tokudb::fix_rec_update_buff_for_blob(ulong length) {
    if (!rec_update_buff || (length > alloced_update_rec_buff_length)) {
        uchar *newptr;
        if (!(newptr = (uchar *) my_realloc((void *) rec_update_buff, length, MYF(MY_ALLOW_ZERO_PTR))))
            return 1;
        rec_update_buff= newptr;
        alloced_update_rec_buff_length = length;
    }
    return 0;
}

/* Calculate max length needed for row */
ulong ha_tokudb::max_row_length(const uchar * buf) {
    ulong length = table_share->reclength + table_share->fields * 2;
    uint *ptr, *end;
    for (ptr = table_share->blob_field, end = ptr + table_share->blob_fields; ptr != end; ptr++) {
        Field_blob *blob = ((Field_blob *) table->field[*ptr]);
        length += blob->get_length((uchar *) (buf + field_offset(blob, table))) + 2;
    }
    return length;
}

/*
*/
//
// take the row passed in as a DBT*, and convert it into a row in MySQL format in record
// Pack a row for storage.
// If the row is of fixed length, just store the  row 'as is'.
// If not, we will generate a packed row suitable for storage.
// This will only fail if we don't have enough memory to pack the row,
// which may only happen in rows with blobs, as the default row length is
// pre-allocated.
// Parameters:
//      [out]   row - row stored in DBT to be converted
//      [out]   buf - buffer where row is packed
//      [in]    record - row in MySQL format
//

int ha_tokudb::pack_row_in_buff(
    DBT * row, 
    const uchar* record,
    uint index,
    uchar* row_buff
    ) 
{
    uchar* fixed_field_ptr = NULL;
    uchar* var_field_offset_ptr = NULL;
    uchar* start_field_data_ptr = NULL;
    uchar* var_field_data_ptr = NULL;
    int r = ENOSYS;
    memset((void *) row, 0, sizeof(*row));

    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
    
    // Copy null bytes
    memcpy(row_buff, record, table_share->null_bytes);
    fixed_field_ptr = row_buff + table_share->null_bytes;
    var_field_offset_ptr = fixed_field_ptr + share->kc_info.mcp_info[index].fixed_field_size;
    start_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;
    var_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;

    // assert that when the hidden primary key exists, primary_key_offsets is NULL
    for (uint i = 0; i < table_share->fields; i++) {
        Field* field = table->field[i];
        uint curr_field_offset = field_offset(field, table);
        if (bitmap_is_set(&share->kc_info.key_filters[index],i)) {
            continue;
        }
        if (share->kc_info.field_lengths[i]) {
            fixed_field_ptr = pack_fixed_field(
                fixed_field_ptr,
                record + curr_field_offset, 
                share->kc_info.field_lengths[i]
                );
        }
        else if (share->kc_info.length_bytes[i]) {
            var_field_data_ptr = pack_var_field(
                var_field_offset_ptr,
                var_field_data_ptr,
                start_field_data_ptr,
                record + curr_field_offset,
                share->kc_info.length_bytes[i],
                share->kc_info.num_offset_bytes
                );
            var_field_offset_ptr += share->kc_info.num_offset_bytes;
        }
    }

    for (uint i = 0; i < share->kc_info.num_blobs; i++) {
        Field* field = table->field[share->kc_info.blob_fields[i]];
        var_field_data_ptr = pack_toku_field_blob(
            var_field_data_ptr,
            record + field_offset(field, table),
            field
            );
    }

    row->data = row_buff;
    row->size = (size_t) (var_field_data_ptr - row_buff);
    r = 0;

    dbug_tmp_restore_column_map(table->write_set, old_map);
    return r;
}


int ha_tokudb::pack_row(
    DBT * row, 
    const uchar* record,
    uint index
    )
{
    return pack_row_in_buff(row,record,index,rec_buff);
}

int ha_tokudb::pack_old_row_for_update(
    DBT * row, 
    const uchar* record,
    uint index
    )
{
    return pack_row_in_buff(row,record,index,rec_update_buff);
}


int ha_tokudb::unpack_blobs(
    uchar* record,
    const uchar* from_tokudb_blob,
    uint32_t num_bytes,
    bool check_bitmap
    )
{
    uint error = 0;
    uchar* ptr = NULL;
    const uchar* buff = NULL;
    //
    // assert that num_bytes > 0 iff share->num_blobs > 0
    //
    assert( !((share->kc_info.num_blobs == 0) && (num_bytes > 0)) );
    if (num_bytes > num_blob_bytes) {
        ptr = (uchar *)my_realloc((void *)blob_buff, num_bytes, MYF(MY_ALLOW_ZERO_PTR));
        if (ptr == NULL) {
            error = ENOMEM;
            goto exit;
        }
        blob_buff = ptr;
        num_blob_bytes = num_bytes;
    }
    
    memcpy(blob_buff, from_tokudb_blob, num_bytes);
    buff= blob_buff;
    for (uint i = 0; i < share->kc_info.num_blobs; i++) {
        uint32_t curr_field_index = share->kc_info.blob_fields[i]; 
        bool skip = check_bitmap ? 
            !(bitmap_is_set(table->read_set,curr_field_index) || 
                bitmap_is_set(table->write_set,curr_field_index)) : 
            false;
        Field* field = table->field[curr_field_index];
        uint32_t len_bytes = field->row_pack_length();
        buff = unpack_toku_field_blob(
            record + field_offset(field, table),
            buff,
            len_bytes,
            skip
            );
    }

    error = 0;
exit:
    return error;
}

//
// take the row passed in as a DBT*, and convert it into a row in MySQL format in record
// Parameters:
//      [out]   record - row in MySQL format
//      [in]    row - row stored in DBT to be converted
//
int ha_tokudb::unpack_row(
    uchar* record, 
    DBT const *row, 
    DBT const *key,
    uint index
    ) 
{
    //
    // two cases, fixed length row, and variable length row
    // fixed length row is first below
    //
    /* Copy null bits */
    int error = 0;
    const uchar* fixed_field_ptr = (const uchar *) row->data;
    const uchar* var_field_offset_ptr = NULL;
    const uchar* var_field_data_ptr = NULL;
    uint32_t data_end_offset = 0;
    memcpy(record, fixed_field_ptr, table_share->null_bytes);
    fixed_field_ptr += table_share->null_bytes;

    var_field_offset_ptr = fixed_field_ptr + share->kc_info.mcp_info[index].fixed_field_size;
    var_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;

    //
    // unpack the key, if necessary
    //
    if (!(hidden_primary_key && index == primary_key)) {
        unpack_key(record,key,index);
    }

    uint32_t last_offset = 0;
    //
    // we have two methods of unpacking, one if we need to unpack the entire row
    // the second if we unpack a subset of the entire row
    // first method here is if we unpack the entire row
    //
    if (unpack_entire_row) {
        //
        // fill in parts of record that are not part of the key
        //
        for (uint i = 0; i < table_share->fields; i++) {
            Field* field = table->field[i];
            if (bitmap_is_set(&share->kc_info.key_filters[index],i)) {
                continue;
            }

            if (share->kc_info.field_lengths[i]) {
                fixed_field_ptr = unpack_fixed_field(
                    record + field_offset(field, table),
                    fixed_field_ptr,
                    share->kc_info.field_lengths[i]
                    );
            }
            //
            // here, we DO modify var_field_data_ptr or var_field_offset_ptr
            // as we unpack variable sized fields
            //
            else if (share->kc_info.length_bytes[i]) {
                switch (share->kc_info.num_offset_bytes) {
                case (1):
                    data_end_offset = var_field_offset_ptr[0];
                    break;
                case (2):
                    data_end_offset = uint2korr(var_field_offset_ptr);
                    break;
                default:
                    assert(false);
                    break;
                }
                unpack_var_field(
                    record + field_offset(field, table),
                    var_field_data_ptr,
                    data_end_offset - last_offset,
                    share->kc_info.length_bytes[i]
                    );
                var_field_offset_ptr += share->kc_info.num_offset_bytes;
                var_field_data_ptr += data_end_offset - last_offset;
                last_offset = data_end_offset;
            }
        }
        error = unpack_blobs(
            record,
            var_field_data_ptr,
            row->size - (uint32_t)(var_field_data_ptr - (const uchar *)row->data),
            false
            );
        if (error) {
            goto exit;
        }
    }
    //
    // in this case, we unpack only what is specified 
    // in fixed_cols_for_query and var_cols_for_query
    //
    else {
        //
        // first the fixed fields
        //
        for (uint32_t i = 0; i < num_fixed_cols_for_query; i++) {
            uint field_index = fixed_cols_for_query[i];
            Field* field = table->field[field_index];
            unpack_fixed_field(
                record + field_offset(field, table),
                fixed_field_ptr + share->kc_info.cp_info[index][field_index].col_pack_val,
                share->kc_info.field_lengths[field_index]
                );
        }

        //
        // now the var fields
        // here, we do NOT modify var_field_data_ptr or var_field_offset_ptr
        //
        for (uint32_t i = 0; i < num_var_cols_for_query; i++) {
            uint field_index = var_cols_for_query[i];
            Field* field = table->field[field_index];
            uint32_t var_field_index = share->kc_info.cp_info[index][field_index].col_pack_val;
            uint32_t data_start_offset;
            uint32_t field_len;
            
            get_var_field_info(
                &field_len, 
                &data_start_offset, 
                var_field_index, 
                var_field_offset_ptr, 
                share->kc_info.num_offset_bytes
                );

            unpack_var_field(
                record + field_offset(field, table),
                var_field_data_ptr + data_start_offset,
                field_len,
                share->kc_info.length_bytes[field_index]
                );
        }

        if (read_blobs) {
            //
            // now the blobs
            //
            get_blob_field_info(
                &data_end_offset, 
                share->kc_info.mcp_info[index].len_of_offsets,
                var_field_data_ptr, 
                share->kc_info.num_offset_bytes
                );

            var_field_data_ptr += data_end_offset;
            error = unpack_blobs(
                record,
                var_field_data_ptr,
                row->size - (uint32_t)(var_field_data_ptr - (const uchar *)row->data),
                true
                );
            if (error) {
                goto exit;
            }
        }
    }
    error = 0;
exit:
    return error;
}

uint32_t ha_tokudb::place_key_into_mysql_buff(
    KEY* key_info, 
    uchar * record, 
    uchar* data
    ) 
{
    KEY_PART_INFO *key_part = key_info->key_part, *end = key_part + key_info->key_parts;
    uchar *pos = data;

    for (; key_part != end; key_part++) {
        if (key_part->field->null_bit) {
            uint null_offset = get_null_offset(table, key_part->field);
            if (*pos++ == NULL_COL_VAL) { // Null value
                //
                // We don't need to reset the record data as we will not access it
                // if the null data is set
                //            
                record[null_offset] |= key_part->field->null_bit;
                continue;
            }
            record[null_offset] &= ~key_part->field->null_bit;
        }
#if !defined(MARIADB_BASE_VERSION)
        //
        // HOPEFULLY TEMPORARY
        //
        assert(table->s->db_low_byte_first);
#endif
        pos = unpack_toku_key_field(
            record + field_offset(key_part->field, table),
            pos,
            key_part->field,
            key_part->length
            );
    }
    return pos-data;
}

//
// Store the key and the primary key into the row
// Parameters:
//      [out]   record - key stored in MySQL format
//      [in]    key - key stored in DBT to be converted
//              index -index into key_file that represents the DB 
//                  unpacking a key of
//
void ha_tokudb::unpack_key(uchar * record, DBT const *key, uint index) {
    uint32_t bytes_read;
    uchar *pos = (uchar *) key->data + 1;
    bytes_read = place_key_into_mysql_buff(
        &table->key_info[index], 
        record, 
        pos
        );
    if( (index != primary_key) && !hidden_primary_key) {
        //
        // also unpack primary key
        //
        place_key_into_mysql_buff(
            &table->key_info[primary_key], 
            record, 
            pos+bytes_read
            );
    }
}

uint32_t ha_tokudb::place_key_into_dbt_buff(
    KEY* key_info, 
    uchar * buff, 
    const uchar * record, 
    bool* has_null, 
    int key_length
    ) 
{
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;
    uchar* curr_buff = buff;
    *has_null = false;
    for (; key_part != end && key_length > 0; key_part++) {
        //
        // accessing key_part->field->null_bit instead off key_part->null_bit
        // because key_part->null_bit is not set in add_index
        // filed ticket 862 to look into this
        //
        if (key_part->field->null_bit) {
            /* Store 0 if the key part is a NULL part */
            uint null_offset = get_null_offset(table, key_part->field);
            if (record[null_offset] & key_part->field->null_bit) {
                *curr_buff++ = NULL_COL_VAL;
                *has_null = true;
                continue;
            }
            *curr_buff++ = NONNULL_COL_VAL;        // Store NOT NULL marker
        }
#if !defined(MARIADB_BASE_VERSION)
        //
        // HOPEFULLY TEMPORARY
        //
        assert(table->s->db_low_byte_first);
#endif
        //
        // accessing field_offset(key_part->field) instead off key_part->offset
        // because key_part->offset is SET INCORRECTLY in add_index
        // filed ticket 862 to look into this
        //
        curr_buff = pack_toku_key_field(
            curr_buff,
            (uchar *) (record + field_offset(key_part->field, table)),
            key_part->field,
            key_part->length
            );
        key_length -= key_part->length;
    }
    return curr_buff - buff;
}



//
// Create a packed key from a row. This key will be written as such
// to the index tree.  This will never fail as the key buffer is pre-allocated.
// Parameters:
//      [out]   key - DBT that holds the key
//      [in]    key_info - holds data about the key, such as it's length and offset into record
//      [out]   buff - buffer that will hold the data for key (unless 
//                  we have a hidden primary key)
//      [in]    record - row from which to create the key
//              key_length - currently set to MAX_KEY_LENGTH, is it size of buff?
// Returns:
//      the parameter key
//

DBT* ha_tokudb::create_dbt_key_from_key(
    DBT * key,
    KEY* key_info, 
    uchar * buff,
    const uchar * record, 
    bool* has_null,
    bool dont_pack_pk,
    int key_length
    ) 
{
    uint32_t size = 0;
    uchar* tmp_buff = buff;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    key->data = buff;

    //
    // first put the "infinity" byte at beginning. States if missing columns are implicitly
    // positive infinity or negative infinity or zero. For this, because we are creating key
    // from a row, there is no way that columns can be missing, so in practice,
    // this will be meaningless. Might as well put in a value
    //
    *tmp_buff++ = COL_ZERO;
    size++;
    size += place_key_into_dbt_buff(
        key_info, 
        tmp_buff, 
        record, 
        has_null, 
        key_length
        );
    if (!dont_pack_pk) {
        tmp_buff = buff + size;
        if (hidden_primary_key) {
            memcpy_fixed(tmp_buff, current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
            size += TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        }
        else {
            bool tmp_bool = false;
            size += place_key_into_dbt_buff(
                &table->key_info[primary_key], 
                tmp_buff, 
                record, 
                &tmp_bool, 
                MAX_KEY_LENGTH //this parameter does not matter
                );
        }
    }

    key->size = size;
    DBUG_DUMP("key", (uchar *) key->data, key->size);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    return key;
}


//
// Create a packed key from a row. This key will be written as such
// to the index tree.  This will never fail as the key buffer is pre-allocated.
// Parameters:
//      [out]   key - DBT that holds the key
//              keynr - index for which to create the key
//      [out]   buff - buffer that will hold the data for key (unless 
//                  we have a hidden primary key)
//      [in]    record - row from which to create the key
//      [out]   has_null - says if the key has a NULL value for one of its columns
//              key_length - currently set to MAX_KEY_LENGTH, is it size of buff?
// Returns:
//      the parameter key
//
DBT *ha_tokudb::create_dbt_key_from_table(
    DBT * key, 
    uint keynr, 
    uchar * buff, 
    const uchar * record, 
    bool* has_null, 
    int key_length
    ) 
{
    TOKUDB_DBUG_ENTER("ha_tokudb::create_dbt_key_from_table");
    memset((void *) key, 0, sizeof(*key));
    if (hidden_primary_key && keynr == primary_key) {
        key->data = buff;
        memcpy(buff, &current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        key->size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        *has_null = false;
        DBUG_RETURN(key);
    }
    DBUG_RETURN(create_dbt_key_from_key(key, &table->key_info[keynr],buff,record, has_null, (keynr == primary_key), key_length));
}

DBT* ha_tokudb::create_dbt_key_for_lookup(
    DBT * key, 
    KEY* key_info, 
    uchar * buff, 
    const uchar * record, 
    bool* has_null, 
    int key_length
    )
{
    TOKUDB_DBUG_ENTER("ha_tokudb::create_dbt_key_from_lookup");
    DBUG_RETURN(create_dbt_key_from_key(key, key_info, buff, record, has_null, true, key_length));    
}

//
// Create a packed key from from a MySQL unpacked key (like the one that is
// sent from the index_read() This key is to be used to read a row
// Parameters:
//      [out]   key - DBT that holds the key
//              keynr - index for which to pack the key
//      [out]   buff - buffer that will hold the data for key
//      [in]    key_ptr - MySQL unpacked key
//              key_length - length of key_ptr
// Returns:
//      the parameter key
//
DBT *ha_tokudb::pack_key(
    DBT * key, 
    uint keynr, 
    uchar * buff, 
    const uchar * key_ptr, 
    uint key_length, 
    int8_t inf_byte
    ) 
{
    TOKUDB_DBUG_ENTER("ha_tokudb::pack_key");
    KEY *key_info = &table->key_info[keynr];
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    memset((void *) key, 0, sizeof(*key));
    key->data = buff;

    //
    // first put the "infinity" byte at beginning. States if missing columns are implicitly
    // positive infinity or negative infinity
    //
    *buff++ = (uchar)inf_byte;

    for (; key_part != end && (int) key_length > 0; key_part++) {
        uint offset = 0;
        if (key_part->null_bit) {
            if (!(*key_ptr == 0)) {
                *buff++ = NULL_COL_VAL;
                key_length -= key_part->store_length;
                key_ptr += key_part->store_length;
                continue;
            }
            *buff++ = NONNULL_COL_VAL;
            offset = 1;         // Data is at key_ptr+1
        }
#if !defined(MARIADB_BASE_VERSION)
        assert(table->s->db_low_byte_first);
#endif
        buff = pack_key_toku_key_field(
            buff,
            (uchar *) key_ptr + offset,
            key_part->field,
            key_part->length
            );
        
        key_ptr += key_part->store_length;
        key_length -= key_part->store_length;
    }
    key->size = (buff - (uchar *) key->data);
    DBUG_DUMP("key", (uchar *) key->data, key->size);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    DBUG_RETURN(key);
}

//
// get max used hidden primary key value
//
void ha_tokudb::init_hidden_prim_key_info() {
    TOKUDB_DBUG_ENTER("ha_tokudb::init_prim_key_info");
    pthread_mutex_lock(&share->mutex);
    if (!(share->status & STATUS_PRIMARY_KEY_INIT)) {
        int error = 0;
        THD* thd = ha_thd();
        DB_TXN* txn = NULL;
        DBC* c = NULL;
        tokudb_trx_data *trx = NULL;
        trx = (tokudb_trx_data *) thd_data_get(ha_thd(), tokudb_hton->slot);
        bool do_commit = false;
        if (thd_sql_command(thd) == SQLCOM_CREATE_TABLE && trx && trx->sub_sp_level) {
            txn = trx->sub_sp_level;
        }
        else {
            do_commit = true;
            error = db_env->txn_begin(db_env, 0, &txn, 0);
            assert(error == 0);
        }
        
        error = share->key_file[primary_key]->cursor(
            share->key_file[primary_key],
            txn,
            &c,
            0
            );
        assert(error == 0);
        DBT key,val;        
        memset(&key, 0, sizeof(key));
        memset(&val, 0, sizeof(val));
        error = c->c_get(c, &key, &val, DB_LAST);
        if (error == 0) {
            assert(key.size == TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
            share->auto_ident = hpk_char_to_num((uchar *)key.data);
        }
        error = c->c_close(c);
        assert(error == 0);
        if (do_commit) {
            commit_txn(txn, 0);
        }
        share->status |= STATUS_PRIMARY_KEY_INIT;
    }
    pthread_mutex_unlock(&share->mutex);
    DBUG_VOID_RETURN;
}



/** @brief
    Get metadata info stored in status.tokudb
    */
int ha_tokudb::get_status(DB_TXN* txn) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_status");
    DBT key, value;
    HA_METADATA_KEY curr_key;
    int error;

    //
    // open status.tokudb
    //
    if (!share->status_block) {
        error = open_status_dictionary(
            &share->status_block, 
            share->table_name, 
            txn
            );
        if (error) { 
            goto cleanup; 
        }
    }
    
    //
    // transaction to be used for putting metadata into status.tokudb
    //
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    key.data = &curr_key;
    key.size = sizeof(curr_key);
    value.flags = DB_DBT_USERMEM;

    assert(share->status_block);
    //
    // get version
    //
    value.ulen = sizeof(share->version);
    value.data = &share->version;
    curr_key = hatoku_new_version;
    error = share->status_block->get(
        share->status_block, 
        txn, 
        &key, 
        &value, 
        0
        );
    if (error == DB_NOTFOUND) {
        //
        // hack to keep handle the issues of going back and forth
        // between 5.0.3 to 5.0.4
        // the problem with going back and forth
        // is with storing the frm file, 5.0.4 stores it, 5.0.3 does not
        // so, if a user goes back and forth and alters the schema
        // the frm stored can get out of sync with the schema of the table
        // This can cause issues.
        // To take care of this, we are doing this versioning work here.
        // We change the key that stores the version. 
        // In 5.0.3, it is hatoku_old_version, in 5.0.4 it is hatoku_new_version
        // When we encounter a table that does not have hatoku_new_version
        // set, we give it the right one, and overwrite the old one with zero.
        // This ensures that 5.0.3 cannot open the table. Once it has been opened by 5.0.4
        //
        uint dummy_version = 0;
        share->version = HA_TOKU_ORIG_VERSION;
        error = write_to_status(
            share->status_block, 
            hatoku_new_version,
            &share->version,
            sizeof(share->version), 
            txn
            );
        if (error) { goto cleanup; }
        error = write_to_status(
            share->status_block, 
            hatoku_old_version,
            &dummy_version,
            sizeof(dummy_version), 
            txn
            );
        if (error) { goto cleanup; }
    }
    else if (error || value.size != sizeof(share->version)) {
        if (error == 0) {
            error = HA_ERR_INTERNAL_ERROR;
        }
        goto cleanup;
    }
    //
    // get capabilities
    //
    curr_key = hatoku_capabilities;
    value.ulen = sizeof(share->capabilities);
    value.data = &share->capabilities;
    error = share->status_block->get(
        share->status_block, 
        txn, 
        &key, 
        &value, 
        0
        );
    if (error == DB_NOTFOUND) {
        share->capabilities= 0;
    }
    else if (error || value.size != sizeof(share->version)) {
        if (error == 0) {
            error = HA_ERR_INTERNAL_ERROR;
        }
        goto cleanup;
    }
    
    error = 0;
cleanup:
    TOKUDB_DBUG_RETURN(error);
}

/** @brief
    Return an estimated of the number of rows in the table.
    Used when sorting to allocate buffers and by the optimizer.
    This is used in filesort.cc. 
*/
ha_rows ha_tokudb::estimate_rows_upper_bound() {
    TOKUDB_DBUG_ENTER("ha_tokudb::estimate_rows_upper_bound");
    DBUG_RETURN(share->rows + HA_TOKUDB_EXTRA_ROWS);
}

//
// Function that compares two primary keys that were saved as part of rnd_pos
// and ::position
//
int ha_tokudb::cmp_ref(const uchar * ref1, const uchar * ref2) {
    int ret_val = 0;
    ret_val = tokudb_compare_two_keys(
        ref1 + sizeof(uint32_t),
        *(uint32_t *)ref1,
        ref2 + sizeof(uint32_t),
        *(uint32_t *)ref2,
        (uchar *)share->file->descriptor->dbt.data + 4,
        *(uint32_t *)share->file->descriptor->dbt.data - 4,
        false
        );
    return ret_val;
}

bool ha_tokudb::check_if_incompatible_data(HA_CREATE_INFO * info, uint table_changes) {
  //
  // This is a horrendous hack for now, as copied by InnoDB.
  // This states that if the auto increment create field has changed,
  // via a "alter table foo auto_increment=new_val", that this
  // change is incompatible, and to rebuild the entire table
  // This will need to be fixed
  //
  if ((info->used_fields & HA_CREATE_USED_AUTO) &&
      info->auto_increment_value != 0) {

    return COMPATIBLE_DATA_NO;
  }
  if (table_changes != IS_EQUAL_YES)
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

//
// Method that is called before the beginning of many calls
// to insert rows (ha_tokudb::write_row). There is no guarantee
// that start_bulk_insert is called, however there is a guarantee
// that if start_bulk_insert is called, then end_bulk_insert may be
// called as well.
// Parameters:
//      [in]    rows - an estimate of the number of rows that will be inserted
//                     if number of rows is unknown (such as if doing 
//                     "insert into foo select * from bar), then rows 
//                     will be 0
//
//
// This function returns true if the table MAY be empty.
// It is NOT meant to be a 100% check for emptiness.
// This is used for a bulk load optimization.
//
bool ha_tokudb::may_table_be_empty(DB_TXN *txn) {
    int error;
    bool ret_val = false;
    DBC* tmp_cursor = NULL;
    DB_TXN* tmp_txn = NULL;

    if (txn == NULL) {
        error = db_env->txn_begin(db_env, 0, &tmp_txn, 0);
        if (error) {
            goto cleanup;
        }
        txn = tmp_txn;
    }

    error = share->file->cursor(share->file, txn, &tmp_cursor, 0);
    if (error) {
        goto cleanup;
    }
    error = tmp_cursor->c_getf_next(tmp_cursor, 0, smart_dbt_do_nothing, NULL);
    if (error == DB_NOTFOUND) {
        ret_val = true;
    }
    else {
        ret_val = false;
    }
    error = 0;
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
        tmp_cursor = NULL;
    }
    if (tmp_txn) {
        commit_txn(tmp_txn, 0);
        tmp_txn = NULL;
    }
    return ret_val;
}

void ha_tokudb::start_bulk_insert(ha_rows rows) {
    TOKUDB_DBUG_ENTER("ha_tokudb::start_bulk_insert");
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    delay_updating_ai_metadata = true;
    ai_metadata_update_required = false;
    abort_loader = false;
    
    rw_rdlock(&share->num_DBs_lock);
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    num_DBs_locked_in_bulk = true;
    lock_count = 0;
    
    if (share->try_table_lock) {
        if (get_prelock_empty(thd) && may_table_be_empty(transaction)) {
            if (using_ignore || is_insert_ignore(thd) || thd->lex->duplicates != DUP_ERROR) {
                acquire_table_lock(transaction, lock_write);
            }
            else {
                mult_dbt_flags[primary_key] = 0;
                if (!thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS) && !hidden_primary_key) {
                    mult_put_flags[primary_key] = DB_NOOVERWRITE;
                }
                uint32_t loader_flags = (get_load_save_space(thd)) ? 
                    LOADER_USE_PUTS : 0;

                int error = db_env->create_loader(
                    db_env, 
                    transaction, 
                    &loader, 
                    NULL, // no src_db needed
                    curr_num_DBs, 
                    share->key_file, 
                    mult_put_flags,
                    mult_dbt_flags,
                    loader_flags
                    );
                if (error) { 
                    assert(loader == NULL);
                    goto exit_try_table_lock;
                }

                lc.thd = thd;
                lc.ha = this;
                
                error = loader->set_poll_function(loader, loader_poll_fun, &lc);
                assert(!error);

                error = loader->set_error_callback(loader, loader_dup_fun, &lc);
                assert(!error);

                trx->stmt_progress.using_loader = true;
            }
        }
    exit_try_table_lock:
        pthread_mutex_lock(&share->mutex);
        share->try_table_lock = false;
        pthread_mutex_unlock(&share->mutex);
    }
    DBUG_VOID_RETURN;
}

//
// Method that is called at the end of many calls to insert rows
// (ha_tokudb::write_row). If start_bulk_insert is called, then
// this is guaranteed to be called.
//
int ha_tokudb::end_bulk_insert(bool abort) {
    TOKUDB_DBUG_ENTER("ha_tokudb::end_bulk_insert");
    int error = 0;
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    bool using_loader = (loader != NULL);
    if (ai_metadata_update_required) {
        pthread_mutex_lock(&share->mutex);
        error = update_max_auto_inc(share->status_block, share->last_auto_increment);
        pthread_mutex_unlock(&share->mutex);
        if (error) { goto cleanup; }
    }
    delay_updating_ai_metadata = false;
    ai_metadata_update_required = false;
    loader_error = 0;
    if (loader) {
        if (!abort_loader && !thd->killed) {
            error = loader->close(loader);
            loader = NULL;
            if (error) { 
                if (thd->killed) {
                    my_error(ER_QUERY_INTERRUPTED, MYF(0));
                }
                goto cleanup; 
            }

            for (uint i = 0; i < table_share->keys; i++) {
                if (table_share->key_info[i].flags & HA_NOSAME) {
                    bool is_unique;
                    if (i == primary_key) {
                        continue;
                    }
                    error = is_index_unique(
                        &is_unique, 
                        transaction, 
                        share->key_file[i], 
                        &table->key_info[i]
                        );
                    if (error) goto cleanup;
                    if (!is_unique) {
                        error = HA_ERR_FOUND_DUPP_KEY;
                        last_dup_key = i;
                        goto cleanup;
                    }
                }
            }
        }
        else {
            error = sprintf(write_status_msg, "aborting bulk load"); 
            thd_proc_info(thd, write_status_msg);
            loader->abort(loader);
            loader = NULL;
            share->try_table_lock = true;
        }
    }

cleanup:
    if (num_DBs_locked_in_bulk) {
        rw_unlock(&share->num_DBs_lock);
    }
    num_DBs_locked_in_bulk = false;
    lock_count = 0;
    if (loader) {
        error = sprintf(write_status_msg, "aborting bulk load"); 
        thd_proc_info(thd, write_status_msg);
        loader->abort(loader);
        loader = NULL;
    }
    abort_loader = false;
    memset(&lc, 0, sizeof(lc));
    if (error || loader_error) {
        my_errno = error ? error : loader_error;
        if (using_loader) {
            //
            // TODO: (Zardosht), remove trx->should_abort as part of
            // #5481
            trx->should_abort = true;
            share->try_table_lock = true;
        }
    }
    trx->stmt_progress.using_loader = false;
    TOKUDB_DBUG_RETURN(error ? error : loader_error);
}

int ha_tokudb::end_bulk_insert() {
    return end_bulk_insert( false );
}

int ha_tokudb::is_index_unique(bool* is_unique, DB_TXN* txn, DB* db, KEY* key_info) {
    int error;
    DBC* tmp_cursor1 = NULL;
    DBC* tmp_cursor2 = NULL;
    DBT key1, key2, val, packed_key1, packed_key2;
    uint64_t cnt = 0;
    char status_msg[MAX_ALIAS_NAME + 200]; //buffer of 200 should be a good upper bound.
    THD* thd = ha_thd();
    memset(&key1, 0, sizeof(key1));
    memset(&key2, 0, sizeof(key2));
    memset(&val, 0, sizeof(val));
    memset(&packed_key1, 0, sizeof(packed_key1));
    memset(&packed_key2, 0, sizeof(packed_key2));
    *is_unique = true;
    
    error = db->cursor(
        db, 
        txn, 
        &tmp_cursor1, 
        DB_SERIALIZABLE
        );
    if (error) { goto cleanup; }

    error = db->cursor(
        db, 
        txn, 
        &tmp_cursor2,
        DB_SERIALIZABLE
        );
    if (error) { goto cleanup; }

    
    error = tmp_cursor1->c_get(
        tmp_cursor1, 
        &key1, 
        &val, 
        DB_NEXT
        );
    if (error == DB_NOTFOUND) {
        *is_unique = true;
        error = 0;
        goto cleanup;
    }
    else if (error) { goto cleanup; }
    error = tmp_cursor2->c_get(
        tmp_cursor2, 
        &key2, 
        &val, 
        DB_NEXT
        );
    if (error) { goto cleanup; }

    error = tmp_cursor2->c_get(
        tmp_cursor2, 
        &key2, 
        &val, 
        DB_NEXT
        );
    if (error == DB_NOTFOUND) {
        *is_unique = true;
        error = 0;
        goto cleanup;
    }
    else if (error) { goto cleanup; }

    while (error != DB_NOTFOUND) {
        bool has_null1;
        bool has_null2;
        int cmp;
        place_key_into_mysql_buff(
            key_info,
            table->record[0], 
            (uchar *) key1.data + 1
            );
        place_key_into_mysql_buff(
            key_info,
            table->record[1], 
            (uchar *) key2.data + 1
            );
        
        create_dbt_key_for_lookup(
            &packed_key1,
            key_info,
            key_buff,
            table->record[0],
            &has_null1
            );
        create_dbt_key_for_lookup(
            &packed_key2,
            key_info,
            key_buff2,
            table->record[1],
            &has_null2
            );

        if (!has_null1 && !has_null2) {
            cmp = tokudb_prefix_cmp_dbt_key(db, &packed_key1, &packed_key2);
            if (cmp == 0) {
                memcpy(key_buff, key1.data, key1.size);
                place_key_into_mysql_buff(
                    key_info,
                    table->record[0], 
                    (uchar *) key_buff + 1
                    );
                *is_unique = false;
                break;
            }
        }

        error = tmp_cursor1->c_get(
            tmp_cursor1, 
            &key1, 
            &val, 
            DB_NEXT
            );
        if (error) { goto cleanup; }
        error = tmp_cursor2->c_get(
            tmp_cursor2, 
            &key2, 
            &val, 
            DB_NEXT
            );
        if (error && (error != DB_NOTFOUND)) { goto cleanup; }

        cnt++;
        if ((cnt % 10000) == 0) {
            sprintf(
                status_msg, 
                "Verifying index uniqueness: Checked %llu of %llu rows in key-%s.", 
                (long long unsigned) cnt, 
                share->rows, 
                key_info->name);
            thd_proc_info(thd, status_msg);
            if (thd->killed) {
                my_error(ER_QUERY_INTERRUPTED, MYF(0));
                error = ER_QUERY_INTERRUPTED;
                goto cleanup;
            }
        }
    }

    error = 0;

cleanup:
    if (tmp_cursor1) {
        tmp_cursor1->c_close(tmp_cursor1);
        tmp_cursor1 = NULL;
    }
    if (tmp_cursor2) {
        tmp_cursor2->c_close(tmp_cursor2);
        tmp_cursor2 = NULL;
    }
    return error;
}

int ha_tokudb::is_val_unique(bool* is_unique, uchar* record, KEY* key_info, uint dict_index, DB_TXN* txn) {
    DBT key;
    int error = 0;
    bool has_null;
    DBC* tmp_cursor = NULL;
    struct index_read_info ir_info;
    struct smart_dbt_info info;
    memset((void *)&key, 0, sizeof(key));
    info.ha = this;
    info.buf = NULL;
    info.keynr = dict_index;

    ir_info.smart_dbt_info = info;
    
    create_dbt_key_for_lookup(
        &key,
        key_info,
        key_buff3,
        record,
        &has_null
        );
    ir_info.orig_key = &key;

    if (has_null) {
        error = 0;
        *is_unique = true;
        goto cleanup;
    }
    
    error = share->key_file[dict_index]->cursor(
        share->key_file[dict_index], 
        txn, 
        &tmp_cursor, 
        DB_SERIALIZABLE
        );
    if (error) { goto cleanup; }

    error = tmp_cursor->c_getf_set_range(
        tmp_cursor, 
        0, 
        &key, 
        smart_dbt_callback_lookup, 
        &ir_info
        );
    if (error == DB_NOTFOUND) {
        *is_unique = true;
        error = 0;
        goto cleanup;
    }
    else if (error) {
        goto cleanup;
    }
    if (ir_info.cmp) {
        *is_unique = true;
    }
    else {
        *is_unique = false;
    }
    error = 0;

cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
        tmp_cursor = NULL;
    }
    return error;
}

int ha_tokudb::do_uniqueness_checks(uchar* record, DB_TXN* txn, THD* thd) {
    int error;
    //
    // first do uniqueness checks
    //
    if (share->has_unique_keys && !thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
        for (uint keynr = 0; keynr < table_share->keys; keynr++) {
            bool is_unique_key = table->key_info[keynr].flags & HA_NOSAME;
            bool is_unique = false;
            //
            // don't need to do check for primary key
            //
            if (keynr == primary_key) {
                continue;
            }
            if (!is_unique_key) {
                continue;
            }
            //
            // if unique key, check uniqueness constraint
            // but, we do not need to check it if the key has a null
            // and we do not need to check it if unique_checks is off
            //
            error = is_val_unique(&is_unique, record, &table->key_info[keynr], keynr, txn);
            if (error) { goto cleanup; }
            if (!is_unique) {
                error = DB_KEYEXIST;
                last_dup_key = keynr;
                goto cleanup;
            }
        }
    }    
    error = 0;
cleanup:
    return error;
}

void ha_tokudb::test_row_packing(uchar* record, DBT* pk_key, DBT* pk_val) {
    int error;
    DBT row, key;
    //
    // variables for testing key packing, only used in some debug modes
    //
    uchar* tmp_pk_key_data = NULL;
    uchar* tmp_pk_val_data = NULL;
    DBT tmp_pk_key;
    DBT tmp_pk_val;
    bool has_null;
    int cmp;

    memset(&tmp_pk_key, 0, sizeof(DBT));
    memset(&tmp_pk_val, 0, sizeof(DBT));

    //
    //use for testing the packing of keys
    //
    tmp_pk_key_data = (uchar *)my_malloc(pk_key->size, MYF(MY_WME));
    assert(tmp_pk_key_data);
    tmp_pk_val_data = (uchar *)my_malloc(pk_val->size, MYF(MY_WME));
    assert(tmp_pk_val_data);
    memcpy(tmp_pk_key_data, pk_key->data, pk_key->size);
    memcpy(tmp_pk_val_data, pk_val->data, pk_val->size);
    tmp_pk_key.data = tmp_pk_key_data;
    tmp_pk_key.size = pk_key->size;
    tmp_pk_val.data = tmp_pk_val_data;
    tmp_pk_val.size = pk_val->size;

    for (uint keynr = 0; keynr < table_share->keys; keynr++) {
        uint32_t tmp_num_bytes = 0;
        uchar* row_desc = NULL;
        uint32_t desc_size = 0;
        
        if (keynr == primary_key) {
            continue;
        }

        create_dbt_key_from_table(&key, keynr, key_buff2, record, &has_null); 

        //
        // TEST
        //
        row_desc = (uchar *)share->key_file[keynr]->descriptor->dbt.data;
        row_desc += (*(uint32_t *)row_desc);
        desc_size = (*(uint32_t *)row_desc) - 4;
        row_desc += 4;
        tmp_num_bytes = pack_key_from_desc(
            key_buff3,
            row_desc,
            desc_size,
            &tmp_pk_key,
            &tmp_pk_val
            );
        assert(tmp_num_bytes == key.size);
        cmp = memcmp(key_buff3,key_buff2,tmp_num_bytes);
        assert(cmp == 0);

        //
        // test key packing of clustering keys
        //
        if (table->key_info[keynr].flags & HA_CLUSTERING) {
            error = pack_row(&row, (const uchar *) record, keynr);
            assert(error == 0);
            uchar* tmp_buff = NULL;
            tmp_buff = (uchar *)my_malloc(alloced_rec_buff_length,MYF(MY_WME));
            assert(tmp_buff);
            row_desc = (uchar *)share->key_file[keynr]->descriptor->dbt.data;
            row_desc += (*(uint32_t *)row_desc);
            row_desc += (*(uint32_t *)row_desc);
            desc_size = (*(uint32_t *)row_desc) - 4;
            row_desc += 4;
            tmp_num_bytes = pack_clustering_val_from_desc(
                tmp_buff,
                row_desc,
                desc_size,
                &tmp_pk_val
                );
            assert(tmp_num_bytes == row.size);
            cmp = memcmp(tmp_buff,rec_buff,tmp_num_bytes);
            assert(cmp == 0);
            my_free(tmp_buff,MYF(MY_ALLOW_ZERO_PTR));
        }
    }

    //
    // copy stuff back out
    //
    error = pack_row(pk_val, (const uchar *) record, primary_key);
    assert(pk_val->size == tmp_pk_val.size);
    cmp = memcmp(pk_val->data, tmp_pk_val_data, pk_val->size);    
    assert( cmp == 0);

    my_free(tmp_pk_key_data,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_pk_val_data,MYF(MY_ALLOW_ZERO_PTR));
}

//
// set the put flags for the main dictionary
//
void ha_tokudb::set_main_dict_put_flags(
    THD* thd, 
    bool opt_eligible,
    uint32_t* put_flags
    ) 
{
    uint32_t old_prelock_flags = 0;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    bool in_hot_index = share->num_DBs > curr_num_DBs;
    bool using_ignore_flag_opt = do_ignore_flag_optimization(
            thd, table, share->replace_into_fast);
    //
    // optimization for "REPLACE INTO..." (and "INSERT IGNORE") command
    // if the command is "REPLACE INTO" and the only table
    // is the main table (or all indexes are a subset of the pk), 
    // then we can simply insert the element
    // with DB_YESOVERWRITE. If the element does not exist,
    // it will act as a normal insert, and if it does exist, it 
    // will act as a replace, which is exactly what REPLACE INTO is supposed
    // to do. We cannot do this if otherwise, because then we lose
    // consistency between indexes
    //
    if (hidden_primary_key) 
    {
        *put_flags = old_prelock_flags;
    }
    else if (thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)
            && !is_replace_into(thd) && !is_insert_ignore(thd))
    {
        *put_flags = old_prelock_flags;
    }
    else if (using_ignore_flag_opt && is_replace_into(thd) 
            && !in_hot_index)
    {
        *put_flags = old_prelock_flags;
    }
    else if (opt_eligible && using_ignore_flag_opt && is_insert_ignore(thd) 
            && !in_hot_index)
    {
        *put_flags = DB_NOOVERWRITE_NO_ERROR | old_prelock_flags;
    }
    else 
    {
        *put_flags = DB_NOOVERWRITE | old_prelock_flags;
    }
}

int ha_tokudb::insert_row_to_main_dictionary(uchar* record, DBT* pk_key, DBT* pk_val, DB_TXN* txn) {
    int error = 0;
    uint32_t put_flags = mult_put_flags[primary_key];
    THD *thd = ha_thd();
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);

    assert(curr_num_DBs == 1);
    
    set_main_dict_put_flags(thd, true, &put_flags);

    error = share->file->put(
        share->file, 
        txn, 
        pk_key,
        pk_val, 
        put_flags
        );

    if (error) {
        last_dup_key = primary_key;
        goto cleanup;
    }

cleanup:
    return error;
}

int ha_tokudb::insert_rows_to_dictionaries_mult(DBT* pk_key, DBT* pk_val, DB_TXN* txn, THD* thd) {
    int error = 0;
    uint curr_num_DBs = share->num_DBs;
    set_main_dict_put_flags(thd, true, &mult_put_flags[primary_key]);
    uint32_t i, flags = mult_put_flags[primary_key];

    // the insert ignore optimization uses DB_NOOVERWRITE_NO_ERROR, 
    // which is not allowed with env->put_multiple. 
    // we have to insert the rows one by one in this case.
    if (flags & DB_NOOVERWRITE_NO_ERROR) {
        DB * src_db = share->key_file[primary_key];
        for (i = 0; i < curr_num_DBs; i++) {
            DB * db = share->key_file[i];
            if (i == primary_key) {
                // if it's the primary key, insert the rows
                // as they are.
                error = db->put(db, txn, pk_key, pk_val, flags);
            } else {
                // generate a row for secondary keys.
                // use our multi put key/rec buffers
                // just as the ydb layer would have in
                // env->put_multiple(), except that
                // we will just do a put() right away.
                error = tokudb_generate_row(db, src_db,
                        &mult_key_dbt[i], &mult_rec_dbt[i], 
                        pk_key, pk_val);
                if (error != 0) {
                    goto out;
                }
                error = db->put(db, txn, &mult_key_dbt[i], 
                        &mult_rec_dbt[i], flags);
            }
            if (error != 0) {
                goto out;
            }
        }
    } else {
        // not insert ignore, so we can use put multiple
        error = db_env->put_multiple(
            db_env, 
            share->key_file[primary_key], 
            txn, 
            pk_key, 
            pk_val,
            curr_num_DBs, 
            share->key_file, 
            mult_key_dbt,
            mult_rec_dbt,
            mult_put_flags
            );
    }

out:
    //
    // We break if we hit an error, unless it is a dup key error
    // and MySQL told us to ignore duplicate key errors
    //
    if (error) {
        last_dup_key = primary_key;
    }
    return error;
}

volatile int ha_tokudb_write_row_wait = 0; // debug

//
// Stores a row in the table, called when handling an INSERT query
// Parameters:
//      [in]    record - a row in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::write_row(uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::write_row");
    while (ha_tokudb_write_row_wait) sleep(1); // debug

    DBT row, prim_key;
    int error;
    THD *thd = ha_thd();
    bool has_null;
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;
    tokudb_trx_data *trx = NULL;
    uint curr_num_DBs;
    bool create_sub_trans = false;

    //
    // some crap that needs to be done because MySQL does not properly abstract
    // this work away from us, namely filling in auto increment and setting auto timestamp
    //
    statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
#if MYSQL_VERSION_ID < 50600
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT) {
        table->timestamp_field->set_time();
    }
#endif
    if (table->next_number_field && record == table->record[0]) {
        update_auto_increment();
    }

    //
    // check to see if some value for the auto increment column that is bigger
    // than anything else til now is being used. If so, update the metadata to reflect it
    // the goal here is we never want to have a dup key error due to a bad increment
    // of the auto inc field.
    //
    if (share->has_auto_inc && record == table->record[0]) {
        pthread_mutex_lock(&share->mutex);
        ulonglong curr_auto_inc = retrieve_auto_increment(
            table->field[share->ai_field_index]->key_type(), 
            field_offset(table->field[share->ai_field_index], table),
            record
            );
        if (curr_auto_inc > share->last_auto_increment) {
            share->last_auto_increment = curr_auto_inc;
            if (delay_updating_ai_metadata) {
                ai_metadata_update_required = true;
            }
            else {
                update_max_auto_inc(share->status_block, share->last_auto_increment);
            }
         }
        pthread_mutex_unlock(&share->mutex);
    }

    //
    // grab reader lock on numDBs_lock
    //
    if (!num_DBs_locked_in_bulk) {
        rw_rdlock(&share->num_DBs_lock);
    }
    else {
        lock_count++;
        if (lock_count >= 2000) {
            rw_unlock(&share->num_DBs_lock);
            rw_rdlock(&share->num_DBs_lock);
            lock_count = 0;
        }
    }
    curr_num_DBs = share->num_DBs;
    
    if (hidden_primary_key) {
        get_auto_primary_key(current_ident);
    }

    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(record))) {
            error = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }

    create_dbt_key_from_table(&prim_key, primary_key, primary_key_buff, record, &has_null);
    if ((error = pack_row(&row, (const uchar *) record, primary_key))){
        goto cleanup;
    }

    create_sub_trans = (using_ignore && !(do_ignore_flag_optimization(thd,table,share->replace_into_fast)));
    if (create_sub_trans) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
    }
    
    txn = create_sub_trans ? sub_trans : transaction;

    if (tokudb_debug & TOKUDB_DEBUG_CHECK_KEY) {
        test_row_packing(record,&prim_key,&row);
    }

    if (loader) {
        error = loader->put(loader, &prim_key, &row);
        if (error) {
            abort_loader = true;
            goto cleanup;
        }
    }
    else {
        if (curr_num_DBs == 1) {
            error = insert_row_to_main_dictionary(record,&prim_key, &row, txn);
            if (error) { goto cleanup; }
        }
        else {
            error = do_uniqueness_checks(record, txn, thd);
            if (error) {
                // for #4633
                // if we have a duplicate key error, let's check the primary key to see
                // if there is a duplicate there. If so, set last_dup_key to the pk
                if (error == DB_KEYEXIST && !test(hidden_primary_key)) {
                    int r = share->file->getf_set(
                        share->file, 
                        txn, 
                        0, 
                        &prim_key, 
                        smart_dbt_do_nothing, 
                        NULL
                        );
                    if (r == 0) {
                        // if we get no error, that means the row
                        // was found and this is a duplicate key,
                        // so we set last_dup_key
                        last_dup_key = primary_key;
                    }
                    else if (r != DB_NOTFOUND) {
                        // if some other error is returned, return that to the user.
                        error = r;
                    }
                }
                goto cleanup; 
            }

            error = insert_rows_to_dictionaries_mult(&prim_key, &row, txn, thd);
            if (error) { goto cleanup; }
        }
    }

    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!error) {
        added_rows++;
        trx->stmt_progress.inserted++;
        track_progress(thd);
    }
cleanup:
    if (!num_DBs_locked_in_bulk) {
       rw_unlock(&share->num_DBs_lock);
    }
    if (error == DB_KEYEXIST) {
        error = HA_ERR_FOUND_DUPP_KEY;
    }
    if (sub_trans) {
        // no point in recording error value of abort.
        // nothing we can do about it anyway and it is not what
        // we want to return.
        if (error) {
            abort_txn(sub_trans);
        }
        else {
            commit_txn(sub_trans, DB_TXN_NOSYNC);
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

/* Compare if a key in a row has changed */
bool ha_tokudb::key_changed(uint keynr, const uchar * old_row, const uchar * new_row) {
    DBT old_key;
    DBT new_key;
    memset((void *) &old_key, 0, sizeof(old_key));
    memset((void *) &new_key, 0, sizeof(new_key));

    bool has_null;
    create_dbt_key_from_table(&new_key, keynr, key_buff2, new_row, &has_null);
    create_dbt_key_for_lookup(&old_key,&table->key_info[keynr], key_buff3, old_row, &has_null);
    return tokudb_prefix_cmp_dbt_key(share->key_file[keynr], &old_key, &new_key);
}

//
// Updates a row in the table, called when handling an UPDATE query
// Parameters:
//      [in]    old_row - row to be updated, in MySQL format
//      [in]    new_row - new row, in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::update_row(const uchar * old_row, uchar * new_row) {
    TOKUDB_DBUG_ENTER("update_row");
    DBT prim_key, old_prim_key, prim_row, old_prim_row;
    int error;
    bool has_null;
    THD* thd = ha_thd();
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    uint curr_num_DBs;

    LINT_INIT(error);
    memset((void *) &prim_key, 0, sizeof(prim_key));
    memset((void *) &old_prim_key, 0, sizeof(old_prim_key));
    memset((void *) &prim_row, 0, sizeof(prim_row));
    memset((void *) &old_prim_row, 0, sizeof(old_prim_row));


    statistic_increment(table->in_use->status_var.ha_update_count, &LOCK_status);
#if MYSQL_VERSION_ID < 50600
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE) {
        table->timestamp_field->set_time();
    }
#endif
    //
    // check to see if some value for the auto increment column that is bigger
    // than anything else til now is being used. If so, update the metadata to reflect it
    // the goal here is we never want to have a dup key error due to a bad increment
    // of the auto inc field.
    //
    if (share->has_auto_inc && new_row == table->record[0]) {
        pthread_mutex_lock(&share->mutex);
        ulonglong curr_auto_inc = retrieve_auto_increment(
            table->field[share->ai_field_index]->key_type(), 
            field_offset(table->field[share->ai_field_index], table),
            new_row
            );
        if (curr_auto_inc > share->last_auto_increment) {
            error = update_max_auto_inc(share->status_block, curr_auto_inc);
            if (!error) {
                share->last_auto_increment = curr_auto_inc;
            }
        }
        pthread_mutex_unlock(&share->mutex);
    }

    //
    // grab reader lock on numDBs_lock
    //
    rw_rdlock(&share->num_DBs_lock);
    curr_num_DBs = share->num_DBs;

    if (using_ignore) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
    }
    txn = using_ignore ? sub_trans : transaction;


    if (hidden_primary_key) {
        memset((void *) &prim_key, 0, sizeof(prim_key));
        prim_key.data = (void *) current_ident;
        prim_key.size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        old_prim_key = prim_key;
    } 
    else {
        create_dbt_key_from_table(&prim_key, primary_key, key_buff, new_row, &has_null);
        create_dbt_key_from_table(&old_prim_key, primary_key, primary_key_buff, old_row, &has_null);
    }

    //
    // do uniqueness checks
    //
    if (share->has_unique_keys && !thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
        for (uint keynr = 0; keynr < table_share->keys; keynr++) {
            bool is_unique_key = table->key_info[keynr].flags & HA_NOSAME;
            if (keynr == primary_key) {
                continue;
            }
            if (is_unique_key) {
                bool key_ch = key_changed(keynr, old_row, new_row);
                if (key_ch) {
                    bool is_unique;
                    error = is_val_unique(&is_unique, new_row, &table->key_info[keynr], keynr, txn);
                    if (error) goto cleanup;
                    if (!is_unique) {
                        error = DB_KEYEXIST;
                        last_dup_key = keynr;
                        goto cleanup;
                    }
                }
            }
        }
    }
    
    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(new_row))) {
            error = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
        if (fix_rec_update_buff_for_blob(max_row_length(old_row))) {
            error = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }

    error = pack_row(&prim_row, new_row, primary_key);
    if (error) { goto cleanup; }

    error = pack_old_row_for_update(&old_prim_row, old_row, primary_key);
    if (error) { goto cleanup; }

    set_main_dict_put_flags(thd, false, &mult_put_flags[primary_key]);

    error = db_env->update_multiple(
        db_env, 
        share->key_file[primary_key], 
        txn,
        &old_prim_key, 
        &old_prim_row,
        &prim_key, 
        &prim_row,
        curr_num_DBs, 
        share->key_file,
        mult_put_flags,
        2*curr_num_DBs, 
        mult_key_dbt,
        curr_num_DBs, 
        mult_rec_dbt
        );
    
    if (error == DB_KEYEXIST) {
        last_dup_key = primary_key;
    }    
    else if (!error) {
        trx->stmt_progress.updated++;
        track_progress(thd);
    }


cleanup:
    rw_unlock(&share->num_DBs_lock);
    if (error == DB_KEYEXIST) {
        error = HA_ERR_FOUND_DUPP_KEY;
    }
    if (sub_trans) {
        // no point in recording error value of abort.
        // nothing we can do about it anyway and it is not what
        // we want to return.
        if (error) {
            abort_txn(sub_trans);
        }
        else {
            commit_txn(sub_trans, DB_TXN_NOSYNC);
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Deletes a row in the table, called when handling a DELETE query
// Parameters:
//      [in]    record - row to be deleted, in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::delete_row(const uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::delete_row");
    int error = ENOSYS;
    DBT row, prim_key;
    bool has_null;
    THD* thd = ha_thd();
    uint curr_num_DBs;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;

    statistic_increment(table->in_use->status_var.ha_delete_count, &LOCK_status);

    //
    // grab reader lock on numDBs_lock
    //
    rw_rdlock(&share->num_DBs_lock);
    curr_num_DBs = share->num_DBs;

    create_dbt_key_from_table(&prim_key, primary_key, key_buff, record, &has_null);
    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(record))) {
            error = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }
    if ((error = pack_row(&row, (const uchar *) record, primary_key))){
        goto cleanup;
    }

    error = db_env->del_multiple(
        db_env, 
        share->key_file[primary_key], 
        transaction, 
        &prim_key, 
        &row,
        curr_num_DBs, 
        share->key_file, 
        mult_key_dbt,
        mult_del_flags
        );

    if (error) {
        DBUG_PRINT("error", ("Got error %d", error));
    }
    else {
        deleted_rows++;
        trx->stmt_progress.deleted++;
        track_progress(thd);
    }
cleanup:
    rw_unlock(&share->num_DBs_lock);
    TOKUDB_DBUG_RETURN(error);
}

//
// takes as input table->read_set and table->write_set
// and puts list of field indexes that need to be read in
// unpack_row in the member variables fixed_cols_for_query
// and var_cols_for_query
//
void ha_tokudb::set_query_columns(uint keynr) {
    uint32_t curr_fixed_col_index = 0;
    uint32_t curr_var_col_index = 0;
    read_key = false;
    read_blobs = false;
    //
    // i know this is probably confusing and will need to be explained better
    //
    uint key_index = 0;

    if (keynr == primary_key || keynr == MAX_KEY) {
        key_index = primary_key;
    }
    else {
        key_index = (table->key_info[keynr].flags & HA_CLUSTERING ? keynr : primary_key);
    }
    for (uint i = 0; i < table_share->fields; i++) {
        if (bitmap_is_set(table->read_set,i) || 
            bitmap_is_set(table->write_set,i)
            ) 
        {
            if (bitmap_is_set(&share->kc_info.key_filters[key_index],i)) {
                read_key = true;
            }
            else {
                //
                // if fixed field length
                //
                if (share->kc_info.field_lengths[i] != 0) {
                    //
                    // save the offset into the list
                    //
                    fixed_cols_for_query[curr_fixed_col_index] = i;
                    curr_fixed_col_index++;
                }
                //
                // varchar or varbinary
                //
                else if (share->kc_info.length_bytes[i] != 0) {
                    var_cols_for_query[curr_var_col_index] = i;
                    curr_var_col_index++;
                }
                //
                // it is a blob
                //
                else {
                    read_blobs = true;
                }
            }
        }
    }
    num_fixed_cols_for_query = curr_fixed_col_index;
    num_var_cols_for_query = curr_var_col_index;
}

void ha_tokudb::column_bitmaps_signal() {
    //
    // if we have max number of indexes, then MAX_KEY == primary_key
    //
    if (tokudb_active_index != MAX_KEY || tokudb_active_index == primary_key) {
        set_query_columns(tokudb_active_index);
    }
}

//
// Notification that a scan of entire secondary table is about
// to take place. Will pre acquire table read lock
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::prepare_index_scan() {
    int error = 0;
    HANDLE_INVALID_CURSOR();
    error = prelock_range(NULL, NULL);
    if (error) { last_cursor_error = error; goto cleanup; }

    range_lock_grabbed = true;
    error = 0;
cleanup:
    return error;
}


//
// Notification that a range query getting all elements that equal a key
//  to take place. Will pre acquire read lock
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::prepare_index_key_scan(const uchar * key, uint key_len) {
    int error = 0;
    DBT start_key, end_key;
    THD* thd = ha_thd();
    HANDLE_INVALID_CURSOR();
    pack_key(&start_key, tokudb_active_index, prelocked_left_range, key, key_len, COL_NEG_INF);
    prelocked_left_range_size = start_key.size;
    pack_key(&end_key, tokudb_active_index, prelocked_right_range, key, key_len, COL_POS_INF);
    prelocked_right_range_size = end_key.size;

    error = cursor->c_pre_acquire_range_lock(
        cursor, 
        &start_key, 
        &end_key 
        );

    if (error){ 
        goto cleanup; 
    }

    range_lock_grabbed = true;
    doing_bulk_fetch = (thd_sql_command(thd) == SQLCOM_SELECT);
    bulk_fetch_iteration = 0;
    rows_fetched_using_bulk_fetch = 0;
    error = 0;
cleanup:
    if (error) {
        last_cursor_error = error;
        //
        // cursor should be initialized here, but in case it is not, 
        // we still check
        //
        if (cursor) {
            int r = cursor->c_close(cursor);
            assert(r==0);
            cursor = NULL;
        }
    }
    return error;
}

void ha_tokudb::invalidate_bulk_fetch() {
    bytes_used_in_range_query_buff= 0;
    curr_range_query_buff_offset = 0;
}

volatile int ha_tokudb_index_init_wait = 0; // debug

//
// Initializes local cursor on DB with index keynr
// Parameters:
//          keynr - key (index) number
//          sorted - 1 if result MUST be sorted according to index
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::index_init(uint keynr, bool sorted) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_init %p %d", this, keynr);
    while (ha_tokudb_index_init_wait) sleep(1); // debug

    int error;
    THD* thd = ha_thd(); 
    DBUG_PRINT("enter", ("table: '%s'  key: %d", table_share->table_name.str, keynr));

    /*
       Under some very rare conditions (like full joins) we may already have
       an active cursor at this point
     */
    if (cursor) {
        DBUG_PRINT("note", ("Closing active cursor"));
        int r = cursor->c_close(cursor);
        assert(r==0);
    }
    active_index = keynr;

    if (active_index < MAX_KEY) {
        DBUG_ASSERT(keynr <= table->s->keys);
    } else {
        DBUG_ASSERT(active_index == MAX_KEY);
        keynr = primary_key;
    }
    tokudb_active_index = keynr;

    last_cursor_error = 0;
    range_lock_grabbed = false;
    DBUG_ASSERT(share->key_file[keynr]);
    cursor_flags = get_cursor_isolation_flags(lock.type, thd);
    if (use_write_locks) {
        cursor_flags |= DB_RMW;
    }
    if (get_disable_prefetching(thd)) {
        cursor_flags |= DBC_DISABLE_PREFETCHING;
    }
    if ((error = share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, cursor_flags))) {
        if (error == TOKUDB_MVCC_DICTIONARY_TOO_NEW) {
            error = HA_ERR_TABLE_DEF_CHANGED;
            my_error(ER_TABLE_DEF_CHANGED, MYF(0));
        }
        if (error == DB_LOCK_NOTGRANTED) {
            error = HA_ERR_LOCK_WAIT_TIMEOUT;
            my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
        }
        table->status = STATUS_NOT_FOUND;
        last_cursor_error = error;
        cursor = NULL;             // Safety
        goto exit;
    }
    memset((void *) &last_key, 0, sizeof(last_key));

    if (thd_sql_command(thd) == SQLCOM_SELECT) {
        set_query_columns(keynr);
        unpack_entire_row = false;
    }
    else {
        unpack_entire_row = true;
    }
    invalidate_bulk_fetch();
    doing_bulk_fetch = false;
    error = 0;
exit:
    TOKUDB_DBUG_RETURN(error);
}

//
// closes the local cursor
//
int ha_tokudb::index_end() {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_end %p", this);
    range_lock_grabbed = false;
    if (cursor) {
        DBUG_PRINT("enter", ("table: '%s'", table_share->table_name.str));
        int r = cursor->c_close(cursor);
        assert(r==0);
        cursor = NULL;
        last_cursor_error = 0;
    }
    active_index = tokudb_active_index = MAX_KEY;

    //
    // reset query variables
    //
    unpack_entire_row = true;
    read_blobs = true;
    read_key = true;
    num_fixed_cols_for_query = 0;
    num_var_cols_for_query = 0;

    invalidate_bulk_fetch();
    doing_bulk_fetch = false;

    TOKUDB_DBUG_RETURN(0);
}


int ha_tokudb::handle_cursor_error(int error, int err_to_return, uint keynr) {
    TOKUDB_DBUG_ENTER("ha_tokudb::handle_cursor_error");
    if (error) {
        if (error == DB_LOCK_NOTGRANTED) {
            error = HA_ERR_LOCK_WAIT_TIMEOUT;
        }
        last_cursor_error = error;
        table->status = STATUS_NOT_FOUND;
        int r = cursor->c_close(cursor);
        assert(r==0);
        cursor = NULL;
        if (error == DB_NOTFOUND) {
            error = err_to_return;
            if ((share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, cursor_flags))) {
                cursor = NULL;             // Safety
            }
        }
    }
    TOKUDB_DBUG_RETURN(error);
}


//
// Helper function for read_row and smart_dbt_callback_xxx functions
// When using a hidden primary key, upon reading a row, 
// we set the current_ident field to whatever the primary key we retrieved
// was
//
void ha_tokudb::extract_hidden_primary_key(uint keynr, DBT const *found_key) {
    //
    // extract hidden primary key to current_ident
    //
    if (hidden_primary_key) {
        if (keynr == primary_key) {
            memcpy_fixed(current_ident, (char *) found_key->data, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        }
        //
        // if secondary key, hidden primary key is at end of found_key
        //
        else {
            memcpy_fixed(
                current_ident, 
                (char *) found_key->data + found_key->size - TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH, 
                TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH
                );
        }
    }
}


int ha_tokudb::read_row_callback (uchar * buf, uint keynr, DBT const *row, DBT const *found_key) {
    assert(keynr == primary_key);
    return unpack_row(buf, row,found_key, keynr);
}

//
// Reads the contents of row and found_key, DBT's retrieved from the DB associated to keynr, into buf
// This function assumes that we are using a covering index, as a result, if keynr is the primary key,
// we do not read row into buf
// Parameters:
//      [out]   buf - buffer for the row, in MySQL format
//              keynr - index into key_file that represents DB we are currently operating on.
//      [in]    row - the row that has been read from the preceding DB call
//      [in]    found_key - key used to retrieve the row
//
void ha_tokudb::read_key_only(uchar * buf, uint keynr, DBT const *found_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_key_only");
    table->status = 0;
    //
    // only case when we do not unpack the key is if we are dealing with the main dictionary
    // of a table with a hidden primary key
    //
    if (!(hidden_primary_key && keynr == primary_key)) {
        unpack_key(buf, found_key, keynr);
    }
    DBUG_VOID_RETURN;
}

//
// Helper function used to try to retrieve the entire row
// If keynr is associated with the main table, reads contents of found_key and row into buf, otherwise,
// makes copy of primary key and saves it to last_key. This can later be used to retrieve the entire row
// Parameters:
//      [out]   buf - buffer for the row, in MySQL format
//              keynr - index into key_file that represents DB we are currently operating on.
//      [in]    row - the row that has been read from the preceding DB call
//      [in]    found_key - key used to retrieve the row
//
int ha_tokudb::read_primary_key(uchar * buf, uint keynr, DBT const *row, DBT const *found_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_primary_key");
    int error = 0;
    table->status = 0;
    //
    // case where we read from secondary table that is not clustered
    //
    if (keynr != primary_key && !(table->key_info[keynr].flags & HA_CLUSTERING)) {
        bool has_null;
        //
        // create a DBT that has the same data as row, this is inefficient
        // extract_hidden_primary_key MUST have been called before this
        //
        memset((void *) &last_key, 0, sizeof(last_key));
        if (!hidden_primary_key) {
            unpack_key(buf, found_key, keynr);
        }
        create_dbt_key_from_table(
            &last_key, 
            primary_key,
            key_buff,
            buf,
            &has_null
            );
    }
    //
    // else read from clustered/primary key
    //
    else {
        error = unpack_row(buf, row, found_key, keynr);
        if (error) { goto exit; }
    }
    if (found_key) { DBUG_DUMP("read row key", (uchar *) found_key->data, found_key->size); }
    error = 0;
exit:
    TOKUDB_DBUG_RETURN(error);
}

//
// This function reads an entire row into buf. This function also assumes that
// the key needed to retrieve the row is stored in the member variable last_key
// Parameters:
//      [out]   buf - buffer for the row, in MySQL format
// Returns:
//      0 on success, error otherwise
//
int ha_tokudb::read_full_row(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_full_row");
    int error = 0;
    struct smart_dbt_info info;
    info.ha = this;
    info.buf = buf;
    info.keynr = primary_key;
    //
    // assumes key is stored in this->last_key
    //

    error = share->file->getf_set(
        share->file, 
        transaction, 
        cursor_flags, 
        &last_key, 
        smart_dbt_callback_rowread_ptquery, 
        &info
        );

    if (error) {
        if (error == DB_LOCK_NOTGRANTED) {
            error = HA_ERR_LOCK_WAIT_TIMEOUT;
        }
        table->status = STATUS_NOT_FOUND;
        TOKUDB_DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
    }

    TOKUDB_DBUG_RETURN(error);
}


// 
// Reads the next row matching to the key, on success, advances cursor 
// Parameters: 
//      [out]   buf - buffer for the next row, in MySQL format 
//      [in]     key - key value 
//                keylen - length of key 
// Returns: 
//      0 on success 
//      HA_ERR_END_OF_FILE if not found 
//      error otherwise 
// 
int ha_tokudb::index_next_same(uchar * buf, const uchar * key, uint keylen) { 
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next_same");
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);

    DBT curr_key;
    DBT found_key;
    bool has_null;
    int cmp;
    int error = get_next(buf, 1);
    if (error) {
        goto cleanup;
    }
    //
    // now do the comparison
    //
    pack_key(&curr_key, tokudb_active_index, key_buff2, key, keylen, COL_ZERO);
    create_dbt_key_from_table(&found_key,tokudb_active_index,key_buff3,buf,&has_null);
    cmp = tokudb_prefix_cmp_dbt_key(share->key_file[tokudb_active_index], &curr_key, &found_key);
    if (cmp) {
        error = HA_ERR_END_OF_FILE; 
    }

cleanup: 
    TOKUDB_DBUG_RETURN(error);
} 


//
// According to InnoDB handlerton: Positions an index cursor to the index 
// specified in keynr. Fetches the row if any
// Parameters:
//      [out]       buf - buffer for the  returned row
//      [in]         key - key value, according to InnoDB, if NULL, 
//                              position cursor at start or end of index,
//                              not sure if this is done now
//                    key_len - length of key
//                    find_flag - according to InnoDB, search flags from my_base.h
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found (per InnoDB), 
//          we seem to return HA_ERR_END_OF_FILE if find_flag != HA_READ_KEY_EXACT
//          TODO: investigate this for correctness
//      error otherwise
//
int ha_tokudb::index_read(uchar * buf, const uchar * key, uint key_len, enum ha_rkey_function find_flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_read %p find %d", this, find_flag);
    invalidate_bulk_fetch();
    // TOKUDB_DBUG_DUMP("key=", key, key_len);
    DBT row;
    DBT lookup_key;
    int error = 0;    
    uint32_t flags = 0;
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    struct smart_dbt_info info;
    struct index_read_info ir_info;

    HANDLE_INVALID_CURSOR();

    table->in_use->status_var.ha_read_key_count++;
    memset((void *) &row, 0, sizeof(row));

    info.ha = this;
    info.buf = buf;
    info.keynr = tokudb_active_index;

    ir_info.smart_dbt_info = info;
    ir_info.cmp = 0;

    flags = SET_PRELOCK_FLAG(0);
    switch (find_flag) {
    case HA_READ_KEY_EXACT: /* Find first record else error */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_NEG_INF);
        ir_info.orig_key = &lookup_key;

        error = cursor->c_getf_set_range(cursor, flags,
                &lookup_key, SMART_DBT_IR_CALLBACK, &ir_info);
        if (ir_info.cmp) {
            error = DB_NOTFOUND;
        }
        break;
    case HA_READ_AFTER_KEY: /* Find next rec. after key-record */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_POS_INF);
        error = cursor->c_getf_set_range(cursor, flags,
                &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_BEFORE_KEY: /* Find next rec. before key-record */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_NEG_INF);
        error = cursor->c_getf_set_range_reverse(cursor, flags, 
                &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_KEY_OR_NEXT: /* Record or next record */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_NEG_INF);
        error = cursor->c_getf_set_range(cursor, flags,
                &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    //
    // This case does not seem to ever be used, it is ok for it to be slow
    //
    case HA_READ_KEY_OR_PREV: /* Record or previous */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_NEG_INF);
        ir_info.orig_key = &lookup_key;
        error = cursor->c_getf_set_range(cursor, flags,
                &lookup_key, SMART_DBT_IR_CALLBACK, &ir_info);
        if (error == DB_NOTFOUND) {
            error = cursor->c_getf_last(cursor, flags, SMART_DBT_CALLBACK, &info);
        }
        else if (ir_info.cmp) {
            error = cursor->c_getf_prev(cursor, flags, SMART_DBT_CALLBACK, &info);
        }
        break;
    case HA_READ_PREFIX_LAST_OR_PREV: /* Last or prev key with the same prefix */
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_POS_INF);
        error = cursor->c_getf_set_range_reverse(cursor, flags, 
                    &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_PREFIX_LAST:
        pack_key(&lookup_key, tokudb_active_index, key_buff3, key, key_len, COL_POS_INF);
        ir_info.orig_key = &lookup_key;
        error = cursor->c_getf_set_range_reverse(cursor, flags, &lookup_key, SMART_DBT_IR_CALLBACK, &ir_info);
        if (ir_info.cmp) {
            error = DB_NOTFOUND;
        }
        break;
    default:
        TOKUDB_TRACE("unsupported:%d\n", find_flag);
        error = HA_ERR_UNSUPPORTED;
        break;
    }
    error = handle_cursor_error(error,HA_ERR_KEY_NOT_FOUND,tokudb_active_index);
    if (!error && !key_read && tokudb_active_index != primary_key && !(table->key_info[tokudb_active_index].flags & HA_CLUSTERING)) {
        error = read_full_row(buf);
    }
    
    if (error && (tokudb_debug & TOKUDB_DEBUG_ERROR)) {
        TOKUDB_TRACE("error:%d:%d\n", error, find_flag);
    }
    trx->stmt_progress.queried++;
    track_progress(thd);

cleanup:
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::read_data_from_range_query_buff(uchar* buf, bool need_val) {
    // buffer has the next row, get it from there
    int error;
    uchar* curr_pos = range_query_buff+curr_range_query_buff_offset;
    DBT curr_key;
    memset((void *) &curr_key, 0, sizeof(curr_key));
    
    // get key info
    uint32_t key_size = *(uint32_t *)curr_pos;
    curr_pos += sizeof(key_size);
    uchar* curr_key_buff = curr_pos;
    curr_pos += key_size;
    
    curr_key.data = curr_key_buff;
    curr_key.size = key_size;
    
    // if this is a covering index, this is all we need
    if (this->key_read) {
        assert(!need_val);
        extract_hidden_primary_key(tokudb_active_index, &curr_key);
        read_key_only(buf, tokudb_active_index, &curr_key);
        error = 0;
    }
    // we need to get more data
    else {
        DBT curr_val;
        memset((void *) &curr_val, 0, sizeof(curr_val));
        uchar* curr_val_buff = NULL;
        uint32_t val_size = 0;
        // in this case, we don't have a val, we are simply extracting the pk
        if (!need_val) {
            curr_val.data = curr_val_buff;
            curr_val.size = val_size;
            extract_hidden_primary_key(tokudb_active_index, &curr_key);
            error = read_primary_key( buf, tokudb_active_index, &curr_val, &curr_key);
        }
        else {
            extract_hidden_primary_key(tokudb_active_index, &curr_key);
            // need to extract a val and place it into buf
            if (unpack_entire_row) {
                // get val info
                val_size = *(uint32_t *)curr_pos;
                curr_pos += sizeof(val_size);
                curr_val_buff = curr_pos;
                curr_pos += val_size;
                curr_val.data = curr_val_buff;
                curr_val.size = val_size;
                error = unpack_row(buf,&curr_val, &curr_key, tokudb_active_index);
            }
            else {
                if (!(hidden_primary_key && tokudb_active_index == primary_key)) {
                    unpack_key(buf,&curr_key,tokudb_active_index);
                }
                // read rows we care about

                // first the null bytes;
                memcpy(buf, curr_pos, table_share->null_bytes);
                curr_pos += table_share->null_bytes;

                // now the fixed sized rows                
                for (uint32_t i = 0; i < num_fixed_cols_for_query; i++) {
                    uint field_index = fixed_cols_for_query[i];
                    Field* field = table->field[field_index];
                    unpack_fixed_field(
                        buf + field_offset(field, table),
                        curr_pos,
                        share->kc_info.field_lengths[field_index]
                        );
                    curr_pos += share->kc_info.field_lengths[field_index];
                }
                // now the variable sized rows
                for (uint32_t i = 0; i < num_var_cols_for_query; i++) {
                    uint field_index = var_cols_for_query[i];
                    Field* field = table->field[field_index];
                    uint32_t field_len = *(uint32_t *)curr_pos;
                    curr_pos += sizeof(field_len);
                    unpack_var_field(
                        buf + field_offset(field, table),
                        curr_pos,
                        field_len,
                        share->kc_info.length_bytes[field_index]
                        );
                    curr_pos += field_len;
                }
                // now the blobs
                if (read_blobs) {
                    uint32_t blob_size = *(uint32_t *)curr_pos;
                    curr_pos += sizeof(blob_size);
                    error = unpack_blobs(
                        buf,
                        curr_pos,
                        blob_size,
                        true
                        );
                    curr_pos += blob_size;
                    if (error) {
                        invalidate_bulk_fetch();
                        goto exit;
                    }
                }
                error = 0;
            }
        }
    }
    
    curr_range_query_buff_offset = curr_pos - range_query_buff;
exit:
    return error;
}

static int
smart_dbt_bf_callback(DBT const *key, DBT  const *row, void *context) {
    SMART_DBT_BF_INFO info = (SMART_DBT_BF_INFO)context;
    return info->ha->fill_range_query_buf(info->need_val, key, row, info->direction, info->thd);
}

// fill in the range query buf for bulk fetch
int ha_tokudb::fill_range_query_buf(
    bool need_val, 
    DBT const *key, 
    DBT  const *row, 
    int direction,
    THD* thd
    ) {
    int error;
    //
    // first put the value into range_query_buf
    //
    uint32_t size_remaining = size_range_query_buff - bytes_used_in_range_query_buff;
    uint32_t size_needed;
    uint32_t user_defined_size = get_tokudb_read_buf_size(thd);
    uchar* curr_pos = NULL;
    if (need_val) {
        if (unpack_entire_row) {
            size_needed = 2*sizeof(uint32_t) + key->size + row->size;
        }
        else {
            // this is an upper bound
            size_needed = sizeof(uint32_t) + // size of key length
                          key->size + row->size + //key and row
                          num_var_cols_for_query*(sizeof(uint32_t)) + //lengths of varchars stored
                          sizeof(uint32_t); //length of blobs
        }
    }
    else {
        size_needed = sizeof(uint32_t) + key->size;
    }
    if (size_remaining < size_needed) {
        range_query_buff = (uchar *)my_realloc(
            (void *)range_query_buff, 
            bytes_used_in_range_query_buff+size_needed, 
            MYF(MY_WME)
            );
        if (range_query_buff == NULL) {
            error = ENOMEM;
            invalidate_bulk_fetch();
            goto cleanup;
        }
        size_range_query_buff = bytes_used_in_range_query_buff+size_needed;
    }
    //
    // now we know we have the size, let's fill the buffer, starting with the key
    //
    curr_pos = range_query_buff + bytes_used_in_range_query_buff;

    *(uint32_t *)curr_pos = key->size;
    curr_pos += sizeof(uint32_t);
    memcpy(curr_pos, key->data, key->size);
    curr_pos += key->size;
    if (need_val) {
        if (unpack_entire_row) {
            *(uint32_t *)curr_pos = row->size;
            curr_pos += sizeof(uint32_t);
            memcpy(curr_pos, row->data, row->size);
            curr_pos += row->size;
        }
        else {
            // need to unpack just the data we care about
            const uchar* fixed_field_ptr = (const uchar *) row->data;
            fixed_field_ptr += table_share->null_bytes;

            const uchar* var_field_offset_ptr = NULL;
            const uchar* var_field_data_ptr = NULL;
            
            var_field_offset_ptr = fixed_field_ptr + share->kc_info.mcp_info[tokudb_active_index].fixed_field_size;
            var_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[tokudb_active_index].len_of_offsets;

            // first the null bytes
            memcpy(curr_pos, row->data, table_share->null_bytes);
            curr_pos += table_share->null_bytes;
            // now the fixed fields
            //
            // first the fixed fields
            //
            for (uint32_t i = 0; i < num_fixed_cols_for_query; i++) {
                uint field_index = fixed_cols_for_query[i];
                memcpy(
                    curr_pos, 
                    fixed_field_ptr + share->kc_info.cp_info[tokudb_active_index][field_index].col_pack_val,
                    share->kc_info.field_lengths[field_index]
                    );
                curr_pos += share->kc_info.field_lengths[field_index];
            }
            
            //
            // now the var fields
            //
            for (uint32_t i = 0; i < num_var_cols_for_query; i++) {
                uint field_index = var_cols_for_query[i];
                uint32_t var_field_index = share->kc_info.cp_info[tokudb_active_index][field_index].col_pack_val;
                uint32_t data_start_offset;
                uint32_t field_len;
                
                get_var_field_info(
                    &field_len, 
                    &data_start_offset, 
                    var_field_index, 
                    var_field_offset_ptr, 
                    share->kc_info.num_offset_bytes
                    );
                memcpy(curr_pos, &field_len, sizeof(field_len));
                curr_pos += sizeof(field_len);
                memcpy(curr_pos, var_field_data_ptr + data_start_offset, field_len);
                curr_pos += field_len;
            }
            
            if (read_blobs) {
                uint32_t blob_offset = 0;
                uint32_t data_size = 0;
                //
                // now the blobs
                //
                get_blob_field_info(
                    &blob_offset, 
                    share->kc_info.mcp_info[tokudb_active_index].len_of_offsets,
                    var_field_data_ptr, 
                    share->kc_info.num_offset_bytes
                    );
                data_size = row->size - blob_offset - (uint32_t)(var_field_data_ptr - (const uchar *)row->data);
                memcpy(curr_pos, &data_size, sizeof(data_size));
                curr_pos += sizeof(data_size);
                memcpy(curr_pos, var_field_data_ptr + blob_offset, data_size);
                curr_pos += data_size;
            }
        }
    }

    bytes_used_in_range_query_buff = curr_pos - range_query_buff;
    assert(bytes_used_in_range_query_buff <= size_range_query_buff);

    //
    // now determine if we should continue with the bulk fetch
    // we want to stop under these conditions:
    //  - we overran the prelocked range
    //  - we are close to the end of the buffer
    //  - we have fetched an exponential amount of rows with
    //  respect to the bulk fetch iteration, which is initialized 
    //  to 0 in index_init() and prelock_range().

    rows_fetched_using_bulk_fetch++;
    // if the iteration is less than the number of possible shifts on
    // a 64 bit integer, check that we haven't exceeded this iterations
    // row fetch upper bound.
    if (bulk_fetch_iteration < HA_TOKU_BULK_FETCH_ITERATION_MAX) {
        uint64_t row_fetch_upper_bound = 1LLU << bulk_fetch_iteration;
        assert(row_fetch_upper_bound > 0);
        if (rows_fetched_using_bulk_fetch >= row_fetch_upper_bound) { 
            error = 0;
            goto cleanup;
        }
    }

    if (bytes_used_in_range_query_buff + table_share->rec_buff_length > user_defined_size) {
        error = 0;
        goto cleanup;
    }
    if (direction > 0) {
        // compare what we got to the right endpoint of prelocked range
        // because we are searching keys in ascending order
        if (prelocked_right_range_size == 0) {
            error = TOKUDB_CURSOR_CONTINUE;
            goto cleanup;
        }
        DBT right_range;
        memset(&right_range, 0, sizeof(right_range));
        right_range.size = prelocked_right_range_size;
        right_range.data = prelocked_right_range;
        int cmp = tokudb_cmp_dbt_key(
            share->key_file[tokudb_active_index], 
            key, 
            &right_range
            );
        error = (cmp > 0) ? 0 : TOKUDB_CURSOR_CONTINUE;
    }
    else {
        // compare what we got to the left endpoint of prelocked range
        // because we are searching keys in descending order
        if (prelocked_left_range_size == 0) {
            error = TOKUDB_CURSOR_CONTINUE;
            goto cleanup;
        }
        DBT left_range;
        memset(&left_range, 0, sizeof(left_range));
        left_range.size = prelocked_left_range_size;
        left_range.data = prelocked_left_range;
        int cmp = tokudb_cmp_dbt_key(
            share->key_file[tokudb_active_index], 
            key, 
            &left_range
            );
        error = (cmp < 0) ? 0 : TOKUDB_CURSOR_CONTINUE;
    }
cleanup:
    return error;
}

int ha_tokudb::get_next(uchar* buf, int direction) {
    int error = 0; 
    uint32_t flags = SET_PRELOCK_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    bool need_val;
    HANDLE_INVALID_CURSOR();

    // we need to read the val of what we retrieve if
    // we do NOT have a covering index AND we are using a clustering secondary
    // key
    need_val = (this->key_read == 0) && 
                (tokudb_active_index == primary_key || 
                 table->key_info[tokudb_active_index].flags & HA_CLUSTERING
                       );

    if ((bytes_used_in_range_query_buff - curr_range_query_buff_offset) > 0) {
        error = read_data_from_range_query_buff(buf, need_val);
    }
    else {
        invalidate_bulk_fetch();
        if (doing_bulk_fetch) {
            struct smart_dbt_bf_info bf_info;
            bf_info.ha = this;
            // you need the val if you have a clustering index and key_read is not 0;
            bf_info.direction = direction;
            bf_info.thd = ha_thd();
            bf_info.need_val = need_val;
            //
            // call c_getf_next with purpose of filling in range_query_buff
            //
            rows_fetched_using_bulk_fetch = 0;
            if (direction > 0) {
                error = cursor->c_getf_next(cursor, flags, smart_dbt_bf_callback, &bf_info);
            } else {
                error = cursor->c_getf_prev(cursor, flags, smart_dbt_bf_callback, &bf_info);
            }
            if (bulk_fetch_iteration < HA_TOKU_BULK_FETCH_ITERATION_MAX) {
                bulk_fetch_iteration++;
            }

            error = handle_cursor_error(error, HA_ERR_END_OF_FILE,tokudb_active_index);
            if (error) { goto cleanup; }
            
            //
            // now that range_query_buff is filled, read an element
            //
            error = read_data_from_range_query_buff(buf, need_val);
        }
        else {
            struct smart_dbt_info info;
            info.ha = this;
            info.buf = buf;
            info.keynr = tokudb_active_index;

            if (direction > 0) {
                error = cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info);
            } else {
                error = cursor->c_getf_prev(cursor, flags, SMART_DBT_CALLBACK, &info);
            }
            error = handle_cursor_error(error, HA_ERR_END_OF_FILE, tokudb_active_index);
        }
    }

    //
    // at this point, one of two things has happened
    // either we have unpacked the data into buf, and we 
    // are done, or we have unpacked the primary key
    // into last_key, and we use the code below to
    // read the full row by doing a point query into the 
    // main table.
    //
    
    if (!error && !key_read && (tokudb_active_index != primary_key) && !(table->key_info[tokudb_active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }
    trx->stmt_progress.queried++;
    track_progress(thd);
cleanup:
    return error;
}


//
// Reads the next row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_next(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next");
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);
    int error = get_next(buf, 1);
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::index_read_last(uchar * buf, const uchar * key, uint key_len) {
    return(index_read(buf, key, key_len, HA_READ_PREFIX_LAST));    
}


//
// Reads the previous row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_prev(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_prev");
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);
    int error = get_next(buf, -1);
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the first row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_first(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_first");
    invalidate_bulk_fetch();
    int error = 0;
    struct smart_dbt_info info;
    uint32_t flags = SET_PRELOCK_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();

    statistic_increment(table->in_use->status_var.ha_read_first_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = tokudb_active_index;

    error = cursor->c_getf_first(cursor, flags,
            SMART_DBT_CALLBACK, &info);
    error = handle_cursor_error(error,HA_ERR_END_OF_FILE,tokudb_active_index);

    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (tokudb_active_index != primary_key) && !(table->key_info[tokudb_active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }
    trx->stmt_progress.queried++;
    track_progress(thd);
    
cleanup:
    TOKUDB_DBUG_RETURN(error);
}

//
// Reads the last row from the active index (cursor) into buf, and advances cursor
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::index_last(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_last");
    invalidate_bulk_fetch();
    int error = 0;
    struct smart_dbt_info info;
    uint32_t flags = SET_PRELOCK_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();

    statistic_increment(table->in_use->status_var.ha_read_last_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = tokudb_active_index;

    error = cursor->c_getf_last(cursor, flags,
            SMART_DBT_CALLBACK, &info);
    error = handle_cursor_error(error,HA_ERR_END_OF_FILE,tokudb_active_index);
    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (tokudb_active_index != primary_key) && !(table->key_info[tokudb_active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }

    if (trx) {
        trx->stmt_progress.queried++;
    }
    track_progress(thd);
cleanup:
    TOKUDB_DBUG_RETURN(error);
}

//
// Initialize a scan of the table (which is why index_init is called on primary_key)
// Parameters:
//          scan - unused
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::rnd_init(bool scan) {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_init");
    int error = 0;
    range_lock_grabbed = false;
    error = index_init(MAX_KEY, 0);
    if (error) { goto cleanup;}

    if (scan) {
        error = prelock_range(NULL, NULL);
        if (error) { goto cleanup; }
    }
    //
    // only want to set range_lock_grabbed to true after index_init
    // successfully executed for two reasons:
    // 1) index_init will reset it to false anyway
    // 2) if it fails, we don't want prelocking on,
    //
    if (scan) { range_lock_grabbed = true; }
    error = 0;
cleanup:
    if (error) { 
        index_end();
        last_cursor_error = error; 
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// End a scan of the table
//
int ha_tokudb::rnd_end() {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_end");
    range_lock_grabbed = false;
    TOKUDB_DBUG_RETURN(index_end());
}


//
// Read the next row in a table scan
// Parameters:
//      [out]   buf - buffer for the next row, in MySQL format
// Returns:
//      0 on success
//      HA_ERR_END_OF_FILE if not found
//      error otherwise
//
int ha_tokudb::rnd_next(uchar * buf) {
    TOKUDB_DBUG_ENTER("ha_tokudb::ha_tokudb::rnd_next");
    statistic_increment(table->in_use->status_var.ha_read_rnd_next_count, &LOCK_status);
    int error = get_next(buf, 1);
    TOKUDB_DBUG_RETURN(error);
}


void ha_tokudb::track_progress(THD* thd) {
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (trx) {
        ulonglong num_written = trx->stmt_progress.inserted + trx->stmt_progress.updated + trx->stmt_progress.deleted;
        bool update_status = 
            (trx->stmt_progress.queried && tokudb_read_status_frequency && (trx->stmt_progress.queried % tokudb_read_status_frequency) == 0) ||
            (num_written && tokudb_write_status_frequency && (num_written % tokudb_write_status_frequency) == 0);
        if (update_status) {
            char *next_status = write_status_msg;
            bool first = true;
            int r;
            if (trx->stmt_progress.queried) {
                r = sprintf(next_status, "Queried about %llu row%s", trx->stmt_progress.queried, trx->stmt_progress.queried == 1 ? "" : "s"); 
                assert(r >= 0);
                next_status += r;
                first = false;
            }
            if (trx->stmt_progress.inserted) {
                if (trx->stmt_progress.using_loader) {
                    r = sprintf(next_status, "%sFetched about %llu row%s, loading data still remains", first ? "" : ", ", trx->stmt_progress.inserted, trx->stmt_progress.inserted == 1 ? "" : "s"); 
                }
                else {
                    r = sprintf(next_status, "%sInserted about %llu row%s", first ? "" : ", ", trx->stmt_progress.inserted, trx->stmt_progress.inserted == 1 ? "" : "s"); 
                }
                assert(r >= 0);
                next_status += r;
                first = false;
            }
            if (trx->stmt_progress.updated) {
                r = sprintf(next_status, "%sUpdated about %llu row%s", first ? "" : ", ", trx->stmt_progress.updated, trx->stmt_progress.updated == 1 ? "" : "s"); 
                assert(r >= 0);
                next_status += r;
                first = false;
            }
            if (trx->stmt_progress.deleted) {
                r = sprintf(next_status, "%sDeleted about %llu row%s", first ? "" : ", ", trx->stmt_progress.deleted, trx->stmt_progress.deleted == 1 ? "" : "s"); 
                assert(r >= 0);
                next_status += r;
                first = false;
            }
            if (!first)
                thd_proc_info(thd, write_status_msg);
        }
    }
}


DBT *ha_tokudb::get_pos(DBT * to, uchar * pos) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_pos");
    /* We don't need to set app_data here */
    memset((void *) to, 0, sizeof(*to));
    to->data = pos + sizeof(uint32_t);
    to->size = *(uint32_t *)pos;
    DBUG_DUMP("key", (const uchar *) to->data, to->size);
    DBUG_RETURN(to);
}

//
// Retrieves a row with based on the primary key saved in pos
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found
//      error otherwise
//
int ha_tokudb::rnd_pos(uchar * buf, uchar * pos) {
    TOKUDB_DBUG_ENTER("ha_tokudb::rnd_pos");
    DBT db_pos;
    int error = 0;
    struct smart_dbt_info info;
    bool old_unpack_entire_row = unpack_entire_row;
    DBT* key = get_pos(&db_pos, pos); 

    unpack_entire_row = true;
    statistic_increment(table->in_use->status_var.ha_read_rnd_count, &LOCK_status);
    tokudb_active_index = MAX_KEY;

    info.ha = this;
    info.buf = buf;
    info.keynr = primary_key;

    error = share->file->getf_set(share->file, transaction, 
            get_cursor_isolation_flags(lock.type, ha_thd()), 
            key, smart_dbt_callback_rowread_ptquery, &info);

    if (error == DB_NOTFOUND) {
        error = HA_ERR_KEY_NOT_FOUND;
        goto cleanup;
    }
cleanup:
    unpack_entire_row = old_unpack_entire_row;
    TOKUDB_DBUG_RETURN(error);
}

int ha_tokudb::prelock_range( const key_range *start_key, const key_range *end_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::prelock_range");
    THD* thd = ha_thd(); 

    int error = 0;
    DBT start_dbt_key;
    DBT end_dbt_key;
    uchar* start_key_buff  = prelocked_left_range;
    uchar* end_key_buff = prelocked_right_range;

    memset((void *) &start_dbt_key, 0, sizeof(start_dbt_key));
    memset((void *) &end_dbt_key, 0, sizeof(end_dbt_key));

    HANDLE_INVALID_CURSOR();
    if (start_key) {
        switch (start_key->flag) {
        case HA_READ_AFTER_KEY:
            pack_key(&start_dbt_key, tokudb_active_index, start_key_buff, start_key->key, start_key->length, COL_POS_INF);
            break;
        default:
            pack_key(&start_dbt_key, tokudb_active_index, start_key_buff, start_key->key, start_key->length, COL_NEG_INF);
            break;
        }
        prelocked_left_range_size = start_dbt_key.size;
    }
    else {
        prelocked_left_range_size = 0;
    }

    if (end_key) {
        switch (end_key->flag) {
        case HA_READ_BEFORE_KEY:
            pack_key(&end_dbt_key, tokudb_active_index, end_key_buff, end_key->key, end_key->length, COL_NEG_INF);
            break;
        default:
            pack_key(&end_dbt_key, tokudb_active_index, end_key_buff, end_key->key, end_key->length, COL_POS_INF);
            break;
        }        
        prelocked_right_range_size = end_dbt_key.size;
    }
    else {
        prelocked_right_range_size = 0;
    }

    error = cursor->c_pre_acquire_range_lock(
        cursor, 
        start_key ? &start_dbt_key : share->key_file[tokudb_active_index]->dbt_neg_infty(), 
        end_key ? &end_dbt_key : share->key_file[tokudb_active_index]->dbt_pos_infty()
        );
    if (error){ 
        last_cursor_error = error;
        //
        // cursor should be initialized here, but in case it is not, we still check
        //
        if (cursor) {
            int r = cursor->c_close(cursor);
            assert(r==0);
            cursor = NULL;
        }
        goto cleanup; 
    }

    //
    // at this point, determine if we will be doing bulk fetch
    // as of now, only do it if we are doing a select
    //
    doing_bulk_fetch = (thd_sql_command(thd) == SQLCOM_SELECT);
    bulk_fetch_iteration = 0;
    rows_fetched_using_bulk_fetch = 0;

cleanup:
    TOKUDB_DBUG_RETURN(error);
}

//
// Prelock range if possible, start_key is leftmost, end_key is rightmost
// whether scanning forward or backward.  This function is called by MySQL
// for backward range queries (in QUICK_SELECT_DESC::get_next). 
// Forward scans use read_range_first()/read_range_next().
//
int ha_tokudb::prepare_range_scan( const key_range *start_key, const key_range *end_key) {
    int error = prelock_range(start_key, end_key);
    if (!error) {
        range_lock_grabbed = true;
    }
    return error;
}

int ha_tokudb::read_range_first(
    const key_range *start_key,
    const key_range *end_key,
    bool eq_range, 
    bool sorted) 
{
    int error = prelock_range(start_key, end_key);
    if (error) { goto cleanup; }
    range_lock_grabbed = true;
    
    error = handler::read_range_first(start_key, end_key, eq_range, sorted);
cleanup:
    return error;
}

int ha_tokudb::read_range_next()
{
    TOKUDB_DBUG_ENTER("ha_tokudb::read_range_next");
    int error;
    error = handler::read_range_next();
    if (error) {
        range_lock_grabbed = false;
    }
    TOKUDB_DBUG_RETURN(error);
}



/*
  Set a reference to the current record in (ref,ref_length).

  SYNOPSIS
  ha_tokudb::position()
  record                      The current record buffer

  DESCRIPTION
  The BDB handler stores the primary key in (ref,ref_length).
  There is either an explicit primary key, or an implicit (hidden)
  primary key.
  During open(), 'ref_length' is calculated as the maximum primary
  key length. When an actual key is shorter than that, the rest of
  the buffer must be cleared out. The row cannot be identified, if
  garbage follows behind the end of the key. There is no length
  field for the current key, so that the whole ref_length is used
  for comparison.

  RETURN
  nothing
*/
void ha_tokudb::position(const uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::position");
    DBT key;
    if (hidden_primary_key) {
        DBUG_ASSERT(ref_length == (TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH + sizeof(uint32_t)));
        memcpy_fixed(ref + sizeof(uint32_t), current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        *(uint32_t *)ref = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    } 
    else {
        bool has_null;
        //
        // save the data
        //
        create_dbt_key_from_table(&key, primary_key, ref + sizeof(uint32_t), record, &has_null);
        //
        // save the size of data in the first four bytes of ref
        //
        memcpy(ref, &key.size, sizeof(uint32_t));
    }
    DBUG_VOID_RETURN;
}

//
// Per InnoDB: Returns statistics information of the table to the MySQL interpreter,
// in various fields of the handle object. 
// Return:
//      0, always success
//
int ha_tokudb::info(uint flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::info %p %d %lld", this, flag, (long long) share->rows);
    int error;
    bool key_file_lock_taken = false;
    DB_TXN* txn = NULL;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    DB_BTREE_STAT64 dict_stats;
    if (flag & HA_STATUS_VARIABLE) {
        // Just to get optimizations right
        stats.records = share->rows + share->rows_from_locked_table;
        if (stats.records == 0) {
            stats.records++;
        }
        stats.deleted = 0;
        if (!(flag & HA_STATUS_NO_LOCK)) {
            uint64_t num_rows = 0;
            TOKU_DB_FRAGMENTATION_S frag_info;
            memset(&frag_info, 0, sizeof frag_info);

            error = db_env->txn_begin(db_env, NULL, &txn, DB_READ_UNCOMMITTED);
            if (error) { goto cleanup; }

            share_key_file_rdlock(share);
            key_file_lock_taken = true;
            // we should always have a primary key
            assert(share->file != NULL);

            error = estimate_num_rows(share->file,&num_rows, txn);
            if (error == 0) {
                share->rows = num_rows;
                stats.records = num_rows;
                if (stats.records == 0) {
                    stats.records++;
                }
            }
            else {
                goto cleanup;
            }
            error = share->file->get_fragmentation(
                share->file,
                &frag_info
                );
            if (error) { goto cleanup; }
            stats.delete_length = frag_info.unused_bytes;

            error = share->file->stat64(
                share->file, 
                txn, 
                &dict_stats
                );
            if (error) { goto cleanup; }
            
            stats.create_time = dict_stats.bt_create_time_sec;
            stats.update_time = dict_stats.bt_modify_time_sec;
            stats.check_time = dict_stats.bt_verify_time_sec;
            stats.data_file_length = dict_stats.bt_dsize;
            if (hidden_primary_key) {
                //
                // in this case, we have a hidden primary key, do not
                // want to report space taken up by the hidden primary key to the user
                //
                uint64_t hpk_space = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH*dict_stats.bt_ndata;
                stats.data_file_length = (hpk_space > stats.data_file_length) ? 0 : stats.data_file_length - hpk_space;
            }
            else {
                //
                // one infinity byte per key needs to be subtracted
                //
                uint64_t inf_byte_space = dict_stats.bt_ndata;
                stats.data_file_length = (inf_byte_space > stats.data_file_length) ? 0 : stats.data_file_length - inf_byte_space;
            }

            stats.mean_rec_length = stats.records ? (ulong)(stats.data_file_length/stats.records) : 0;
            stats.index_file_length = 0;
            // curr_num_DBs is the number of keys we have, according
            // to the mysql layer. if drop index is running concurrently
            // with info() (it can, because info does not take table locks),
            // then it could be the case that one of the dbs was dropped
            // and set to NULL before mysql was able to set table->s->keys
            // accordingly. 
            //
            // we should just ignore any DB * that is NULL. 
            //
            // this solution is much simpler than trying to maintain an 
            // accurate number of valid keys at the handlerton layer.
            for (uint i = 0; i < curr_num_DBs; i++) {
                // skip the primary key, skip dropped indexes
                if (i == primary_key || share->key_file[i] == NULL) {
                    continue;
                }
                error = share->key_file[i]->stat64(
                    share->key_file[i], 
                    txn, 
                    &dict_stats
                    );
                if (error) { goto cleanup; }
                stats.index_file_length += dict_stats.bt_dsize;

                error = share->file->get_fragmentation(
                    share->file,
                    &frag_info
                    );
                if (error) { goto cleanup; }
                stats.delete_length += frag_info.unused_bytes;
            }
        }
    }
    if ((flag & HA_STATUS_CONST)) {
        stats.max_data_file_length=  9223372036854775807ULL;
        for (uint i = 0; i < table_share->keys; i++) {
            table->key_info[i].rec_per_key[table->key_info[i].key_parts - 1] = 0;
        }
    }
    /* Don't return key if we got an error for the internal primary key */
    if (flag & HA_STATUS_ERRKEY && last_dup_key < table_share->keys) {
        errkey = last_dup_key;
    }    
    if (flag & HA_STATUS_AUTO && table->found_next_number_field) {        
        THD *thd= table->in_use;
        struct system_variables *variables= &thd->variables;
        stats.auto_increment_value = share->last_auto_increment + variables->auto_increment_increment;
    }
    error = 0;
cleanup:
    if (key_file_lock_taken) {
        share_key_file_unlock(share);
    }
    if (txn != NULL) {
        commit_txn(txn, DB_TXN_NOSYNC);
        txn = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

//
//  Per InnoDB: Tells something additional to the handler about how to do things.
//
int ha_tokudb::extra(enum ha_extra_function operation) {
    TOKUDB_DBUG_ENTER("extra %p %d", this, operation);
    switch (operation) {
    case HA_EXTRA_RESET_STATE:
        reset();
        break;
    case HA_EXTRA_KEYREAD:
        key_read = 1;           // Query satisfied with key
        break;
    case HA_EXTRA_NO_KEYREAD:
        key_read = 0;
        break;
    case HA_EXTRA_IGNORE_DUP_KEY:
        using_ignore = 1;
        break;
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
        using_ignore = 0;
        break;
    default:
        break;
    }
    TOKUDB_DBUG_RETURN(0);
}

int ha_tokudb::reset(void) {
    TOKUDB_DBUG_ENTER("ha_tokudb::reset");
    key_read = 0;
    using_ignore = 0;
    TOKUDB_DBUG_RETURN(0);
}


//
// helper function that iterates through all DB's 
// and grabs a lock (either read or write, but not both)
// Parameters:
//      [in]    trans - transaction to be used to pre acquire the lock
//              lt - type of lock to get, either lock_read or lock_write
//  Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::acquire_table_lock (DB_TXN* trans, TABLE_LOCK_TYPE lt) {
    int error = ENOSYS;
    rw_rdlock(&share->num_DBs_lock);
    uint curr_num_DBs = share->num_DBs;
    if (lt == lock_read) {
        error = 0;
        goto cleanup;
    }
    else if (lt == lock_write) {
        if (tokudb_debug & TOKUDB_DEBUG_LOCK)
            TOKUDB_TRACE("%s\n", __FUNCTION__);
        for (uint i = 0; i < curr_num_DBs; i++) {
            DB* db = share->key_file[i];
            error = db->pre_acquire_table_lock(db, trans);
            if (error == EINVAL) 
                TOKUDB_TRACE("%s %d db=%p trans=%p\n", __FUNCTION__, i, db, trans);
            if (error) break;
        }
        if (tokudb_debug & TOKUDB_DEBUG_LOCK)
            TOKUDB_TRACE("%s error=%d\n", __FUNCTION__, error);
        if (error) goto cleanup;
    }
    else {
        error = ENOSYS;
        goto cleanup;
    }

    error = 0;
cleanup:
    rw_unlock(&share->num_DBs_lock);
    return error;
}


int ha_tokudb::create_txn(THD* thd, tokudb_trx_data* trx) {
    int error;
    ulong tx_isolation = thd_tx_isolation(thd);
    HA_TOKU_ISO_LEVEL toku_iso_level = tx_to_toku_iso(tx_isolation);
    bool is_autocommit = !thd_test_options(
            thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN);

    /* First table lock, start transaction */
    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) && 
         !trx->all &&
         (thd_sql_command(thd) != SQLCOM_CREATE_TABLE) &&
         (thd_sql_command(thd) != SQLCOM_DROP_TABLE) &&
         (thd_sql_command(thd) != SQLCOM_DROP_INDEX) &&
         (thd_sql_command(thd) != SQLCOM_CREATE_INDEX) &&
         (thd_sql_command(thd) != SQLCOM_ALTER_TABLE)) {
        /* QQQ We have to start a master transaction */
        // DBUG_PRINT("trans", ("starting transaction all "));
        if ((error = db_env->txn_begin(db_env, NULL, &trx->all, toku_iso_to_txn_flag(toku_iso_level)))) {
            trx->tokudb_lock_count--;      // We didn't get the lock
            goto cleanup;
        }
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("just created master:%p\n", trx->all);
        }
        trx->sp_level = trx->all;
        trans_register_ha(thd, true, tokudb_hton);
    }
    DBUG_PRINT("trans", ("starting transaction stmt"));
    if (trx->stmt) { 
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("warning:stmt=%p\n", trx->stmt);
        }
    }
    uint32_t txn_begin_flags;
    if (trx->all == NULL) {
        txn_begin_flags = toku_iso_to_txn_flag(toku_iso_level);
        if (txn_begin_flags == 0 && is_autocommit && thd_sql_command(thd) == SQLCOM_SELECT) {
            txn_begin_flags = DB_TXN_SNAPSHOT;
        }
    }
    else {
        txn_begin_flags = DB_INHERIT_ISOLATION;
    }
    if ((error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, txn_begin_flags))) {
        /* We leave the possible master transaction open */
        trx->tokudb_lock_count--;  // We didn't get the lock
        goto cleanup;
    }
    trx->sub_sp_level = trx->stmt;
    if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("just created stmt:%p:%p\n", trx->sp_level, trx->stmt);
    }
    reset_stmt_progress(&trx->stmt_progress);
    trans_register_ha(thd, false, tokudb_hton);
cleanup:
    return error;
}


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement to be able to rollback the statement.
  If not, we have to start a master transaction if there doesn't exist
  one from before.
*/
//
// Parameters:
//      [in]    thd - handle to the user thread
//              lock_type - the type of lock
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::external_lock(THD * thd, int lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::external_lock cmd=%d %d", thd_sql_command(thd), lock_type);
    if (tokudb_debug & TOKUDB_DEBUG_LOCK)
        TOKUDB_TRACE("%s cmd=%d %d\n", __FUNCTION__, thd_sql_command(thd), lock_type);

    int error = 0;
    tokudb_trx_data *trx = NULL;
    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        error = create_tokudb_trx_data_instance(&trx);
        if (error) { goto cleanup; }
        thd_data_set(thd, tokudb_hton->slot, trx);
    }
    if (trx->all == NULL) {
        trx->sp_level = NULL;
    }
    if (lock_type != F_UNLCK) {
        is_fast_alter_running = false;
        use_write_locks = false;
        if (lock_type == F_WRLCK)
            use_write_locks = true;
        if (!trx->tokudb_lock_count++) {
            DBUG_ASSERT(trx->stmt == 0);
            transaction = NULL;    // Safety
            error = create_txn(thd, trx);
            if (error) {
                goto cleanup;
            }
        }
        assert(thd->in_sub_stmt == 0);
        transaction = trx->sub_sp_level;
    }
    else {
        pthread_mutex_lock(&share->mutex);
        // hate dealing with comparison of signed vs unsigned, so doing this
        if (deleted_rows > added_rows && share->rows < (deleted_rows - added_rows)) {
            share->rows = 0;
        }
        else {
            share->rows += (added_rows - deleted_rows);
        }
        pthread_mutex_unlock(&share->mutex);
        added_rows = 0;
        deleted_rows = 0;
        share->rows_from_locked_table = 0;
        if (trx->tokudb_lock_count > 0 && !--trx->tokudb_lock_count) {
            if (trx->stmt) {
                /*
                   F_UNLCK is done without a transaction commit / rollback.
                   This happens if the thread didn't update any rows
                   We must in this case commit the work to keep the row locks
                 */
                DBUG_PRINT("trans", ("commiting non-updating transaction"));
                reset_stmt_progress(&trx->stmt_progress);
                if (!is_fast_alter_running) {
                    //
                    // TODO: (Zardosht), remove trx->should_abort as part of
                    // #5481
                    //
                    if (trx->should_abort) {
                        abort_txn(trx->stmt);
                        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
                            TOKUDB_TRACE("rollback:%p\n", trx->stmt);
                        }
                    }
                    else {
                        commit_txn(trx->stmt, 0);
                        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
                            TOKUDB_TRACE("commit:%p:%d\n", trx->stmt, error);
                        }
                    }
                    trx->stmt = NULL;
                    trx->sub_sp_level = NULL;
                    trx->should_abort = false;
                }
            }
        }
        transaction = NULL;
        is_fast_alter_running = false;
    }
cleanup:
    if (tokudb_debug & TOKUDB_DEBUG_LOCK)
        TOKUDB_TRACE("%s error=%d\n", __FUNCTION__, error);
    TOKUDB_DBUG_RETURN(error);
}


/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_tokudb::start_stmt(THD * thd, thr_lock_type lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::start_stmt cmd=%d %d", thd_sql_command(thd), lock_type);
    int error = 0;


    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    DBUG_ASSERT(trx);
    /*
       note that trx->stmt may have been already initialized as start_stmt()
       is called for *each table* not for each storage engine,
       and there could be many bdb tables referenced in the query
     */
    if (!trx->stmt) {
        error = create_txn(thd, trx);
        if (error) {
            goto cleanup;
        }
    }
    else {
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("trx->stmt already existed\n");
        }
    }
    //
    // we know we are in lock tables
    // attempt to grab a table lock
    // if fail, continue, do not return error
    // This is because a failure ok, it simply means
    // another active transaction has some locks.
    // That other transaction modify this table
    // until it is unlocked, therefore having acquire_table_lock
    // potentially grab some locks but not all is ok.
    //
    if (lock.type <= TL_READ_NO_INSERT) {
        acquire_table_lock(trx->sub_sp_level,lock_read);
    }
    else {
        if (!(thd_sql_command(thd) == SQLCOM_CREATE_INDEX ||
            thd_sql_command(thd) == SQLCOM_ALTER_TABLE ||
            thd_sql_command(thd) == SQLCOM_DROP_INDEX ||
            thd_sql_command(thd) == SQLCOM_TRUNCATE)) {
            acquire_table_lock(trx->sub_sp_level,lock_write);
        }
    }    
    if (added_rows > deleted_rows) {
        share->rows_from_locked_table = added_rows - deleted_rows;
    }
    transaction = trx->sub_sp_level;
    trans_register_ha(thd, false, tokudb_hton);
cleanup:
    TOKUDB_DBUG_RETURN(error);
}


uint32_t ha_tokudb::get_cursor_isolation_flags(enum thr_lock_type lock_type, THD* thd) {
    uint sql_command = thd_sql_command(thd);
    bool in_lock_tables = thd_in_lock_tables(thd);

    //
    // following InnoDB's lead and having checksum command use a snapshot read if told
    //
    if (sql_command == SQLCOM_CHECKSUM) {
        return 0;
    }
    else if ((lock_type == TL_READ && in_lock_tables) || 
             (lock_type == TL_READ_HIGH_PRIORITY && in_lock_tables) || 
             sql_command != SQLCOM_SELECT ||
             (sql_command == SQLCOM_SELECT && lock_type >= TL_WRITE_ALLOW_WRITE)) { // select for update 
        return DB_SERIALIZABLE;
    }
    else {
        return 0;
    }
}

/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Tokudb DB changes all WRITE locks to TL_WRITE_ALLOW_WRITE (which
  signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.
*/

THR_LOCK_DATA **ha_tokudb::store_lock(THD * thd, THR_LOCK_DATA ** to, enum thr_lock_type lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::store_lock, lock_type=%d cmd=%d", lock_type, thd_sql_command(thd));
    if (tokudb_debug & TOKUDB_DEBUG_LOCK) {
        TOKUDB_TRACE("%s lock_type=%d cmd=%d\n", __FUNCTION__, lock_type, thd_sql_command(thd));
    }

    if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
        // if creating a hot index
        if (get_create_index_online(thd) && thd_sql_command(thd)== SQLCOM_CREATE_INDEX) {
            rw_rdlock(&share->num_DBs_lock);
            if (share->num_DBs == (table->s->keys + test(hidden_primary_key))) {
                lock_type = TL_WRITE_ALLOW_WRITE;
            }
            lock.type = lock_type;
            rw_unlock(&share->num_DBs_lock);
        } 

        // 5.5 supports reads concurrent with alter table.  just use the default lock type.
#if MYSQL_VERSION_ID < 50500
        else if (thd_sql_command(thd)== SQLCOM_CREATE_INDEX || 
                 thd_sql_command(thd)== SQLCOM_ALTER_TABLE ||
                 thd_sql_command(thd)== SQLCOM_DROP_INDEX) {
            // force alter table to lock out other readers
            lock_type = TL_WRITE;
            lock.type = lock_type;
        }
#endif
        else {
            // If we are not doing a LOCK TABLE, then allow multiple writers
            if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) && 
                !thd->in_lock_tables && thd_sql_command(thd) != SQLCOM_TRUNCATE && !thd_tablespace_op(thd)) {
                lock_type = TL_WRITE_ALLOW_WRITE;
            }
            lock.type = lock_type;
        }
    }
    *to++ = &lock;
    if (tokudb_debug & TOKUDB_DEBUG_LOCK)
        TOKUDB_TRACE("%s lock_type=%d\n", __FUNCTION__, lock_type);
    DBUG_RETURN(to);
}

static inline enum row_type
compression_method_to_row_type(enum toku_compression_method method)
{
    switch (method) {
#if TOKU_INCLUDE_ROW_TYPE_COMPRESSION
    case TOKU_NO_COMPRESSION:
        return ROW_TYPE_TOKU_UNCOMPRESSED;
    case TOKU_ZLIB_METHOD:
        return ROW_TYPE_TOKU_ZLIB;
    case TOKU_QUICKLZ_METHOD:
        return ROW_TYPE_TOKU_QUICKLZ;
    case TOKU_LZMA_METHOD:
        return ROW_TYPE_TOKU_LZMA;
    case TOKU_FAST_COMPRESSION_METHOD:
        return ROW_TYPE_TOKU_FAST;
    case TOKU_SMALL_COMPRESSION_METHOD:
        return ROW_TYPE_TOKU_SMALL;
#endif
    case TOKU_DEFAULT_COMPRESSION_METHOD:
        return ROW_TYPE_DEFAULT;
    default:
        assert(false);
    }
}

static enum row_type
get_row_type_for_key(DB *file)
{
    enum toku_compression_method method;
    int r = file->get_compression_method(file, &method);
    assert(r == 0);
    return compression_method_to_row_type(method);
}

enum row_type
#if MYSQL_VERSION_ID >= 50521
ha_tokudb::get_row_type(void) const
#else
ha_tokudb::get_row_type(void)
#endif
{
    return get_row_type_for_key(share->file);
}

static inline enum toku_compression_method
row_type_to_compression_method(enum row_type type)
{
    switch (type) {
#if TOKU_INCLUDE_ROW_TYPE_COMPRESSION
    case ROW_TYPE_TOKU_UNCOMPRESSED:
        return TOKU_NO_COMPRESSION;
    case ROW_TYPE_TOKU_ZLIB:
        return TOKU_ZLIB_METHOD;
    case ROW_TYPE_TOKU_QUICKLZ:
        return TOKU_QUICKLZ_METHOD;
    case ROW_TYPE_TOKU_LZMA:
        return TOKU_LZMA_METHOD;
    case ROW_TYPE_TOKU_SMALL:
        return TOKU_SMALL_COMPRESSION_METHOD;
    case ROW_TYPE_TOKU_FAST:
        return TOKU_FAST_COMPRESSION_METHOD;
#endif
    default:
        DBUG_PRINT("info", ("Ignoring ROW_FORMAT not used by TokuDB, using TOKUDB_FAST by default instead"));
    case ROW_TYPE_DEFAULT:
        return TOKU_DEFAULT_COMPRESSION_METHOD;
    }
}

static int create_sub_table(
    const char *table_name, 
    DBT* row_descriptor, 
    DB_TXN* txn, 
    uint32_t block_size, 
    uint32_t read_block_size,
    enum toku_compression_method compression_method,
    bool is_hot_index
    ) 
{
    TOKUDB_DBUG_ENTER("create_sub_table");
    int error;
    DB *file = NULL;
    uint32_t create_flags;
    
    
    error = db_create(&file, db_env, 0);
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when creating table", error));
        my_errno = error;
        goto exit;
    }
        

    if (block_size != 0) {
        error = file->set_pagesize(file, block_size);
        if (error != 0) {
            DBUG_PRINT("error", ("Got error: %d when setting block size %u for table '%s'", error, block_size, table_name));
            goto exit;
        }
    }
    if (read_block_size != 0) {
        error = file->set_readpagesize(file, read_block_size);
        if (error != 0) {
            DBUG_PRINT("error", ("Got error: %d when setting read block size %u for table '%s'", error, read_block_size, table_name));
            goto exit;
        }
    }
    error = file->set_compression_method(file, compression_method);
    if (error != 0) {
        DBUG_PRINT("error", ("Got error: %d when setting compression type %u for table '%s'", error, compression_method, table_name));
        goto exit;
    }

    create_flags = DB_THREAD | DB_CREATE | DB_EXCL | (is_hot_index ? DB_IS_HOT_INDEX : 0);    
    error = file->open(file, txn, table_name, NULL, DB_BTREE, create_flags, my_umask);
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when opening table '%s'", error, table_name));
        goto exit;
    } 

    error = file->change_descriptor(file, txn, row_descriptor, (is_hot_index ? DB_IS_HOT_INDEX | DB_UPDATE_CMP_DESCRIPTOR : DB_UPDATE_CMP_DESCRIPTOR));
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when setting row descriptor for table '%s'", error, table_name));
        goto exit;
    }

    error = 0;
exit:
    if (file) {
        int r = file->close(file, 0);
        assert(r==0);
    }
    TOKUDB_DBUG_RETURN(error);
}

void ha_tokudb::update_create_info(HA_CREATE_INFO* create_info) {
    if (share->has_auto_inc) {
        info(HA_STATUS_AUTO);
        if (!(create_info->used_fields & HA_CREATE_USED_AUTO) ||
            create_info->auto_increment_value < stats.auto_increment_value) {
            create_info->auto_increment_value = stats.auto_increment_value;
        }
    }
    if (!(create_info->used_fields & HA_CREATE_USED_ROW_FORMAT)) {
        // show create table asks us to update this create_info, this makes it
        // so we'll always show what compression type we're using
        create_info->row_type = get_row_type();
    }
}

//
// removes key name from status.tokudb.
// needed for when we are dropping indexes, so that 
// during drop table, we do not attempt to remove already dropped
// indexes because we did not keep status.tokudb in sync with list of indexes.
//
int ha_tokudb::remove_key_name_from_status(DB* status_block, char* key_name, DB_TXN* txn) {
    int error;
    uchar status_key_info[FN_REFLEN + sizeof(HA_METADATA_KEY)];
    HA_METADATA_KEY md_key = hatoku_key_name;
    memcpy(status_key_info, &md_key, sizeof(HA_METADATA_KEY));
    //
    // put index name in status.tokudb
    // 
    memcpy(
        status_key_info + sizeof(HA_METADATA_KEY), 
        key_name, 
        strlen(key_name) + 1
        );
    error = remove_metadata(
        status_block,
        status_key_info,
        sizeof(HA_METADATA_KEY) + strlen(key_name) + 1,
        txn
        );
    return error;
}

//
// writes the key name in status.tokudb, so that we may later delete or rename
// the dictionary associated with key_name
//
int ha_tokudb::write_key_name_to_status(DB* status_block, char* key_name, DB_TXN* txn) {
    int error;
    uchar status_key_info[FN_REFLEN + sizeof(HA_METADATA_KEY)];
    HA_METADATA_KEY md_key = hatoku_key_name;
    memcpy(status_key_info, &md_key, sizeof(HA_METADATA_KEY));
    //
    // put index name in status.tokudb
    // 
    memcpy(
        status_key_info + sizeof(HA_METADATA_KEY), 
        key_name, 
        strlen(key_name) + 1
        );
    error = write_metadata(
        status_block,
        status_key_info,
        sizeof(HA_METADATA_KEY) + strlen(key_name) + 1,
        NULL,
        0,
        txn
        );
    return error;
}

//
// some tracing moved out of ha_tokudb::create, because ::create was getting cluttered
//
void ha_tokudb::trace_create_table_info(const char *name, TABLE * form) {
    uint i;
    //
    // tracing information about what type of table we are creating
    //
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        for (i = 0; i < form->s->fields; i++) {
            Field *field = form->s->field[i];
            TOKUDB_TRACE("field:%d:%s:type=%d:flags=%x\n", i, field->field_name, field->type(), field->flags);
        }
        for (i = 0; i < form->s->keys; i++) {
            KEY *key = &form->s->key_info[i];
            TOKUDB_TRACE("key:%d:%s:%d\n", i, key->name, key->key_parts);
            uint p;
            for (p = 0; p < key->key_parts; p++) {
                KEY_PART_INFO *key_part = &key->key_part[p];
                Field *field = key_part->field;
                TOKUDB_TRACE("key:%d:%d:length=%d:%s:type=%d:flags=%x\n",
                             i, p, key_part->length, field->field_name, field->type(), field->flags);
            }
        }
    }
}

static uint32_t get_max_desc_size(KEY_AND_COL_INFO* kc_info, TABLE* form) {
    uint32_t max_row_desc_buff_size;
    max_row_desc_buff_size = 2*(form->s->fields * 6)+10; // upper bound of key comparison descriptor
    max_row_desc_buff_size += get_max_secondary_key_pack_desc_size(kc_info); // upper bound for sec. key part
    max_row_desc_buff_size += get_max_clustering_val_pack_desc_size(form->s); // upper bound for clustering val part
    return max_row_desc_buff_size;
}

static uint32_t create_secondary_key_descriptor(
    uchar* buf,
    KEY* key_info,
    KEY* prim_key,
    uint hpk,
    TABLE* form,
    uint primary_key,
    uint32_t keynr,
    KEY_AND_COL_INFO* kc_info    
    ) 
{
    uchar* ptr = NULL;

    ptr = buf;
    ptr += create_toku_key_descriptor(
        ptr,
        false,
        key_info,
        hpk,
        prim_key
        );

    ptr += create_toku_secondary_key_pack_descriptor(
        ptr,
        hpk,
        primary_key,
        form->s,
        form,
        kc_info,
        key_info,
        prim_key
        );

    ptr += create_toku_clustering_val_pack_descriptor(
        ptr,
        primary_key,
        form->s,
        kc_info,
        keynr,
        key_info->flags & HA_CLUSTERING
        );
    return ptr - buf;
}


//
// creates dictionary for secondary index, with key description key_info, all using txn
//
int ha_tokudb::create_secondary_dictionary(
    const char* name, TABLE* form, 
    KEY* key_info, 
    DB_TXN* txn, 
    KEY_AND_COL_INFO* kc_info, 
    uint32_t keynr,
    bool is_hot_index,
    enum row_type row_type
    ) 
{
    int error;
    DBT row_descriptor;
    uchar* row_desc_buff = NULL;
    char* newname = NULL;
    KEY* prim_key = NULL;
    char dict_name[MAX_DICT_NAME_LEN];
    uint32_t max_row_desc_buff_size;
    uint hpk= (form->s->primary_key >= MAX_KEY) ? TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH : 0;
    uint32_t block_size;
    uint32_t read_block_size;
    THD* thd = ha_thd();

    memset(&row_descriptor, 0, sizeof(row_descriptor));
    
    max_row_desc_buff_size = get_max_desc_size(kc_info,form);

    row_desc_buff = (uchar *)my_malloc(max_row_desc_buff_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}

    newname = (char *)my_malloc(get_max_dict_name_path_length(name),MYF(MY_WME));
    if (newname == NULL){ error = ENOMEM; goto cleanup;}

    sprintf(dict_name, "key-%s", key_info->name);
    make_name(newname, name, dict_name);

    prim_key = (hpk) ? NULL : &form->s->key_info[primary_key];

    //
    // setup the row descriptor
    //
    row_descriptor.data = row_desc_buff;
    //
    // save data necessary for key comparisons
    //
    row_descriptor.size = create_secondary_key_descriptor(
        row_desc_buff,
        key_info,
        prim_key,
        hpk,
        form,
        primary_key,
        keynr,
        kc_info    
        );
    assert(row_descriptor.size <= max_row_desc_buff_size);

    block_size = key_info->block_size << 10;
    if (block_size == 0) {
        block_size = get_tokudb_block_size(thd);
    }
    read_block_size = get_tokudb_read_block_size(thd);

    error = create_sub_table(newname, &row_descriptor, txn, block_size, read_block_size, row_type_to_compression_method(row_type), is_hot_index);
cleanup:    
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}


static uint32_t create_main_key_descriptor(
    uchar* buf,
    KEY* prim_key,
    uint hpk,
    uint primary_key,
    TABLE* form,
    KEY_AND_COL_INFO* kc_info
    ) 
{
    uchar* ptr = buf;
    ptr += create_toku_key_descriptor(
        ptr, 
        hpk,
        prim_key,
        false,
        NULL
        );
    
    ptr += create_toku_main_key_pack_descriptor(
        ptr
        );

    ptr += create_toku_clustering_val_pack_descriptor(
        ptr,
        primary_key,
        form->s,
        kc_info,
        primary_key,
        false
        );
    return ptr - buf;
}

//
// create and close the main dictionarr with name of "name" using table form, all within
// transaction txn.
//
int ha_tokudb::create_main_dictionary(const char* name, TABLE* form, DB_TXN* txn, KEY_AND_COL_INFO* kc_info, enum row_type row_type) {
    int error;
    DBT row_descriptor;
    uchar* row_desc_buff = NULL;
    char* newname = NULL;
    KEY* prim_key = NULL;
    uint32_t max_row_desc_buff_size;
    uint hpk= (form->s->primary_key >= MAX_KEY) ? TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH : 0;
    uint32_t block_size;
    uint32_t read_block_size;
    THD* thd = ha_thd();

    memset(&row_descriptor, 0, sizeof(row_descriptor));
    max_row_desc_buff_size = get_max_desc_size(kc_info, form);

    row_desc_buff = (uchar *)my_malloc(max_row_desc_buff_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}

    newname = (char *)my_malloc(get_max_dict_name_path_length(name),MYF(MY_WME));
    if (newname == NULL){ error = ENOMEM; goto cleanup;}

    make_name(newname, name, "main");

    prim_key = (hpk) ? NULL : &form->s->key_info[primary_key];

    //
    // setup the row descriptor
    //
    row_descriptor.data = row_desc_buff;
    //
    // save data necessary for key comparisons
    //
    row_descriptor.size = create_main_key_descriptor(
        row_desc_buff,
        prim_key,
        hpk,
        primary_key,
        form,
        kc_info
        );
    assert(row_descriptor.size <= max_row_desc_buff_size);

    block_size = 0;
    if (prim_key)
        block_size = prim_key->block_size << 10;
    if (block_size == 0) {
        block_size = get_tokudb_block_size(thd);
    }
    read_block_size = get_tokudb_read_block_size(thd);

    /* Create the main table that will hold the real rows */
    error = create_sub_table(newname, &row_descriptor, txn, block_size, read_block_size, row_type_to_compression_method(row_type), false);
cleanup:    
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

static inline enum row_type
row_format_to_row_type(srv_row_format_t row_format)
{
#if TOKU_INCLUDE_ROW_TYPE_COMPRESSION
    switch (row_format) {
    case SRV_ROW_FORMAT_UNCOMPRESSED:
        return ROW_TYPE_TOKU_UNCOMPRESSED;
    case SRV_ROW_FORMAT_ZLIB:
        return ROW_TYPE_TOKU_ZLIB;
    case SRV_ROW_FORMAT_QUICKLZ:
        return ROW_TYPE_TOKU_QUICKLZ;
    case SRV_ROW_FORMAT_LZMA:
        return ROW_TYPE_TOKU_LZMA;
    case SRV_ROW_FORMAT_SMALL:
        return ROW_TYPE_TOKU_SMALL;
    case SRV_ROW_FORMAT_FAST:
        return ROW_TYPE_TOKU_FAST;
    case SRV_ROW_FORMAT_DEFAULT:
        return ROW_TYPE_DEFAULT;
    }
    assert(0);
#endif
    return ROW_TYPE_DEFAULT;;
}

//
// Creates a new table
// Parameters:
//      [in]    name - table name
//      [in]    form - info on table, columns and indexes
//      [in]    create_info - more info on table, CURRENTLY UNUSED
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::create(const char *name, TABLE * form, HA_CREATE_INFO * create_info) {
    TOKUDB_DBUG_ENTER("ha_tokudb::create");
    int error;
    DB *status_block = NULL;
    uint version;
    uint capabilities;
    DB_TXN* txn = NULL;
    bool do_commit = false;
    char* newname = NULL;
    KEY_AND_COL_INFO kc_info;
    tokudb_trx_data *trx = NULL;
    THD* thd = ha_thd();
    bool create_from_engine= (create_info->table_options & HA_OPTION_CREATE_FROM_ENGINE);
    memset(&kc_info, 0, sizeof(kc_info));

    pthread_mutex_lock(&tokudb_meta_mutex);

    trx = (tokudb_trx_data *) thd_data_get(ha_thd(), tokudb_hton->slot);

    const enum row_type row_type = ((create_info->used_fields & HA_CREATE_USED_ROW_FORMAT)
                                    ? create_info->row_type
                                    : row_format_to_row_type(get_row_format(thd)));

    if (create_from_engine) {
        // table already exists, nothing to do
        error = 0;
        goto cleanup;
    }

    newname = (char *)my_malloc(get_max_dict_name_path_length(name),MYF(MY_WME));
    if (newname == NULL){ error = ENOMEM; goto cleanup;}

    if (thd_sql_command(thd) == SQLCOM_CREATE_TABLE && trx && trx->sub_sp_level) {
        txn = trx->sub_sp_level;
    }
    else {
        do_commit = true;
        error = db_env->txn_begin(db_env, 0, &txn, 0);
        if (error) { goto cleanup; }        
    }
    
    primary_key = form->s->primary_key;
    hidden_primary_key = (primary_key  >= MAX_KEY) ? TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH : 0;
    if (hidden_primary_key) {
        primary_key = form->s->keys;
    }

    /* do some tracing */
    trace_create_table_info(name,form);

    /* Create status.tokudb and save relevant metadata */
    make_name(newname, name, "status");

    error = db_create(&status_block, db_env, 0);
    if (error) { goto cleanup; }

    error = status_block->set_pagesize(status_block, status_dict_pagesize);
    if (error) { goto cleanup; }

    error = status_block->open(status_block, txn, newname, NULL, DB_BTREE, DB_CREATE | DB_EXCL, 0);
    if (error) { goto cleanup; }

    version = HA_TOKU_VERSION;
    capabilities = HA_TOKU_CAP;
    
    error = write_to_status(status_block, hatoku_new_version,&version,sizeof(version), txn);
    if (error) { goto cleanup; }

    error = write_to_status(status_block, hatoku_capabilities,&capabilities,sizeof(capabilities), txn);
    if (error) { goto cleanup; }

    error = write_auto_inc_create(status_block, create_info->auto_increment_value, txn);
    if (error) { goto cleanup; }

    // only for tables that are not partitioned
    if (TOKU_PARTITION_WRITE_FRM_DATA || form->part_info == NULL) {
        error = write_frm_data(status_block, txn, form->s->path.str);
        if (error) { goto cleanup; }
    }
    error = allocate_key_and_col_info(form->s, &kc_info);
    if (error) { goto cleanup; }

    error = initialize_key_and_col_info(
        form->s, 
        form,
        &kc_info,
        hidden_primary_key,
        primary_key
        );
    if (error) { goto cleanup; }

    error = create_main_dictionary(name, form, txn, &kc_info, row_type);
    if (error) {
        goto cleanup;
    }


    for (uint i = 0; i < form->s->keys; i++) {
        if (i != primary_key) {
            error = create_secondary_dictionary(name, form, &form->key_info[i], txn, &kc_info, i, false, row_type);
            if (error) {
                goto cleanup;
            }

            error = write_key_name_to_status(status_block, form->s->key_info[i].name, txn);
            if (error) { goto cleanup; }
        }
    }

    error = add_table_to_metadata(name, form, txn);
    if (error) { goto cleanup; }

    error = 0;
cleanup:
    if (status_block != NULL) {
        int r = status_block->close(status_block, 0);
        assert(r==0);
    }
    free_key_and_col_info(&kc_info);
    if (do_commit && txn) {
        if (error) {
            abort_txn(txn);
        }
        else {
            commit_txn(txn,0);
        }
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    pthread_mutex_unlock(&tokudb_meta_mutex);
    TOKUDB_DBUG_RETURN(error);
}

int ha_tokudb::discard_or_import_tablespace(my_bool discard) {
    /*
    if (discard) {
        my_errno=HA_ERR_WRONG_COMMAND;
        return my_errno;
    }
    return add_table_to_metadata(share->table_name);
    */
    my_errno=HA_ERR_WRONG_COMMAND;
    return my_errno;
}


//
// deletes from_name or renames from_name to to_name, all using transaction txn.
// is_delete specifies which we are doing
// is_key specifies if it is a secondary index (and hence a "key-" needs to be prepended) or
// if it is not a secondary index
//
int ha_tokudb::delete_or_rename_dictionary( const char* from_name, const char* to_name, const char* secondary_name, bool is_key, DB_TXN* txn, bool is_delete) {
    int error;
    char dict_name[MAX_DICT_NAME_LEN];
    char* new_from_name = NULL;
    char* new_to_name = NULL;
    assert(txn);
    
    new_from_name = (char *)my_malloc(
        get_max_dict_name_path_length(from_name), 
        MYF(MY_WME)
        );
    if (new_from_name == NULL) {
        error = ENOMEM;
        goto cleanup;
    }
    if (!is_delete) {
        assert(to_name);
        new_to_name = (char *)my_malloc(
            get_max_dict_name_path_length(to_name), 
            MYF(MY_WME)
            );
        if (new_to_name == NULL) {
            error = ENOMEM;
            goto cleanup;
        }
    }
    
    if (is_key) {
        sprintf(dict_name, "key-%s", secondary_name);
        make_name(new_from_name, from_name, dict_name);
    }
    else {
        make_name(new_from_name, from_name, secondary_name);
    }
    if (!is_delete) {
        if (is_key) {
            sprintf(dict_name, "key-%s", secondary_name);
            make_name(new_to_name, to_name, dict_name);
        }
        else {
            make_name(new_to_name, to_name, secondary_name);
        }
    }

    if (is_delete) {    
        error = db_env->dbremove(db_env, txn, new_from_name, NULL, 0);
    }
    else {
        error = db_env->dbrename(db_env, txn, new_from_name, NULL, new_to_name, 0);
    }
    if (error) { goto cleanup; }

cleanup:
    my_free(new_from_name, MYF(MY_ALLOW_ZERO_PTR));
    my_free(new_to_name, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}


//
// deletes or renames a table. if is_delete is true, then we delete, and to_name can be NULL
// if is_delete is false, then to_name must be non-NULL, as we are renaming the table.
//
int ha_tokudb::delete_or_rename_table (const char* from_name, const char* to_name, bool is_delete) {
    int error;
    DB* status_db = NULL;
    DBC* status_cursor = NULL;
    DB_TXN* txn = NULL;
    DBT curr_key;
    DBT curr_val;
    memset(&curr_key, 0, sizeof(curr_key));
    memset(&curr_val, 0, sizeof(curr_val));
    pthread_mutex_lock(&tokudb_meta_mutex);

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    //
    // modify metadata db
    //
    if (is_delete) {
        error = drop_table_from_metadata(from_name, txn);
    }
    else {
        error = rename_table_in_metadata(from_name, to_name, txn);
    }
    if (error) { goto cleanup; }

    //
    // open status db,
    // create cursor,
    // for each name read out of there, create a db and delete or rename it
    //
    error = open_status_dictionary(&status_db, from_name, txn);
    if (error) { goto cleanup; }

    error = status_db->cursor(status_db, txn, &status_cursor, 0);
    if (error) { goto cleanup; }

    while (error != DB_NOTFOUND) {
        error = status_cursor->c_get(
            status_cursor,
            &curr_key,
            &curr_val,
            DB_NEXT
            );
        if (error && error != DB_NOTFOUND) { goto cleanup; }
        if (error == DB_NOTFOUND) { break; }

        HA_METADATA_KEY mk = *(HA_METADATA_KEY *)curr_key.data;
        if (mk != hatoku_key_name) {
            continue;
        }
        error = delete_or_rename_dictionary(from_name, to_name, (char *)((char *)curr_key.data + sizeof(HA_METADATA_KEY)), true, txn, is_delete);
        if (error) { goto cleanup; }
    }

    //
    // delete or rename main.tokudb
    //
    error = delete_or_rename_dictionary(from_name, to_name, "main", false, txn, is_delete);
    if (error) { goto cleanup; }

    error = status_cursor->c_close(status_cursor);
    assert(error==0);
    status_cursor = NULL;
    if (error) { goto cleanup; }

    error = status_db->close(status_db, 0);
    assert(error == 0);
    status_db = NULL;
    
    //
    // delete or rename status.tokudb
    //
    error = delete_or_rename_dictionary(from_name, to_name, "status", false, txn, is_delete);
    if (error) { goto cleanup; }

    my_errno = error;
cleanup:
    if (status_cursor) {
        int r = status_cursor->c_close(status_cursor);
        assert(r==0);
    }
    if (status_db) {
        int r = status_db->close(status_db, 0);
        assert(r==0);
    }
    if (txn) {
        if (error) {
            abort_txn(txn);
        }
        else {
            commit_txn(txn, 0);
        }
    }
    pthread_mutex_unlock(&tokudb_meta_mutex);
    return error;
}


//
// Drops table
// Parameters:
//      [in]    name - name of table to be deleted
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::delete_table(const char *name) {
    TOKUDB_DBUG_ENTER("ha_tokudb::delete_table");
    int error;
    error = delete_or_rename_table(name, NULL, true);
    if (error == DB_LOCK_NOTGRANTED && ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0)) {
        sql_print_error("Could not delete table %s because \
another transaction has accessed the table. \
To drop the table, make sure no transactions touch the table.", name);
    }
    TOKUDB_DBUG_RETURN(error);
}


//
// renames table from "from" to "to"
// Parameters:
//      [in]    name - old name of table
//      [in]    to - new name of table
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::rename_table(const char *from, const char *to) {
    TOKUDB_DBUG_ENTER("%s %s %s", __FUNCTION__, from, to);
    int error;
    error = delete_or_rename_table(from, to, false);
    if (error == DB_LOCK_NOTGRANTED && ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0)) {
        sql_print_error("Could not rename table from %s to %s because \
another transaction has accessed the table. \
To rename the table, make sure no transactions touch the table.", from, to);
    }
    TOKUDB_DBUG_RETURN(error);
}


/*
  Returns estimate on number of seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/
/// QQQ why divide by 3
double ha_tokudb::scan_time() {
    TOKUDB_DBUG_ENTER("ha_tokudb::scan_time");
    double ret_val = (double)stats.records / 3;
    DBUG_RETURN(ret_val);
}

double ha_tokudb::keyread_time(uint index, uint ranges, ha_rows rows)
{
  if ((table->key_info[index].flags & HA_CLUSTERING) || (index == primary_key)) {
    return read_time(index, ranges, rows);
  }
  /*
    It is assumed that we will read trough the whole key range and that all
    key blocks are half full (normally things are much better). It is also
    assumed that each time we read the next key from the index, the handler
    performs a random seek, thus the cost is proportional to the number of
    blocks read. This model does not take into account clustered indexes -
    engines that support that (e.g. InnoDB) may want to overwrite this method.
  */
  double keys_per_block= (stats.block_size/2.0/
                          (table->key_info[index].key_length +
                           ref_length) + 1);
  return (rows + keys_per_block - 1)/ keys_per_block;
}


//
// Calculate the time it takes to read a set of ranges through an index
// This enables us to optimize reads for clustered indexes.
// Implementation pulled from InnoDB
// Parameters:
//          index - index to use
//          ranges - number of ranges
//          rows - estimated number of rows in the range
// Returns:
//      estimated time measured in disk seeks
//
double ha_tokudb::read_time(
    uint    index,
    uint    ranges,
    ha_rows rows
    )
{
    double total_scan;
    double ret_val; 
    bool is_primary = (index == primary_key);
    bool is_clustering;

    //
    // in case for hidden primary key, this is called
    //
    if (index >= table_share->keys) {
        ret_val = handler::read_time(index, ranges, rows);
        goto cleanup;
    }
    
    is_clustering = (table->key_info[index].flags & HA_CLUSTERING);


    //
    // if it is not the primary key, and it is not a clustering key, then return handler::read_time
    //
    if (!(is_primary || is_clustering)) {
        ret_val = handler::read_time(index, ranges, rows);
        goto cleanup;
    }

    //
    // for primary key and for clustered keys, return a fraction of scan_time()
    //
    total_scan = scan_time();

    if (stats.records < rows) {
        ret_val = is_clustering ? total_scan + 0.00001 : total_scan;
        goto cleanup;
    }

    //
    // one disk seek per range plus the proportional scan time of the rows
    //
    ret_val = (ranges + (double) rows / (double) stats.records * total_scan);
    ret_val = is_clustering ? ret_val + 0.00001 : ret_val;
    
cleanup:
    return ret_val;
}


//
// Estimates the number of index records in a range. In case of errors, return
//   HA_TOKUDB_RANGE_COUNT instead of HA_POS_ERROR. This was behavior
//   when we got the handlerton from MySQL.
// Parameters:
//              keynr -index to use 
//      [in]    start_key - low end of the range
//      [in]    end_key - high end of the range
// Returns:
//      0 - There are no matching keys in the given range
//      number > 0 - There are approximately number matching rows in the range
//      HA_POS_ERROR - Something is wrong with the index tree
//
ha_rows ha_tokudb::records_in_range(uint keynr, key_range* start_key, key_range* end_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::records_in_range");
    DBT key;
    ha_rows ret_val = HA_TOKUDB_RANGE_COUNT;
    DB *kfile = share->key_file[keynr];
    uint64_t less, equal, greater;
    uint64_t total_rows_estimate = HA_TOKUDB_RANGE_COUNT;
    uint64_t start_rows, end_rows, rows;
    int is_exact;
    int error;
    uchar inf_byte;

    //
    // get start_rows and end_rows values so that we can estimate range
    // when calling key_range64, the only value we can trust is the value for less
    // The reason is that the key being passed in may be a prefix of keys in the DB
    // As a result, equal may be 0 and greater may actually be equal+greater
    // So, we call key_range64 on the key, and the key that is after it.
    //
    if (!start_key && !end_key) {
        error = estimate_num_rows(kfile, &end_rows, transaction);
        if (error) {
            ret_val = HA_TOKUDB_RANGE_COUNT;
            goto cleanup;
        }
        ret_val = (end_rows <= 1) ? 1 : end_rows;
        goto cleanup;
    }
    if (start_key) {
        inf_byte = (start_key->flag == HA_READ_KEY_EXACT) ? 
            COL_NEG_INF : COL_POS_INF;
        pack_key(
            &key, 
            keynr, 
            key_buff, 
            start_key->key, 
            start_key->length, 
            inf_byte
            ); 
        error = kfile->key_range64(
            kfile, 
            transaction, 
            &key,
            &less,
            &equal,
            &greater,
            &is_exact
            );
        if (error) {
            ret_val = HA_TOKUDB_RANGE_COUNT;
            goto cleanup;
        }
        start_rows= less;
        total_rows_estimate = less + equal + greater;
    }
    else {
        start_rows= 0;
    }

    if (end_key) {
        inf_byte = (end_key->flag == HA_READ_BEFORE_KEY) ?
            COL_NEG_INF : COL_POS_INF;
        pack_key(
            &key, 
            keynr, 
            key_buff, 
            end_key->key, 
            end_key->length, 
            inf_byte
            );
        error = kfile->key_range64(
            kfile, 
            transaction, 
            &key,
            &less,
            &equal,
            &greater,
            &is_exact
            );
        if (error) {
            ret_val = HA_TOKUDB_RANGE_COUNT;
            goto cleanup;
        }
        end_rows= less;
    }
    else {
        //
        // first if-clause ensures that start_key is non-NULL
        //
        assert(start_key);
        end_rows = total_rows_estimate;
    }

    rows = (end_rows > start_rows) ? end_rows - start_rows : 1;

    //
    // MySQL thinks a return value of 0 means there are exactly 0 rows
    // Therefore, always return non-zero so this assumption is not made
    //
    ret_val = (ha_rows) (rows <= 1 ? 1 : rows);
cleanup:
    DBUG_RETURN(ret_val);
}


//
// Initializes the auto-increment data in the local "share" object to the
// greater of two values: what's stored in the metadata or the last inserted
// auto-increment field (if auto-increment field is the first field of a key).
//
void ha_tokudb::init_auto_increment() {
    DBT key;
    DBT value;
    int error;
    HA_METADATA_KEY key_val = hatoku_max_ai;
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    key.data = &key_val;
    key.size = sizeof(key_val);
    value.flags = DB_DBT_USERMEM;
    DB_TXN* txn = NULL;

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) {
        share->last_auto_increment = 0;    
    }
    else {
        //
        // First retrieve hatoku_max_ai, which is max value used by auto increment
        // column so far, the max value could have been auto generated (e.g. insert (NULL))
        // or it could have been manually inserted by user (e.g. insert (345))
        //
        value.ulen = sizeof(share->last_auto_increment);
        value.data = &share->last_auto_increment;
        error = share->status_block->get(
            share->status_block, 
            txn, 
            &key, 
            &value, 
            0
            );
        
        if (error || value.size != sizeof(share->last_auto_increment)) {
            share->last_auto_increment = 0;
        }

        //
        // Now retrieve the initial auto increment value, as specified by create table
        // so if a user does "create table t1 (a int auto_increment, primary key (a)) auto_increment=100",
        // then the value 100 should be stored here
        //
        key_val = hatoku_ai_create_value;
        value.ulen = sizeof(share->auto_inc_create_value);
        value.data = &share->auto_inc_create_value;
        error = share->status_block->get(
            share->status_block, 
            txn, 
            &key, 
            &value, 
            0
            );
        
        if (error || value.size != sizeof(share->auto_inc_create_value)) {
            share->auto_inc_create_value = 0;
        }

        commit_txn(txn, 0);
    }
    if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT) {
        TOKUDB_TRACE("init auto increment:%lld\n", share->last_auto_increment);
    }
}

void ha_tokudb::get_auto_increment(ulonglong offset, ulonglong increment, ulonglong nb_desired_values, ulonglong * first_value, ulonglong * nb_reserved_values) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_auto_increment");
    ulonglong nr;

    pthread_mutex_lock(&share->mutex);

    if (share->auto_inc_create_value > share->last_auto_increment) {
        nr = share->auto_inc_create_value;
        share->last_auto_increment = share->auto_inc_create_value;
    }
    else {
        nr = share->last_auto_increment + increment;
    }
    share->last_auto_increment = nr + (nb_desired_values - 1)*increment;
    if (delay_updating_ai_metadata) {
        ai_metadata_update_required = true;
    }
    else {
        update_max_auto_inc(share->status_block, share->last_auto_increment);
    }

    if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT) {
        TOKUDB_TRACE("get_auto_increment(%lld,%lld,%lld):got:%lld:%lld\n",
                     offset, increment, nb_desired_values, nr, nb_desired_values);
    }
    *first_value = nr;
    *nb_reserved_values = nb_desired_values;
    pthread_mutex_unlock(&share->mutex);
    DBUG_VOID_RETURN;
}

bool ha_tokudb::is_optimize_blocking() {
    return false;
}

bool ha_tokudb::is_auto_inc_singleton(){
    return false;
}

volatile int ha_tokudb_tokudb_add_index_wait = 0; // debug

//
// Internal function called by ha_tokudb::add_index and ha_tokudb::alter_table_phase2
// With a transaction, drops dictionaries associated with indexes in key_num
//
//
// Adds indexes to the table. Takes the array of KEY passed in key_info, and creates
// DB's that will go at the end of share->key_file. THE IMPLICIT ASSUMPTION HERE is
// that the table will be modified and that these added keys will be appended to the end
// of the array table->key_info
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_info - array of KEY's to be added
//              num_of_keys - number of keys to be added, number of elements in key_info
//  Returns:
//      0 on success, error otherwise
//
int ha_tokudb::tokudb_add_index(
    TABLE *table_arg, 
    KEY *key_info, 
    uint num_of_keys, 
    DB_TXN* txn, 
    bool* inc_num_DBs,
    bool* modified_DBs
    ) 
{
    TOKUDB_DBUG_ENTER("ha_tokudb::tokudb_add_index");
    assert(txn);

    while (ha_tokudb_tokudb_add_index_wait) sleep(1); // debug

    int error;
    uint curr_index = 0;
    DBC* tmp_cursor = NULL;
    int cursor_ret_val = 0;
    DBT curr_pk_key, curr_pk_val;
    THD* thd = ha_thd(); 
    DB_LOADER* loader = NULL;
    DB_INDEXER* indexer = NULL;
    bool loader_use_puts = get_load_save_space(thd);
    bool use_hot_index = (lock.type == TL_WRITE_ALLOW_WRITE);
    uint32_t loader_flags = loader_use_puts ? LOADER_USE_PUTS : 0;
    uint32_t indexer_flags = 0;
    uint32_t mult_db_flags[MAX_KEY + 1] = {0};
    uint32_t mult_put_flags[MAX_KEY + 1];
    uint32_t mult_dbt_flags[MAX_KEY + 1];
    bool creating_hot_index = false;
    struct loader_context lc;
    memset(&lc, 0, sizeof lc);
    lc.thd = thd;
    lc.ha = this;
    loader_error = 0;
    bool rw_lock_taken = false;
    *inc_num_DBs = false;
    *modified_DBs = false;
    invalidate_bulk_fetch();
    unpack_entire_row = true; // for bulk fetching rows
    for (uint32_t i = 0; i < MAX_KEY+1; i++) {
        mult_put_flags[i] = 0;
        mult_dbt_flags[i] = DB_DBT_REALLOC;
    }
    //
    // number of DB files we have open currently, before add_index is executed
    //
    uint curr_num_DBs = table_arg->s->keys + test(hidden_primary_key);

    //
    // get the row type to use for the indexes we're adding
    //
    const enum row_type row_type = get_row_type_for_key(share->file);

    //
    // status message to be shown in "show process list"
    //
    char status_msg[MAX_ALIAS_NAME + 200]; //buffer of 200 should be a good upper bound.
    ulonglong num_processed = 0; //variable that stores number of elements inserted thus far
    thd_proc_info(thd, "Adding indexes");
    
    //
    // in unpack_row, MySQL passes a buffer that is this long,
    // so this length should be good enough for us as well
    //
    memset((void *) &curr_pk_key, 0, sizeof(curr_pk_key));
    memset((void *) &curr_pk_val, 0, sizeof(curr_pk_val));

    //
    // The files for secondary tables are derived from the name of keys
    // If we try to add a key with the same name as an already existing key,
    // We can crash. So here we check if any of the keys added has the same
    // name of an existing key, and if so, we fail gracefully
    //
    for (uint i = 0; i < num_of_keys; i++) {
        for (uint j = 0; j < table_arg->s->keys; j++) {
            if (strcmp(key_info[i].name, table_arg->s->key_info[j].name) == 0) {
                error = HA_ERR_WRONG_COMMAND;
                goto cleanup;
            }
        }
    }
    
    rw_wrlock(&share->num_DBs_lock);
    rw_lock_taken = true;
    //
    // open all the DB files and set the appropriate variables in share
    // they go to the end of share->key_file
    //
    creating_hot_index = use_hot_index && num_of_keys == 1 && (key_info[0].flags & HA_NOSAME) == 0;
    if (use_hot_index && (share->num_DBs > curr_num_DBs)) {
        //
        // already have hot index in progress, get out
        //
        error = HA_ERR_INTERNAL_ERROR;
        goto cleanup;
    }
    curr_index = curr_num_DBs;
    *modified_DBs = true;
    for (uint i = 0; i < num_of_keys; i++, curr_index++) {
        if (key_info[i].flags & HA_CLUSTERING) {
            set_key_filter(
                &share->kc_info.key_filters[curr_index],
                &key_info[i],
                table_arg,
                false
                );                
            if (!hidden_primary_key) {
                set_key_filter(
                    &share->kc_info.key_filters[curr_index],
                    &table_arg->key_info[primary_key],
                    table_arg,
                    false
                    );
            }

            error = initialize_col_pack_info(&share->kc_info,table_arg->s,curr_index);
            if (error) {
                goto cleanup;
            }
        }


        error = create_secondary_dictionary(share->table_name, table_arg, &key_info[i], txn, &share->kc_info, curr_index, creating_hot_index, row_type);
        if (error) { goto cleanup; }

        error = open_secondary_dictionary(
            &share->key_file[curr_index], 
            &key_info[i],
            share->table_name,
            false,
            txn
            );
        if (error) { goto cleanup; }
    }
    
    if (creating_hot_index) {
        share->num_DBs++;
        *inc_num_DBs = true;
        error = db_env->create_indexer(
            db_env,
            txn,
            &indexer,
            share->file,
            num_of_keys,
            &share->key_file[curr_num_DBs],
            mult_db_flags,
            indexer_flags
            );
        if (error) { goto cleanup; }

        error = indexer->set_poll_function(indexer, ai_poll_fun, &lc);
        if (error) { goto cleanup; }

        error = indexer->set_error_callback(indexer, loader_ai_err_fun, &lc);
        if (error) { goto cleanup; }

        rw_unlock(&share->num_DBs_lock);
        rw_lock_taken = false;
        
#ifdef HA_TOKUDB_HAS_THD_PROGRESS
        // initialize a one phase progress report.
        // incremental reports are done in the indexer's callback function.
        thd_progress_init(thd, 1);
#endif

        error = indexer->build(indexer);

        if (error) { goto cleanup; }

        rw_wrlock(&share->num_DBs_lock);
        error = indexer->close(indexer);
        rw_unlock(&share->num_DBs_lock);
        if (error) { goto cleanup; }
        indexer = NULL;
    }
    else {
        rw_unlock(&share->num_DBs_lock);
        rw_lock_taken = false;
        prelocked_right_range_size = 0;
        prelocked_left_range_size = 0;
        struct smart_dbt_bf_info bf_info;
        bf_info.ha = this;
        // you need the val if you have a clustering index and key_read is not 0;
        bf_info.direction = 1;
        bf_info.thd = ha_thd();
        bf_info.need_val = true;

        error = db_env->create_loader(
            db_env, 
            txn, 
            &loader, 
            NULL, // no src_db needed
            num_of_keys, 
            &share->key_file[curr_num_DBs], 
            mult_put_flags,
            mult_dbt_flags,
            loader_flags
            );
        if (error) { goto cleanup; }

        error = loader->set_poll_function(loader, loader_poll_fun, &lc);
        if (error) { goto cleanup; }

        error = loader->set_error_callback(loader, loader_ai_err_fun, &lc);
        if (error) { goto cleanup; }
        //
        // scan primary table, create each secondary key, add to each DB
        //    
        if ((error = share->file->cursor(share->file, txn, &tmp_cursor, DB_SERIALIZABLE))) {
            tmp_cursor = NULL;             // Safety
            goto cleanup;
        }

        //
        // grab some locks to make this go faster
        // first a global read lock on the main DB, because
        // we intend to scan the entire thing
        //
        error = tmp_cursor->c_pre_acquire_range_lock(
            tmp_cursor,
            share->file->dbt_neg_infty(),
            share->file->dbt_pos_infty()
            );
        if (error) { goto cleanup; }

        // set the bulk fetch iteration to its max so that adding an
        // index fills the bulk fetch buffer every time. we do not
        // want it to grow exponentially fast.
        rows_fetched_using_bulk_fetch = 0;
        bulk_fetch_iteration = HA_TOKU_BULK_FETCH_ITERATION_MAX;
        cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED,smart_dbt_bf_callback, &bf_info);

#ifdef HA_TOKUDB_HAS_THD_PROGRESS
        // initialize a two phase progress report.
        // first phase: putting rows into the loader
        thd_progress_init(thd, 2);
#endif

        while (cursor_ret_val != DB_NOTFOUND || ((bytes_used_in_range_query_buff - curr_range_query_buff_offset) > 0)) {
            if ((bytes_used_in_range_query_buff - curr_range_query_buff_offset) == 0) {
                invalidate_bulk_fetch(); // reset the buffers
                cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_bf_callback, &bf_info);
                if (cursor_ret_val != DB_NOTFOUND && cursor_ret_val != 0) {
                    error = cursor_ret_val;
                    goto cleanup;
                }
            }
            // do this check in case the the c_getf_next did not put anything into the buffer because
            // there was no more data
            if ((bytes_used_in_range_query_buff - curr_range_query_buff_offset) == 0) {
                break;
            }
            // at this point, we know the range query buffer has at least one key/val pair
            uchar* curr_pos = range_query_buff+curr_range_query_buff_offset;
            
            uint32_t key_size = *(uint32_t *)curr_pos;    
            curr_pos += sizeof(key_size);    
            uchar* curr_key_buff = curr_pos;    
            curr_pos += key_size;        
            curr_pk_key.data = curr_key_buff;    
            curr_pk_key.size = key_size;
            
            uint32_t val_size = *(uint32_t *)curr_pos;    
            curr_pos += sizeof(val_size);    
            uchar* curr_val_buff = curr_pos;    
            curr_pos += val_size;        
            curr_pk_val.data = curr_val_buff;    
            curr_pk_val.size = val_size;
            
            curr_range_query_buff_offset = curr_pos - range_query_buff;

            error = loader->put(loader, &curr_pk_key, &curr_pk_val);
            if (error) { goto cleanup; }

            num_processed++; 

            if ((num_processed % 1000) == 0) {
                if (loader_use_puts) {
                    sprintf(status_msg, "Adding indexes: Processed %llu of about %llu rows.", num_processed, (long long unsigned) share->rows);
                }
                else {
                    sprintf(status_msg, "Adding indexes: Fetched %llu of about %llu rows, loading of data still remains.", num_processed, (long long unsigned) share->rows);
                }
                thd_proc_info(thd, status_msg);

#ifdef HA_TOKUDB_HAS_THD_PROGRESS
                thd_progress_report(thd, num_processed, (long long unsigned) share->rows);
#endif

                if (thd->killed) {
                    error = ER_ABORTING_CONNECTION;
                    goto cleanup;
                }
            }
        }
        error = tmp_cursor->c_close(tmp_cursor);
        assert(error==0);
        tmp_cursor = NULL;

#ifdef HA_TOKUDB_HAS_THD_PROGRESS
        // next progress report phase: closing the loader. 
        // incremental reports are done in the loader's callback function.
        thd_progress_next_stage(thd);
#endif

        error = loader->close(loader);
        loader = NULL;

        if (error) goto cleanup;
    }
    curr_index = curr_num_DBs;
    for (uint i = 0; i < num_of_keys; i++, curr_index++) {
        if (key_info[i].flags & HA_NOSAME) {
            bool is_unique;
            error = is_index_unique(
                &is_unique, 
                txn, 
                share->key_file[curr_index], 
                &key_info[i]
                );
            if (error) goto cleanup;
            if (!is_unique) {
                error = HA_ERR_FOUND_DUPP_KEY;
                last_dup_key = i;
                goto cleanup;
            }
        }
    }

    //
    // We have an accurate row count, might as well update share->rows
    //
    if(!creating_hot_index) {
        pthread_mutex_lock(&share->mutex);
        share->rows = num_processed;
        pthread_mutex_unlock(&share->mutex);
    }
    //
    // now write stuff to status.tokudb
    //
    pthread_mutex_lock(&share->mutex);
    for (uint i = 0; i < num_of_keys; i++) {
        write_key_name_to_status(share->status_block, key_info[i].name, txn);
    }
    pthread_mutex_unlock(&share->mutex);
    
    error = 0;
cleanup:
#ifdef HA_TOKUDB_HAS_THD_PROGRESS
    thd_progress_end(thd);
#endif
    if (rw_lock_taken) {
        rw_unlock(&share->num_DBs_lock);
        rw_lock_taken = false;
    }
    if (tmp_cursor) {            
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
        tmp_cursor = NULL;
    }
    if (loader != NULL) {
        sprintf(status_msg, "aborting creation of indexes.");
        thd_proc_info(thd, status_msg);
        loader->abort(loader);
    }
    if (indexer != NULL) {
        sprintf(status_msg, "aborting creation of indexes.");
        thd_proc_info(thd, status_msg);
        rw_wrlock(&share->num_DBs_lock);
        indexer->abort(indexer);
        rw_unlock(&share->num_DBs_lock);
    }
    if (error == DB_LOCK_NOTGRANTED && ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0)) {
        sql_print_error("Could not add indexes to table %s because \
another transaction has accessed the table. \
To add indexes, make sure no transactions touch the table.", share->table_name);
    }
    TOKUDB_DBUG_RETURN(error ? error : loader_error);
}

//
// Internal function called by ha_tokudb::add_index and ha_tokudb::alter_table_phase2
// Closes added indexes in case of error in error path of add_index and alter_table_phase2
//
void ha_tokudb::restore_add_index(TABLE* table_arg, uint num_of_keys, bool incremented_numDBs, bool modified_DBs) {
    uint curr_num_DBs = table_arg->s->keys + test(hidden_primary_key);
    uint curr_index = 0;

    //
    // need to restore num_DBs, and we have to do it before we close the dictionaries
    // so that there is not a window 
    //
    if (incremented_numDBs) {
        rw_wrlock(&share->num_DBs_lock);
        share->num_DBs--;
    }
    if (modified_DBs) {
        curr_index = curr_num_DBs;
        for (uint i = 0; i < num_of_keys; i++, curr_index++) {
            reset_key_and_col_info(&share->kc_info, curr_index);
        }
        curr_index = curr_num_DBs;
        for (uint i = 0; i < num_of_keys; i++, curr_index++) {
            if (share->key_file[curr_index]) {
                int r = share->key_file[curr_index]->close(
                    share->key_file[curr_index],
                    0
                    );
                assert(r==0);
                share->key_file[curr_index] = NULL;
            }
        }
    }
    if (incremented_numDBs) {
        rw_unlock(&share->num_DBs_lock);
    }
}

volatile int ha_tokudb_drop_indexes_wait = 0; // debug

//
// Internal function called by ha_tokudb::prepare_drop_index and ha_tokudb::alter_table_phase2
// With a transaction, drops dictionaries associated with indexes in key_num
//
int ha_tokudb::drop_indexes(TABLE *table_arg, uint *key_num, uint num_of_keys, KEY *key_info, DB_TXN* txn) {
    TOKUDB_DBUG_ENTER("ha_tokudb::drop_indexes");
    assert(txn);

    while (ha_tokudb_drop_indexes_wait) sleep(1); // debug

    share_key_file_wrlock(share);

    int error = 0;
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = key_num[i];
        error = share->key_file[curr_index]->pre_acquire_fileops_lock(share->key_file[curr_index],txn);
        if (error != 0) {
            goto cleanup;
        }
    }
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = key_num[i];
        int r = share->key_file[curr_index]->close(share->key_file[curr_index],0);
        assert(r==0);
        share->key_file[curr_index] = NULL;

        error = remove_key_name_from_status(share->status_block, key_info[curr_index].name, txn);
        if (error) { goto cleanup; }
        
        error = delete_or_rename_dictionary(share->table_name, NULL, key_info[curr_index].name, true, txn, true);
        if (error) { goto cleanup; }
    }

cleanup:
    if (error == DB_LOCK_NOTGRANTED && ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0)) {
        sql_print_error("Could not drop indexes from table %s because \
another transaction has accessed the table. \
To drop indexes, make sure no transactions touch the table.", share->table_name);
    }
    share_key_file_unlock(share);
    TOKUDB_DBUG_RETURN(error);
}

//
// Internal function called by ha_tokudb::prepare_drop_index and ha_tokudb::alter_table_phase2
// Restores dropped indexes in case of error in error path of prepare_drop_index and alter_table_phase2
//
void ha_tokudb::restore_drop_indexes(TABLE *table_arg, uint *key_num, uint num_of_keys) {
    share_key_file_wrlock(share);

    //
    // reopen closed dictionaries
    //
    for (uint i = 0; i < num_of_keys; i++) {
        int r;
        uint curr_index = key_num[i];
        if (share->key_file[curr_index] == NULL) {
            r = open_secondary_dictionary(
                &share->key_file[curr_index], 
                &table_share->key_info[curr_index],
                share->table_name,
                false, // 
                NULL
                );
            assert(!r);
        }
    }            
    share_key_file_unlock(share);
}

void ha_tokudb::print_error(int error, myf errflag) {
    if (error == DB_LOCK_DEADLOCK)
        error = HA_ERR_LOCK_DEADLOCK;
    if (error == DB_LOCK_NOTGRANTED)
        error = HA_ERR_LOCK_WAIT_TIMEOUT;
#if defined(HA_ERR_DISK_FULL)
    if (error == ENOSPC) {
        error = HA_ERR_DISK_FULL;
    }
#endif
    if (error == DB_KEYEXIST) {
        error = HA_ERR_FOUND_DUPP_KEY;
    }
#if defined(HA_ALTER_ERROR)
    if (error == HA_ALTER_ERROR) {
        error = HA_ERR_UNSUPPORTED;
    }
#endif
    // TODO: should rename debug code to something better
    // just reusing this so that tests don' start complaining
#if MYSQL_VERSION_ID < 50500
    if ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0) {
        THD* thd = ha_thd();
        if (get_log_client_errors(thd)) {
            sql_print_error("query \"%s\" returned handler error %d", thd->query_string.str, error);
        }
    }
#endif
    handler::print_error(error, errflag);
}


//
// truncate's dictionary associated with keynr index using transaction txn
// does so by deleting and then recreating the dictionary in the context
// of a transaction
//
int ha_tokudb::truncate_dictionary( uint keynr, DB_TXN* txn ) {
    int error;
    bool is_pk = (keynr == primary_key);

    const enum row_type row_type = get_row_type_for_key(share->key_file[keynr]);
    error = share->key_file[keynr]->close(share->key_file[keynr], 0);
    assert(error == 0);

    share->key_file[keynr] = NULL;
    if (is_pk) { share->file = NULL; }

    if (is_pk) {
        error = delete_or_rename_dictionary(
            share->table_name, 
            NULL,
            "main", 
            false, //is_key
            txn,
            true // is a delete
            );
        if (error) { goto cleanup; }
    }
    else {
        error = delete_or_rename_dictionary(
            share->table_name, 
            NULL,
            table_share->key_info[keynr].name, 
            true, //is_key
            txn,
            true // is a delete
            );
        if (error) { goto cleanup; }
    }

    if (is_pk) {
        error = create_main_dictionary(share->table_name, table, txn, &share->kc_info, row_type);
    }
    else {
        error = create_secondary_dictionary(
            share->table_name, 
            table, 
            &table_share->key_info[keynr], 
            txn,
            &share->kc_info,
            keynr,
            false,
            row_type
            );
    }
    if (error) { goto cleanup; }

cleanup:
    return error;
}

volatile int ha_tokudb_truncate_wait = 0; // debug

// for 5.5
int ha_tokudb::truncate() {
    TOKUDB_DBUG_ENTER("truncate");
    while (ha_tokudb_truncate_wait) sleep(1); // debug
    int error = delete_all_rows_internal();
    TOKUDB_DBUG_RETURN(error);
}

// delete all rows from a table
//
// effects: delete all of the rows in the main dictionary and all of the
// indices.  this must be atomic, so we use the statement transaction
// for all of the truncate operations.
// locks:  if we have an exclusive table write lock, all of the concurrency
// issues go away.
// returns: 0 if success
int ha_tokudb::delete_all_rows() {
    TOKUDB_DBUG_ENTER("delete_all_rows");
    int error = 0;
    if (thd_sql_command(ha_thd()) != SQLCOM_TRUNCATE) {
        share->try_table_lock = true;
        error = HA_ERR_WRONG_COMMAND;
    }
    if (error == 0)
        error = delete_all_rows_internal();
    TOKUDB_DBUG_RETURN(error);
}

int ha_tokudb::delete_all_rows_internal() {
    TOKUDB_DBUG_ENTER("delete_all_rows_internal");
    int error = 0;
    uint curr_num_DBs = 0;
    DB_TXN* txn = NULL;

    share_key_file_wrlock(share);

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    curr_num_DBs = table->s->keys + test(hidden_primary_key);
    for (uint i = 0; i < curr_num_DBs; i++) {
        error = share->key_file[i]->pre_acquire_fileops_lock(
            share->key_file[i], 
            txn
            );
        if (error) { goto cleanup; }
        error = share->key_file[i]->pre_acquire_table_lock(
            share->key_file[i], 
            txn
            );
        if (error) { goto cleanup; }
    }
    for (uint i = 0; i < curr_num_DBs; i++) {
        error = truncate_dictionary(i, txn);
        if (error) { goto cleanup; }
    }

    // zap the row count
    if (error == 0) {
        share->rows = 0;
    }

    share->try_table_lock = true;
cleanup:
    if (txn) {
        if (error) {
            abort_txn(txn);
        }
        else {
            commit_txn(txn,0);
        }
    }

    if (error == DB_LOCK_NOTGRANTED && ((tokudb_debug & TOKUDB_DEBUG_HIDE_DDL_LOCK_ERRORS) == 0)) {
        sql_print_error("Could not truncate table %s because another transaction has accessed the \
        table. To truncate the table, make sure no transactions touch the table.", 
        share->table_name);
    }
    //
    // regardless of errors, need to reopen the DB's
    //    
    for (uint i = 0; i < curr_num_DBs; i++) {
        int r = 0;
        if (share->key_file[i] == NULL) {
            if (i != primary_key) {
                r = open_secondary_dictionary(
                    &share->key_file[i], 
                    &table_share->key_info[i],
                    share->table_name,
                    false, // 
                    NULL
                    );
                assert(!r);
            }
            else {
                r = open_main_dictionary(
                    share->table_name, 
                    false, 
                    NULL
                    );
                assert(!r);
            }
        }
    }
    share_key_file_unlock(share);
    TOKUDB_DBUG_RETURN(error);
}

void ha_tokudb::set_loader_error(int err) {
    loader_error = err;
}

void ha_tokudb::set_dup_value_for_pk(DBT* key) {
    assert(!hidden_primary_key);
    unpack_key(table->record[0],key,primary_key);
    last_dup_key = primary_key;
}

// table admin 
#include "ha_tokudb_admin.cc"

// alter table code for various mysql distros
#include "ha_tokudb_update_fun.cc"
#include "ha_tokudb_alter_51.cc"
#include "ha_tokudb_alter_55.cc"
#include "ha_tokudb_alter_56.cc"

// key comparisons
#include "hatoku_cmp.cc"

// handlerton
#include "hatoku_hton.cc"
