#if TOKU_INCLUDE_ALTER_56

#include "ha_tokudb_alter_common.cc"

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

// require that there is no intersection of add and drop names.
static bool
is_disjoint_add_drop(Alter_inplace_info *ha_alter_info) {
    for (uint d = 0; d < ha_alter_info->index_drop_count; d++) {
        KEY *drop_key = ha_alter_info->index_drop_buffer[d];
        for (uint a = 0; a < ha_alter_info->index_add_count; a++) {
            KEY *add_key = &ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[a]];
            if (strcmp(drop_key->name, add_key->name) == 0) {
                return false;
            }
        }
    }
    return true;
}

#if 50600 < MYSQL_VERSION_ID
#define TOKU_ALTER_RENAME ALTER_RENAME
#elif 50500 < MYSQL_VERSION_ID
#define TOKU_ALTER_RENAME ALTER_RENAME_56
#else
#error
#endif

class tokudb_alter_ctx : public inplace_alter_handler_ctx {
public:
    tokudb_alter_ctx() {
        alter_txn = NULL;
        add_index_changed = false;
        drop_index_changed = false;
        compression_changed = false;
    }
public:
    DB_TXN *alter_txn;
    bool add_index_changed;
    bool incremented_num_DBs, modified_DBs;
    bool drop_index_changed;
    bool compression_changed;
    enum toku_compression_method orig_compression_method;
};

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

    // always allow rename table + any other operation, so turn off the handler flag
    if (handler_flags & Alter_inplace_info::TOKU_ALTER_RENAME) {
        handler_flags &= ~Alter_inplace_info::TOKU_ALTER_RENAME;
    }

    // add/drop index
    if (only_flags(handler_flags, Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX + 
                                  Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX)) {
        if ((ha_alter_info->index_add_count > 0 || ha_alter_info->index_drop_count > 0) &&
            !tables_have_same_keys(table, altered_table, false, false) &&
            is_disjoint_add_drop(ha_alter_info)) {

            result = HA_ALTER_INPLACE_SHARED_LOCK;

            // someday, allow multiple hot indexes via alter table add key. don't forget to change the store_lock function.
            // for now, hot indexing is only supported via session variable with the create index sql command
            if (ha_alter_info->index_add_count == 1 && ha_alter_info->index_drop_count == 0 && 
                get_create_index_online(thd) && thd_sql_command(thd) == SQLCOM_CREATE_INDEX) {
                result = HA_ALTER_INPLACE_NO_LOCK;
            }
        }
    } else
    // column default
    if (only_flags(handler_flags, Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        result = HA_ALTER_INPLACE_NO_LOCK;
    } else
    // column rename
    if (only_flags(handler_flags, Alter_inplace_info::ALTER_COLUMN_NAME + Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        // we have identified a possible column rename, 
        // but let's do some more checks

        // we will only allow an hcr if there are no changes
        // in column positions (ALTER_COLUMN_ORDER is not set)

        // now need to verify that one and only one column
        // has changed only its name. If we find anything to
        // the contrary, we don't allow it, also check indexes
        bool cr_supported = column_rename_supported(table, altered_table, (ha_alter_info->handler_flags & Alter_inplace_info::ALTER_COLUMN_ORDER) != 0);
        if (cr_supported)
            result = HA_ALTER_INPLACE_NO_LOCK;
    } else    
    // add column
    if (only_flags(handler_flags, Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER)) {
        uint32_t added_columns[altered_table->s->fields];
        uint32_t num_added_columns = 0;
        int r = find_changed_columns(added_columns, &num_added_columns, table, altered_table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
                for (uint32_t i = 0; i < num_added_columns; i++) {
                    uint32_t curr_added_index = added_columns[i];
                    Field* curr_added_field = altered_table->field[curr_added_index];
                    printf("Added column: index %d, name %s\n", curr_added_index, curr_added_field->field_name);
                }
            }
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    // drop column
    if (only_flags(handler_flags, Alter_inplace_info::DROP_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER)) {
        uint32_t dropped_columns[table->s->fields];
        uint32_t num_dropped_columns = 0;
        int r = find_changed_columns(dropped_columns, &num_dropped_columns, altered_table, table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE_INFO) {
                for (uint32_t i = 0; i < num_dropped_columns; i++) {
                    uint32_t curr_dropped_index = dropped_columns[i];
                    Field* curr_dropped_field = table->field[curr_dropped_index];
                    printf("Dropped column: index %d, name %s\n", curr_dropped_index, curr_dropped_field->field_name);
                }
            }
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    if (only_flags(handler_flags, Alter_inplace_info::CHANGE_CREATE_OPTION)) {
        // alter auto_increment
        if (only_flags(create_info->used_fields, HA_CREATE_USED_AUTO)) {
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        } else
        // alter row_format
        if (only_flags(create_info->used_fields, HA_CREATE_USED_ROW_FORMAT)) {
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    }

    // turn not supported into error if the slow alter table (copy) is disabled
    if (result == HA_ALTER_INPLACE_NOT_SUPPORTED && get_disable_slow_alter(thd)) {
        print_error(HA_ERR_UNSUPPORTED, MYF(0));
        result = HA_ALTER_ERROR;
    }
    
    DBUG_RETURN(result);
}

bool 
ha_tokudb::prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_DBUG_ENTER("prepare_inplace_alter_table");
    bool result = false; // success

    assert(ha_alter_info->handler_ctx == NULL);
    tokudb_alter_ctx *ctx = new tokudb_alter_ctx;
    assert(ctx);
    ha_alter_info->handler_ctx = ctx;

    int r = db_env->txn_begin(db_env, 0, &ctx->alter_txn, 0);
    if (r != 0)
        result = true; // fail
    DBUG_RETURN(result);
}

bool 
ha_tokudb::inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_DBUG_ENTER("inplace_alter_table");

    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);

    HA_CREATE_INFO *create_info = ha_alter_info->create_info;
    ulong handler_flags = fix_handler_flags(ha_alter_info, table, altered_table);

    if (error == 0 && (handler_flags & (Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX))) {
        error = alter_table_drop_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX))) {
        error = alter_table_add_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & (Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::DROP_COLUMN))) { 
        error = alter_table_add_or_drop_column(altered_table, ha_alter_info);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) && (create_info->used_fields & HA_CREATE_USED_AUTO)) {
        error = write_auto_inc_create(share->status_block, create_info->auto_increment_value, ctx->alter_txn);
    }
    if (error == 0 && (handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) && (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT)) {
        // Get the current type
        tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
        DB *db = share->key_file[0];
        error = db->get_compression_method(db, &ctx->orig_compression_method);
        assert(error == 0);

        // Set the new type.
        enum toku_compression_method method = row_type_to_compression_method(create_info->row_type);
        uint32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);
        for (uint32_t i = 0; i < curr_num_DBs; i++) {
            db = share->key_file[i];
            error = db->change_compression_method(db, method);
            if (error)
                break;
            ctx->compression_changed = true;
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

    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    ctx->add_index_changed = true;
    int error = tokudb_add_index(table, key_info, ha_alter_info->index_add_count, ctx->alter_txn, &ctx->incremented_num_DBs, &ctx->modified_DBs);
    if (error == HA_ERR_FOUND_DUPP_KEY) {
        // hack for now, in case of duplicate key error, 
        // because at the moment we cannot display the right key
        // information to the user, so that he knows potentially what went
        // wrong.
        last_dup_key = MAX_KEY;
    }

    my_free(key_info);

    return error;
}

