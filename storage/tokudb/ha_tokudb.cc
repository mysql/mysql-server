#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation          // gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "mysql_priv.h"
#include "hatoku_cmp.h"
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
    return thd->ha_data[slot].ha_ptr;
}

static inline void thd_data_set(THD *thd, int slot, void *data) {
    thd->ha_data[slot].ha_ptr = data;
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
#include <mysql/plugin.h>

static const char *ha_tokudb_exts[] = {
    ha_tokudb_ext,
    NullS
};

#define lockretryN(N) \
  for (ulonglong lockretrycount=0; lockretrycount<(N/(1<<3) + 1); lockretrycount++)

#define lockretry lockretryN(800)

#define lockretry_wait \
        if (error != DB_LOCK_NOTGRANTED) { \
            break;  \
        } \
        if (tokudb_debug & TOKUDB_DEBUG_LOCKRETRY) { \
            TOKUDB_TRACE("%s count=%d\n", __FUNCTION__, lockretrycount); \
        } \
        if (lockretrycount%200 == 0) { \
            if (ha_thd()->killed) { \
                error = DB_LOCK_NOTGRANTED; \
                break; \
            } \
        } \
        usleep((lockretrycount<4 ? (1<<lockretrycount) : (1<<3)) * 1024); \

//
// This offset is calculated starting from AFTER the NULL bytes
//
inline u_int32_t get_var_len_offset(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
    uint offset = 0;
    for (uint i = 0; i < table_share->fields; i++) {
        if (kc_info->field_lengths[i] && !bitmap_is_set(&kc_info->key_filters[keynr],i)) {
            offset += kc_info->field_lengths[i];
        }
    }
    return offset;
}


inline u_int32_t get_len_of_offsets(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
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
    kc_info->field_lengths = (uchar *)my_malloc(table_share->fields, MYF(MY_WME | MY_ZEROFILL));
    kc_info->length_bytes= (uchar *)my_malloc(table_share->fields, MYF(MY_WME | MY_ZEROFILL));
    kc_info->blob_fields= (u_int32_t *)my_malloc(table_share->fields*sizeof(u_int32_t), MYF(MY_WME | MY_ZEROFILL));
    
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

        bzero((void *) share->key_file, sizeof(share->key_file));

        error = my_hash_insert(&tokudb_open_tables, (uchar *) share);
        if (error) {
            goto exit;
        }
        thr_lock_init(&share->lock);
        pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
    }
    pthread_mutex_unlock(&tokudb_mutex);

exit:
    if (error) {
        pthread_mutex_destroy(&share->mutex);
        my_free((uchar *) share, MYF(0));
        share = NULL;
    }
    return share;
}


