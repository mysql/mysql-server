#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation          // gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "mysql_priv.h"
extern "C" {
#include "stdint.h"
#if defined(_WIN32)
#include "misc.h"
#endif
}

#if !defined(HA_END_SPACE_KEY) || HA_END_SPACE_KEY != 0
#error
#endif

static inline void *thd_data_get(THD *thd, int slot) {
#if MYSQL_VERSION_ID <= 50123
    return thd->ha_data[slot];
#else
    return thd->ha_data[slot].ha_ptr;
#endif
}

static inline void thd_data_set(THD *thd, int slot, void *data) {
#if MYSQL_VERSION_ID <= 50123
    thd->ha_data[slot] = data;
#else
    thd->ha_data[slot].ha_ptr = data;
#endif
}

#undef PACKAGE
#undef VERSION
#undef HAVE_DTRACE
#undef _DTRACE_VERSION

//#include "tokudb_config.h"

/* We define DTRACE after mysql_priv.h in case it disabled dtrace in the main server */
#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#endif

#include "tokudb_probes.h"

#include "hatoku_defines.h"
#include "ha_tokudb.h"
#include "hatoku_hton.h"
#include "hatoku_cmptrace.h"
#include <mysql/plugin.h>


/** @brief
    Simple lock controls. The "share" it creates is a structure we will
    pass to each tokudb handler. Do you have to have one of these? Well, you have
    pieces that are used for locking, and they are needed to function.
*/
static TOKUDB_SHARE *get_share(const char *table_name, TABLE * table) {
    TOKUDB_SHARE *share;
    uint length;

    pthread_mutex_lock(&tokudb_mutex);
    length = (uint) strlen(table_name);

    if (!(share = (TOKUDB_SHARE *) hash_search(&tokudb_open_tables, (uchar *) table_name, length))) {
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

        bzero((void *) share->key_file, sizeof(share->key_file));

        if (my_hash_insert(&tokudb_open_tables, (uchar *) share))
            goto error;
        thr_lock_init(&share->lock);
        pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
    }
    pthread_mutex_unlock(&tokudb_mutex);

    return share;

  error:
    pthread_mutex_destroy(&share->mutex);
    my_free((uchar *) share, MYF(0));

    return NULL;
}

static int free_share(TOKUDB_SHARE * share, TABLE * table, uint hidden_primary_key, bool mutex_is_locked) {
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
                if (error) {
                    result = error;
                }
                share->key_file[i] = NULL;
            }
        }

        if (share->status_block && (error = share->status_block->close(share->status_block, 0))) {
            result = error;
        }
        

        hash_delete(&tokudb_open_tables, (uchar *) share);
        thr_lock_delete(&share->lock);
        pthread_mutex_destroy(&share->mutex);
        my_free((uchar *) share, MYF(0));
    }
    pthread_mutex_unlock(&tokudb_mutex);

    return result;
}

static int get_name_length(const char *name) {
    int n = 0;
    const char *newname = name;
    if (tokudb_data_dir) {
        n += strlen(tokudb_data_dir) + 1;
        if (strncmp("./", name, 2) == 0) 
            newname = name + 2;
    }
    n += strlen(newname);
    n += strlen(ha_tokudb_ext);
    return n;
}

static void make_name(char *newname, const char *tablename, const char *dictname) {
    const char *newtablename = tablename;
    char *nn = newname;
    if (tokudb_data_dir) {
        nn += sprintf(nn, "%s/", tokudb_data_dir);
        if (strncmp("./", tablename, 2) == 0)
            newtablename = tablename + 2;
    }
    nn += sprintf(nn, "%s%s", newtablename, ha_tokudb_ext);
    if (dictname)
        nn += sprintf(nn, "/%s%s", dictname, ha_tokudb_ext);
}


#define HANDLE_INVALID_CURSOR() \
    if (cursor == NULL) { \
        error = last_cursor_error; \
        goto cleanup; \
    }

ha_tokudb::ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg):handler(hton, table_arg) 
    // flags defined in sql\handler.h
{
    int_table_flags = HA_REC_NOT_IN_SEQ | HA_FAST_KEY_READ | HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_PRIMARY_KEY_IN_READ_INDEX | 
                    HA_FILE_BASED | HA_AUTO_PART_KEY | HA_TABLE_SCAN_ON_INDEX |HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;
	alloc_ptr = NULL;
	rec_buff = NULL;
    transaction = NULL;
	added_rows = 0;
	deleted_rows = 0;
	last_dup_key = UINT_MAX;
	using_ignore = 0;
	last_cursor_error = 0;
	range_lock_grabbed = false;
	primary_key_offsets = NULL;
    num_added_rows_in_stmt = 0;
    num_deleted_rows_in_stmt = 0;
    num_updated_rows_in_stmt = 0;
}

static const char *ha_tokudb_exts[] = {
    ha_tokudb_ext,
    NullS
};

/* 
 *  returns NULL terminated file extension string
 */
const char **ha_tokudb::bas_ext() const {
    TOKUDB_DBUG_ENTER("ha_tokudb::bas_ext");
    DBUG_RETURN(ha_tokudb_exts);
}

//
// Returns a bit mask of capabilities of the key or its part specified by 
// the arguments. The capabilities are defined in sql/handler.h.
//
ulong ha_tokudb::index_flags(uint idx, uint part, bool all_parts) const {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_flags");
    ulong flags = (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_KEYREAD_ONLY | HA_READ_RANGE);
    DBUG_RETURN(flags);
}


static inline int tokudb_compare_two_hidden_keys(
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size
    ) {
    assert( (new_key_size >= TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH) && (saved_key_size >= TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH) );
    ulonglong a = hpk_char_to_num((uchar *) new_key_data);
    ulonglong b = hpk_char_to_num((uchar *) saved_key_data);
    return a < b ? -1 : (a > b ? 1 : 0);
}

static int tokudb_cmp_hidden_key(DB * file, const DBT * new_key, const DBT * saved_key) {
    return tokudb_compare_two_hidden_keys(
        new_key->data, 
        new_key->size,
        saved_key->data,
        saved_key->size
        );
}

static int tokudb_compare_two_keys(
    KEY *key, 
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size,
    bool cmp_prefix
    ) {
    uchar new_key_inf_val = *(uchar *) new_key_data;
    uchar saved_key_inf_val = *(uchar *) saved_key_data;
    //
    // first byte is "infinity" byte
    //
    uchar *new_key_ptr = (uchar *)(new_key_data) + 1;
    uchar *saved_key_ptr = (uchar *)(saved_key_data) + 1;
    KEY_PART_INFO *key_part = key->key_part, *end = key_part + key->key_parts;
    int ret_val;
    //
    // do not include the inf val at the beginning
    //
    uint new_key_length = new_key_size - sizeof(uchar);
    uint saved_key_length = saved_key_size - sizeof(uchar);

    //DBUG_DUMP("key_in_index", saved_key_ptr, saved_key->size);
    for (; key_part != end && (int) new_key_length > 0 && (int) saved_key_length > 0; key_part++) {
        int cmp;
        uint new_key_field_length;
        uint saved_key_field_length;
        if (key_part->field->null_bit) {
            assert(new_key_ptr   < (uchar *) new_key_data   + new_key_size);
            assert(saved_key_ptr < (uchar *) saved_key_data + saved_key_size);
            if (*new_key_ptr != *saved_key_ptr) {
                return ((int) *new_key_ptr - (int) *saved_key_ptr); }
            saved_key_ptr++;
            new_key_length--;
            saved_key_length--;
            if (!*new_key_ptr++) { continue; }
        }
        new_key_field_length     = key_part->field->packed_col_length(new_key_ptr,   key_part->length);
        saved_key_field_length   = key_part->field->packed_col_length(saved_key_ptr, key_part->length);
        assert(new_key_length >= new_key_field_length);
        assert(saved_key_length >= saved_key_field_length);
        if ((cmp = key_part->field->pack_cmp(new_key_ptr, saved_key_ptr, key_part->length, 0)))
            return cmp;
        new_key_ptr      += new_key_field_length;
        new_key_length   -= new_key_field_length;
        saved_key_ptr    += saved_key_field_length;
        saved_key_length -= saved_key_field_length;
    }
    if (cmp_prefix || (new_key_length == 0 && saved_key_length == 0) ) {
        ret_val = 0;
    }
    //
    // at this point, one SHOULD be 0
    //
    else if (new_key_length == 0 && saved_key_length > 0) {
        ret_val = (new_key_inf_val == COL_POS_INF ) ? 1 : -1; 
    }
    else if (new_key_length > 0 && saved_key_length == 0) {
        ret_val = (saved_key_inf_val == COL_POS_INF ) ? -1 : 1; 
    }
    //
    // this should never happen, perhaps we should assert(false)
    //
    else {
        ret_val = new_key_length - saved_key_length;
    }
    return ret_val;
}



//
// this is super super ugly, copied from compare_two_keys so that it can get done fast
//
static int tokudb_compare_two_clustered_keys(KEY *key, KEY* primary_key, const DBT * new_key, const DBT * saved_key) {
    uchar new_key_inf_val = *(uchar *) new_key->data;
    uchar saved_key_inf_val = *(uchar *) saved_key->data;
    //
    // first byte is "infinity" byte
    //
    uchar *new_key_ptr = (uchar *)(new_key->data) + 1;
    uchar *saved_key_ptr = (uchar *)(saved_key->data) + 1;
    KEY_PART_INFO *key_part = key->key_part, *end = key_part + key->key_parts;
    int ret_val;
    //
    // do not include the inf val at the beginning
    //
    uint new_key_length = new_key->size - sizeof(uchar);
    uint saved_key_length = saved_key->size - sizeof(uchar);

    //DBUG_DUMP("key_in_index", saved_key_ptr, saved_key->size);
    for (; key_part != end && (int) new_key_length > 0 && (int) saved_key_length > 0; key_part++) {
        int cmp;
        uint new_key_field_length;
        uint saved_key_field_length;
        if (key_part->field->null_bit) {
            assert(new_key_ptr   < (uchar *) new_key->data   + new_key->size);
            assert(saved_key_ptr < (uchar *) saved_key->data + saved_key->size);
            if (*new_key_ptr != *saved_key_ptr) {
                return ((int) *new_key_ptr - (int) *saved_key_ptr); }
            saved_key_ptr++;
            new_key_length--;
            saved_key_length--;
            if (!*new_key_ptr++) { continue; }
        }
        new_key_field_length     = key_part->field->packed_col_length(new_key_ptr,   key_part->length);
        saved_key_field_length   = key_part->field->packed_col_length(saved_key_ptr, key_part->length);
        assert(new_key_length >= new_key_field_length);
        assert(saved_key_length >= saved_key_field_length);
        if ((cmp = key_part->field->pack_cmp(new_key_ptr, saved_key_ptr, key_part->length, 0)))
            return cmp;
        new_key_ptr      += new_key_field_length;
        new_key_length   -= new_key_field_length;
        saved_key_ptr    += saved_key_field_length;
        saved_key_length -= saved_key_field_length;
    }
    if (new_key_length == 0 && saved_key_length == 0){
        ret_val = 0;
    }
    else if (new_key_length == 0 && saved_key_length > 0) {
        ret_val = (new_key_inf_val == COL_POS_INF ) ? 1 : -1; 
    }
    else if (new_key_length > 0 && saved_key_length == 0) {
        ret_val = (saved_key_inf_val == COL_POS_INF ) ? -1 : 1; 
    }
    //
    // now we compare the primary key
    //
    else {
        if (primary_key == NULL) {
            //
            // primary key hidden
            //
            ulonglong a = hpk_char_to_num((uchar *) new_key_ptr);
            ulonglong b = hpk_char_to_num((uchar *) saved_key_ptr);
            ret_val =  a < b ? -1 : (a > b ? 1 : 0);
        }
        else {
            //
            // primary key not hidden, I know this is bad, basically copying the code from above
            //
            key_part = primary_key->key_part;
            end = key_part + primary_key->key_parts;
            for (; key_part != end && (int) new_key_length > 0 && (int) saved_key_length > 0; key_part++) {
                int cmp;
                uint new_key_field_length;
                uint saved_key_field_length;
                if (key_part->field->null_bit) {
                    assert(new_key_ptr   < (uchar *) new_key->data   + new_key->size);
                    assert(saved_key_ptr < (uchar *) saved_key->data + saved_key->size);
                    if (*new_key_ptr != *saved_key_ptr) {
                        return ((int) *new_key_ptr - (int) *saved_key_ptr); }
                    saved_key_ptr++;
                    new_key_length--;
                    saved_key_length--;
                    if (!*new_key_ptr++) { continue; }
                }
                new_key_field_length     = key_part->field->packed_col_length(new_key_ptr,   key_part->length);
                saved_key_field_length   = key_part->field->packed_col_length(saved_key_ptr, key_part->length);
                assert(new_key_length >= new_key_field_length);
                assert(saved_key_length >= saved_key_field_length);
                if ((cmp = key_part->field->pack_cmp(new_key_ptr, saved_key_ptr, key_part->length, 0)))
                    return cmp;
                new_key_ptr      += new_key_field_length;
                new_key_length   -= new_key_field_length;
                saved_key_ptr    += saved_key_field_length;
                saved_key_length -= saved_key_field_length;
            }
            //
            // at this point, we have compared the actual keys and the primary key, we return 0
            //
            ret_val = 0;
        }
    }
    return ret_val;
}


static int tokudb_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    KEY *primary_key = (KEY *) file->api_internal;
    if (key->flags & HA_CLUSTERING) {
        return tokudb_compare_two_clustered_keys(key, primary_key, keya, keyb);
    }
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, false);
}

static int tokudb_cmp_primary_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->api_internal;
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, false);
}

//TODO: QQQ Only do one direction for prefix.
static int tokudb_prefix_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, true);
}

int primary_key_part_compare (const void* left, const void* right) {
    PRIM_KEY_PART_INFO* left_part= (PRIM_KEY_PART_INFO *)left;
    PRIM_KEY_PART_INFO* right_part = (PRIM_KEY_PART_INFO *)right;
    return left_part->offset - right_part->offset;
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

//
// struct that will be used as a context for smart DBT callbacks
// ONLY for the function add_index
//
typedef struct smart_dbt_ai_info {
    ha_tokudb* ha; //instance to ha_tokudb needed for reading the row
    DBT* prim_key; // DBT to store the primary key
    uchar* buf; // buffer to unpack the row
    //
    // index into key_file that holds DB* that is indexed on
    // the primary_key. this->key_file[primary_index] == this->file
    //
    uint pk_index;
} *SMART_DBT_AI_INFO;

static int smart_dbt_ai_callback (DBT const *key, DBT const *row, void *context) {
    SMART_DBT_AI_INFO info = (SMART_DBT_AI_INFO)context;
    info->ha->unpack_row(info->buf,row,key, true);
    //
    // copy the key to prim_key
    // This will be used as the data value for elements in the secondary index
    // being created
    //
    info->prim_key->size = key->size;
    memcpy(info->prim_key->data, key->data, key->size);
    //
    // For clustering keys on tables with a hidden primary key, we need to copy
    // the primary key to current_ident, because that is what the function
    // create_dbt_key_from_key uses to create the key in a clustering index
    //
    info->ha->extract_hidden_primary_key(info->pk_index,row,key);
    return 0;
}

//
// smart DBT callback function for optimize
// in optimize, we want to flatten DB by doing
// a full table scan. Therefore, we don't
// want to actually do anything with the data, hence
// callback does nothing
//
static int smart_dbt_opt_callback (DBT const *key, DBT  const *row, void *context) {
  return 0;
}


//
// Smart DBT callback function in case where we have a covering index
//
static int
smart_dbt_callback_keyread(DBT const *key, DBT  const *row, void *context) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->extract_hidden_primary_key(info->keynr, row, key);
    info->ha->read_key_only(info->buf,info->keynr,row,key);
    return 0;
}

//
// Smart DBT callback function in case where we do NOT have a covering index
//
static int
smart_dbt_callback_rowread(DBT const *key, DBT  const *row, void *context) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->extract_hidden_primary_key(info->keynr, row, key);
    info->ha->read_primary_key(info->buf,info->keynr,row,key);
    return 0;
}

