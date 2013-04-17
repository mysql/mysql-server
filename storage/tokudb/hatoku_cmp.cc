#include "mysql_priv.h"

extern "C" {
#include "stdint.h"
}
#include "hatoku_cmp.h"


#ifdef WORDS_BIGENDIAN
#error "WORDS_BIGENDIAN not supported"
#endif

TOKU_TYPE mysql_to_toku_type (Field* field) {
    TOKU_TYPE ret_val = toku_type_unknown;
    enum_field_types mysql_type = field->real_type();
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
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
        ret_val = toku_type_int;
        goto exit;
    case MYSQL_TYPE_DOUBLE:
        ret_val = toku_type_double;
        goto exit;
    case MYSQL_TYPE_FLOAT:
        ret_val = toku_type_float;
        goto exit;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_BIT:
        ret_val = toku_type_fixbinary;
        goto exit;
    case MYSQL_TYPE_STRING:
        if (field->binary()) {
            ret_val = toku_type_fixbinary;
        }
        else {
            ret_val = toku_type_fixstring;
        }
        goto exit;
    case MYSQL_TYPE_VARCHAR:
        if (field->binary()) {
            ret_val = toku_type_varbinary;
        }
        else {
            ret_val = toku_type_varstring;
        }
        goto exit;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
        ret_val = toku_type_blob;
        goto exit;
    //
    // I believe these are old types that are no longer
    // in any 5.1 tables, so tokudb does not need
    // to worry about them
    // Putting in this assert in case I am wrong.
    // Do not support geometry yet.
    //
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_VAR_STRING:
    default:
        assert(false);
    }
exit:
    return ret_val;
}


//
// used to read the length of a variable sized field in a tokudb key (buf).
//
inline u_int32_t get_length_from_var_tokudata (uchar* buf, u_int32_t length_bytes) {
    u_int32_t length = (u_int32_t)(buf[0]);
    if (length_bytes == 2) {
        u_int32_t rest_of_length = (u_int32_t)buf[1];
        length += rest_of_length<<8;
    }
    return length;
}

//
// used to deduce the number of bytes used to store the length of a varstring/varbinary
// in a key field stored in tokudb
//
inline u_int32_t get_length_bytes_from_max(u_int32_t max_num_bytes) {
    return (max_num_bytes > 255) ? 2 : 1;
}



//
// assuming MySQL in little endian, and we are storing in little endian
//
inline uchar* pack_toku_int (uchar* to_tokudb, uchar* from_mysql, u_int32_t num_bytes) {
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
        assert(false);
    }
    return to_tokudb+num_bytes;
}

//
// assuming MySQL in little endian, and we are unpacking to little endian
//
inline uchar* unpack_toku_int(uchar* to_mysql, uchar* from_tokudb, u_int32_t num_bytes) {
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
        assert(false);
    }
    return from_tokudb+num_bytes;
}

inline int cmp_toku_int (uchar* a_buf, uchar* b_buf, bool is_unsigned, u_int32_t num_bytes) {
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
            ret_val = a_num-b_num;
            goto exit;
        case (2):
            a_num = uint2korr(a_buf);
            b_num = uint2korr(b_buf);
            ret_val = a_num-b_num;
            goto exit;
        case (3):
            a_num = uint3korr(a_buf);
            b_num = uint3korr(b_buf);
            ret_val = a_num-b_num;
            goto exit;
        case (4):
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
            ret_val = a_num-b_num;
            goto exit;
        case (2):
            a_num = sint2korr(a_buf);
            b_num = sint2korr(b_buf);
            ret_val = a_num-b_num;
            goto exit;
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
    return ret_val;    
}

inline uchar* pack_toku_double (uchar* to_tokudb, uchar* from_mysql) {
    memcpy(to_tokudb, from_mysql, sizeof(double));
    return to_tokudb + sizeof(double);
}


inline uchar* unpack_toku_double(uchar* to_mysql, uchar* from_tokudb) {
    memcpy(to_mysql, from_tokudb, sizeof(double));
    return from_tokudb + sizeof(double);
}

inline int cmp_toku_double(uchar* a_buf, uchar* b_buf) {
    int ret_val;
    double a_num;
    double b_num;
    doubleget(a_num, a_buf);
    doubleget(b_num, b_buf);
    if (a_num < b_num) {
        ret_val = -1;
        goto exit;
    }
    else if (a_num > b_num) {
        ret_val = 1;
        goto exit;
    }
    ret_val = 0;
exit:
    return ret_val;
}