static bool find_index_of_key(const char *key_name, TABLE *table, uint *index_offset_ptr) {
    for (uint i = 0; i < table->s->keys; i++) {
        if (strcmp(key_name, table->key_info[i].name) == 0) {
            *index_offset_ptr = i;
            return true;
        }
    }
    return false;
}

static bool find_index_of_key(const char *key_name, KEY *key_info, uint key_count, uint *index_offset_ptr) {
    for (uint i = 0; i < key_count; i++) {
        if (strcmp(key_name, key_info[i].name) == 0) {
            *index_offset_ptr = i;
            return true;
        }
    }
    return false;
}

int
ha_tokudb::alter_table_drop_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    KEY *key_info = table->key_info;
    // translate key names to indexes into the key_info array
    uint index_drop_offsets[ha_alter_info->index_drop_count];
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
        bool found;
        found = find_index_of_key(ha_alter_info->index_drop_buffer[i]->name, table, &index_drop_offsets[i]);
        if (!found) {
            // undo of add key in partition engine
            found = find_index_of_key(ha_alter_info->index_drop_buffer[i]->name, ha_alter_info->key_info_buffer, ha_alter_info->key_count, &index_drop_offsets[i]);
            assert(found);
            key_info = ha_alter_info->key_info_buffer;
        }
    }
    
    // drop indexes
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    ctx->drop_index_changed = true;

    int error = drop_indexes(table, index_drop_offsets, ha_alter_info->index_drop_count, key_info, ctx->alter_txn);

    return error;
}

int
ha_tokudb::alter_table_add_or_drop_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    int error;
    uchar *column_extra = NULL;
    uchar *row_desc_buff = NULL;
    uint32_t max_new_desc_size = 0;
    uint32_t max_column_extra_size;
    uint32_t num_column_extra;
    uint32_t num_columns = 0;
    uint32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);

    uint32_t columns[table->s->fields + altered_table->s->fields]; // set size such that we know it is big enough for both cases
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
                                                      ctx->alter_txn,
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
                                                         ctx->alter_txn,
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
    
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    bool result = false; // success

    if (commit) {
        if (altered_table->part_info == NULL) {
            // read frm data for the altered table
            uchar *frm_data; size_t frm_len;
            int error = readfrm(altered_table->s->path.str, &frm_data, &frm_len);
            if (!error) {
                // transactionally write frm data to status
                error = write_to_status(share->status_block, hatoku_frm_data, (void *)frm_data, (uint)frm_len, ctx->alter_txn);
                my_free(frm_data);
            }
            if (error) {
                commit = false; 
                result = true;
                print_error(error, MYF(0));
            }
        }
    }

    if (commit) {
        // commit the alter transaction NOW
        commit_txn(ctx->alter_txn, 0);
        ctx->alter_txn = NULL;
    } else { 
        // abort the alter transaction NOW so that any alters are rolled back. this allows the following restores to work.
        abort_txn(ctx->alter_txn);
        ctx->alter_txn = NULL;

        if (ctx->add_index_changed) {
            restore_add_index(table, ha_alter_info->index_add_count, ctx->incremented_num_DBs, ctx->modified_DBs);
        }
        if (ctx->drop_index_changed) {

            // translate key names to indexes into the key_info array
            uint index_drop_offsets[ha_alter_info->index_drop_count];
            for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
                bool found = find_index_of_key(ha_alter_info->index_drop_buffer[i]->name, table, &index_drop_offsets[i]);
                assert(found);
            }
            restore_drop_indexes(table, index_drop_offsets, ha_alter_info->index_drop_count);
        }
        if (ctx->compression_changed) {
            uint32_t curr_num_DBs = table->s->keys + test(hidden_primary_key);
            for (uint32_t i = 0; i < curr_num_DBs; i++) {
                DB *db = share->key_file[i];
                int error = db->change_compression_method(db, ctx->orig_compression_method);
                assert(error == 0);
            }
        }
    }
    
    DBUG_RETURN(result);
}

#endif
