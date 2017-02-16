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
#if TOKU_INCLUDE_ALTER_56

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
#define TOKU_ALTER_RENAME ALTER_RENAME
#define DYNAMIC_ARRAY_ELEMENTS_TYPE size_t
#elif (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
      (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799)
#define TOKU_ALTER_RENAME ALTER_RENAME
#define DYNAMIC_ARRAY_ELEMENTS_TYPE int
#elif 50500 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50599
#define TOKU_ALTER_RENAME ALTER_RENAME_56
#define DYNAMIC_ARRAY_ELEMENTS_TYPE int
#else
#error
#endif

#include "ha_tokudb_alter_common.cc"
#include <sql_array.h>
#include <sql_base.h>

// The tokudb alter context contains the alter state that is set in the check if supported method and used
// later when the alter operation is executed.
class tokudb_alter_ctx : public inplace_alter_handler_ctx {
public:
    tokudb_alter_ctx() :
        handler_flags(0),
        alter_txn(NULL),
        add_index_changed(false),
        drop_index_changed(false),
        reset_card(false),
        compression_changed(false),
        expand_varchar_update_needed(false),
        expand_fixed_update_needed(false),
        expand_blob_update_needed(false),
        optimize_needed(false),
        table_kc_info(NULL),
        altered_table_kc_info(NULL) {
    }
    ~tokudb_alter_ctx() {
        if (altered_table_kc_info)
            free_key_and_col_info(altered_table_kc_info);
    }
public:
    ulong handler_flags;
    DB_TXN *alter_txn;
    bool add_index_changed;
    bool incremented_num_DBs, modified_DBs;
    bool drop_index_changed;
    bool reset_card;
    bool compression_changed;
    enum toku_compression_method orig_compression_method;
    bool expand_varchar_update_needed;
    bool expand_fixed_update_needed;
    bool expand_blob_update_needed;
    bool optimize_needed;
    Dynamic_array<uint> changed_fields;
    KEY_AND_COL_INFO *table_kc_info;
    KEY_AND_COL_INFO *altered_table_kc_info;
    KEY_AND_COL_INFO altered_table_kc_info_base;
};

// Debug function to print out an alter table operation
void ha_tokudb::print_alter_info(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
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
          "name: %s, types: %u %u, nullable: %d, null_offset: %d, is_null_field: %d, is_null %d, pack_length %u\n", 
          curr_field->field_name, 
          curr_field->real_type(), mysql_to_toku_type(curr_field),
          curr_field->null_bit,
          null_offset,
          curr_field->real_maybe_null(),
          curr_field->real_maybe_null() ? table->s->default_values[null_offset] & curr_field->null_bit : 0xffffffff,
          curr_field->pack_length()
          );
    }
    printf("******\n");
    printf("***altered table***\n");
    for (uint i = 0; i < altered_table->s->fields; i++) {
      Field* curr_field = altered_table->field[i];
      uint null_offset = get_null_offset(altered_table, curr_field);
      printf(
         "name: %s, types: %u %u, nullable: %d, null_offset: %d, is_null_field: %d, is_null %d, pack_length %u\n", 
         curr_field->field_name,
         curr_field->real_type(), mysql_to_toku_type(curr_field),
         curr_field->null_bit,
         null_offset,
         curr_field->real_maybe_null(),
         curr_field->real_maybe_null() ? altered_table->s->default_values[null_offset] & curr_field->null_bit : 0xffffffff,
         curr_field->pack_length()
         );
    }
    printf("******\n");
}

// Given two tables with equal number of fields, find all of the fields with different types
// and return the indexes of the different fields in the changed_fields array. This function ignores field
// name differences.
static int find_changed_fields(TABLE *table_a, TABLE *table_b, Dynamic_array<uint> &changed_fields) {
    for (uint i = 0; i < table_a->s->fields; i++) {
        Field *field_a = table_a->field[i];
        Field *field_b = table_b->field[i];
        if (!fields_are_same_type(field_a, field_b)) 
            changed_fields.append(i);
    }
    return changed_fields.elements();
}

static bool change_length_is_supported(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx);

static bool change_type_is_supported(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx);

// The ha_alter_info->handler_flags can not be trusted.  This function maps the bogus handler flags to something we like.
static ulong fix_handler_flags(THD *thd, TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    ulong handler_flags = ha_alter_info->handler_flags;

#if 100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099
    // This is automatically supported, hide the flag from later checks
    handler_flags &= ~Alter_inplace_info::ALTER_PARTITIONED;
#endif

    // workaround for fill_alter_inplace_info bug (#5193)
    // the function erroneously sets the ADD_INDEX and DROP_INDEX flags for a column addition that does not
    // change the keys.  the following code turns the ADD_INDEX and DROP_INDEX flags so that we can do hot
    // column addition later.
    if (handler_flags & (Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::DROP_COLUMN)) {
        if (handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::DROP_INDEX)) {
            if (tables_have_same_keys(table, altered_table, THDVAR(thd, alter_print_error) != 0, false)) {
                handler_flags &= ~(Alter_inplace_info::ADD_INDEX + Alter_inplace_info::DROP_INDEX);
            }
        }
    }

    // always allow rename table + any other operation, so turn off the rename flag
    if (handler_flags & Alter_inplace_info::TOKU_ALTER_RENAME) {
        handler_flags &= ~Alter_inplace_info::TOKU_ALTER_RENAME;
    }

    // ALTER_COLUMN_TYPE may be set when no columns have been changed, so turn off the flag
    if (handler_flags & Alter_inplace_info::ALTER_COLUMN_TYPE) {
        if (all_fields_are_same_type(table, altered_table)) {
            handler_flags &= ~Alter_inplace_info::ALTER_COLUMN_TYPE;
        }
    }

    return handler_flags;
}

