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
// Update operation codes.  These codes get stuffed into update messages, so they can not change.
// The operations are currently stored in a single byte in the update message, so only 256 operations
// are supported.  When we need more, we can use the last (255) code to indicate that the operation code
// is expanded beyond 1 byte.
enum {
    UPDATE_OP_COL_ADD_OR_DROP = 0,

    UPDATE_OP_EXPAND_VARIABLE_OFFSETS = 1,
    UPDATE_OP_EXPAND_INT = 2,
    UPDATE_OP_EXPAND_UINT = 3,
    UPDATE_OP_EXPAND_CHAR = 4,
    UPDATE_OP_EXPAND_BINARY = 5,
    UPDATE_OP_EXPAND_BLOB = 6,

    UPDATE_OP_UPDATE_1 = 10,
    UPDATE_OP_UPSERT_1 = 11,
    UPDATE_OP_UPDATE_2 = 12,
    UPDATE_OP_UPSERT_2 = 13,
};

// Field types used in the update messages
enum {
    UPDATE_TYPE_UNKNOWN = 0,
    UPDATE_TYPE_INT = 1,
    UPDATE_TYPE_UINT = 2,
    UPDATE_TYPE_CHAR = 3,
    UPDATE_TYPE_BINARY = 4,
    UPDATE_TYPE_VARCHAR = 5,
    UPDATE_TYPE_VARBINARY = 6,
    UPDATE_TYPE_TEXT = 7,
    UPDATE_TYPE_BLOB = 8,
};

#define UP_COL_ADD_OR_DROP UPDATE_OP_COL_ADD_OR_DROP

// add or drop column sub-operations
#define COL_DROP 0xaa
#define COL_ADD 0xbb

// add or drop column types
#define COL_FIXED 0xcc
#define COL_VAR 0xdd
#define COL_BLOB 0xee

#define STATIC_ROW_MUTATOR_SIZE 1+8+2+8+8+8

// how much space do I need for the mutators?
// static stuff first:
// operation 1 == UP_COL_ADD_OR_DROP
// 8 - old null, new null
// 2 - old num_offset, new num_offset
// 8 - old fixed_field size, new fixed_field_size
// 8 - old and new length of offsets
// 8 - old and new starting null bit position
// TOTAL: 27

// dynamic stuff:
// 4 - number of columns
// for each column:
// 1 - add or drop
// 1 - is nullable
// 4 - if nullable, position
// 1 - if add, whether default is null or not
// 1 - if fixed, var, or not
//  for fixed, entire default
//  for var, 4 bytes length, then entire default
//  for blob, nothing
// So, an upperbound is 4 + num_fields(12) + all default stuff

// static blob stuff:
// 4 - num blobs
// 1 byte for each num blobs in old table
// So, an upperbound is 4 + kc_info->num_blobs

// dynamic blob stuff:
// for each blob added:
// 1 - state if we are adding or dropping
// 4 - blob index
// if add, 1 len bytes
//  at most, 4 0's
// So, upperbound is num_blobs(1+4+1+4) = num_columns*10

// The expand varchar offsets message is used to expand the size of an offset from 1 to 2 bytes.  Not VLQ coded.
//     uint8  operation          = UPDATE_OP_EXPAND_VARIABLE_OFFSETS
//     uint32 number of offsets
//     uint32 starting offset of the variable length field offsets 

// Expand the size of a fixed length column message. Not VLQ coded.
// The field type is encoded in the operation code.
//     uint8  operation          = UPDATE_OP_EXPAND_INT/UINT/CHAR/BINARY
//     uint32 offset             offset of the field
//     uint32 old length         the old length of the field's value
//     uint32 new length         the new length of the field's value

//     uint8  operation          = UPDATE_OP_EXPAND_CHAR/BINARY
//     uint32 offset             offset of the field
//     uint32 old length         the old length of the field's value
//     uint32 new length         the new length of the field's value
//     uint8  pad char

// Expand blobs message. VLQ coded.
//     uint8  operation = UPDATE_OP_EXPAND_BLOB
//     uint32 start variable offset
//     uint32 variable offset bytes
//     uint32 bytes per offset
//     uint32 num blobs = N
//     uint8  old lengths[N]
//     uint8  new lengths[N]

// Update and Upsert version 1 messages. Not VLQ coded. Not used anymore, but may be in the
// fractal tree from a previous build.
//
// Field descriptor:
// Operations:
//     update operation   4 == { '=', '+', '-' }
//         x = k
//         x = x + k
//         x = x - k
//     field type         4 see field types above
//     unused             4 unused
//     field null num     4 bit 31 is 1 if the field is nullible and the remaining bits contain the null bit number
//     field offset       4 for fixed fields, this is the offset from begining of the row of the field
//     value:
//         value length   4 == N, length of the value
//         value          N value to add or subtract
//
// Update_1 message:
//     Operation          1 == UPDATE_OP_UPDATE_1
//     fixed field offset 4 offset of the beginning of the fixed fields
//     var field offset   4 offset of the variable length offsets
//     var_offset_bytes   1 length of offsets (Note: not big enough)
//     bytes_per_offset   4 number of bytes per offset
//     Number of update ops 4 == N
//     Update ops [N]
//
// Upsert_1 message:
//     Operation          1 == UPDATE_OP_UPSERT_1
//     Insert row:
//         length         4 == N
//         data           N
//     fixed field offset 4 offset of the beginning of the fixed fields
//     var field offset   4 offset of the variable length offsets
//     var_offset_bytes   1 length of offsets (Note: not big enough)
//     bytes_per_offset   4 number of bytes per offset
//     Number of update ops 4 == N
//     Update ops [N] 

// Update and Upserver version 2 messages. VLQ coded.
// Update version 2
//     uint8  operation = UPDATE_OP_UPDATE_2
//     uint32 number of update ops = N
//     uint8  update ops [ N ] 
//
// Upsert version 2
//     uint8 operation = UPDATE_OP_UPSERT_2
//     uint32 insert length = N
//     uint8 insert data [ N ]
//     uint32 number of update ops = M
//     update ops [ M ]
// 
// Variable fields info
//     uint32 update operation = 'v'
//     uint32 start offset
//     uint32 num varchars
//     uint32 bytes per offset
//
// Blobs info
//     uint32 update operation = 'b'
//     uint32 num blobs = N
//     uint8  blob lengths [ N ]
//
// Update operation on fixed length fields
//     uint32 update operation = '=', '+', '-'
//     uint32 field type
//     uint32 null num 0 => not nullable, otherwise encoded as field_null_num + 1
//     uint32 offset
//     uint32 value length = N
//     uint8  value [ N ] 
//
// Update operation on varchar fields
//     uint32 update operation = '='
//     uint32 field type
//     uint32 null num
//     uint32 var index
//     uint32 value length = N
//     uint8  value [ N ] 
//
// Update operation on blob fields
//     uint32 update operation = '='
//     uint32 field type
//     uint32 null num
//     uint32 blob index
//     uint32 value length = N
//     uint8  value [ N ] 

#include "tokudb_buffer.h"
#include "tokudb_math.h"

//
// checks whether the bit at index pos in data is set or not
//
static inline bool is_overall_null_position_set(uchar* data, uint32_t pos) {
    uint32_t offset = pos/8;
    uchar remainder = pos%8; 
    uchar null_bit = 1<<remainder;
    return ((data[offset] & null_bit) != 0);
}

