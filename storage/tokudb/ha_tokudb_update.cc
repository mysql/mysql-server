#if TOKU_INCLUDE_UPSERT

// Point updates and upserts
//
// Restrictions:
//   No triggers
//   No binary logging
//   Primary key must be defined
//   Simple and compound primary key
//   Int, char and varchar primary key types
//   No updates on fields that are part of any key
//   No clustering keys
//   Integer and char field updates
//   Update expressions:
//       x = constant
//       x = x+constant
//       x = x-constant
//      x = if(x=0,0,x-1)
//   Session variable enables fast updates and fast upserts
//   Session variable disables slow updates and slow upserts
// Bugs:
//   Does this work with hot indexing?  Probably not.
// Future features:
//   Support more primary key types
//   Allow statement based binary logging
//   Force statement logging for fast updates
//   Support clustering keys using broadcast updates
//   Support primary key ranges using multicast messages
//   Support more complicated update expressions
//   Replace field_offset

int tokudb_fast_update_debug = 0;
int tokudb_upsert_debug = 0;

// Debug function to dump an Item
static void dump_item(Item *item) {
    fprintf(stderr, "%u", item->type());
    switch (item->type()) {
    case Item::FUNC_ITEM: {
        Item_func *func = static_cast<Item_func*>(item);
        uint n = func->argument_count();
        Item **arguments = func->arguments();
        fprintf(stderr, ":func=%u,%s,%u(", func->functype(), func->func_name(), n);
        for (uint i = 0; i < n ; i++) {
            dump_item(arguments[i]);
            if (i < n-1)
                fprintf(stderr,",");
        }
        fprintf(stderr, ")");
        break;
    }
    case Item::INT_ITEM: {
        Item_int *int_item = static_cast<Item_int*>(item);
        fprintf(stderr, ":int=%lld", int_item->val_int());
        break;
    }
    case Item::STRING_ITEM: {
        Item_string *str_item = static_cast<Item_string*>(item);
        fprintf(stderr, ":str=%s", str_item->val_str(NULL)->c_ptr());
        break;
    }
    case Item::FIELD_ITEM: {
        Item_field *field_item = static_cast<Item_field*>(item);
        fprintf(stderr, ":field=%s.%s.%s", field_item->db_name, field_item->table_name, field_item->field_name);
        break;
    }
    case Item::COND_ITEM: {
        Item_cond *cond_item = static_cast<Item_cond*>(item);
        fprintf(stderr, ":cond=%s(\n", cond_item->func_name());
        List_iterator<Item> li(*cond_item->argument_list());
        Item *list_item;
        while ((list_item = li++)) {
            dump_item(list_item);
            fprintf(stderr, "\n");  
        }
        fprintf(stderr, ")\n");
        break;
    }
    default:
        break;
    }
}

// Debug function to dump an Item list
static void dump_item_list(const char *h, List<Item> &l) {
    fprintf(stderr, "%s elements=%u\n", h, l.elements);
    List_iterator<Item> li(l);
    Item *item;
    while ((item = li++) != NULL) {
        dump_item(item);
        fprintf(stderr, "\n");  
    }
}

// Find a Field by its Item name
static Field *find_field_by_name(TABLE *table, Item *item) {
    if (item->type() != Item::FIELD_ITEM)
        return NULL;
    Item_field *field_item = static_cast<Item_field*>(item);
#if 0
    if (strcmp(table->s->db.str, field_item->db_name) != 0 ||
        strcmp(table->s->table_name.str, field_item->table_name) != 0)
        return NULL;
    // TODO: item->field may be a shortcut instead of this table lookup
    Field *found_field = NULL;
    for (uint i = 0; i < table->s->fields; i++) {
        Field *test_field = table->s->field[i];
        if (strcmp(field_item->field_name, test_field->field_name) == 0) {
            found_field = test_field;
            break;
        }
    }
    return found_field;
#else
    return field_item->field;
#endif
}

// Return the starting offset in the value for a particular index (selected by idx) of a
// particular field (selected by expand_field_num).
// This only works for fixed length fields
static uint32_t update_field_offset(uint32_t null_bytes, KEY_AND_COL_INFO *kc_info, int idx, int expand_field_num) {
    uint32_t offset = null_bytes;
    for (int i = 0; i < expand_field_num; i++) {
        if (bitmap_is_set(&kc_info->key_filters[idx], i)) // skip key fields
            continue;
        offset += kc_info->field_lengths[i];
    }
    return offset;
}