//
// Smart DBT callback function in c_getf_heaviside, in case where we have a covering index, 
//
static int
smart_dbt_callback_keyread_heavi(DBT const *key, DBT  const *row, void *context, int r_h) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->heavi_ret_val = r_h;
    smart_dbt_callback_keyread(key,row,context);
    return 0;
}

//
// Smart DBT callback function in c_getf_heaviside, in case where we do NOT have a covering index
//
static int
smart_dbt_callback_rowread_heavi(DBT const *key, DBT  const *row, void *context, int r_h) {
    SMART_DBT_INFO info = (SMART_DBT_INFO)context;
    info->ha->heavi_ret_val = r_h;
    smart_dbt_callback_rowread(key,row,context);
    return 0;
}

//
// Smart DBT callback function in records_in_range
//
static int
smart_dbt_callback_ror_heavi(DBT const *key, DBT  const *row, void *context, int r_h) {
    DBT* copied_key = (DBT *)context;
    copied_key->size = key->size;
    memcpy(copied_key->data, key->data, key->size);
    return 0;
}


//
// macro for Smart DBT callback function, 
// so we do not need to put this long line of code in multiple places
//
#define SMART_DBT_CALLBACK ( this->key_read ? smart_dbt_callback_keyread : smart_dbt_callback_rowread ) 


//
// macro that modifies read flag for cursor operations depending on whether
// we have preacquired lock or not
//
#define SET_READ_FLAG(flg) ((range_lock_grabbed || current_thd->options & OPTION_TABLE_LOCK) ? ((flg) | DB_PRELOCKED) : (flg))


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



inline uint get_null_offset(TABLE* table, Field* field) {
    return (uint) ((uchar*) field->null_ptr - (uchar*) table->record[0]);
}

inline bool
is_null_field( TABLE* table, Field* field, const uchar* record) {
    uint null_offset;
    bool ret_val;
    if (!field->null_ptr) {
        ret_val = false;
        goto exitpt;
    }
    null_offset = get_null_offset(table,field);
    ret_val = (record[null_offset] & field->null_bit) ? true: false;

exitpt:
    return ret_val;
}

inline ulong field_offset(Field* field, TABLE* table) {
    return((ulong) (field->ptr - table->record[0]));
}

inline HA_TOKU_ISO_LEVEL tx_to_toku_iso(ulong tx_isolation) {
    if (tx_isolation == ISO_READ_UNCOMMITTED) {
        return hatoku_iso_read_uncommitted;
    }
    else {
        return hatoku_iso_serializable;
    }
}

inline u_int32_t toku_iso_to_txn_flag (HA_TOKU_ISO_LEVEL lvl) {
    if (lvl == hatoku_iso_read_uncommitted) {
        return DB_READ_UNCOMMITTED;
    }
    else {
        return 0;
    }
}




