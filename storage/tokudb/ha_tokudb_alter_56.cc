#if TOKU_INCLUDE_ALTER_56

static bool 
tables_have_same_keys(TABLE* table, TABLE* altered_table, bool print_error, bool check_field_index) {
    bool retval;
    if (table->s->keys != altered_table->s->keys) {
        if (print_error) {
            sql_print_error("tables have different number of keys");
        }
        retval = false;
        goto cleanup;
    }
    if (table->s->primary_key != altered_table->s->primary_key) {
        if (print_error) {
            sql_print_error(
                "Tables have different primary keys, %d %d", 
                table->s->primary_key,
                altered_table->s->primary_key
                );
        }
        retval = false;
        goto cleanup;
    }
    for (u_int32_t i=0; i < table->s->keys; i++) {
        KEY* curr_orig_key = &table->key_info[i];
        KEY* curr_altered_key = &altered_table->key_info[i];
        if (strcmp(curr_orig_key->name, curr_altered_key->name)) {
            if (print_error) {
                sql_print_error(
                    "key %d has different name, %s %s", 
                    i, 
                    curr_orig_key->name,
                    curr_altered_key->name
                    );
            }
            retval = false;
            goto cleanup;
        }
        if (((curr_orig_key->flags & HA_CLUSTERING) == 0) != ((curr_altered_key->flags & HA_CLUSTERING) == 0)) {
            if (print_error) {
                sql_print_error(
                    "keys disagree on if they are clustering, %d, %d",
                    curr_orig_key->key_parts,
                    curr_altered_key->key_parts
                    );
            }
            retval = false;
            goto cleanup;
        }
        if (((curr_orig_key->flags & HA_NOSAME) == 0) != ((curr_altered_key->flags & HA_NOSAME) == 0)) {
            if (print_error) {
                sql_print_error(
                    "keys disagree on if they are unique, %d, %d",
                    curr_orig_key->key_parts,
                    curr_altered_key->key_parts
                    );
            }
            retval = false;
            goto cleanup;
        }
        if (curr_orig_key->key_parts != curr_altered_key->key_parts) {
            if (print_error) {
                sql_print_error(
                    "keys have different number of parts, %d, %d",
                    curr_orig_key->key_parts,
                    curr_altered_key->key_parts
                    );
            }
            retval = false;
            goto cleanup;
        }
        //
        // now verify that each field in the key is the same
        //
        for (u_int32_t j = 0; j < curr_orig_key->key_parts; j++) {
            KEY_PART_INFO* curr_orig_part = &curr_orig_key->key_part[j];
            KEY_PART_INFO* curr_altered_part = &curr_altered_key->key_part[j];
            Field* curr_orig_field = curr_orig_part->field;
            Field* curr_altered_field = curr_altered_part->field;
            if (curr_orig_part->length != curr_altered_part->length) {
                if (print_error) {
                    sql_print_error(
                        "Key %s has different length at index %d", 
                        curr_orig_key->name, 
                        j
                        );
                }
                retval = false;
                goto cleanup;
            }
            bool are_fields_same;
            are_fields_same = (check_field_index) ? 
                (curr_orig_part->fieldnr == curr_altered_part->fieldnr && 
                 fields_are_same_type(curr_orig_field, curr_altered_field)) :
                (are_two_fields_same(curr_orig_field,curr_altered_field));
                
            if (!are_fields_same) {
                if (print_error) {
                    sql_print_error(
                        "Key %s has different field at index %d", 
                        curr_orig_key->name, 
                        j
                        );
                }
                retval = false;
                goto cleanup;
            }
        }
    }

    retval = true;
cleanup:
    return retval;
}

//
// MySQL sets the null_bit as a number that you can bit-wise AND a byte to
// to evaluate whether a field is NULL or not. This value is a power of 2, from
// 2^0 to 2^7. We return the position of the bit within the byte, which is
// lg null_bit
//
static inline u_int32_t 
get_null_bit_position(u_int32_t null_bit) {
    u_int32_t retval = 0;
    switch(null_bit) {
    case (1):
        retval = 0;
        break;
    case (2):
        retval = 1;
        break;
    case (4):
        retval = 2;
        break;
    case (8):
        retval = 3;
        break;
    case (16):
        retval = 4;
        break;
    case (32):
        retval = 5;
        break;
    case (64):
        retval = 6;
        break;
    case (128):
        retval = 7;
        break;        
    default:
        assert(false);
    }
    return retval;
}

//
// returns the index of the null bit of field. 
//
static inline u_int32_t 
get_overall_null_bit_position(TABLE* table, Field* field) {
    u_int32_t offset = get_null_offset(table, field);
    u_int32_t null_bit = field->null_bit;
    return offset*8 + get_null_bit_position(null_bit);
}