// Determine if an update operation can be offloaded to the storage engine.
// The update operation consists of a list of update expressions (fields[i] = values[i]), and a list
// of where conditions (conds).  The function returns true is the update is handled in the storage engine.
// Otherwise, false is returned.
int ha_tokudb::fast_update(THD *thd, List<Item> &fields, List<Item> &values, Item *conds) {
    int error = 0;

    if (tokudb_fast_update_debug) {
        dump_item_list("fields", fields);
        dump_item_list("values", values);
        if (conds) {
            fprintf(stderr, "conds\n"); dump_item(conds); fprintf(stderr, "\n");
        }
    }

    if (fields.elements < 1 || fields.elements != values.elements)
        return ENOTSUP;  // something is fishy with the parameters

    rw_rdlock(&share->num_DBs_lock);

    if (share->num_DBs > table->s->keys + test(hidden_primary_key)) // hot index in progress
        error = ENOTSUP; // run on the slow path

    if (error == 0 && !check_fast_update(thd, fields, values, conds))
        error = ENOTSUP;

    if (error == 0)
        error = send_update_message(fields, values, conds, transaction);

    rw_unlock(&share->num_DBs_lock);

    if (error != 0) {
        if (get_disable_slow_update(thd))
            error = HA_ERR_UNSUPPORTED;
        if (error != ENOTSUP)
            print_error(error, MYF(0));
    }

    return error;
}

// Return true if an expression is a simple int expression or a simple function of +- int expression.
static bool check_int_result(Item *item) {
    Item::Type t = item->type();
    if (t == Item::INT_ITEM)
        return true;
    else if (t == Item::FUNC_ITEM) {
        Item_func *item_func = static_cast<Item_func*>(item);
        if (strcmp(item_func->func_name(), "+") != 0 && strcmp(item_func->func_name(), "-") != 0)
            return false;
        if (item_func->argument_count() != 1)
            return false;
        Item **arguments = item_func->arguments();
        if (arguments[0]->type() != Item::INT_ITEM)
            return false;
        return true;
    } else
        return false;
}

// Return true if an expression looks like field_name op constant.
static bool check_x_op_constant(const char *field_name, Item *item, const char *op, Item **item_constant) {
    if (item->type() != Item::FUNC_ITEM)
        return false;
    Item_func *item_func = static_cast<Item_func*>(item);
    if (strcmp(item_func->func_name(), op) != 0)
        return false;
    Item **arguments = item_func->arguments();
    uint n = item_func->argument_count();
    if (n != 2)
        return false;
    if (arguments[0]->type() != Item::FIELD_ITEM)
        return false;
    Item_field *arg0 = static_cast<Item_field*>(arguments[0]);
    if (strcmp(field_name, arg0->field_name) != 0)
        return false;
    if (!check_int_result(arguments[1]))
        return false;
    *item_constant = arguments[1];
    return true;
}

// Return true if an expression looks like field_name = constant
static bool check_x_equal_0(const char *field_name, Item *item) {
    Item *item_constant;
    if (!check_x_op_constant(field_name, item, "=", &item_constant))
        return false;
    if (item_constant->val_int() != 0)
        return false;
    return true;
}

// Return true if an expression looks like fieldname - 1
static bool check_x_minus_1(const char *field_name, Item *item) {
    Item *item_constant;
    if (!check_x_op_constant(field_name, item, "-", &item_constant))
        return false;
    if (item_constant->val_int() != 1)
        return false;
    return true;
}

// Return true if an expression looks like if(fieldname=0, 0, fieldname-1) and
// the field named by fieldname is an unsigned int.
static bool check_decr_floor_expression(Field *lhs_field, Item *item) {
    if (item->type() != Item::FUNC_ITEM)
        return false;
    Item_func *item_func = static_cast<Item_func*>(item);
    Item **arguments = item_func->arguments();
    uint n = item_func->argument_count();
    if (n != 3)
        return false;
    if (!check_x_equal_0(lhs_field->field_name, arguments[0]))
        return false;
    if (arguments[1]->type() != Item::INT_ITEM || arguments[1]->val_int() != 0)
        return false;
    if (!check_x_minus_1(lhs_field->field_name, arguments[2]))
        return false;
    if (!(lhs_field->flags & UNSIGNED_FLAG))
        return false;
    return true;
}

