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
    toku_type_unknown
} TOKU_TYPE;


inline TOKU_TYPE mysql_to_toku_type (enum_field_types mysql_type);
int compare_field(uchar* a_buf, Field* a_field, uchar* b_buf, Field* b_field);


uchar* pack_toku_int (uchar* to_tokudb, uchar* from_mysql, u_int32_t num_bytes);
uchar* unpack_toku_int(uchar* to_mysql, uchar* from_tokudb, u_int32_t num_bytes);
int cmp_toku_int (uchar* a, uchar* b, bool is_unsigned, u_int32_t num_bytes);


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



inline int tokudb_compare_two_hidden_keys(
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size
    );

int tokudb_compare_two_keys(
    KEY *key, 
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size,
    bool cmp_prefix
    );

int tokudb_cmp_hidden_key(
    DB* file, 
    const DBT* new_key, 
    const DBT* saved_key
    );


int tokudb_compare_two_clustered_keys(
    KEY *key, 
    KEY* primary_key, 
    const DBT * new_key, 
    const DBT * saved_key
    );


int tokudb_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb);

int tokudb_cmp_primary_key(DB *file, const DBT *keya, const DBT *keyb);
    
//TODO: QQQ Only do one direction for prefix.
int tokudb_prefix_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb);

#endif