//
// Open a secondary table, the key will be a secondary index, the data will be a primary key
//
int ha_tokudb::open_secondary_table(DB** ptr, KEY* key_info, const char* name, int mode, u_int32_t* key_type) {
    int error = ENOSYS;
    char part[MAX_ALIAS_NAME + 10];
    char name_buff[FN_REFLEN];
    char error_msg[MAX_ALIAS_NAME + 50]; //50 is arbitrary upper bound of extra txt
    uint open_flags = (mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
    DBT cmp_byte_stream;
    char* newname = NULL;
    newname = (char *)my_malloc(strlen(name) + NAME_CHAR_LEN, MYF(MY_WME));
    if (newname == NULL) {
        error = ENOMEM;
        goto cleanup;
    }

    open_flags += DB_AUTO_COMMIT;

    if ((error = db_create(ptr, db_env, 0))) {
        my_errno = error;
        goto cleanup;
    }
    sprintf(part, "key-%s", key_info->name);
    make_name(newname, name, part);
    fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
    *key_type = key_info->flags & HA_NOSAME ? DB_NOOVERWRITE : DB_YESOVERWRITE;
    (*ptr)->app_private = (void *) (key_info);
    if (tokudb_debug & TOKUDB_DEBUG_SAVE_TRACE) {
        bzero((void *) &cmp_byte_stream, sizeof(cmp_byte_stream));
        cmp_byte_stream.flags = DB_DBT_MALLOC;
        if ((error = tokutrace_db_get_cmp_byte_stream(*ptr, &cmp_byte_stream))) {
            my_errno = error;
            goto cleanup;
        }
        (*ptr)->set_bt_compare(*ptr, tokudb_cmp_packed_key);
        my_free(cmp_byte_stream.data, MYF(0));
    }
    else {
        (*ptr)->set_bt_compare(*ptr, tokudb_cmp_packed_key);    
    }
    
    DBUG_PRINT("info", ("Setting DB_DUP+DB_DUPSORT for key %s\n", key_info->name));
    //
    // clustering keys are not DB_DUP, because their keys are unique (they have the PK embedded)
    //
    if (!(key_info->flags & HA_CLUSTERING)) {
        (*ptr)->set_flags(*ptr, DB_DUP + DB_DUPSORT);
        (*ptr)->set_dup_compare(*ptr, hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_primary_key);
    }
    (*ptr)->api_internal = share->file->app_private;

    if ((error = (*ptr)->open(*ptr, 0, name_buff, NULL, DB_BTREE, open_flags, 0))) {
        if (error == TOKUDB_DIRTY_DICTIONARY) {
            sprintf(error_msg, "File %s is dirty, not opening DB", name_buff);
            sql_print_error(error_msg);
        }
        my_errno = error;
        goto cleanup;
    }
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("open:%s:file=%p\n", newname, *ptr);
    }
cleanup:
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
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
    TOKUDB_OPEN();

    char name_buff[FN_REFLEN];
    char error_msg[MAX_ALIAS_NAME + 50]; //50 is arbitrary upper bound of extra txt
    uint open_flags = (mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
    uint max_key_length;
    int error;
    char* newname = NULL;

    transaction = NULL;
    cursor = NULL;

    open_flags += DB_AUTO_COMMIT;

    newname = (char *)my_malloc(strlen(name) + NAME_CHAR_LEN,MYF(MY_WME));
    if (newname == NULL) { 
        TOKUDB_DBUG_RETURN(1);
    }

    /* Open primary key */
    hidden_primary_key = 0;
    if ((primary_key = table_share->primary_key) >= MAX_KEY) {
        // No primary key
        primary_key = table_share->keys;
        key_used_on_scan = MAX_KEY;
        ref_length = hidden_primary_key = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    } 
    else {
        key_used_on_scan = primary_key;
    }
    /* Need some extra memory in case of packed keys */
    // the "+ 1" is for the first byte that states +/- infinity
    // multiply everything by 2 to account for clustered keys having a key and primary key together
    max_key_length = 2*(table_share->max_key_length + MAX_REF_PARTS * 3 + sizeof(uchar));
    if (!(alloc_ptr =
          my_multi_malloc(MYF(MY_WME),
                          &key_buff, max_key_length, 
                          &key_buff2, max_key_length, 
                          &primary_key_buff, (hidden_primary_key ? 0 : max_key_length), 
                          NullS))) {
        my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
        TOKUDB_DBUG_RETURN(1);
    }
    if (!(rec_buff = (uchar *) my_malloc((alloced_rec_buff_length = table_share->rec_buff_length), MYF(MY_WME)))) {
        my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
        my_free(alloc_ptr, MYF(0));
        alloc_ptr = NULL;
        TOKUDB_DBUG_RETURN(1);
    }

    /* Init shared structure */
    if (!(share = get_share(name, table))) {
        my_free((char *) rec_buff, MYF(0));
        rec_buff = NULL;
        my_free(alloc_ptr, MYF(0));
        alloc_ptr = NULL;
        my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
        TOKUDB_DBUG_RETURN(1);
    }
    /* Make sorted list of primary key parts, if they exist*/
    if (!hidden_primary_key) {
        uint num_prim_key_parts = table_share->key_info[table_share->primary_key].key_parts;
        primary_key_offsets = (PRIM_KEY_PART_INFO *)my_malloc(
            num_prim_key_parts*sizeof(*primary_key_offsets), 
            MYF(MY_WME)
            );
        
        if (!primary_key_offsets) {
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            rec_buff = NULL;
            my_free(alloc_ptr, MYF(0));
            alloc_ptr = NULL;
            my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
            TOKUDB_DBUG_RETURN(1);
        }
        for (uint i = 0; i < table_share->key_info[table_share->primary_key].key_parts; i++) {
            primary_key_offsets[i].offset = table_share->key_info[table_share->primary_key].key_part[i].offset;
            primary_key_offsets[i].part_index = i;
        }
        qsort(
            primary_key_offsets, // start of array
            num_prim_key_parts, //num elements
            sizeof(*primary_key_offsets), //size of each element
            primary_key_part_compare
            );
    }

    thr_lock_data_init(&share->lock, &lock, NULL);
    bzero((void *) &current_row, sizeof(current_row));

    /* Fill in shared structure, if needed */
    pthread_mutex_lock(&share->mutex);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("tokudbopen:%p:share=%p:file=%p:table=%p:table->s=%p:%d\n", 
                     this, share, share->file, table, table->s, share->use_count);
    }
    if (!share->use_count++) {
        DBUG_PRINT("info", ("share->use_count %u", share->use_count));
        DBT cmp_byte_stream;

        if ((error = db_create(&share->file, db_env, 0))) {
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            rec_buff = NULL;
            my_free(alloc_ptr, MYF(0));
            alloc_ptr = NULL;
            if (primary_key_offsets) {
                my_free(primary_key_offsets, MYF(0));
                primary_key_offsets = NULL;
            }
            my_errno = error;
            my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
            TOKUDB_DBUG_RETURN(1);
        }

        if (!hidden_primary_key) {
            share->file->app_private = (void *) (table_share->key_info + table_share->primary_key);
        }
        else {
            share->file->app_private = NULL;
        }
        if (tokudb_debug & TOKUDB_DEBUG_SAVE_TRACE) {
            bzero((void *) &cmp_byte_stream, sizeof(cmp_byte_stream));
            cmp_byte_stream.flags = DB_DBT_MALLOC;
            if ((error = tokutrace_db_get_cmp_byte_stream(share->file, &cmp_byte_stream))) {
                free_share(share, table, hidden_primary_key, 1);
                my_free((char *) rec_buff, MYF(0));
                rec_buff = NULL;
                my_free(alloc_ptr, MYF(0));
                alloc_ptr = NULL;
                if (primary_key_offsets) {
                    my_free(primary_key_offsets, MYF(0));
                    primary_key_offsets = NULL;
                }
                my_errno = error;
                my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
                TOKUDB_DBUG_RETURN(1);
            }
            share->file->set_bt_compare(share->file, (hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_packed_key));
            my_free(cmp_byte_stream.data, MYF(0));
        }
        else
            share->file->set_bt_compare(share->file, (hidden_primary_key ? tokudb_cmp_hidden_key : tokudb_cmp_packed_key));
        
        make_name(newname, name, "main");
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
        if ((error = share->file->open(share->file, 0, name_buff, NULL, DB_BTREE, open_flags, 0))) {
            if (error == TOKUDB_DIRTY_DICTIONARY) {
                sprintf(error_msg, "File %s is dirty, not opening DB", name_buff);
                sql_print_error(error_msg);
            }
            free_share(share, table, hidden_primary_key, 1);
            my_free((char *) rec_buff, MYF(0));
            rec_buff = NULL;
            my_free(alloc_ptr, MYF(0));
            alloc_ptr = NULL;
            if (primary_key_offsets) {
                my_free(primary_key_offsets, MYF(0));
                primary_key_offsets = NULL;
            }
            my_errno = error;
            my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
            TOKUDB_DBUG_RETURN(1);
        }
        if (tokudb_debug & TOKUDB_DEBUG_OPEN)
            TOKUDB_TRACE("open:%s:file=%p\n", newname, share->file);

        /* Open other keys;  These are part of the share structure */
        share->key_file[primary_key] = share->file;
        share->key_type[primary_key] = hidden_primary_key ? DB_YESOVERWRITE : DB_NOOVERWRITE;

        DB **ptr = share->key_file;
        for (uint i = 0; i < table_share->keys; i++, ptr++) {
            if (i != primary_key) {
                if ((error = open_secondary_table(ptr,&table_share->key_info[i],name,mode,&share->key_type[i]))) {
                    __close(1);
                    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
                    TOKUDB_DBUG_RETURN(1);
                }
            }
        }
        /* Calculate pack_length of primary key */
        share->fixed_length_primary_key = 1;
        if (!hidden_primary_key) {
            //
            // I realize this is incredibly confusing, and refactoring should take 
            // care of this, but we need to set the ref_length to start at 5, to account for
            // the "infinity byte" in keys, and for placing the DBT size in the first four bytes
            //
            ref_length = sizeof(u_int32_t) + sizeof(uchar);
            KEY_PART_INFO *key_part = table->key_info[primary_key].key_part;
            KEY_PART_INFO *end = key_part + table->key_info[primary_key].key_parts;
            for (; key_part != end; key_part++) {
                ref_length += key_part->field->max_packed_col_length(key_part->length);
            }
            share->fixed_length_primary_key = (ref_length == table->key_info[primary_key].key_length + sizeof(uchar) + sizeof(u_int32_t));
            share->status |= STATUS_PRIMARY_KEY_INIT;
        }
        share->ref_length = ref_length;

        error = get_status();
        if (error || share->version < HA_TOKU_VERSION) {
            __close(1);
            my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
            TOKUDB_DBUG_RETURN(1);
        }
        //////////////////////
        u_int64_t num_rows = 0;
        int error = estimate_num_rows(share->file,&num_rows);
        //
        // estimate_num_rows should not fail under normal conditions
        //
        if (error == 0) {
            share->rows = num_rows;
        }
        else {
            __close(1);
            my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
            TOKUDB_DBUG_RETURN(1);
        }
        //
        // initialize auto increment data
        //
        share->has_auto_inc = has_auto_increment_flag(&share->ai_field_index);
        if (share->has_auto_inc) {
            init_auto_increment();
        }
    }
    ref_length = share->ref_length;     // If second open
    pthread_mutex_unlock(&share->mutex);

    key_read = false;
    stats.block_size = 1<<20;    // QQQ Tokudb DB block size
    share->fixed_length_row = !(table_share->db_create_options & HA_OPTION_PACK_RECORD);

    init_hidden_prim_key_info();

    info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(0);
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
int ha_tokudb::estimate_num_rows(DB* db, u_int64_t* num_rows) {
    DBT key;
    DBT data;
    int error = ENOSYS;
    DBC* crsr = NULL;
    u_int64_t less, equal, greater;
    int is_exact;
    bool do_commit = false;

    bzero((void *)&key, sizeof(key));
    bzero((void *)&data, sizeof(data));

    if (transaction == NULL) {
        error = db_env->txn_begin(db_env, 0, &transaction, DB_READ_UNCOMMITTED);
        if (error) goto cleanup;
        do_commit = true;
    }
    
    error = db->cursor(db, transaction, &crsr, 0);
    if (error) { goto cleanup; }

    //
    // get the first element, then estimate number of records
    // by calling key_range64 on the first element
    //
    error = crsr->c_get(crsr, &key, &data, DB_FIRST);
    if (error == DB_NOTFOUND) {
        *num_rows = 0;
        error = 0;
        goto cleanup;
    }
    else if (error) { goto cleanup; }

    error = db->key_range64(
        db, 
        transaction, 
        &key, 
        &less,
        &equal,
        &greater,
        &is_exact
        );
    if (error) {
        goto cleanup;
    }


    *num_rows = equal + greater;
    error = 0;
cleanup:
    if (do_commit) {
        transaction->commit(transaction, 0);
        transaction = NULL;
    }
    if (crsr != NULL) {
        crsr->c_close(crsr);
        crsr = NULL;
    }
    return error;
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
    uint i = 0;
    for (Field ** field = table->field; *field; field++,i++) {
        if ((*field)->flags & AUTO_INCREMENT_FLAG) {
            ai_found = true;
            *index = i;
            break;
        }
    }
    return ai_found;
}

//
// helper function to write a piece of metadata in to status.tokudb
//
int ha_tokudb::write_metadata(DB* db, HA_METADATA_KEY curr_key_data, void* data, uint size ){
    int error;
    DBT key;
    DBT value;
    DB_TXN* txn = NULL;
    //
    // transaction to be used for putting metadata into status.tokudb
    //
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { 
        goto cleanup;
    }

    bzero(&key, sizeof(key));
    bzero(&value, sizeof(value));
    key.data = &curr_key_data;
    key.size = sizeof(curr_key_data);
    value.data = data;
    value.size = size;
    error = db->put(db, txn, &key, &value, 0);
    if (error) { 
        goto cleanup; 
    }
    
    error = 0;
cleanup:
    if (txn) {
        if (!error) {
            txn->commit(txn, DB_TXN_NOSYNC);
        }
        else {
            txn->abort(txn);
        }
    }
    return error;
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
    return write_metadata(db,hatoku_max_ai,&val,sizeof(val));
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
int ha_tokudb::write_auto_inc_create(DB* db, ulonglong val){
    return write_metadata(db,hatoku_ai_create_value,&val,sizeof(val));
}


//
// Closes a handle to a table. 
//
int ha_tokudb::close(void) {
    TOKUDB_DBUG_ENTER("ha_tokudb::close %p", this);
    TOKUDB_CLOSE();
    TOKUDB_DBUG_RETURN(__close(0));
}

int ha_tokudb::__close(int mutex_is_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::__close %p", this);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) 
        TOKUDB_TRACE("close:%p\n", this);
    my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
    my_free(primary_key_offsets, MYF(MY_ALLOW_ZERO_PTR));
    rec_buff = NULL;
    alloc_ptr = NULL;
    primary_key_offsets = NULL;
    ha_tokudb::reset();         // current_row buffer
    TOKUDB_DBUG_RETURN(free_share(share, table, hidden_primary_key, mutex_is_locked));
}

//
// Reallocate record buffer (rec_buff) if needed
// If not needed, does nothing
// Parameters:
//          length - size of buffer required for rec_buff
//
bool ha_tokudb::fix_rec_buff_for_blob(ulong length) {
    if (!rec_buff || length > alloced_rec_buff_length) {
        uchar *newptr;
        if (!(newptr = (uchar *) my_realloc((void *) rec_buff, length, MYF(MY_ALLOW_ZERO_PTR))))
            return 1;
        rec_buff = newptr;
        alloced_rec_buff_length = length;
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
//      [in]    record - row in MySQL format
//

int ha_tokudb::pack_row(DBT * row, const uchar * record, bool strip_pk) {
    uchar *ptr;
    int r = ENOSYS;
    bzero((void *) row, sizeof(*row));
    uint curr_skip_index;

    KEY *key_info = table->key_info + primary_key;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    //
    // two cases, fixed length row, and variable length row
    // fixed length row is first below
    //
    if (share->fixed_length_row) {
        if (hidden_primary_key || !strip_pk) {
            row->data = (void *)record;
            row->size = table_share->reclength;
            r = 0;
            goto cleanup;
        }
        else {
            //
            // if the primary key is not hidden, then it is part of the record
            // because primary key information is already stored in the key
            // that will be passed to the fractal tree, we do not copy
            // components that belong to the primary key
            //
            if (fix_rec_buff_for_blob(table_share->reclength)) {
                r = HA_ERR_OUT_OF_MEM;
                goto cleanup;
            }

            uchar* tmp_dest = rec_buff;
            const uchar* tmp_src = record;
            uint i = 0;
            //
            // say we have 100 bytes in record, and bytes 25-50 and 75-90 belong to the primary key
            // this for loop will do a memcpy [0,25], [51,75] and [90,100]
            //
            for (i =0; i < key_info->key_parts; i++){
                uint curr_index = primary_key_offsets[i].part_index;
                uint bytes_to_copy = record + key_info->key_part[curr_index].offset - tmp_src;
                memcpy(tmp_dest,tmp_src, bytes_to_copy);
                tmp_dest += bytes_to_copy;
                tmp_src = record + key_info->key_part[curr_index].offset + key_info->key_part[curr_index].length;
            }
            memcpy(tmp_dest,tmp_src, record + table_share->reclength - tmp_src);
            tmp_dest += record + table_share->reclength - tmp_src;

            row->data = rec_buff;
            row->size = (size_t) (tmp_dest - rec_buff);

            r = 0;
            goto cleanup;
        }
    }
    
    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(record))) {
            r = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }

    /* Copy null bits */
    memcpy(rec_buff, record, table_share->null_bytes);
    ptr = rec_buff + table_share->null_bytes;

    //
    // assert that when the hidden primary key exists, primary_key_offsets is NULL
    //
    assert( (hidden_primary_key != 0) == (primary_key_offsets == NULL));
    curr_skip_index = 0;
    for (Field ** field = table->field; *field; field++) {
        uint curr_field_offset = field_offset(*field, table);
        //
        // if the primary key is hidden, primary_key_offsets will be NULL and
        // this clause will not execute
        //
        if (primary_key_offsets && 
            strip_pk &&
            curr_skip_index < table_share->key_info[table_share->primary_key].key_parts
            ) {
            uint curr_skip_offset = primary_key_offsets[curr_skip_index].offset;
            if (curr_skip_offset == curr_field_offset) {
                //
                // we have hit a field that is a portion of the primary key
                //
                uint curr_key_index = primary_key_offsets[curr_skip_index].part_index;
                curr_skip_index++;
                //
                // only choose to continue over the key if the key's length matches the field's length
                // otherwise, we may have a situation where the column is a varchar(10), the
                // key is only the first 3 characters, and we end up losing the last 7 bytes of the
                // column
                //
                if (table->key_info[primary_key].key_part[curr_key_index].length == (*field)->field_length) {
                    continue;
                }
            }
        }
        if (is_null_field(table, *field, record)) {
            continue;
        }
        ptr = (*field)->pack(ptr, (const uchar *)
                             (record + curr_field_offset));
    }

    row->data = rec_buff;
    row->size = (size_t) (ptr - rec_buff);
    r = 0;

cleanup:
    dbug_tmp_restore_column_map(table->write_set, old_map);

    return r;
}

//
// take the row passed in as a DBT*, and convert it into a row in MySQL format in record
// Parameters:
//      [out]   record - row in MySQL format
//      [in]    row - row stored in DBT to be converted
//
void ha_tokudb::unpack_row(uchar * record, DBT const *row, DBT const *key, bool pk_stripped) {
    //
    // two cases, fixed length row, and variable length row
    // fixed length row is first below
    //
    if (share->fixed_length_row) {
        if (hidden_primary_key || !pk_stripped) {
            memcpy(record, (void *) row->data, table_share->reclength);
        }
        else {
            my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
            KEY *key_info = table_share->key_info + primary_key;

            uchar* tmp_dest = record;
            uchar* tmp_src = (uchar *)row->data;
            uint i = 0;

            //
            // unpack_key will fill in parts of record that are part of the primary key
            //
            unpack_key(record, key, primary_key);

            //
            // produces the opposite effect to what happened in pack_row
            // first we fill in the parts of record that are not part of the key
            //
            for (i =0; i < key_info->key_parts; i++){
                uint curr_index = primary_key_offsets[i].part_index;
                uint bytes_to_copy = record + key_info->key_part[curr_index].offset - tmp_dest;
                memcpy(tmp_dest,tmp_src, bytes_to_copy);
                tmp_src += bytes_to_copy;
                tmp_dest = record + key_info->key_part[curr_index].offset + key_info->key_part[curr_index].length;
            }
            memcpy(tmp_dest,tmp_src, record + table_share->reclength - tmp_dest);
            dbug_tmp_restore_column_map(table->write_set, old_map);
        }
    }
    else {
        /* Copy null bits */
        my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
        const uchar *ptr = (const uchar *) row->data;
        memcpy(record, ptr, table_share->null_bytes);
        ptr += table_share->null_bytes;
        if (primary_key_offsets && pk_stripped) {
            //
            // unpack_key will fill in parts of record that are part of the primary key
            //
            unpack_key(record, key, primary_key);
        }

        //
        // fill in parts of record that are not part of the key
        //
        uint curr_skip_index = 0;
        for (Field ** field = table->field; *field; field++) {
            uint curr_field_offset = field_offset(*field, table);
            if (primary_key_offsets && 
                pk_stripped  &&
                curr_skip_index < table_share->key_info[table_share->primary_key].key_parts
                ) {
                uint curr_skip_offset = primary_key_offsets[curr_skip_index].offset;
                if (curr_skip_offset == curr_field_offset) {
                    //
                    // we have hit a field that is a portion of the primary key
                    //
                    uint curr_key_index = primary_key_offsets[curr_skip_index].part_index;
                    curr_skip_index++;
                    //
                    // only choose to continue over the key if the key's length matches the field's length
                    // otherwise, we may have a situation where the column is a varchar(10), the
                    // key is only the first 3 characters, and we end up losing the last 7 bytes of the
                    // column
                    //
                    if (table->key_info[primary_key].key_part[curr_key_index].length == (*field)->field_length) {
                        continue;
                    }
                }
            }
            //
            // null bytes MUST have been copied before doing this
            //
            if (is_null_field(table, *field, record)) {
                continue;
            }
            ptr = (*field)->unpack(record + field_offset(*field, table), ptr);
        }
        dbug_tmp_restore_column_map(table->write_set, old_map);
    }
}



u_int32_t ha_tokudb::place_key_into_mysql_buff(uchar * record, uchar* data, uint index) {
    KEY *key_info = table->key_info + index;
    KEY_PART_INFO *key_part = key_info->key_part, *end = key_part + key_info->key_parts;
    uchar *pos = data;

    for (; key_part != end; key_part++) {
        if (key_part->null_bit) {
            if (*pos++ == NULL_COL_VAL) { // Null value
                /*
                   We don't need to reset the record data as we will not access it
                   if the null data is set
                 */
                record[key_part->null_offset] |= key_part->null_bit;
                continue;
            }
            record[key_part->null_offset] &= ~key_part->null_bit;
        }
        /* tokutek change to make pack_key and unpack_key work for
           decimals */
        uint unpack_length = key_part->length;
        pos = (uchar *) key_part->field->unpack_key(record + field_offset(key_part->field, table), pos,
#if MYSQL_VERSION_ID < 50123
                                                    unpack_length);
#else
                                                    unpack_length, table->s->db_low_byte_first);
#endif
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
    u_int32_t bytes_read;
    uchar *pos = (uchar *) key->data + 1;
    bytes_read = place_key_into_mysql_buff(record,pos,index);
    if((table->key_info[index].flags & HA_CLUSTERING) && !hidden_primary_key) {
        //
        // also unpack primary key
        //
        place_key_into_mysql_buff(record,pos+bytes_read,primary_key);
    }
}




u_int32_t ha_tokudb::place_key_into_dbt_buff(KEY* key_info, uchar * buff, const uchar * record, bool* has_null, int key_length) {
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
            uint null_offset = (uint) ((char*) key_part->field->null_ptr
                            - (char*) table->record[0]);
            if (record[null_offset] & key_part->field->null_bit) {
                *curr_buff++ = NULL_COL_VAL;
                *has_null = true;
                //
                // fractal tree does not handle this falg at the moment
                // so commenting out for now
                //
                //key->flags |= DB_DBT_DUPOK;
                continue;
            }
            *curr_buff++ = NONNULL_COL_VAL;        // Store NOT NULL marker
        }
        //
        // accessing field_offset(key_part->field) instead off key_part->offset
        // because key_part->offset is SET INCORRECTLY in add_index
        // filed ticket 862 to look into this
        //
        curr_buff = key_part->field->pack_key(curr_buff, (uchar *) (record + field_offset(key_part->field, table)),
#if MYSQL_VERSION_ID < 50123
                                         key_part->length);
#else
                                         key_part->length, table->s->db_low_byte_first);
#endif
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

DBT* ha_tokudb::create_dbt_key_from_key(DBT * key, KEY* key_info, uchar * buff, const uchar * record, bool* has_null, int key_length) {
    u_int32_t size = 0;
    uchar* tmp_buff = buff;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    key->data = buff;

    //
    // first put the "infinity" byte at beginning. States if missing columns are implicitly
    // positive infinity or negative infinity. For this, because we are creating key
    // from a row, there is no way that columns can be missing, so in practice,
    // this will be meaningless. Might as well put in a value
    //
    *tmp_buff++ = COL_NEG_INF;
    size++;
    size += place_key_into_dbt_buff(key_info, tmp_buff, record, has_null, key_length);
    if (key_info->flags & HA_CLUSTERING) {
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
DBT *ha_tokudb::create_dbt_key_from_table(DBT * key, uint keynr, uchar * buff, const uchar * record, bool* has_null, int key_length) {
    TOKUDB_DBUG_ENTER("ha_tokudb::create_dbt_key_from_table");
    bzero((void *) key, sizeof(*key));
    if (hidden_primary_key && keynr == primary_key) {
        key->data = current_ident;
        key->size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        *has_null = false;
        DBUG_RETURN(key);
    }
    DBUG_RETURN(create_dbt_key_from_key(key, &table->key_info[keynr],buff,record, has_null, key_length));
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
DBT *ha_tokudb::pack_key(DBT * key, uint keynr, uchar * buff, const uchar * key_ptr, uint key_length, uchar inf_byte) {
    TOKUDB_DBUG_ENTER("ha_tokudb::pack_key");
    KEY *key_info = table->key_info + keynr;
    KEY_PART_INFO *key_part = key_info->key_part;
    KEY_PART_INFO *end = key_part + key_info->key_parts;
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);

    bzero((void *) key, sizeof(*key));
    key->data = buff;

    //
    // first put the "infinity" byte at beginning. States if missing columns are implicitly
    // positive infinity or negative infinity
    //
    *buff++ = inf_byte;

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
        buff = key_part->field->pack_key_from_key_image(buff, (uchar *) key_ptr + offset,
#if MYSQL_VERSION_ID < 50123
                                                        key_part->length);
#else
                                                        key_part->length, table->s->db_low_byte_first);
#endif
        key_ptr += key_part->store_length;
        key_length -= key_part->store_length;
    }
    key->size = (buff - (uchar *) key->data);
    DBUG_DUMP("key", (uchar *) key->data, key->size);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    DBUG_RETURN(key);
}

int ha_tokudb::read_last() {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_last");
    int do_commit = 0;
    if (transaction == NULL) {
        int r = db_env->txn_begin(db_env, 0, &transaction, 0);
        assert(r == 0);
        do_commit = 1;
    }
    int error = index_init(primary_key, 0);
    if (error == 0)
        error = index_last(table->record[1]);
    index_end();
    if (do_commit) {
        int r = transaction->commit(transaction, 0);
        assert(r == 0);
        transaction = NULL;
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// get max used hidden primary key value
//
void ha_tokudb::init_hidden_prim_key_info() {
    TOKUDB_DBUG_ENTER("ha_tokudb::init_prim_key_info");
    pthread_mutex_lock(&share->mutex);
    if (!(share->status & STATUS_PRIMARY_KEY_INIT)) {
        (void) extra(HA_EXTRA_KEYREAD);
        int error = read_last();
        (void) extra(HA_EXTRA_NO_KEYREAD);
        if (error == 0) {
            share->auto_ident = hpk_char_to_num(current_ident);
        }

        share->status |= STATUS_PRIMARY_KEY_INIT;
    }
    pthread_mutex_unlock(&share->mutex);
    DBUG_VOID_RETURN;
}



/** @brief
    Get metadata info stored in status.tokudb
    */
int ha_tokudb::get_status() {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_status");
    DB_TXN* txn = NULL;
    DBT key, value;
    HA_METADATA_KEY curr_key;
    int error;
    char error_msg[MAX_ALIAS_NAME + 50]; //50 is arbitrary upper bound of extra txt
    char* newname = NULL;
    //
    // open status.tokudb
    //
    if (!share->status_block) {
        char name_buff[FN_REFLEN];
        newname = (char *)my_malloc(get_name_length(share->table_name) + NAME_CHAR_LEN, MYF(MY_WME));
        if (newname == NULL) {
            error = ENOMEM;
            goto cleanup;
        }
        make_name(newname, share->table_name, "status");
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
        uint open_mode = (((table->db_stat & HA_READ_ONLY) ? DB_RDONLY : 0)
                          | DB_THREAD);
        if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
            TOKUDB_TRACE("open:%s\n", newname);
        }
        error = db_create(&share->status_block, db_env, 0);
        if (error) { goto cleanup; }

        error = share->status_block->open(share->status_block, NULL, name_buff, NULL, DB_BTREE, open_mode, 0);
        if (error) { 
            if (error == TOKUDB_DIRTY_DICTIONARY) {
                sprintf(error_msg, "File %s is dirty, not opening DB", name_buff);
                sql_print_error(error_msg);
            }
            goto cleanup; 
        }
    }
    
    //
    // transaction to be used for putting metadata into status.tokudb
    //
    bzero(&key, sizeof(key));
    bzero(&value, sizeof(value));
    key.data = &curr_key;
    key.size = sizeof(curr_key);
    value.flags = DB_DBT_MALLOC;
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    if (share->status_block) {
        int error;
        //
        // get version
        //
        curr_key = hatoku_version;
        error = share->status_block->get(
            share->status_block, 
            txn, 
            &key, 
            &value, 
            0
            );
        if (error == DB_NOTFOUND) {
            share->version = 0;
        }
        else if (error == 0 && value.size == sizeof(share->version)) {
            share->version = *(uint *)value.data;
            free(value.data);
            value.data = NULL;
        }
        else {
            goto cleanup;
        }
        //
        // get capabilities
        //
        curr_key = hatoku_capabilities;
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
        else if (error == 0 && value.size == sizeof(share->version)) {
            share->capabilities= *(uint *)value.data;
            free(value.data);
            value.data = NULL;
        }
        else {
            goto cleanup;
        }
    }
    error = 0;
cleanup:
    if (txn) {
        txn->commit(txn,0);
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    if (error) {
        if (share->status_block) {
            share->status_block->close(share->status_block, 0);
            share->status_block = NULL;
        }
    }
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
    KEY *key_info = NULL;

    if (hidden_primary_key) {
        ret_val =  tokudb_compare_two_hidden_keys(
            (void *)ref1, 
            TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH, 
            (void *)ref2, 
            TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH
            );
        goto exit;
    }    
    key_info = &table->key_info[table_share->primary_key];
    ret_val = tokudb_compare_two_keys(
        key_info,
        ref1 + sizeof(u_int32_t),
        *(u_int32_t *)ref1,
        ref2 + sizeof(u_int32_t),
        *(u_int32_t *)ref2,
        false
        );
exit:
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
// Stores a row in the table, called when handling an INSERT query
// Parameters:
//      [in]    record - a row in MySQL format
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::write_row(uchar * record) {
    TOKUDB_DBUG_ENTER("ha_tokudb::write_row");
    DBT row, prim_key, key;
    int error;
    THD *thd = NULL;
    u_int32_t put_flags;
    bool has_null;
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;

    //
    // some crap that needs to be done because MySQL does not properly abstract
    // this work away from us, namely filling in auto increment and setting auto timestamp
    //
    statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT) {
        table->timestamp_field->set_time();
    }
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
            error = update_max_auto_inc(share->status_block, curr_auto_inc);
            if (!error) {
                share->last_auto_increment = curr_auto_inc;
            }
        }
        pthread_mutex_unlock(&share->mutex);
    }

    if ((error = pack_row(&row, (const uchar *) record, true))){
        goto cleanup;
    }
    
    if (hidden_primary_key) {
        get_auto_primary_key(current_ident);
    }

    if (using_ignore) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, 0);
        if (error) {
            goto cleanup;
        }
    }
    
    txn = using_ignore ? sub_trans : transaction;
    //
    // first the primary key (because it must be unique, has highest chance of failure)
    //
    put_flags = share->key_type[primary_key];
    thd = ha_thd();
    if (thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
        put_flags = DB_YESOVERWRITE;
    }
    error = share->file->put(
        share->file, 
        txn, 
        create_dbt_key_from_table(&prim_key, primary_key, key_buff, record, &has_null), 
        &row, 
        put_flags
        );
    if (error) {
        last_dup_key = primary_key;
        goto cleanup;
    }
    
    //
    // now insertion for rest of indexes
    //
    for (uint keynr = 0; keynr < table_share->keys; keynr++) {
        bool cluster_row_created = false;
        if (keynr == primary_key) {
            continue;
        }
        put_flags = share->key_type[keynr];
        create_dbt_key_from_table(&key, keynr, key_buff2, record, &has_null); 

        if (put_flags == DB_NOOVERWRITE && (has_null || thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS))) {
            put_flags = DB_YESOVERWRITE;
        }
        if (table->key_info[keynr].flags & HA_CLUSTERING) {
            if (!cluster_row_created) {
                if ((error = pack_row(&row, (const uchar *) record, false))){
                   goto cleanup;
                }
                cluster_row_created = true;
            }
            error = share->key_file[keynr]->put(
                share->key_file[keynr], 
                txn,
                &key,
                &row, 
                put_flags
                );
        }
        else {
            error = share->key_file[keynr]->put(
                share->key_file[keynr], 
                txn,
                &key,
                &prim_key, 
                put_flags
                );
        }
        //
        // We break if we hit an error, unless it is a dup key error
        // and MySQL told us to ignore duplicate key errors
        //
        if (error) {
            last_dup_key = keynr;
            goto cleanup;
        }
    }

    if (!error) {
        added_rows++;
        num_added_rows_in_stmt++;
        if ((num_added_rows_in_stmt % 1000) == 0) {
            sprintf(write_status_msg, "Inserted about %llu rows", num_added_rows_in_stmt);
            thd_proc_info(thd, write_status_msg);
        }
    }