void free_key_and_col_info (KEY_AND_COL_INFO* kc_info) {
    for (uint i = 0; i < MAX_KEY+1; i++) {
        bitmap_free(&kc_info->key_filters[i]);
    }
    
    for (uint i = 0; i < MAX_KEY+1; i++) {
        my_free(kc_info->cp_info[i], MYF(MY_ALLOW_ZERO_PTR));
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



//
// struct that will be used as a context for smart DBT callbacks
// contains parameters needed to complete the smart DBT cursor call
//
typedef struct smart_dbt_info {
    ha_tokudb* ha; //instance to ha_tokudb needed for reading the row
    uchar* buf; // output buffer where row will be written
    uint keynr; // index into share->key_file that represents DB we are currently operating on
} *SMART_DBT_INFO;

typedef struct index_read_info {
    struct smart_dbt_info smart_dbt_info;
    int cmp;
    DBT* orig_key;
} *INDEX_READ_INFO;

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

typedef struct row_buffers {
    uchar** key_buff;
    uchar** rec_buff;
} *ROW_BUFFERS;

static int smart_dbt_ai_callback (DBT const *key, DBT const *row, void *context) {
    int error = 0;
    SMART_DBT_AI_INFO info = (SMART_DBT_AI_INFO)context;
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
    info->ha->extract_hidden_primary_key(info->pk_index,key);
    error = info->ha->unpack_row(info->buf,row,key, info->ha->primary_key);
    return error;
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
#define SET_READ_FLAG(flg) ((range_lock_grabbed) ? ((flg) | DB_PRELOCKED) : (flg))


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



int filter_key_part_compare (const void* left, const void* right) {
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


inline uchar* pack_fixed_field(
    uchar* to_tokudb,
    const uchar* from_mysql,
    u_int32_t num_bytes
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

inline const uchar* unpack_fixed_field(
    uchar* to_mysql,
    const uchar* from_tokudb,
    u_int32_t num_bytes
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


inline uchar* pack_var_field(
    uchar* to_tokudb_offset_ptr, //location where offset data is going to be written
    uchar* to_tokudb_data,
    uchar* to_tokudb_offset_start, //location where offset starts
    const uchar * from_mysql,
    u_int32_t mysql_length_bytes,
    u_int32_t offset_bytes
    )
{
    uint data_length = 0;
    u_int32_t offset = 0;
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
    memcpy(to_tokudb_data, from_mysql + mysql_length_bytes, data_length);
    //
    // for offset, we pack the offset where the data ENDS!
    //
    offset = to_tokudb_data + data_length - to_tokudb_offset_start;
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

inline void unpack_var_field(
    uchar* to_mysql,
    const uchar* from_tokudb_data,
    u_int32_t from_tokudb_data_len,
    u_int32_t mysql_length_bytes
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

uchar* pack_toku_field_blob(
    uchar* to_tokudb,
    const uchar* from_mysql,
    Field* field
    )
{
    u_int32_t len_bytes = field->row_pack_length();
    u_int32_t length = 0;
    uchar* data_ptr = NULL;
    memcpy(to_tokudb, from_mysql, len_bytes);

    switch (len_bytes) {
    case (1):
        length = (u_int32_t)(*from_mysql);
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
    
    bzero((void *)&key, sizeof(key));
    bzero((void *)&val, sizeof(val));
    key.data = (void *)name;
    key.size = strlen(name) + 1;
    val.data = &hidden_primary_key;
    val.size = sizeof(hidden_primary_key);
    error = metadata_db->put(
        metadata_db,
        txn,
        &key,
        &val,
        DB_YESOVERWRITE
        );
    return error;
}

static int drop_table_from_metadata(const char *name, DB_TXN* txn) {
    int error = 0;
    DBT key;
    DBT data;
    assert(txn);
    bzero((void *)&key, sizeof(key));
    bzero((void *)&data, sizeof(data));
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
    
    bzero((void *)&from_key, sizeof(from_key));
    bzero((void *)&to_key, sizeof(to_key));
    bzero((void *)&val, sizeof(val));
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
        DB_YESOVERWRITE
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


static int check_table_in_metadata(const char *name, bool* table_found) {
    int error = 0;
    DBT key;
    DB_TXN* txn = NULL;
    pthread_mutex_lock(&tokudb_meta_mutex);
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) {
        goto cleanup;
    }
    
    bzero((void *)&key, sizeof(key));
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

cleanup:
    if (txn) {
        commit_txn(txn, 0);
    }
    pthread_mutex_unlock(&tokudb_meta_mutex);
    return error;
}

int create_tokudb_trx_data_instance(tokudb_trx_data** out_trx) {
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

int generate_row_for_put(
    DB *dest_db, 
    DB *src_db,
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *src_key, 
    const DBT *src_val,
    void *extra
    ) 
{
    int error;

    DB* curr_db = dest_db;
    uchar* row_desc = NULL;
    u_int32_t desc_size;
    uchar* buff = NULL;
    u_int32_t max_key_len = 0;
    
    row_desc = (uchar *)curr_db->descriptor->data;
    row_desc += (*(u_int32_t *)row_desc);
    desc_size = (*(u_int32_t *)row_desc) - 4;
    row_desc += 4;
    
    if (is_key_pk(row_desc, desc_size)) {
        assert(dest_key->flags != DB_DBT_USERMEM);
        assert(dest_val->flags != DB_DBT_USERMEM);
        if (dest_key->flags == DB_DBT_REALLOC && dest_key->data != NULL) {
            free(dest_key->data);
        }
        if (dest_val->flags == DB_DBT_REALLOC && dest_val->data != NULL) {
            free(dest_val->data);
        }
        dest_key->data = src_key->data;
        dest_key->size = src_key->size;
        dest_key->flags = 0;
        dest_val->data = src_val->data;
        dest_val->size = src_val->size;
        dest_val->flags = 0;
        error = 0;
        goto cleanup;
    }
    if (dest_key->flags == DB_DBT_USERMEM) {
        buff = (uchar *)dest_key->data;
    }
    else if (dest_key->flags == DB_DBT_REALLOC) {
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
    desc_size = (*(u_int32_t *)row_desc) - 4;
    row_desc += 4;
    if (!is_key_clustering(row_desc, desc_size)) {
        dest_val->size = 0;
    }
    else {
        uchar* buff = NULL;
        if (dest_val->flags == DB_DBT_USERMEM) {
            buff = (uchar *)dest_val->data;
        }
        else if (dest_val->flags == DB_DBT_REALLOC){
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

    error = 0;
cleanup:
    return error;
}


ha_tokudb::ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg):handler(hton, table_arg) 
    // flags defined in sql\handler.h
{
    int_table_flags = HA_REC_NOT_IN_SEQ  | HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_PRIMARY_KEY_IN_READ_INDEX | 
                    HA_FILE_BASED | HA_AUTO_PART_KEY | HA_TABLE_SCAN_ON_INDEX |HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;
    alloc_ptr = NULL;
    rec_buff = NULL;
    transaction = NULL;
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
    bzero(mult_key_buff, sizeof(mult_key_buff));
    bzero(mult_rec_buff, sizeof(mult_rec_buff));
    bzero(mult_key_dbt, sizeof(mult_key_dbt));
    bzero(mult_rec_dbt, sizeof(mult_rec_dbt));
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

int ha_tokudb::open_status_dictionary(DB** ptr, const char* name, DB_TXN* txn) {
    int error;
    char* newname = NULL;
    uint open_mode = DB_THREAD;
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

int initialize_col_pack_info(KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, uint keynr) {
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
    u_int32_t curr_fixed_offset = 0;
    u_int32_t curr_var_index = 0;
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
    kc_info->mcp_info[keynr].var_len_offset = get_var_len_offset(
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

int initialize_key_and_col_info(TABLE_SHARE* table_share, TABLE* table, KEY_AND_COL_INFO* kc_info, uint hidden_primary_key, uint primary_key) {
    int error;
    u_int32_t curr_blob_field_index = 0;
    u_int32_t max_var_bytes = 0;
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
            assert(pack_length < 256);
            kc_info->field_lengths[i] = (uchar)pack_length;
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
            if (table_share->key_info[i].flags & HA_CLUSTERING) {
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


int ha_tokudb::initialize_share(
    const char* name,
    int mode
    )
{
    int error = 0;
    u_int64_t num_rows = 0;
    bool table_exists;
    DBUG_PRINT("info", ("share->use_count %u", share->use_count));

    table_exists = true;
    error = check_table_in_metadata(name, &table_exists);

    if (error) {
        goto exit;
    }
    if (!table_exists) {
        sql_print_error("table %s does not exist in metadata, was it moved from someplace else? Not opening table", name);
        error = HA_ADMIN_FAILED;
        goto exit;
    }
    
    error = initialize_key_and_col_info(
        table_share,
        table, 
        &share->kc_info,
        hidden_primary_key,
        primary_key
        );
    if (error) { goto exit; }
    
    error = open_main_dictionary(name, mode == O_RDONLY, NULL);
    if (error) { goto exit; }

    share->has_unique_keys = false;
    /* Open other keys;  These are part of the share structure */
    for (uint i = 0; i < table_share->keys + test(hidden_primary_key); i++) {
        if (table_share->key_info[i].flags & HA_NOSAME) {
            share->has_unique_keys = true;
        }
        if (i != primary_key) {
            error = open_secondary_dictionary(
                &share->key_file[i],
                &table_share->key_info[i],
                name,
                mode == O_RDONLY,
                NULL
                );
            if (error) {
                goto exit;
            }
            share->mult_put_flags[i] = DB_YESOVERWRITE;
        }
        else {
            share->mult_put_flags[i] = DB_NOOVERWRITE;
        }
    }
    if (!hidden_primary_key) {
        //
        // We need to set the ref_length to start at 5, to account for
        // the "infinity byte" in keys, and for placing the DBT size in the first four bytes
        //
        ref_length = sizeof(u_int32_t) + sizeof(uchar);
        KEY_PART_INFO *key_part = table->key_info[primary_key].key_part;
        KEY_PART_INFO *end = key_part + table->key_info[primary_key].key_parts;
        for (; key_part != end; key_part++) {
            ref_length += key_part->field->max_packed_col_length(key_part->length);
        }
        share->status |= STATUS_PRIMARY_KEY_INIT;
    }
    share->ref_length = ref_length;

    error = get_status();
    if (error) {
        goto exit;
    }
    if (share->version < HA_TOKU_VERSION) {
        error = ENOSYS;
        goto exit;
    }

    error = estimate_num_rows(share->file,&num_rows);
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

    if (may_table_be_empty()) {
        share->try_table_lock = true;
    }
    else {
        share->try_table_lock = false;
    }

    error = 0;
exit:
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

    int error = 0;
    int ret_val = 0;

    transaction = NULL;
    cursor = NULL;


    /* Open primary key */
    hidden_primary_key = 0;
    if ((primary_key = table_share->primary_key) >= MAX_KEY) {
        // No primary key
        primary_key = table_share->keys;
        key_used_on_scan = MAX_KEY;
        hidden_primary_key = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        ref_length = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH + sizeof(u_int32_t);
    } 
    else {
        key_used_on_scan = primary_key;
    }

    /* Need some extra memory in case of packed keys */
    // the "+ 1" is for the first byte that states +/- infinity
    // multiply everything by 2 to account for clustered keys having a key and primary key together
    max_key_length = 2*(table_share->max_key_length + MAX_REF_PARTS * 3 + sizeof(uchar));
    alloc_ptr = my_multi_malloc(MYF(MY_WME),
        &key_buff, max_key_length, 
        &key_buff2, max_key_length, 
        &key_buff3, max_key_length, 
        &primary_key_buff, (hidden_primary_key ? 0 : max_key_length),
        &fixed_cols_for_query, table_share->fields*sizeof(u_int32_t),
        &var_cols_for_query, table_share->fields*sizeof(u_int32_t),
        NullS
        );
    if (alloc_ptr == NULL) {
        ret_val = 1;
        goto exit;
    }

    alloced_rec_buff_length = table_share->rec_buff_length + table_share->fields;
    rec_buff = (uchar *) my_malloc(alloced_rec_buff_length, MYF(MY_WME));

    if (rec_buff == NULL) {
        ret_val = 1;
        goto exit;
    }

    for (u_int32_t i = 0; i < (table_share->keys); i++) {
        if (i == primary_key) {
            continue;
        }
        mult_key_buff[i] = (uchar *)my_malloc(max_key_length, MYF(MY_WME));
        assert(mult_key_buff[i] != NULL);
        mult_key_dbt[i].ulen = max_key_length;
        mult_key_dbt[i].flags = DB_DBT_USERMEM;
        mult_key_dbt[i].data = mult_key_buff[i];
        if (table_share->key_info[i].flags & HA_CLUSTERING) {
            mult_rec_buff[i] = (uchar *) my_malloc(alloced_rec_buff_length, MYF(MY_WME));
            assert(mult_rec_buff[i]);
            mult_rec_dbt[i].ulen = alloced_rec_buff_length;
            mult_rec_dbt[i].flags = DB_DBT_USERMEM;
            mult_rec_dbt[i].data = mult_rec_buff[i];
        }
    }
    alloced_mult_rec_buff_length = alloced_rec_buff_length;

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
        my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
        alloc_ptr = NULL;
        my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
        rec_buff = NULL;
        for (u_int32_t i = 0; i < (table_share->keys); i++) {
            my_free(mult_key_buff[i], MYF(MY_ALLOW_ZERO_PTR));
            my_free(mult_rec_buff[i], MYF(MY_ALLOW_ZERO_PTR));
        }
        
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
    if (crsr != NULL) {
        int r = crsr->c_close(crsr);
        assert(r==0);
        crsr = NULL;
    }
    if (do_commit) {
        commit_txn(transaction, 0);
        transaction = NULL;
    }
    return error;
}


int ha_tokudb::write_to_status(DB* db, HA_METADATA_KEY curr_key_data, void* data, uint size, DB_TXN* txn ){
    return write_metadata(db, &curr_key_data, sizeof(curr_key_data), data, size, txn);
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

    bzero(&key, sizeof(key));
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

    bzero(&key, sizeof(key));
    bzero(&value, sizeof(value));
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
    TOKUDB_CLOSE();
    TOKUDB_DBUG_RETURN(__close(0));
}

int ha_tokudb::__close(int mutex_is_locked) {
    TOKUDB_DBUG_ENTER("ha_tokudb::__close %p", this);
    if (tokudb_debug & TOKUDB_DEBUG_OPEN) 
        TOKUDB_TRACE("close:%p\n", this);
    my_free(rec_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(blob_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(alloc_ptr, MYF(MY_ALLOW_ZERO_PTR));
    for (u_int32_t i = 0; i < (table_share->keys); i++) {
        my_free(mult_key_buff[i], MYF(MY_ALLOW_ZERO_PTR));
        my_free(mult_rec_buff[i], MYF(MY_ALLOW_ZERO_PTR));
    }
    rec_buff = NULL;
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

void ha_tokudb::fix_mult_rec_buff() {
    if (alloced_rec_buff_length > alloced_mult_rec_buff_length) {
        for (uint i = 0; i < table_share->keys; i++) {
            if (table_share->key_info[i].flags & HA_CLUSTERING) {
                uchar *newptr;
                if (!(newptr = (uchar *) my_realloc((void *) mult_rec_buff[i], alloced_rec_buff_length, MYF(MY_ALLOW_ZERO_PTR)))) {
                    assert(false);
                }
                mult_rec_buff[i] = newptr;
                mult_rec_dbt[i].ulen = alloced_rec_buff_length;
                mult_rec_dbt[i].flags = DB_DBT_USERMEM;
                mult_rec_dbt[i].data = mult_rec_buff[i];
            }
        }
        alloced_mult_rec_buff_length = alloced_rec_buff_length;
    }
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

int ha_tokudb::pack_row(
    DBT * row, 
    const uchar* record,
    uint index
    ) 
{
    uchar* fixed_field_ptr = NULL;
    uchar* var_field_offset_ptr = NULL;
    uchar* start_field_data_ptr = NULL;
    uchar* var_field_data_ptr = NULL;
    int r = ENOSYS;
    bzero((void *) row, sizeof(*row));

    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
    
    if (table_share->blob_fields) {
        if (fix_rec_buff_for_blob(max_row_length(record))) {
            r = HA_ERR_OUT_OF_MEM;
            goto cleanup;
        }
    }


    /* Copy null bits */
    memcpy(rec_buff, record, table_share->null_bytes);
    fixed_field_ptr = rec_buff + table_share->null_bytes;
    var_field_offset_ptr = fixed_field_ptr + share->kc_info.mcp_info[index].var_len_offset;
    start_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;
    var_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;

    //
    // assert that when the hidden primary key exists, primary_key_offsets is NULL
    //
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

    row->data = rec_buff;
    row->size = (size_t) (var_field_data_ptr - rec_buff);
    r = 0;

cleanup:
    dbug_tmp_restore_column_map(table->write_set, old_map);
    return r;
}


int ha_tokudb::unpack_blobs(
    uchar* record,
    const uchar* from_tokudb_blob,
    u_int32_t num_bytes,
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
        u_int32_t curr_field_index = share->kc_info.blob_fields[i]; 
        bool skip = check_bitmap ? 
            !(bitmap_is_set(table->read_set,curr_field_index) || 
                bitmap_is_set(table->write_set,curr_field_index)) : 
            false;
        Field* field = table->field[curr_field_index];
        u_int32_t len_bytes = field->row_pack_length();
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
    u_int32_t data_end_offset = 0;
    memcpy(record, fixed_field_ptr, table_share->null_bytes);
    fixed_field_ptr += table_share->null_bytes;

    var_field_offset_ptr = fixed_field_ptr + share->kc_info.mcp_info[index].var_len_offset;
    var_field_data_ptr = var_field_offset_ptr + share->kc_info.mcp_info[index].len_of_offsets;

    //
    // unpack the key, if necessary
    //
    if (!(hidden_primary_key && index == primary_key)) {
        unpack_key(record,key,index);
    }

    u_int32_t last_offset = 0;
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
            row->size - (u_int32_t)(var_field_data_ptr - (const uchar *)row->data),
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
        for (u_int32_t i = 0; i < num_fixed_cols_for_query; i++) {
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
        for (u_int32_t i = 0; i < num_var_cols_for_query; i++) {
            uint field_index = var_cols_for_query[i];
            Field* field = table->field[field_index];
            u_int32_t var_field_index = share->kc_info.cp_info[index][field_index].col_pack_val;
            u_int32_t data_start_offset;
            u_int32_t field_len;
            
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
                row->size - (u_int32_t)(var_field_data_ptr - (const uchar *)row->data),
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

u_int32_t ha_tokudb::place_key_into_mysql_buff(
    KEY* key_info, 
    uchar * record, 
    uchar* data
    ) 
{
    KEY_PART_INFO *key_part = key_info->key_part, *end = key_part + key_info->key_parts;
    uchar *pos = data;

    for (; key_part != end; key_part++) {
        if (key_part->null_bit) {
            if (*pos++ == NULL_COL_VAL) { // Null value
                //
                // We don't need to reset the record data as we will not access it
                // if the null data is set
                //            
                record[key_part->null_offset] |= key_part->null_bit;
                continue;
            }
            record[key_part->null_offset] &= ~key_part->null_bit;
        }
        //
        // HOPEFULLY TEMPORARY
        //
        assert(table->s->db_low_byte_first);
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
    u_int32_t bytes_read;
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

u_int32_t ha_tokudb::place_key_into_dbt_buff(
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
        //
        // HOPEFULLY TEMPORARY
        //
        assert(table->s->db_low_byte_first);
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
    u_int32_t size = 0;
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
    bzero((void *) key, sizeof(*key));
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

    bzero((void *) key, sizeof(*key));
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
        assert(table->s->db_low_byte_first);

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
// Reads the last element of dictionary of index keynr, and places
// the data into table->record[1].
//
int ha_tokudb::read_last(uint keynr) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_last");
    int do_commit = 0;
    if (transaction == NULL) {
        int r = db_env->txn_begin(db_env, 0, &transaction, 0);
        assert(r == 0);
        do_commit = 1;
    }
    int error = index_init(keynr, 0);
    if (error == 0)
        error = index_last(table->record[1]);
    index_end();
    if (do_commit) {
        commit_txn(transaction, 0);
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
        int error = read_last(primary_key);
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
    //
    // open status.tokudb
    //
    if (!share->status_block) {
        error = open_status_dictionary(
            &share->status_block, 
            share->table_name, 
            NULL
            );
        if (error) { 
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
    value.flags = DB_DBT_USERMEM;
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    assert(share->status_block);
    //
    // get version
    //
    value.ulen = sizeof(share->version);
    value.data = &share->version;
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
    if (txn) {
        commit_txn(txn,0);
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
    ret_val = tokudb_compare_two_keys(
        ref1 + sizeof(u_int32_t),
        *(u_int32_t *)ref1,
        ref2 + sizeof(u_int32_t),
        *(u_int32_t *)ref2,
        (uchar *)share->file->descriptor->data + 4,
        *(u_int32_t *)share->file->descriptor->data - 4,
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
bool ha_tokudb::may_table_be_empty() {
    int error;
    bool ret_val = false;
    DBC* tmp_cursor = NULL;
    DB_TXN* txn = NULL;

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) {
        goto cleanup;
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
    if (txn) {
        commit_txn(txn, 0);
        txn = NULL;
    }
    return ret_val;
}

void ha_tokudb::start_bulk_insert(ha_rows rows) {
    delay_updating_ai_metadata = true;
    ai_metadata_update_required = false;
    if (share->try_table_lock) {
        if (tokudb_prelock_empty && may_table_be_empty()) {
            acquire_table_lock(transaction, lock_write);
        }
        pthread_mutex_lock(&share->mutex);
        share->try_table_lock = false;
        pthread_mutex_unlock(&share->mutex);
    }
}

//
// Method that is called at the end of many calls to insert rows
// (ha_tokudb::write_row). If start_bulk_insert is called, then
// this is guaranteed to be called.
//
int ha_tokudb::end_bulk_insert() {
    int error = 0;
    if (ai_metadata_update_required) {
        pthread_mutex_lock(&share->mutex);
        error = update_max_auto_inc(share->status_block, share->last_auto_increment);
        pthread_mutex_unlock(&share->mutex);
    }
    delay_updating_ai_metadata = false;
    ai_metadata_update_required = false;
    return error;
}


int ha_tokudb::is_val_unique(bool* is_unique, uchar* record, KEY* key_info, uint dict_index, DB_TXN* txn) {
    DBT key;
    int error = 0;
    bool has_null;
    DBC* tmp_cursor = NULL;
    struct index_read_info ir_info;
    struct smart_dbt_info info;
    bzero((void *)&key, sizeof(key));
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
        0
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

int ha_tokudb::test_row_packing(uchar* record, DBT* pk_key, DBT* pk_val) {
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

    bzero(&tmp_pk_key, sizeof(DBT));
    bzero(&tmp_pk_val, sizeof(DBT));

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
        u_int32_t tmp_num_bytes = 0;
        int cmp;
        uchar* row_desc = NULL;
        u_int32_t desc_size = 0;
        
        if (keynr == primary_key) {
            continue;
        }

        create_dbt_key_from_table(&key, keynr, mult_key_buff[keynr], record, &has_null); 

        //
        // TEST
        //
        row_desc = (uchar *)share->key_file[keynr]->descriptor->data;
        row_desc += (*(u_int32_t *)row_desc);
        desc_size = (*(u_int32_t *)row_desc) - 4;
        row_desc += 4;
        tmp_num_bytes = pack_key_from_desc(
            key_buff3,
            row_desc,
            desc_size,
            &tmp_pk_key,
            &tmp_pk_val
            );
        assert(tmp_num_bytes == key.size);
        cmp = memcmp(key_buff3,mult_key_buff[keynr],tmp_num_bytes);
        assert(cmp == 0);

        //
        // test key packing of clustering keys
        //
        if (table->key_info[keynr].flags & HA_CLUSTERING) {
            error = pack_row(&row, (const uchar *) record, keynr);
            if (error) { goto cleanup; }
            uchar* tmp_buff = NULL;
            tmp_buff = (uchar *)my_malloc(alloced_rec_buff_length,MYF(MY_WME));
            assert(tmp_buff);
            row_desc = (uchar *)share->key_file[keynr]->descriptor->data;
            row_desc += (*(u_int32_t *)row_desc);
            row_desc += (*(u_int32_t *)row_desc);
            desc_size = (*(u_int32_t *)row_desc) - 4;
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

    error = 0;
cleanup:
    my_free(tmp_pk_key_data,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_pk_val_data,MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

int ha_tokudb::insert_rows_to_dictionaries(uchar* record, DBT* pk_key, DBT* pk_val, DB_TXN* txn) {
    int error;
    DBT row, key;
    u_int32_t put_flags;
    THD *thd = ha_thd();
    bool is_replace_into;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    ulonglong wait_lock_time = get_write_lock_wait_time(thd);

    is_replace_into = (thd_sql_command(thd) == SQLCOM_REPLACE) || 
        (thd_sql_command(thd) == SQLCOM_REPLACE_SELECT);

    //
    // first the primary key (because it must be unique, has highest chance of failure)
    //
    put_flags = hidden_primary_key ? DB_YESOVERWRITE : DB_NOOVERWRITE;
    if (thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS) && !is_replace_into) {
        put_flags = DB_YESOVERWRITE;
    }
    //
    // optimization for "REPLACE INTO..." command
    // if the command is "REPLACE INTO" and the only table
    // is the main table, then we can simply insert the element
    // with DB_YESOVERWRITE. If the element does not exist,
    // it will act as a normal insert, and if it does exist, it 
    // will act as a replace, which is exactly what REPLACE INTO is supposed
    // to do. We cannot do this if curr_num_DBs > 1, because then we lose
    // consistency between indexes
    //
    if (is_replace_into && (curr_num_DBs == 1)) {
        put_flags = DB_YESOVERWRITE; // original put_flags can only be DB_YESOVERWRITE or DB_NOOVERWRITE
    }
 

    lockretryN(wait_lock_time){
        error = share->file->put(
            share->file, 
            txn, 
            pk_key,
            pk_val, 
            put_flags
            );
        lockretry_wait;
    }

    if (error) {
        last_dup_key = primary_key;
        goto cleanup;
    }

    //
    // now insertion for rest of indexes
    //
    for (uint keynr = 0; keynr < table_share->keys; keynr++) {
        bool has_null;
        
        if (keynr == primary_key) {
            continue;
        }

        create_dbt_key_from_table(&key, keynr, mult_key_buff[keynr], record, &has_null); 

        put_flags = DB_YESOVERWRITE;

        if (table->key_info[keynr].flags & HA_CLUSTERING) {
            error = pack_row(&row, (const uchar *) record, keynr);
            if (error) { goto cleanup; }
        }
        else {
            bzero((void *) &row, sizeof(row));
        }

        lockretryN(wait_lock_time){
            error = share->key_file[keynr]->put(
                share->key_file[keynr], 
                txn,
                &key,
                &row,
                put_flags
                );
            lockretry_wait;
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

cleanup:
    return error;
}

int ha_tokudb::insert_rows_to_dictionaries_mult(DBT* pk_key, DBT* pk_val, DB_TXN* txn, THD* thd) {
    int error;
    bool is_replace_into;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    ulonglong wait_lock_time = get_write_lock_wait_time(thd);
    is_replace_into = (thd_sql_command(thd) == SQLCOM_REPLACE) || 
        (thd_sql_command(thd) == SQLCOM_REPLACE_SELECT);

    if (thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS) && !is_replace_into) {
        share->mult_put_flags[primary_key] = DB_YESOVERWRITE;
    }
    else {
        share->mult_put_flags[primary_key] = DB_NOOVERWRITE;
    }
    
    lockretryN(wait_lock_time){
        error = db_env->put_multiple(
            db_env, 
            NULL, 
            txn, 
            pk_key, 
            pk_val,
            curr_num_DBs, 
            share->key_file, 
            mult_key_dbt,
            mult_rec_dbt,
            share->mult_put_flags, 
            NULL
            );
        lockretry_wait;
    }

    //
    // We break if we hit an error, unless it is a dup key error
    // and MySQL told us to ignore duplicate key errors
    //
    if (error) {
        last_dup_key = primary_key;
    }
    return error;
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
    DBT row, prim_key;
    int error;
    THD *thd = ha_thd();
    bool has_null;
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;
    tokudb_trx_data *trx = NULL;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);

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

    if (using_ignore) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
    }
    
    txn = using_ignore ? sub_trans : transaction;    

    //
    // make sure the buffers for the rows are big enough
    //
    fix_mult_rec_buff();

    error = do_uniqueness_checks(record, txn, thd);
    if (error) { goto cleanup; }

    if (tokudb_debug & TOKUDB_DEBUG_CHECK_KEY) {
        error = test_row_packing(record,&prim_key,&row);
        if (error) { goto cleanup; }
    }

    if (curr_num_DBs == 1 || share->version <= 2) {
        error = insert_rows_to_dictionaries(record,&prim_key, &row, txn);
        if (error) { goto cleanup; }
    }
    else {
        error = insert_rows_to_dictionaries_mult(&prim_key, &row, txn, thd);
        if (error) { goto cleanup; }
    }

    trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    if (!error) {
        added_rows++;
        trx->stmt_progress.inserted++;
        track_progress(thd);
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
            abort_txn(sub_trans);
        }
        else {
            commit_txn(sub_trans, DB_TXN_NOSYNC);
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
    DBT prim_key, key, old_prim_key, row, prim_row;
    int error;
    bool primary_key_changed;
    bool has_null;
    THD* thd = ha_thd();
    DB_TXN* sub_trans = NULL;
    DB_TXN* txn = NULL;
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    u_int32_t mult_put_flags[MAX_KEY + 1] = {DB_YESOVERWRITE};
    DB* dbs[MAX_KEY + 1];
    DBT key_dbts[MAX_KEY + 1];
    DBT rec_dbts[MAX_KEY + 1];
    u_int32_t curr_db_index;
    bool use_put_multiple = share->version > 2;
    ulonglong wait_lock_time = get_write_lock_wait_time(thd);

    LINT_INIT(error);
    bzero((void *) &row, sizeof(row));
    bzero((void *) &prim_key, sizeof(prim_key));
    bzero((void *) &old_prim_key, sizeof(old_prim_key));
    bzero((void *) &prim_row, sizeof(prim_row));
    bzero((void *) &key, sizeof(key));
    bzero((void *) &key_dbts, sizeof(key));
    bzero((void *) &rec_dbts, sizeof(key));


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

    if (using_ignore) {
        error = db_env->txn_begin(db_env, transaction, &sub_trans, DB_INHERIT_ISOLATION);
        if (error) {
            goto cleanup;
        }
    }    
    txn = using_ignore ? sub_trans : transaction;


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

    if (primary_key_changed) {
        // Primary key changed or we are updating a key that can have duplicates.
        // Delete the old row and add a new one
        error = remove_key(txn, primary_key, old_row, &old_prim_key);
        if (error) { goto cleanup; }
    }

    error = pack_row(&prim_row, new_row, primary_key);
    if (error) { goto cleanup; }


    if (use_put_multiple) {
        dbs[0] = share->key_file[primary_key];
        key_dbts[0] = prim_key;
        rec_dbts[0] = prim_row;
        mult_put_flags[0] = primary_key_changed ? DB_NOOVERWRITE : DB_YESOVERWRITE;
    }
    else {
        u_int32_t put_flags = primary_key_changed ? DB_NOOVERWRITE : DB_YESOVERWRITE;
        error = share->file->put(share->file, txn, &prim_key, &prim_row, put_flags);
        if (error) { 
            last_dup_key = primary_key;
            goto cleanup; 
        }
    }
    curr_db_index = 1;
    // Update all other keys
    for (uint keynr = 0; keynr < table_share->keys; keynr++) {
        bool secondary_key_changed = key_cmp(keynr, old_row, new_row);
        if (keynr == primary_key) {
            continue;
        }
        if (table->key_info[keynr].flags & HA_CLUSTERING ||
            secondary_key_changed || 
            primary_key_changed
            ) 
        {
            bool is_unique_key = table->key_info[keynr].flags & HA_NOSAME;
            //
            // only remove the old value if the key has changed 
            // if the key has not changed (in case of clustering keys, 
            // then we overwrite  the old value)
            // 
            if (secondary_key_changed || primary_key_changed) {
                error = remove_key(txn, keynr, old_row, &old_prim_key);
                if (error) {
                    goto cleanup;
                }
            }

            //
            // if unique key, check uniqueness constraint
            // but, we do not need to check it if the key has a null
            // and we do not need to check it if unique_checks is off
            //
            if (is_unique_key && !thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
                bool is_unique = false;
                error = is_val_unique(&is_unique, new_row, &table->key_info[keynr], keynr, txn);
                if (error) { goto cleanup; }
                if (!is_unique) {
                    error = DB_KEYEXIST;
                    last_dup_key = keynr;
                    goto cleanup;
                }
            }

            if (!use_put_multiple) {
                u_int32_t put_flags;
                put_flags = DB_YESOVERWRITE;
                create_dbt_key_from_table(&key, keynr, key_buff2, new_row, &has_null);

                if (table->key_info[keynr].flags & HA_CLUSTERING) {
                    error = pack_row(&row, (const uchar *) new_row, keynr);
                    if (error){ goto cleanup; }
                }
                else {
                    bzero((void *) &row, sizeof(row));
                }
                //
                // make sure that for clustering keys, we are using DB_YESOVERWRITE,
                // therefore making this put an overwrite if the key has not changed
                //
                lockretryN(wait_lock_time){
                    error = share->key_file[keynr]->put(
                        share->key_file[keynr], 
                        txn,
                        &key,
                        &row, 
                        put_flags
                        );
                    lockretry_wait;
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
            else {
                dbs[curr_db_index] = share->key_file[keynr];
                key_dbts[curr_db_index] = mult_key_dbt[keynr];
                rec_dbts[curr_db_index] = mult_rec_dbt[keynr];
                curr_db_index++;
            }
        }
    }

    if (use_put_multiple) {
        lockretryN(wait_lock_time){
            error = db_env->put_multiple(
                db_env, 
                NULL, 
                txn, 
                &prim_key, 
                &prim_row,
                curr_db_index, 
                dbs, 
                key_dbts,
                rec_dbts,
                mult_put_flags, 
                NULL
                );
            lockretry_wait;
        }
    }
    if (!error) {
        trx->stmt_progress.updated++;
        track_progress(thd);
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
            abort_txn(sub_trans);
        }
        else {
            commit_txn(sub_trans, DB_TXN_NOSYNC);
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
    ulonglong wait_lock_time = get_write_lock_wait_time(ha_thd());
    DBUG_PRINT("enter", ("index: %d", keynr));
    DBUG_PRINT("primary", ("index: %d", primary_key));
    DBUG_DUMP("prim_key", (uchar *) prim_key->data, prim_key->size);

    if (keynr == primary_key) {  // Unique key
        DBUG_PRINT("Primary key", ("index: %d", keynr));
        lockretryN(wait_lock_time){
            error = share->key_file[keynr]->del(share->key_file[keynr], trans, prim_key , DB_DELETE_ANY);
            lockretry_wait;
        }
    }
    else {
        DBUG_PRINT("Secondary key", ("index: %d", keynr));
        create_dbt_key_from_table(&key, keynr, key_buff2, record, &has_null);
        lockretryN(wait_lock_time){
            error = share->key_file[keynr]->del(share->key_file[keynr], trans, &key , DB_DELETE_ANY);
            lockretry_wait;
        }
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
int ha_tokudb::remove_keys(DB_TXN * trans, const uchar * record, DBT * prim_key) {
    int result = 0;
    for (uint keynr = 0; keynr < table_share->keys + test(hidden_primary_key); keynr++) {
        int new_error = remove_key(trans, keynr, record, prim_key);
        if (new_error) {
            result = new_error;     // Return last error
            break;          // Let rollback correct things
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
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;

    statistic_increment(table->in_use->status_var.ha_delete_count, &LOCK_status);

    create_dbt_key_from_table(&prim_key, primary_key, key_buff, record, &has_null);
    if (hidden_primary_key) {
        keys.set_bit(primary_key);
    }
    /* Subtransactions may be used in order to retry the delete in
       case we get a DB_LOCK_DEADLOCK error. */
    DB_TXN *sub_trans = transaction;
    error = remove_keys(sub_trans, record, &prim_key);
    if (error) {
        DBUG_PRINT("error", ("Got error %d", error));
    }
    else {
        deleted_rows++;
        trx->stmt_progress.deleted++;
        track_progress(thd);
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// takes as input table->read_set and table->write_set
// and puts list of field indexes that need to be read in
// unpack_row in the member variables fixed_cols_for_query
// and var_cols_for_query
//
void ha_tokudb::set_query_columns(uint keynr) {
    u_int32_t curr_fixed_col_index = 0;
    u_int32_t curr_var_col_index = 0;
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
    if (active_index != MAX_KEY || active_index == primary_key) {
        set_query_columns(active_index);
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
    int error;
    DB* db = share->key_file[active_index];
    lockretry {
        error = db->pre_acquire_read_lock(
            db, 
            transaction, 
            db->dbt_neg_infty(), db->dbt_neg_infty(), 
            db->dbt_pos_infty(), db->dbt_pos_infty()
            );
        lockretry_wait;
    }
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
int ha_tokudb::prepare_index_key_scan( const uchar * key, uint key_len ) {
    int error = 0;
    DBT start_key, end_key;
    pack_key(&start_key, active_index, key_buff, key, key_len, COL_NEG_INF);
    pack_key(&end_key, active_index, key_buff2, key, key_len, COL_POS_INF);

    lockretry {
        error = share->key_file[active_index]->pre_acquire_read_lock(
            share->key_file[active_index], 
            transaction, 
            &start_key, 
            share->key_file[active_index]->dbt_neg_infty(), 
            &end_key, 
            share->key_file[active_index]->dbt_pos_infty()
            );
        lockretry_wait;            
    }
    if (error){ 
        goto cleanup; 
    }

    range_lock_grabbed = true;
    error = 0;
cleanup:
    if (error) {
        last_cursor_error = error;
        //
        // cursor should be initialized here, but in case it is not, we still check
        //
        if (cursor) {
            int r = cursor->c_close(cursor);
            assert(r==0);
            cursor = NULL;
        }
    }
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
    last_cursor_error = 0;
    range_lock_grabbed = false;
    DBUG_ASSERT(keynr <= table->s->keys);
    DBUG_ASSERT(share->key_file[keynr]);
    if ((error = share->key_file[keynr]->cursor(share->key_file[keynr], transaction, &cursor, 0))) {
        last_cursor_error = error;
        cursor = NULL;             // Safety
        goto exit;
    }
    bzero((void *) &last_key, sizeof(last_key));

    if (thd_sql_command(thd) == SQLCOM_SELECT) {
        set_query_columns(keynr);
        unpack_entire_row = false;
    }
    else {
        unpack_entire_row = true;
    }
    error = 0;
exit:
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
        assert(error==0);
        cursor = NULL;
        last_cursor_error = 0;
    }
    active_index = MAX_KEY;

    //
    // reset query variables
    //
    unpack_entire_row = true;
    read_blobs = true;
    read_key = true;
    num_fixed_cols_for_query = 0;
    num_var_cols_for_query = 0;
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::handle_cursor_error(int error, int err_to_return, uint keynr) {
    TOKUDB_DBUG_ENTER("ha_tokudb::handle_cursor_error");
    if (error) {
        last_cursor_error = error;
        table->status = STATUS_NOT_FOUND;
        int r = cursor->c_close(cursor);
        assert(r==0);
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
        bzero((void *) &last_key, sizeof(last_key));
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
    int error;
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
        0, 
        &last_key, 
        smart_dbt_callback_rowread_ptquery, 
        &info
        );
    if (error) {
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
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next_same %p", this); 
    int error; 
    struct smart_dbt_info info; 
    DBT curr_key;
    DBT found_key;
    bool has_null;
    int cmp;
    u_int32_t flags;
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR(); 

    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status); 
    info.ha = this; 
    info.buf = buf; 
    info.keynr = active_index; 

    pack_key(&curr_key, active_index, key_buff2, key, keylen, COL_ZERO);

    flags = SET_READ_FLAG(0); 
    lockretry {
        error = cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error, HA_ERR_END_OF_FILE,active_index);
    if (error) {
        goto cleanup;
    }
    if (!key_read && active_index != primary_key && !(table->key_info[active_index].flags & HA_CLUSTERING)) { 
        error = read_full_row(buf); 
        if (error) {
            goto cleanup;
        }
    } 
    //
    // now do the comparison
    //
    create_dbt_key_from_table(&found_key,active_index,key_buff3,buf,&has_null);
    cmp = tokudb_prefix_cmp_dbt_key(share->key_file[active_index], &curr_key, &found_key);
    if (cmp) {
        error = HA_ERR_END_OF_FILE; 
    }
    trx->stmt_progress.queried++;
    track_progress(thd);
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
    // TOKUDB_DBUG_DUMP("key=", key, key_len);
    DBT row;
    DBT lookup_key;
    int error;    
    u_int32_t flags = 0;
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    struct smart_dbt_info info;
    struct index_read_info ir_info;

    HANDLE_INVALID_CURSOR();

    table->in_use->status_var.ha_read_key_count++;
    bzero((void *) &row, sizeof(row));

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    ir_info.smart_dbt_info = info;
    ir_info.cmp = 0;

    flags = SET_READ_FLAG(0);
    switch (find_flag) {
    case HA_READ_KEY_EXACT: /* Find first record else error */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_NEG_INF);
        ir_info.orig_key = &lookup_key;
        error = cursor->c_getf_set_range(cursor, flags, &lookup_key, SMART_DBT_IR_CALLBACK, &ir_info);
        if (ir_info.cmp) {
            error = DB_NOTFOUND;
        }
        break;
    case HA_READ_AFTER_KEY: /* Find next rec. after key-record */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_POS_INF);
        error = cursor->c_getf_set_range(cursor, flags, &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_BEFORE_KEY: /* Find next rec. before key-record */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_NEG_INF);
        error = cursor->c_getf_set_range_reverse(cursor, flags, &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_KEY_OR_NEXT: /* Record or next record */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_NEG_INF);
        error = cursor->c_getf_set_range(cursor, flags, &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    //
    // This case does not seem to ever be used, it is ok for it to be slow
    //
    case HA_READ_KEY_OR_PREV: /* Record or previous */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_NEG_INF);
        ir_info.orig_key = &lookup_key;
        error = cursor->c_getf_set_range(cursor, flags, &lookup_key, SMART_DBT_IR_CALLBACK, &ir_info);
        if (error == DB_NOTFOUND) {
            error = cursor->c_getf_last(cursor, flags, SMART_DBT_CALLBACK, &info);
        }
        else if (ir_info.cmp) {
            error = cursor->c_getf_prev(cursor, flags, SMART_DBT_CALLBACK, &info);
        }
        break;
    case HA_READ_PREFIX_LAST_OR_PREV: /* Last or prev key with the same prefix */
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_POS_INF);
        error = cursor->c_getf_set_range_reverse(cursor, flags, &lookup_key, SMART_DBT_CALLBACK, &info);
        break;
    case HA_READ_PREFIX_LAST:
        pack_key(&lookup_key, active_index, key_buff3, key, key_len, COL_POS_INF);
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
    error = handle_cursor_error(error,HA_ERR_KEY_NOT_FOUND,active_index);
    if (!error && !key_read && active_index != primary_key && !(table->key_info[active_index].flags & HA_CLUSTERING)) {
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
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();
    
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    lockretry {
        error = cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error, HA_ERR_END_OF_FILE,active_index);
    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }
    trx->stmt_progress.queried++;
    track_progress(thd);
cleanup:
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
    TOKUDB_DBUG_ENTER("ha_tokudb::index_next");
    int error; 
    struct smart_dbt_info info;
    u_int32_t flags = SET_READ_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();
    
    statistic_increment(table->in_use->status_var.ha_read_next_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    lockretry {
        error = cursor->c_getf_prev(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error,HA_ERR_END_OF_FILE,active_index);

    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
        error = read_full_row(buf);
    }
    trx->stmt_progress.queried++;
    track_progress(thd);
    
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
    struct smart_dbt_info info;
    u_int32_t flags = SET_READ_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();

    statistic_increment(table->in_use->status_var.ha_read_first_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    lockretry {
        error = cursor->c_getf_first(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error,HA_ERR_END_OF_FILE,active_index);

    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
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
    int error;
    struct smart_dbt_info info;
    u_int32_t flags = SET_READ_FLAG(0);
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    HANDLE_INVALID_CURSOR();

    statistic_increment(table->in_use->status_var.ha_read_last_count, &LOCK_status);

    info.ha = this;
    info.buf = buf;
    info.keynr = active_index;

    lockretry {
        error = cursor->c_getf_last(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error,HA_ERR_END_OF_FILE,active_index);
    //
    // still need to get entire contents of the row if operation done on
    // secondary DB and it was NOT a covering index
    //
    if (!error && !key_read && (active_index != primary_key) && !(table->key_info[active_index].flags & HA_CLUSTERING) ) {
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
    int error;
    range_lock_grabbed = false;
    if (scan) {
        DB* db = share->key_file[primary_key];
        lockretry {
            error = db->pre_acquire_read_lock(db, transaction, db->dbt_neg_infty(), NULL, db->dbt_pos_infty(), NULL);
            lockretry_wait;
        }
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
    THD* thd = ha_thd();
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);;
    
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

    lockretry {
        error = cursor->c_getf_next(cursor, flags, SMART_DBT_CALLBACK, &info);
        lockretry_wait;
    }
    error = handle_cursor_error(error, HA_ERR_END_OF_FILE,primary_key);

    trx->stmt_progress.queried++;
    track_progress(thd);
cleanup:
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
                r = sprintf(next_status, "%sInserted about %llu row%s", first ? "" : ", ", trx->stmt_progress.inserted, trx->stmt_progress.inserted == 1 ? "" : "s"); 
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
    bzero((void *) to, sizeof(*to));
    to->data = pos + sizeof(u_int32_t);
    to->size = *(u_int32_t *)pos;
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
    struct smart_dbt_info info;
    bool old_unpack_entire_row = unpack_entire_row;
    DBT* key = get_pos(&db_pos, pos); 

    unpack_entire_row = true;
    statistic_increment(table->in_use->status_var.ha_read_rnd_count, &LOCK_status);
    active_index = MAX_KEY;

    info.ha = this;
    info.buf = buf;
    info.keynr = primary_key;

    error = share->file->getf_set(share->file, transaction, 0, key, smart_dbt_callback_rowread_ptquery, &info);
    if (error == DB_NOTFOUND) {
        error = HA_ERR_KEY_NOT_FOUND;
        goto cleanup;
    }
cleanup:
    unpack_entire_row = old_unpack_entire_row;
    TOKUDB_DBUG_RETURN(error);
}


int ha_tokudb::prelock_range( const key_range *start_key, const key_range *end_key) {
    TOKUDB_DBUG_ENTER("ha_tokudb::read_range_first");
    int error;
    DBT start_dbt_key;
    const DBT* start_dbt_data = NULL;
    DBT end_dbt_key;
    const DBT* end_dbt_data = NULL;
    uchar* start_key_buff  = key_buff2;
    uchar* end_key_buff = key_buff3;

    bzero((void *) &start_dbt_key, sizeof(start_dbt_key));
    bzero((void *) &end_dbt_key, sizeof(end_dbt_key));

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

    lockretry {
        error = share->key_file[active_index]->pre_acquire_read_lock(
            share->key_file[active_index], 
            transaction, 
            start_key ? &start_dbt_key : share->key_file[active_index]->dbt_neg_infty(), 
            start_dbt_data, 
            end_key ? &end_dbt_key : share->key_file[active_index]->dbt_pos_infty(), 
            end_dbt_data
            );
        lockretry_wait;
    }
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
    int error;
    error = prelock_range(start_key, end_key);
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
        DBUG_ASSERT(ref_length == (TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH + sizeof(u_int32_t)));
        memcpy_fixed(ref + sizeof(u_int32_t), current_ident, TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH);
        *(u_int32_t *)ref = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
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
    int error;
    DB_TXN* txn = NULL;
    uint curr_num_DBs = table->s->keys + test(hidden_primary_key);
    DB_BTREE_STAT64 dict_stats;
    if (flag & HA_STATUS_VARIABLE) {
        // Just to get optimizations right
        stats.records = share->rows + share->rows_from_locked_table;
        stats.deleted = 0;
        if (!(flag & HA_STATUS_NO_LOCK)) {
            error = db_env->txn_begin(db_env, NULL, &txn, DB_READ_UNCOMMITTED);
            if (error) { goto cleanup; }

            error = share->file->stat64(
                share->file, 
                txn, 
                &dict_stats
                );
            if (error) { goto cleanup; }
            
            stats.data_file_length = dict_stats.bt_dsize;
            if (hidden_primary_key) {
                //
                // in this case, we have a hidden primary key, do not
                // want to report space taken up by the hidden primary key to the user
                //
                u_int64_t hpk_space = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH*dict_stats.bt_ndata;
                stats.data_file_length = (hpk_space > stats.data_file_length) ? 0 : stats.data_file_length - hpk_space;
            }
            else {
                //
                // one infinity byte per key needs to be subtracted
                //
                u_int64_t inf_byte_space = dict_stats.bt_ndata;
                stats.data_file_length = (inf_byte_space > stats.data_file_length) ? 0 : stats.data_file_length - inf_byte_space;
            }

            stats.mean_rec_length = stats.records ? (ulong)(stats.data_file_length/stats.records) : 0;
            stats.index_file_length = 0;
            for (uint i = 0; i < curr_num_DBs; i++) {
                if (i == primary_key) {
                    continue;
                }
                error = share->key_file[i]->stat64(
                    share->key_file[i], 
                    txn, 
                    &dict_stats
                    );
                if (error) { goto cleanup; }
                stats.index_file_length += dict_stats.bt_dsize;
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
            if (error) break;
        }
        if (error) goto cleanup;
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
    return error;
}


int ha_tokudb::create_txn(THD* thd, tokudb_trx_data* trx) {
    int error;
    ulong tx_isolation = thd_tx_isolation(thd);
    HA_TOKU_ISO_LEVEL toku_iso_level = tx_to_toku_iso(tx_isolation);

    /* First table lock, start transaction */
    if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) && 
         !trx->all &&
         (thd_sql_command(thd) != SQLCOM_CREATE_TABLE) &&
         (thd_sql_command(thd) != SQLCOM_DROP_TABLE) &&
         (thd_sql_command(thd) != SQLCOM_ALTER_TABLE)) {
        /* QQQ We have to start a master transaction */
        DBUG_PRINT("trans", ("starting transaction all:  options: 0x%lx", (ulong) thd->options));
        if ((error = db_env->txn_begin(db_env, NULL, &trx->all, toku_iso_to_txn_flag(toku_iso_level)))) {
            trx->tokudb_lock_count--;      // We didn't get the lock
            goto cleanup;
        }
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("master:%p\n", trx->all);
        }
        trx->sp_level = trx->all;
        trans_register_ha(thd, TRUE, tokudb_hton);
    }
    DBUG_PRINT("trans", ("starting transaction stmt"));
    if (trx->stmt) { 
        if (tokudb_debug & TOKUDB_DEBUG_TXN) {
            TOKUDB_TRACE("warning:stmt=%p\n", trx->stmt);
        }
    }
    u_int32_t txn_begin_flags;
    if (trx->all == NULL) {
        txn_begin_flags = toku_iso_to_txn_flag(toku_iso_level);
    }
    else {
        txn_begin_flags = DB_INHERIT_ISOLATION;
    }
    if ((error = db_env->txn_begin(db_env, trx->sp_level, &trx->stmt, txn_begin_flags))) {
        /* We leave the possible master transaction open */
        trx->tokudb_lock_count--;  // We didn't get the lock
        goto cleanup;
    }
    if (tokudb_debug & TOKUDB_DEBUG_TXN) {
        TOKUDB_TRACE("stmt:%p:%p\n", trx->sp_level, trx->stmt);
    }
    reset_stmt_progress(&trx->stmt_progress);
    trans_register_ha(thd, FALSE, tokudb_hton);
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
    // QQQ this is here to allow experiments without transactions
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
        if (!trx->tokudb_lock_count++) {
            DBUG_ASSERT(trx->stmt == 0);
            transaction = NULL;    // Safety
            error = create_txn(thd, trx);
            if (error) {
                goto cleanup;
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
                commit_txn(trx->stmt, 0);
                reset_stmt_progress(&trx->stmt_progress);
                if (tokudb_debug & TOKUDB_DEBUG_TXN)
                    TOKUDB_TRACE("commit:%p:%d\n", trx->stmt, error);
                trx->stmt = NULL;
            }
        }
        transaction = NULL;
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
    TOKUDB_DBUG_ENTER("ha_tokudb::start_stmt");
    int error = 0;


    tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
    DBUG_ASSERT(trx);
    /*
       note that trx->stmt may have been already initialized as start_stmt()
       is called for *each table* not for each storage engine,
       and there could be many bdb tables referenced in the query
     */
    if (!trx->stmt) {
        DBUG_PRINT("trans", ("starting transaction stmt"));
        error = create_txn(thd, trx);
        if (error) {
            goto cleanup;
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
        acquire_table_lock(trx->stmt,lock_read);
    }
    else {
        acquire_table_lock(trx->stmt,lock_write);
    }    
    if (added_rows > deleted_rows) {
        share->rows_from_locked_table = added_rows - deleted_rows;
    }
    transaction = trx->stmt;
cleanup:
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
    if (tokudb_debug & TOKUDB_DEBUG_LOCK)
        TOKUDB_TRACE("%s lock_type=%d cmd=%d\n", __FUNCTION__, lock_type, thd_sql_command(thd));
    if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
        /* If we are not doing a LOCK TABLE, then allow multiple writers */
        if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) && 
            !thd->in_lock_tables && thd_sql_command(thd) != SQLCOM_TRUNCATE && !thd_tablespace_op(thd)) {
            lock_type = TL_WRITE_ALLOW_WRITE;
        }
        lock.type = lock_type;
    }
    *to++ = &lock;
    if (tokudb_debug & TOKUDB_DEBUG_LOCK)
        TOKUDB_TRACE("%s lock_type=%d\n", __FUNCTION__, lock_type);
    DBUG_RETURN(to);
}

int toku_dbt_up(DB*,
                                 u_int32_t old_version, const DBT *old_descriptor, const DBT *old_key, const DBT *old_val,
                                 u_int32_t new_version, const DBT *new_descriptor, const DBT *new_key, const DBT *new_val) {
    assert(false);
    return 0;
}


static int create_sub_table(const char *table_name, DBT* row_descriptor, DB_TXN* txn) {
    TOKUDB_DBUG_ENTER("create_sub_table");
    int error;
    DB *file = NULL;
    
    
    error = db_create(&file, db_env, 0);
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when creating table", error));
        my_errno = error;
        goto exit;
    }
        
    error = file->set_descriptor(file, 1, row_descriptor, toku_dbt_up);
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when setting row descriptor for table '%s'", error, table_name));
        goto exit;
    }
    
    error = file->open(file, txn, table_name, NULL, DB_BTREE, DB_THREAD | DB_CREATE | DB_EXCL, my_umask);
    if (error) {
        DBUG_PRINT("error", ("Got error: %d when opening table '%s'", error, table_name));
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
        create_info->auto_increment_value = stats.auto_increment_value;
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

//
// creates dictionary for secondary index, with key description key_info, all using txn
//
int ha_tokudb::create_secondary_dictionary(const char* name, TABLE* form, KEY* key_info, DB_TXN* txn, KEY_AND_COL_INFO* kc_info, u_int32_t keynr) {
    int error;
    DBT row_descriptor;
    uchar* row_desc_buff = NULL;
    uchar* ptr = NULL;
    char* newname = NULL;
    KEY* prim_key = NULL;
    char dict_name[MAX_DICT_NAME_LEN];
    u_int32_t max_row_desc_buff_size;

    uint hpk= (form->s->primary_key >= MAX_KEY) ? TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH : 0;

    bzero(&row_descriptor, sizeof(row_descriptor));
    
    max_row_desc_buff_size = 2*(form->s->fields * 6)+10; // upper bound of key comparison descriptor
    max_row_desc_buff_size += get_max_secondary_key_pack_desc_size(kc_info); // upper bound for sec. key part
    max_row_desc_buff_size += get_max_clustering_val_pack_desc_size(form->s); // upper bound for clustering val part


    row_desc_buff = (uchar *)my_malloc(max_row_desc_buff_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}
    ptr = row_desc_buff;

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
    ptr += create_toku_key_descriptor(
        row_desc_buff,
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

    row_descriptor.size = ptr - row_desc_buff;
    assert(row_descriptor.size <= max_row_desc_buff_size);

    error = create_sub_table(newname, &row_descriptor, txn);
cleanup:    
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

//
// create and close the main dictionarr with name of "name" using table form, all within
// transaction txn.
//
int ha_tokudb::create_main_dictionary(const char* name, TABLE* form, DB_TXN* txn, KEY_AND_COL_INFO* kc_info) {
    int error;
    DBT row_descriptor;
    uchar* row_desc_buff = NULL;
    uchar* ptr = NULL;
    char* newname = NULL;
    KEY* prim_key = NULL;
    u_int32_t max_row_desc_buff_size;

    uint hpk= (form->s->primary_key >= MAX_KEY) ? TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH : 0;

    bzero(&row_descriptor, sizeof(row_descriptor));
    max_row_desc_buff_size = 2*(form->s->fields * 6)+10; // upper bound of key comparison descriptor
    max_row_desc_buff_size += get_max_secondary_key_pack_desc_size(kc_info); // upper bound for sec. key part
    max_row_desc_buff_size += get_max_clustering_val_pack_desc_size(form->s); // upper bound for clustering val part

    row_desc_buff = (uchar *)my_malloc(max_row_desc_buff_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}
    ptr = row_desc_buff;

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
    ptr += create_toku_key_descriptor(
        row_desc_buff, 
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


    row_descriptor.size = ptr - row_desc_buff;
    assert(row_descriptor.size <= max_row_desc_buff_size);

    /* Create the main table that will hold the real rows */
    error = create_sub_table(newname, &row_descriptor, txn);
cleanup:    
    my_free(newname, MYF(MY_ALLOW_ZERO_PTR));
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    return error;
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
    char* newname = NULL;
    KEY_AND_COL_INFO kc_info;
    bzero(&kc_info, sizeof(kc_info));

    pthread_mutex_lock(&tokudb_meta_mutex);

    newname = (char *)my_malloc(get_max_dict_name_path_length(name),MYF(MY_WME));
    if (newname == NULL){ error = ENOMEM; goto cleanup;}

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

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

    error = status_block->open(status_block, txn, newname, NULL, DB_BTREE, DB_CREATE | DB_EXCL, 0);
    if (error) { goto cleanup; }

    version = HA_TOKU_VERSION;
    capabilities = HA_TOKU_CAP;
    
    error = write_to_status(status_block, hatoku_version,&version,sizeof(version), txn);
    if (error) { goto cleanup; }

    error = write_to_status(status_block, hatoku_capabilities,&capabilities,sizeof(capabilities), txn);
    if (error) { goto cleanup; }

    error = write_auto_inc_create(status_block, create_info->auto_increment_value, txn);
    if (error) { goto cleanup; }

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

    error = create_main_dictionary(name, form, txn, &kc_info);
    if (error) {
        goto cleanup;
    }


    for (uint i = 0; i < form->s->keys; i++) {
        if (i != primary_key) {
            error = create_secondary_dictionary(name, form, &form->key_info[i], txn, &kc_info, i);
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
    if (txn) {
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
    bzero(&curr_key, sizeof(curr_key));
    bzero(&curr_val, sizeof(curr_val));
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
    if (error == DB_LOCK_NOTGRANTED) {
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
    if (error == DB_LOCK_NOTGRANTED) {
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
    DBT key;
    ha_rows ret_val = HA_TOKUDB_RANGE_COUNT;
    DB *kfile = share->key_file[keynr];
    u_int64_t less, equal, greater;
    u_int64_t start_rows, end_rows, rows;
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
        end_rows = stats.records;
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
    bzero(&key, sizeof(key));
    bzero(&value, sizeof(value));
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
    int error;
    uint curr_index = 0;
    DBC* tmp_cursor = NULL;
    int cursor_ret_val = 0;
    DBT current_primary_key;
    DB_TXN* txn = NULL;
    uchar* tmp_key_buff = NULL;
    uchar* tmp_prim_key_buff = NULL;
    uchar* tmp_record = NULL;
    THD* thd = ha_thd(); 
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

    tmp_key_buff = (uchar *)my_malloc(2*table_arg->s->rec_buff_length, MYF(MY_WME));
    tmp_prim_key_buff = (uchar *)my_malloc(2*table_arg->s->rec_buff_length, MYF(MY_WME));
    tmp_record = table->record[0];
    if (tmp_key_buff == NULL ||
        tmp_prim_key_buff == NULL ) {
        error = ENOMEM;
        goto cleanup;
    }

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }


    
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
    // open all the DB files and set the appropriate variables in share
    // they go to the end of share->key_file
    //
    curr_index = curr_num_DBs;
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


        error = create_secondary_dictionary(share->table_name, table_arg, &key_info[i], txn, &share->kc_info, curr_index);
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
    

    //
    // grab some locks to make this go faster
    // first a global read lock on the main DB, because
    // we intend to scan the entire thing
    //
    lockretry {
        error = share->file->pre_acquire_read_lock(
            share->file, 
            txn, 
            share->file->dbt_neg_infty(), 
            NULL, 
            share->file->dbt_pos_infty(), 
            NULL
            );
        lockretry_wait;
    }
    if (error) { goto cleanup; }

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
        if (error) { goto cleanup; }
    }

    //
    // scan primary table, create each secondary key, add to each DB
    //    
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

    unpack_entire_row = true;
    cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_ai_callback, &info);
    while (cursor_ret_val != DB_NOTFOUND) {
        if (cursor_ret_val) {
            error = cursor_ret_val;
            goto cleanup;
        }

        for (uint i = 0; i < num_of_keys; i++) {
            DBT secondary_key, row;
            bool is_unique_key = key_info[i].flags & HA_NOSAME;
            u_int32_t put_flags = DB_YESOVERWRITE;
            bool has_null = false;
            create_dbt_key_from_key(&secondary_key,&key_info[i], tmp_key_buff, tmp_record, &has_null, false);
            uint curr_index = i + curr_num_DBs;

            //
            // if unique key, check uniqueness constraint
            // but, we do not need to check it if the key has a null
            // and we do not need to check it if unique_checks is off
            //
            if (is_unique_key && !thd_test_options(thd, OPTION_RELAXED_UNIQUE_CHECKS)) {
                bool is_unique = false;
                error = is_val_unique(&is_unique, tmp_record, &key_info[i], curr_index, txn);
                if (error) { goto cleanup; }
                if (!is_unique) {
                    error = HA_ERR_FOUND_DUPP_KEY;
                    last_dup_key = i;
                    memcpy(table_arg->record[0], tmp_record, table_arg->s->rec_buff_length);
                    goto cleanup;
                }
            }

            if (key_info[i].flags & HA_CLUSTERING) {
                if ((error = pack_row(&row, (const uchar *) tmp_record, curr_index))){
                   goto cleanup;
                }
                error = share->key_file[curr_index]->put(share->key_file[curr_index], txn, &secondary_key, &row, put_flags);
            }
            else {
                bzero((void *)&row, sizeof(row));
                error = share->key_file[curr_index]->put(share->key_file[curr_index], txn, &secondary_key, &row, put_flags);
            }
            if (error) { goto cleanup; }

        }
        num_processed++; 

        if ((num_processed % 1000) == 0) {
            sprintf(status_msg, "Adding indexes: Processed %llu of about %llu rows.", num_processed, share->rows);
            thd_proc_info(thd, status_msg);
            if (thd->killed) {
                error = ER_ABORTING_CONNECTION;
                goto cleanup;
            }
        }
        cursor_ret_val = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_ai_callback, &info);
    }
    error = tmp_cursor->c_close(tmp_cursor);
    assert(error==0);
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
            error = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_do_nothing, NULL);
            if (error && error != DB_NOTFOUND) {
                goto cleanup;
            }
            num_processed++;
            if ((num_processed % 1000) == 0) {
                sprintf(status_msg, "Adding indexes: Applied %llu of %llu rows in key-%s.", num_processed, share->rows, key_info[i].name);
                thd_proc_info(thd, status_msg);
                if (thd->killed) {
                    error = ER_ABORTING_CONNECTION;
                    goto cleanup;
                }
            }
        }
        
        error = tmp_cursor->c_close(tmp_cursor);
        assert(error==0);
        tmp_cursor = NULL;
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
    if (tmp_cursor) {            
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
        tmp_cursor = NULL;
    }
    if (txn) {
        if (error) {
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
            abort_txn(txn);
        }
        else {
            commit_txn(txn,0);
        }
    }
            if (error == DB_LOCK_NOTGRANTED) {
                sql_print_error("Could not add indexes to table %s because \
another transaction has accessed the table. \
To add indexes, make sure no transactions touch the table.", share->table_name);
            }
    my_free(tmp_key_buff,MYF(MY_ALLOW_ZERO_PTR));
    my_free(tmp_prim_key_buff,MYF(MY_ALLOW_ZERO_PTR));
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
    DB_TXN* txn = NULL;

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }
    
    for (uint i = 0; i < num_of_keys; i++) {
        uint curr_index = key_num[i];
        int r = share->key_file[curr_index]->close(share->key_file[curr_index],0);
        assert(r==0);
        share->key_file[curr_index] = NULL;

        error = remove_key_name_from_status(share->status_block, table_arg->key_info[curr_index].name, txn);
        if (error) { goto cleanup; }
        
        error = delete_or_rename_dictionary(share->table_name, NULL, table_arg->key_info[curr_index].name, true, txn, true);
        if (error) { goto cleanup; }
    }
cleanup:
    if (txn) {
        if (error) {
            abort_txn(txn);
        }
        else {
            commit_txn(txn,0);
        }
    }
        if (error == DB_LOCK_NOTGRANTED) {
            sql_print_error("Could not drop indexes from table %s because \
another transaction has accessed the table. \
To drop indexes, make sure no transactions touch the table.", share->table_name);
        }
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
    if (error == ENOSPC) {
        error = HA_ERR_DISK_FULL;
    }
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
        error = share->key_file[i]->cursor(share->key_file[i], txn, &tmp_cursor, 0);
        if (error) {
            tmp_cursor = NULL;
            goto cleanup;
        }
        while (error != DB_NOTFOUND) {
            error = tmp_cursor->c_getf_next(tmp_cursor, DB_PRELOCKED, smart_dbt_do_nothing, NULL);
            if (error && error != DB_NOTFOUND) {
                goto cleanup;
            }
        }
        error = tmp_cursor->c_close(tmp_cursor);
        assert(error==0);
        tmp_cursor = NULL;
    }

    error = 0;
cleanup:
    if (tmp_cursor) {
        int r = tmp_cursor->c_close(tmp_cursor);
        assert(r==0);
        tmp_cursor = NULL;
    }
    if (do_commit) {
        commit_txn(txn, 0);
    }
    TOKUDB_DBUG_RETURN(error);
}

//
// truncate's dictionary associated with keynr index using transaction txn
// does so by deleting and then recreating the dictionary in the context
// of a transaction
//
int ha_tokudb::truncate_dictionary( uint keynr, DB_TXN* txn ) {
    int error;
    bool is_pk = (keynr == primary_key);
    
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
        error = create_main_dictionary(share->table_name, table, txn, &share->kc_info);
    }
    else {
        error = create_secondary_dictionary(
            share->table_name, 
            table, 
            &table_share->key_info[keynr], 
            txn,
            &share->kc_info,
            keynr
            );
    }
    if (error) { goto cleanup; }

cleanup:
    return error;
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
    uint curr_num_DBs = 0;
    DB_TXN* txn = NULL;

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    if (thd_sql_command(ha_thd()) != SQLCOM_TRUNCATE) {
        error = HA_ERR_WRONG_COMMAND;
        goto cleanup;
    }

    curr_num_DBs = table->s->keys + test(hidden_primary_key);
    for (uint i = 0; i < curr_num_DBs; i++) {
        error = truncate_dictionary(i, txn);
        if (error) { goto cleanup; }
    }

    // zap the row count
    if (error == 0) {
        share->rows = 0;
    }

cleanup:
    if (txn) {
        if (error) {
            abort_txn(txn);
        }
        else {
            commit_txn(txn,0);
        }
    }

            if (error == DB_LOCK_NOTGRANTED) {
                sql_print_error("Could not truncate table %s because \
another transaction has accessed the table. \
To truncate the table, make sure no transactions touch the table.", share->table_name);
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
    TOKUDB_DBUG_RETURN(error);
}