//
// sets the bit at index pos in data to 1 if is_null, 0 otherwise
// 
static inline void set_overall_null_position(uchar* data, uint32_t pos, bool is_null) {
    uint32_t offset = pos/8;
    uchar remainder = pos%8;
    uchar null_bit = 1<<remainder;
    if (is_null) {
        data[offset] |= null_bit;
    }
    else {
        data[offset] &= ~null_bit;
    }
}

static inline void copy_null_bits(
    uint32_t start_old_pos,
    uint32_t start_new_pos,
    uint32_t num_bits,
    uchar* old_null_bytes,
    uchar* new_null_bytes
    ) 
{
    for (uint32_t i = 0; i < num_bits; i++) {
        uint32_t curr_old_pos = i + start_old_pos;
        uint32_t curr_new_pos = i + start_new_pos;
        // copy over old null bytes
        if (is_overall_null_position_set(old_null_bytes,curr_old_pos)) {
            set_overall_null_position(new_null_bytes,curr_new_pos,true);
        }
        else {
            set_overall_null_position(new_null_bytes,curr_new_pos,false);
        }
    }
}

static inline void copy_var_fields(
    uint32_t start_old_num_var_field, //index of var fields that we should start writing
    uint32_t num_var_fields, // number of var fields to copy
    uchar* old_var_field_offset_ptr, //static ptr to where offset bytes begin in old row
    uchar old_num_offset_bytes, //number of offset bytes used in old row
    uchar* start_new_var_field_data_ptr, // where the new var data should be written
    uchar* start_new_var_field_offset_ptr, // where the new var offsets should be written
    uchar* new_var_field_data_ptr, // pointer to beginning of var fields in new row
    uchar* old_var_field_data_ptr, // pointer to beginning of var fields in old row
    uint32_t new_num_offset_bytes, // number of offset bytes used in new row
    uint32_t* num_data_bytes_written,
    uint32_t* num_offset_bytes_written
    ) 
{
    uchar* curr_new_var_field_data_ptr = start_new_var_field_data_ptr;
    uchar* curr_new_var_field_offset_ptr = start_new_var_field_offset_ptr;
    for (uint32_t i = 0; i < num_var_fields; i++) {
        uint32_t field_len;
        uint32_t start_read_offset;
        uint32_t curr_old = i + start_old_num_var_field;
        uchar* data_to_copy = NULL;
        // get the length and pointer to data that needs to be copied
        get_var_field_info(
            &field_len, 
            &start_read_offset, 
            curr_old, 
            old_var_field_offset_ptr, 
            old_num_offset_bytes
            );
        data_to_copy = old_var_field_data_ptr + start_read_offset;
        // now need to copy field_len bytes starting from data_to_copy
        curr_new_var_field_data_ptr = write_var_field(
            curr_new_var_field_offset_ptr,
            curr_new_var_field_data_ptr,
            new_var_field_data_ptr,
            data_to_copy,
            field_len,
            new_num_offset_bytes
            );
        curr_new_var_field_offset_ptr += new_num_offset_bytes;
    }
    *num_data_bytes_written = (uint32_t)(curr_new_var_field_data_ptr - start_new_var_field_data_ptr);
    *num_offset_bytes_written = (uint32_t)(curr_new_var_field_offset_ptr - start_new_var_field_offset_ptr);
}

static inline uint32_t copy_toku_blob(uchar* to_ptr, uchar* from_ptr, uint32_t len_bytes, bool skip) {
    uint32_t length = 0;
    if (!skip) {
        memcpy(to_ptr, from_ptr, len_bytes);
    }
    length = get_blob_field_len(from_ptr,len_bytes);
    if (!skip) {
        memcpy(to_ptr + len_bytes, from_ptr + len_bytes, length);
    }
    return (length + len_bytes);
}