cleanup:
    if (error == DB_KEYEXIST) {
        error = HA_ERR_FOUND_DUPP_KEY;
    }
    if (sub_trans) {
        // no point in recording error value of abort.
        // nothing we can do about it anyway and it is not what
        // we want to return.
        if (error) {
            sub_trans->abort(sub_trans);
        }
        else {
            error = sub_trans->commit(sub_trans, DB_TXN_NOSYNC);
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

/* Compare if a key in a row has changed */
int ha_tokudb::key_cmp(uint keynr, const uchar * old_row, const uchar * new_row) {
    KEY_PART_INFO *key_part = table->key_info[keynr].key_part;
    KEY_PART_INFO *end = key_part + table->key_info[keynr].key_parts;

    for (; key_part != end; key_part++) {
        if (key_part->null_bit) {
            if ((old_row[key_part->null_offset] & key_part->null_bit) != (new_row[key_part->null_offset] & key_part->null_bit))
                return 1;
        }
        if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART)) {

            if (key_part->field->cmp_binary((uchar *) (old_row + key_part->offset), (uchar *) (new_row + key_part->offset), (ulong) key_part->length))
                return 1;
        } else {
            if (memcmp(old_row + key_part->offset, new_row + key_part->offset, key_part->length))
                return 1;
        }
    }
    return 0;
}


/*
  Update a row from one value to another.
  Clobbers key_buff2
*/
int ha_tokudb::update_primary_key(DB_TXN * trans, bool primary_key_changed, const uchar * old_row, DBT * old_key, const uchar * new_row, DBT * new_key) {
    TOKUDB_DBUG_ENTER("update_primary_key");
    DBT row;
    int error;

    if (primary_key_changed) {
        // Primary key changed or we are updating a key that can have duplicates.
        // Delete the old row and add a new one
        if (!(error = remove_key(trans, primary_key, old_row, old_key))) {
            if (!(error = pack_row(&row, new_row, true))) {
                if ((error = share->file->put(share->file, trans, new_key, &row, share->key_type[primary_key]))) {
                    // Probably a duplicated key; restore old key and row if needed
                    last_dup_key = primary_key;
                }
            }
        }
    } 
    else {
        // Primary key didn't change;  just update the row data
        if (!(error = pack_row(&row, new_row, true))) {
            error = share->file->put(share->file, trans, new_key, &row, 0);
        }
    }
    TOKUDB_DBUG_RETURN(error);
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
    DBT prim_key, key, old_prim_key, row;
    int error;
    bool primary_key_changed;
    bool has_null;
    THD* thd = ha_thd();
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;

    LINT_INIT(error);
    statistic_increment(table->in_use->status_var.ha_update_count, &LOCK_status);
    if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE) {
        table->timestamp_field->set_time();
    }

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



    if (hidden_primary_key) {
        primary_key_changed = 0;
        bzero((void *) &prim_key, sizeof(prim_key));
        prim_key.data = (void *) current_ident;
        prim_key.size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        old_prim_key = prim_key;
    } 
    else {
        create_dbt_key_from_table(&prim_key, primary_key, key_buff, new_row, &has_null);
        if ((primary_key_changed = key_cmp(primary_key, old_row, new_row))) {
            create_dbt_key_from_table(&old_prim_key, primary_key, primary_key_buff, old_row, &has_null);
        }
        else {
            old_prim_key = prim_key;
        }
    }

    if (using_ignore) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, 0);
        if (error) {
            goto cleanup;
        }
    }    
    txn = using_ignore ? sub_trans : transaction;

    /* Start by updating the primary key */
    error = update_primary_key(txn, primary_key_changed, old_row, &old_prim_key, new_row, &prim_key);
    if (error) {
        last_dup_key = primary_key;
        goto cleanup;
    }
    // Update all other keys
    for (uint keynr = 0; keynr < table_share->keys; keynr++) {
        bool cluster_row_created = false;
        if (keynr == primary_key) {
            continue;
        }
        if (key_cmp(keynr, old_row, new_row) || primary_key_changed) {
            u_int32_t put_flags;
            if ((error = remove_key(txn, keynr, old_row, &old_prim_key))) {
                goto cleanup;
            }
            create_dbt_key_from_table(&key, keynr, key_buff2, new_row, &has_null), 
            put_flags = share->key_type[keynr];
            if (put_flags == DB_NOOVERWRITE && (has_null || thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS))) {
                put_flags = DB_YESOVERWRITE;
            }
            if (table->key_info[keynr].flags & HA_CLUSTERING) {
                if (!cluster_row_created) {
                    if ((error = pack_row(&row, (const uchar *) new_row, false))){
                       goto cleanup;
                    }
                    cluster_row_created = true;
                }
                error = share->key_file[keynr]->put(
                    share->key_file[keynr], 
                    txn,
                    &key,
                    &row, 
                    put_flags
                    );
            }
            else {
                error = share->key_file[keynr]->put(
                    share->key_file[keynr], 
                    txn, 
                    &key,
                    &prim_key, 
                    put_flags
                    );
            }
            //
            // We break if we hit an error, unless it is a dup key error
            // and MySQL told us to ignore duplicate key errors
            //
            if (error) {
                last_dup_key = keynr;
                goto cleanup;
            }
        }
    }

    if (!error) {
        num_updated_rows_in_stmt++;
        if ((num_updated_rows_in_stmt % 1000) == 0) {
            sprintf(write_status_msg, "Updated about %llu rows", num_updated_rows_in_stmt);
            thd_proc_info(thd, write_status_msg);
        }
    }