// Check if lhs = rhs expression is simple.  Return true if it is.
static bool check_simple_update_expression(Item *lhs_item, Item *rhs_item, TABLE *table) {
    Field *lhs_field = find_field_by_name(table, lhs_item);
    if (lhs_field == NULL)
        return false;
    if (!lhs_field->part_of_key.is_clear_all())
        return false;
    enum_field_types lhs_type = lhs_field->type();
    Item::Type rhs_type = rhs_item->type();
    switch (lhs_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
        if (check_int_result(rhs_item))
            return true;
        Item *item_constant;
        if (check_x_op_constant(lhs_field->field_name, rhs_item, "+", &item_constant))
            return true;
        if (check_x_op_constant(lhs_field->field_name, rhs_item, "-", &item_constant))
            return true;
        if (check_decr_floor_expression(lhs_field, rhs_item))
            return true;
        break;
    case MYSQL_TYPE_STRING:
        if (rhs_type == Item::INT_ITEM || rhs_type == Item::STRING_ITEM) 
            return true;
        break;
    default:
        break;
    }
    return false;
}

// Check that all update expressions are simple.  Return true if they are.
static bool check_all_update_expressions(List<Item> &fields, List<Item> &values, TABLE *table) {
    List_iterator<Item> lhs_i(fields);
    List_iterator<Item> rhs_i(values);
    while (1) {
        Item *lhs_item = lhs_i++;
        if (lhs_item == NULL)
            break;
        Item *rhs_item = rhs_i++;
        if (rhs_item == NULL)
            assert(0); // can not happen
        if (!check_simple_update_expression(lhs_item, rhs_item, table))
            return false;
    }
    return true;
}

#if 0
static bool field_name_in_primary_key(TABLE *table, const char *field_name) {
    if (table->s->primary_key >= table->s->keys)
        return false;
    KEY *key = &table->s->key_info[table->s->primary_key];
    if (key->key_parts != 1)
        return false;
    KEY_PART_INFO *key_part = &key->key_part[0];
    if (key->key_length != key_part->store_length)
        return false;
    if (strcmp(field_name, key_part->field->field_name) != 0)
        return false;
    return true;
}
#endif

// Check that an expression looks like fieldname = constant, fieldname is part of the
// primary key, and the named field is an int, char or varchar type.  Return true if it does.
static bool check_pk_field_equal_constant(Item *item, TABLE *table, MY_BITMAP &pk_fields) {
    if (item->type() != Item::FUNC_ITEM)
        return false;
    Item_func *func = static_cast<Item_func*>(item);
    if (strcmp(func->func_name(), "=") != 0)
        return false;   
    uint n = func->argument_count();
    if (n != 2)
        return false;
    Item **arguments = func->arguments();
    Field *field = find_field_by_name(table, arguments[0]);
    if (field == NULL)
        return false;
    if (!bitmap_test_and_clear(&pk_fields, field->field_index))
        return false;
    switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
        return arguments[1]->type() == Item::INT_ITEM || arguments[1]->type() == Item::STRING_ITEM;
    default:
        return false;
    }
}

// Check that the where condition covers all of the primary key components with fieldname = constant
// expressions.  Return true if it does.
static bool check_point_update(Item *conds, TABLE *table) {
    bool result = false;

    if (conds == NULL)
        return false; // no where condition on the update

    if (table->s->primary_key >= table->s->keys)
        return false; // no primary key defined

    // use a bitmap of the primary key fields to keep track of those fields that are covered
    // by the where conditions
    MY_BITMAP pk_fields;
    if (bitmap_init(&pk_fields, NULL, table->s->fields, FALSE)) // 1 -> failure
        return false;
    KEY *key = &table->s->key_info[table->s->primary_key];
    for (uint i = 0; i < key->key_parts; i++) 
        bitmap_set_bit(&pk_fields, key->key_part[i].field->field_index);

    switch (conds->type()) {
    case Item::FUNC_ITEM:
        result = check_pk_field_equal_constant(conds, table, pk_fields);
        break;
    case Item::COND_ITEM: {
        Item_cond *cond_item = static_cast<Item_cond*>(conds);
        if (strcmp(cond_item->func_name(), "and") != 0)
            break;
        List_iterator<Item> li(*cond_item->argument_list());
        Item *list_item;
        result = true;
        while (result == true && (list_item = li++)) {
            result = check_pk_field_equal_constant(list_item, table, pk_fields);
        }
        break;
    }
    default:
        break;
    }

    if (!bitmap_is_clear_all(&pk_fields))
        result = false;
    bitmap_free(&pk_fields);
    return result;
}

