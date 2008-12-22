#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
}
#include "hatoku_cmptrace.h"


/*
    Things that are required for ALL data types:
        key_part->field->null_bit
        key_part->length
        key_part->field->packed_col_length(...)
            DEFAULT: virtual uint packed_col_length(const uchar *to, uint length)
                { return length;}
            All integer types use this.
            String types MIGHT use different one, espescially the varchars
        key_part->field->pack_cmp(...)
            DEFAULT: virtual int pack_cmp(...)
                { return cmp(a,b); }
            All integer types use the obvious one.
            Assume X byte bytestream, int =:
            ((u_int64_t)((u_int8_t)bytes[0])) << 0 | 
            ((u_int64_t)((u_int8_t)bytes[1])) << 8 | 
            ((u_int64_t)((u_int8_t)bytes[2])) << 16 | 
            ((u_int64_t)((u_int8_t)bytes[3])) << 24 | 
            ((u_int64_t)((u_int8_t)bytes[4])) << 32 | 
            ((u_int64_t)((u_int8_t)bytes[5])) << 40 | 
            ((u_int64_t)((u_int8_t)bytes[6])) << 48 | 
            ((u_int64_t)((u_int8_t)bytes[7])) << 56
            If the integer type is < 8 bytes, just skip the unneeded ones.
            Then compare the integers in the obvious way.
        Strings:
            Empty space differences at end are ignored.
            i.e. delete all empty space at end first, and then compare.
    Possible prerequisites:
        key_part->field->cmp
            NO DEFAULT
*/

typedef enum {
    TOKUTRACE_SIGNED_INTEGER   = 0,
    TOKUTRACE_UNSIGNED_INTEGER = 1,
    TOKUTRACE_CHAR = 2
} tokutrace_field_type;

typedef struct {
    tokutrace_field_type    type;
    bool                    null_bit;
    u_int32_t               length;
} tokutrace_field;

typedef struct {
    u_int16_t           version;
    u_int32_t           num_fields;
    tokutrace_field     fields[1];
} tokutrace_cmp_fun;

int tokutrace_db_get_cmp_byte_stream(DB* db, DBT* byte_stream) {
    int r      = ENOSYS;
    void* data = NULL;
    KEY* key   = NULL;
    if (byte_stream->flags != DB_DBT_MALLOC) { return EINVAL; }
    bzero((void *) byte_stream, sizeof(*byte_stream));

    u_int32_t num_fields = 0;
    if (!db->app_private) { num_fields = 1; }
    else {
        key = (KEY*)db->app_private;
        num_fields = key->key_parts;
    }
    size_t need_size = sizeof(tokutrace_cmp_fun) +
                       num_fields * sizeof(tokutrace_field);

    data = my_malloc(need_size, MYF(MY_FAE | MY_ZEROFILL | MY_WME));
    if (!data) { return ENOMEM; }

    tokutrace_cmp_fun* info = (tokutrace_cmp_fun*)data;
    info->version     = 1;
    info->num_fields  = num_fields;
    
    if (!db->app_private) {
        info->fields[0].type     = TOKUTRACE_UNSIGNED_INTEGER;
        info->fields[0].null_bit = false;
        info->fields[0].length   = 40 / 8;
        goto finish;
    }
    assert(db->app_private);
    assert(key);
    u_int32_t i;
    for (i = 0; i < num_fields; i++) {
        info->fields[i].null_bit = key->key_part[i].null_bit;
        info->fields[i].length   = key->key_part[i].length;
        enum_field_types type    = key->key_part[i].field->type();
        switch (type) {
#ifdef HAVE_LONG_LONG
            case (MYSQL_TYPE_LONGLONG):
#endif
            case (MYSQL_TYPE_LONG):
            case (MYSQL_TYPE_INT24):
            case (MYSQL_TYPE_SHORT):
            case (MYSQL_TYPE_TINY): {
                /* Integer */
                Field_num* field = static_cast<Field_num*>(key->key_part[i].field);
                if (field->unsigned_flag) {
                    info->fields[i].type = TOKUTRACE_UNSIGNED_INTEGER; }
                else {
                    info->fields[i].type = TOKUTRACE_SIGNED_INTEGER; }
                break;
            }
            default: {
                fprintf(stderr, "Cannot save cmp function for type %d.\n", type);
                r = ENOSYS;
                goto cleanup;
            }
        }
    }
finish:
    byte_stream->data = data;
    byte_stream->size = need_size;
    r = 0;
cleanup:
    if (r!=0) {
        if (data) { my_free(data, MYF(0)); }
    }
    return r;
}