cleanup:
    if (error == DB_KEYEXIST) {
        error = HA_ERR_FOUND_DUPP_KEY;
    }
    if (sub_trans) {
        // no point in recording error value of abort.
        // nothing we can do about it anyway and it is not what
        // we want to return.
        if (error) {
            sub_trans->abort(sub_trans);
        }
        else {
            error = sub_trans->commit(sub_trans, DB_TXN_NOSYNC);
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

//
//
// Delete one key in key_file[keynr]
// This uses key_buff2, when keynr != primary key, so it's important that
// a function that calls this doesn't use this buffer for anything else.
// Parameters:
//      [in]    trans - transaction to be used for the delete
//              keynr - index for which a key needs to be deleted
//      [in]    record - row in MySQL format. Must delete a key for this row
//      [in]    prim_key - key for record in primary table
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::remove_key(DB_TXN * trans, uint keynr, const uchar * record, DBT * prim_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::remove_key");
    int error;
    DBT key;
    bool has_null;
    DBUG_PRINT("enter", ("index: %d", keynr));
    DBUG_PRINT("primary", ("index: %d", primary_key));
    DBUG_DUMP("prim_key", (uchar *) prim_key->data, prim_key->size);

    if (keynr == active_index && cursor) {
        error = cursor->c_del(cursor, 0);
    }
    else if (keynr == primary_key) {  // Unique key
        DBUG_PRINT("Unique key", ("index: %d", keynr));
        error = share->key_file[keynr]->del(share->key_file[keynr], trans, prim_key , DB_DELETE_ANY);
    }
    else if (table->key_info[keynr].flags & HA_CLUSTERING) {
        DBUG_PRINT("clustering key", ("index: %d", keynr));
        create_dbt_key_from_table(&key, keynr, key_buff2, record, &has_null);
        error = share->key_file[keynr]->del(share->key_file[keynr], trans, &key , DB_DELETE_ANY);
    }
    else {
        create_dbt_key_from_table(&key, keynr, key_buff2, record, &has_null);
        error = share->key_file[keynr]->delboth(
            share->key_file[keynr],
            trans,
            &key,
            prim_key,
            DB_DELETE_ANY
            );
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Delete all keys for new_record
// Parameters:
//      [in]    trans - transaction to be used for the delete
//      [in]    record - row in MySQL format. Must delete all keys for this row
//      [in]    prim_key - key for record in primary table
//      [in]    keys - array that states if a key is set, and hence needs 
//                  removal
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::remove_keys(DB_TXN * trans, const uchar * record, DBT * prim_key, key_map * keys) {
    int result = 0;
    for (uint keynr = 0; keynr < table_share->keys + test(hidden_primary_key); keynr++) {
        if (keys->is_set(keynr)) {
            int new_error = remove_key(trans, keynr, record, prim_key);
            if (new_error) {
                result = new_error;     // Return last error
                break;          // Let rollback correct things
            }
        }
    }
    return result;
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
    DBT prim_key;
    key_map keys = table_share->keys_in_use;
    bool has_null;
    THD* thd = ha_thd();
    statistic_increment(table->in_use->status_var.ha_delete_count, &LOCK_status);

    create_dbt_key_from_table(&prim_key, primary_key, key_buff, record, &has_null);
    if (hidden_primary_key) {
        keys.set_bit(primary_key);
    }
    /* Subtransactions may be used in order to retry the delete in
       case we get a DB_LOCK_DEADLOCK error. */
    DB_TXN *sub_trans = transaction;
    error = remove_keys(sub_trans, record, &prim_key, &keys);
    if (error) {
        DBUG_PRINT("error", ("Got error %d", error));
    }
    else {
        deleted_rows++;
        num_deleted_rows_in_stmt++;
        if ((num_deleted_rows_in_stmt % 1000) == 0) {
            sprintf(write_status_msg, "Deleted about %llu rows", num_deleted_rows_in_stmt);
            thd_proc_info(thd, write_status_msg);
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// Notification that a scan of entire secondary table is about
// to take place. Will pre acquire table read lock
// Returns:
//      0 on success
//      error otherwise
//
int ha_tokudb::prepare_index_scan() {
    int error;
    DB* db = share->key_file[active_index];
    error = db->pre_acquire_read_lock(
        db, 
        transaction, 
        db->dbt_neg_infty(), db->dbt_neg_infty(), 
        db->dbt_pos_infty(), db->dbt_pos_infty()
        );
    if (error) { last_cursor_error = error; goto cleanup; }

    range_lock_grabbed = true;
    error = 0;
cleanup:
    return error;
}


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
    int error;
    DBUG_PRINT("enter", ("table: '%s'  key: %d", table_share->table_name.str, keynr));

    /*
       Under some very rare conditions (like full joins) we may already have
       an active cursor at this point
     */
    if (cursor) {
        DBUG_PRINT("note", ("Closing active cursor"));
        cursor->c_close(cursor);
    }
    active_index = keynr;
    last_cursor_error = 0;
    range_lock_grabbed = false;
    DBUG_ASSERT(keynr <= table->s->keys);
    DBUG_ASSERT(share->key_file[keynr]);
    if ((error = share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, 0))) {
        last_cursor_error = error;
        cursor = NULL;             // Safety
    }
    bzero((void *) &last_key, sizeof(last_key));
    TOKUDB_DBUG_RETURN(error);
}

//
// closes the local cursor
//
int ha_tokudb::index_end() {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_end %p", this);
    int error = 0;
    range_lock_grabbed = false;
    if (cursor) {
        DBUG_PRINT("enter", ("table: '%s'", table_share->table_name.str));
        error = cursor->c_close(cursor);
        cursor = NULL;
        last_cursor_error = 0;
    }
    active_index = MAX_KEY;
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::handle_cursor_error(int error, int err_to_return, uint keynr) {
    TOKUDB_DBUG_ENTER("ha_tokudb::handle_cursor_error");
    if (error) {
        last_cursor_error = error;
        table->status = STATUS_NOT_FOUND;
        cursor->c_close(cursor);
        cursor = NULL;
        if (error == DB_NOTFOUND) {
            error = err_to_return;
            if ((share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, 0))) {
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
void ha_tokudb::extract_hidden_primary_key(uint keynr, DBT const *row, DBT const *found_key) {
    //
    // extract hidden primary key to current_ident
    //
    if (hidden_primary_key) {
        if (keynr == primary_key) {
            memcpy_fixed(current_ident, (char *) found_key->data, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        }
        //
        // if clustering key, hidden primary key is at end of found_key
        //
        else if (table->key_info[keynr].flags & HA_CLUSTERING) {
            memcpy_fixed(
                current_ident, 
                (char *) found_key->data + found_key->size - TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH, 
                TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH
                );
        }
        else {
            memcpy_fixed(current_ident, (char *) row->data, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        }
    }
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
void ha_tokudb::read_key_only(uchar * buf, uint keynr, DBT const *row, DBT const *found_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_key_only");
    table->status = 0;
    unpack_key(buf, found_key, keynr);
    if (!hidden_primary_key && (keynr != primary_key) && !(table->key_info[keynr].flags & HA_CLUSTERING)) {
        unpack_key(buf, row, primary_key);
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
void ha_tokudb::read_primary_key(uchar * buf, uint keynr, DBT const *row, DBT const *found_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_primary_key");
    table->status = 0;
    //
    // case where we read from secondary table that is not clustered
    //
    if (keynr != primary_key && !(table->key_info[keynr].flags & HA_CLUSTERING)) {
        //
        // create a DBT that has the same data as row,
        //
        bzero((void *) &last_key, sizeof(last_key));
        last_key.data = key_buff;
        last_key.size = row->size;
        memcpy(key_buff, row->data, row->size);
    }
    //
    // case we read from clustered key, so unpack the entire row
    //
    else if (keynr != primary_key && (table->key_info[keynr].flags & HA_CLUSTERING)) {
        unpack_row(buf, row, found_key, false);
    }
    //
    // case we read from the primary key, so unpack the entire row
    //
    else {
        unpack_row(buf, row, found_key, true);
    }
    if (found_key) { DBUG_DUMP("read row key", (uchar *) found_key->data, found_key->size); }
    DBUG_VOID_RETURN;
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
    int error;
    //
    // Read the data into current_row, assumes key is stored in this->last_key
    //
    current_row.flags = DB_DBT_REALLOC;
    if ((error = share->file->get(share->file, transaction, &last_key, &current_row, 0))) {
        table->status = STATUS_NOT_FOUND;
        TOKUDB_DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
    }
    unpack_row(buf, &current_row, &last_key, true);

    TOKUDB_DBUG_RETURN(0);
}


//
// The funtion read_row checks whether the row was obtained from the primary table or 
// from an index table. If it was obtained from an index table, it further dereferences on
// the main table. In the end, the read_row function will manage to return the actual row
// of interest in the buf parameter.
//
// Parameters:
//      [out]   buf - buffer for the row, in MySQL format
//              keynr - index into key_file that represents DB we are currently operating on.
//      [in]    row - the row that has been read from the preceding DB call
//      [in]    found_key - key used to retrieve the row
//
int ha_tokudb::read_row(uchar * buf, uint keynr, DBT const *row, DBT const *found_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_row");
    int error;

    extract_hidden_primary_key(keynr, row, found_key);

    table->status = 0;
    //
    // if the index shows that the table we read the row from was indexed on the primary key,
    // that means we have our row and can skip
    // this entire if clause. All that is required is to unpack row.
    // if the index shows that what we read was from a table that was NOT indexed on the 
    // primary key, then we must still retrieve the row, as the "row" value is indeed just
    // a primary key, whose row we must still read
    //
    if (keynr != primary_key) {
        if (key_read && found_key) {
            // TOKUDB_DBUG_DUMP("key=", found_key->data, found_key->size);
            unpack_key(buf, found_key, keynr);
            if (!hidden_primary_key && !(table->key_info[keynr].flags & HA_CLUSTERING)) {
                // TOKUDB_DBUG_DUMP("row=", row->data, row->size);
                unpack_key(buf, row, primary_key);
            }
            TOKUDB_DBUG_RETURN(0);
        }
        //
        // in this case we have a clustered key, so no need to do pt query
        //
        if (table->key_info[keynr].flags & HA_CLUSTERING) {
            unpack_row(buf, row, found_key, false);
            TOKUDB_DBUG_RETURN(0);
        }
        //
        // create a DBT that has the same data as row,
        //
        DBT key;
        bzero((void *) &key, sizeof(key));
        key.data = key_buff;
        key.size = row->size;
        memcpy(key_buff, row->data, row->size);
        //
        // Read the data into current_row
        //
        current_row.flags = DB_DBT_REALLOC;
        if ((error = share->file->get(share->file, transaction, &key, &current_row, 0))) {
            table->status = STATUS_NOT_FOUND;
            TOKUDB_DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
        }
        unpack_row(buf, &current_row, &key, true);
    }
    else {
        //
        // in this case we are dealing with the primary key
        //
        if (key_read && !hidden_primary_key) {
            unpack_key(buf, found_key, keynr);
        }
        else {
            unpack_row(buf, row, found_key, true);
        }
    }
    if (found_key) { DBUG_DUMP("read row key", (uchar *) found_key->data, found_key->size); }
    TOKUDB_DBUG_RETURN(0);
}

//
// This is only used to read whole keys
// According to InnoDB handlerton: Positions an index cursor to the index 
// specified in keynr. Fetches the row if any
// Parameters:
//      [out]        buf - buffer for the  returned row
//                   keynr - index to use
//      [in]         key - key value, according to InnoDB, if NULL, 
//                              position cursor at start or end of index,
//                              not sure if this is done now
//                     key_len - length of key
//                     find_flag - according to InnoDB, search flags from my_base.h
// Returns:
//      0 on success
//      HA_ERR_KEY_NOT_FOUND if not found (per InnoDB), 
//      error otherwise
//
int ha_tokudb::index_read_idx(uchar * buf, uint keynr, const uchar * key, uint key_len, enum ha_rkey_function find_flag) {
    TOKUDB_DBUG_ENTER("ha_tokudb::index_read_idx");
    int error;
    table->in_use->status_var.ha_read_key_count++;
    current_row.flags = DB_DBT_REALLOC;
    active_index = MAX_KEY;

    error = share->key_file[keynr]->get(share->key_file[keynr], transaction, pack_key(&last_key, keynr, key_buff, key, key_len, COL_NEG_INF), &current_row, 0);
    if (error == DB_NOTFOUND) {
        error = HA_ERR_KEY_NOT_FOUND;
        goto cleanup;
    }
    if (!error) {
        error = read_row(buf, keynr, &current_row, &last_key);
    }
cleanup:
    TOKUDB_DBUG_RETURN(error);
}


//
// context information for the heaviside functions.
// Context information includes data necessary
// to perform comparisons
//
typedef struct heavi_info {
    DB        *db;
    const DBT *key;
} *HEAVI_INFO;

//
// effect:
//  heaviside function used for HA_READ_AFTER_KEY.
//  to use this heaviside function in ha_read_after_key, use direction>0
//  the stored key (in heavi_info) contains a prefix of the columns in the candidate
//  keys. only the columns in the stored key will be used for comparison.
//
// parameters:
//  [in]    key - candidate key in db that is being compared
//  [in]    value - candidate value, unused
//  [in]    extra_h - a heavi_info that contains information necessary for
//              the comparison
// returns:
//  >0 : candidate key > stored key
//  <0 : otherwise
// examples:
//  columns: (a,b,c,d)
//  stored key = (3,4) (only a,b)
//  candidate keys have (a,b,c,d)
//  (3,2,1,1) < (3,4)
//  (3,4,1,1) == (3,4)
//  (3,5,1,1) > (3,4)
//
static int after_key_heavi(const DBT *key, const DBT *value, void *extra_h) {
    HEAVI_INFO info = (HEAVI_INFO)extra_h;
    int cmp = tokudb_prefix_cmp_packed_key(info->db, key, info->key);
    return cmp>0 ? 1 : -1;
}

//
// effect:
//  heaviside function used for HA_READ_PREFIX_LAST_OR_PREV.
//  to use this heaviside function in HA_READ_PREFIX_LAST_OR_PREV, use direction<0
//  the stored key (in heavi_info) contains a prefix of the columns in the candidate
//  keys. only the columns in the stored key will be used for comparison.
//
// parameters:
//  [in]    key - candidate key in db that is being compared
//  [in]    value - candidate value, unused
//  [in]    extra_h - a heavi_info that contains information necessary for
//              the comparison
// returns:
//  >0 : candidate key > stored key
//  0  : candidate key == stored key
//  <0 : candidate key < stored key
// examples:
//  columns: (a,b,c,d)
//  stored key = (3,4) (only a,b)
//  candidate keys have (a,b,c,d)
//  (3,2,1,1) < (3,4)
//  (3,4,1,1) == (3,4)
//  (3,5,1,1) > (3,4)
//
static int prefix_last_or_prev_heavi(const DBT *key, const DBT *value, void *extra_h) {
    HEAVI_INFO info = (HEAVI_INFO)extra_h;
    int cmp = tokudb_prefix_cmp_packed_key(info->db, key, info->key);
    return cmp;
}

//
// effect:
//  heaviside function used for HA_READ_BEFORE_KEY.
//  to use this heaviside function in HA_READ_BEFORE_KEY, use direction<0
//  the stored key (in heavi_info) contains a prefix of the columns in the candidate
//  keys. only the columns in the stored key will be used for comparison.
//
// parameters:
//  [in]    key - candidate key in db that is being compared
//  [in]    value - candidate value, unused
//  [in]    extra_h - a heavi_info that contains information necessary for
//              the comparison
// returns:
//  <0 : candidate key < stored key
//  >0 : otherwise
// examples:
//  columns: (a,b,c,d)
//  stored key = (3,4) (only a,b)
//  candidate keys have (a,b,c,d)
//  (3,2,1,1) < (3,4)
//  (3,4,1,1) == (3,4)
//  (3,5,1,1) > (3,4)
//
static int before_key_heavi(const DBT *key, const DBT *value, void *extra_h) {
    HEAVI_INFO info = (HEAVI_INFO)extra_h;
    int cmp = tokudb_prefix_cmp_packed_key(info->db, key, info->key);
    return (cmp<0) ? -1 : 1;
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
    // TOKUDB_DBUG_DUMP("key=", key, key_len);
    DBT row;
    int error;    
    u_int32_t flags = 0;
    struct smart_dbt_info info;
    struct heavi_info heavi_info;
    bool do_read_row = true;

    HANDLE_INVALID_CURSOR();

    table->in_use->status_var.ha_read_key_count++;
    bzero((void *) &row, sizeof(row));
    pack_key(&last_key, active_index, key_buff, key, key_len, COL_NEG_INF);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    heavi_info.db = share->key_file[active_index];
    heavi_info.key = &last_key;
    switch (find_flag) {
    case HA_READ_KEY_EXACT: /* Find first record else error */
        flags = SET_READ_FLAG(DB_SET_RANGE);
        error = cursor->c_get(cursor, &last_key, &row, flags);
        if (error == 0) {
            DBT orig_key;
            pack_key(&orig_key, active_index, key_buff2, key, key_len, COL_NEG_INF);
            if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key)) {
                error = DB_NOTFOUND;
            }
        }
        break;
    case HA_READ_AFTER_KEY: /* Find next rec. after key-record */
        flags = SET_READ_FLAG(0);
        error = cursor->c_getf_heaviside(
            cursor, flags, 
            key_read ? smart_dbt_callback_keyread_heavi : smart_dbt_callback_rowread_heavi, &info,
            after_key_heavi, &heavi_info, 
            1
            );
        do_read_row = false;
        break;
    case HA_READ_BEFORE_KEY: /* Find next rec. before key-record */
        flags = SET_READ_FLAG(0);
        error = cursor->c_getf_heaviside(
            cursor, flags, 
            key_read ? smart_dbt_callback_keyread_heavi : smart_dbt_callback_rowread_heavi, &info,
            before_key_heavi, &heavi_info, 
            -1
            );
        do_read_row = false;
        break;
    case HA_READ_KEY_OR_NEXT: /* Record or next record */
        flags = SET_READ_FLAG(DB_SET_RANGE);
        error = cursor->c_get(cursor, &last_key, &row, flags);
        break;
    case HA_READ_KEY_OR_PREV: /* Record or previous */
        flags = SET_READ_FLAG(DB_SET_RANGE);
        error = cursor->c_get(cursor, &last_key, &row, flags);
        if (error == 0) {
            DBT orig_key; 
            pack_key(&orig_key, active_index, key_buff2, key, key_len, COL_NEG_INF);
            if (tokudb_prefix_cmp_packed_key(share->key_file[active_index], &orig_key, &last_key) != 0) {
                error = cursor->c_get(cursor, &last_key, &row, DB_PREV);
            }
        }
        else if (error == DB_NOTFOUND)
            error = cursor->c_get(cursor, &last_key, &row, DB_LAST);
        break;
    case HA_READ_PREFIX_LAST_OR_PREV: /* Last or prev key with the same prefix */
        flags = SET_READ_FLAG(0);
        error = cursor->c_getf_heaviside(
            cursor, flags, 
            key_read ? smart_dbt_callback_keyread_heavi : smart_dbt_callback_rowread_heavi, &info,
            prefix_last_or_prev_heavi, &heavi_info, 
            -1
            );
        do_read_row = false;
        break;
    case HA_READ_PREFIX_LAST:
        flags = SET_READ_FLAG(0);
        error = cursor->c_getf_heaviside(
            cursor, flags, 
            key_read ? smart_dbt_callback_keyread_heavi : smart_dbt_callback_rowread_heavi, &info,
            prefix_last_or_prev_heavi, &heavi_info, 
            -1
            );
        if (!error && heavi_ret_val != 0) {
            error = DB_NOTFOUND;
        }
        do_read_row = false;
        break;
    default:
        TOKUDB_TRACE("unsupported:%d\n", find_flag);
        error = HA_ERR_UNSUPPORTED;
        break;
    }
    error = handle_cursor_error(error,HA_ERR_KEY_NOT_FOUND,active_index);
    if (!error && do_read_row) {
        error = read_row(buf, active_index, &row, &last_key);
    }
    else if (!error && !key_read && active_index != primary_key && !(table->key_info[active_index].flags & HA_CLUSTERING)) {
        error = read_full_row(buf);
    }
    
    if (error && (tokudb_debug & TOKUDB_DEBUG_ERROR)) {
        TOKUDB_TRACE("error:%d:%d\n", error, find_flag);
    }
cleanup:
    TOKUDB_DBUG_RETURN(error);
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
    int error; 
    struct smart_dbt_info info;
    u_int32_t flags = SET_READ_FLAG(0);
    HANDLE_INVALID_CURSOR();
    
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;
    error = handle_cursor_error(cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info), HA_ERR_END_OF_FILE,active_index);
    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }
cleanup:
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
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next_same %p", this);
    int error;
    struct smart_dbt_info info;
    HANDLE_INVALID_CURSOR();
    
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);
    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;
    /* QQQ NEXT_DUP on nodup returns EINVAL for tokudb */
    if (keylen == table->key_info[active_index].key_length && 
        !(table->key_info[active_index].flags & HA_NOSAME) && 
        !(table->key_info[active_index].flags & HA_END_SPACE_KEY)) {

        u_int32_t flags = SET_READ_FLAG(0);
        error = handle_cursor_error(cursor->c_getf_next_dup(cursor, flags, SMART_DBT_CALLBACK, &info),HA_ERR_END_OF_FILE,active_index);
        if (!error && !key_read && active_index != primary_key && !(table->key_info[active_index].flags & HA_CLUSTERING)) {
            error = read_full_row(buf);
        }
    } else {
        u_int32_t flags = SET_READ_FLAG(0);
        error = handle_cursor_error(cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info),HA_ERR_END_OF_FILE,active_index);
        if (!error && !key_read && active_index != primary_key && !(table->key_info[active_index].flags & HA_CLUSTERING)) {
            error = read_full_row(buf);
        }
        if (!error &&::key_cmp_if_same(table, key, active_index, keylen))
            error = HA_ERR_END_OF_FILE;
    }
cleanup:
    TOKUDB_DBUG_RETURN(error);
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
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next");
    int error; 
    struct smart_dbt_info info;
    u_int32_t flags = SET_READ_FLAG(0);
    HANDLE_INVALID_CURSOR();
    
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;
    error = handle_cursor_error(cursor->c_getf_prev(cursor, flags, SMART_DBT_CALLBACK, &info),HA_ERR_END_OF_FILE,active_index);
    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }

cleanup:
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
    int error;
    DBT row;
    HANDLE_INVALID_CURSOR();
    statistic_increment(table->in_use->status_var.ha_read_first_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));

    error = handle_cursor_error(cursor->c_get(cursor, &last_key, &row, DB_FIRST),HA_ERR_END_OF_FILE,active_index);
    if (!error) {
        error = read_row(buf, active_index, &row, &last_key);
    }

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
    int error;
    DBT row;
    HANDLE_INVALID_CURSOR();
    statistic_increment(table->in_use->status_var.ha_read_last_count, &LOCK_status);
    bzero((void *) &row, sizeof(row));

    error = handle_cursor_error(cursor->c_get(cursor, &last_key, &row, DB_LAST),HA_ERR_END_OF_FILE,active_index);
    if (!error) {
        error = read_row(buf, active_index, &row, &last_key);
    }
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
    int error;
    current_row.flags = DB_DBT_REALLOC;
    range_lock_grabbed = false;
    if (scan) {
        DB* db = share->key_file[primary_key];
        error = db->pre_acquire_read_lock(db, transaction, db->dbt_neg_infty(), NULL, db->dbt_pos_infty(), NULL);
        if (error) { last_cursor_error = error; goto cleanup; }
    }
    error = index_init(primary_key, 0);
    if (error) { goto cleanup;}

    //
    // only want to set range_lock_grabbed to true after index_init
    // successfully executed for two reasons:
    // 1) index_init will reset it to false anyway
    // 2) if it fails, we don't want prelocking on,
    //
    if (scan) { range_lock_grabbed = true; }
    error = 0;
cleanup:
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
    int error;
    u_int32_t flags = SET_READ_FLAG(0);
    struct smart_dbt_info info;

    HANDLE_INVALID_CURSOR();
    //
    // The reason we do not just call index_next is that index_next 
    // increments a different variable than we do here
    //
    statistic_increment(table->in_use->status_var.ha_read_rnd_next_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = primary_key;
    
    error = handle_cursor_error(cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info),HA_ERR_END_OF_FILE,primary_key);
cleanup:
    TOKUDB_DBUG_RETURN(error);
}


DBT *ha_tokudb::get_pos(DBT * to, uchar * pos) {
    TOKUDB_DBUG_ENTER("ha_tokudb::get_pos");
    /* We don't need to set app_data here */
    bzero((void *) to, sizeof(*to));
    if (hidden_primary_key) {
        to->data = pos;
        to->size = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    }
    else {
        to->data = pos + sizeof(u_int32_t);
        to->size = *(u_int32_t *)pos;
    }
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
    int error;
    statistic_increment(table->in_use->status_var.ha_read_rnd_count, &LOCK_status);
    active_index = MAX_KEY;
    DBT* key = get_pos(&db_pos, pos); 
    error = share->file->get(share->file, transaction, key, &current_row, 0);
    if (error == DB_NOTFOUND) {
        error = HA_ERR_KEY_NOT_FOUND;
        goto cleanup;
    }    

    if (!error) {
        error = read_row(buf, primary_key, &current_row, key);
    }
cleanup:
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::read_range_first(
    const key_range *start_key,
    const key_range *end_key,
    bool eq_range, 
    bool sorted) 
{
    TOKUDB_DBUG_ENTER("ha_tokudb::read_range_first");
    int error;
    DBT start_dbt_key;
    const DBT* start_dbt_data = NULL;
    DBT end_dbt_key;
    const DBT* end_dbt_data = NULL;
    uchar* start_key_buff  = NULL;
    uchar* end_key_buff = NULL;
    start_key_buff = (uchar *)my_malloc(table_share->max_key_length + MAX_REF_PARTS * 3 + sizeof(uchar), MYF(MY_WME));
    if (start_key_buff == NULL) {
        error = ENOMEM;
        goto cleanup;
    }
    end_key_buff = (uchar *)my_malloc(table_share->max_key_length + MAX_REF_PARTS * 3 + sizeof(uchar), MYF(MY_WME));
    if (end_key_buff == NULL) {
        error = ENOMEM;
        goto cleanup;
    }


    bzero((void *) &start_dbt_key, sizeof(start_dbt_key));
    bzero((void *) &end_dbt_key, sizeof(end_dbt_key));
    range_lock_grabbed = false;


    if (start_key) {
        switch (start_key->flag) {
        case HA_READ_AFTER_KEY:
            pack_key(&start_dbt_key, active_index, start_key_buff, start_key->key, start_key->length, COL_POS_INF);
            start_dbt_data = share->key_file[active_index]->dbt_pos_infty();
            break;
        default:
            pack_key(&start_dbt_key, active_index, start_key_buff, start_key->key, start_key->length, COL_NEG_INF);
            start_dbt_data = share->key_file[active_index]->dbt_neg_infty();
            break;
        }
    }
    else {
        start_dbt_data = share->key_file[active_index]->dbt_neg_infty();
    }

    if (end_key) {
        switch (end_key->flag) {
        case HA_READ_BEFORE_KEY:
            pack_key(&end_dbt_key, active_index, end_key_buff, end_key->key, end_key->length, COL_NEG_INF);
            end_dbt_data = share->key_file[active_index]->dbt_neg_infty();
            break;
        default:
            pack_key(&end_dbt_key, active_index, end_key_buff, end_key->key, end_key->length, COL_POS_INF);
            end_dbt_data = share->key_file[active_index]->dbt_pos_infty();
            break;
        }
        
    }
    else {
        end_dbt_data = share->key_file[active_index]->dbt_pos_infty();
    }

    

    error = share->key_file[active_index]->pre_acquire_read_lock(
        share->key_file[active_index], 
        transaction, 
        start_key ? &start_dbt_key : share->key_file[active_index]->dbt_neg_infty(), 
        start_dbt_data, 
        end_key ? &end_dbt_key : share->key_file[active_index]->dbt_pos_infty(), 
        end_dbt_data
        );
    if (error){ 
        last_cursor_error = error;
        //
        // cursor should be initialized here, but in case it is not, we still check
        //
        if (cursor) {
            cursor->c_close(cursor);
            cursor = NULL;
        }
        goto cleanup; 
    }
    range_lock_grabbed = true;
    error = handler::read_range_first(start_key, end_key, eq_range, sorted);

cleanup:
    my_free(start_key_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(end_key_buff, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
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
        DBUG_ASSERT(ref_length == TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        memcpy_fixed(ref, (char *) current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
    } 
    else {
        bool has_null;
        //
        // save the data
        //
        create_dbt_key_from_table(&key, primary_key, ref + sizeof(u_int32_t), record, &has_null);
        //
        // save the size of data in the first four bytes of ref
        //
        memcpy(ref, &key.size, sizeof(u_int32_t));
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
    TOKUDB_DBUG_ENTER("ha_tokudb::info %p %d %lld", this, flag, share->rows);
    if (flag & HA_STATUS_VARIABLE) {
        // Just to get optimizations right
        stats.records = share->rows + share->rows_from_locked_table;
        stats.deleted = 0;
    }
    if ((flag & HA_STATUS_CONST)) {
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
    TOKUDB_DBUG_RETURN(0);
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
    if (current_row.flags & (DB_DBT_MALLOC | DB_DBT_REALLOC)) {
        current_row.flags = 0;
        if (current_row.data) {
            free(current_row.data);
            current_row.data = 0;
        }
    }
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
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    if (lt == lock_read) {
        for (uint i = 0; i < curr_num_DBs; i++) {
            DB* db = share->key_file[i];
            error = db->pre_acquire_read_lock(
                db, 
                trans, 
                db->dbt_neg_infty(), db->dbt_neg_infty(), 
                db->dbt_pos_infty(), db->dbt_pos_infty()
                );
            if (error) { goto cleanup; }
        }
    }
    else if (lt == lock_write) {
        for (uint i = 0; i < curr_num_DBs; i++) {
            DB* db = share->key_file[i];
            error = db->pre_acquire_table_lock(db, trans);
            if (error) { goto cleanup; }
        }
    }
    else {
        error = ENOSYS;
        goto cleanup;
    }

    error = 0;
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
    TOKUDB_DBUG_ENTER("ha_tokudb::external_lock %d", thd_sql_command(thd));
    // QQQ this is here to allow experiments without transactions
    int error = 0;
    ulong tx_isolation = thd_tx_isolation(thd);
    HA_TOKU_ISO_LEVEL toku_iso_level = tx_to_toku_iso(tx_isolation);
    tokudb_trx_data *trx = NULL;

    //
    // reset per-stmt variables
    //
    num_added_rows_in_stmt = 0;
    num_deleted_rows_in_stmt = 0;
    num_updated_rows_in_stmt = 0;

    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!trx) {
        trx = (tokudb_trx_data *) my_malloc(sizeof(*trx), MYF(MY_ZEROFILL));
        if (!trx) {
            error = 1;
            goto cleanup;
        }
        trx->iso_level = hatoku_iso_not_set;
        thd_data_set(thd, tokudb_hton->slot, trx);
    }
    if (trx->all == NULL) {
        trx->sp_level = NULL;
    }
    if (lock_type != F_UNLCK) {
        if (!trx->tokudb_lock_count++) {
            DBUG_ASSERT(trx->stmt == 0);
            transaction = NULL;    // Safety
            /* First table lock, start transaction */
            if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK)) && 
                 !trx->all &&
                 (thd_sql_command(thd) != SQLCOM_CREATE_TABLE) &&
                 (thd_sql_command(thd) != SQLCOM_DROP_TABLE) &&
                 (thd_sql_command(thd) != SQLCOM_ALTER_TABLE)) {
                /* QQQ We have to start a master transaction */
                DBUG_PRINT("trans", ("starting transaction all:  options: 0x%lx", (ulong) thd->options));
                //
                // set the isolation level for the tranaction
                //
                trx->iso_level = toku_iso_level;
                if ((error = db_env->txn_begin(db_env, NULL, &trx->all, toku_iso_to_txn_flag(toku_iso_level)))) {
                    trx->tokudb_lock_count--;      // We didn't get the lock
                    goto cleanup;
                }
                if (tokudb_debug & TOKUDB_DEBUG_TXN) {
                    TOKUDB_TRACE("master:%p\n", trx->all);
                }
                trx->sp_level = trx->all;
                trans_register_ha(thd, TRUE, tokudb_hton);
                if (thd->in_lock_tables && thd_sql_command(thd) == SQLCOM_LOCK_TABLES) {
                    //
                    // grab table locks
                    // For the command "Lock tables foo read, bar read"
                    // This statement is grabbing the locks for the table
                    // foo. The locks for bar will be grabbed when 
                    // trx->tokudb_lock_count has been initialized
                    //
                    if (lock.type <= TL_READ_NO_INSERT) {
                        error = acquire_table_lock(trx->all,lock_read);
                    }
                    else {
                        error = acquire_table_lock(trx->all,lock_write);
                    }
                    // Don't create stmt trans
                    if (error) {trx->tokudb_lock_count--;}
                    goto cleanup;
                }
            }
            DBUG_PRINT("trans", ("starting transaction stmt"));
            if (trx->stmt) { 
                if (tokudb_debug & TOKUDB_DEBUG_TXN) {
                    TOKUDB_TRACE("warning:stmt=%p\n", trx->stmt);
                }
            }
            u_int32_t txn_begin_flags;
            if (trx->iso_level == hatoku_iso_not_set) {
                txn_begin_flags = toku_iso_to_txn_flag(toku_iso_level);
            }
            else {
                txn_begin_flags = toku_iso_to_txn_flag(trx->iso_level);
            }
            if ((error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, txn_begin_flags))) {
                /* We leave the possible master transaction open */
                trx->tokudb_lock_count--;  // We didn't get the lock
                goto cleanup;
            }
            if (tokudb_debug & TOKUDB_DEBUG_TXN) {
                TOKUDB_TRACE("stmt:%p:%p\n", trx->sp_level, trx->stmt);
            }
            trans_register_ha(thd, FALSE, tokudb_hton);
        }
        else {
            if (thd->in_lock_tables && thd_sql_command(thd) == SQLCOM_LOCK_TABLES) {
                assert(trx->all != NULL);
                //
                // For the command "Lock tables foo read, bar read"
                // This statement is grabbing the locks for the table
                // bar. The locks for foo will be grabbed when 
                // trx->tokudb_lock_count is 0 and we are initializing
                // trx->all above
                //
                if (lock.type <= TL_READ_NO_INSERT) {
                    error = acquire_table_lock(trx->all,lock_read);
                }
                else {
                    error = acquire_table_lock(trx->all,lock_write);
                }
                if (error) {trx->tokudb_lock_count--; goto cleanup;}
            }
        }
        transaction = trx->stmt;
    }
    else {
        lock.type = TL_UNLOCK;  // Unlocked

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
        if (!--trx->tokudb_lock_count) {
            if (trx->stmt) {
                /*
                   F_UNLCK is done without a transaction commit / rollback.
                   This happens if the thread didn't update any rows
                   We must in this case commit the work to keep the row locks
                 */
                DBUG_PRINT("trans", ("commiting non-updating transaction"));
                error = trx->stmt->commit(trx->stmt, 0);
                if (tokudb_debug & TOKUDB_DEBUG_TXN)
                    TOKUDB_TRACE("commit:%p:%d\n", trx->stmt, error);
                trx->stmt = NULL;
            }
        }
        transaction = NULL;
    }
cleanup:
    TOKUDB_DBUG_RETURN(error);
}


/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_tokudb::start_stmt(THD * thd, thr_lock_type lock_type) {
    TOKUDB_DBUG_ENTER("ha_tokudb::start_stmt");
    int error = 0;

    //
    // reset per-stmt variables
    //
    num_added_rows_in_stmt = 0;
    num_deleted_rows_in_stmt = 0;
    num_updated_rows_in_stmt = 0;

    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    DBUG_ASSERT(trx);
    /*
       note that trx->stmt may have been already initialized as start_stmt()
       is called for *each table* not for each storage engine,
       and there could be many bdb tables referenced in the query
     */
    if (!trx->stmt) {
        DBUG_PRINT("trans", ("starting transaction stmt"));
        error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, toku_iso_to_txn_flag(trx->iso_level));
        trans_register_ha(thd, FALSE, tokudb_hton);
    }
    if (added_rows > deleted_rows) {
        share->rows_from_locked_table = added_rows - deleted_rows;
    }
    transaction = trx->stmt;
    TOKUDB_DBUG_RETURN(error);
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
    if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
        /* If we are not doing a LOCK TABLE, then allow multiple writers */
        if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) && 
            !thd->in_lock_tables && thd_sql_command(thd) != SQLCOM_TRUNCATE) {
            lock_type = TL_WRITE_ALLOW_WRITE;
        }
        lock.type = lock_type;
    }
    *to++ = &lock;
    DBUG_RETURN(to);
}


static int create_sub_table(const char *table_name, int flags) {
    TOKUDB_DBUG_ENTER("create_sub_table");
    int error;
    DB *file;
    DBUG_PRINT("enter", ("flags: %d", flags));

    if (!(error = db_create(&file, db_env, 0))) {
        file->set_flags(file, flags);
        error = (file->open(file, NULL, table_name, NULL, DB_BTREE, DB_THREAD | DB_CREATE, my_umask));
        if (error) {
            DBUG_PRINT("error", ("Got error: %d when opening table '%s'", error, table_name));
            (void) file->remove(file, table_name, NULL, 0);
        } else
            (void) file->close(file, 0);
    } else {
        DBUG_PRINT("error", ("Got error: %d when creating table", error));
    }
    if (error)
        my_errno = error;
    TOKUDB_DBUG_RETURN(error);
}

static int mkdirpath(char *name, mode_t mode) {    
    char* parent = NULL;
    int r = toku_os_mkdir(name, mode);
    if (r == -1 && errno == ENOENT) {
        parent = (char *)my_malloc(strlen(name)+1,MYF(MY_WME));
        if (parent == NULL) {
            r = ENOMEM;
            goto cleanup;
        }
        strcpy(parent, name);
        char *cp = strrchr(parent, '/');
        if (cp) {
            *cp = 0;
            r = toku_os_mkdir(parent, 0755);
            if (r == 0)
                r = toku_os_mkdir(name, mode);
            }    
        }
cleanup:    
    my_free(parent, MYF(MY_ALLOW_ZERO_PTR));    
    return r;
}

extern "C" {
#include <dirent.h>
}

static int rmall(const char *dname) {
    int error = 0;
    char* fname = NULL;
    struct dirent *dirent = NULL;;
    DIR *d = opendir(dname);

    if (d == NULL) {
        error = errno;
        goto cleanup;
    }
    //
    // we do two loops, first loop just removes all the .tokudb files
    // second loop removes extraneous files
    //
    while ((dirent = readdir(d)) != 0) {
        if (0 == strcmp(dirent->d_name, ".") || 0 == strcmp(dirent->d_name, ".."))
            continue;
        fname = (char *)my_malloc(strlen(dname) + 1 + strlen(dirent->d_name) + 1, MYF(MY_WME));
        sprintf(fname, "%s/%s", dname, dirent->d_name);
        if (dirent->d_type == DT_DIR) {
            error = rmall(fname);
            if (error) { goto cleanup; }
        } 
        else {
            //
            // if clause checks if the file is a .tokudb file
            //
            if (strlen(fname) >= strlen (ha_tokudb_ext) &&
                strcmp(fname + (strlen(fname) - strlen(ha_tokudb_ext)), ha_tokudb_ext) == 0) 
            {
                if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
                    TOKUDB_TRACE("removing:%s\n", fname);
                }
                //
                // if this fails under low memory conditions, gracefully exit and return error
                // user will be notified that something went wrong, and he will
                // have to deal with it
                //
                DB* db = NULL;
                error = db_create(&db, db_env, 0);
                if (error) { goto cleanup; }

                //
                // it is ok to do db->remove on any .tokudb file, because any such
                // file was created with db->open
                //
                error = db->remove(db, fname, NULL, 0);
                if (error) { goto cleanup; }
            }
            else {
                continue;
            }
            my_free(fname, MYF(MY_ALLOW_ZERO_PTR));
            fname = NULL;
        }
    }
    closedir(d);
    d = NULL;

    fname = NULL;
    d = opendir(dname);
    if (d == NULL) {
        error = errno;
        goto cleanup;
    }
    //
    // second loop to remove extraneous files
    //
    while ((dirent = readdir(d)) != 0) {
        if (0 == strcmp(dirent->d_name, ".") || 0 == strcmp(dirent->d_name, ".."))
            continue;
        fname = (char *)my_malloc(strlen(dname) + 1 + strlen(dirent->d_name) + 1, MYF(MY_WME));
        sprintf(fname, "%s/%s", dname, dirent->d_name);
        if (dirent->d_type == DT_DIR) {
            error = rmall(fname);
            if (error) { goto cleanup; }
        } 
        else {
            if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
                TOKUDB_TRACE("removing:%s\n", fname);
            }
            //
            // Now we are removing files that are not .tokudb, we just delete it
            //
            error = unlink(fname);
            if (error != 0) {
                error = errno;
                break;
            }
            my_free(fname, MYF(MY_ALLOW_ZERO_PTR));
            fname = NULL;
        }
    }
    closedir(d);
    d = NULL;
    
    error = rmdir(dname);
    if (error != 0) {
        error = errno;
        goto cleanup;
    }
 
cleanup:    
    return error;
}


void ha_tokudb::update_create_info(HA_CREATE_INFO* create_info) {
    if (share->has_auto_inc) {
        info(HA_STATUS_AUTO);
        create_info->auto_increment_value = stats.auto_increment_value;
    }
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
    char name_buff[FN_REFLEN];
    int error;
    DB *status_block = NULL;
    bool dir_path_made = false;
    char* dirname = NULL;
    char* newname = NULL;

    dirname = (char *)my_malloc(get_name_length(name) + NAME_CHAR_LEN,MYF(MY_WME));
    if (dirname == NULL){ error = ENOMEM; goto cleanup;}
    newname = (char *)my_malloc(get_name_length(name) + NAME_CHAR_LEN,MYF(MY_WME));
    if (newname == NULL){ error = ENOMEM; goto cleanup;}

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


    // a table is a directory of dictionaries
    make_name(dirname, name, 0);
    error = mkdirpath(dirname, 0777);
    if (error != 0) {
        error = errno;
        goto cleanup;
    }
    dir_path_made = true;

    make_name(newname, name, "main");
    fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

    /* Create the main table that will hold the real rows */
    error = create_sub_table(name_buff, 0);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
        TOKUDB_TRACE("create:%s:error=%d\n", newname, error);
    }
    if (error) {
        goto cleanup;
    }

    primary_key = form->s->primary_key;

    /* Create the keys */
    char part[MAX_ALIAS_NAME + 10];
    for (uint i = 0; i < form->s->keys; i++) {
        if (i != primary_key) {
            int flags = (form->s->key_info[i].flags & HA_CLUSTERING) ? 0 : DB_DUP + DB_DUPSORT;
            sprintf(part, "key-%s", form->s->key_info[i].name);
            make_name(newname, name, part);
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            error = create_sub_table(name_buff, flags);
            if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
                TOKUDB_TRACE("create:%s:flags=%ld:error=%d\n", newname, form->key_info[i].flags, error);
            }
            if (error) {
                goto cleanup;
            }
        }
    }


    /* Create status.tokudb and save relevant metadata */
    if (!(error = (db_create(&status_block, db_env, 0)))) {
        make_name(newname, name, "status");
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

        if (!(error = (status_block->open(status_block, NULL, name_buff, NULL, DB_BTREE, DB_CREATE, 0)))) {
            uint version = HA_TOKU_VERSION;
            uint capabilities = HA_TOKU_CAP;
            
            error = write_metadata(status_block, hatoku_version,&version,sizeof(version));
            if (error) { goto cleanup; }

            error = write_metadata(status_block, hatoku_capabilities,&capabilities,sizeof(capabilities));
            if (error) { goto cleanup; }

            error = write_auto_inc_create(status_block, create_info->auto_increment_value);
            if (error) { goto cleanup; }

        }
    }