// Require that there is no intersection of add and drop names.
static bool is_disjoint_add_drop(Alter_inplace_info *ha_alter_info) {
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

// Return true if some bit in mask is set and no bit in ~mask is set, otherwise return false.
static bool only_flags(ulong bits, ulong mask) {
    return (bits & mask) != 0 && (bits & ~mask) == 0;
}

// Check if an alter table operation on this table and described by the alter table parameters is supported inplace
// and if so, what type of locking is needed to execute it.
// return values:

// HA_ALTER_INPLACE_NOT_SUPPORTED: alter operation is not supported as an inplace operation, a table copy is required
// HA_ALTER_ERROR: the alter table operation should fail

// HA_ALTER_INPLACE_EXCLUSIVE_LOCK: prepare and alter runs with MDL X

// HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE: prepare runs with MDL X, alter runs with MDL SNW
// HA_ALTER_INPLACE_SHARED_LOCK: prepare and alter methods called with MDL SNW, concurrent reads, no writes

// HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE: prepare runs with MDL X, alter runs with MDL SW
// HA_ALTER_INPLACE_NO_LOCK: prepare and alter methods called with MDL SW, concurrent reads, writes.
//                           must set WRITE_ALLOW_WRITE lock type in the external lock method to avoid deadlocks
//                           with the MDL lock and the table lock
enum_alter_inplace_result ha_tokudb::check_if_supported_inplace_alter(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_HANDLER_DBUG_ENTER("");

    if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE) {
        print_alter_info(altered_table, ha_alter_info);
    }

    enum_alter_inplace_result result = HA_ALTER_INPLACE_NOT_SUPPORTED; // default is NOT inplace
    THD *thd = ha_thd();

    // setup context
    tokudb_alter_ctx *ctx = new tokudb_alter_ctx;
    ha_alter_info->handler_ctx = ctx;
    ctx->handler_flags = fix_handler_flags(thd, table, altered_table, ha_alter_info);
    ctx->table_kc_info = &share->kc_info;
    ctx->altered_table_kc_info = &ctx->altered_table_kc_info_base;
    memset(ctx->altered_table_kc_info, 0, sizeof (KEY_AND_COL_INFO));

    if (get_disable_hot_alter(thd)) {
        ; // do nothing
    } else
    // add or drop index
    if (only_flags(ctx->handler_flags, Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX + 
                   Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX)) {
        if (table->s->null_bytes == altered_table->s->null_bytes && 
            (ha_alter_info->index_add_count > 0 || ha_alter_info->index_drop_count > 0) &&
            !tables_have_same_keys(table, altered_table, THDVAR(thd, alter_print_error) != 0, false) &&
            is_disjoint_add_drop(ha_alter_info)) {

            if (ctx->handler_flags & (Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX)) {
                // the fractal tree can not handle dropping an index concurrent with querying with the index.
                // we grab an exclusive MDL for the drop index.
                result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
            } else {
                result = HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE;

                // someday, allow multiple hot indexes via alter table add key. don't forget to change the store_lock function.
                // for now, hot indexing is only supported via session variable with the create index sql command
                if (ha_alter_info->index_add_count == 1 && ha_alter_info->index_drop_count == 0 &&  // only one add or drop
                    ctx->handler_flags == Alter_inplace_info::ADD_INDEX &&                          // must be add index not add unique index
                    thd_sql_command(thd) == SQLCOM_CREATE_INDEX &&                                  // must be a create index command
                    get_create_index_online(thd)) {                                                 // must be enabled
                    // external_lock set WRITE_ALLOW_WRITE which allows writes concurrent with the index creation
                    result = HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE; 
                }
            }
        }
    } else
    // column default
    if (only_flags(ctx->handler_flags, Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        if (table->s->null_bytes == altered_table->s->null_bytes)
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
    } else
    // column rename
    if (ctx->handler_flags & Alter_inplace_info::ALTER_COLUMN_NAME &&
        only_flags(ctx->handler_flags, Alter_inplace_info::ALTER_COLUMN_NAME + Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        // we have identified a possible column rename, 
        // but let's do some more checks
        
        // we will only allow an hcr if there are no changes
        // in column positions (ALTER_COLUMN_ORDER is not set)
        
        // now need to verify that one and only one column
        // has changed only its name. If we find anything to
        // the contrary, we don't allow it, also check indexes
        if (table->s->null_bytes == altered_table->s->null_bytes) {
            bool cr_supported = column_rename_supported(table, altered_table, (ctx->handler_flags & Alter_inplace_info::ALTER_COLUMN_ORDER) != 0);
            if (cr_supported)
                result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else    
    // add column
    if (ctx->handler_flags & Alter_inplace_info::ADD_COLUMN &&
        only_flags(ctx->handler_flags, Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER) &&
        setup_kc_info(altered_table, ctx->altered_table_kc_info) == 0) {

        uint32_t added_columns[altered_table->s->fields];
        uint32_t num_added_columns = 0;
        int r = find_changed_columns(added_columns, &num_added_columns, table, altered_table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE) {
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
    if (ctx->handler_flags & Alter_inplace_info::DROP_COLUMN &&
        only_flags(ctx->handler_flags, Alter_inplace_info::DROP_COLUMN + Alter_inplace_info::ALTER_COLUMN_ORDER) &&
        setup_kc_info(altered_table, ctx->altered_table_kc_info) == 0) {

        uint32_t dropped_columns[table->s->fields];
        uint32_t num_dropped_columns = 0;
        int r = find_changed_columns(dropped_columns, &num_dropped_columns, altered_table, table);
        if (r == 0) {
            if (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE) {
                for (uint32_t i = 0; i < num_dropped_columns; i++) {
                    uint32_t curr_dropped_index = dropped_columns[i];
                    Field* curr_dropped_field = table->field[curr_dropped_index];
                    printf("Dropped column: index %d, name %s\n", curr_dropped_index, curr_dropped_field->field_name);
                }
            }
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    // change column length
    if ((ctx->handler_flags & Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH) && 
        only_flags(ctx->handler_flags, Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH + Alter_inplace_info::ALTER_COLUMN_DEFAULT) &&
        table->s->fields == altered_table->s->fields &&
        find_changed_fields(table, altered_table, ctx->changed_fields) > 0 && 
        setup_kc_info(altered_table, ctx->altered_table_kc_info) == 0) {

        if (change_length_is_supported(table, altered_table, ha_alter_info, ctx)) {
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    // change column type
    if ((ctx->handler_flags & Alter_inplace_info::ALTER_COLUMN_TYPE) &&
        only_flags(ctx->handler_flags, Alter_inplace_info::ALTER_COLUMN_TYPE + Alter_inplace_info::ALTER_COLUMN_DEFAULT) && 
        table->s->fields == altered_table->s->fields && 
        find_changed_fields(table, altered_table, ctx->changed_fields) > 0 &&
        setup_kc_info(altered_table, ctx->altered_table_kc_info) == 0) {
        
        if (change_type_is_supported(table, altered_table, ha_alter_info, ctx)) {
            result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
        }
    } else
    if (only_flags(ctx->handler_flags, Alter_inplace_info::CHANGE_CREATE_OPTION)) {
        HA_CREATE_INFO *create_info = ha_alter_info->create_info;
#if TOKU_INCLUDE_OPTION_STRUCTS
        // set the USED_ROW_FORMAT flag for use later in this file for changes in the table's 
        // compression
        if (create_info->option_struct->row_format != table_share->option_struct->row_format)
            create_info->used_fields |= HA_CREATE_USED_ROW_FORMAT;
#endif
        // alter auto_increment
        if (only_flags(create_info->used_fields, HA_CREATE_USED_AUTO)) {
            // do a sanity check that the table is what we think it is
            if (tables_have_same_keys_and_columns(table, altered_table, THDVAR(thd, alter_print_error) != 0)) {
                result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
            }
        }
        // alter row_format
        else if (only_flags(create_info->used_fields, HA_CREATE_USED_ROW_FORMAT)) {
            // do a sanity check that the table is what we think it is
            if (tables_have_same_keys_and_columns(table, altered_table, THDVAR(thd, alter_print_error) != 0)) {
                result = HA_ALTER_INPLACE_EXCLUSIVE_LOCK;
            }
        }
    } 
#if TOKU_OPTIMIZE_WITH_RECREATE
    else if (only_flags(ctx->handler_flags, Alter_inplace_info::RECREATE_TABLE + Alter_inplace_info::ALTER_COLUMN_DEFAULT)) {
        ctx->optimize_needed = true;
        result = HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE;
    }
#endif

    if (result != HA_ALTER_INPLACE_NOT_SUPPORTED && table->s->null_bytes != altered_table->s->null_bytes &&
        (tokudb_debug & TOKUDB_DEBUG_ALTER_TABLE)) {
        TOKUDB_HANDLER_TRACE("q %s", thd->query());
        TOKUDB_HANDLER_TRACE("null bytes %u -> %u", table->s->null_bytes, altered_table->s->null_bytes);
    }

    // turn a not supported result into an error if the slow alter table (copy) is disabled
    if (result == HA_ALTER_INPLACE_NOT_SUPPORTED && get_disable_slow_alter(thd)) {
        print_error(HA_ERR_UNSUPPORTED, MYF(0));
        result = HA_ALTER_ERROR;
    }

    DBUG_RETURN(result);
}

// Prepare for the alter operations
bool ha_tokudb::prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_HANDLER_DBUG_ENTER("");
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    assert(transaction); // transaction must exist after table is locked
    ctx->alter_txn = transaction;
    bool result = false; // success
    DBUG_RETURN(result);
}

// Execute the alter operations.
bool ha_tokudb::inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    TOKUDB_HANDLER_DBUG_ENTER("");

    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    HA_CREATE_INFO *create_info = ha_alter_info->create_info;

    if (error == 0 && (ctx->handler_flags & (Alter_inplace_info::DROP_INDEX + Alter_inplace_info::DROP_UNIQUE_INDEX))) {
        error = alter_table_drop_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (ctx->handler_flags & (Alter_inplace_info::ADD_INDEX + Alter_inplace_info::ADD_UNIQUE_INDEX))) {
        error = alter_table_add_index(altered_table, ha_alter_info);
    }
    if (error == 0 && (ctx->handler_flags & (Alter_inplace_info::ADD_COLUMN + Alter_inplace_info::DROP_COLUMN))) { 
        error = alter_table_add_or_drop_column(altered_table, ha_alter_info);
    }
    if (error == 0 && (ctx->handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) && (create_info->used_fields & HA_CREATE_USED_AUTO)) {
        error = write_auto_inc_create(share->status_block, create_info->auto_increment_value, ctx->alter_txn);
    }
    if (error == 0 && (ctx->handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) && (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT)) {
        // Get the current compression
        DB *db = share->key_file[0];
        error = db->get_compression_method(db, &ctx->orig_compression_method);
        assert(error == 0);

        // Set the new compression
#if TOKU_INCLUDE_OPTION_STRUCTS
        toku_compression_method method = row_format_to_toku_compression_method((srv_row_format_t) create_info->option_struct->row_format);
#else
        toku_compression_method method = row_type_to_toku_compression_method(create_info->row_type);
#endif
        uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);
        for (uint32_t i = 0; i < curr_num_DBs; i++) {
            db = share->key_file[i];
            error = db->change_compression_method(db, method);
            if (error)
                break;
            ctx->compression_changed = true;
        }
    }

    // note: only one column expansion is allowed

    if (error == 0 && ctx->expand_fixed_update_needed)
        error = alter_table_expand_columns(altered_table, ha_alter_info);

    if (error == 0 && ctx->expand_varchar_update_needed)
        error = alter_table_expand_varchar_offsets(altered_table, ha_alter_info);

    if (error == 0 && ctx->expand_blob_update_needed) 
        error = alter_table_expand_blobs(altered_table, ha_alter_info);

    if (error == 0 && ctx->reset_card) {
        error = tokudb::alter_card(share->status_block, ctx->alter_txn, table->s, altered_table->s);
    }
    if (error == 0 && ctx->optimize_needed) {
        error = do_optimize(ha_thd());
    }

#if (50600 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
    (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799)
#if WITH_PARTITION_STORAGE_ENGINE
    if (error == 0 && (TOKU_PARTITION_WRITE_FRM_DATA || altered_table->part_info == NULL)) {
#else
    if (error == 0) {
#endif
        error = write_frm_data(share->status_block, ctx->alter_txn, altered_table->s->path.str);
    }
#endif

    bool result = false; // success
    if (error) {
        print_error(error, MYF(0));
        result = true;  // failure
    }

    DBUG_RETURN(result);
}

int ha_tokudb::alter_table_add_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {

    // sort keys in add index order
    KEY *key_info = (KEY*) tokudb_my_malloc(sizeof (KEY) * ha_alter_info->index_add_count, MYF(MY_WME));
    for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
        KEY *key = &key_info[i];
        *key = ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[i]];
        for (KEY_PART_INFO *key_part= key->key_part; key_part < key->key_part + get_key_parts(key); key_part++)
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

    tokudb_my_free(key_info);
    
    if (error == 0)
        ctx->reset_card = true;

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

int ha_tokudb::alter_table_drop_index(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
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

    if (error == 0)
        ctx->reset_card = true;

    return error;
}

int ha_tokudb::alter_table_add_or_drop_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    int error;
    uchar *column_extra = NULL;
    uint32_t max_column_extra_size;
    uint32_t num_column_extra;
    uint32_t num_columns = 0;
    uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);

    uint32_t columns[table->s->fields + altered_table->s->fields]; // set size such that we know it is big enough for both cases
    memset(columns, 0, sizeof(columns));

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
    column_extra = (uchar *)tokudb_my_malloc(max_column_extra_size, MYF(MY_WME));
    if (column_extra == NULL) { error = ENOMEM; goto cleanup; }
    
    for (uint32_t i = 0; i < curr_num_DBs; i++) {
        // change to a new descriptor
        DBT row_descriptor; memset(&row_descriptor, 0, sizeof row_descriptor);
        error = new_row_descriptor(table, altered_table, ha_alter_info, i, &row_descriptor);
        if (error)
            goto cleanup;
        error = share->key_file[i]->change_descriptor(share->key_file[i], ctx->alter_txn, &row_descriptor, 0);
        tokudb_my_free(row_descriptor.data);
        if (error)
            goto cleanup;
        
        if (i == primary_key || key_is_clustering(&table_share->key_info[i])) {
            num_column_extra = fill_row_mutator(
                                                column_extra,
                                                columns,
                                                num_columns,
                                                altered_table,
                                                ctx->altered_table_kc_info,
                                                i,
                                                (ha_alter_info->handler_flags & Alter_inplace_info::ADD_COLUMN) != 0 // true if adding columns, otherwise is a drop
                                                );
            
            DBT column_dbt; memset(&column_dbt, 0, sizeof column_dbt);
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
    tokudb_my_free(column_extra);
    return error;
}

// Commit or abort the alter operations.
// If commit then write the new frm data to the status using the alter transaction.
// If abort then abort the alter transaction and try to rollback the non-transactional changes.
bool ha_tokudb::commit_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit) {
    TOKUDB_HANDLER_DBUG_ENTER("");
    
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    bool result = false; // success
    THD *thd = ha_thd();

    if (commit) {
#if (50613 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50699) || \
    (50700 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50799) || \
    (100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099)
        if (ha_alter_info->group_commit_ctx) {
            ha_alter_info->group_commit_ctx = NULL;
        }
#endif
#if (50500 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 50599) || \
    (100000 <= MYSQL_VERSION_ID && MYSQL_VERSION_ID <= 100099)
#if WITH_PARTITION_STORAGE_ENGINE
        if (TOKU_PARTITION_WRITE_FRM_DATA || altered_table->part_info == NULL) {
#else
        if (true) {
#endif
            int error = write_frm_data(share->status_block, ctx->alter_txn, altered_table->s->path.str);
            if (error) {
                commit = false; 
                result = true;
                print_error(error, MYF(0));
            }
        }
#endif
    }

    if (!commit) {
        if (table->mdl_ticket->get_type() != MDL_EXCLUSIVE && 
            (ctx->add_index_changed || ctx->drop_index_changed || ctx->compression_changed)) {

            // get exclusive lock no matter what
#if defined(MARIADB_BASE_VERSION)
            killed_state saved_killed_state = thd->killed;
            thd->killed = NOT_KILLED;
            for (volatile uint i = 0; wait_while_table_is_used(thd, table, HA_EXTRA_NOT_USED); i++) {
                if (thd->killed != NOT_KILLED)
                    thd->killed = NOT_KILLED;
                sleep(1);
            }
            assert(table->mdl_ticket->get_type() == MDL_EXCLUSIVE);
            if (thd->killed == NOT_KILLED)
                thd->killed = saved_killed_state;
#else
            THD::killed_state saved_killed_state = thd->killed;
            thd->killed = THD::NOT_KILLED;
            // MySQL does not handle HA_EXTRA_NOT_USED so we use HA_EXTRA_PREPARE_FOR_RENAME since it is passed through 
            // the partition storage engine and is treated as a NOP by tokudb
            for (volatile uint i = 0; wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_RENAME); i++) {
                if (thd->killed != THD::NOT_KILLED)
                    thd->killed = THD::NOT_KILLED;
                sleep(1);
            }
            assert(table->mdl_ticket->get_type() == MDL_EXCLUSIVE);
            if (thd->killed == THD::NOT_KILLED)
                thd->killed = saved_killed_state;
#endif
        }

        // abort the alter transaction NOW so that any alters are rolled back. this allows the following restores to work.
        tokudb_trx_data *trx = (tokudb_trx_data *) thd_get_ha_data(thd, tokudb_hton);
        assert(ctx->alter_txn == trx->stmt);
        assert(trx->tokudb_lock_count > 0);
        // for partitioned tables, we use a single transaction to do all of the partition changes.  the tokudb_lock_count
        // is a reference count for each of the handlers to the same transaction.  obviously, we want to only abort once.
        if (trx->tokudb_lock_count > 0) {
            if (--trx->tokudb_lock_count <= trx->create_lock_count) {
                trx->create_lock_count = 0;
                abort_txn(ctx->alter_txn);
                ctx->alter_txn = NULL;
                trx->stmt = NULL;
                trx->sub_sp_level = NULL;
            }
            transaction = NULL;
        }

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
            uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);
            for (uint32_t i = 0; i < curr_num_DBs; i++) {
                DB *db = share->key_file[i];
                int error = db->change_compression_method(db, ctx->orig_compression_method);
                assert(error == 0);
            }
        }
    }
    
    DBUG_RETURN(result);
}

// Setup the altered table's key and col info.
int ha_tokudb::setup_kc_info(TABLE *altered_table, KEY_AND_COL_INFO *altered_kc_info) {
    int error = allocate_key_and_col_info(altered_table->s, altered_kc_info);
    if (error == 0)
        error = initialize_key_and_col_info(altered_table->s, altered_table, altered_kc_info, hidden_primary_key, primary_key);
    return error;
}

// Expand the variable length fields offsets from 1 to 2 bytes.
int ha_tokudb::alter_table_expand_varchar_offsets(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);

    uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);
    for (uint32_t i = 0; i < curr_num_DBs; i++) {
        // change to a new descriptor
        DBT row_descriptor; memset(&row_descriptor, 0, sizeof row_descriptor);
        error = new_row_descriptor(table, altered_table, ha_alter_info, i, &row_descriptor);
        if (error)
            break;
        error = share->key_file[i]->change_descriptor(share->key_file[i], ctx->alter_txn, &row_descriptor, 0);
        tokudb_my_free(row_descriptor.data);
        if (error)
            break;

        // for all trees that have values, make an update variable offsets message and broadcast it into the tree
        if (i == primary_key || key_is_clustering(&table_share->key_info[i])) {
            uint32_t offset_start = table_share->null_bytes + share->kc_info.mcp_info[i].fixed_field_size;
            uint32_t offset_end = offset_start + share->kc_info.mcp_info[i].len_of_offsets;
            uint32_t number_of_offsets = offset_end - offset_start;

            // make the expand variable offsets message
            DBT expand; memset(&expand, 0, sizeof expand);
            expand.size = sizeof (uchar) + sizeof offset_start + sizeof offset_end;
            expand.data = tokudb_my_malloc(expand.size, MYF(MY_WME));
            if (!expand.data) {
                error = ENOMEM;
                break;
            }
            uchar *expand_ptr = (uchar *)expand.data;
            expand_ptr[0] = UPDATE_OP_EXPAND_VARIABLE_OFFSETS;
            expand_ptr += sizeof (uchar);
        
            memcpy(expand_ptr, &number_of_offsets, sizeof number_of_offsets);
            expand_ptr += sizeof number_of_offsets;

            memcpy(expand_ptr, &offset_start, sizeof offset_start);
            expand_ptr += sizeof offset_start;

            // and broadcast it into the tree
            error = share->key_file[i]->update_broadcast(share->key_file[i], ctx->alter_txn, &expand, DB_IS_RESETTING_OP);
            tokudb_my_free(expand.data);
            if (error)
                break;
        }
    }

    return error;
}

// Return true if a field is part of a key
static bool field_in_key(KEY *key, Field *field) {
    for (uint i = 0; i < get_key_parts(key); i++) {
        KEY_PART_INFO *key_part = &key->key_part[i];
        if (strcmp(key_part->field->field_name, field->field_name) == 0)
            return true;
    }
    return false;
}

// Return true if a field is part of any key
static bool field_in_key_of_table(TABLE *table, Field *field) {
    for (uint i = 0; i < table->s->keys; i++) {
        if (field_in_key(&table->key_info[i], field))
            return true;
    }
    return false;
}

// Return true if all changed varchar/varbinary field lengths can be changed inplace, otherwise return false
static bool change_varchar_length_is_supported(Field *old_field, Field *new_field, TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx) {
    if (old_field->real_type() != MYSQL_TYPE_VARCHAR || 
        new_field->real_type() != MYSQL_TYPE_VARCHAR || 
        old_field->binary() != new_field->binary() || 
        old_field->charset()->number != new_field->charset()->number ||
        old_field->field_length > new_field->field_length)
        return false;
    if (ctx->table_kc_info->num_offset_bytes > ctx->altered_table_kc_info->num_offset_bytes)
        return false; // shrink is not supported
    if (ctx->table_kc_info->num_offset_bytes < ctx->altered_table_kc_info->num_offset_bytes)
        ctx->expand_varchar_update_needed = true; // sum of varchar lengths changed from 1 to 2
    return true;
}

// Return true if all changed field lengths can be changed inplace, otherwise return false
static bool change_length_is_supported(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx) {
    if (table->s->fields != altered_table->s->fields)
        return false;
    if (table->s->null_bytes != altered_table->s->null_bytes)
        return false;
    if (ctx->changed_fields.elements() > 1)
        return false; // only support one field change
    for (DYNAMIC_ARRAY_ELEMENTS_TYPE ai = 0; ai < ctx->changed_fields.elements(); ai++) {
        uint i = ctx->changed_fields.at(ai);
        Field *old_field = table->field[i];
        Field *new_field = altered_table->field[i];
        if (old_field->real_type() != new_field->real_type())
            return false; // no type conversions
        if (old_field->real_type() != MYSQL_TYPE_VARCHAR)
            return false; // only varchar
        if (field_in_key_of_table(table, old_field) || field_in_key_of_table(altered_table, new_field))
            return false; // not in any key
        if (!change_varchar_length_is_supported(old_field, new_field, table, altered_table, ha_alter_info, ctx))
            return false;
    }

    return true;
}

// Debug function that ensures that the array is sorted
static bool is_sorted(Dynamic_array<uint> &a) {
    bool r = true;
    if (a.elements() > 0) {
        uint lastelement = a.at(0);
        for (DYNAMIC_ARRAY_ELEMENTS_TYPE i = 1; i < a.elements(); i++)
            if (lastelement > a.at(i))
                r = false;
    }
    return r;
}

int ha_tokudb::alter_table_expand_columns(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    assert(is_sorted(ctx->changed_fields)); // since we build the changed_fields array in field order, it must be sorted
    for (DYNAMIC_ARRAY_ELEMENTS_TYPE ai = 0; error == 0 && ai < ctx->changed_fields.elements(); ai++) {
        uint expand_field_num = ctx->changed_fields.at(ai);
        error = alter_table_expand_one_column(altered_table, ha_alter_info, expand_field_num);
    }

    return error;
}

// Return true if the field is an unsigned int
static bool is_unsigned(Field *f) {
    return (f->flags & UNSIGNED_FLAG) != 0;
}

// Return the starting offset in the value for a particular index (selected by idx) of a
// particular field (selected by expand_field_num)
// TODO: replace this?
static uint32_t alter_table_field_offset(uint32_t null_bytes, KEY_AND_COL_INFO *kc_info, int idx, int expand_field_num) {
    uint32_t offset = null_bytes;
    for (int i = 0; i < expand_field_num; i++) {
        if (bitmap_is_set(&kc_info->key_filters[idx], i)) // skip key fields
            continue;
        offset += kc_info->field_lengths[i];
    }
    return offset;
}

// Send an expand message into all clustered indexes including the primary
int ha_tokudb::alter_table_expand_one_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info, int expand_field_num) {
    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);

    Field *old_field = table->field[expand_field_num];
    TOKU_TYPE old_field_type = mysql_to_toku_type(old_field);
    Field *new_field = altered_table->field[expand_field_num];
    TOKU_TYPE new_field_type = mysql_to_toku_type(new_field);
    assert(old_field_type == new_field_type);

    uchar operation;
    uchar pad_char;
    switch (old_field_type) {
    case toku_type_int:
        assert(is_unsigned(old_field) == is_unsigned(new_field));
        if (is_unsigned(old_field))
            operation = UPDATE_OP_EXPAND_UINT;
        else
            operation = UPDATE_OP_EXPAND_INT;
        pad_char = 0;
        break;
    case toku_type_fixstring:
        operation = UPDATE_OP_EXPAND_CHAR;
        pad_char = old_field->charset()->pad_char;
        break;
    case toku_type_fixbinary:
        operation = UPDATE_OP_EXPAND_BINARY;
        pad_char = 0;
        break;
    default:
        assert(0);
    }

    uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);
    for (uint32_t i = 0; i < curr_num_DBs; i++) {
        // change to a new descriptor
        DBT row_descriptor; memset(&row_descriptor, 0, sizeof row_descriptor);
        error = new_row_descriptor(table, altered_table, ha_alter_info, i, &row_descriptor);
        if (error)
            break;
        error = share->key_file[i]->change_descriptor(share->key_file[i], ctx->alter_txn, &row_descriptor, 0);
        tokudb_my_free(row_descriptor.data);
        if (error)
            break;

        // for all trees that have values, make an expand update message and broadcast it into the tree
        if (i == primary_key || key_is_clustering(&table_share->key_info[i])) {
            uint32_t old_offset = alter_table_field_offset(table_share->null_bytes, ctx->table_kc_info, i, expand_field_num);
            uint32_t new_offset = alter_table_field_offset(table_share->null_bytes, ctx->altered_table_kc_info, i, expand_field_num);
            assert(old_offset <= new_offset);

            uint32_t old_length = ctx->table_kc_info->field_lengths[expand_field_num];
            assert(old_length == old_field->pack_length());

            uint32_t new_length = ctx->altered_table_kc_info->field_lengths[expand_field_num];
            assert(new_length == new_field->pack_length());

            DBT expand; memset(&expand, 0, sizeof expand);
            expand.size = sizeof operation + sizeof new_offset + sizeof old_length + sizeof new_length;
            if (operation == UPDATE_OP_EXPAND_CHAR || operation == UPDATE_OP_EXPAND_BINARY)
                expand.size += sizeof pad_char;
            expand.data = tokudb_my_malloc(expand.size, MYF(MY_WME));
            if (!expand.data) {
                error = ENOMEM;
                break;
            }
            uchar *expand_ptr = (uchar *)expand.data;
            expand_ptr[0] = operation;
            expand_ptr += sizeof operation;

            // for the first altered field, old_offset == new_offset.  for the subsequent altered fields, the new_offset
            // should be used as it includes the length changes from the previous altered fields.
            memcpy(expand_ptr, &new_offset, sizeof new_offset);
            expand_ptr += sizeof new_offset;

            memcpy(expand_ptr, &old_length, sizeof old_length);
            expand_ptr += sizeof old_length;

            memcpy(expand_ptr, &new_length, sizeof new_length);
            expand_ptr += sizeof new_length;

            if (operation == UPDATE_OP_EXPAND_CHAR || operation == UPDATE_OP_EXPAND_BINARY) {
                memcpy(expand_ptr, &pad_char, sizeof pad_char);
                expand_ptr += sizeof pad_char;
            }

            assert(expand_ptr == (uchar *)expand.data + expand.size);

            // and broadcast it into the tree
            error = share->key_file[i]->update_broadcast(share->key_file[i], ctx->alter_txn, &expand, DB_IS_RESETTING_OP);
            tokudb_my_free(expand.data);
            if (error)
                break;
        }
    }

    return error;
}