#if 0
static bool 
are_null_bits_in_order(TABLE* table) {
    u_int32_t curr_null_pos = 0;
    bool first = true;
    bool retval = true;
    for (uint i = 0; i < table->s->fields; i++) {
        Field* curr_field = table->field[i];
        bool nullable = (curr_field->null_bit != 0);
        if (nullable) {
            u_int32_t pos = get_overall_null_bit_position(
                table,
                curr_field
                );
            if (!first && pos != curr_null_pos+1){
                retval = false;
                break;
            }
            first = false;
            curr_null_pos = pos;
        }
    }
    return retval;
}
#endif

static u_int32_t 
get_first_null_bit_pos(TABLE* table) {
    u_int32_t table_pos = 0;
    for (uint i = 0; i < table->s->fields; i++) {
        Field* curr_field = table->field[i];
        bool nullable = (curr_field->null_bit != 0);
        if (nullable) {
            table_pos = get_overall_null_bit_position(
                table,
                curr_field
                );
            break;
        }
    }
    return table_pos;
}

#if 0
static bool 
is_column_default_null(TABLE* src_table, u_int32_t field_index) {
    Field* curr_field = src_table->field[field_index];
    bool is_null_default = false;
    bool nullable = curr_field->null_bit != 0;
    if (nullable) {
        u_int32_t null_bit_position = get_overall_null_bit_position(src_table, curr_field);
        is_null_default = is_overall_null_position_set(
            src_table->s->default_values,
            null_bit_position
            );
    }
    return is_null_default;
}
#endif

static u_int32_t 
fill_static_row_mutator(
    uchar* buf, 
    TABLE* orig_table,
    TABLE* altered_table,
    KEY_AND_COL_INFO* orig_kc_info,
    KEY_AND_COL_INFO* altered_kc_info,
    u_int32_t keynr
    ) 
{
    //
    // start packing extra
    //
    uchar* pos = buf;
    // says what the operation is
    pos[0] = UP_COL_ADD_OR_DROP;
    pos++;
    
    //
    // null byte information
    //
    memcpy(pos, &orig_table->s->null_bytes, sizeof(orig_table->s->null_bytes));
    pos += sizeof(orig_table->s->null_bytes);
    memcpy(pos, &altered_table->s->null_bytes, sizeof(orig_table->s->null_bytes));
    pos += sizeof(altered_table->s->null_bytes);
    
    //
    // num_offset_bytes
    //
    assert(orig_kc_info->num_offset_bytes <= 2);
    pos[0] = orig_kc_info->num_offset_bytes;
    pos++;
    assert(altered_kc_info->num_offset_bytes <= 2);
    pos[0] = altered_kc_info->num_offset_bytes;
    pos++;
    
    //
    // size of fixed fields
    //
    u_int32_t fixed_field_size = orig_kc_info->mcp_info[keynr].fixed_field_size;
    memcpy(pos, &fixed_field_size, sizeof(fixed_field_size));
    pos += sizeof(fixed_field_size);
    fixed_field_size = altered_kc_info->mcp_info[keynr].fixed_field_size;
    memcpy(pos, &fixed_field_size, sizeof(fixed_field_size));
    pos += sizeof(fixed_field_size);
    
    //
    // length of offsets
    //
    u_int32_t len_of_offsets = orig_kc_info->mcp_info[keynr].len_of_offsets;
    memcpy(pos, &len_of_offsets, sizeof(len_of_offsets));
    pos += sizeof(len_of_offsets);
    len_of_offsets = altered_kc_info->mcp_info[keynr].len_of_offsets;
    memcpy(pos, &len_of_offsets, sizeof(len_of_offsets));
    pos += sizeof(len_of_offsets);

    u_int32_t orig_start_null_pos = get_first_null_bit_pos(orig_table);
    memcpy(pos, &orig_start_null_pos, sizeof(orig_start_null_pos));
    pos += sizeof(orig_start_null_pos);
    u_int32_t altered_start_null_pos = get_first_null_bit_pos(altered_table);
    memcpy(pos, &altered_start_null_pos, sizeof(altered_start_null_pos));
    pos += sizeof(altered_start_null_pos);

    assert((pos-buf) == STATIC_ROW_MUTATOR_SIZE);
    return pos - buf;
}