cleanup:
    if (status_block != NULL) {
        status_block->close(status_block, 0);
    }
    if (error && dir_path_made) {
        rmall(dirname);
    }
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    my_free(dirname, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
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
    // remove all of the dictionaries in the table directory 
    char* newname = NULL;
    newname = (char *)my_malloc((tokudb_data_dir ? strlen(tokudb_data_dir) : 0) + strlen(name) + NAME_CHAR_LEN, MYF(MY_WME));
    if (newname == NULL) {
        error = ENOMEM;
        goto cleanup;
    }
    make_name(newname, name, 0);
    error = rmall(newname);
    my_errno = error;
cleanup:
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
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
    char* newfrom = NULL;
    char* newto = NULL;

    int n = get_name_length(from) + NAME_CHAR_LEN;
    newfrom = (char *)my_malloc(n,MYF(MY_WME));
    if (newfrom == NULL){
        error = ENOMEM;
        goto cleanup;
    }
    make_name(newfrom, from, 0);

    n = get_name_length(to) + NAME_CHAR_LEN;
    newto = (char *)my_malloc(n,MYF(MY_WME));
    if (newto == NULL){
        error = ENOMEM;
        goto cleanup;
    }
    make_name(newto, to, 0);

    error = rename(newfrom, newto);
    if (error != 0) {
        error = my_errno = errno;
    }

cleanup:
    my_free(newfrom, MYF(MY_ALLOW_ZERO_PTR));
    my_free(newto, MYF(MY_ALLOW_ZERO_PTR));
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
	uint	index,
	uint	ranges,
	ha_rows rows
	)
{
    double total_scan;
    double ret_val; 

    //
    // in case for hidden primary key, this is called
    //
    if (index >= table_share->keys) {
        ret_val = handler::read_time(index, ranges, rows);
        goto cleanup;
    }


    //
    // if it is not the primary key, and it is not a clustering key, then return handler::read_time
    //
    if (index != primary_key && !(table->key_info[index].flags & HA_CLUSTERING)) {
        ret_val = handler::read_time(index, ranges, rows);
        goto cleanup;
    }

    //
    // for primary key and for clustered keys, return a fraction of scan_time()
    //
    total_scan = scan_time();

    if (stats.records < rows) {
        ret_val = total_scan;
        goto cleanup;
    }

    //
    // one disk seek per range plus the proportional scan time of the rows
    //
    ret_val = (ranges + (double) rows / (double) stats.records * total_scan);
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
    DBT key, after_key;
    ha_rows ret_val = HA_TOKUDB_RANGE_COUNT;
    DB *kfile = share->key_file[keynr];
    u_int64_t less, equal, greater;
    u_int64_t start_rows, end_rows, rows;
    int is_exact;
    int error;
    struct heavi_info heavi_info;
    DBC* tmp_cursor = NULL;
    u_int64_t after_key_less, after_key_equal, after_key_greater;
    heavi_info.db = kfile;
    heavi_info.key = &key;
    after_key.data = key_buff2;

    error = kfile->cursor(kfile, transaction, &tmp_cursor, 0);
    if (error) {
        ret_val = HA_TOKUDB_RANGE_COUNT;
        goto cleanup;
    }

    //
    // get start_rows and end_rows values so that we can estimate range
    // when calling key_range64, the only value we can trust is the value for less
    // The reason is that the key being passed in may be a prefix of keys in the DB
    // As a result, equal may be 0 and greater may actually be equal+greater
    // So, we call key_range64 on the key, and the key that is after it.
    //
    if (start_key) {
        pack_key(&key, keynr, key_buff, start_key->key, start_key->length, COL_NEG_INF); 
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
        if (start_key->flag == HA_READ_KEY_EXACT) {
            start_rows= less;
        }
        else {
            error = tmp_cursor->c_getf_heaviside(
                tmp_cursor, 
                0, 
                smart_dbt_callback_ror_heavi, 
                &after_key,
                after_key_heavi, 
                &heavi_info, 
                1
                );
            if (error && error != DB_NOTFOUND) {
                ret_val = HA_TOKUDB_RANGE_COUNT;
                goto cleanup;
            }
            else if (error == DB_NOTFOUND) {
                start_rows = stats.records;
            }
            else {
                error = kfile->key_range64(
                    kfile, 
                    transaction, 
                    &after_key,
                    &after_key_less,
                    &after_key_equal,
                    &after_key_greater,
                    &is_exact
                    );
                if (error) {
                    ret_val = HA_TOKUDB_RANGE_COUNT;
                    goto cleanup;
                }
                start_rows = after_key_less;
            }
        }
    }
    else {
        start_rows= 0;
    }

    if (end_key) {
        pack_key(&key, keynr, key_buff, end_key->key, end_key->length, COL_NEG_INF);
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
        if (end_key->flag == HA_READ_BEFORE_KEY) {
            end_rows= less;
        }
        else {
            error = tmp_cursor->c_getf_heaviside(
                tmp_cursor, 
                0, 
                smart_dbt_callback_ror_heavi, 
                &after_key,
                after_key_heavi, 
                &heavi_info, 
                1
                );
            if (error && error != DB_NOTFOUND) {
                ret_val = HA_TOKUDB_RANGE_COUNT;
                goto cleanup;
            }
            else if (error == DB_NOTFOUND) {
                end_rows = stats.records;
            }
            else {
                error = kfile->key_range64(
                    kfile, 
                    transaction, 
                    &after_key,
                    &after_key_less,
                    &after_key_equal,
                    &after_key_greater,
                    &is_exact
                    );
                if (error) {
                    ret_val = HA_TOKUDB_RANGE_COUNT;
                    goto cleanup;
                }
                end_rows= after_key_less;
            }
        }
    }
    else {
        end_rows = stats.records;
    }

    rows = (end_rows > start_rows) ? end_rows - start_rows : 1;

    //
    // MySQL thinks a return value of 0 means there are exactly 0 rows
    // Therefore, always return non-zero so this assumption is not made
    //
    ret_val = (ha_rows) (rows <= 1 ? 1 : rows);
cleanup:
    if (tmp_cursor) {
        tmp_cursor->c_close(tmp_cursor);
        tmp_cursor = NULL;
    }
    DBUG_RETURN(ret_val);
}


