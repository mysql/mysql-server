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
    for (uint32_t i=0; i < table->s->keys; i++) {
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
        for (uint32_t j = 0; j < curr_orig_key->key_parts; j++) {
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

// MySQL sets the null_bit as a number that you can bit-wise AND a byte to
// to evaluate whether a field is NULL or not. This value is a power of 2, from
// 2^0 to 2^7. We return the position of the bit within the byte, which is
// lg null_bit
static inline uint32_t 
get_null_bit_position(uint32_t null_bit) {
    uint32_t retval = 0;
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

// returns the index of the null bit of field. 
static inline uint32_t 
get_overall_null_bit_position(TABLE* table, Field* field) {
    uint32_t offset = get_null_offset(table, field);
    uint32_t null_bit = field->null_bit;
    return offset*8 + get_null_bit_position(null_bit);
}

// not static since 51 uses this and 56 does not
bool 
are_null_bits_in_order(TABLE* table) {
    uint32_t curr_null_pos = 0;
    bool first = true;
    bool retval = true;
    for (uint i = 0; i < table->s->fields; i++) {
        Field* curr_field = table->field[i];
        bool nullable = (curr_field->null_bit != 0);
        if (nullable) {
            uint32_t pos = get_overall_null_bit_position(
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

static uint32_t 
get_first_null_bit_pos(TABLE* table) {
    uint32_t table_pos = 0;
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
is_column_default_null(TABLE* src_table, uint32_t field_index) {
    Field* curr_field = src_table->field[field_index];
    bool is_null_default = false;
    bool nullable = curr_field->null_bit != 0;
    if (nullable) {
        uint32_t null_bit_position = get_overall_null_bit_position(src_table, curr_field);
        is_null_default = is_overall_null_position_set(
            src_table->s->default_values,
            null_bit_position
            );
    }
    return is_null_default;
}
#endif

static uint32_t 
fill_static_row_mutator(
    uchar* buf, 
    TABLE* orig_table,
    TABLE* altered_table,
    KEY_AND_COL_INFO* orig_kc_info,
    KEY_AND_COL_INFO* altered_kc_info,
    uint32_t keynr
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
    uint32_t fixed_field_size = orig_kc_info->mcp_info[keynr].fixed_field_size;
    memcpy(pos, &fixed_field_size, sizeof(fixed_field_size));
    pos += sizeof(fixed_field_size);
    fixed_field_size = altered_kc_info->mcp_info[keynr].fixed_field_size;
    memcpy(pos, &fixed_field_size, sizeof(fixed_field_size));
    pos += sizeof(fixed_field_size);
    
    //
    // length of offsets
    //
    uint32_t len_of_offsets = orig_kc_info->mcp_info[keynr].len_of_offsets;
    memcpy(pos, &len_of_offsets, sizeof(len_of_offsets));
    pos += sizeof(len_of_offsets);
    len_of_offsets = altered_kc_info->mcp_info[keynr].len_of_offsets;
    memcpy(pos, &len_of_offsets, sizeof(len_of_offsets));
    pos += sizeof(len_of_offsets);

    uint32_t orig_start_null_pos = get_first_null_bit_pos(orig_table);
    memcpy(pos, &orig_start_null_pos, sizeof(orig_start_null_pos));
    pos += sizeof(orig_start_null_pos);
    uint32_t altered_start_null_pos = get_first_null_bit_pos(altered_table);
    memcpy(pos, &altered_start_null_pos, sizeof(altered_start_null_pos));
    pos += sizeof(altered_start_null_pos);

    assert((pos-buf) == STATIC_ROW_MUTATOR_SIZE);
    return pos - buf;
}

static uint32_t 
fill_dynamic_row_mutator(
    uchar* buf,
    uint32_t* columns, 
    uint32_t num_columns,
    TABLE* src_table,
    KEY_AND_COL_INFO* src_kc_info,
    uint32_t keynr,
    bool is_add,
    bool* out_has_blobs
    ) 
{
    uchar* pos = buf;
    bool has_blobs = false;
    uint32_t cols = num_columns;
    memcpy(pos, &cols, sizeof(cols));
    pos += sizeof(cols);
    for (uint32_t i = 0; i < num_columns; i++) {
        uint32_t curr_index = columns[i];
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
            uint32_t null_bit_position = get_overall_null_bit_position(src_table, curr_field);
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
            uint32_t fixed_field_offset = src_kc_info->cp_info[keynr][curr_index].col_pack_val;
            memcpy(pos, &fixed_field_offset, sizeof(fixed_field_offset));
            pos += sizeof(fixed_field_offset);
            //store the number of bytes
            uint32_t num_bytes = src_kc_info->field_lengths[curr_index];
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
            uint32_t var_field_index = src_kc_info->cp_info[keynr][curr_index].col_pack_val;
            memcpy(pos, &var_field_index, sizeof(var_field_index));
            pos += sizeof(var_field_index);
            if (is_add && !is_null_default) {
                uint curr_field_offset = field_offset(curr_field, src_table);
                uint32_t len_bytes = src_kc_info->length_bytes[curr_index];
                uint32_t data_length = get_var_data_length(
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

static uint32_t 
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
    for (uint32_t i = 0; i < src_kc_info->num_blobs; i++) {
        uint32_t curr_field_index = src_kc_info->blob_fields[i]; 
        Field* field = src_table->field[curr_field_index];
        uint32_t len_bytes = field->row_pack_length();
        assert(len_bytes <= 4);
        pos[0] = len_bytes;
        pos++;
    }
    
    return pos-buf;
}

static uint32_t 
fill_dynamic_blob_row_mutator(
    uchar* buf,
    uint32_t* columns, 
    uint32_t num_columns,
    TABLE* src_table,
    KEY_AND_COL_INFO* src_kc_info,
    bool is_add
    ) 
{
    uchar* pos = buf;
    for (uint32_t i = 0; i < num_columns; i++) {
        uint32_t curr_field_index = columns[i];
        Field* curr_field = src_table->field[curr_field_index];
        if (src_kc_info->field_lengths[curr_field_index] == 0 && 
            src_kc_info->length_bytes[curr_field_index]== 0
            ) 
        {
            // find out which blob it is
            uint32_t blob_index = src_kc_info->num_blobs;
            for (uint32_t j = 0; j < src_kc_info->num_blobs; j++) {
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
                uint32_t len_bytes = curr_field->row_pack_length();
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
uint32_t 
ha_tokudb::fill_row_mutator(
    uchar* buf, 
    uint32_t* columns, 
    uint32_t num_columns,
    TABLE* altered_table,
    KEY_AND_COL_INFO* altered_kc_info,
    uint32_t keynr,
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
all_fields_are_same_type(TABLE *table_a, TABLE *table_b) {
    if (table_a->s->fields != table_b->s->fields)
        return false;
    for (uint i = 0; i < table_a->s->fields; i++) {
        Field *field_a = table_a->field[i];
        Field *field_b = table_b->field[i];
        if (!fields_are_same_type(field_a, field_b))
            return false;
    }
    return true;
}

static bool 
column_rename_supported(
    TABLE* orig_table, 
    TABLE* new_table,
    bool alter_column_order
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
    if (alter_column_order) {
        retval = false;
        goto cleanup;
    }
    if (!all_fields_are_same_type(orig_table, new_table)) {
        retval = false;
        goto cleanup;
    }
    for (uint i = 0; i < orig_table->s->fields; i++) {
        Field* orig_field = orig_table->field[i];
        Field* new_field = new_table->field[i];
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
    uint32_t* changed_columns,
    uint32_t* num_changed_columns,
    TABLE* smaller_table, 
    TABLE* bigger_table
    ) 
{
    int retval;
    uint curr_new_col_index = 0;
    uint32_t curr_num_changed_columns=0;
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

static bool
tables_have_same_keys_and_columns(TABLE* first_table, TABLE* second_table, bool print_error) {
    bool retval;
    if (first_table->s->null_bytes != second_table->s->null_bytes) {
        retval = false;
        if (print_error) {
            sql_print_error(
                "tables have different number of null bytes, %d, %d", 
                first_table->s->null_bytes, 
                second_table->s->null_bytes
                );
        }
        goto exit;
    }
    if (first_table->s->fields != second_table->s->fields) {
        retval = false;
        if (print_error) {
            sql_print_error(
                "tables have different number of fields, %d, %d", 
                first_table->s->fields, 
                second_table->s->fields
                );
        }
        goto exit;
    }
    for (uint i = 0; i < first_table->s->fields; i++) {
        Field* a = first_table->field[i];
        Field* b = second_table->field[i];
        if (!are_two_fields_same(a,b)) {
            retval = false;
            sql_print_error(
                "tables have different fields at position %d", 
                i
                );
            goto exit;
        }
    }
    if (!tables_have_same_keys(first_table, second_table, print_error, true)) {
        retval = false;
        goto exit;
    }

    retval = true;
exit:
    return retval;
}

#if TOKU_INCLUDE_WRITE_FRM_DATA
// write the new frm data to the status dictionary using the alter table transaction
int 
ha_tokudb::write_frm_data(const uchar *frm_data, size_t frm_len) {
    TOKUDB_DBUG_ENTER("write_frm_data");

    int error = 0;
    if (TOKU_PARTITION_WRITE_FRM_DATA || table->part_info == NULL) {
        // write frmdata to status
        THD *thd = ha_thd();
        tokudb_trx_data *trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);
        assert(trx);
        DB_TXN *txn = trx->stmt; // use alter table transaction
        assert(txn);
        error = write_to_status(share->status_block, hatoku_frm_data, (void *)frm_data, (uint)frm_len, txn);
    }
   
    TOKUDB_DBUG_RETURN(error);
}
#endif
