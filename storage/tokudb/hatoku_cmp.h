#ifndef _HATOKU_CMP
#define _HATOKU_CMP

#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
}


#include <db.h>

typedef struct st_col_pack_info {
    u_int32_t col_pack_val; //offset if fixed, pack_index if var
} COL_PACK_INFO;

typedef struct st_multi_col_pack_info {
    u_int32_t var_len_offset; //where the fixed length stuff ends and the offsets for var stuff begins
    u_int32_t len_of_offsets; //length of the offset bytes in a packed row
} MULTI_COL_PACK_INFO;


typedef struct st_key_and_col_info {
    MY_BITMAP key_filters[MAX_KEY+1];
    uchar* field_lengths; //stores the field lengths of fixed size fields (255 max)
    uchar* length_bytes; // stores the length of lengths of varchars and varbinaries
    u_int32_t* blob_fields; // list of indexes of blob fields
    u_int32_t num_blobs;
    MULTI_COL_PACK_INFO mcp_info[MAX_KEY+1];
    COL_PACK_INFO* cp_info[MAX_KEY+1];
    u_int32_t num_offset_bytes; //number of bytes needed to encode the offset
} KEY_AND_COL_INFO;

void get_var_field_info(
    u_int32_t* field_len, 
    u_int32_t* start_offset, 
    u_int32_t var_field_index, 
    const uchar* var_field_offset_ptr, 
    u_int32_t num_offset_bytes
    );

void get_blob_field_info(
    u_int32_t* start_offset, 
    u_int32_t len_of_offsets,
    const uchar* var_field_data_ptr, 
    u_int32_t num_offset_bytes
    );

inline u_int32_t get_blob_field_len(
    const uchar* from_tokudb, 
    u_int32_t len_bytes
    ) 
{
    u_int32_t length = 0;
    switch (len_bytes) {
    case (1):
        length = (u_int32_t)(*from_tokudb);
        break;
    case (2):
        length = uint2korr(from_tokudb);
        break;
    case (3):
        length = uint3korr(from_tokudb);
        break;
    case (4):
        length = uint4korr(from_tokudb);
        break;
    default:
        assert(false);
    }
    return length;
}


inline const uchar* unpack_toku_field_blob(
    uchar *to_mysql, 
    const uchar* from_tokudb,
    u_int32_t len_bytes,
    bool skip
    )
{
    u_int32_t length = 0;
    const uchar* data_ptr = NULL;
    if (!skip) {
        memcpy(to_mysql, from_tokudb, len_bytes);
    }
    length = get_blob_field_len(from_tokudb,len_bytes);

    data_ptr = from_tokudb + len_bytes;
    if (!skip) {
        memcpy(to_mysql + len_bytes, (uchar *)(&data_ptr), sizeof(uchar *));
    }
    return (from_tokudb + len_bytes + length);
}

inline uint get_null_offset(TABLE* table, Field* field) {
    return (uint) ((uchar*) field->null_ptr - (uchar*) table->record[0]);
}


typedef enum {
    toku_type_int = 0,
    toku_type_double,
    toku_type_float,
    toku_type_fixbinary,
    toku_type_fixstring,
    toku_type_varbinary,
    toku_type_varstring,
    toku_type_blob,
    toku_type_hpk, //for hidden primary key
    toku_type_unknown
} TOKU_TYPE;


TOKU_TYPE mysql_to_toku_type (Field* field);

uchar* pack_toku_varbinary_from_desc(
    uchar* to_tokudb, 
    const uchar* from_desc, 
    u_int32_t key_part_length, //number of bytes to use to encode the length in to_tokudb
    u_int32_t field_length //length of field
    );

uchar* pack_toku_varstring_from_desc(
    uchar* to_tokudb, 
    const uchar* from_desc, 
    u_int32_t key_part_length, //number of bytes to use to encode the length in to_tokudb
    u_int32_t field_length,
    u_int32_t charset_num//length of field
    );


uchar* pack_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    );

uchar* pack_key_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    );

uchar* unpack_toku_key_field(
    uchar* to_mysql,
    uchar* from_tokudb,
    Field* field,
    u_int32_t key_part_length
    );


//
// for storing NULL byte in keys
//
#define NULL_COL_VAL 0
#define NONNULL_COL_VAL 1

//
// for storing if rest of key is +/- infinity
//
#define COL_NEG_INF -1 
#define COL_ZERO 0 
#define COL_POS_INF 1

//
// information for hidden primary keys
//
#define TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH 8

//
// function to convert a hidden primary key into a byte stream that can be stored in DBT
//
inline void hpk_num_to_char(uchar* to, ulonglong num) {
    int8store(to, num);
}

//
// function that takes a byte stream of a hidden primary key and returns a ulonglong
//
inline ulonglong hpk_char_to_num(uchar* val) {
    return uint8korr(val);
}

int tokudb_compare_two_keys(
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size,
    const void*  row_desc,
    const u_int32_t row_desc_size,
    bool cmp_prefix
    );


int tokudb_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb);

//TODO: QQQ Only do one direction for prefix.
int tokudb_prefix_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb);

int create_toku_key_descriptor(
    uchar* buf, 
    bool is_first_hpk, 
    KEY* first_key, 
    bool is_second_hpk, 
    KEY* second_key
    );


u_int32_t create_toku_main_key_pack_descriptor (
    uchar* buf
    );

u_int32_t get_max_clustering_val_pack_desc_size(
    TABLE_SHARE* table_share
    );

u_int32_t create_toku_clustering_val_pack_descriptor (
    uchar* buf,
    uint pk_index,
    TABLE_SHARE* table_share,
    KEY_AND_COL_INFO* kc_info,
    u_int32_t keynr,
    bool is_clustering
    );

inline bool is_key_clustering(
    void* row_desc,
    u_int32_t row_desc_size
    ) 
{
    return (row_desc_size > 0);
}

u_int32_t pack_clustering_val_from_desc(
    uchar* buf,
    void* row_desc,
    u_int32_t row_desc_size,
    DBT* pk_val
    );

u_int32_t get_max_secondary_key_pack_desc_size(
    KEY_AND_COL_INFO* kc_info
    );

u_int32_t create_toku_secondary_key_pack_descriptor (
    uchar* buf,
    bool has_hpk,
    uint pk_index,
    TABLE_SHARE* table_share,
    TABLE* table,
    KEY_AND_COL_INFO* kc_info,
    KEY* key_info,
    KEY* prim_key
    );

inline bool is_key_pk(
    void* row_desc,
    u_int32_t row_desc_size
    ) 
{
    uchar* buf = (uchar *)row_desc;
    return buf[0];
}

u_int32_t max_key_size_from_desc(
    void* row_desc,
    u_int32_t row_desc_size
    );


u_int32_t pack_key_from_desc(
    uchar* buf,
    void* row_desc,
    u_int32_t row_desc_size,
    DBT* pk_key,
    DBT* pk_val
    );


#endif

