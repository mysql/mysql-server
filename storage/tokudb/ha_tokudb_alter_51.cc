#if TOKU_INCLUDE_ALTER_51

volatile int ha_tokudb_add_index_wait = 0;

int 
ha_tokudb::add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::add_index");
    DB_TXN* txn = NULL;
    int error;
    bool incremented_numDBs = false;
    bool modified_DBs = false;
    
    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }

    error = tokudb_add_index(
        table_arg,
        key_info,
        num_of_keys,
        txn,
        &incremented_numDBs,
        &modified_DBs
        );
    if (error) { goto cleanup; }
    
cleanup:
    if (error) {
        if (txn) {
            restore_add_index(table_arg, num_of_keys, incremented_numDBs, modified_DBs);
            abort_txn(txn);
        }
    }
    else {
      commit_txn(txn, 0);
    }
    TOKUDB_DBUG_RETURN(error);
}

volatile int ha_tokudb_prepare_drop_index_wait = 0; //debug

//
// Prepares to drop indexes to the table. For each value, i, in the array key_num,
// table->key_info[i] is a key that is to be dropped.
//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments
// Parameters:
//      [in]    table_arg - table that is being modified, seems to be identical to this->table
//      [in]    key_num - array of indexes that specify which keys of the array table->key_info
//                  are to be dropped
//              num_of_keys - size of array, key_num
//  Returns:
//      0 on success, error otherwise
//
int 
ha_tokudb::prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys) {
    TOKUDB_DBUG_ENTER("ha_tokudb::prepare_drop_index");
    while (ha_tokudb_prepare_drop_index_wait) sleep(1); // debug

    int error;
    DB_TXN* txn = NULL;

    error = db_env->txn_begin(db_env, 0, &txn, 0);
    if (error) { goto cleanup; }
    
    error = drop_indexes(table_arg, key_num, num_of_keys, table_arg->key_info, txn);
    if (error) { goto cleanup; }

cleanup:
    if (txn) {
        if (error) {
            abort_txn(txn);
            restore_drop_indexes(table_arg, key_num, num_of_keys);
        }
        else {
            commit_txn(txn,0);
        }
    }

    TOKUDB_DBUG_RETURN(error);
}

volatile int ha_tokudb_final_drop_index_wait = 0; // debug

//  ***********NOTE*******************
// Although prepare_drop_index is supposed to just get the DB's ready for removal,
// and not actually do the removal, we are doing it here and not in final_drop_index
// For the flags we expose in alter_table_flags, namely xxx_NO_WRITES, this is allowed
// Changes for "future-proofing" this so that it works when we have the equivalent flags
// that are not NO_WRITES are not worth it at the moments, therefore, we can make
// this function just return
int
ha_tokudb::final_drop_index(TABLE *table_arg) {
    TOKUDB_DBUG_ENTER("ha_tokudb::final_drop_index");
    while (ha_tokudb_final_drop_index_wait) sleep(1); // debug

    int error = 0;
    DBUG_EXECUTE_IF("final_drop_index_fail", {
        error = 1;
    });
    TOKUDB_DBUG_RETURN(error);
}

#if defined(HA_GENERAL_ONLINE)

#include "ha_tokudb_alter_common.cc"

void 
ha_tokudb::print_alter_info(
    TABLE *altered_table,
    HA_CREATE_INFO *create_info,
    HA_ALTER_FLAGS *alter_flags,
    uint table_changes
    )
{
    printf("***are keys of two tables same? %d\n", tables_have_same_keys(table,altered_table,false, false));
    printf("***alter flags set ***\n");
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
      if (alter_flags->is_set(i)) {
        printf("flag: %d\n", i);
      }
    }
    //
    // everyone calculates data by doing some default_values - record[0], but I do not see why
    // that is necessary
    //
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

static bool 
alter_has_other_flag_set(HA_ALTER_FLAGS* alter_flags, uint flag) {
    bool retval = false;
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == flag)
        {
            continue;
        }
        if (alter_flags->is_set(i)) {
            retval = true;
            break;
        }
    }
    return retval;
}

