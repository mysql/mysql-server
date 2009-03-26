#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
}
#include "hatoku_cmp.h"


inline TOKU_TYPE mysql_to_toku_type (enum_field_types mysql_type) {
    TOKU_TYPE ret_val = toku_type_unknown;
    switch (mysql_type) {
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
        ret_val = toku_type_int;
        break;
    default:
        ret_val = toku_type_unknown;
    }
    return ret_val;
}


int compare_field(
    uchar* a_buf, 
    uchar* b_buf, 
    Field* field,
    u_int32_t key_part_length, //I really hope this is temporary as I phase out the pack_cmp stuff
    u_int32_t* a_bytes_read, 
    u_int32_t* b_bytes_read
    ) {
    int ret_val = 0;
    TOKU_TYPE toku_type = mysql_to_toku_type(field->type());
    switch(toku_type) {
    case (toku_type_int):
        ret_val = cmp_toku_int(
            a_buf, 
            b_buf, 
            field->flags & UNSIGNED_FLAG, 
            field->pack_length()
            );
        *a_bytes_read = field->pack_length();
        *b_bytes_read = field->pack_length();
        goto exit;
    default:
        *a_bytes_read = field->packed_col_length(a_buf, key_part_length);
        *b_bytes_read = field->packed_col_length(b_buf, key_part_length);
        ret_val = field->pack_cmp(a_buf, b_buf, key_part_length, 0);
        goto exit;
    }
    assert(false);
exit:
    return ret_val;
}


//
// assuming MySQL in little endian, and we are storing in little endian
//
uchar* pack_toku_int (uchar* to_tokudb, uchar* from_mysql, u_int32_t num_bytes) {
    switch (num_bytes) {
    case (1):
    case (2):
    case (3):
    case (4):
    case (8):
        memcpy(to_tokudb, from_mysql, num_bytes);
    default:
        assert(false);
    }
    return to_tokudb+num_bytes;
}

//
// assuming MySQL in little endian, and we are unpacking to little endian
//
uchar* unpack_toku_int(uchar* to_mysql, uchar* from_tokudb, u_int32_t num_bytes) {
    switch (num_bytes) {
    case (1):
    case (2):
    case (3):
    case (4):
    case (8):
        memcpy(to_mysql, from_tokudb, num_bytes);
    default:
        assert(false);
    }
    return from_tokudb+num_bytes;
}

int cmp_toku_int (uchar* a_buf, uchar* b_buf, bool is_unsigned, u_int32_t num_bytes) {
    int ret_val = 0;
    //
    // case for unsigned integers
    //
    if (is_unsigned) {
        u_int32_t a_num, b_num = 0;
        u_int64_t a_big_num, b_big_num = 0;
        switch (num_bytes) {
        case (1):
            a_num = *a_buf;
            b_num = *b_buf;
        case (2):
            a_num = uint2korr(a_buf);
            b_num = uint2korr(b_buf);
        case (3):
            a_num = uint3korr(a_buf);
            b_num = uint3korr(b_buf);
            ret_val = a_num-b_num;
            goto exit;
        case (4):
            printf("a: %d, %d, %d, %d\n", a_buf[0], a_buf[1], a_buf[2], a_buf[3]);
            printf("a: %d, %d, %d, %d\n", a_buf[0], a_buf[1], a_buf[2], a_buf[3]);
            a_num = uint4korr(a_buf);
            b_num = uint4korr(b_buf);
            if (a_num < b_num) {
                ret_val = -1; goto exit;
            }
            if (a_num > b_num) {
                ret_val = 1; goto exit;
            }
            ret_val = 0;
            goto exit;
        case (8):
            a_big_num = uint8korr(a_buf);
            b_big_num = uint8korr(b_buf);
            if (a_big_num < b_big_num) {
                ret_val = -1; goto exit;
            }
            else if (a_big_num > b_big_num) {
                ret_val = 1; goto exit;
            }
            ret_val = 0;
            goto exit;
        default:
            assert(false);
        }
    }
    //
    // case for signed integers
    //
    else {
        int32_t a_num, b_num = 0;
        int64_t a_big_num, b_big_num = 0;
        switch (num_bytes) {
        case (1):
            a_num = *(signed char *)a_buf;
            b_num = *(signed char *)b_buf;
        case (2):
            a_num = sint2korr(a_buf);
            b_num = sint2korr(b_buf);
        case (3):
            a_num = sint3korr(a_buf);
            b_num = sint3korr(b_buf);
            ret_val = a_num - b_num;
            goto exit;
        case (4):
            a_num = sint4korr(a_buf);
            b_num = sint4korr(b_buf);
            if (a_num < b_num) {
                ret_val = -1; goto exit;
            }
            if (a_num > b_num) {
                ret_val = 1; goto exit;
            }
            ret_val = 0;
            goto exit;
        case (8):
            a_big_num = sint8korr(a_buf);
            b_big_num = sint8korr(b_buf);
            if (a_big_num < b_big_num) {
                ret_val = -1; goto exit;
            }
            else if (a_big_num > b_big_num) {
                ret_val = 1; goto exit;
            }
            ret_val = 0;
            goto exit;
        default:
            assert(false);
        }
    }
    //
    // if this is hit, indicates bug in writing of this function
    //
    assert(false);
exit:
    printf("ret_val %d\n", ret_val);
    return ret_val;    
}


inline int tokudb_compare_two_hidden_keys(
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

int tokudb_cmp_hidden_key(DB * file, const DBT * new_key, const DBT * saved_key) {
    return tokudb_compare_two_hidden_keys(
        new_key->data, 
        new_key->size,
        saved_key->data,
        saved_key->size
        );
}

int tokudb_compare_two_keys(
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
        cmp = compare_field(
            new_key_ptr, 
            saved_key_ptr,
            key_part->field,
            key_part->length,
            &new_key_field_length,
            &saved_key_field_length
            );
        if (cmp) {
            return cmp;
        }

        assert(new_key_length >= new_key_field_length);
        assert(saved_key_length >= saved_key_field_length);

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
int tokudb_compare_two_clustered_keys(KEY *key, KEY* primary_key, const DBT * new_key, const DBT * saved_key) {
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
        cmp = compare_field(
            new_key_ptr, 
            saved_key_ptr,
            key_part->field,
            key_part->length,
            &new_key_field_length,
            &saved_key_field_length
            );
        if (cmp) {
            return cmp;
        }

        assert(new_key_length >= new_key_field_length);
        assert(saved_key_length >= saved_key_field_length);

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
                cmp = compare_field(
                    new_key_ptr, 
                    saved_key_ptr,
                    key_part->field,
                    key_part->length,
                    &new_key_field_length,
                    &saved_key_field_length
                    );
                if (cmp) {
                    return cmp;
                }
                
                assert(new_key_length >= new_key_field_length);
                assert(saved_key_length >= saved_key_field_length);
                
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


int tokudb_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    KEY *primary_key = (KEY *) file->api_internal;
    if (key->flags & HA_CLUSTERING) {
        return tokudb_compare_two_clustered_keys(key, primary_key, keya, keyb);
    }
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, false);
}

int tokudb_cmp_primary_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->api_internal;
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, false);
}

//TODO: QQQ Only do one direction for prefix.
int tokudb_prefix_cmp_packed_key(DB *file, const DBT *keya, const DBT *keyb) {
    assert(file->app_private != 0);
    KEY *key = (KEY *) file->app_private;
    return tokudb_compare_two_keys(key, keya->data, keya->size, keyb->data, keyb->size, true);
}