inline uchar* pack_toku_float (uchar* to_tokudb, uchar* from_mysql) {
    memcpy(to_tokudb, from_mysql, sizeof(float));
    return to_tokudb + sizeof(float);
}


inline uchar* unpack_toku_float(uchar* to_mysql, uchar* from_tokudb) {
    memcpy(to_mysql, from_tokudb, sizeof(float));
    return from_tokudb + sizeof(float);
}

inline int cmp_toku_float(uchar* a_buf, uchar* b_buf) {
    int ret_val;
    float a_num;
    float b_num;
    //
    // This is the way Field_float::cmp gets the floats from the buffers
    //
    memcpy(&a_num, a_buf, sizeof(float));
    memcpy(&b_num, b_buf, sizeof(float));
    if (a_num < b_num) {
        ret_val = -1;
        goto exit;
    }
    else if (a_num > b_num) {
        ret_val = 1;
        goto exit;
    }
    ret_val = 0;
exit:
    return ret_val;
}


inline uchar* pack_toku_binary(uchar* to_tokudb, uchar* from_mysql, u_int32_t num_bytes) {
    memcpy(to_tokudb, from_mysql, num_bytes);
    return to_tokudb + num_bytes;
}

inline uchar* unpack_toku_binary(uchar* to_mysql, uchar* from_tokudb, u_int32_t num_bytes) {
    memcpy(to_mysql, from_tokudb, num_bytes);
    return from_tokudb + num_bytes;
}


inline int cmp_toku_binary(
    uchar* a_buf, 
    u_int32_t a_num_bytes, 
    uchar* b_buf, 
    u_int32_t b_num_bytes
    ) 
{
    int ret_val = 0;
    u_int32_t num_bytes_to_cmp = (a_num_bytes < b_num_bytes) ? a_num_bytes : b_num_bytes;
    ret_val = memcmp(a_buf, b_buf, num_bytes_to_cmp);
    if ((ret_val != 0) || (a_num_bytes == b_num_bytes)) {
        goto exit;
    }
    if (a_num_bytes < b_num_bytes) {
        ret_val = -1;
        goto exit;
    }
    else {
        ret_val = 1;
        goto exit;
    }
exit:
    return ret_val;
}

inline uchar* pack_toku_varbinary(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    u_int32_t length_bytes_in_tokudb, //number of bytes to use to encode the length in to_tokudb
    u_int32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    u_int32_t max_num_bytes
    ) 
{
    u_int32_t length = 0;
    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
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
    }
    set_if_smaller(length,max_num_bytes);

    //
    // copy the length bytes, assuming both are in little endian
    //
    to_tokudb[0] = (uchar)length & 255;
    if (length_bytes_in_tokudb > 1) {
        to_tokudb[1] = (uchar) (length >> 8);
    }
    //
    // copy the string
    //
    memcpy(to_tokudb + length_bytes_in_tokudb, from_mysql + length_bytes_in_mysql, length);
    return to_tokudb + length + length_bytes_in_tokudb;
}

inline uchar* unpack_toku_varbinary(
    uchar* to_mysql, 
    uchar* from_tokudb, 
    u_int32_t length_bytes_in_tokudb, // number of bytes used to encode length in from_tokudb
    u_int32_t length_bytes_in_mysql // number of bytes used to encode length in to_mysql
    ) 
{
    u_int32_t length = get_length_from_var_tokudata(from_tokudb, length_bytes_in_tokudb);

    //
    // copy the length into the mysql buffer
    //
    switch (length_bytes_in_mysql) {
    case (0):
        break;
    case (1):
        *to_mysql = (uchar) length;
        break;
    case (2):
        int2store(to_mysql, length);
        break;
    case (3):
        int3store(to_mysql, length);
        break;
    case (4):
        int4store(to_mysql, length);
        break;
    default:
        assert(false);
    }
    //
    // copy the binary data
    //
    memcpy(to_mysql + length_bytes_in_mysql, from_tokudb + length_bytes_in_tokudb, length);
    return from_tokudb + length_bytes_in_tokudb+ length;
}

inline int cmp_toku_varbinary(
    uchar* a_buf, 
    uchar* b_buf, 
    u_int32_t length_bytes, //number of bytes used to encode length in a_buf and b_buf
    u_int32_t* a_bytes_read, 
    u_int32_t* b_bytes_read
    ) 
{
    int ret_val = 0;
    u_int32_t a_len = get_length_from_var_tokudata(a_buf, length_bytes);
    u_int32_t b_len = get_length_from_var_tokudata(b_buf, length_bytes);
    ret_val = cmp_toku_binary(
        a_buf + length_bytes,
        a_len,
        b_buf + length_bytes,
        b_len
        );
    *a_bytes_read = a_len + length_bytes;
    *b_bytes_read = b_len + length_bytes;
    return ret_val;
}

