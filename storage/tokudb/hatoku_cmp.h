#ifndef _HATOKU_CMP
#define _HATOKU_CMP

#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
}


#include <db.h>




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


inline TOKU_TYPE mysql_to_toku_type (enum_field_types mysql_type);


uchar* pack_toku_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    );

uchar* pack_key_toku_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    );

uchar* unpack_toku_field(
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
#define COL_NEG_INF 0 
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

int tokudb_cmp_dbt_data(DB *file, const DBT *keya, const DBT *keyb);

//TODO: QQQ Only do one direction for prefix.
int tokudb_prefix_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb);

int create_toku_key_descriptor(
    uchar* buf, 
    bool is_first_hpk, 
    bool is_clustering_key, 
    KEY* first_key, 
    bool is_second_hpk, 
    KEY* second_key
    );
#endif