static int tokudb_hcad_update_fun(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    uint32_t max_num_bytes;
    uint32_t num_columns;
    DBT new_val;
    uint32_t num_bytes_left;
    uint32_t num_var_fields_to_copy;
    uint32_t num_data_bytes_written = 0;
    uint32_t num_offset_bytes_written = 0;
    int error;
    memset(&new_val, 0, sizeof(DBT));
    uchar operation;
    uchar* new_val_data = NULL;
    uchar* extra_pos = NULL;
    uchar* extra_pos_start = NULL;
    //
    // info for pointers into rows
    //
    uint32_t old_num_null_bytes;
    uint32_t new_num_null_bytes;
    uchar old_num_offset_bytes;
    uchar new_num_offset_bytes;
    uint32_t old_fixed_field_size;
    uint32_t new_fixed_field_size;
    uint32_t old_len_of_offsets;
    uint32_t new_len_of_offsets;

    uchar* old_fixed_field_ptr = NULL;
    uchar* new_fixed_field_ptr = NULL;
    uint32_t curr_old_fixed_offset;
    uint32_t curr_new_fixed_offset;

    uchar* old_null_bytes = NULL;
    uchar* new_null_bytes = NULL;
    uint32_t curr_old_null_pos;
    uint32_t curr_new_null_pos;    
    uint32_t old_null_bits_left;
    uint32_t new_null_bits_left;
    uint32_t overall_null_bits_left;

    uint32_t old_num_var_fields;
    // uint32_t new_num_var_fields;
    uint32_t curr_old_num_var_field;
    uint32_t curr_new_num_var_field;
    uchar* old_var_field_offset_ptr = NULL;
    uchar* new_var_field_offset_ptr = NULL;
    uchar* curr_new_var_field_offset_ptr = NULL;
    uchar* old_var_field_data_ptr = NULL;
    uchar* new_var_field_data_ptr = NULL;
    uchar* curr_new_var_field_data_ptr = NULL;

    uint32_t start_blob_offset;
    uchar* start_blob_ptr;
    uint32_t num_blob_bytes;

    // came across a delete, nothing to update
    if (old_val == NULL) {
        error = 0;
        goto cleanup;
    }

    extra_pos_start = (uchar *)extra->data;
    extra_pos = (uchar *)extra->data;

    operation = extra_pos[0];
    extra_pos++;
    assert(operation == UP_COL_ADD_OR_DROP);

    memcpy(&old_num_null_bytes, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);
    memcpy(&new_num_null_bytes, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);

    old_num_offset_bytes = extra_pos[0];
    extra_pos++;
    new_num_offset_bytes = extra_pos[0];
    extra_pos++;

    memcpy(&old_fixed_field_size, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);
    memcpy(&new_fixed_field_size, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);

    memcpy(&old_len_of_offsets, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);
    memcpy(&new_len_of_offsets, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);

    max_num_bytes = old_val->size + extra->size + new_len_of_offsets + new_fixed_field_size;
    new_val_data = (uchar *)tokudb_my_malloc(
        max_num_bytes, 
        MYF(MY_FAE)
        );
    if (new_val_data == NULL) { goto cleanup; }

    old_fixed_field_ptr = (uchar *) old_val->data;
    old_fixed_field_ptr += old_num_null_bytes;
    new_fixed_field_ptr = new_val_data + new_num_null_bytes;
    curr_old_fixed_offset = 0;
    curr_new_fixed_offset = 0;

    old_num_var_fields = old_len_of_offsets/old_num_offset_bytes;
    // new_num_var_fields = new_len_of_offsets/new_num_offset_bytes;
    // following fields will change as we write the variable data
    old_var_field_offset_ptr = old_fixed_field_ptr + old_fixed_field_size;
    new_var_field_offset_ptr = new_fixed_field_ptr + new_fixed_field_size;
    old_var_field_data_ptr = old_var_field_offset_ptr + old_len_of_offsets;
    new_var_field_data_ptr = new_var_field_offset_ptr + new_len_of_offsets;
    curr_new_var_field_offset_ptr = new_var_field_offset_ptr;
    curr_new_var_field_data_ptr = new_var_field_data_ptr;
    curr_old_num_var_field = 0;
    curr_new_num_var_field = 0;

    old_null_bytes = (uchar *)old_val->data;
    new_null_bytes = new_val_data;
    
    memcpy(&curr_old_null_pos, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);
    memcpy(&curr_new_null_pos, extra_pos, sizeof(uint32_t));
    extra_pos += sizeof(uint32_t);

    memcpy(&num_columns, extra_pos, sizeof(num_columns));
    extra_pos += sizeof(num_columns);

    memset(new_null_bytes, 0, new_num_null_bytes); // shut valgrind up
    
    //
    // now go through and apply the change into new_val_data
    //
    for (uint32_t i = 0; i < num_columns; i++) {
        uchar op_type = extra_pos[0];
        bool is_null_default = false;
        extra_pos++;

        assert(op_type == COL_DROP || op_type == COL_ADD);
        bool nullable = (extra_pos[0] != 0);
        extra_pos++;
        if (nullable) {
            uint32_t null_bit_position;
            memcpy(&null_bit_position, extra_pos, sizeof(uint32_t));
            extra_pos += sizeof(uint32_t);
            uint32_t num_bits;
            if (op_type == COL_DROP) {
                assert(curr_old_null_pos <= null_bit_position);
                num_bits = null_bit_position - curr_old_null_pos;
            }
            else {
                assert(curr_new_null_pos <= null_bit_position);
                num_bits = null_bit_position - curr_new_null_pos;
            }
            copy_null_bits(
                curr_old_null_pos,
                curr_new_null_pos,
                num_bits,
                old_null_bytes,
                new_null_bytes
                );
            // update the positions
            curr_new_null_pos += num_bits;
            curr_old_null_pos += num_bits;
            if (op_type == COL_DROP) {
                curr_old_null_pos++; // account for dropped column
            }
            else {
                is_null_default = (extra_pos[0] != 0);
                extra_pos++;
                set_overall_null_position(
                    new_null_bytes,
                    null_bit_position,
                    is_null_default
                    );
                curr_new_null_pos++; //account for added column
            }
        }
        uchar col_type = extra_pos[0];
        extra_pos++;
        if (col_type == COL_FIXED) {
            uint32_t col_offset;
            uint32_t col_size;
            uint32_t num_bytes_to_copy;
            memcpy(&col_offset, extra_pos, sizeof(uint32_t));
            extra_pos += sizeof(uint32_t);
            memcpy(&col_size, extra_pos, sizeof(uint32_t));
            extra_pos += sizeof(uint32_t);

            if (op_type == COL_DROP) {
                num_bytes_to_copy = col_offset - curr_old_fixed_offset;
            }
            else {
                num_bytes_to_copy = col_offset - curr_new_fixed_offset;
            }
            memcpy(
                new_fixed_field_ptr + curr_new_fixed_offset,
                old_fixed_field_ptr + curr_old_fixed_offset, 
                num_bytes_to_copy
                );
            curr_old_fixed_offset += num_bytes_to_copy;
            curr_new_fixed_offset += num_bytes_to_copy;
            if (op_type == COL_DROP) {
                // move old_fixed_offset val to skip OVER column that is being dropped
                curr_old_fixed_offset += col_size;
            }
            else {
                if (is_null_default) {
                    // copy zeroes
                    memset(new_fixed_field_ptr + curr_new_fixed_offset, 0, col_size);
                }
                else {
                    // copy data from extra_pos into new row
                    memcpy(
                        new_fixed_field_ptr + curr_new_fixed_offset,
                        extra_pos,
                        col_size
                        );
                    extra_pos += col_size;
                }
                curr_new_fixed_offset += col_size;
            }
            
        }
        else if (col_type == COL_VAR) {
            uint32_t var_col_index;
            memcpy(&var_col_index, extra_pos, sizeof(uint32_t));
            extra_pos += sizeof(uint32_t);
            if (op_type == COL_DROP) {
                num_var_fields_to_copy = var_col_index - curr_old_num_var_field;
            }
            else {
                num_var_fields_to_copy = var_col_index - curr_new_num_var_field;
            }
            copy_var_fields(
                curr_old_num_var_field,
                num_var_fields_to_copy,
                old_var_field_offset_ptr,
                old_num_offset_bytes,
                curr_new_var_field_data_ptr,
                curr_new_var_field_offset_ptr,
                new_var_field_data_ptr, // pointer to beginning of var fields in new row
                old_var_field_data_ptr, // pointer to beginning of var fields in old row
                new_num_offset_bytes, // number of offset bytes used in new row
                &num_data_bytes_written,
                &num_offset_bytes_written
                );
            curr_new_var_field_data_ptr += num_data_bytes_written;
            curr_new_var_field_offset_ptr += num_offset_bytes_written;
            curr_new_num_var_field += num_var_fields_to_copy;
            curr_old_num_var_field += num_var_fields_to_copy;
            if (op_type == COL_DROP) {
                curr_old_num_var_field++; // skip over dropped field
            }
            else {
                if (is_null_default) {
                    curr_new_var_field_data_ptr = write_var_field(
                        curr_new_var_field_offset_ptr,
                        curr_new_var_field_data_ptr,
                        new_var_field_data_ptr,
                        NULL, //copying no data
                        0, //copying 0 bytes
                        new_num_offset_bytes
                        );
                    curr_new_var_field_offset_ptr += new_num_offset_bytes;
                }
                else {
                    uint32_t data_length;
                    memcpy(&data_length, extra_pos, sizeof(data_length));
                    extra_pos += sizeof(data_length);
                    curr_new_var_field_data_ptr = write_var_field(
                        curr_new_var_field_offset_ptr,
                        curr_new_var_field_data_ptr,
                        new_var_field_data_ptr,
                        extra_pos, //copying data from mutator
                        data_length, //copying data_length bytes
                        new_num_offset_bytes
                        );
                    extra_pos += data_length;
                    curr_new_var_field_offset_ptr += new_num_offset_bytes;
                }
                curr_new_num_var_field++; //account for added column
            }
        }
        else if (col_type == COL_BLOB) {
            // handle blob data later
            continue;
        }
        else {
            assert(false);
        }
    }
    // finish copying the null stuff
    old_null_bits_left = 8*old_num_null_bytes - curr_old_null_pos;
    new_null_bits_left = 8*new_num_null_bytes - curr_new_null_pos;
    overall_null_bits_left = old_null_bits_left;
    set_if_smaller(overall_null_bits_left, new_null_bits_left);
    copy_null_bits(
        curr_old_null_pos,
        curr_new_null_pos,
        overall_null_bits_left,
        old_null_bytes,
        new_null_bytes
        );
    // finish copying fixed field stuff
    num_bytes_left = old_fixed_field_size - curr_old_fixed_offset;
    memcpy(
        new_fixed_field_ptr + curr_new_fixed_offset,
        old_fixed_field_ptr + curr_old_fixed_offset, 
        num_bytes_left
        );
    curr_old_fixed_offset += num_bytes_left;
    curr_new_fixed_offset += num_bytes_left;
    // sanity check
    assert(curr_new_fixed_offset == new_fixed_field_size);

    // finish copying var field stuff
    num_var_fields_to_copy = old_num_var_fields - curr_old_num_var_field;
    copy_var_fields(
        curr_old_num_var_field,
        num_var_fields_to_copy,
        old_var_field_offset_ptr,
        old_num_offset_bytes,
        curr_new_var_field_data_ptr,
        curr_new_var_field_offset_ptr,
        new_var_field_data_ptr, // pointer to beginning of var fields in new row
        old_var_field_data_ptr, // pointer to beginning of var fields in old row
        new_num_offset_bytes, // number of offset bytes used in new row
        &num_data_bytes_written,
        &num_offset_bytes_written
        );
    curr_new_var_field_offset_ptr += num_offset_bytes_written;
    curr_new_var_field_data_ptr += num_data_bytes_written;
    // sanity check
    assert(curr_new_var_field_offset_ptr == new_var_field_data_ptr);

    // start handling blobs
    get_blob_field_info(
        &start_blob_offset, 
        old_len_of_offsets,
        old_var_field_data_ptr,
        old_num_offset_bytes
        );
    start_blob_ptr = old_var_field_data_ptr + start_blob_offset;
    // if nothing else in extra, then there are no blobs to add or drop, so can copy blobs straight
    if ((extra_pos - extra_pos_start) == extra->size) {
        num_blob_bytes = old_val->size - (start_blob_ptr - old_null_bytes);
        memcpy(curr_new_var_field_data_ptr, start_blob_ptr, num_blob_bytes);
        curr_new_var_field_data_ptr += num_blob_bytes;
    }
    // else, there is blob information to process
    else {
        uchar* len_bytes = NULL;
        uint32_t curr_old_blob = 0;
        uint32_t curr_new_blob = 0;
        uint32_t num_old_blobs = 0;
        uchar* curr_old_blob_ptr = start_blob_ptr;
        memcpy(&num_old_blobs, extra_pos, sizeof(num_old_blobs));
        extra_pos += sizeof(num_old_blobs);
        len_bytes = extra_pos;
        extra_pos += num_old_blobs;
        // copy over blob fields one by one
        while ((extra_pos - extra_pos_start) < extra->size) {
            uchar op_type = extra_pos[0];
            extra_pos++;
            uint32_t num_blobs_to_copy = 0;
            uint32_t blob_index;
            memcpy(&blob_index, extra_pos, sizeof(blob_index));
            extra_pos += sizeof(blob_index);
            assert (op_type == COL_DROP || op_type == COL_ADD);
            if (op_type == COL_DROP) {
                num_blobs_to_copy = blob_index - curr_old_blob;
            }
            else {
                num_blobs_to_copy = blob_index - curr_new_blob;
            }
            for (uint32_t i = 0; i < num_blobs_to_copy; i++) {
                uint32_t num_bytes_written = copy_toku_blob(
                    curr_new_var_field_data_ptr,
                    curr_old_blob_ptr,
                    len_bytes[curr_old_blob + i],
                    false
                    );
                curr_old_blob_ptr += num_bytes_written;
                curr_new_var_field_data_ptr += num_bytes_written;
            }
            curr_old_blob += num_blobs_to_copy;
            curr_new_blob += num_blobs_to_copy;
            if (op_type == COL_DROP) {
                // skip over blob in row
                uint32_t num_bytes = copy_toku_blob(
                    NULL,
                    curr_old_blob_ptr,
                    len_bytes[curr_old_blob],
                    true
                    );
                curr_old_blob++;
                curr_old_blob_ptr += num_bytes;
            }
            else {
                // copy new data
                uint32_t new_len_bytes = extra_pos[0];
                extra_pos++;
                uint32_t num_bytes = copy_toku_blob(
                    curr_new_var_field_data_ptr,
                    extra_pos,
                    new_len_bytes,
                    false
                    );
                curr_new_blob++;
                curr_new_var_field_data_ptr += num_bytes;
                extra_pos += num_bytes;
            }                
        }
        num_blob_bytes = old_val->size - (curr_old_blob_ptr - old_null_bytes);
        memcpy(curr_new_var_field_data_ptr, curr_old_blob_ptr, num_blob_bytes);
        curr_new_var_field_data_ptr += num_blob_bytes;
    }
    new_val.data = new_val_data;
    new_val.size = curr_new_var_field_data_ptr - new_val_data;
    set_val(&new_val, set_extra);
    
    error = 0;
cleanup:
    tokudb_my_free(new_val_data);
    return error;    
}