inline uchar* pack_toku_blob(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    u_int32_t length_bytes_in_tokudb, //number of bytes to use to encode the length in to_tokudb
    u_int32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    u_int32_t max_num_bytes,
    CHARSET_INFO* charset
    ) 
{
    u_int32_t length = 0;
    u_int32_t local_char_length = 0;
    uchar* blob_buf = NULL;

    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
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
    }
    set_if_smaller(length,max_num_bytes);

    memcpy(&blob_buf,from_mysql+length_bytes_in_mysql,sizeof(uchar *));

    local_char_length= ((charset->mbmaxlen > 1) ?
                       max_num_bytes/charset->mbmaxlen : max_num_bytes);
    if (length > local_char_length)
    {
      local_char_length= my_charpos(
        charset, 
        blob_buf, 
        blob_buf+length,
        local_char_length
        );
      set_if_smaller(length, local_char_length);
    }


    //
    // copy the length bytes, assuming both are in little endian
    //
    to_tokudb[0] = (uchar)length & 255;
    if (length_bytes_in_tokudb > 1) {
        to_tokudb[1] = (uchar) (length >> 8);
    }
    //
    // copy the string
    //
    memcpy(to_tokudb + length_bytes_in_tokudb, blob_buf, length);
    return to_tokudb + length + length_bytes_in_tokudb;
}


inline uchar* unpack_toku_blob(
    uchar* to_mysql, 
    uchar* from_tokudb, 
    u_int32_t length_bytes_in_tokudb, // number of bytes used to encode length in from_tokudb
    u_int32_t length_bytes_in_mysql // number of bytes used to encode length in to_mysql
    ) 
{
    u_int32_t length = get_length_from_var_tokudata(from_tokudb, length_bytes_in_tokudb);
    uchar* blob_pos = NULL;
    //
    // copy the length into the mysql buffer
    //
    switch (length_bytes_in_mysql) {
    case (0):
        break;
    case (1):
        *to_mysql = (uchar) length;
        break;
    case (2):
        int2store(to_mysql, length);
        break;
    case (3):
        int3store(to_mysql, length);
        break;
    case (4):
        int4store(to_mysql, length);
        break;
    default:
        assert(false);
    }
    //
    // copy the binary data
    //
    blob_pos = from_tokudb + length_bytes_in_tokudb;
    memcpy(to_mysql + length_bytes_in_mysql, &blob_pos, sizeof(uchar *));
    return from_tokudb + length_bytes_in_tokudb+ length;
}


inline uchar* pack_toku_varstring(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    u_int32_t length_bytes_in_tokudb, //number of bytes to use to encode the length in to_tokudb
    u_int32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    u_int32_t max_num_bytes,
    CHARSET_INFO* charset
    ) 
{
    u_int32_t length = 0;
    u_int32_t local_char_length = 0;

    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
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
    }
    set_if_smaller(length,max_num_bytes);

    local_char_length= ((charset->mbmaxlen > 1) ?
                       max_num_bytes/charset->mbmaxlen : max_num_bytes);
    if (length > local_char_length)
    {
      local_char_length= my_charpos(
        charset, 
        from_mysql+length_bytes_in_mysql, 
        from_mysql+length_bytes_in_mysql+length,
        local_char_length
        );
      set_if_smaller(length, local_char_length);
    }


    //
    // copy the length bytes, assuming both are in little endian
    //
    to_tokudb[0] = (uchar)length & 255;
    if (length_bytes_in_tokudb > 1) {
        to_tokudb[1] = (uchar) (length >> 8);
    }
    //
    // copy the string
    //
    memcpy(to_tokudb + length_bytes_in_tokudb, from_mysql + length_bytes_in_mysql, length);
    return to_tokudb + length + length_bytes_in_tokudb;
}

inline int cmp_toku_string(
    uchar* a_buf,
    u_int32_t a_num_bytes,
    uchar* b_buf, 
    u_int32_t b_num_bytes,
    u_int32_t charset_number
    ) 
{
    int ret_val = 0;
    CHARSET_INFO* charset = NULL;

    //
    // patternmatched off of InnoDB, due to MySQL bug 42649
    //
    if (charset_number == default_charset_info->number) {
        charset = default_charset_info;
    }
    else if (charset_number == my_charset_latin1.number) {
        charset = &my_charset_latin1;
    }
    else {
        charset = get_charset(charset_number, MYF(MY_WME));
    } 

    ret_val = charset->coll->strnncollsp(
        charset,
        a_buf, 
        a_num_bytes,
        b_buf, 
        b_num_bytes, 
        0
        );
    return ret_val;
}