static u_int32_t 
fill_dynamic_row_mutator(
    uchar* buf,
    u_int32_t* columns, 
    u_int32_t num_columns,
    TABLE* src_table,
    KEY_AND_COL_INFO* src_kc_info,
    u_int32_t keynr,
    bool is_add,
    bool* out_has_blobs
    ) 
{
    uchar* pos = buf;
    bool has_blobs = false;
    u_int32_t cols = num_columns;
    memcpy(pos, &cols, sizeof(cols));
    pos += sizeof(cols);
    for (u_int32_t i = 0; i < num_columns; i++) {
        u_int32_t curr_index = columns[i];
        Field* curr_field = src_table->field[curr_index];
    
        pos[0] = is_add ? COL_ADD : COL_DROP;
        pos++;
        //
        // NULL bit information
        //
        bool is_null_default = false;
        bool nullable = curr_field->null_bit != 0;
        if (!nullable) {
            pos[0] = 0;
            pos++;
        }
        else {
            pos[0] = 1;
            pos++;
            // write position of null byte that is to be removed
            u_int32_t null_bit_position = get_overall_null_bit_position(src_table, curr_field);
            memcpy(pos, &null_bit_position, sizeof(null_bit_position));
            pos += sizeof(null_bit_position);
            //
            // if adding a column, write the value of the default null_bit
            //
            if (is_add) {
                is_null_default = is_overall_null_position_set(
                    src_table->s->default_values,
                    null_bit_position
                    );
                pos[0] = is_null_default ? 1 : 0;
                pos++;
            }
        }
        if (src_kc_info->field_lengths[curr_index] != 0) {
            // we have a fixed field being dropped
            // store the offset and the number of bytes
            pos[0] = COL_FIXED;
            pos++;
            //store the offset
            u_int32_t fixed_field_offset = src_kc_info->cp_info[keynr][curr_index].col_pack_val;
            memcpy(pos, &fixed_field_offset, sizeof(fixed_field_offset));
            pos += sizeof(fixed_field_offset);
            //store the number of bytes
            u_int32_t num_bytes = src_kc_info->field_lengths[curr_index];
            memcpy(pos, &num_bytes, sizeof(num_bytes));
            pos += sizeof(num_bytes);
            if (is_add && !is_null_default) {
                uint curr_field_offset = field_offset(curr_field, src_table);
                memcpy(
                    pos, 
                    src_table->s->default_values + curr_field_offset, 
                    num_bytes
                    );
                pos += num_bytes;
            }
        }
        else if (src_kc_info->length_bytes[curr_index] != 0) {
            pos[0] = COL_VAR;
            pos++;
            //store the index of the variable column
            u_int32_t var_field_index = src_kc_info->cp_info[keynr][curr_index].col_pack_val;
            memcpy(pos, &var_field_index, sizeof(var_field_index));
            pos += sizeof(var_field_index);
            if (is_add && !is_null_default) {
                uint curr_field_offset = field_offset(curr_field, src_table);
                u_int32_t len_bytes = src_kc_info->length_bytes[curr_index];
                u_int32_t data_length = get_var_data_length(
                    src_table->s->default_values + curr_field_offset,
                    len_bytes
                    );
                memcpy(pos, &data_length, sizeof(data_length));
                pos += sizeof(data_length);
                memcpy(
                    pos, 
                    src_table->s->default_values + curr_field_offset + len_bytes,
                    data_length
                    );
                pos += data_length;
            }
        }
        else {
            pos[0] = COL_BLOB;
            pos++;
            has_blobs = true;
        }
    }
    *out_has_blobs = has_blobs;
    return pos-buf;
}

static u_int32_t 
fill_static_blob_row_mutator(
    uchar* buf,
    TABLE* src_table,
    KEY_AND_COL_INFO* src_kc_info
    ) 
{
    uchar* pos = buf;
    // copy number of blobs
    memcpy(pos, &src_kc_info->num_blobs, sizeof(src_kc_info->num_blobs));
    pos += sizeof(src_kc_info->num_blobs);
    // copy length bytes for each blob
    for (u_int32_t i = 0; i < src_kc_info->num_blobs; i++) {
        u_int32_t curr_field_index = src_kc_info->blob_fields[i]; 
        Field* field = src_table->field[curr_field_index];
        u_int32_t len_bytes = field->row_pack_length();
        assert(len_bytes <= 4);
        pos[0] = len_bytes;
        pos++;
    }
    
    return pos-buf;
}