// Expand the variable offset array in the old row given the update mesage in the extra.
static int tokudb_expand_variable_offsets(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    tokudb::buffer extra_val(extra->data, 0, extra->size);

    // decode the operation
    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_EXPAND_VARIABLE_OFFSETS);

    // decode number of offsets
    uint32_t number_of_offsets;
    extra_val.consume(&number_of_offsets, sizeof number_of_offsets);

    // decode the offset start
    uint32_t offset_start;
    extra_val.consume(&offset_start, sizeof offset_start);

    assert(extra_val.size() == extra_val.limit());

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        assert(offset_start + number_of_offsets <= old_val->size);
    
        // compute the new val from the old val
        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)tokudb_my_malloc(number_of_offsets + old_val->size, MYF(MY_FAE));
        if (!new_val_ptr) {
            error = ENOMEM;
            goto cleanup;
        }
        new_val.data = new_val_ptr;
        
        // copy up to the start of the varchar offset
        memcpy(new_val_ptr, old_val_ptr, offset_start);
        new_val_ptr += offset_start;
        old_val_ptr += offset_start;
        
        // expand each offset from 1 to 2 bytes
        for (uint32_t i = 0; i < number_of_offsets; i++) {
            uint16_t new_offset = *old_val_ptr;
            int2store(new_val_ptr, new_offset);
            new_val_ptr += 2;
            old_val_ptr += 1;
        }
        
        // copy the rest of the row
        size_t n = old_val->size - (old_val_ptr - (uchar *)old_val->data);
        memcpy(new_val_ptr, old_val_ptr, n);
        new_val_ptr += n;
        old_val_ptr += n;
        new_val.size = new_val_ptr - (uchar *)new_val.data;

        assert(new_val_ptr == (uchar *)new_val.data + new_val.size);
        assert(old_val_ptr == (uchar *)old_val->data + old_val->size);

        // set the new val
        set_val(&new_val, set_extra);
    }

    error = 0;

cleanup:
    tokudb_my_free(new_val.data);
    return error;
}

// Expand an int field in a old row given the expand message in the extra.
static int tokudb_expand_int_field(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    tokudb::buffer extra_val(extra->data, 0, extra->size);

    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_EXPAND_INT || operation == UPDATE_OP_EXPAND_UINT);
    uint32_t the_offset;
    extra_val.consume(&the_offset, sizeof the_offset);
    uint32_t old_length;
    extra_val.consume(&old_length, sizeof old_length);
    uint32_t new_length;
    extra_val.consume(&new_length, sizeof new_length);
    assert(extra_val.size() == extra_val.limit());

    assert(new_length >= old_length); // expand only

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        assert(the_offset + old_length <= old_val->size); // old field within the old val

        // compute the new val from the old val
        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)tokudb_my_malloc(old_val->size + (new_length - old_length), MYF(MY_FAE));
        if (!new_val_ptr) {
            error = ENOMEM;
            goto cleanup;
        }
        new_val.data = new_val_ptr;
        
        // copy up to the old offset
        memcpy(new_val_ptr, old_val_ptr, the_offset);
        new_val_ptr += the_offset;
        old_val_ptr += the_offset;
        
        switch (operation) {
        case UPDATE_OP_EXPAND_INT:
            // fill the entire new value with ones or zeros depending on the sign bit
            // the encoding is little endian
            memset(new_val_ptr, (old_val_ptr[old_length-1] & 0x80) ? 0xff : 0x00, new_length);
            memcpy(new_val_ptr, old_val_ptr, old_length);  // overlay the low bytes of the new value with the old value
            new_val_ptr += new_length;
            old_val_ptr += old_length;
            break;
        case UPDATE_OP_EXPAND_UINT:
            memset(new_val_ptr, 0, new_length);            // fill the entire new value with zeros
            memcpy(new_val_ptr, old_val_ptr, old_length);  // overlay the low bytes of the new value with the old value
            new_val_ptr += new_length;
            old_val_ptr += old_length;
            break;
        default:
            assert(0);
        }
        
        // copy the rest
        size_t n = old_val->size - (old_val_ptr - (uchar *)old_val->data);
        memcpy(new_val_ptr, old_val_ptr, n);
        new_val_ptr += n;
        old_val_ptr += n;
        new_val.size = new_val_ptr - (uchar *)new_val.data;

        assert(new_val_ptr == (uchar *)new_val.data + new_val.size);
        assert(old_val_ptr == (uchar *)old_val->data + old_val->size);

        // set the new val
        set_val(&new_val, set_extra);
    }

    error = 0;