// Return true if there are any clustering keys (except the primary).
// Precompute this when the table is opened.
static bool clustering_keys_exist(TABLE *table) {
    for (uint i = 0; i < table->s->keys; i++)
        if (i != table->s->primary_key && (table->s->key_info[i].flags & HA_CLUSTERING))
            return true;
    return false;
}

#include <binlog.h>

// Check if an update operation can be handled by this storage engine.  Return true if it can.
bool ha_tokudb::check_fast_update(THD *thd, List<Item> &fields, List<Item> &values, Item *conds) {
    // fast upserts disabled
    if (!get_enable_fast_update(thd))
        return false;

    if (!transaction)
        return false;

    // avoid strict mode arithmetic overflow issues
    if (thd->is_strict_mode())
        return false;

    // no triggers
    if (table->triggers) 
        return false;
    
    // no binlog
    if (mysql_bin_log.is_open())
        return false;

    // no clustering keys (need to broadcast an increment into the clustering keys since we are selecting with the primary key)
    if (clustering_keys_exist(table)) 
        return false;

    // fast updates enabled with session variable
    if (!get_enable_fast_update(thd))
        return false;

    if (!check_all_update_expressions(fields, values, table))
        return false;

    if (!check_point_update(conds, table))
        return false;

    return true;
}

// Marshall a simple row descriptor to a buffer.
static void marshall_simple_descriptor(tokudb::buffer &b, TABLE *table, KEY_AND_COL_INFO &kc_info, uint key_num) {
    Simple_row_descriptor sd;
    sd.m_fixed_field_offset = table->s->null_bytes;
    sd.m_var_field_offset = sd.m_fixed_field_offset + kc_info.mcp_info[key_num].fixed_field_size;
    sd.m_var_offset_bytes = kc_info.mcp_info[key_num].len_of_offsets;
    sd.m_num_var_fields = sd.m_var_offset_bytes == 0 ? 0 : kc_info.mcp_info[key_num].len_of_offsets / sd.m_var_offset_bytes;
    sd.append(b);
}

static inline uint32_t get_null_bit_position(uint32_t null_bit);