static u_int32_t 
fill_dynamic_blob_row_mutator(
    uchar* buf,
    u_int32_t* columns, 
    u_int32_t num_columns,
    TABLE* src_table,
    KEY_AND_COL_INFO* src_kc_info,
    bool is_add
    ) 
{
    uchar* pos = buf;
    for (u_int32_t i = 0; i < num_columns; i++) {
        u_int32_t curr_field_index = columns[i];
        Field* curr_field = src_table->field[curr_field_index];
        if (src_kc_info->field_lengths[curr_field_index] == 0 && 
            src_kc_info->length_bytes[curr_field_index]== 0
            ) 
        {
            // find out which blob it is
            u_int32_t blob_index = src_kc_info->num_blobs;
            for (u_int32_t j = 0; j < src_kc_info->num_blobs; j++) {
                if (curr_field_index  == src_kc_info->blob_fields[j]) {
                    blob_index = j;
                    break;
                }
            }
            // assert we found blob in list
            assert(blob_index < src_kc_info->num_blobs);
            pos[0] = is_add ? COL_ADD : COL_DROP;
            pos++;
            memcpy(pos, &blob_index, sizeof(blob_index));
            pos += sizeof(blob_index);
            if (is_add) {
                u_int32_t len_bytes = curr_field->row_pack_length();
                assert(len_bytes <= 4);
                pos[0] = len_bytes;
                pos++;

                // create a zero length blob field that can be directly copied in
                // for now, in MySQL, we can only have blob fields 
                // that have no default value
                memset(pos, 0, len_bytes);
                pos += len_bytes;
            }
        }
        else {
            // not a blob, continue
            continue;
        }
    }
    return pos-buf;
}

// TODO: carefully review to make sure that the right information is used
// TODO: namely, when do we get stuff from share->kc_info and when we get
// TODO: it from altered_kc_info, and when is keynr associated with the right thing
u_int32_t 
ha_tokudb::fill_row_mutator(
    uchar* buf, 
    u_int32_t* columns, 
    u_int32_t num_columns,
    TABLE* altered_table,
    KEY_AND_COL_INFO* altered_kc_info,
    u_int32_t keynr,
    bool is_add
    ) 
{
    if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
        printf("*****some info:*************\n");
        printf(
            "old things: num_null_bytes %d, num_offset_bytes %d, fixed_field_size %d, fixed_field_size %d\n",
            table->s->null_bytes,
            share->kc_info.num_offset_bytes,
            share->kc_info.mcp_info[keynr].fixed_field_size,
            share->kc_info.mcp_info[keynr].len_of_offsets
            );
        printf(
            "new things: num_null_bytes %d, num_offset_bytes %d, fixed_field_size %d, fixed_field_size %d\n",
            altered_table->s->null_bytes,
            altered_kc_info->num_offset_bytes,
            altered_kc_info->mcp_info[keynr].fixed_field_size,
            altered_kc_info->mcp_info[keynr].len_of_offsets
            );
        printf("****************************\n");
    }
    uchar* pos = buf;
    bool has_blobs = false;
    pos += fill_static_row_mutator(
        pos,
        table,
        altered_table,
        &share->kc_info,
        altered_kc_info,
        keynr
        );
    
    if (is_add) {
        pos += fill_dynamic_row_mutator(
            pos,
            columns,
            num_columns,
            altered_table,
            altered_kc_info,
            keynr,
            is_add,
            &has_blobs
            );
    }
    else {
        pos += fill_dynamic_row_mutator(
            pos,
            columns,
            num_columns,
            table,
            &share->kc_info,
            keynr,
            is_add,
            &has_blobs
            );
    }
    if (has_blobs) {
        pos += fill_static_blob_row_mutator(
            pos,
            table,
            &share->kc_info
            );
        if (is_add) {
            pos += fill_dynamic_blob_row_mutator(
                pos,
                columns,
                num_columns,
                altered_table,
                altered_kc_info,
                is_add
                );
        }
        else {
            pos += fill_dynamic_blob_row_mutator(
                pos,
                columns,
                num_columns,
                table,
                &share->kc_info,
                is_add
                );
        }
    }
    return pos-buf;
}

static bool 
column_rename_supported(
    Alter_inplace_info *ha_alter_info,
    TABLE* orig_table, 
    TABLE* new_table
    ) 
{
    bool retval = false;
    bool keys_same_for_cr;
    uint num_fields_with_different_names = 0;
    uint field_with_different_name = orig_table->s->fields;
    if (orig_table->s->fields != new_table->s->fields) {
        retval = false;
        goto cleanup;
    }
    if (ha_alter_info->handler_flags & Alter_inplace_info::ALTER_COLUMN_ORDER) {
        retval = false;
        goto cleanup;
    }
    for (uint i = 0; i < orig_table->s->fields; i++) {
        Field* orig_field = orig_table->field[i];
        Field* new_field = new_table->field[i];
        if (!fields_are_same_type(orig_field, new_field)) {
            retval = false;
            goto cleanup;
        }
        if (!fields_have_same_name(orig_field, new_field)) {
            num_fields_with_different_names++;
            field_with_different_name = i;
        }
    }
    // only allow one renamed field
    if (num_fields_with_different_names != 1) {
        retval = false;
        goto cleanup;
    }
    assert(field_with_different_name < orig_table->s->fields);
    //
    // at this point, we have verified that the two tables have
    // the same field types and with ONLY one field with a different name. 
    // We have also identified the field with the different name
    //
    // Now we need to check the indexes
    //
    keys_same_for_cr = tables_have_same_keys(
        orig_table,
        new_table,
        false,
        true
        );
    if (!keys_same_for_cr) {
        retval = false;
        goto cleanup;
    }
    retval = true;
cleanup:
    return retval;
}