cleanup:
    tokudb_my_free(new_val.data);
    return error;
}

// Expand a char field in a old row given the expand message in the extra.
static int tokudb_expand_char_field(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    tokudb::buffer extra_val(extra->data, 0, extra->size);

    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_EXPAND_CHAR || operation == UPDATE_OP_EXPAND_BINARY);
    uint32_t the_offset;
    extra_val.consume(&the_offset, sizeof the_offset);
    uint32_t old_length;
    extra_val.consume(&old_length, sizeof old_length);
    uint32_t new_length;
    extra_val.consume(&new_length, sizeof new_length);
    uchar pad_char;
    extra_val.consume(&pad_char, sizeof pad_char);
    assert(extra_val.size() == extra_val.limit());

    assert(new_length >= old_length); // expand only

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        assert(the_offset + old_length <= old_val->size); // old field within the old val

        // compute the new val from the old val
        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)tokudb_my_malloc(old_val->size + (new_length - old_length), MYF(MY_FAE));
        if (!new_val_ptr) {
            error = ENOMEM;
            goto cleanup;
        }
        new_val.data = new_val_ptr;
        
        // copy up to the old offset
        memcpy(new_val_ptr, old_val_ptr, the_offset);
        new_val_ptr += the_offset;
        old_val_ptr += the_offset;
        
        switch (operation) {
        case UPDATE_OP_EXPAND_CHAR:
        case UPDATE_OP_EXPAND_BINARY:
            memset(new_val_ptr, pad_char, new_length);     // fill the entire new value with the pad char
            memcpy(new_val_ptr, old_val_ptr, old_length);  // overlay the low bytes of the new value with the old value
            new_val_ptr += new_length;
            old_val_ptr += old_length;
            break;
        default:
            assert(0);
        }
        
        // copy the rest
        size_t n = old_val->size - (old_val_ptr - (uchar *)old_val->data);
        memcpy(new_val_ptr, old_val_ptr, n);
        new_val_ptr += n;
        old_val_ptr += n;
        new_val.size = new_val_ptr - (uchar *)new_val.data;

        assert(new_val_ptr == (uchar *)new_val.data + new_val.size);
        assert(old_val_ptr == (uchar *)old_val->data + old_val->size);

        // set the new val
        set_val(&new_val, set_extra);
    }

    error = 0;

cleanup:
    tokudb_my_free(new_val.data);
    return error;
}

namespace tokudb {

class var_fields {
public:
    var_fields() {
    }
    void init_var_fields(uint32_t var_offset, uint32_t offset_bytes, uint32_t bytes_per_offset, tokudb::buffer *val_buffer) {
        assert(bytes_per_offset == 0 || bytes_per_offset == 1 || bytes_per_offset == 2);
        m_var_offset = var_offset;
        m_val_offset = m_var_offset + offset_bytes;
        m_bytes_per_offset = bytes_per_offset;
        if (bytes_per_offset > 0) {
            m_num_fields = offset_bytes / bytes_per_offset;
        } else {
            assert(offset_bytes == 0);
            m_num_fields = 0;
        }
        m_val_buffer = val_buffer;
    }
    uint32_t value_offset(uint32_t var_index);
    uint32_t value_length(uint32_t var_index);
    void update_offsets(uint32_t var_index, uint32_t old_s, uint32_t new_s);
    uint32_t end_offset();
    void replace(uint32_t var_index, void *new_val_ptr, uint32_t new_val_length);
private:
    uint32_t read_offset(uint32_t var_index);
    void write_offset(uint32_t var_index, uint32_t v);
private:
    uint32_t m_var_offset;
    uint32_t m_val_offset;
    uint32_t m_bytes_per_offset;
    uint32_t m_num_fields;
    tokudb::buffer *m_val_buffer;
};

// Return the ith variable length offset
uint32_t var_fields::read_offset(uint32_t var_index) {
    uint32_t offset = 0;
    m_val_buffer->read(&offset, m_bytes_per_offset, m_var_offset + var_index * m_bytes_per_offset);
    return offset;
}

// Write the ith variable length offset with a new offset.
void var_fields::write_offset(uint32_t var_index, uint32_t new_offset) {
    m_val_buffer->write(&new_offset, m_bytes_per_offset, m_var_offset + var_index * m_bytes_per_offset);
}

// Return the offset of the ith variable length field
uint32_t var_fields::value_offset(uint32_t var_index) {
    assert(var_index < m_num_fields);
    if (var_index == 0)
        return m_val_offset;
    else
        return m_val_offset + read_offset(var_index-1);
}

// Return the length of the ith variable length field
uint32_t var_fields::value_length(uint32_t var_index) {
    assert(var_index < m_num_fields);
    if (var_index == 0)
        return read_offset(0);
    else
        return read_offset(var_index) - read_offset(var_index-1);
}

// The length of the ith variable length fields changed.  Update all of the subsequent offsets.
void var_fields::update_offsets(uint32_t var_index, uint32_t old_s, uint32_t new_s) {
    assert(var_index < m_num_fields);
    if (old_s == new_s)
        return;
    for (uint i = var_index; i < m_num_fields; i++) {
        uint32_t v = read_offset(i);
        if (new_s > old_s)
            write_offset(i, v + (new_s - old_s));
        else
            write_offset(i, v - (old_s - new_s));
    }
}

uint32_t var_fields::end_offset() {
    if (m_num_fields == 0)
        return m_val_offset;
    else
        return m_val_offset + read_offset(m_num_fields-1);
}

void var_fields::replace(uint32_t var_index, void *new_val_ptr, uint32_t new_val_length) {
    // replace the new val with the extra val
    uint32_t the_offset = value_offset(var_index);
    uint32_t old_s = value_length(var_index);
    uint32_t new_s = new_val_length;
    m_val_buffer->replace(the_offset, old_s, new_val_ptr, new_s);

    // update the var offsets
    update_offsets(var_index, old_s, new_s);
}

class blob_fields {
public:
    blob_fields() {
    }
    void init_blob_fields(uint32_t num_blobs, const uint8_t *blob_lengths, tokudb::buffer *val_buffer) {
        m_num_blobs = num_blobs; m_blob_lengths = blob_lengths; m_val_buffer = val_buffer;
    }
    void start_blobs(uint32_t offset) {
        m_blob_offset = offset;
    }
    void replace(uint32_t blob_index, uint32_t length, void *p);
    