int 
ha_tokudb::check_if_supported_alter(TABLE *altered_table,
    HA_CREATE_INFO *create_info,
    HA_ALTER_FLAGS *alter_flags,
    HA_ALTER_INFO  *alter_info,
    uint table_changes)
{
    TOKUDB_DBUG_ENTER("check_if_supported_alter");
    int retval;
    THD* thd = ha_thd(); 
    bool keys_same = tables_have_same_keys(table,altered_table, false, false);


    if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
      printf("has after or first %d\n", alter_info->contains_first_or_after);
        print_alter_info(altered_table, create_info, alter_flags, table_changes);
    }
    bool has_added_columns = alter_flags->is_set(HA_ADD_COLUMN);
    bool has_dropped_columns = alter_flags->is_set(HA_DROP_COLUMN);
    bool has_column_rename = alter_flags->is_set(HA_CHANGE_COLUMN) && 
                             alter_flags->is_set(HA_ALTER_COLUMN_NAME);
    bool has_auto_inc_change = alter_flags->is_set(HA_CHANGE_AUTOINCREMENT_VALUE);
    //
    // We do not check for changes to foreign keys or primary keys. They are not supported
    // Changing the primary key implies changing keys in all dictionaries. that is why we don't
    // try to make it fast
    //
    bool has_indexing_changes = alter_flags->is_set(HA_DROP_INDEX) || 
                                alter_flags->is_set(HA_DROP_UNIQUE_INDEX) ||
                                alter_flags->is_set(HA_ADD_INDEX) ||
                                alter_flags->is_set(HA_ADD_UNIQUE_INDEX);
    
    // Check if the row format (read: compression) has 
    // changed as part of this alter statment.
    bool has_row_format_changes = alter_flags->is_set(HA_ALTER_ROW_FORMAT);
    bool has_non_indexing_changes = false;
    bool has_non_dropped_changes = false;
    bool has_non_added_changes = false;
    bool has_non_column_rename_changes = false;
    bool has_non_row_format_changes = false;
    bool has_non_auto_inc_change = alter_has_other_flag_set(alter_flags, HA_CHANGE_AUTOINCREMENT_VALUE);

    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == HA_DROP_INDEX ||
            i == HA_DROP_UNIQUE_INDEX ||
            i == HA_ADD_INDEX ||
            i == HA_ADD_UNIQUE_INDEX)
        {
            continue;
        }
        if (alter_flags->is_set(i)) {
            has_non_indexing_changes = true;
            break;
        }
    }
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == HA_ALTER_COLUMN_NAME||
            i == HA_CHANGE_COLUMN)
        {
            continue;
        }
        if (alter_flags->is_set(i)) {
            has_non_column_rename_changes = true;
            break;
        }
    }
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == HA_DROP_COLUMN) {
            continue;
        }
        if (keys_same && 
            (i == HA_ALTER_INDEX || i == HA_ALTER_UNIQUE_INDEX || i == HA_ALTER_PK_INDEX)) {
            continue;
        }
        if (alter_flags->is_set(i)) {
            has_non_dropped_changes = true;
            break;
        }
    }
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == HA_ADD_COLUMN) {
            continue;
        }
        if (keys_same && 
            (i == HA_ALTER_INDEX || i == HA_ALTER_UNIQUE_INDEX || i == HA_ALTER_PK_INDEX)) {
            continue;
        }
        if (alter_flags->is_set(i)) {
            has_non_added_changes = true;
            break;
        }
    }
    // See if any flags other than row format have been set.
    for (uint i = 0; i < HA_MAX_ALTER_FLAGS; i++) {
        if (i == HA_ALTER_ROW_FORMAT) {
            continue;
        }
        if (alter_flags->is_set(i)) {
            has_non_row_format_changes = true;
            break;
        }
    }

    if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
        printf("has indexing changes %d, has non indexing changes %d\n", has_indexing_changes, has_non_indexing_changes);
    }
#ifdef MARIADB_BASE_VERSION
#if MYSQL_VERSION_ID >= 50203
    if (table->s->vfields || altered_table->s->vfields) {
      retval = HA_ALTER_ERROR;
      goto cleanup;
    }
