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

// The expand varchar offsets message is used to expand the size of an offset from 1 to 2 bytes.
// operation     1  == UPDATE_OP_EXPAND_VARIABLE_OFFSETS
// n_offsets     4  number of offsets
// offset_start  4  starting offset of the variable length field offsets 

// These expand messages are used to expand the size of a fixed length field.  
// The field type is encoded in the operation code.
// operation     1  == UPDATE_OP_EXPAND_INT, UPDATE_OP_EXPAND_UINT, UPDATE_OP_EXPAND_CHAR, UPDATE_OP_EXPAND_BINARY
// offset        4 starting offset of the field in the row's value
// old length    4 the old length of the field's value
// new length    4 the new length of the field's value

// operation     1  == UPDATE_OP_EXPAND_CHAR, UPDATE_OP_EXPAND_BINARY
// offset        4 starting offset of the field in the row's value
// old length    4 the old length of the field's value
// new length    4 the new length of the field's value
// pad char      1

// The int add and sub update messages are used to add or subtract a constant to or from an integer field.
// operation     1 == UPDATE_OP_INT_ADD, UPDATE_OP_INT_SUB, UPDATE_OP_UINT_ADD, UPDATE_OP_UINT_SUB
// offset        4 starting offset of the int type field
// length        4 length of the int type field
// value         4 value to add or subtract (common use case is increment or decrement by 1)

//
// checks whether the bit at index pos in data is set or not
//
static inline bool 
is_overall_null_position_set(uchar* data, uint32_t pos) {
    uint32_t offset = pos/8;
    uchar remainder = pos%8; 
    uchar null_bit = 1<<remainder;
    return ((data[offset] & null_bit) != 0);
}

//
// sets the bit at index pos in data to 1 if is_null, 0 otherwise
// 
static inline void 
set_overall_null_position(uchar* data, uint32_t pos, bool is_null) {
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

static inline void 
copy_null_bits(
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

static inline void 
copy_var_fields(
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

static inline uint32_t 
copy_toku_blob(uchar* to_ptr, uchar* from_ptr, uint32_t len_bytes, bool skip) {
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

static int 
tokudb_hcad_update_fun(
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
    new_val_data = (uchar *)my_malloc(
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
    my_free(new_val_data, MYF(MY_ALLOW_ZERO_PTR));
    return error;    
}

// Expand the variable offset array in the old row given the update mesage in the extra.
static int 
tokudb_expand_variable_offsets(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    uchar *extra_pos = (uchar *)extra->data;

    // decode the operation
    uchar operation = extra_pos[0];
    assert(operation == UPDATE_OP_EXPAND_VARIABLE_OFFSETS);
    extra_pos += sizeof operation;

    // decode number of offsets
    uint32_t number_of_offsets;
    memcpy(&number_of_offsets, extra_pos, sizeof number_of_offsets);
    extra_pos += sizeof number_of_offsets;

    // decode the offset start
    uint32_t offset_start;
    memcpy(&offset_start, extra_pos, sizeof offset_start);
    extra_pos += sizeof offset_start;

    assert(extra_pos == (uchar *)extra->data + extra->size);
    assert(offset_start + number_of_offsets < old_val->size);

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        // compute the new val from the old val

        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)my_malloc(number_of_offsets + old_val->size, MYF(MY_FAE));
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
    my_free(new_val.data, MYF(MY_ALLOW_ZERO_PTR));        

    return error;
}

// Expand an int field in a old row given the expand message in the extra.
static int 
tokudb_expand_int_field(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    uchar *extra_pos = (uchar *)extra->data;

    uchar operation = extra_pos[0];
    assert(operation == UPDATE_OP_EXPAND_INT || operation == UPDATE_OP_EXPAND_UINT);
    extra_pos += sizeof operation;

    uint32_t the_offset;
    memcpy(&the_offset, extra_pos, sizeof the_offset);
    extra_pos += sizeof the_offset;

    uint32_t old_length;
    memcpy(&old_length, extra_pos, sizeof old_length);
    extra_pos += sizeof old_length;

    uint32_t new_length;
    memcpy(&new_length, extra_pos, sizeof new_length);
    extra_pos += sizeof new_length;

    assert(extra_pos == (uchar *)extra->data + extra->size); // consumed the entire message
    assert(new_length >= old_length); // expand only
    assert(the_offset + old_length <= old_val->size); // old field within the old val

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        // compute the new val from the old val
        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)my_malloc(old_val->size + (new_length - old_length), MYF(MY_FAE));
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
    my_free(new_val.data, MYF(MY_ALLOW_ZERO_PTR));        

    return error;
}

// Expand a char field in a old row given the expand message in the extra.
static int 
tokudb_expand_char_field(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    int error = 0;
    uchar *extra_pos = (uchar *)extra->data;

    uchar operation = extra_pos[0];
    assert(operation == UPDATE_OP_EXPAND_CHAR || operation == UPDATE_OP_EXPAND_BINARY);
    extra_pos += sizeof operation;

    uint32_t the_offset;
    memcpy(&the_offset, extra_pos, sizeof the_offset);
    extra_pos += sizeof the_offset;

    uint32_t old_length;
    memcpy(&old_length, extra_pos, sizeof old_length);
    extra_pos += sizeof old_length;

    uint32_t new_length;
    memcpy(&new_length, extra_pos, sizeof new_length);
    extra_pos += sizeof new_length;

    uchar pad_char = 0;
    memcpy(&pad_char, extra_pos, sizeof pad_char);
    extra_pos += sizeof pad_char;

    assert(extra_pos == (uchar *)extra->data + extra->size); // consumed the entire message
    assert(new_length >= old_length); // expand only
    assert(the_offset + old_length <= old_val->size); // old field within the old val

    DBT new_val; memset(&new_val, 0, sizeof new_val);

    if (old_val != NULL) {
        // compute the new val from the old val
        uchar *old_val_ptr = (uchar *)old_val->data;

        // allocate space for the new val's data
        uchar *new_val_ptr = (uchar *)my_malloc(old_val->size + (new_length - old_length), MYF(MY_FAE));
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
    my_free(new_val.data, MYF(MY_ALLOW_ZERO_PTR));        

    return error;
}

int 
tokudb_update_fun(
    DB* db,
    const DBT *key,
    const DBT *old_val, 
    const DBT *extra,
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra
    ) 
{
    uchar *extra_pos = (uchar *)extra->data;
    uchar operation = extra_pos[0];
    int error = 0;
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
    default:
        error = EINVAL;
        break;
    }
    return error;
}