    void expand_length(uint32_t blob_index, uint8_t old_length_length, uint8_t new_length_length);
private:
    uint32_t read_length(uint32_t offset, size_t size);
    void write_length(uint32_t offset, size_t size, uint32_t new_length);
    uint32_t blob_offset(uint32_t blob_index);
private:
    uint32_t m_blob_offset;
    uint32_t m_num_blobs;
    const uint8_t *m_blob_lengths;
    tokudb::buffer *m_val_buffer;
};

uint32_t blob_fields::read_length(uint32_t offset, size_t blob_length) {
    uint32_t length = 0;
    m_val_buffer->read(&length, blob_length, offset);
    return length;
}

void blob_fields::write_length(uint32_t offset, size_t size, uint32_t new_length) {
    m_val_buffer->write(&new_length, size, offset);
}

uint32_t blob_fields::blob_offset(uint32_t blob_index) {
    assert(blob_index < m_num_blobs);
    uint32_t offset = m_blob_offset;
    for (uint i = 0; i < blob_index; i++) {
        uint32_t blob_length = m_blob_lengths[i];
        uint32_t length = read_length(offset, blob_length);
        offset += blob_length + length;
    }
    return offset;
}

void blob_fields::replace(uint32_t blob_index, uint32_t new_length, void *new_value) {
    assert(blob_index < m_num_blobs);

    // compute the ith blob offset
    uint32_t offset = blob_offset(blob_index);
    uint8_t blob_length = m_blob_lengths[blob_index];

    // read the old length
    uint32_t old_length = read_length(offset, blob_length);

    // replace the data
    m_val_buffer->replace(offset + blob_length, old_length, new_value, new_length);

    // write the new length
    write_length(offset, blob_length, new_length);
}

void blob_fields::expand_length(uint32_t blob_index, uint8_t old_length_length, uint8_t new_length_length) {
    assert(blob_index < m_num_blobs);
    assert(old_length_length == m_blob_lengths[blob_index]);

    // compute the ith blob offset
    uint32_t offset = blob_offset(blob_index);

    // read the blob length
    uint32_t blob_length = read_length(offset, old_length_length);

    // expand the length
    m_val_buffer->replace(offset, old_length_length, &blob_length, new_length_length);
}

class value_map {
public:
    value_map(tokudb::buffer *val_buffer) : m_val_buffer(val_buffer) {
    }

    void init_var_fields(uint32_t var_offset, uint32_t offset_bytes, uint32_t bytes_per_offset) {
        m_var_fields.init_var_fields(var_offset, offset_bytes, bytes_per_offset, m_val_buffer);
    }

    void init_blob_fields(uint32_t num_blobs, const uint8_t *blob_lengths) {
        m_blob_fields.init_blob_fields(num_blobs, blob_lengths, m_val_buffer);
    }

    // Replace the value of a fixed length field
    void replace_fixed(uint32_t the_offset, uint32_t field_null_num, void *new_val_ptr, uint32_t new_val_length) {
        m_val_buffer->replace(the_offset, new_val_length, new_val_ptr, new_val_length);
        maybe_clear_null(field_null_num);
    }

    // Replace the value of a variable length field
    void replace_varchar(uint32_t var_index, uint32_t field_null_num, void *new_val_ptr, uint32_t new_val_length) {
        m_var_fields.replace(var_index, new_val_ptr, new_val_length);
        maybe_clear_null(field_null_num);
    }

    // Replace the value of a blob field
    void replace_blob(uint32_t blob_index, uint32_t field_null_num, void *new_val_ptr, uint32_t new_val_length) {
        m_blob_fields.start_blobs(m_var_fields.end_offset());
        m_blob_fields.replace(blob_index, new_val_length, new_val_ptr);
        maybe_clear_null(field_null_num);
    }

    void expand_blob_lengths(uint32_t num_blob, const uint8_t *old_length, const uint8_t *new_length);

    void int_op(uint32_t operation, uint32_t the_offset, uint32_t length, uint32_t field_null_num,
                tokudb::buffer &old_val, void *extra_val);

    void uint_op(uint32_t operation, uint32_t the_offset, uint32_t length, uint32_t field_null_num,
                 tokudb::buffer &old_val, void *extra_val);

private:
    bool is_null(uint32_t null_num, uchar *null_bytes) {
        bool field_is_null = false;
        if (null_num) {
            if (null_num & (1<<31))
                null_num &= ~(1<<31);
            else
                null_num -= 1;
            field_is_null = is_overall_null_position_set(null_bytes, null_num);
        }
        return field_is_null;
    }

    void maybe_clear_null(uint32_t null_num) {
        if (null_num) {
            if (null_num & (1<<31))
                null_num &= ~(1<<31);
            else
                null_num -= 1;            
            set_overall_null_position((uchar *) m_val_buffer->data(), null_num, false);
        }
    }

private:
    var_fields m_var_fields;
    blob_fields m_blob_fields;
    tokudb::buffer *m_val_buffer;
};

// Update an int field: signed newval@offset = old_val@offset OP extra_val
void value_map::int_op(uint32_t operation, uint32_t the_offset, uint32_t length, uint32_t field_null_num,
                              tokudb::buffer &old_val, void *extra_val) {
    assert(the_offset + length <= m_val_buffer->size());
    assert(the_offset + length <= old_val.size());
    assert(length == 1 || length == 2 || length == 3 || length == 4 || length == 8);

    uchar *old_val_ptr = (uchar *) old_val.data();
    bool field_is_null = is_null(field_null_num, old_val_ptr);
    int64_t v = 0;
    memcpy(&v, old_val_ptr + the_offset, length);
    v = tokudb::int_sign_extend(v, 8*length);
    int64_t extra_v = 0;
    memcpy(&extra_v, extra_val, length);
    extra_v = tokudb::int_sign_extend(extra_v, 8*length);
    switch (operation) {
    case '+':
        if (!field_is_null) {
            bool over;
            v = tokudb::int_add(v, extra_v, 8*length, &over);
            if (over) {
                if (extra_v > 0)
                    v = tokudb::int_high_endpoint(8*length);
                else
                    v = tokudb::int_low_endpoint(8*length);
            }
            m_val_buffer->replace(the_offset, length, &v, length);
        }
        break;
    case '-':
        if (!field_is_null) {
            bool over;
            v = tokudb::int_sub(v, extra_v, 8*length, &over);
            if (over) {
                if (extra_v > 0)
                    v = tokudb::int_low_endpoint(8*length);
                else
                    v = tokudb::int_high_endpoint(8*length);
            }
            m_val_buffer->replace(the_offset, length, &v, length);
        }
        break;
    default:
        assert(0);
    }
}

// Update an unsigned field: unsigned newval@offset = old_val@offset OP extra_val
void value_map::uint_op(uint32_t operation, uint32_t the_offset, uint32_t length, uint32_t field_null_num,
                               tokudb::buffer &old_val, void *extra_val) {
    assert(the_offset + length <= m_val_buffer->size());
    assert(the_offset + length <= old_val.size());
    assert(length == 1 || length == 2 || length == 3 || length == 4 || length == 8);

    uchar *old_val_ptr = (uchar *) old_val.data();
    bool field_is_null = is_null(field_null_num, old_val_ptr);
    uint64_t v = 0;
    memcpy(&v, old_val_ptr + the_offset, length);
    uint64_t extra_v = 0;
    memcpy(&extra_v, extra_val, length);
    switch (operation) {
    case '+':
        if (!field_is_null) {
            bool over;
            v = tokudb::uint_add(v, extra_v, 8*length, &over);
            if (over) {
                v = tokudb::uint_high_endpoint(8*length);
            }
            m_val_buffer->replace(the_offset, length, &v, length);
        }
        break;
    case '-':
        if (!field_is_null) {
            bool over;
            v = tokudb::uint_sub(v, extra_v, 8*length, &over);
            if (over) {
                v = tokudb::uint_low_endpoint(8*length);
            }
            m_val_buffer->replace(the_offset, length, &v, length);
        }
        break;
    default:
        assert(0);
    }
}

void value_map::expand_blob_lengths(uint32_t num_blob, const uint8_t *old_length, const uint8_t *new_length) {
    uint8_t current_length[num_blob];
    memcpy(current_length, old_length, num_blob);
    for (uint32_t i = 0; i < num_blob; i++) {
        if (new_length[i] > current_length[i]) {
            m_blob_fields.init_blob_fields(num_blob, current_length, m_val_buffer);
            m_blob_fields.start_blobs(m_var_fields.end_offset());
            m_blob_fields.expand_length(i, current_length[i], new_length[i]);
            current_length[i] = new_length[i];
        }
    }
}

}