static void marshall_blob_lengths(tokudb::buffer &b, uint32_t n, TABLE *table, KEY_AND_COL_INFO *kc_info) {
    for (uint i = 0; i < n; i++) {
        uint blob_field_index = kc_info->blob_fields[i];
        assert(blob_field_index < table->s->fields);
        uint8_t blob_field_length = table->s->field[blob_field_index]->row_pack_length();
        b.append(&blob_field_length, sizeof blob_field_length);
    }
}

int ha_tokudb::alter_table_expand_blobs(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);

    uint32_t curr_num_DBs = table->s->keys + tokudb_test(hidden_primary_key);
    for (uint32_t i = 0; i < curr_num_DBs; i++) {
        // change to a new descriptor
        DBT row_descriptor; memset(&row_descriptor, 0, sizeof row_descriptor);
        error = new_row_descriptor(table, altered_table, ha_alter_info, i, &row_descriptor);
        if (error)
            break;
        error = share->key_file[i]->change_descriptor(share->key_file[i], ctx->alter_txn, &row_descriptor, 0);
        tokudb_my_free(row_descriptor.data);
        if (error)
            break;

        // for all trees that have values, make an update blobs message and broadcast it into the tree
        if (i == primary_key || key_is_clustering(&table_share->key_info[i])) {
            tokudb::buffer b;
            uint8_t op = UPDATE_OP_EXPAND_BLOB;
            b.append(&op, sizeof op);
            b.append_ui<uint32_t>(table->s->null_bytes + ctx->table_kc_info->mcp_info[i].fixed_field_size);
            uint32_t var_offset_bytes = ctx->table_kc_info->mcp_info[i].len_of_offsets;
            b.append_ui<uint32_t>(var_offset_bytes);
            b.append_ui<uint32_t>(var_offset_bytes == 0 ? 0 : ctx->table_kc_info->num_offset_bytes);
            
            // add blobs info
            uint32_t num_blobs = ctx->table_kc_info->num_blobs;
            b.append_ui<uint32_t>(num_blobs);
            marshall_blob_lengths(b, num_blobs, table, ctx->table_kc_info);
            marshall_blob_lengths(b, num_blobs, altered_table, ctx->altered_table_kc_info);

            // and broadcast it into the tree
            DBT expand; memset(&expand, 0, sizeof expand);
            expand.data = b.data();
            expand.size = b.size();
            error = share->key_file[i]->update_broadcast(share->key_file[i], ctx->alter_txn, &expand, DB_IS_RESETTING_OP);
            if (error)
                break;
        }
    }

    return error;
}