// Marshall update operatins to a buffer.
static void marshall_simple_update(tokudb::buffer &b, Item *lhs_item, Item *rhs_item, TABLE *table, TOKUDB_SHARE *share) {
    // figure out the update operation type (again)
    Field *lhs_field = find_field_by_name(table, lhs_item);
    assert(lhs_field); // we found it before, so this should work

    // compute the update info
    uint32_t field_type;
    uint32_t field_num = lhs_field->field_index;
    uint32_t field_null_num = 0;
    if (lhs_field->real_maybe_null())
        field_null_num = (1<<31) + (field_num/8)*8 + get_null_bit_position(lhs_field->null_bit);
    uint32_t offset = update_field_offset(table->s->null_bytes, &share->kc_info, table->s->primary_key, lhs_field->field_index);
    void *v_ptr = NULL;
    uint32_t v_length;
    uint32_t update_operation;
    longlong v_ll; 
    String v_str;

    switch (lhs_field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG: {
        Field_num *lhs_num = static_cast<Field_num*>(lhs_field);
        field_type = lhs_num->unsigned_flag ? UPDATE_TYPE_UINT : UPDATE_TYPE_INT;
        switch (rhs_item->type()) {
        case Item::INT_ITEM: {
            update_operation = '=';
            v_ll = rhs_item->val_int();
            v_length = lhs_field->pack_length();
            v_ptr = &v_ll;
            break;
        }
        case Item::FUNC_ITEM: {
            Item_func *rhs_func = static_cast<Item_func*>(rhs_item);
            Item **arguments = rhs_func->arguments();
            if (strcmp(rhs_func->func_name(), "if") == 0) {
                update_operation = '-'; // we only support one if function for now, and it is a descrement with floor.
                v_ll = 1;
            } else if (rhs_func->argument_count() == 1) {
                update_operation = '=';
                v_ll = rhs_func->val_int();
            } else {
                update_operation = rhs_func->func_name()[0];
                v_ll = arguments[1]->val_int();
            }
            v_length = lhs_field->pack_length();
            v_ptr = &v_ll;
            break;
        }
        default:
            assert(0);
        }
        break;
    }

    case MYSQL_TYPE_STRING: {
        update_operation = '=';
        field_type = lhs_field->binary() ? UPDATE_TYPE_BINARY : UPDATE_TYPE_CHAR;        
        v_str = *rhs_item->val_str(&v_str);
        v_length = v_str.length();
        if (v_length >= lhs_field->pack_length()) {
            v_length = lhs_field->pack_length();
            v_str.length(v_length); // truncate
        } else {
            v_length = lhs_field->pack_length();
            uchar pad_char = lhs_field->binary() ? 0 : lhs_field->charset()->pad_char;
            v_str.fill(lhs_field->pack_length(), pad_char); // pad
        }
        v_ptr = v_str.c_ptr();
        break;
    }
    default:
        assert(0);
    }

    // marshall the update fields into the buffer
    b.append(&update_operation, sizeof update_operation);
    b.append(&field_type, sizeof field_type);
    b.append(&field_num, sizeof field_num);
    b.append(&field_null_num, sizeof field_null_num);
    b.append(&offset, sizeof offset);
    b.append(&v_length, sizeof v_length);
    b.append(v_ptr, v_length);
}

// Save an item's value into the appropriate field.  Return 0 if successful.
static int save_in_field(Item *item, TABLE *table) {
    assert(item->type() == Item::FUNC_ITEM);
    Item_func *func = static_cast<Item_func*>(item);
    assert(strcmp(func->func_name(), "=") == 0);
    uint n = func->argument_count();
    assert(n == 2);
    Item **arguments = func->arguments();
    assert(arguments[0]->type() == Item::FIELD_ITEM);
    Item_field *field_item = static_cast<Item_field*>(arguments[0]);
    my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
    int error = arguments[1]->save_in_field(field_item->field, 0);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    return error;
}

// Generate an update message for an update operation and send it into the primary tree.  Return 0 if successful.
int ha_tokudb::send_update_message(List<Item> &fields, List<Item> &values, Item *conds, DB_TXN *txn) {
    int error;

    // Save the primary key from the where conditions
    Item::Type t = conds->type();
    if (t == Item::FUNC_ITEM) {
        error = save_in_field(conds, table);
    } else if (t == Item::COND_ITEM) {
        Item_cond *cond_item = static_cast<Item_cond*>(conds);
        List_iterator<Item> li(*cond_item->argument_list());
        Item *list_item;
        for (error = 0; error == 0 && (list_item = li++); ) {
            error = save_in_field(list_item, table);
        }
    } else
        assert(0);
    if (error)
        return error;

    // put the primary key into key_buff and wrap it with key_dbt
    DBT key_dbt; 
    bool has_null;
    create_dbt_key_from_table(&key_dbt, primary_key, key_buff, table->record[0], &has_null);
    
    // construct the update message
    tokudb::buffer update_message;
        
    uchar operation = UPDATE_OP_SIMPLE_UPDATE;
    update_message.append(&operation, sizeof operation);
    uint32_t update_mode = 0;
    update_message.append(&update_mode, sizeof update_mode);
    
    // append the descriptor
    marshall_simple_descriptor(update_message, table, share->kc_info, primary_key);    

    // append the updates
    List_iterator<Item> lhs_i(fields);
    List_iterator<Item> rhs_i(values);
    while (error == 0) {
        Item *lhs_item = lhs_i++;
        if (lhs_item == NULL)
            break;
        Item *rhs_item = rhs_i++;
        if (rhs_item == NULL)
            assert(0); // can not happen
        marshall_simple_update(update_message, lhs_item, rhs_item, table, share);
    }

    // send the message    
    DBT update_dbt; memset(&update_dbt, 0, sizeof update_dbt);
    update_dbt.data = update_message.data();
    update_dbt.size = update_message.size();
    error = share->key_file[primary_key]->update(share->key_file[primary_key], txn, &key_dbt, &update_dbt, 0);
        
    return error;
}