static uint32_t consume_uint32(tokudb::buffer &b) {
    uint32_t n;
    size_t s = b.consume_ui<uint32_t>(&n);
    assert(s > 0);
    return n;
}

static uint8_t *consume_uint8_array(tokudb::buffer &b, uint32_t array_size) {
    uint8_t *p = (uint8_t *) b.consume_ptr(array_size);
    assert(p);
    return p;
}

static int tokudb_expand_blobs(
    DB* db,
    const DBT *key_dbt,
    const DBT *old_val_dbt,
    const DBT *extra,
    void (*set_val)(const DBT *new_val_dbt, void *set_extra),
    void *set_extra
    )
{
    tokudb::buffer extra_val(extra->data, 0, extra->size);
    
    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_EXPAND_BLOB);

    if (old_val_dbt != NULL) {
        // new val = old val
        tokudb::buffer new_val;
        new_val.append(old_val_dbt->data, old_val_dbt->size);

        tokudb::value_map vd(&new_val);

        // decode variable field info
        uint32_t var_field_offset = consume_uint32(extra_val);
        uint32_t var_offset_bytes = consume_uint32(extra_val);
        uint32_t bytes_per_offset = consume_uint32(extra_val);
        vd.init_var_fields(var_field_offset, var_offset_bytes, bytes_per_offset);

        // decode blob info
        uint32_t num_blob = consume_uint32(extra_val);
        const uint8_t *old_blob_length = consume_uint8_array(extra_val, num_blob);
        const uint8_t *new_blob_length = consume_uint8_array(extra_val, num_blob);
        assert(extra_val.size() == extra_val.limit());

        // expand blob lengths
        vd.expand_blob_lengths(num_blob, old_blob_length, new_blob_length);

        // set the new val
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.data = new_val.data();
        new_val_dbt.size = new_val.size();
        set_val(&new_val_dbt, set_extra);
    }
    return 0;
}

// Decode and apply a sequence of update operations defined in the extra to the old value and put the result in the new value.
static void apply_1_updates(tokudb::value_map &vd, tokudb::buffer &new_val, tokudb::buffer &old_val, tokudb::buffer &extra_val) {
    uint32_t num_updates;
    extra_val.consume(&num_updates, sizeof num_updates);
    for ( ; num_updates > 0; num_updates--) {
        // get the update operation
        uint32_t update_operation;
        extra_val.consume(&update_operation, sizeof update_operation);
        uint32_t field_type;
        extra_val.consume(&field_type, sizeof field_type);
        uint32_t unused;
        extra_val.consume(&unused, sizeof unused);
        uint32_t field_null_num;
        extra_val.consume(&field_null_num, sizeof field_null_num);
        uint32_t the_offset;
        extra_val.consume(&the_offset, sizeof the_offset);
        uint32_t extra_val_length;
        extra_val.consume(&extra_val_length, sizeof extra_val_length);
        void *extra_val_ptr = extra_val.consume_ptr(extra_val_length);

        // apply the update
        switch (field_type) {
        case UPDATE_TYPE_INT:
            if (update_operation == '=')
                vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
            else
                vd.int_op(update_operation, the_offset, extra_val_length, field_null_num, old_val, extra_val_ptr);
            break;
        case UPDATE_TYPE_UINT:
            if (update_operation == '=')
                vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
            else
                vd.uint_op(update_operation, the_offset, extra_val_length, field_null_num, old_val, extra_val_ptr);
            break;
        case UPDATE_TYPE_CHAR:
        case UPDATE_TYPE_BINARY:
            if (update_operation == '=')
                vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
            else 
                assert(0);
            break;
        default:
            assert(0);
            break;
        }
    }
    assert(extra_val.size() == extra_val.limit());
}

// Simple update handler. Decode the update message, apply the update operations to the old value, and set the new value.
static int tokudb_update_1_fun(
    DB* db,
    const DBT *key_dbt,
    const DBT *old_val_dbt,
    const DBT *extra,
    void (*set_val)(const DBT *new_val_dbt, void *set_extra),
    void *set_extra
    )
{
    tokudb::buffer extra_val(extra->data, 0, extra->size);
    
    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_UPDATE_1);

    if (old_val_dbt != NULL) {
        // get the simple descriptor
        uint32_t m_fixed_field_offset;
        extra_val.consume(&m_fixed_field_offset, sizeof m_fixed_field_offset);
        uint32_t m_var_field_offset;
        extra_val.consume(&m_var_field_offset, sizeof m_var_field_offset);
        uint32_t m_var_offset_bytes;
        extra_val.consume(&m_var_offset_bytes, sizeof m_var_offset_bytes);
        uint32_t m_bytes_per_offset;
        extra_val.consume(&m_bytes_per_offset, sizeof m_bytes_per_offset);

        tokudb::buffer old_val(old_val_dbt->data, old_val_dbt->size, old_val_dbt->size);

        // new val = old val
        tokudb::buffer new_val;
        new_val.append(old_val_dbt->data, old_val_dbt->size);

        tokudb::value_map vd(&new_val);
        vd.init_var_fields(m_var_field_offset, m_var_offset_bytes, m_bytes_per_offset);

        // apply updates to new val
        apply_1_updates(vd, new_val, old_val, extra_val);

        // set the new val
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.data = new_val.data();
        new_val_dbt.size = new_val.size();
        set_val(&new_val_dbt, set_extra);
    }

    return 0;
}

// Simple upsert handler. Decode the upsert message. If the key does not exist, then insert a new value from the extra.
// Otherwise, apply the update operations to the old value, and then set the new value.
static int tokudb_upsert_1_fun(
    DB* db,
    const DBT *key_dbt,
    const DBT *old_val_dbt, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val_dbt, void *set_extra),
    void *set_extra
    )
{
    tokudb::buffer extra_val(extra->data, 0, extra->size);

    uint8_t operation;
    extra_val.consume(&operation, sizeof operation);
    assert(operation == UPDATE_OP_UPSERT_1);

    uint32_t insert_length;
    extra_val.consume(&insert_length, sizeof insert_length);
    void *insert_row = extra_val.consume_ptr(insert_length);

    if (old_val_dbt == NULL) {
        // insert a new row
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.size = insert_length;
        new_val_dbt.data = insert_row;
        set_val(&new_val_dbt, set_extra);
    } else {
        // decode the simple descriptor
        uint32_t m_fixed_field_offset;
        extra_val.consume(&m_fixed_field_offset, sizeof m_fixed_field_offset);
        uint32_t m_var_field_offset;
        extra_val.consume(&m_var_field_offset, sizeof m_var_field_offset);
        uint32_t m_var_offset_bytes;
        extra_val.consume(&m_var_offset_bytes, sizeof m_var_offset_bytes);
        uint32_t m_bytes_per_offset;
        extra_val.consume(&m_bytes_per_offset, sizeof m_bytes_per_offset);

        tokudb::buffer old_val(old_val_dbt->data, old_val_dbt->size, old_val_dbt->size);

        // new val = old val
        tokudb::buffer new_val;
        new_val.append(old_val_dbt->data, old_val_dbt->size);

        tokudb::value_map vd(&new_val);
        vd.init_var_fields(m_var_field_offset, m_var_offset_bytes, m_bytes_per_offset);

        // apply updates to new val
        apply_1_updates(vd, new_val, old_val, extra_val);

        // set the new val
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.data = new_val.data();
        new_val_dbt.size = new_val.size();
        set_val(&new_val_dbt, set_extra);
    }

    return 0;
}