//
// initializes the auto increment data needed
//
void ha_tokudb::init_auto_increment() {
    DBT key;
    DBT value;
    int error;
    HA_METADATA_KEY key_val = hatoku_max_ai;
    bzero(&key, sizeof(key));
    bzero(&value, sizeof(value));
    key.data = &key_val;
    key.size = sizeof(key_val);
    value.flags = DB_DBT_MALLOC;
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
        error = share->status_block->get(
            share->status_block, 
            txn, 
            &key, 
            &value, 
            0
            );
        
        if (error == 0 && value.size == sizeof(share->last_auto_increment)) {
            share->last_auto_increment = *(uint *)value.data;
            free(value.data);
            value.data = NULL;
        }
        else {
            share->last_auto_increment = 0;
        }
        //
        // Now retrieve the initial auto increment value, as specified by create table
        // so if a user does "create table t1 (a int auto_increment, primary key (a)) auto_increment=100",
        // then the value 100 should be stored here
        //
        key_val = hatoku_ai_create_value;
        error = share->status_block->get(
            share->status_block, 
            txn, 
            &key, 
            &value, 
            0
            );
        
        if (error == 0 && value.size == sizeof(share->auto_inc_create_value)) {
            share->auto_inc_create_value = *(uint *)value.data;
            free(value.data);
            value.data = NULL;
        }
        else {
            share->auto_inc_create_value = 0;
        }

        txn->commit(txn,DB_TXN_NOSYNC);
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
    update_max_auto_inc(share->status_block, nr + (nb_desired_values - 1)*increment);
    share->last_auto_increment = nr + (nb_desired_values - 1)*increment;

    if (tokudb_debug & TOKUDB_DEBUG_AUTO_INCREMENT) {
        TOKUDB_TRACE("get_auto_increment(%lld,%lld,%lld):got:%lld:%lld\n",
                     offset, increment, nb_desired_values, nr, nb_desired_values);
    }
    *first_value = nr;
    *nb_reserved_values = nb_desired_values;
    pthread_mutex_unlock(&share->mutex);
    DBUG_VOID_RETURN;
}

