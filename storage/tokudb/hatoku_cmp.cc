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
#include "hatoku_cmp.h"

#ifdef WORDS_BIGENDIAN
#error "WORDS_BIGENDIAN not supported"
#endif

// returns true if the field is a valid field to be used
// in a TokuDB table. The non-valid fields are those
// that have been deprecated since before 5.1, and can
// only exist through upgrades of old versions of MySQL
static bool field_valid_for_tokudb_table(Field* field) {
    bool ret_val = false;
    enum_field_types mysql_type = field->real_type();
    switch (mysql_type) {
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_FLOAT:
#if (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
    (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799) || \
    (100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099)
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIME2:
#endif
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
        ret_val = true;
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
    case MYSQL_TYPE_NULL:
        ret_val = false;
    }
exit:
    return ret_val;
}

static void get_var_field_info(
    uint32_t* field_len, // output: length of field
    uint32_t* start_offset, // output, length of offset where data starts
    uint32_t var_field_index, //input, index of var field we want info on
    const uchar* var_field_offset_ptr, //input, pointer to where offset information for all var fields begins
    uint32_t num_offset_bytes //input, number of bytes used to store offsets starting at var_field_offset_ptr
    ) 
{
    uint32_t data_start_offset = 0;
    uint32_t data_end_offset = 0;
    switch (num_offset_bytes) {
    case (1):
        data_end_offset = (var_field_offset_ptr + var_field_index)[0];
        break;
    case (2):
        data_end_offset = uint2korr(var_field_offset_ptr + 2*var_field_index);
        break;
    default:
        assert(false);
        break;
    }
    
    if (var_field_index) {
        switch (num_offset_bytes) {
        case (1):
            data_start_offset = (var_field_offset_ptr + var_field_index - 1)[0];
            break;
        case (2):
            data_start_offset = uint2korr(var_field_offset_ptr + 2*(var_field_index-1));
            break;
        default:
            assert(false);
            break;
        }
    }
    else {
        data_start_offset = 0;
    }

    *start_offset = data_start_offset;
    assert(data_end_offset >= data_start_offset);
    *field_len = data_end_offset - data_start_offset;
}

static void get_blob_field_info(
    uint32_t* start_offset, 
    uint32_t len_of_offsets,
    const uchar* var_field_data_ptr, 
    uint32_t num_offset_bytes
    ) 
{
    uint32_t data_end_offset;
    //
    // need to set var_field_data_ptr to point to beginning of blobs, which
    // is at the end of the var stuff (if they exist), if var stuff does not exist
    // then the bottom variable will be 0, and var_field_data_ptr is already
    // set correctly
    //
    if (len_of_offsets) {
        switch (num_offset_bytes) {
        case (1):
            data_end_offset = (var_field_data_ptr - 1)[0];
            break;
        case (2):
            data_end_offset = uint2korr(var_field_data_ptr - 2);
            break;
        default:
            assert(false);
            break;
        }
    }
    else {
        data_end_offset = 0;
    }
    *start_offset = data_end_offset;
}


// this function is pattern matched from 
// InnoDB's get_innobase_type_from_mysql_type
static TOKU_TYPE mysql_to_toku_type (Field* field) {
    TOKU_TYPE ret_val = toku_type_unknown;
    enum_field_types mysql_type = field->real_type();
    switch (mysql_type) {
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
        ret_val = toku_type_int;
        goto exit;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
#ifdef MARIADB_BASE_VERSION
        // case to handle fractional seconds in MariaDB
        // 
        if (field->key_type() == HA_KEYTYPE_BINARY) {
            ret_val = toku_type_fixbinary;
            goto exit;
        }
#endif
        ret_val = toku_type_int;
        goto exit;
    case MYSQL_TYPE_DOUBLE:
        ret_val = toku_type_double;
        goto exit;
    case MYSQL_TYPE_FLOAT:
        ret_val = toku_type_float;
        goto exit;
#if (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
    (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799) || \
    (100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099)
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIME2:
#endif
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
    case MYSQL_TYPE_NULL:
        assert(false);
    }
exit:
    return ret_val;
}


static inline CHARSET_INFO* get_charset_from_num (uint32_t charset_number) {
    //
    // patternmatched off of InnoDB, due to MySQL bug 42649
    //
    if (charset_number == default_charset_info->number) {
        return default_charset_info;
    }
    else if (charset_number == my_charset_latin1.number) {
        return &my_charset_latin1;
    }
    else {
        return get_charset(charset_number, MYF(MY_WME));
    } 
}



//
// used to read the length of a variable sized field in a tokudb key (buf).
//
static inline uint32_t get_length_from_var_tokudata (uchar* buf, uint32_t length_bytes) {
    uint32_t length = (uint32_t)(buf[0]);
    if (length_bytes == 2) {
        uint32_t rest_of_length = (uint32_t)buf[1];
        length += rest_of_length<<8;
    }
    return length;
}

//
// used to deduce the number of bytes used to store the length of a varstring/varbinary
// in a key field stored in tokudb
//
static inline uint32_t get_length_bytes_from_max(uint32_t max_num_bytes) {
    return (max_num_bytes > 255) ? 2 : 1;
}