inline int cmp_toku_varstring(
    uchar* a_buf, 
    uchar* b_buf, 
    u_int32_t length_bytes, //number of bytes used to encode length in a_buf and b_buf
    u_int32_t charset_num,
    u_int32_t* a_bytes_read, 
    u_int32_t* b_bytes_read
    ) 
{
    int ret_val = 0;
    u_int32_t a_len = get_length_from_var_tokudata(a_buf, length_bytes);
    u_int32_t b_len = get_length_from_var_tokudata(b_buf, length_bytes);
    ret_val = cmp_toku_string(
        a_buf + length_bytes,
        a_len,
        b_buf + length_bytes,
        b_len,
        charset_num
        );
    *a_bytes_read = a_len + length_bytes;
    *b_bytes_read = b_len + length_bytes;
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

//
// Returns number of bytes used for a given TOKU_TYPE
// in a key descriptor. The number of bytes returned
// here MUST match the number of bytes used for the encoding
// in create_toku_key_descriptor_for_key
// Parameters:
//      [in]    row_desc - buffer that contains portion of descriptor
//              created in create_toku_key_descriptor_for_key. The first
//              byte points to the TOKU_TYPE.
//
u_int32_t skip_field_in_descriptor(uchar* row_desc) {
    uchar* row_desc_pos = row_desc;
    TOKU_TYPE toku_type = (TOKU_TYPE)row_desc_pos[0];
    row_desc_pos++;
    
    switch (toku_type) {
    case (toku_type_hpk):
    case (toku_type_double):
    case (toku_type_float):
        break;
    case (toku_type_int):
        row_desc_pos += 2;
        break;
    case (toku_type_fixbinary):
    case (toku_type_varbinary):
        row_desc_pos++;
        break;
    case (toku_type_fixstring):
    case (toku_type_varstring):
    case (toku_type_blob):
        row_desc_pos++;
        row_desc_pos += sizeof(u_int32_t);
        break;
    default:
        assert(false);
        break;
    }
    return (u_int32_t)(row_desc_pos - row_desc);
}

//
// outputs a descriptor for key into buf. Returns number of bytes used in buf
// to store the descriptor. Number of bytes used MUST match number of bytes
// we would skip in skip_field_in_descriptor
//
int create_toku_key_descriptor_for_key(KEY* key, uchar* buf) {
    uchar* pos = buf;
    u_int32_t num_bytes_in_field = 0;
    u_int32_t charset_num = 0;
    for (uint i = 0; i < key->key_parts; i++){
        Field* field = key->key_part[i].field;
        //
        // The first byte states if there is a null byte
        // 0 means no null byte, non-zer means there
        // is one
        //
        *pos = field->null_bit;
        pos++;

        //
        // The second byte for each field is the type
        //
        TOKU_TYPE type = mysql_to_toku_type(field);
        assert (type < 256);
        *pos = (uchar)(type & 255);
        pos++;

        //
        // based on the type, extra data follows afterwards
        //
        switch (type) {
        //
        // two bytes follow for ints, first one states how many
        // bytes the int is (1 , 2, 3, 4 or 8)
        // next one states if it is signed or not
        //
        case (toku_type_int):
            num_bytes_in_field = field->pack_length();
            assert (num_bytes_in_field < 256);
            *pos = (uchar)(num_bytes_in_field & 255);
            pos++;
            *pos = (field->flags & UNSIGNED_FLAG) ? 1 : 0;
            pos++;
            break;
        //
        // nothing follows floats and doubles
        //
        case (toku_type_double):
        case (toku_type_float):
            break;
        //
        // one byte follow stating the length of the field
        //
        case (toku_type_fixbinary):
            num_bytes_in_field = field->pack_length();
            set_if_smaller(num_bytes_in_field, key->key_part[i].length);
            assert(num_bytes_in_field < 256);
            pos[0] = (uchar)(num_bytes_in_field & 255);
            pos++;
            break;
        //
        // one byte follows: the number of bytes used to encode the length
        //
        case (toku_type_varbinary):
            *pos = (uchar)(get_length_bytes_from_max(key->key_part[i].length) & 255);
            pos++;
            break;
        //
        // five bytes follow: one for the number of bytes to encode the length,
        //                           four for the charset number 
        //
        case (toku_type_fixstring):
        case (toku_type_varstring):
        case (toku_type_blob):
            *pos = (uchar)(get_length_bytes_from_max(key->key_part[i].length) & 255);
            pos++;
            charset_num = field->charset()->number;
            pos[0] = (uchar)(charset_num & 255);
            pos[1] = (uchar)((charset_num >> 8) & 255);
            pos[2] = (uchar)((charset_num >> 16) & 255);
            pos[3] = (uchar)((charset_num >> 24) & 255);
            pos += 4;
            break;
        default:
            assert(false);
            
        }
    }
    return pos - buf;
}


//
// Creates a descriptor for a DB. That contains all information necessary 
// to do both key comparisons and data comparisons (for dup-sort databases). 
//
// There are two types of descriptors we care about:
// 1) Primary key, (in a no-dup database)
// 2) secondary keys, which are a secondary key followed by a primary key,
//      but in a no-dup database.
//
// I realize this may be confusing, but here is how it works.
// All DB's have a key compare.
// The format of the descriptor must be able to handle both.
//
// The first four bytes store an offset into the descriptor to the second piece
// used for data comparisons. So, if in the future we want to append something
// to the descriptor, we can.
// 
//
int create_toku_key_descriptor(
    uchar* buf, 
    bool is_first_hpk, 
    KEY* first_key, 
    bool is_second_hpk, 
    KEY* second_key
    ) 
{
    //
    // The first four bytes always contain the offset of where the first key
    // ends. 
    //
    uchar* pos = buf + 4;
    u_int32_t num_bytes = 0;
    u_int32_t offset = 0;


    if (is_first_hpk) {
        pos[0] = 0; //say there is NO infinity byte
        pos[1] = 0; //field cannot be NULL, stating it
        pos[2] = toku_type_hpk;
        pos += 3;
    }
    else {
        //
        // first key is NOT a hidden primary key, so we now pack first_key
        //
        pos[0] = 1; //say there is an infinity byte
        pos++;
        num_bytes = create_toku_key_descriptor_for_key(first_key, pos);
        pos += num_bytes;
    }

    //
    // if we do not have a second key, we can jump to exit right now
    // we do not have a second key if it is not a hidden primary key
    // and if second_key is NULL
    //
    if (is_first_hpk || (!is_second_hpk && (second_key == NULL)) ) {
        goto exit;
    }

    //
    // if we have a second key, and it is an hpk, we need to pack it, and
    // write in the offset to this position in the first four bytes
    //
    if (is_second_hpk) {
        pos[0] = 0; //field cannot be NULL, stating it
        pos[1] = toku_type_hpk;
        pos += 2;
    }
    else {
        //
        // second key is NOT a hidden primary key, so we now pack second_key
        //
        num_bytes = create_toku_key_descriptor_for_key(second_key, pos);
        pos += num_bytes;
    }
    
    
exit:
    offset = pos - buf;
    buf[0] = (uchar)(offset & 255);
    buf[1] = (uchar)((offset >> 8) & 255);
    buf[2] = (uchar)((offset >> 16) & 255);
    buf[3] = (uchar)((offset >> 24) & 255);

    return pos - buf;
}

    
inline int compare_toku_field(
    uchar* a_buf, 
    uchar* b_buf, 
    uchar* row_desc,
    u_int32_t* a_bytes_read, 
    u_int32_t* b_bytes_read,
    u_int32_t* row_desc_bytes_read
    )
{
    int ret_val = 0;
    uchar* row_desc_pos = row_desc;
    u_int32_t num_bytes = 0;
    u_int32_t length_bytes = 0;
    u_int32_t charset_num = 0;
    bool is_unsigned = false;

    TOKU_TYPE toku_type = (TOKU_TYPE)row_desc_pos[0];
    row_desc_pos++;
    
    switch (toku_type) {
    case (toku_type_hpk):
        ret_val = tokudb_compare_two_hidden_keys(
            a_buf, 
            TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH,
            b_buf,
            TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH
            );
        *a_bytes_read = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        *b_bytes_read = TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
        break;
    case (toku_type_int):
        num_bytes = row_desc_pos[0];
        is_unsigned = row_desc_pos[1];
        ret_val = cmp_toku_int(
            a_buf,
            b_buf,
            is_unsigned,
            num_bytes
            );
        *a_bytes_read = num_bytes;
        *b_bytes_read = num_bytes;
        row_desc_pos += 2;
        break;
    case (toku_type_double):
        ret_val = cmp_toku_double(a_buf, b_buf);
        *a_bytes_read = sizeof(double);
        *b_bytes_read = sizeof(double);
        break;
    case (toku_type_float):
        ret_val = cmp_toku_float(a_buf, b_buf);
        *a_bytes_read = sizeof(float);
        *b_bytes_read = sizeof(float);
        break;
    case (toku_type_fixbinary):
        num_bytes = row_desc_pos[0];
        ret_val = cmp_toku_binary(a_buf, num_bytes, b_buf,num_bytes);
        *a_bytes_read = num_bytes;
        *b_bytes_read = num_bytes;
        row_desc_pos++;
        break;
    case (toku_type_varbinary):
        length_bytes = row_desc_pos[0];
        ret_val = cmp_toku_varbinary(
            a_buf,
            b_buf,
            length_bytes,
            a_bytes_read,
            b_bytes_read
            );
        row_desc_pos++;
        break;
    case (toku_type_fixstring):
    case (toku_type_varstring):
    case (toku_type_blob):
        length_bytes = row_desc_pos[0];
        row_desc_pos++;
        //
        // not sure we want to read charset_num like this
        //
        charset_num = *(u_int32_t *)row_desc_pos;
        row_desc_pos += sizeof(u_int32_t);
        ret_val = cmp_toku_varstring(
            a_buf,
            b_buf,
            length_bytes,
            charset_num,
            a_bytes_read,
            b_bytes_read
            );
        break;
    default:
        assert(false);
        break;
    }
    
    *row_desc_bytes_read = row_desc_pos - row_desc;
    return ret_val;
}

//
// packs a field from a  MySQL buffer into a tokudb buffer.
// Used for inserts/updates
//
uchar* pack_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    )
{
    uchar* new_pos = NULL;
    u_int32_t num_bytes = 0;
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    switch(toku_type) {
    case (toku_type_int):
        assert(key_part_length == field->pack_length());
        new_pos = pack_toku_int(
            to_tokudb, 
            from_mysql,
            field->pack_length()
            );
        goto exit; 
    case (toku_type_double):
        assert(field->pack_length() == sizeof(double));
        assert(key_part_length == sizeof(double));
        new_pos = pack_toku_double(to_tokudb, from_mysql);
        goto exit;
    case (toku_type_float):
        assert(field->pack_length() == sizeof(float));
        assert(key_part_length == sizeof(float));
        new_pos = pack_toku_float(to_tokudb, from_mysql);
        goto exit;
    case (toku_type_fixbinary):
        num_bytes = field->pack_length();
        set_if_smaller(num_bytes, key_part_length);
        new_pos = pack_toku_binary(
            to_tokudb,
            from_mysql,
            num_bytes
            );
        goto exit;
    case (toku_type_fixstring):
        num_bytes = field->pack_length();
        set_if_smaller(num_bytes, key_part_length);
        new_pos = pack_toku_varstring(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            0,
            num_bytes,
            field->charset()
            );
        goto exit;
    case (toku_type_varbinary):
        new_pos = pack_toku_varbinary(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            ((Field_varstring *)field)->length_bytes,
            key_part_length
            );
        goto exit;
    case (toku_type_varstring):
        new_pos = pack_toku_varstring(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            ((Field_varstring *)field)->length_bytes,
            key_part_length,
            field->charset()
            );
        goto exit;
    case (toku_type_blob):
        new_pos = pack_toku_blob(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            ((Field_blob *)field)->row_pack_length(), //only calling this because packlength is returned
            key_part_length,
            field->charset()
            );
        goto exit;
    default:
        assert(false);
    }
    assert(false);
exit:
    return new_pos;
}