// Determine if an upsert operation can be offloaded to the storage engine.
// An upsert consists of a row and a list of update expressions (update_fields[i] = update_values[i]).
// The function returns true is the upsert is handled in the storage engine.  Otherwise, false is returned.
int ha_tokudb::upsert(THD *thd, uchar *record, List<Item> &update_fields, List<Item> &update_values) {
    int error = 0;

    if (tokudb_upsert_debug) {
        fprintf(stderr, "upsert\n");
        dump_item_list("update_fields", update_fields);
        dump_item_list("update_values", update_values);
    }

    if (update_fields.elements < 1 || update_fields.elements != update_values.elements)
        return ENOTSUP;  // not an upsert or something is fishy with the parameters

    rw_rdlock(&share->num_DBs_lock);

    if (share->num_DBs > table->s->keys + test(hidden_primary_key)) // hot index in progress
        error = ENOTSUP; // run on the slow path

    if (error == 0 && !check_upsert(thd, update_fields, update_values))
        error = ENOTSUP;
    
    if (error == 0) 
        error = send_upsert_message(thd, record, update_fields, update_values, transaction);

    rw_unlock(&share->num_DBs_lock);

    if (error != 0) {
        if (get_disable_slow_upsert(thd))
            error = HA_ERR_UNSUPPORTED;
        if (error != ENOTSUP)
            print_error(error, MYF(0));
    }
    return error;
}

// Check if an upsert can be handled by this storage engine.  Return trus if it can.
bool ha_tokudb::check_upsert(THD *thd, List<Item> &update_fields, List<Item> &update_values) {
    // fast upserts disabled
    if (!get_enable_fast_upsert(thd))
        return false;

    if (!transaction)
        return false;

    // avoid strict mode arithmetic overflow issues
    if (thd->is_strict_mode())
        return false;

    // no triggers
    if (table->triggers) 
        return false;
    
    // no binlog
    if (mysql_bin_log.is_open())
        return false;

    // primary key must exist
    if (table->s->primary_key >= table->s->keys)
        return false;

    // no clustering keys (need to broadcast an increment into the clustering keys since we are selecting with the primary key)
    if (clustering_keys_exist(table)) 
        return false;

    if (!check_all_update_expressions(update_fields, update_values, table))
        return false;

    return true;
}

// Generate an upsert message and send it into the primary tree.  Return 0 if successful.
int ha_tokudb::send_upsert_message(THD *thd, uchar *record, List<Item> &update_fields, List<Item> &update_values, DB_TXN *txn) {
    int error = 0;

    // generate primary key
    DBT key_dbt;
    bool has_null;
    create_dbt_key_from_table(&key_dbt, primary_key, primary_key_buff, record, &has_null);

    // generate packed row
    DBT row;
    error = pack_row(&row, (const uchar *) record, primary_key);
    if (error)
        return error;

    tokudb::buffer update_message;

    // append the operation
    uchar operation = UPDATE_OP_SIMPLE_UPSERT;
    update_message.append(&operation, sizeof operation);
    uint32_t update_mode = 0;
    update_message.append(&update_mode, sizeof update_mode);

    // append the row
    uint32_t row_length = row.size;
    update_message.append(&row_length, sizeof row_length);
    update_message.append(row.data, row_length);

    // append the descriptor
    marshall_simple_descriptor(update_message, table, share->kc_info, primary_key);

    // append the update expressions
    List_iterator<Item> lhs_i(update_fields);
    List_iterator<Item> rhs_i(update_values);
    while (1) {
        Item *lhs_item = lhs_i++;
        if (lhs_item == NULL)
            break;
        Item *rhs_item = rhs_i++;
        if (rhs_item == NULL)
            assert(0); // can not happen
        marshall_simple_update(update_message, lhs_item, rhs_item, table, share);
    }

    // send the upsert message
    DBT update_dbt; memset(&update_dbt, 0, sizeof update_dbt);
    update_dbt.data = update_message.data();
    update_dbt.size = update_message.size();
    error = share->key_file[primary_key]->update(share->key_file[primary_key], txn, &key_dbt, &update_dbt, 0);

    return error;
}

#endif