// Return true if two fixed length fields can be changed inplace
static bool change_fixed_length_is_supported(TABLE *table, TABLE *altered_table, Field *old_field, Field *new_field, tokudb_alter_ctx *ctx) {
    // no change in size is supported
    if (old_field->pack_length() == new_field->pack_length())
        return true;
    // shrink is not supported
    if (old_field->pack_length() > new_field->pack_length())
        return false;
    ctx->expand_fixed_update_needed = true;
    return true;
}

static bool change_blob_length_is_supported(TABLE *table, TABLE *altered_table, Field *old_field, Field *new_field, tokudb_alter_ctx *ctx) {
    // blob -> longer or equal length blob
    if (old_field->binary() && new_field->binary() && old_field->pack_length() <= new_field->pack_length()) {
        ctx->expand_blob_update_needed = true;
        return true;
    }
    // text -> longer or equal length text
    if (!old_field->binary() && !new_field->binary() &&
        old_field->pack_length() <= new_field->pack_length() &&
        old_field->charset()->number == new_field->charset()->number) {
        ctx->expand_blob_update_needed = true;
        return true;
    }
    return false;
}

// Return true if the MySQL type is an int or unsigned int type
static bool is_int_type(enum_field_types t) {
    switch (t) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
        return true;
    default:
        return false;
    }
}