//
// packs a field from a  MySQL buffer into a tokudb buffer.
// Used for queries. The only difference between this function
// and pack_toku_key_field is that all variable sized columns
// use 2 bytes to encode the length, regardless of the field
// So varchar(4) will still use 2 bytes to encode the field
//
uchar* pack_key_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    u_int32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    )
{
    uchar* new_pos = NULL;
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    switch(toku_type) {
    case (toku_type_int):
    case (toku_type_double):
    case (toku_type_float):
    case (toku_type_fixbinary):
    case (toku_type_fixstring):
        new_pos = pack_toku_key_field(to_tokudb, from_mysql, field, key_part_length);
        goto exit;
    case (toku_type_varbinary):
        new_pos = pack_toku_varbinary(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            2, // for some idiotic reason, 2 bytes are always used here, regardless of length of field
            key_part_length
            );
        goto exit;
    case (toku_type_varstring):
    case (toku_type_blob):
        new_pos = pack_toku_varstring(
            to_tokudb,
            from_mysql,
            get_length_bytes_from_max(key_part_length),
            2, // for some idiotic reason, 2 bytes are always used here, regardless of length of field
            key_part_length,
            field->charset()
            );
        goto exit;
    default:
        assert(false);
    }

    assert(false);
exit:
    return new_pos;
}