#endif
#endif
    if (table->s->tmp_table != NO_TMP_TABLE) {
      retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
      goto cleanup;
    }
    if (!(are_null_bits_in_order(table) && 
          are_null_bits_in_order(altered_table)
          )
       ) 
    {
        sql_print_error("Problems parsing null bits of the original and altered table");
        retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
        goto cleanup;
    }
    if (has_added_columns && !has_non_added_changes) {
        uint32_t added_columns[altered_table->s->fields];
        uint32_t num_added_columns = 0;
        int r = find_changed_columns(
            added_columns,
            &num_added_columns,
            table,
            altered_table
            );
        if (r) {
            retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
            goto cleanup;
        }
        if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
            for (uint32_t i = 0; i < num_added_columns; i++) {
                uint32_t curr_added_index = added_columns[i];
                Field* curr_added_field = altered_table->field[curr_added_index];
                printf(
                    "Added column: index %d, name %s\n", 
                    curr_added_index, 
                    curr_added_field->field_name
                    );
            }
        }
    }
    if (has_dropped_columns && !has_non_dropped_changes) {
        uint32_t dropped_columns[table->s->fields];
        uint32_t num_dropped_columns = 0;
        int r = find_changed_columns(
            dropped_columns,
            &num_dropped_columns,
            altered_table,
            table
            );
        if (r) {
            retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
            goto cleanup;
        }
        if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
            for (uint32_t i = 0; i < num_dropped_columns; i++) {
                uint32_t curr_dropped_index = dropped_columns[i];
                Field* curr_dropped_field = table->field[curr_dropped_index];
                printf(
                    "Dropped column: index %d, name %s\n", 
                    curr_dropped_index, 
                    curr_dropped_field->field_name
                    );
            }
        }
    }
    
    if (has_indexing_changes && !has_non_indexing_changes) {
        retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
    }
    else if (has_dropped_columns && !has_non_dropped_changes) {
        retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
    }
    else if (has_added_columns && !has_non_added_changes) {
        retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
    }
    else if (has_row_format_changes && !has_non_row_format_changes && tables_have_same_keys_and_columns(table, altered_table, true)) {
        retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
    }
    else if (has_auto_inc_change && !has_non_auto_inc_change && tables_have_same_keys_and_columns(table, altered_table, true)) {
        retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
    }
    else if (has_column_rename && !has_non_column_rename_changes) {
        // we have identified a possible column rename, 
        // but let's do some more checks

        // we will only allow an hcr if there are no changes
        // in column positions
        if (alter_info->contains_first_or_after) {
            retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
            goto cleanup;
        }

        // now need to verify that one and only one column
        // has changed only its name. If we find anything to
        // the contrary, we don't allow it, also check indexes

        bool cr_supported = column_rename_supported(table, altered_table, alter_info->contains_first_or_after);
        if (cr_supported) {
            retval = HA_ALTER_SUPPORTED_WAIT_LOCK;
        }
        else {
            retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
        }
    }
    else { 
        retval = (get_disable_slow_alter(thd)) ? HA_ALTER_ERROR : HA_ALTER_NOT_SUPPORTED;
    }
cleanup:
    DBUG_RETURN(retval);
}