// Return true if two field types can be changed inplace
static bool change_field_type_is_supported(Field *old_field, Field *new_field, TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx) {
    enum_field_types old_type = old_field->real_type();
    enum_field_types new_type = new_field->real_type();
    if (is_int_type(old_type)) {
        // int and unsigned int expansion
        if (is_int_type(new_type) && is_unsigned(old_field) == is_unsigned(new_field))
            return change_fixed_length_is_supported(table, altered_table, old_field, new_field, ctx);
        else
            return false;
    } else if (old_type == MYSQL_TYPE_STRING) {
        // char(X) -> char(Y) and binary(X) -> binary(Y) expansion
        if (new_type == MYSQL_TYPE_STRING && 
            old_field->binary() == new_field->binary() && 
            old_field->charset()->number == new_field->charset()->number)
            return change_fixed_length_is_supported(table, altered_table, old_field, new_field, ctx);
        else
            return false;
    } else if (old_type == MYSQL_TYPE_VARCHAR) {
        // varchar(X) -> varchar(Y) and varbinary(X) -> varbinary(Y) expansion where X < 256 <= Y
        // the ALTER_COLUMN_TYPE handler flag is set for these cases
        return change_varchar_length_is_supported(old_field, new_field, table, altered_table, ha_alter_info, ctx);
    } else if (old_type == MYSQL_TYPE_BLOB && new_type == MYSQL_TYPE_BLOB) {
        return change_blob_length_is_supported(table, altered_table, old_field, new_field, ctx);
    } else
        return false;
}