uchar* unpack_toku_key_field(
    uchar* to_mysql,
    uchar* from_tokudb,
    Field* field,
    u_int32_t key_part_length
    )
{
    uchar* new_pos = NULL;
    u_int32_t num_bytes = 0;
    u_int32_t num_bytes_copied;
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    switch(toku_type) {
    case (toku_type_int):
        assert(key_part_length == field->pack_length());
        new_pos = unpack_toku_int(
            to_mysql,
            from_tokudb,
            field->pack_length()
            );
        goto exit;    
    case (toku_type_double):
        assert(field->pack_length() == sizeof(double));
        assert(key_part_length == sizeof(double));
        new_pos = unpack_toku_double(to_mysql, from_tokudb);
        goto exit;
    case (toku_type_float):
        assert(field->pack_length() == sizeof(float));
        assert(key_part_length == sizeof(float));
        new_pos = unpack_toku_float(to_mysql, from_tokudb);
        goto exit;
    case (toku_type_fixbinary):
        num_bytes = field->pack_length();
        set_if_smaller(num_bytes, key_part_length);
        new_pos = unpack_toku_binary(
            to_mysql,
            from_tokudb,
            num_bytes
            );
        goto exit;
    case (toku_type_fixstring):
        num_bytes = field->pack_length();
        new_pos = unpack_toku_varbinary(
            to_mysql,
            from_tokudb,
            get_length_bytes_from_max(key_part_length),
            0
            );
        num_bytes_copied = new_pos - (from_tokudb + get_length_bytes_from_max(key_part_length));
        assert(num_bytes_copied <= num_bytes);
        bfill(to_mysql+num_bytes_copied, num_bytes - num_bytes_copied, field->charset()->pad_char);
        goto exit;
    case (toku_type_varbinary):
    case (toku_type_varstring):
        new_pos = unpack_toku_varbinary(
            to_mysql,
            from_tokudb,
            get_length_bytes_from_max(key_part_length),
            ((Field_varstring *)field)->length_bytes
            );
        goto exit;
    case (toku_type_blob):
        new_pos = unpack_toku_blob(
            to_mysql,
            from_tokudb,
            get_length_bytes_from_max(key_part_length),
            ((Field_blob *)field)->row_pack_length() //only calling this because packlength is returned
            );
        goto exit;
    default:
        assert(false);
    }
    assert(false);
exit:
    return new_pos;
}