int 
ha_tokudb::alter_table_phase2(
    THD *thd,
    TABLE *altered_table,
    HA_CREATE_INFO *create_info,
    HA_ALTER_INFO *alter_info,
    HA_ALTER_FLAGS *alter_flags
    )
{
    TOKUDB_DBUG_ENTER("ha_tokudb::alter_table_phase2");
    int error;
    DB_TXN* txn = NULL;
    bool incremented_numDBs = false;
    bool modified_DBs = false;
    bool has_dropped_columns = alter_flags->is_set(HA_DROP_COLUMN);
    bool has_added_columns = alter_flags->is_set(HA_ADD_COLUMN);
    bool has_row_format_changes = alter_flags->is_set(HA_ALTER_ROW_FORMAT);
    KEY_AND_COL_INFO altered_kc_info;
    memset(&altered_kc_info, 0, sizeof(altered_kc_info));
    uint32_t max_new_desc_size = 0;
    uchar* row_desc_buff = NULL;
    uchar* column_extra = NULL; 
    bool dropping_indexes = alter_info->index_drop_count > 0 && !tables_have_same_keys(table,altered_table,false, false);
    bool adding_indexes = alter_info->index_add_count > 0 && !tables_have_same_keys(table,altered_table,false, false);
    bool change_autoinc = alter_flags->is_set(HA_CHANGE_AUTOINCREMENT_VALUE);
    
    tokudb_trx_data* trx = (tokudb_trx_data *) thd_data_get(thd, tokudb_hton->slot);

    is_fast_alter_running = true;

    if (!trx || 
        (trx->all != NULL) || 
        (trx->sp_level != NULL) ||
        (trx->stmt == NULL) ||
        (trx->sub_sp_level != trx->stmt)
       )
    {
      error = HA_ERR_UNSUPPORTED;
      goto cleanup;
    }
    txn = trx->stmt;

    error = allocate_key_and_col_info(altered_table->s, &altered_kc_info);
    if (error) { goto cleanup; }

    max_new_desc_size = get_max_desc_size(&altered_kc_info, altered_table);
    row_desc_buff = (uchar *)my_malloc(max_new_desc_size, MYF(MY_WME));
    if (row_desc_buff == NULL){ error = ENOMEM; goto cleanup;}

    if (change_autoinc) {
        error = write_auto_inc_create(share->status_block, create_info->auto_increment_value, txn);
        if (error) { goto cleanup; }
    }

    // drop indexes
    if (dropping_indexes) {
        error = drop_indexes(table, alter_info->index_drop_buffer, alter_info->index_drop_count, table->key_info, txn);
        if (error) { goto cleanup; }
    }

    // add indexes
    if (adding_indexes) {
        KEY           *key_info;
        KEY           *key;
        uint          *idx_p;
        uint          *idx_end_p;
        KEY_PART_INFO *key_part;
        KEY_PART_INFO *part_end;
        /* The add_index() method takes an array of KEY structs. */
        key_info= (KEY*) thd->alloc(sizeof(KEY) * alter_info->index_add_count);
        key= key_info;
        for (idx_p= alter_info->index_add_buffer, idx_end_p= idx_p + alter_info->index_add_count;
             idx_p < idx_end_p;
             idx_p++, key++)
        {
          /* Copy the KEY struct. */
          *key= alter_info->key_info_buffer[*idx_p];
          /* Fix the key parts. */
          part_end= key->key_part + key->key_parts;
          for (key_part= key->key_part; key_part < part_end; key_part++)
            key_part->field = table->field[key_part->fieldnr];
        }
        error = tokudb_add_index(
            table, 
            key_info,
            alter_info->index_add_count,
            txn,
            &incremented_numDBs,
            &modified_DBs
            );
        if (error) { 
            // hack for now, in case of duplicate key error, 
            // because at the moment we cannot display the right key
            // information to the user, so that he knows potentially what went
            // wrong.
            last_dup_key = MAX_KEY;
            goto cleanup;
        }
    }

    if (has_dropped_columns || has_added_columns) {
        DBT column_dbt;
        memset(&column_dbt, 0, sizeof(DBT));
        uint32_t max_column_extra_size;
        uint32_t num_column_extra;
        uint32_t columns[table->s->fields + altered_table->s->fields]; // set size such that we know it is big enough for both cases
        uint32_t num_columns = 0;
        uint32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);
        memset(columns, 0, sizeof(columns));

        if (has_added_columns && has_dropped_columns) {
            error = HA_ERR_UNSUPPORTED;
            goto cleanup;
        }
        if (!tables_have_same_keys(table, altered_table, true, false)) {
            error = HA_ERR_UNSUPPORTED;
            goto cleanup;
        }

        error = initialize_key_and_col_info(
            altered_table->s, 
            altered_table,
            &altered_kc_info,
            hidden_primary_key,
            primary_key
            );
        if (error) { goto cleanup; }

        // generate the array of columns
        if (has_dropped_columns) {
            find_changed_columns(
                columns,
                &num_columns,
                altered_table,
                table
                );
        }
        if (has_added_columns) {
            find_changed_columns(
                columns,
                &num_columns,
                table,
                altered_table
                );
        }
        max_column_extra_size = 
            STATIC_ROW_MUTATOR_SIZE + //max static row_mutator
            4 + num_columns*(1+1+4+1+1+4) + altered_table->s->reclength + // max dynamic row_mutator
            (4 + share->kc_info.num_blobs) + // max static blob size
            (num_columns*(1+4+1+4)); // max dynamic blob size
        column_extra = (uchar *)my_malloc(max_column_extra_size, MYF(MY_WME));
        if (column_extra == NULL) { error = ENOMEM; goto cleanup; }

        for (uint32_t i = 0; i < curr_num_DBs; i++) {
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
                txn,
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
                    has_added_columns // true if adding columns, otherwise is a drop
                    );
                
                column_dbt.data = column_extra;
                column_dbt.size = num_column_extra;
                DBUG_ASSERT(num_column_extra <= max_column_extra_size);
                
                error = share->key_file[i]->update_broadcast(
                    share->key_file[i],
                    txn,
                    &column_dbt,
                    DB_IS_RESETTING_OP
                    );
                if (error) { goto cleanup; }
            }
        }
    }
    
    // Check if compression type has been altered.
    if (has_row_format_changes) {
        // Determine the new compression type.
        enum toku_compression_method method = TOKU_NO_COMPRESSION;
        method = row_type_to_compression_method(create_info->row_type);

        // Set the new type.
        uint32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);
        for (uint32_t i = 0; i < curr_num_DBs; ++i) {
            DB *db = share->key_file[i];
            error = db->change_compression_method(db, method);
            if (error) {
                goto cleanup;
            }
        }
    }

    // update frm file    
    // only for tables that are not partitioned
    if (altered_table->part_info == NULL) {
        error = write_frm_data(share->status_block, txn, altered_table->s->path.str);
        if (error) { goto cleanup; }
    }    
    if (thd->killed) {
        error = ER_ABORTING_CONNECTION;
        goto cleanup;
    }

    error = 0;    
cleanup:
    free_key_and_col_info(&altered_kc_info);
    my_free(row_desc_buff, MYF(MY_ALLOW_ZERO_PTR));
    my_free(column_extra, MYF(MY_ALLOW_ZERO_PTR));
    if (txn) {
        if (error) {
            if (adding_indexes) {
                restore_add_index(table, alter_info->index_add_count, incremented_numDBs, modified_DBs);
            }
            abort_txn(txn);
            trx->stmt = NULL;
            trx->sub_sp_level = NULL;
            if (dropping_indexes) {
                restore_drop_indexes(table, alter_info->index_drop_buffer, alter_info->index_drop_count);
            }
        }
    }
    TOKUDB_DBUG_RETURN(error);
}

#endif

#endif