// Return true if all changed field types can be changed inplace
static bool change_type_is_supported(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, tokudb_alter_ctx *ctx) {
    if (table->s->null_bytes != altered_table->s->null_bytes)
        return false;
    if (table->s->fields != altered_table->s->fields)
        return false;
    if (ctx->changed_fields.elements() > 1)
        return false; // only support one field change
    for (DYNAMIC_ARRAY_ELEMENTS_TYPE  ai = 0; ai < ctx->changed_fields.elements(); ai++) {
        uint i = ctx->changed_fields.at(ai);
        Field *old_field = table->field[i];
        Field *new_field = altered_table->field[i];
        if (field_in_key_of_table(table, old_field) || field_in_key_of_table(altered_table, new_field))
            return false;
        if (!change_field_type_is_supported(old_field, new_field, table, altered_table, ha_alter_info, ctx))
            return false;            
    }
    return true;
}

// Allocate and initialize a new descriptor for a dictionary in the altered table identified with idx.
// Return the new descriptor in the row_descriptor DBT.
// Return non-zero on error.
int ha_tokudb::new_row_descriptor(TABLE *table, TABLE *altered_table, Alter_inplace_info *ha_alter_info, uint32_t idx, DBT *row_descriptor) {
    int error = 0;
    tokudb_alter_ctx *ctx = static_cast<tokudb_alter_ctx *>(ha_alter_info->handler_ctx);
    row_descriptor->size = get_max_desc_size(ctx->altered_table_kc_info, altered_table);
    row_descriptor->data = (uchar *) tokudb_my_malloc(row_descriptor->size, MYF(MY_WME));
    if (row_descriptor->data == NULL) {
        error = ENOMEM;
    } else {
        KEY* prim_key = hidden_primary_key ? NULL : &altered_table->s->key_info[primary_key];
        if (idx == primary_key) {
            row_descriptor->size = create_main_key_descriptor((uchar *)row_descriptor->data,
                                                              prim_key,
                                                              hidden_primary_key,
                                                              primary_key,
                                                              altered_table,
                                                              ctx->altered_table_kc_info);
        } else {
            row_descriptor->size = create_secondary_key_descriptor((uchar *)row_descriptor->data,
                                                                   &altered_table->key_info[idx],
                                                                   prim_key,
                                                                   hidden_primary_key,
                                                                   altered_table,
                                                                   primary_key,
                                                                   idx,
                                                                   ctx->altered_table_kc_info);
        }
        error = 0;
    }
    return error;
}

#endif