static int 
find_changed_columns(
    u_int32_t* changed_columns,
    u_int32_t* num_changed_columns,
    TABLE* smaller_table, 
    TABLE* bigger_table
    ) 
{
    int retval;
    uint curr_new_col_index = 0;
    u_int32_t curr_num_changed_columns=0;
    assert(bigger_table->s->fields > smaller_table->s->fields);
    for (uint i = 0; i < smaller_table->s->fields; i++, curr_new_col_index++) {
        if (curr_new_col_index >= bigger_table->s->fields) {
            sql_print_error("error in determining changed columns");
            retval = 1;
            goto cleanup;
        }
        Field* curr_field_in_new = bigger_table->field[curr_new_col_index];
        Field* curr_field_in_orig = smaller_table->field[i];
        while (!fields_have_same_name(curr_field_in_orig, curr_field_in_new)) {
            changed_columns[curr_num_changed_columns] = curr_new_col_index;
            curr_num_changed_columns++;
            curr_new_col_index++;
            curr_field_in_new = bigger_table->field[curr_new_col_index];
            if (curr_new_col_index >= bigger_table->s->fields) {
                sql_print_error("error in determining changed columns");
                retval = 1;
                goto cleanup;
            }
        }
        // at this point, curr_field_in_orig and curr_field_in_new should be the same, let's verify
        // make sure the two fields that have the same name are ok
        if (!are_two_fields_same(curr_field_in_orig, curr_field_in_new)) {
            sql_print_error(
                "Two fields that were supposedly the same are not: \
                %s in original, %s in new", 
                curr_field_in_orig->field_name,
                curr_field_in_new->field_name
                );
            retval = 1;
            goto cleanup;
        }
    }
    for (uint i = curr_new_col_index; i < bigger_table->s->fields; i++) {
        changed_columns[curr_num_changed_columns] = i;
        curr_num_changed_columns++;
    }
    *num_changed_columns = curr_num_changed_columns;
    retval = 0;
cleanup:
    return retval;
}

class ha_tokudb_add_index_ctx : public inplace_alter_handler_ctx {
public:
    ha_tokudb_add_index_ctx(bool _incremented_num_DBs, bool _modified_DBs) : incremented_num_DBs(_incremented_num_DBs), modified_DBs(_modified_DBs) {
    }
    virtual ~ha_tokudb_add_index_ctx() {
    }
public:
    bool incremented_num_DBs, modified_DBs;
};

// workaround for fill_alter_inplace_info bug (#5193)
// the function erroneously sets the ADD_INDEX and DROP_INDEX flags for a column addition that does not
// change the keys.  the following code turns the ADD_INDEX and DROP_INDEX flags so that we can do hot
// column addition later.
static ulong
fix_handler_flags(Alter_inplace_info *ha_alter_info, TABLE *table, TABLE *altered_table) {
    ulong handler_flags = ha_alter_info->handler_flags;
    if ((handler_flags & (Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::DROP_COLUMN)) != 0) {
        if ((handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::DROP_INDEX)) != 0) {
            if (tables_have_same_keys(table, altered_table, false, false)) {
                handler_flags &= ~(Alter_inplace_info::ADD_INDEX + Alter_inplace_info::DROP_INDEX);
            }
        }
    }
    return handler_flags;
}

// true if some bit in mask is set and no bit in ~mask is set
// otherwise false
static bool
only_flags(ulong bits, ulong mask) {
    return (bits & mask) != 0 && (bits & ~mask) == 0;
}