// Decode and apply a sequence of update operations defined in the extra to the old value and put the result in the new value.
static void apply_2_updates(tokudb::value_map &vd, tokudb::buffer &new_val, tokudb::buffer &old_val, tokudb::buffer &extra_val) {
    uint32_t num_updates = consume_uint32(extra_val);
    for (uint32_t i = 0; i < num_updates; i++) {
        uint32_t update_operation = consume_uint32(extra_val);
        if (update_operation == 'v') {
            uint32_t var_field_offset = consume_uint32(extra_val);
            uint32_t var_offset_bytes = consume_uint32(extra_val);
            uint32_t bytes_per_offset = consume_uint32(extra_val);
            vd.init_var_fields(var_field_offset, var_offset_bytes, bytes_per_offset);
        } else if (update_operation == 'b') {
            uint32_t num_blobs = consume_uint32(extra_val);
            const uint8_t *blob_lengths = consume_uint8_array(extra_val, num_blobs);
            vd.init_blob_fields(num_blobs, blob_lengths);
        } else {
            uint32_t field_type = consume_uint32(extra_val);
            uint32_t field_null_num = consume_uint32(extra_val);
            uint32_t the_offset = consume_uint32(extra_val);
            uint32_t extra_val_length = consume_uint32(extra_val);
            void *extra_val_ptr = extra_val.consume_ptr(extra_val_length); assert(extra_val_ptr);

            switch (field_type) {
            case UPDATE_TYPE_INT:
                if (update_operation == '=')
                    vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
                else
                    vd.int_op(update_operation, the_offset, extra_val_length, field_null_num, old_val, extra_val_ptr);
                break;
            case UPDATE_TYPE_UINT:
                if (update_operation == '=')
                    vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
                else
                    vd.uint_op(update_operation, the_offset, extra_val_length, field_null_num, old_val, extra_val_ptr);
                break;
            case UPDATE_TYPE_CHAR:
            case UPDATE_TYPE_BINARY:
                if (update_operation == '=')
                    vd.replace_fixed(the_offset, field_null_num, extra_val_ptr, extra_val_length);
                else 
                    assert(0);
                break;
            case UPDATE_TYPE_VARBINARY:
            case UPDATE_TYPE_VARCHAR:
                if (update_operation == '=') 
                    vd.replace_varchar(the_offset, field_null_num, extra_val_ptr, extra_val_length);
                else
                    assert(0);
                break;
            case UPDATE_TYPE_TEXT:
            case UPDATE_TYPE_BLOB: 
                if (update_operation == '=')
                    vd.replace_blob(the_offset, field_null_num, extra_val_ptr, extra_val_length);
                else
                    assert(0);
                break;
            default:
                assert(0);
                break;
            }
        }
    }
    assert(extra_val.size() == extra_val.limit());
}

// Simple update handler. Decode the update message, apply the update operations to the old value, and set the new value.
static int tokudb_update_2_fun(
    DB* db,
    const DBT *key_dbt,
    const DBT *old_val_dbt,
    const DBT *extra,
    void (*set_val)(const DBT *new_val_dbt, void *set_extra),
    void *set_extra
    )
{
    tokudb::buffer extra_val(extra->data, 0, extra->size);
    
    uint8_t op;
    extra_val.consume(&op, sizeof op);
    assert(op == UPDATE_OP_UPDATE_2);

    if (old_val_dbt != NULL) {
        tokudb::buffer old_val(old_val_dbt->data, old_val_dbt->size, old_val_dbt->size);

        // new val = old val
        tokudb::buffer new_val;
        new_val.append(old_val_dbt->data, old_val_dbt->size);

        tokudb::value_map vd(&new_val);

        // apply updates to new val
        apply_2_updates(vd, new_val, old_val, extra_val);

        // set the new val
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.data = new_val.data();
        new_val_dbt.size = new_val.size();
        set_val(&new_val_dbt, set_extra);
    }

    return 0;
}

// Simple upsert handler. Decode the upsert message. If the key does not exist, then insert a new value from the extra.
// Otherwise, apply the update operations to the old value, and then set the new value.
static int tokudb_upsert_2_fun(
    DB* db,
    const DBT *key_dbt,
    const DBT *old_val_dbt, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val_dbt, void *set_extra),
    void *set_extra
    )
{
    tokudb::buffer extra_val(extra->data, 0, extra->size);

    uint8_t op;
    extra_val.consume(&op, sizeof op);
    assert(op == UPDATE_OP_UPSERT_2);

    uint32_t insert_length = consume_uint32(extra_val);
    assert(insert_length < extra_val.limit());
    void *insert_row = extra_val.consume_ptr(insert_length); assert(insert_row);

    if (old_val_dbt == NULL) {
        // insert a new row
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.size = insert_length;
        new_val_dbt.data = insert_row;
        set_val(&new_val_dbt, set_extra);
    } else {
        tokudb::buffer old_val(old_val_dbt->data, old_val_dbt->size, old_val_dbt->size);

        // new val = old val
        tokudb::buffer new_val;
        new_val.append(old_val_dbt->data, old_val_dbt->size);

        tokudb::value_map vd(&new_val);

        // apply updates to new val
        apply_2_updates(vd, new_val, old_val, extra_val);

        // set the new val
        DBT new_val_dbt; memset(&new_val_dbt, 0, sizeof new_val_dbt);
        new_val_dbt.data = new_val.data();
        new_val_dbt.size = new_val.size();
        set_val(&new_val_dbt, set_extra);
    }

    return 0;
}

// This function is the update callback function that is registered with the YDB environment.
// It uses the first byte in the update message to identify the update message type and call
// the handler for that message.
int tokudb_update_fun(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    assert(extra->size > 0);
    uint8_t *extra_pos = (uchar *)extra->data;
    uint8_t operation = extra_pos[0];
    int error;
    switch (operation) {
    case UPDATE_OP_COL_ADD_OR_DROP:
        error = tokudb_hcad_update_fun(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_EXPAND_VARIABLE_OFFSETS:
        error = tokudb_expand_variable_offsets(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_EXPAND_INT:
    case UPDATE_OP_EXPAND_UINT:
        error = tokudb_expand_int_field(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_EXPAND_CHAR:
    case UPDATE_OP_EXPAND_BINARY:
        error = tokudb_expand_char_field(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_EXPAND_BLOB:
        error = tokudb_expand_blobs(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_UPDATE_1:
        error = tokudb_update_1_fun(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_UPSERT_1:
        error = tokudb_upsert_1_fun(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_UPDATE_2:
        error = tokudb_update_2_fun(db, key, old_val, extra, set_val, set_extra);
        break;
    case UPDATE_OP_UPSERT_2:
        error = tokudb_upsert_2_fun(db, key, old_val, extra, set_val, set_extra);
        break;
    default:
        assert(0);
        error = EINVAL;
        break;
    }
    return error;
}