bool ha_tokudb::is_auto_inc_singleton(){
    return false;
}

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
int ha_tokudb::add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::add_index");
    char name_buff[FN_REFLEN];
    int error;
    uint curr_index = 0;
    DBC* tmp_cursor = NULL;
    int cursor_ret_val = 0;
    DBT current_primary_key;
    DB_TXN* txn = NULL;
    char* newname = NULL;
    uchar* tmp_key_buff = NULL;
    uchar* tmp_prim_key_buff = NULL;
    uchar* tmp_record = NULL;
    THD* thd = ha_thd(); 
    uchar* tmp_record2 = NULL;
    //
    // these variables are for error handling
    //
    uint num_files_created = 0;
    uint num_DB_opened = 0;
    //
    // number of DB files we have open currently, before add_index is executed
    //
    uint curr_num_DBs = table_arg->s->keys + test(hidden_primary_key);

    //
    // status message to be shown in "show process list"
    //
    char status_msg[MAX_ALIAS_NAME + 200]; //buffer of 200 should be a good upper bound.
    ulonglong num_processed = 0; //variable that stores number of elements inserted thus far
    thd_proc_info(thd, "Adding indexes");

    newname = (char *)my_malloc(share->table_name_length + NAME_CHAR_LEN, MYF(MY_WME));
    tmp_key_buff = (uchar *)my_malloc(2*table_arg->s->rec_buff_length, MYF(MY_WME));
    tmp_prim_key_buff = (uchar *)my_malloc(2*table_arg->s->rec_buff_length, MYF(MY_WME));
    tmp_record = (uchar *)my_malloc(table_arg->s->rec_buff_length,MYF(MY_WME));
    tmp_record2 = (uchar *)my_malloc(2*table_arg->s->rec_buff_length,MYF(MY_WME));
    if (newname == NULL || 
        tmp_key_buff == NULL ||
        tmp_prim_key_buff == NULL ||
        tmp_record == NULL ||
        tmp_record2 == NULL) {
        error = ENOMEM;
        goto cleanup;
    }


    
    //
    // in unpack_row, MySQL passes a buffer that is this long,
    // so this length should be good enough for us as well
    //
    bzero((void *) &current_primary_key, sizeof(current_primary_key));
    current_primary_key.data = tmp_prim_key_buff;

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
    
    //
    // first create all the DB's files
    //
    char part[MAX_ALIAS_NAME + 10];
    for (uint i = 0; i < num_of_keys; i++) {
        int flags = (key_info[i].flags & HA_CLUSTERING) ? 0 : DB_DUP + DB_DUPSORT;
        sprintf(part, "key-%s", key_info[i].name);
        make_name(newname, share->table_name, part);
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
        error = create_sub_table(name_buff, flags);
        if (tokudb_debug & TOKUDB_DEBUG_OPEN) {
            TOKUDB_TRACE("create:%s:flags=%ld:error=%d\n", newname, key_info[i].flags, error);
        }
        if (error) { goto cleanup; }
        num_files_created++;
    }

    //
    // open all the DB files and set the appropriate variables in share
    // they go to the end of share->key_file
    //
    curr_index = curr_num_DBs;
    for (uint i = 0; i < num_of_keys; i++, curr_index++) {
        error = open_secondary_table(
            &share->key_file[curr_index], 
            &key_info[i],
            share->table_name,
            0,
            &share->key_type[curr_index]
            );
        if (error) { goto cleanup; }
        num_DB_opened++;
    }
    

    //
    // scan primary table, create each secondary key, add to each DB
    //
    
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    assert(error == 0);

    //
    // grab some locks to make this go faster
    // first a global read lock on the main DB, because
    // we intend to scan the entire thing
    //
    error = share->file->pre_acquire_read_lock(
        share->file, 
        txn, 
        share->file->dbt_neg_infty(), 
        NULL, 
        share->file->dbt_pos_infty(), 
        NULL
        );
    if (error) { txn->commit(txn, 0); goto cleanup; }

    //
    // now grab a table write lock for secondary tables we
    // are creating
    //
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = i + curr_num_DBs;
        error = share->key_file[curr_index]->pre_acquire_table_lock(
            share->key_file[curr_index],
            txn
            );
        if (error) { txn->commit(txn, 0); goto cleanup; }
    }

    if ((error = share->file->cursor(share->file, txn, &tmp_cursor, 0))) {
        tmp_cursor = NULL;             // Safety
        goto cleanup;
    }

    //
    // for each element in the primary table, insert the proper key value pair in each secondary table
    // that is created
    //
    struct smart_dbt_ai_info info;
    info.ha = this;
    info.prim_key = &current_primary_key;
    info.buf = tmp_record;
    info.pk_index = primary_key; // needed so that clustering indexes being created will have right pk info


    cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_ai_callback, &info);
    while (cursor_ret_val != DB_NOTFOUND) {
        if (cursor_ret_val) {
            error = cursor_ret_val;
            goto cleanup;
        }

        for (uint i = 0; i < num_of_keys; i++) {
            bool cluster_row_created = false;
            DBT secondary_key, row;
            bool has_null = false;
            create_dbt_key_from_key(&secondary_key,&key_info[i], tmp_key_buff, tmp_record, &has_null);
            uint curr_index = i + curr_num_DBs;
            u_int32_t put_flags = share->key_type[curr_index];
            if (put_flags == DB_NOOVERWRITE && (has_null || thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS))) { 
                put_flags = DB_YESOVERWRITE;
            }

            if (key_info[i].flags & HA_CLUSTERING) {
                if (!cluster_row_created) {
                    if ((error = pack_row(&row, (const uchar *) tmp_record, false))){
                       goto cleanup;
                    }
                    cluster_row_created = true;
                }
                error = share->key_file[curr_index]->put(share->key_file[curr_index], txn, &secondary_key, &row, put_flags);
            }
            else {
                error = share->key_file[curr_index]->put(share->key_file[curr_index], txn, &secondary_key, &current_primary_key, put_flags);
            }
            if (error) {
                //
                // in the case of any error anywhere, we can just nuke all the files created, so we dont need
                // to be tricky and try to roll back changes. That is why we commit the transaction,
                // which should be fast. The DB is going to go away anyway, so no pt in trying to keep
                // it in a good state.
                //
                txn->commit(txn, 0);
                //
                // found a duplicate in a no_dup DB
                //
                if ( (error == DB_KEYEXIST) && (key_info[i].flags & HA_NOSAME)) {
                    error = HA_ERR_FOUND_DUPP_KEY;
                    last_dup_key = i;
                    memcpy(table_arg->record[0], tmp_record, table_arg->s->rec_buff_length);
                }
                goto cleanup;
            }
        }
        num_processed++; 

        if ((num_processed % 1000) == 0) {
            sprintf(status_msg, "Adding indexes: Processed %llu of about %llu rows.", num_processed, share->rows);
            thd_proc_info(thd, status_msg);
            if (thd->killed) {
                error = ER_ABORTING_CONNECTION;
                txn->commit(txn, 0);
                goto cleanup;
            }
        }
        cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_ai_callback, &info);
    }
    tmp_cursor->c_close(tmp_cursor);
    tmp_cursor = NULL;

    //
    // We have an accurate row count, might as well update share->rows
    //
    pthread_mutex_lock(&share->mutex);
    share->rows = num_processed;
    pthread_mutex_unlock(&share->mutex);

    //
    // Now flatten the new DB's created
    //
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = i + curr_num_DBs;
        if ((error = share->key_file[curr_index]->cursor(share->key_file[curr_index], txn, &tmp_cursor, 0))) {
            tmp_cursor = NULL;             // Safety
            goto cleanup;
        }
        error = 0;
        num_processed = 0;
        while (error != DB_NOTFOUND) {
            error = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_opt_callback, NULL);
            if (error && error != DB_NOTFOUND) {
                tmp_cursor->c_close(tmp_cursor);
                tmp_cursor = NULL;
                txn->commit(txn, 0);
                goto cleanup;
            }
            num_processed++;
            if ((num_processed % 1000) == 0) {
                sprintf(status_msg, "Adding indexes: Applied %llu of %llu rows in key-%s.", num_processed, share->rows, key_info[i].name);
                thd_proc_info(thd, status_msg);
                if (thd->killed) {
                    error = ER_ABORTING_CONNECTION;
                    txn->commit(txn, 0);
                    goto cleanup;
                }
            }
        }
        
        tmp_cursor->c_close(tmp_cursor);
        tmp_cursor = NULL;
    }

    error = txn->commit(txn, 0);
    assert(error == 0);
    
    error = 0;
cleanup:
    if (error) {
        if (tmp_cursor) {            
            tmp_cursor->c_close(tmp_cursor);
            tmp_cursor = NULL;
        }
        //
        // We need to delete all the files that may have been created
        // The DB's must be closed and removed
        //
        for (uint i = curr_num_DBs; i < curr_num_DBs + num_DB_opened; i++) {
            share->key_file[i]->close(share->key_file[i], 0);
            share->key_file[i] = NULL;
        }
        for (uint i = 0; i < num_files_created; i++) {
            DB* tmp;
            sprintf(part, "key-%s", key_info[i].name);
            make_name(newname, share->table_name, part);
            fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);
            if (!(db_create(&tmp, db_env, 0))) {
                tmp->remove(tmp, name_buff, NULL, 0);
            }
        }
    }
    my_free(newname,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_key_buff,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_prim_key_buff,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_record,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_record2,MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
}

//
// Prepares to drop indexes to the table. For each value, i, in the array key_num,
// table->key_info[i] is a key that is to be dropped.
//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_num - array of indexes that specify which keys of the array table->key_info
//                  are to be dropped
//              num_of_keys - size of array, key_num
//  Returns:
//      0 on success, error otherwise
//
int ha_tokudb::prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::prepare_drop_index");
    int error;
    char name_buff[FN_REFLEN];
    char* newname = NULL;
    char part[MAX_ALIAS_NAME + 10];
    DB** dbs_to_remove = NULL;

    newname = (char *)my_malloc(share->table_name_length + NAME_CHAR_LEN, MYF(MY_WME));
    if (newname == NULL) {
        error = ENOMEM; 
        goto cleanup;
    }
    //
    // we allocate an array of DB's here to get ready for removal
    // We do this so that all potential memory allocation errors that may occur
    // will do so BEFORE we go about dropping any indexes. This way, we
    // can fail gracefully without losing integrity of data in such cases. If on
    // on the other hand, we started removing DB's, and in the middle, 
    // one failed, it is not immedietely obvious how one would rollback
    //
    dbs_to_remove = (DB **)my_malloc(sizeof(*dbs_to_remove)*num_of_keys, MYF(MY_ZEROFILL));
    if (dbs_to_remove == NULL) {
        error = ENOMEM; 
        goto cleanup;
    }
    for (uint i = 0; i < num_of_keys; i++) {
        error = db_create(&dbs_to_remove[i], db_env, 0);
        if (error) {
            goto cleanup;
        }
    }    
    
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = key_num[i];
        share->key_file[curr_index]->close(share->key_file[curr_index],0);
        share->key_file[curr_index] = NULL;
        
        sprintf(part, "key-%s", table_arg->key_info[curr_index].name);
        make_name(newname, share->table_name, part);
        fn_format(name_buff, newname, "", 0, MY_UNPACK_FILENAME);

        dbs_to_remove[i]->remove(dbs_to_remove[i], name_buff, NULL, 0);
    }
cleanup:
    my_free(dbs_to_remove, MYF(MY_ALLOW_ZERO_PTR));
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    TOKUDB_DBUG_RETURN(error);
}


//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments, therefore, we can make
// this function just return
int ha_tokudb::final_drop_index(TABLE *table_arg) {
    TOKUDB_DBUG_ENTER("ha_tokudb::final_drop_index");
    TOKUDB_DBUG_RETURN(0);
}

void ha_tokudb::print_error(int error, myf errflag) {
    if (error == DB_LOCK_DEADLOCK)
        error = HA_ERR_LOCK_DEADLOCK;
    if (error == DB_LOCK_NOTGRANTED)
        error = HA_ERR_LOCK_WAIT_TIMEOUT;
    handler::print_error(error, errflag);
}

#if 0 // QQQ use default
//
// This function will probably need to be redone from scratch
// if we ever choose to implement it
//
int ha_tokudb::analyze(THD * thd, HA_CHECK_OPT * check_opt) {
    uint i;
    DB_BTREE_STAT *stat = 0;
    DB_TXN_STAT *txn_stat_ptr = 0;
    tokudb_trx_data *trx = (tokudb_trx_data *) thd->ha_data[tokudb_hton->slot];
    DBUG_ASSERT(trx);

    for (i = 0; i < table_share->keys; i++) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((key_file[i]->stat) (key_file[i], trx->all, (void *) &stat, 0))
            goto err;
        share->rec_per_key[i] = (stat->bt_ndata / (stat->bt_nkeys ? stat->bt_nkeys : 1));
    }
    /* A hidden primary key is not in key_file[] */
    if (hidden_primary_key) {
        if (stat) {
            free(stat);
            stat = 0;
        }
        if ((file->stat) (file, trx->all, (void *) &stat, 0))
            goto err;
    }
    pthread_mutex_lock(&share->mutex);
    share->status |= STATUS_TOKUDB_ANALYZE;        // Save status on close
    share->version++;           // Update stat in table
    pthread_mutex_unlock(&share->mutex);
    update_status(share, table);        // Write status to file
    if (stat)
        free(stat);
    return ((share->status & STATUS_TOKUDB_ANALYZE) ? HA_ADMIN_FAILED : HA_ADMIN_OK);

  err:
    if (stat)
        free(stat);
    return HA_ADMIN_FAILED;
}
#endif

//
// flatten all DB's in this table, to do so, just do a full scan on every DB
//
int ha_tokudb::optimize(THD * thd, HA_CHECK_OPT * check_opt) {
    TOKUDB_DBUG_ENTER("ha_tokudb::optimize");
    int error;
    DBC* tmp_cursor = NULL;
    tokudb_trx_data *trx = NULL;
    DB_TXN* txn = NULL;
    bool do_commit = false;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);

    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (trx == NULL) {
        error = HA_ERR_UNSUPPORTED;
        goto cleanup;
    }

    //
    // optimize may be called without a valid transaction, so we have to do this
    // in order to get a valid transaction
    // this is a bit hacky, but it is the best we have right now
    //
    txn = trx->stmt ? trx->stmt : trx->sp_level;
    if (txn == NULL) {        
        error = db_env->txn_begin(db_env, NULL, &txn, 0);
        if (error) {
            goto cleanup;
        }
        do_commit = true;
    }
    //
    // prelock so each scan goes faster
    //
    error = acquire_table_lock(txn,lock_read);
    if (error) {
        goto cleanup;
    }

    //
    // for each DB, scan through entire table and do nothing
    //
    for (uint i = 0; i < curr_num_DBs; i++) {
        error = 0;
        if ((error = share->file->cursor(share->file, txn, &tmp_cursor, 0))) {
            tmp_cursor = NULL;
            goto cleanup;
        }
        while (error != DB_NOTFOUND) {
            error = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_opt_callback, NULL);
            if (error && error != DB_NOTFOUND) {
                goto cleanup;
            }
        }
        tmp_cursor->c_close(tmp_cursor);
    }

    error = 0;
cleanup:
    if (do_commit) {
        error = txn->commit(txn, 0);
    }
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

    // truncate all dictionaries
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    for (uint i = 0; i < curr_num_DBs; i++) {
        DB *db = share->key_file[i];
        u_int32_t row_count = 0;
        error = db->truncate(db, transaction, &row_count, 0);
        if (error) 
            break;
        // do something with the row_count?
        if (tokudb_debug)
            TOKUDB_TRACE("row_count=%u\n", row_count);
    }

    // zap the row count
    if (error == 0)
        share->rows = 0;

    TOKUDB_DBUG_RETURN(error);
}