enum_alter_inplace_result
ha_tokudb::check_if_supported_inplace_alter(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_DBUG_ENTER("check_if_supported_alter");

    if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
        print_alter_info(altered_table, ha_alter_info);
    }

    THD *thd = ha_thd();
    enum_alter_inplace_result result = HA_ALTER_INPLACE_NOT_SUPPORTED; // default is NOT inplace
    HA_CREATE_INFO *create_info = ha_alter_info->create_info;

    ulong handler_flags = fix_handler_flags(ha_alter_info, table, altered_table);

    // column rename
    if (only_flags(handler_flags, Alter_inplace_info::ALTER_COLUMN_NAME + Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        // we have identified a possible column rename, 
        // but let's do some more checks

        // we will only allow an hcr if there are no changes
        // in column positions (ALTER_COLUMN_ORDER is not set)

        // now need to verify that one and only one column
        // has changed only its name. If we find anything to
        // the contrary, we don't allow it, also check indexes
        bool cr_supported = column_rename_supported(ha_alter_info, table, altered_table);
        if (cr_supported)
            result = HA_ALTER_INPLACE_NO_LOCK;
    } else
    // alter auto_increment (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::CHANGE_CREATE_OPTION) && 
        create_info->used_fields == HA_CREATE_USED_AUTO) {
        result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
    } else    
    // alter row_format (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::CHANGE_CREATE_OPTION) && 
        create_info->used_fields == HA_CREATE_USED_ROW_FORMAT) {
        result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
    } else    
    // add index (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX)) {
        assert(ha_alter_info->index_drop_count == 0);
        result = HA_ALTER_INPLACE_SHARED_LOCK;
        // someday, allow multiple hot indexes via alter table add key. don't forget to change the store_lock function.
        // for now, hot indexing is only supported via session variable with the create index sql command
        if (get_create_index_online(thd) && ha_alter_info->index_add_count == 1 && thd_sql_command(thd) == SQLCOM_CREATE_INDEX) 
            result = HA_ALTER_INPLACE_NO_LOCK;
    } else
    // drop index (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX)) {
        assert(ha_alter_info->index_add_count == 0);
        result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;;
    } else
    // add column (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER)) {
        u_int32_t added_columns[altered_table->s->fields];
        u_int32_t num_added_columns = 0;
        int r = find_changed_columns(added_columns, &num_added_columns, table, altered_table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
                for (u_int32_t i = 0; i < num_added_columns; i++) {
                    u_int32_t curr_added_index = added_columns[i];
                    Field* curr_added_field = altered_table->field[curr_added_index];
                    printf("Added column: index %d, name %s\n", curr_added_index, curr_added_field->field_name);
                }
            }
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    // drop column (and nothing else)
    if (only_flags(handler_flags, Alter_inplace_info::DROP_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER)) {
        u_int32_t dropped_columns[table->s->fields];
        u_int32_t num_dropped_columns = 0;
        int r = find_changed_columns(dropped_columns, &num_dropped_columns, altered_table, table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
                for (u_int32_t i = 0; i < num_dropped_columns; i++) {
                    u_int32_t curr_dropped_index = dropped_columns[i];
                    Field* curr_dropped_field = table->field[curr_dropped_index];
                    printf("Dropped column: index %d, name %s\n", curr_dropped_index, curr_dropped_field->field_name);
                }
            }
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    }       

    if (result == HA_ALTER_INPLACE_NOT_SUPPORTED && get_disable_slow_alter(thd)) {
        print_error(HA_ERR_UNSUPPORTED, MYF(0));
        result = HA_ALTER_ERROR;
    }
    
    DBUG_RETURN(result);
}

// prepare not yet called by the mysql 5.5 patch
bool 
ha_tokudb::prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_DBUG_ENTER("prepare_inplace_alter_table");

    bool result = false; // success
    DBUG_RETURN(result);
}

bool 
ha_tokudb::inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_DBUG_ENTER("inplace_alter_table");

    int error = 0;
    HA_CREATE_INFO *create_info = ha_alter_info->create_info;
    ulong handler_flags = fix_handler_flags(ha_alter_info, table, altered_table);

    if (error == 0 && (handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX))) {
        error = alter_table_add_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & (Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX))) {
        error = alter_table_drop_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::ADD_COLUMN)) { 
        error = alter_table_add_or_drop_column(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::DROP_COLUMN)) {
        error = alter_table_add_or_drop_column(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) &&
        (create_info->used_fields & HA_CREATE_USED_AUTO)) {
        error = write_auto_inc_create(share->status_block, create_info->auto_increment_value, transaction);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) &&
        (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT)) {

        enum toku_compression_method method = TOKU_NO_COMPRESSION;
        method = row_type_to_compression_method(create_info->row_type);

        // Set the new type.
        u_int32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);
        for (u_int32_t i = 0; i < curr_num_DBs; ++i) {
            DB *db = share->key_file[i];
            error = db->change_compression_method(db, method);
            if (error) 
                break;
        }
    }

    bool result = false; // success
    if (error) {
        print_error(error, MYF(0));
        result = true;  // failure
    }

    DBUG_RETURN(result);
}