//
// assuming MySQL in little endian, and we are storing in little endian
//
static inline uchar* pack_toku_int (uchar* to_tokudb, uchar* from_mysql, uint32_t num_bytes) {
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
static inline uchar* unpack_toku_int(uchar* to_mysql, uchar* from_tokudb, uint32_t num_bytes) {
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

static inline int cmp_toku_int (uchar* a_buf, uchar* b_buf, bool is_unsigned, uint32_t num_bytes) {
    int ret_val = 0;
    //
    // case for unsigned integers
    //
    if (is_unsigned) {
        uint32_t a_num, b_num = 0;
        uint64_t a_big_num, b_big_num = 0;
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
            a_num = tokudb_uint3korr(a_buf);
            b_num = tokudb_uint3korr(b_buf);
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

static inline uchar* pack_toku_double (uchar* to_tokudb, uchar* from_mysql) {
    memcpy(to_tokudb, from_mysql, sizeof(double));
    return to_tokudb + sizeof(double);
}


static inline uchar* unpack_toku_double(uchar* to_mysql, uchar* from_tokudb) {
    memcpy(to_mysql, from_tokudb, sizeof(double));
    return from_tokudb + sizeof(double);
}

static inline int cmp_toku_double(uchar* a_buf, uchar* b_buf) {
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


static inline uchar* pack_toku_float (uchar* to_tokudb, uchar* from_mysql) {
    memcpy(to_tokudb, from_mysql, sizeof(float));
    return to_tokudb + sizeof(float);
}


static inline uchar* unpack_toku_float(uchar* to_mysql, uchar* from_tokudb) {
    memcpy(to_mysql, from_tokudb, sizeof(float));
    return from_tokudb + sizeof(float);
}

static inline int cmp_toku_float(uchar* a_buf, uchar* b_buf) {
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


static inline uchar* pack_toku_binary(uchar* to_tokudb, uchar* from_mysql, uint32_t num_bytes) {
    memcpy(to_tokudb, from_mysql, num_bytes);
    return to_tokudb + num_bytes;
}

static inline uchar* unpack_toku_binary(uchar* to_mysql, uchar* from_tokudb, uint32_t num_bytes) {
    memcpy(to_mysql, from_tokudb, num_bytes);
    return from_tokudb + num_bytes;
}


static inline int cmp_toku_binary(
    uchar* a_buf, 
    uint32_t a_num_bytes, 
    uchar* b_buf, 
    uint32_t b_num_bytes
    ) 
{
    int ret_val = 0;
    uint32_t num_bytes_to_cmp = (a_num_bytes < b_num_bytes) ? a_num_bytes : b_num_bytes;
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

//
// partially copied from below
//
static uchar* pack_toku_varbinary_from_desc(
    uchar* to_tokudb, 
    const uchar* from_desc, 
    uint32_t key_part_length, //number of bytes to use to encode the length in to_tokudb
    uint32_t field_length //length of field
    )
{
    uint32_t length_bytes_in_tokudb = get_length_bytes_from_max(key_part_length);
    uint32_t length = field_length;
    set_if_smaller(length, key_part_length);
    
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
    memcpy(to_tokudb + length_bytes_in_tokudb, from_desc, length);
    return to_tokudb + length + length_bytes_in_tokudb;
}

static inline uchar* pack_toku_varbinary(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    uint32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    uint32_t max_num_bytes
    ) 
{
    uint32_t length = 0;
    uint32_t length_bytes_in_tokudb;
    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
    case (1):
        length = (uint32_t)(*from_mysql);
        break;
    case (2):
        length = uint2korr(from_mysql);
        break;
    case (3):
        length = tokudb_uint3korr(from_mysql);
        break;
    case (4):
        length = uint4korr(from_mysql);
        break;
    }

    //
    // from this point on, functionality equivalent to pack_toku_varbinary_from_desc
    //
    set_if_smaller(length,max_num_bytes);

    length_bytes_in_tokudb = get_length_bytes_from_max(max_num_bytes);
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

static inline uchar* unpack_toku_varbinary(
    uchar* to_mysql, 
    uchar* from_tokudb, 
    uint32_t length_bytes_in_tokudb, // number of bytes used to encode length in from_tokudb
    uint32_t length_bytes_in_mysql // number of bytes used to encode length in to_mysql
    ) 
{
    uint32_t length = get_length_from_var_tokudata(from_tokudb, length_bytes_in_tokudb);

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

static inline int cmp_toku_varbinary(
    uchar* a_buf, 
    uchar* b_buf, 
    uint32_t length_bytes, //number of bytes used to encode length in a_buf and b_buf
    uint32_t* a_bytes_read, 
    uint32_t* b_bytes_read
    ) 
{
    int ret_val = 0;
    uint32_t a_len = get_length_from_var_tokudata(a_buf, length_bytes);
    uint32_t b_len = get_length_from_var_tokudata(b_buf, length_bytes);
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

static inline uchar* pack_toku_blob(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    uint32_t length_bytes_in_tokudb, //number of bytes to use to encode the length in to_tokudb
    uint32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    uint32_t max_num_bytes,
#if MYSQL_VERSION_ID >= 50600
    const CHARSET_INFO* charset
#else
    CHARSET_INFO* charset
#endif
    ) 
{
    uint32_t length = 0;
    uint32_t local_char_length = 0;
    uchar* blob_buf = NULL;

    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
    case (1):
        length = (uint32_t)(*from_mysql);
        break;
    case (2):
        length = uint2korr(from_mysql);
        break;
    case (3):
        length = tokudb_uint3korr(from_mysql);
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


static inline uchar* unpack_toku_blob(
    uchar* to_mysql, 
    uchar* from_tokudb, 
    uint32_t length_bytes_in_tokudb, // number of bytes used to encode length in from_tokudb
    uint32_t length_bytes_in_mysql // number of bytes used to encode length in to_mysql
    ) 
{
    uint32_t length = get_length_from_var_tokudata(from_tokudb, length_bytes_in_tokudb);
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


//
// partially copied from below
//
static uchar* pack_toku_varstring_from_desc(
    uchar* to_tokudb, 
    const uchar* from_desc, 
    uint32_t key_part_length, //number of bytes to use to encode the length in to_tokudb
    uint32_t field_length,
    uint32_t charset_num//length of field
    )
{
    CHARSET_INFO* charset = NULL;
    uint32_t length_bytes_in_tokudb = get_length_bytes_from_max(key_part_length);
    uint32_t length = field_length;
    uint32_t local_char_length = 0;
    set_if_smaller(length, key_part_length);

    charset = get_charset_from_num(charset_num);
    
    //
    // copy the string
    //
    local_char_length= ((charset->mbmaxlen > 1) ?
                       key_part_length/charset->mbmaxlen : key_part_length);
    if (length > local_char_length)
    {
      local_char_length= my_charpos(
        charset, 
        from_desc, 
        from_desc+length,
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
    memcpy(to_tokudb + length_bytes_in_tokudb, from_desc, length);
    return to_tokudb + length + length_bytes_in_tokudb;
}

static inline uchar* pack_toku_varstring(
    uchar* to_tokudb, 
    uchar* from_mysql, 
    uint32_t length_bytes_in_tokudb, //number of bytes to use to encode the length in to_tokudb
    uint32_t length_bytes_in_mysql, //number of bytes used to encode the length in from_mysql
    uint32_t max_num_bytes,
#if MYSQL_VERSION_ID >= 50600
    const CHARSET_INFO *charset
#else
    CHARSET_INFO* charset
#endif
    ) 
{
    uint32_t length = 0;
    uint32_t local_char_length = 0;

    switch (length_bytes_in_mysql) {
    case (0):
        length = max_num_bytes;
        break;
    case (1):
        length = (uint32_t)(*from_mysql);
        break;
    case (2):
        length = uint2korr(from_mysql);
        break;
    case (3):
        length = tokudb_uint3korr(from_mysql);
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

static inline int cmp_toku_string(
    uchar* a_buf,
    uint32_t a_num_bytes,
    uchar* b_buf, 
    uint32_t b_num_bytes,
    uint32_t charset_number
    ) 
{
    int ret_val = 0;
    CHARSET_INFO* charset = NULL;

    charset = get_charset_from_num(charset_number);

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

static inline int cmp_toku_varstring(
    uchar* a_buf, 
    uchar* b_buf, 
    uint32_t length_bytes, //number of bytes used to encode length in a_buf and b_buf
    uint32_t charset_num,
    uint32_t* a_bytes_read, 
    uint32_t* b_bytes_read
    ) 
{
    int ret_val = 0;
    uint32_t a_len = get_length_from_var_tokudata(a_buf, length_bytes);
    uint32_t b_len = get_length_from_var_tokudata(b_buf, length_bytes);
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

static inline int tokudb_compare_two_hidden_keys(
    const void* new_key_data, 
    const uint32_t new_key_size, 
    const void*  saved_key_data,
    const uint32_t saved_key_size
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
static uint32_t skip_field_in_descriptor(uchar* row_desc) {
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
        row_desc_pos += sizeof(uint32_t);
        break;
    default:
        assert(false);
        break;
    }
    return (uint32_t)(row_desc_pos - row_desc);
}

//
// outputs a descriptor for key into buf. Returns number of bytes used in buf
// to store the descriptor. Number of bytes used MUST match number of bytes
// we would skip in skip_field_in_descriptor
//
static int create_toku_key_descriptor_for_key(KEY* key, uchar* buf) {
    uchar* pos = buf;
    uint32_t num_bytes_in_field = 0;
    uint32_t charset_num = 0;
    for (uint i = 0; i < get_key_parts(key); i++){
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
static int create_toku_key_descriptor(
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
    uint32_t num_bytes = 0;
    uint32_t offset = 0;


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

    
static inline int compare_toku_field(
    uchar* a_buf, 
    uchar* b_buf, 
    uchar* row_desc,
    uint32_t* a_bytes_read, 
    uint32_t* b_bytes_read,
    uint32_t* row_desc_bytes_read,
    bool* read_string
    )
{
    int ret_val = 0;
    uchar* row_desc_pos = row_desc;
    uint32_t num_bytes = 0;
    uint32_t length_bytes = 0;
    uint32_t charset_num = 0;
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
        charset_num = *(uint32_t *)row_desc_pos;
        row_desc_pos += sizeof(uint32_t);
        ret_val = cmp_toku_varstring(
            a_buf,
            b_buf,
            length_bytes,
            charset_num,
            a_bytes_read,
            b_bytes_read
            );
        *read_string = true;
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
static uchar* pack_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    uint32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
    )
{
    uchar* new_pos = NULL;
    uint32_t num_bytes = 0;
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
static uchar* pack_key_toku_key_field(
    uchar* to_tokudb,
    uchar* from_mysql,
    Field* field,
    uint32_t key_part_length //I really hope this is temporary as I phase out the pack_cmp stuff
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
    uint32_t key_part_length
    )
{
    uchar* new_pos = NULL;
    uint32_t num_bytes = 0;
    uint32_t num_bytes_copied;
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
        memset(to_mysql+num_bytes_copied, field->charset()->pad_char, num_bytes - num_bytes_copied);
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


static int tokudb_compare_two_keys(
    const void* new_key_data, 
    const uint32_t new_key_size, 
    const void*  saved_key_data,
    const uint32_t saved_key_size,
    const void*  row_desc,
    const uint32_t row_desc_size,
    bool cmp_prefix,
    bool* read_string
    )
{
    int ret_val = 0;
    int8_t new_key_inf_val = COL_NEG_INF;
    int8_t saved_key_inf_val = COL_NEG_INF;
    
    uchar* row_desc_ptr = (uchar *)row_desc;
    uchar *new_key_ptr = (uchar *)new_key_data;
    uchar *saved_key_ptr = (uchar *)saved_key_data;

    uint32_t new_key_bytes_left = new_key_size;
    uint32_t saved_key_bytes_left = saved_key_size;

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

    while ( (uint32_t)(new_key_ptr - (uchar *)new_key_data) < new_key_size &&
            (uint32_t)(saved_key_ptr - (uchar *)saved_key_data) < saved_key_size &&
            (uint32_t)(row_desc_ptr - (uchar *)row_desc) < row_desc_size
            )
    {
        uint32_t new_key_field_length;
        uint32_t saved_key_field_length;
        uint32_t row_desc_field_length;
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
            &row_desc_field_length,
            read_string
            );
        new_key_ptr += new_key_field_length;
        saved_key_ptr += saved_key_field_length;
        row_desc_ptr += row_desc_field_length;
        if (ret_val) {
            goto exit;
        }

        assert((uint32_t)(new_key_ptr - (uchar *)new_key_data) <= new_key_size);
        assert((uint32_t)(saved_key_ptr - (uchar *)saved_key_data) <= saved_key_size);
        assert((uint32_t)(row_desc_ptr - (uchar *)row_desc) <= row_desc_size);
    }
    new_key_bytes_left = new_key_size - ((uint32_t)(new_key_ptr - (uchar *)new_key_data));
    saved_key_bytes_left = saved_key_size - ((uint32_t)(saved_key_ptr - (uchar *)saved_key_data));
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

static int simple_memcmp(const DBT *keya, const DBT *keyb) {
    int cmp;
    int num_bytes_cmp = keya->size < keyb->size ? 
        keya->size : keyb->size;
    cmp = memcmp(keya->data,keyb->data,num_bytes_cmp);
    if (cmp == 0 && (keya->size != keyb->size)) {
        cmp = keya->size < keyb->size ? -1 : 1;
    }
    return cmp;
}

// comparison function to be used by the fractal trees.
static int tokudb_cmp_dbt_key(DB* file, const DBT *keya, const DBT *keyb) {
    int cmp;
    if (file->cmp_descriptor->dbt.size == 0) {
        cmp = simple_memcmp(keya, keyb);
    }
    else {
        bool read_string = false;
        cmp = tokudb_compare_two_keys(
            keya->data, 
            keya->size, 
            keyb->data,
            keyb->size,
            (uchar *)file->cmp_descriptor->dbt.data + 4,
            (*(uint32_t *)file->cmp_descriptor->dbt.data) - 4,
            false,
            &read_string
            );
        // comparison above may be case-insensitive, but fractal tree
        // needs to distinguish between different data, so we do this
        // additional check here
        if (read_string && (cmp == 0)) {
            cmp = simple_memcmp(keya, keyb);
        }
    }
    return cmp;
}

//TODO: QQQ Only do one direction for prefix.
static int tokudb_prefix_cmp_dbt_key(DB *file, const DBT *keya, const DBT *keyb) {
    // calls to this function are done by the handlerton, and are
    // comparing just the keys as MySQL would compare them.
    bool read_string = false;
    int cmp = tokudb_compare_two_keys(
        keya->data, 
        keya->size, 
        keyb->data,
        keyb->size,
        (uchar *)file->cmp_descriptor->dbt.data + 4,
        *(uint32_t *)file->cmp_descriptor->dbt.data - 4,
        true,
        &read_string
        );
    return cmp;
}

static int tokudb_compare_two_key_parts(
    const void* new_key_data, 
    const uint32_t new_key_size, 
    const void*  saved_key_data,
    const uint32_t saved_key_size,
    const void*  row_desc,
    const uint32_t row_desc_size,
    uint max_parts
    )
{
    int ret_val = 0;
    
    uchar* row_desc_ptr = (uchar *)row_desc;
    uchar *new_key_ptr = (uchar *)new_key_data;
    uchar *saved_key_ptr = (uchar *)saved_key_data;

    //
    // if the keys have an infinity byte, set it
    //
    if (row_desc_ptr[0]) {
        // new_key_inf_val = (int8_t)new_key_ptr[0];
        // saved_key_inf_val = (int8_t)saved_key_ptr[0];
        new_key_ptr++;
        saved_key_ptr++;
    }
    row_desc_ptr++;

    for (uint i = 0; i < max_parts; i++) {
        if (!((uint32_t)(new_key_ptr - (uchar *)new_key_data) < new_key_size &&
               (uint32_t)(saved_key_ptr - (uchar *)saved_key_data) < saved_key_size &&
               (uint32_t)(row_desc_ptr - (uchar *)row_desc) < row_desc_size))
            break;
        uint32_t new_key_field_length;
        uint32_t saved_key_field_length;
        uint32_t row_desc_field_length;
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
        bool read_string = false;
        ret_val = compare_toku_field(
            new_key_ptr, 
            saved_key_ptr, 
            row_desc_ptr,
            &new_key_field_length, 
            &saved_key_field_length,
            &row_desc_field_length,
            &read_string
            );
        new_key_ptr += new_key_field_length;
        saved_key_ptr += saved_key_field_length;
        row_desc_ptr += row_desc_field_length;
        if (ret_val) {
            goto exit;
        }

        assert((uint32_t)(new_key_ptr - (uchar *)new_key_data) <= new_key_size);
        assert((uint32_t)(saved_key_ptr - (uchar *)saved_key_data) <= saved_key_size);
        assert((uint32_t)(row_desc_ptr - (uchar *)row_desc) <= row_desc_size);
    }

    ret_val = 0;
exit:
    return ret_val;
}

static int tokudb_cmp_dbt_key_parts(DB *file, const DBT *keya, const DBT *keyb, uint max_parts) {
    assert(file->cmp_descriptor->dbt.size);
    return tokudb_compare_two_key_parts(
            keya->data, 
            keya->size, 
            keyb->data,
            keyb->size,
            (uchar *)file->cmp_descriptor->dbt.data + 4,
            (*(uint32_t *)file->cmp_descriptor->dbt.data) - 4,
            max_parts);
}

static uint32_t create_toku_main_key_pack_descriptor (
    uchar* buf
    ) 
{
    //
    // The first four bytes always contain the offset of where the first key
    // ends. 
    //
    uchar* pos = buf + 4;
    uint32_t offset = 0;
    //
    // one byte states if this is the main dictionary
    //
    pos[0] = 1;
    pos++;
    goto exit;


exit:
    offset = pos - buf;
    buf[0] = (uchar)(offset & 255);
    buf[1] = (uchar)((offset >> 8) & 255);
    buf[2] = (uchar)((offset >> 16) & 255);
    buf[3] = (uchar)((offset >> 24) & 255);

    return pos - buf;
}

#define COL_HAS_NO_CHARSET 0x44
#define COL_HAS_CHARSET 0x55

#define COL_FIX_PK_OFFSET 0x66
#define COL_VAR_PK_OFFSET 0x77

#define CK_FIX_RANGE 0x88
#define CK_VAR_RANGE 0x99

#define COPY_OFFSET_TO_BUF  memcpy ( \
    pos, \
    &kc_info->cp_info[pk_index][field_index].col_pack_val, \
    sizeof(uint32_t) \
    ); \
    pos += sizeof(uint32_t);


static uint32_t pack_desc_pk_info(uchar* buf, KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, KEY_PART_INFO* key_part) {
    uchar* pos = buf;
    uint16 field_index = key_part->field->field_index;
    Field* field = table_share->field[field_index];
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    uint32_t key_part_length = key_part->length;
    uint32_t field_length;
    uchar len_bytes = 0;

    switch(toku_type) {
    case (toku_type_int):
    case (toku_type_double):
    case (toku_type_float):
        pos[0] = COL_FIX_FIELD;
        pos++;
        assert(kc_info->field_lengths[field_index] < 256);
        pos[0] = kc_info->field_lengths[field_index];
        pos++;
        break;
    case (toku_type_fixbinary):
        pos[0] = COL_FIX_FIELD;
        pos++;
        field_length = field->pack_length();
        set_if_smaller(key_part_length, field_length);
        assert(key_part_length < 256);
        pos[0] = (uchar)key_part_length;
        pos++;
        break;
    case (toku_type_fixstring):
    case (toku_type_varbinary):
    case (toku_type_varstring):
    case (toku_type_blob):
        pos[0] = COL_VAR_FIELD;
        pos++;
        len_bytes = (key_part_length > 255) ? 2 : 1;
        pos[0] = len_bytes;
        pos++;
        break;
    default:
        assert(false);
    }

    return pos - buf;
}

static uint32_t pack_desc_pk_offset_info(
    uchar* buf, 
    KEY_AND_COL_INFO* kc_info, 
    TABLE_SHARE* table_share, 
    KEY_PART_INFO* key_part, 
    KEY* prim_key,
    uchar* pk_info
    ) 
{
    uchar* pos = buf;
    uint16 field_index = key_part->field->field_index;
    bool found_col_in_pk = false;
    uint32_t index_in_pk;

    bool is_constant_offset = true;
    uint32_t offset = 0;
    for (uint i = 0; i < get_key_parts(prim_key); i++) {
        KEY_PART_INFO curr = prim_key->key_part[i];
        uint16 curr_field_index = curr.field->field_index;

        if (pk_info[2*i] == COL_VAR_FIELD) {
            is_constant_offset = false;
        }

        if (curr_field_index == field_index) {
            found_col_in_pk = true;
            index_in_pk = i;
            break;
        }
        offset += pk_info[2*i + 1];
    }
    assert(found_col_in_pk);
    if (is_constant_offset) {
        pos[0] = COL_FIX_PK_OFFSET;
        pos++;
        
        memcpy (pos, &offset, sizeof(offset));
        pos += sizeof(offset);
    }
    else {
        pos[0] = COL_VAR_PK_OFFSET;
        pos++;
        
        memcpy(pos, &index_in_pk, sizeof(index_in_pk));
        pos += sizeof(index_in_pk);
    }
    return pos - buf;
}

static uint32_t pack_desc_offset_info(uchar* buf, KEY_AND_COL_INFO* kc_info, uint pk_index, TABLE_SHARE* table_share, KEY_PART_INFO* key_part) {
    uchar* pos = buf;
    uint16 field_index = key_part->field->field_index;
    Field* field = table_share->field[field_index];
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    bool found_index = false;

    switch(toku_type) {
    case (toku_type_int):
    case (toku_type_double):
    case (toku_type_float):
    case (toku_type_fixbinary):
    case (toku_type_fixstring):
        pos[0] = COL_FIX_FIELD;
        pos++;

        // copy the offset
        COPY_OFFSET_TO_BUF;
        break;
    case (toku_type_varbinary):
    case (toku_type_varstring):
        pos[0] = COL_VAR_FIELD;
        pos++;

        // copy the offset
        COPY_OFFSET_TO_BUF;
        break;
    case (toku_type_blob):
        pos[0] = COL_BLOB_FIELD;
        pos++;
        for (uint32_t i = 0; i < kc_info->num_blobs; i++) {
            uint32_t blob_index = kc_info->blob_fields[i];
            if (blob_index == field_index) {
                uint32_t val = i;
                memcpy(pos, &val, sizeof(uint32_t));
                pos += sizeof(uint32_t);
                found_index = true;
                break;
            }
        }
        assert(found_index);
        break;
    default:
        assert(false);
    }

    return pos - buf;
}

static uint32_t pack_desc_key_length_info(uchar* buf, KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, KEY_PART_INFO* key_part) {
    uchar* pos = buf;
    uint16 field_index = key_part->field->field_index;
    Field* field = table_share->field[field_index];
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    uint32_t key_part_length = key_part->length;
    uint32_t field_length;

    switch(toku_type) {
    case (toku_type_int):
    case (toku_type_double):
    case (toku_type_float):
        // copy the key_part length
        field_length = kc_info->field_lengths[field_index];
        memcpy(pos, &field_length, sizeof(field_length));
        pos += sizeof(key_part_length);
        break;
    case (toku_type_fixbinary):
    case (toku_type_fixstring):
        field_length = field->pack_length();
        set_if_smaller(key_part_length, field_length);
    case (toku_type_varbinary):
    case (toku_type_varstring):
    case (toku_type_blob):
        // copy the key_part length
        memcpy(pos, &key_part_length, sizeof(key_part_length));
        pos += sizeof(key_part_length);
        break;
    default:
        assert(false);
    }

    return pos - buf;
}

static uint32_t pack_desc_char_info(uchar* buf, KEY_AND_COL_INFO* kc_info, TABLE_SHARE* table_share, KEY_PART_INFO* key_part) {
    uchar* pos = buf;
    uint16 field_index = key_part->field->field_index;
    Field* field = table_share->field[field_index];
    TOKU_TYPE toku_type = mysql_to_toku_type(field);
    uint32_t charset_num = 0;

    switch(toku_type) {
    case (toku_type_int):
    case (toku_type_double):
    case (toku_type_float):
    case (toku_type_fixbinary):
    case (toku_type_varbinary):
        pos[0] = COL_HAS_NO_CHARSET;
        pos++;
        break;
    case (toku_type_fixstring):
    case (toku_type_varstring):
    case (toku_type_blob):
        pos[0] = COL_HAS_CHARSET;
        pos++;
        
        // copy the charset
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

    return pos - buf;
}

static uint32_t pack_some_row_info (
    uchar* buf,
    uint pk_index,
    TABLE_SHARE* table_share,
    KEY_AND_COL_INFO* kc_info
    ) 
{
    uchar* pos = buf;
    uint32_t num_null_bytes = 0;
    //
    // four bytes stating number of null bytes
    //
    num_null_bytes = table_share->null_bytes;
    memcpy(pos, &num_null_bytes, sizeof(num_null_bytes));
    pos += sizeof(num_null_bytes);
    //
    // eight bytes stating mcp_info
    //
    memcpy(pos, &kc_info->mcp_info[pk_index], sizeof(MULTI_COL_PACK_INFO));
    pos += sizeof(MULTI_COL_PACK_INFO);
    //
    // one byte for the number of offset bytes
    //
    pos[0] = (uchar)kc_info->num_offset_bytes;
    pos++;

    return pos - buf;
}

static uint32_t get_max_clustering_val_pack_desc_size(
    TABLE_SHARE* table_share
    ) 
{
    uint32_t ret_val = 0;
    //
    // the fixed stuff:
    //  first the things in pack_some_row_info
    //  second another mcp_info
    //  third a byte that states if blobs exist
    ret_val += sizeof(uint32_t) + sizeof(MULTI_COL_PACK_INFO) + 1;
    ret_val += sizeof(MULTI_COL_PACK_INFO);
    ret_val++;
    //
    // now the variable stuff
    //  an upper bound is, for each field, byte stating if it is fixed or var, followed
    // by 8 bytes for endpoints
    //
    ret_val += (table_share->fields)*(1 + 2*sizeof(uint32_t));
    //
    // four bytes storing the length of this portion
    //
    ret_val += 4;

    return ret_val;
}

static uint32_t create_toku_clustering_val_pack_descriptor (
    uchar* buf,
    uint pk_index,
    TABLE_SHARE* table_share,
    KEY_AND_COL_INFO* kc_info,
    uint32_t keynr,
    bool is_clustering
    ) 
{
    uchar* pos = buf + 4;
    uint32_t offset = 0;
    bool start_range_set = false;
    uint32_t last_col = 0;
    //
    // do not need to write anything if the key is not clustering
    //
    if (!is_clustering) {
        goto exit;
    }

    pos += pack_some_row_info(
        pos,
        pk_index,
        table_share,
        kc_info
        );

    //
    // eight bytes stating mcp_info of clustering key
    //
    memcpy(pos, &kc_info->mcp_info[keynr], sizeof(MULTI_COL_PACK_INFO));
    pos += sizeof(MULTI_COL_PACK_INFO);

    //
    // store bit that states if blobs exist
    //
    pos[0] = (kc_info->num_blobs) ? 1 : 0;
    pos++;

    //
    // descriptor assumes that all fields filtered from pk are
    // also filtered from clustering key val. Doing check here to
    // make sure something unexpected does not happen
    //
    for (uint i = 0; i < table_share->fields; i++) {
        bool col_filtered = bitmap_is_set(&kc_info->key_filters[keynr],i);
        bool col_filtered_in_pk = bitmap_is_set(&kc_info->key_filters[pk_index],i);
        if (col_filtered_in_pk) {
            assert(col_filtered);
        }
    }

    //
    // first handle the fixed fields
    // 
    start_range_set = false;
    last_col = 0;
    for (uint i = 0; i < table_share->fields; i++) {
        bool col_filtered = bitmap_is_set(&kc_info->key_filters[keynr],i);
        if (!is_fixed_field(kc_info, i)) {
            //
            // not a fixed field, continue
            //
            continue;
        }
        if (col_filtered && start_range_set) {
            //
            // need to set the end range
            //
            start_range_set = false;
            uint32_t end_offset = kc_info->cp_info[pk_index][last_col].col_pack_val + kc_info->field_lengths[last_col];
            memcpy(pos, &end_offset, sizeof(end_offset));
            pos += sizeof(end_offset);
        }
        else if (!col_filtered) {
            if (!start_range_set) {
                pos[0] = CK_FIX_RANGE;
                pos++;
                start_range_set = true;
                uint32_t start_offset = kc_info->cp_info[pk_index][i].col_pack_val;
                memcpy(pos, &start_offset , sizeof(start_offset));
                pos += sizeof(start_offset);
            }
            last_col = i;
        }
        else {
            continue;
        }
    }
    if (start_range_set) {
        //
        // need to set the end range
        //
        start_range_set = false;
        uint32_t end_offset = kc_info->cp_info[pk_index][last_col].col_pack_val+ kc_info->field_lengths[last_col];
        memcpy(pos, &end_offset, sizeof(end_offset));
        pos += sizeof(end_offset);
    }

    //
    // now handle the var fields
    //
    start_range_set = false;
    last_col = 0;
    for (uint i = 0; i < table_share->fields; i++) {
        bool col_filtered = bitmap_is_set(&kc_info->key_filters[keynr],i);
        if (!is_variable_field(kc_info, i)) {
            //
            // not a var field, continue
            //
            continue;
        }
        if (col_filtered && start_range_set) {
            //
            // need to set the end range
            //
            start_range_set = false;
            uint32_t end_offset = kc_info->cp_info[pk_index][last_col].col_pack_val;
            memcpy(pos, &end_offset, sizeof(end_offset));
            pos += sizeof(end_offset);
        }
        else if (!col_filtered) {
            if (!start_range_set) {
                pos[0] = CK_VAR_RANGE;
                pos++;

                start_range_set = true;
                uint32_t start_offset = kc_info->cp_info[pk_index][i].col_pack_val;
                memcpy(pos, &start_offset , sizeof(start_offset));
                pos += sizeof(start_offset);
            }
            last_col = i;
        }
        else {
            continue;
        }
    }
    if (start_range_set) {
        start_range_set = false;
        uint32_t end_offset = kc_info->cp_info[pk_index][last_col].col_pack_val;
        memcpy(pos, &end_offset, sizeof(end_offset));
        pos += sizeof(end_offset);
    }
    
exit:
    offset = pos - buf;
    buf[0] = (uchar)(offset & 255);
    buf[1] = (uchar)((offset >> 8) & 255);
    buf[2] = (uchar)((offset >> 16) & 255);
    buf[3] = (uchar)((offset >> 24) & 255);

    return pos - buf;
}

static uint32_t pack_clustering_val_from_desc(
    uchar* buf,
    void* row_desc,
    uint32_t row_desc_size,
    const DBT* pk_val
    ) 
{
    uchar* null_bytes_src_ptr = NULL;
    uchar* fixed_src_ptr = NULL;
    uchar* var_src_offset_ptr = NULL;
    uchar* var_src_data_ptr = NULL;
    uchar* fixed_dest_ptr = NULL;
    uchar* var_dest_offset_ptr = NULL;
    uchar* var_dest_data_ptr = NULL;
    uchar* orig_var_dest_data_ptr = NULL;
    uchar* desc_pos = (uchar *)row_desc;
    uint32_t num_null_bytes = 0;
    uint32_t num_offset_bytes;
    MULTI_COL_PACK_INFO src_mcp_info, dest_mcp_info;
    uchar has_blobs;

    memcpy(&num_null_bytes, desc_pos, sizeof(num_null_bytes));
    desc_pos += sizeof(num_null_bytes);

    memcpy(&src_mcp_info, desc_pos, sizeof(src_mcp_info));
    desc_pos += sizeof(src_mcp_info);

    num_offset_bytes = desc_pos[0];
    desc_pos++;

    memcpy(&dest_mcp_info, desc_pos, sizeof(dest_mcp_info));
    desc_pos += sizeof(dest_mcp_info);

    has_blobs = desc_pos[0];
    desc_pos++;

    //
    //set the variables
    //
    null_bytes_src_ptr = (uchar *)pk_val->data;
    fixed_src_ptr = null_bytes_src_ptr + num_null_bytes;    
    var_src_offset_ptr = fixed_src_ptr + src_mcp_info.fixed_field_size;
    var_src_data_ptr = var_src_offset_ptr + src_mcp_info.len_of_offsets;

    fixed_dest_ptr = buf + num_null_bytes;
    var_dest_offset_ptr = fixed_dest_ptr + dest_mcp_info.fixed_field_size;
    var_dest_data_ptr = var_dest_offset_ptr + dest_mcp_info.len_of_offsets;
    orig_var_dest_data_ptr = var_dest_data_ptr;

    //
    // copy the null bytes
    //
    memcpy(buf, null_bytes_src_ptr, num_null_bytes);
    while ( (uint32_t)(desc_pos - (uchar *)row_desc) < row_desc_size) {
        uint32_t start, end, length;
        uchar curr = desc_pos[0];
        desc_pos++;
        
        memcpy(&start, desc_pos, sizeof(start));
        desc_pos += sizeof(start);
        
        memcpy(&end, desc_pos, sizeof(end));
        desc_pos += sizeof(end);
        
        assert (start <= end);

        if (curr == CK_FIX_RANGE) {
            length = end - start;

            memcpy(fixed_dest_ptr, fixed_src_ptr + start, length);
            fixed_dest_ptr += length;
        }
        else if (curr == CK_VAR_RANGE) {
            uint32_t start_data_size;
            uint32_t start_data_offset;
            uint32_t end_data_size;
            uint32_t end_data_offset;
            uint32_t offset_diffs;

            get_var_field_info(
                &start_data_size, 
                &start_data_offset, 
                start, 
                var_src_offset_ptr, 
                num_offset_bytes
                );
            get_var_field_info(
                &end_data_size, 
                &end_data_offset, 
                end, 
                var_src_offset_ptr, 
                num_offset_bytes
                );
            length = end_data_offset + end_data_size - start_data_offset;
            //
            // copy the data
            //
            memcpy(
                var_dest_data_ptr, 
                var_src_data_ptr + start_data_offset, 
                length
                );
            var_dest_data_ptr += length;

            //
            // put in offset info
            //
            offset_diffs = (end_data_offset + end_data_size) - (uint32_t)(var_dest_data_ptr - orig_var_dest_data_ptr);
            for (uint32_t i = start; i <= end; i++) {
                if ( num_offset_bytes == 1 ) {
                    assert(offset_diffs < 256);
                    var_dest_offset_ptr[0] = var_src_offset_ptr[i] - (uchar)offset_diffs;
                    var_dest_offset_ptr++;
                }
                else if ( num_offset_bytes == 2 ) {
                    uint32_t tmp = uint2korr(var_src_offset_ptr + 2*i);
                    uint32_t new_offset = tmp - offset_diffs;
                    assert(new_offset < 1<<16);
                    int2store(var_dest_offset_ptr,new_offset);
                    var_dest_offset_ptr += 2;
                }
                else {
                    assert(false);
                }
            }
        }
        else {
            assert(false);
        }
    }
    //
    // copy blobs
    // at this point, var_dest_data_ptr is pointing to the end, where blobs should be located
    // so, we put the blobs at var_dest_data_ptr
    //
    if (has_blobs) {
        uint32_t num_blob_bytes;
        uint32_t start_offset;
        uchar* src_blob_ptr = NULL;
        get_blob_field_info(
            &start_offset, 
            src_mcp_info.len_of_offsets,
            var_src_data_ptr,
            num_offset_bytes
            );
        src_blob_ptr = var_src_data_ptr + start_offset;
        num_blob_bytes = pk_val->size - (start_offset + (var_src_data_ptr - null_bytes_src_ptr));
        memcpy(var_dest_data_ptr, src_blob_ptr, num_blob_bytes);
        var_dest_data_ptr += num_blob_bytes;
    }
    return var_dest_data_ptr - buf;
}


static uint32_t get_max_secondary_key_pack_desc_size(
    KEY_AND_COL_INFO* kc_info
    ) 
{
    uint32_t ret_val = 0;
    //
    // the fixed stuff:
    //  byte that states if main dictionary
    //  byte that states if hpk
    //  the things in pack_some_row_info
    ret_val++;
    ret_val++;
    ret_val += sizeof(uint32_t) + sizeof(MULTI_COL_PACK_INFO) + 1;
    //
    // now variable sized stuff
    //

    //  first the blobs
    ret_val += sizeof(kc_info->num_blobs);
    ret_val+= kc_info->num_blobs;

    // then the pk
    // one byte for num key parts
    // two bytes for each key part
    ret_val++;
    ret_val += MAX_REF_PARTS*2;

    // then the key
    // null bit, then null byte, 
    // then 1 byte stating what it is, then 4 for offset, 4 for key length, 
    //      1 for if charset exists, and 4 for charset
    ret_val += MAX_REF_PARTS*(1 + sizeof(uint32_t) + 1 + 3*sizeof(uint32_t) + 1);    
    //
    // four bytes storing the length of this portion
    //
    ret_val += 4;
    return ret_val;
}

static uint32_t create_toku_secondary_key_pack_descriptor (
    uchar* buf,
    bool has_hpk,
    uint pk_index,
    TABLE_SHARE* table_share,
    TABLE* table,
    KEY_AND_COL_INFO* kc_info,
    KEY* key_info,
    KEY* prim_key
    ) 
{
    //
    // The first four bytes always contain the offset of where the first key
    // ends. 
    //
    uchar* pk_info = NULL;
    uchar* pos = buf + 4;
    uint32_t offset = 0;

    //
    // first byte states that it is NOT main dictionary
    //
    pos[0] = 0;
    pos++;

    //
    // one byte states if main dictionary has an hpk or not
    //
    if (has_hpk) {
        pos[0] = 1;
    }
    else {
        pos[0] = 0;
    }
    pos++;

    pos += pack_some_row_info(
        pos,
        pk_index,
        table_share,
        kc_info
        );

    //
    // store blob information
    //
    memcpy(pos, &kc_info->num_blobs, sizeof(kc_info->num_blobs));
    pos += sizeof(uint32_t);
    for (uint32_t i = 0; i < kc_info->num_blobs; i++) {
        //
        // store length bytes for each blob
        //
        Field* field = table_share->field[kc_info->blob_fields[i]];
        pos[0] = (uchar)field->row_pack_length();
        pos++;
    }

    //
    // store the pk information
    //
    if (has_hpk) {
        pos[0] = 0;
        pos++;
    }
    else {
        //
        // store number of parts
        //
        assert(get_key_parts(prim_key) < 128);
        pos[0] = 2 * get_key_parts(prim_key);
        pos++;
        //
        // for each part, store if it is a fixed field or var field
        // if fixed, store number of bytes, if var, store
        // number of length bytes
        // total should be two bytes per key part stored
        //
        pk_info = pos;
        uchar* tmp = pos;
        for (uint i = 0; i < get_key_parts(prim_key); i++) {
            tmp += pack_desc_pk_info(
                tmp,
                kc_info,
                table_share,
                &prim_key->key_part[i]
                );
        }
        //
        // asserting that we moved forward as much as we think we have
        //
        assert(tmp - pos == (2 * get_key_parts(prim_key)));
        pos = tmp;
    }

    for (uint i = 0; i < get_key_parts(key_info); i++) {
        KEY_PART_INFO curr_kpi = key_info->key_part[i];
        uint16 field_index = curr_kpi.field->field_index;
        Field* field = table_share->field[field_index];
        bool is_col_in_pk = false;

        if (bitmap_is_set(&kc_info->key_filters[pk_index],field_index)) {
            assert(!has_hpk && prim_key != NULL);
            is_col_in_pk = true;
        }
        else {
            is_col_in_pk = false;
        }

        pos[0] = field->null_bit;
        pos++;

        if (is_col_in_pk) {
            //
            // assert that columns in pk do not have a null bit
            // because in MySQL, pk columns cannot be null
            //
            assert(!field->null_bit);
        }

        if (field->null_bit) {
            uint32_t null_offset = get_null_offset(table,table->field[field_index]);
            memcpy(pos, &null_offset, sizeof(uint32_t));
            pos += sizeof(uint32_t);
        }
        if (is_col_in_pk) {
            pos += pack_desc_pk_offset_info(
                pos,
                kc_info,
                table_share,
                &curr_kpi,
                prim_key,
                pk_info
                );
        }
        else {
            pos += pack_desc_offset_info(
                pos,
                kc_info,
                pk_index,
                table_share,
                &curr_kpi
                );
        }
        pos += pack_desc_key_length_info(
            pos,
            kc_info,
            table_share,
            &curr_kpi
            );
        pos += pack_desc_char_info(
            pos,
            kc_info,
            table_share,
            &curr_kpi
            );
    }

    offset = pos - buf;
    buf[0] = (uchar)(offset & 255);
    buf[1] = (uchar)((offset >> 8) & 255);
    buf[2] = (uchar)((offset >> 16) & 255);
    buf[3] = (uchar)((offset >> 24) & 255);

    return pos - buf;
}

static uint32_t skip_key_in_desc(
    uchar* row_desc
    ) 
{
    uchar* pos = row_desc;
    uchar col_bin_or_char;
    //
    // skip the byte that states if it is a fix field or var field, we do not care
    //
    pos++;

    //
    // skip the offset information
    //
    pos += sizeof(uint32_t);

    //
    // skip the key_part_length info
    //
    pos += sizeof(uint32_t);
    col_bin_or_char = pos[0];
    pos++;
    if (col_bin_or_char == COL_HAS_NO_CHARSET) {
        goto exit;
    }
    //
    // skip the charset info
    //
    pos += 4;
    

exit:
    return (uint32_t)(pos-row_desc);
}


static uint32_t max_key_size_from_desc(
    void* row_desc,
    uint32_t row_desc_size
    ) 
{
    uchar* desc_pos = (uchar *)row_desc;
    uint32_t num_blobs;
    uint32_t num_pk_columns;
    //
    // start at 1 for the infinity byte
    //
    uint32_t max_size = 1;

    // skip byte that states if main dictionary
    bool is_main_dictionary = desc_pos[0];
    desc_pos++;
    assert(!is_main_dictionary);
    
    // skip hpk byte
    desc_pos++;

    // skip num_null_bytes
    desc_pos += sizeof(uint32_t);

    // skip mcp_info
    desc_pos += sizeof(MULTI_COL_PACK_INFO);

    // skip offset_bytes
    desc_pos++;

    // skip over blobs
    memcpy(&num_blobs, desc_pos, sizeof(num_blobs));
    desc_pos += sizeof(num_blobs);
    desc_pos += num_blobs;

    // skip over pk info
    num_pk_columns = desc_pos[0]/2;
    desc_pos++;
    desc_pos += 2*num_pk_columns;

    while ( (uint32_t)(desc_pos - (uchar *)row_desc) < row_desc_size) {
        uchar has_charset;
        uint32_t key_length = 0;

        uchar null_bit = desc_pos[0];
        desc_pos++;

        if (null_bit) {
            //
            // column is NULLable, skip null_offset, and add a null byte
            //
            max_size++;
            desc_pos += sizeof(uint32_t);
        }
        //
        // skip over byte that states if fix or var
        //
        desc_pos++;

        // skip over offset
        desc_pos += sizeof(uint32_t);

        //
        // get the key length and add it to return value
        //
        memcpy(&key_length, desc_pos, sizeof(key_length));
        desc_pos += sizeof(key_length);
        max_size += key_length;
        max_size += 2; // 2 bytes for a potential length bytes, we are upperbounding, does not need to be super tight

        has_charset = desc_pos[0];
        desc_pos++;

        uint32_t charset_num;
        if (has_charset == COL_HAS_CHARSET) {
            // skip over charsent num
            desc_pos += sizeof(charset_num);
        }
        else {
            assert(has_charset == COL_HAS_NO_CHARSET);
        }        
    }
    return max_size;
}

static uint32_t pack_key_from_desc(
    uchar* buf,
    void* row_desc,
    uint32_t row_desc_size,
    const DBT* pk_key,
    const DBT* pk_val
    ) 
{
    MULTI_COL_PACK_INFO mcp_info;
    uint32_t num_null_bytes;
    uint32_t num_blobs;
    uint32_t num_pk_columns;
    uchar* blob_lengths = NULL;
    uchar* pk_info = NULL;
    uchar* pk_data_ptr = NULL;
    uchar* null_bytes_ptr = NULL;
    uchar* fixed_field_ptr = NULL;
    uchar* var_field_offset_ptr = NULL;
    const uchar* var_field_data_ptr = NULL;
    uint32_t num_offset_bytes;
    uchar* packed_key_pos = buf;
    uchar* desc_pos = (uchar *)row_desc;

    bool is_main_dictionary = desc_pos[0];
    desc_pos++;
    assert(!is_main_dictionary);

    //
    // get the constant info out of descriptor
    //
    bool hpk = desc_pos[0];
    desc_pos++;

    memcpy(&num_null_bytes, desc_pos, sizeof(num_null_bytes));
    desc_pos += sizeof(num_null_bytes);

    memcpy(&mcp_info, desc_pos, sizeof(mcp_info));
    desc_pos += sizeof(mcp_info);

    num_offset_bytes = desc_pos[0];
    desc_pos++;

    memcpy(&num_blobs, desc_pos, sizeof(num_blobs));
    desc_pos += sizeof(num_blobs);

    blob_lengths = desc_pos;
    desc_pos += num_blobs;

    num_pk_columns = desc_pos[0]/2;
    desc_pos++;
    pk_info = desc_pos;
    desc_pos += 2*num_pk_columns;

    //
    // now start packing the key
    //

    //
    // pack the infinity byte
    //
    packed_key_pos[0] = COL_ZERO;
    packed_key_pos++;
    //
    // now start packing each column of the key, as described in descriptor
    //
    if (!hpk) {
        // +1 for the infinity byte
        pk_data_ptr = (uchar *)pk_key->data + 1;
    }
    null_bytes_ptr = (uchar *)pk_val->data;
    fixed_field_ptr = null_bytes_ptr + num_null_bytes;
    var_field_offset_ptr = fixed_field_ptr + mcp_info.fixed_field_size;
    var_field_data_ptr = var_field_offset_ptr + mcp_info.len_of_offsets;
    while ( (uint32_t)(desc_pos - (uchar *)row_desc) < row_desc_size) {
        uchar col_fix_val;
        uchar has_charset;
        uint32_t col_pack_val = 0;
        uint32_t key_length = 0;

        uchar null_bit = desc_pos[0];
        desc_pos++;

        if (null_bit) {
            //
            // column is NULLable, need to check the null bytes to see if it is NULL
            //
            uint32_t null_offset = 0;
            bool is_field_null;
            memcpy(&null_offset, desc_pos, sizeof(null_offset));
            desc_pos += sizeof(null_offset);

            is_field_null = (null_bytes_ptr[null_offset] & null_bit) ? true: false;
            if (is_field_null) {
                packed_key_pos[0] = NULL_COL_VAL;
                packed_key_pos++;
                desc_pos += skip_key_in_desc(desc_pos);
                continue;
            }
            else {
                packed_key_pos[0] = NONNULL_COL_VAL;
                packed_key_pos++;
            }
        }
        //
        // now pack the column (unless it was NULL, and we continued)
        //
        col_fix_val = desc_pos[0];
        desc_pos++;

        memcpy(&col_pack_val, desc_pos, sizeof(col_pack_val));
        desc_pos += sizeof(col_pack_val);

        memcpy(&key_length, desc_pos, sizeof(key_length));
        desc_pos += sizeof(key_length);

        has_charset = desc_pos[0];
        desc_pos++;

        uint32_t charset_num = 0;
        if (has_charset == COL_HAS_CHARSET) {
            memcpy(&charset_num, desc_pos, sizeof(charset_num));
            desc_pos += sizeof(charset_num);
        }
        else {
            assert(has_charset == COL_HAS_NO_CHARSET);
        }
        //
        // case where column is in pk val
        //
        if (col_fix_val == COL_FIX_FIELD || col_fix_val == COL_VAR_FIELD || col_fix_val == COL_BLOB_FIELD) {
            if (col_fix_val == COL_FIX_FIELD && has_charset == COL_HAS_NO_CHARSET) {
                memcpy(packed_key_pos, &fixed_field_ptr[col_pack_val], key_length);
                packed_key_pos += key_length;
            }
            else if (col_fix_val == COL_VAR_FIELD && has_charset == COL_HAS_NO_CHARSET) {
                uint32_t data_start_offset = 0;

                uint32_t data_size = 0;
                get_var_field_info(
                    &data_size, 
                    &data_start_offset, 
                    col_pack_val, 
                    var_field_offset_ptr, 
                    num_offset_bytes
                    );

                //
                // length of this field in this row is data_size
                // data is located beginning at var_field_data_ptr + data_start_offset
                //
                packed_key_pos = pack_toku_varbinary_from_desc(
                    packed_key_pos, 
                    var_field_data_ptr + data_start_offset, 
                    key_length, //number of bytes to use to encode the length in to_tokudb
                    data_size //length of field
                    );
            }
            else {
                const uchar* data_start = NULL;
                uint32_t data_start_offset = 0;
                uint32_t data_size = 0;

                if (col_fix_val == COL_FIX_FIELD) {
                    data_start_offset = col_pack_val;
                    data_size = key_length;
                    data_start = fixed_field_ptr + data_start_offset;
                }
                else if (col_fix_val == COL_VAR_FIELD){
                    get_var_field_info(
                        &data_size, 
                        &data_start_offset, 
                        col_pack_val, 
                        var_field_offset_ptr, 
                        num_offset_bytes
                        );
                    data_start = var_field_data_ptr + data_start_offset;
                }
                else if (col_fix_val == COL_BLOB_FIELD) {
                    uint32_t blob_index = col_pack_val;
                    uint32_t blob_offset;
                    const uchar* blob_ptr = NULL;
                    uint32_t field_len;
                    uint32_t field_len_bytes = blob_lengths[blob_index];
                    get_blob_field_info(
                        &blob_offset, 
                        mcp_info.len_of_offsets,
                        var_field_data_ptr, 
                        num_offset_bytes
                        );
                    blob_ptr = var_field_data_ptr + blob_offset;
                    assert(num_blobs > 0);
                    //
                    // skip over other blobs to get to the one we want to make a key out of
                    //
                    for (uint32_t i = 0; i < blob_index; i++) {
                        blob_ptr = unpack_toku_field_blob(
                            NULL,
                            blob_ptr,
                            blob_lengths[i],
                            true
                            );
                    }
                    //
                    // at this point, blob_ptr is pointing to the blob we want to make a key from
                    //
                    field_len = get_blob_field_len(blob_ptr, field_len_bytes);
                    //
                    // now we set the variables to make the key
                    //
                    data_start = blob_ptr + field_len_bytes;
                    data_size = field_len;
                    
                    
                }
                else {
                    assert(false);
                }

                packed_key_pos = pack_toku_varstring_from_desc(
                    packed_key_pos,
                    data_start,
                    key_length,
                    data_size,
                    charset_num
                    );
            }
        }
        //
        // case where column is in pk key
        //
        else {
            if (col_fix_val == COL_FIX_PK_OFFSET) {
                memcpy(packed_key_pos, &pk_data_ptr[col_pack_val], key_length);
                packed_key_pos += key_length;
            }
            else if (col_fix_val == COL_VAR_PK_OFFSET) {
                uchar* tmp_pk_data_ptr = pk_data_ptr;
                uint32_t index_in_pk = col_pack_val;
                //
                // skip along in pk to the right column
                //
                for (uint32_t i = 0; i < index_in_pk; i++) {
                    if (pk_info[2*i] == COL_FIX_FIELD) {
                        tmp_pk_data_ptr += pk_info[2*i + 1];
                    }
                    else if (pk_info[2*i] == COL_VAR_FIELD) {
                        uint32_t len_bytes = pk_info[2*i + 1];
                        uint32_t len;
                        if (len_bytes == 1) {
                            len = tmp_pk_data_ptr[0];
                            tmp_pk_data_ptr++;
                        }
                        else if (len_bytes == 2) {
                            len = uint2korr(tmp_pk_data_ptr);
                            tmp_pk_data_ptr += 2;
                        }
                        else {
                            assert(false);
                        }
                        tmp_pk_data_ptr += len;
                    }
                    else {
                        assert(false);
                    }
                }
                //
                // at this point, tmp_pk_data_ptr is pointing at the column
                //
                uint32_t is_fix_field = pk_info[2*index_in_pk];
                if (is_fix_field == COL_FIX_FIELD) {
                    memcpy(packed_key_pos, tmp_pk_data_ptr, key_length);
                    packed_key_pos += key_length;
                }
                else if (is_fix_field == COL_VAR_FIELD) {
                    const uchar* data_start = NULL;
                    uint32_t data_size = 0;
                    uint32_t len_bytes = pk_info[2*index_in_pk + 1];
                    if (len_bytes == 1) {
                        data_size = tmp_pk_data_ptr[0];
                        tmp_pk_data_ptr++;
                    }
                    else if (len_bytes == 2) {
                        data_size = uint2korr(tmp_pk_data_ptr);
                        tmp_pk_data_ptr += 2;
                    }
                    else {
                        assert(false);
                    }
                    data_start = tmp_pk_data_ptr;

                    if (has_charset == COL_HAS_CHARSET) {
                        packed_key_pos = pack_toku_varstring_from_desc(
                            packed_key_pos,
                            data_start,
                            key_length,
                            data_size,
                            charset_num
                            );
                    }
                    else if (has_charset == COL_HAS_NO_CHARSET) {
                        packed_key_pos = pack_toku_varbinary_from_desc(
                            packed_key_pos, 
                            data_start, 
                            key_length,
                            data_size //length of field
                            );
                    }
                    else {
                        assert(false);
                    }
                }
                else {
                    assert(false);
                }
            }
            else {
                assert(false);
            }
        }
        
    }
    assert( (uint32_t)(desc_pos - (uchar *)row_desc) == row_desc_size);

    //
    // now append the primary key to the end of the key
    //
    if (hpk) {
        memcpy(packed_key_pos, pk_key->data, pk_key->size);
        packed_key_pos += pk_key->size;
    }
    else {
        memcpy(packed_key_pos, (uchar *)pk_key->data + 1, pk_key->size - 1);
        packed_key_pos += (pk_key->size - 1);
    }

    return (uint32_t)(packed_key_pos - buf); // 
}

static bool fields_have_same_name(Field* a, Field* b) {
    return strcmp(a->field_name, b->field_name) == 0;
}

static bool fields_are_same_type(Field* a, Field* b) {
    bool retval = true;
    enum_field_types a_mysql_type = a->real_type();
    enum_field_types b_mysql_type = b->real_type();
    TOKU_TYPE a_toku_type = mysql_to_toku_type(a);
    TOKU_TYPE b_toku_type = mysql_to_toku_type(b);
    // make sure have same names
    // make sure have same types
    if (a_mysql_type != b_mysql_type) {
        retval = false;
        goto cleanup;
    }
    // Thanks to MariaDB 5.5, we can have two fields
    // be the same MySQL type but not the same toku type,
    // This is an issue introduced with MariaDB's fractional time
    // implementation
    if (a_toku_type != b_toku_type) {
        retval = false;
        goto cleanup;
    }
    // make sure that either both are nullable, or both not nullable
    if ((a->null_bit && !b->null_bit) || (!a->null_bit && b->null_bit)) {
        retval = false;
        goto cleanup;
    }
    switch (a_mysql_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
        // length, unsigned, auto increment
        if (a->pack_length() != b->pack_length() ||
            (a->flags & UNSIGNED_FLAG) != (b->flags & UNSIGNED_FLAG) ||
            (a->flags & AUTO_INCREMENT_FLAG) != (b->flags & AUTO_INCREMENT_FLAG)) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_FLOAT:
        // length, unsigned, auto increment
        if (a->pack_length() != b->pack_length() ||
            (a->flags & UNSIGNED_FLAG) != (b->flags & UNSIGNED_FLAG) ||
            (a->flags & AUTO_INCREMENT_FLAG) != (b->flags & AUTO_INCREMENT_FLAG)) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_NEWDECIMAL:
        // length, unsigned
        if (a->pack_length() != b->pack_length() ||
            (a->flags & UNSIGNED_FLAG) != (b->flags & UNSIGNED_FLAG)) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_ENUM: {
        Field_enum *a_enum = static_cast<Field_enum *>(a);
        if (!a_enum->eq_def(b)) {
            retval = false;
            goto cleanup;
        }
        break;
    }   
    case MYSQL_TYPE_SET: {
        Field_set *a_set = static_cast<Field_set *>(a);
        if (!a_set->eq_def(b)) {
            retval = false;
            goto cleanup;
        }
        break;
    }
    case MYSQL_TYPE_BIT:
        // length
        if (a->pack_length() != b->pack_length()) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
#if (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
    (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799) || \
    (100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099)
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIME2:
#endif
        // length
        if (a->pack_length() != b->pack_length()) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
        // test the charset
        if (a->charset()->number != b->charset()->number) {
            retval = false;
            goto cleanup;            
        }
        if (a->row_pack_length() != b->row_pack_length()) {
            retval = false;
            goto cleanup;
        }
        break;
    case MYSQL_TYPE_STRING:
        if (a->pack_length() != b->pack_length()) {
            retval = false;
            goto cleanup;
        }
        // if both are binary, we know have same pack lengths,
        // so we can goto end
        if (a->binary() && b->binary()) {
            // nothing to do, we are good
        }
        else if (!a->binary() && !b->binary()) {
            // test the charset
            if (a->charset()->number != b->charset()->number) {
                retval = false;
                goto cleanup;            
            }
        }
        else {
            // one is binary and the other is not, so not the same
            retval = false;
            goto cleanup;
        }        
        break;
    case MYSQL_TYPE_VARCHAR:
        if (a->field_length != b->field_length) {
            retval = false;
            goto cleanup;
        }
        // if both are binary, we know have same pack lengths,
        // so we can goto end
        if (a->binary() && b->binary()) {
            // nothing to do, we are good
        }
        else if (!a->binary() && !b->binary()) {
            // test the charset
            if (a->charset()->number != b->charset()->number) {
                retval = false;
                goto cleanup;            
            }
        }
        else {
            // one is binary and the other is not, so not the same
            retval = false;
            goto cleanup;
        }        
        break;
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
    case MYSQL_TYPE_NULL:
        assert(false);
    }

cleanup:
    return retval;
}

static bool are_two_fields_same(Field* a, Field* b) {
    return fields_have_same_name(a, b) && fields_are_same_type(a, b);
}