int tokudb_compare_two_keys(
    const void* new_key_data, 
    const u_int32_t new_key_size, 
    const void*  saved_key_data,
    const u_int32_t saved_key_size,
    const void*  row_desc,
    const u_int32_t row_desc_size,
    bool cmp_prefix
    )
{
    int ret_val = 0;
    int8_t new_key_inf_val = COL_NEG_INF;
    int8_t saved_key_inf_val = COL_NEG_INF;
    
    uchar* row_desc_ptr = (uchar *)row_desc;
    uchar *new_key_ptr = (uchar *)new_key_data;
    uchar *saved_key_ptr = (uchar *)saved_key_data;

    u_int32_t new_key_bytes_left = new_key_size;
    u_int32_t saved_key_bytes_left = saved_key_size;

    //
    // if the keys have an infinity byte, set it
    //
    if (row_desc_ptr[0]) {
        new_key_inf_val = (int8_t)new_key_ptr[0];
        saved_key_inf_val = (int8_t)saved_key_ptr[0];
        new_key_ptr++;
        saved_key_ptr++;
    }
    row_desc_ptr++;

    while ( (u_int32_t)(new_key_ptr - (uchar *)new_key_data) < new_key_size &&
            (u_int32_t)(saved_key_ptr - (uchar *)saved_key_data) < saved_key_size &&
            (u_int32_t)(row_desc_ptr - (uchar *)row_desc) < row_desc_size
            )
    {
        u_int32_t new_key_field_length;
        u_int32_t saved_key_field_length;
        u_int32_t row_desc_field_length;
        //
        // if there is a null byte at this point in the key
        //
        if (row_desc_ptr[0]) {
            //
            // compare null bytes. If different, return
            //
            if (new_key_ptr[0] != saved_key_ptr[0]) {
                ret_val = ((int) *new_key_ptr - (int) *saved_key_ptr);
                goto exit;
            }
            saved_key_ptr++;
            //
            // in case we just read the fact that new_key_ptr and saved_key_ptr
            // have NULL as their next field
            //
            if (!*new_key_ptr++) {
                //
                // skip row_desc_ptr[0] read in if clause
                //
                row_desc_ptr++;
                //
                // skip data that describes rest of field
                //
                row_desc_ptr += skip_field_in_descriptor(row_desc_ptr);
                continue; 
            }         
        }
        row_desc_ptr++;

        ret_val = compare_toku_field(
            new_key_ptr, 
            saved_key_ptr, 
            row_desc_ptr,
            &new_key_field_length, 
            &saved_key_field_length,
            &row_desc_field_length
            );
        new_key_ptr += new_key_field_length;
        saved_key_ptr += saved_key_field_length;
        row_desc_ptr += row_desc_field_length;
        if (ret_val) {
            goto exit;
        }

        assert((u_int32_t)(new_key_ptr - (uchar *)new_key_data) <= new_key_size);
        assert((u_int32_t)(saved_key_ptr - (uchar *)saved_key_data) <= saved_key_size);
        assert((u_int32_t)(row_desc_ptr - (uchar *)row_desc) <= row_desc_size);
    }
    new_key_bytes_left = new_key_size - ((u_int32_t)(new_key_ptr - (uchar *)new_key_data));
    saved_key_bytes_left = saved_key_size - ((u_int32_t)(saved_key_ptr - (uchar *)saved_key_data));
    if (cmp_prefix) {
        ret_val = 0;
    }
    //
    // in this case, read both keys to completion, now read infinity byte
    //
    else if (new_key_bytes_left== 0 && saved_key_bytes_left== 0) {
        ret_val = new_key_inf_val - saved_key_inf_val;
    }
    //
    // at this point, one SHOULD be 0
    //
    else if (new_key_bytes_left == 0 && saved_key_bytes_left > 0) {
        ret_val = (new_key_inf_val == COL_POS_INF ) ? 1 : -1; 
    }
    else if (new_key_bytes_left > 0 && saved_key_bytes_left == 0) {
        ret_val = (saved_key_inf_val == COL_POS_INF ) ? -1 : 1; 
    }
    //
    // this should never happen, perhaps we should assert(false)
    //
    else {
        assert(false);
        ret_val = new_key_bytes_left - saved_key_bytes_left;
    }
exit:
    return ret_val;
}