int
ha_tokudb::alter_table_add_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {

    // sort keys in add index order
    KEY *key_info = (KEY*) my_malloc(sizeof (KEY) * ha_alter_info->index_add_count, MYF(MY_WME));
    for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
        KEY *key = &key_info[i];
        *key = ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[i]];
        for (KEY_PART_INFO *key_part= key->key_part; key_part < key->key_part + key->key_parts; key_part++)
            key_part->field = table->field[key_part->fieldnr];
    }

    bool incremented_num_DBs = false;
    bool modified_DBs = false;
    int error = tokudb_add_index(table, key_info, ha_alter_info->index_add_count, transaction, &incremented_num_DBs, &modified_DBs);
    if (error == HA_ERR_FOUND_DUPP_KEY) {
        // hack for now, in case of duplicate key error, 
        // because at the moment we cannot display the right key
        // information to the user, so that he knows potentially what went
        // wrong.
        last_dup_key = MAX_KEY;
    }

    assert(ha_alter_info->handler_ctx == NULL);
    ha_alter_info->handler_ctx = new ha_tokudb_add_index_ctx(incremented_num_DBs, modified_DBs);
    assert(ha_alter_info->handler_ctx);
    
    my_free(key_info);

    return error;
}

int
ha_tokudb::alter_table_drop_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    // translate KEY pointers to indexes into the key_info array
    uint index_drop_offsets[ha_alter_info->index_drop_count];
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++)
        index_drop_offsets[i] = ha_alter_info->index_drop_buffer[i] - table->key_info;
    
    // drop indexes
    int error = drop_indexes(table, index_drop_offsets, ha_alter_info->index_drop_count, transaction);

    return error;
}

int
ha_tokudb::alter_table_add_or_drop_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    int error;
    uchar *column_extra = NULL;
    uchar *row_desc_buff = NULL;
    u_int32_t max_new_desc_size = 0;
    u_int32_t max_column_extra_size;
    u_int32_t num_column_extra;
    u_int32_t num_columns = 0;
    u_int32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);

    u_int32_t columns[table->s->fields + altered_table->s->fields]; // set size such that we know it is big enough for both cases
    memset(columns, 0, sizeof(columns));

    KEY_AND_COL_INFO altered_kc_info;
    memset(&altered_kc_info, 0, sizeof(altered_kc_info));

    error = allocate_key_and_col_info(altered_table->s, &altered_kc_info);
    if (error) { goto cleanup; }

    max_new_desc_size = get_max_desc_size(&altered_kc_info, altered_table);
    row_desc_buff = (uchar *)my_malloc(max_new_desc_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}

    error = initialize_key_and_col_info(
                                        altered_table->s, 
                                        altered_table,
                                        &altered_kc_info,
                                        hidden_primary_key,
                                        primary_key
                                        );
    if (error) { goto cleanup; }

    // generate the array of columns
    if (ha_alter_info->handler_flags & Alter_inplace_info::DROP_COLUMN) {
        find_changed_columns(
                             columns,
                             &num_columns,
                             altered_table,
                             table
                             );
    } else
    if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_COLUMN) {
        find_changed_columns(
                             columns,
                             &num_columns,
                             table,
                             altered_table
                             );
    } else
        assert(0);
    max_column_extra_size = 
        STATIC_ROW_MUTATOR_SIZE + //max static row_mutator
        4 + num_columns*(1+1+4+1+1+4) + altered_table->s->reclength + // max dynamic row_mutator
        (4 + share->kc_info.num_blobs) + // max static blob size
        (num_columns*(1+4+1+4)); // max dynamic blob size
    column_extra = (uchar *)my_malloc(max_column_extra_size, MYF(MY_WME));
    if (column_extra == NULL) { error = ENOMEM; goto cleanup; }
    
    for (u_int32_t i = 0; i < curr_num_DBs; i++) {
        DBT row_descriptor;
        memset(&row_descriptor, 0, sizeof(row_descriptor));
        KEY* prim_key = (hidden_primary_key) ? NULL : &altered_table->s->key_info[primary_key];
        KEY* key_info = &altered_table->key_info[i];
        if (i == primary_key) {
            row_descriptor.size = create_main_key_descriptor(
                                                             row_desc_buff,
                                                             prim_key,
                                                             hidden_primary_key,
                                                             primary_key,
                                                             altered_table,
                                                             &altered_kc_info
                                                             );
            row_descriptor.data = row_desc_buff;
        }
        else {
            row_descriptor.size = create_secondary_key_descriptor(
                                                                  row_desc_buff,
                                                                  key_info,
                                                                  prim_key,
                                                                  hidden_primary_key,
                                                                  altered_table,
                                                                  primary_key,
                                                                  i,
                                                                  &altered_kc_info
                                                                  );
            row_descriptor.data = row_desc_buff;
        }
        error = share->key_file[i]->change_descriptor(
                                                      share->key_file[i],
                                                      transaction,
                                                      &row_descriptor,
                                                      0
                                                      );
        if (error) { goto cleanup; }
        
        if (i == primary_key || table_share->key_info[i].flags & HA_CLUSTERING) {
            num_column_extra = fill_row_mutator(
                                                column_extra,
                                                columns,
                                                num_columns,
                                                altered_table,
                                                &altered_kc_info,
                                                i,
                                                (ha_alter_info->handler_flags & Alter_inplace_info::ADD_COLUMN) != 0 // true if adding columns, otherwise is a drop
                                                );
            
            DBT column_dbt;
            memset(&column_dbt, 0, sizeof column_dbt);
            column_dbt.data = column_extra; 
            column_dbt.size = num_column_extra;
            DBUG_ASSERT(num_column_extra <= max_column_extra_size);            
            error = share->key_file[i]->update_broadcast(
                                                         share->key_file[i],
                                                         transaction,
                                                         &column_dbt,
                                                         DB_IS_RESETTING_OP
                                                         );
            if (error) { goto cleanup; }
        }
    }

    error = 0;
 cleanup:
    free_key_and_col_info(&altered_kc_info);
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(column_extra, MYF(MY_ALLOW_ZERO_PTR));
    return error;
}

bool 
ha_tokudb::commit_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit) {
    TOKUDB_DBUG_ENTER("commit_inplace_alter_table");

    bool result = false; // success
    ulong handler_flags = fix_handler_flags(ha_alter_info, table, altered_table);

    if (commit) {
        if (altered_table->part_info == NULL) {
            // read frmdata for the altered table
            uchar *frm_data; size_t frm_len;
            int error = readfrm(altered_table->s->path.str, &frm_data, &frm_len);
            if (error) {
                result = true;
            } else {
                // transactionally write frmdata to status
                assert(transaction);
                error = write_to_status(share->status_block, hatoku_frm_data, (void *)frm_data, (uint)frm_len, transaction);
                if (error) {
                    result = true;
                }                           

                my_free(frm_data);
            }
            if (error)
                print_error(error, MYF(0));
        }
    }

    if (!commit || result == true) {
        if (handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX)) {
            ha_tokudb_add_index_ctx *ctx = static_cast<ha_tokudb_add_index_ctx *>(ha_alter_info->handler_ctx);
            assert(ctx);
            restore_add_index(table, ha_alter_info->index_add_count, ctx->incremented_num_DBs, ctx->modified_DBs);
        }
        if (handler_flags & (Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX)) {
            // translate KEY pointers to indexes into the key_info array
            uint index_drop_offsets[ha_alter_info->index_drop_count];
            for (uint i = 0; i < ha_alter_info->index_drop_count; i++)
                index_drop_offsets[i] = ha_alter_info->index_drop_buffer[i] - table->key_info;
            
            restore_drop_indexes(table, index_drop_offsets, ha_alter_info->index_drop_count);
        }
        // TODO undo auto increment, row format change
    }
    
    DBUG_RETURN(result);
}

void 
ha_tokudb::print_alter_info(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    printf("***are keys of two tables same? %d\n", tables_have_same_keys(table, altered_table, false, false));
    if (ha_alter_info->handler_flags) {
        printf("***alter flags set ***\n");
        for (int i = 0; i < 32; i++) {
            if (ha_alter_info->handler_flags & (1 << i))
                printf("%d\n", i);
        }
    }

    // everyone calculates data by doing some default_values - record[0], but I do not see why
    // that is necessary
    printf("******\n");
    printf("***orig table***\n");
    for (uint i = 0; i < table->s->fields; i++) {
      //
      // make sure to use table->field, and NOT table->s->field
      //
      Field* curr_field = table->field[i];
      uint null_offset = get_null_offset(table, curr_field);
      printf(
          "name: %s, nullable: %d, null_offset: %d, is_null_field: %d, is_null %d, \n", 
          curr_field->field_name, 
          curr_field->null_bit,
          null_offset,
          (curr_field->null_ptr != NULL),
          (curr_field->null_ptr != NULL) ? table->s->default_values[null_offset] & curr_field->null_bit : 0xffffffff
          );
    }
    printf("******\n");
    printf("***altered table***\n");
    for (uint i = 0; i < altered_table->s->fields; i++) {
      Field* curr_field = altered_table->field[i];
      uint null_offset = get_null_offset(altered_table, curr_field);
      printf(
         "name: %s, nullable: %d, null_offset: %d, is_null_field: %d, is_null %d, \n", 
         curr_field->field_name, 
         curr_field->null_bit,
         null_offset,
         (curr_field->null_ptr != NULL),
         (curr_field->null_ptr != NULL) ? altered_table->s->default_values[null_offset] & curr_field->null_bit : 0xffffffff
         );
    }
    printf("******\n");
}

#endif