int tokudb_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb) {
    int cmp;
    if (file->descriptor->size == 0) {
        int num_bytes_cmp = keya->size < keyb->size ? 
            keya->size : keyb->size;
        cmp = memcmp(keya->data,keyb->data,num_bytes_cmp);
        if (cmp == 0 && (keya->size != keyb->size)) {
            cmp = keya->size < keyb->size ? 1 : -1;
        }
    }
    else {
        cmp = tokudb_compare_two_keys(
            keya->data, 
            keya->size, 
            keyb->data,
            keyb->size,
            (uchar *)file->descriptor->data + 4,
            (*(u_int32_t *)file->descriptor->data) - 4,
            false
            );
    }
    return cmp;
}

int tokudb_cmp_dbt_data(DB *file, const DBT *keya, const DBT *keyb) {
    int row_desc_offset = *(u_int32_t *)file->descriptor->data;
    int cmp;
    //
    // for no_dup tables, file->descriptor->size == row_desc_offset
    // so just use a default comparison function
    //
    if ( (file->descriptor->size == 0) || (file->descriptor->size - row_desc_offset == 0) ) {
        int num_bytes_cmp = keya->size < keyb->size ? 
            keya->size : keyb->size;
        cmp = memcmp(keya->data,keyb->data,num_bytes_cmp);
        if (cmp == 0 && (keya->size != keyb->size)) {
            cmp = keya->size < keyb->size ? 1 : -1;
        }
    }
    else {
        cmp = tokudb_compare_two_keys(
            keya->data, 
            keya->size, 
            keyb->data,
            keyb->size,
            (uchar *)file->descriptor->data + row_desc_offset,
            file->descriptor->size - row_desc_offset,
            false
            );
    }
    return cmp;
}

//TODO: QQQ Only do one direction for prefix.
int tokudb_prefix_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb) {
    int cmp = tokudb_compare_two_keys(
        keya->data, 
        keya->size, 
        keyb->data,
        keyb->size,
        (uchar *)file->descriptor->data + 4,
        *(u_int32_t *)file->descriptor->data - 4,
        true
        );
    return cmp;
}


