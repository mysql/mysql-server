/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/*
  Single table and multi table updates of tables.
  Multi-table updates were introduced by Sinisa & Monty
*/

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_update.h"
#include "sql_cache.h"                          // query_cache_*
#include "sql_base.h"                       // close_tables_for_reopen
#include "sql_parse.h"                          // cleanup_items
#include "sql_partition.h"                   // partition_key_modified
#include "sql_select.h"
#include "sql_view.h"                           // check_key_in_view
#include "sp_head.h"
#include "sql_trigger.h"
#include "probes_mysql.h"
#include "debug_sync.h"
#include "key.h"                                // is_key_used
#include "sql_acl.h"                            // *_ACL, check_grant
#include "records.h"                            // init_read_record,
                                                // end_read_record
#include "filesort.h"                           // filesort
#include "opt_explain.h"
#include "sql_derived.h" // mysql_derived_prepare,
                         // mysql_handle_derived,
                         // mysql_derived_filling
#include "opt_trace.h"   // Opt_trace_object
#include "sql_tmp_table.h"                      // tmp tables
#include "sql_optimizer.h"                      // remove_eq_conds
#include "sql_resolver.h"                       // setup_order, fix_inner_refs

/**
   True if the table's input and output record buffers are comparable using
   compare_records(TABLE*).
 */
bool records_are_comparable(const TABLE *table) {
  return ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) == 0) ||
    bitmap_is_subset(table->write_set, table->read_set);
}


/**
   Compares the input and outbut record buffers of the table to see if a row
   has changed. The algorithm iterates over updated columns and if they are
   nullable compares NULL bits in the buffer before comparing actual
   data. Special care must be taken to compare only the relevant NULL bits and
   mask out all others as they may be undefined. The storage engine will not
   and should not touch them.

   @param table The table to evaluate.

   @return true if row has changed.
   @return false otherwise.
*/
bool compare_records(const TABLE *table)
{
  DBUG_ASSERT(records_are_comparable(table));

  if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0)
  {
    /*
      Storage engine may not have read all columns of the record.  Fields
      (including NULL bits) not in the write_set may not have been read and
      can therefore not be compared.
    */ 
    for (Field **ptr= table->field ; *ptr != NULL; ptr++)
    {
      Field *field= *ptr;
      if (bitmap_is_set(table->write_set, field->field_index))
      {
        if (field->real_maybe_null())
        {
          uchar null_byte_index= field->null_offset();
          
          if (((table->record[0][null_byte_index]) & field->null_bit) !=
              ((table->record[1][null_byte_index]) & field->null_bit))
            return TRUE;
        }
        if (field->cmp_binary_offset(table->s->rec_buff_length))
          return TRUE;
      }
    }
    return FALSE;
  }
  
  /* 
     The storage engine has read all columns, so it's safe to compare all bits
     including those not in the write_set. This is cheaper than the field-by-field
     comparison done above.
  */ 
  if (table->s->blob_fields + table->s->varchar_fields == 0)
    // Fixed-size record: do bitwise comparison of the records 
    return cmp_record(table,record[1]);
  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->s->rec_buff_length,
	     table->s->null_bytes))
    return TRUE;				// Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr= table->field ; *ptr ; ptr++)
  {
    if (bitmap_is_set(table->write_set, (*ptr)->field_index) &&
	(*ptr)->cmp_binary_offset(table->s->rec_buff_length))
      return TRUE;
  }
  return FALSE;
}


/*
  check that all fields are real fields

  SYNOPSIS
    check_fields()
    thd             thread handler
    items           Items for check

  RETURN
    TRUE  Items can't be used in UPDATE
    FALSE Items are OK
*/

static bool check_fields(THD *thd, List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item;
  Item_field *field;

  while ((item= it++))
  {
    if (!(field= item->field_for_view_update()))
    {
      /* item has name, because it comes from VIEW SELECT list */
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), item->item_name.ptr());
      return TRUE;
    }
    /*
      we make temporary copy of Item_field, to avoid influence of changing
      result_field on Item_ref which refer on this field
    */
    thd->change_item_tree(it.ref(), new Item_field(thd, field));
  }
  return FALSE;
}


/**
  Check if all expressions in list are constant expressions

  @param[in] values List of expressions

  @retval true Only constant expressions
  @retval false At least one non-constant expression
*/

static bool check_constant_expressions(List<Item> &values)
{
  Item *value;
  List_iterator_fast<Item> v(values);
  DBUG_ENTER("check_constant_expressions");

  while ((value= v++))
  {
    if (!value->const_item())
    {
      DBUG_PRINT("exit", ("expression is not constant"));
      DBUG_RETURN(false);
    }
  }
  DBUG_PRINT("exit", ("expression is constant"));
  DBUG_RETURN(true);
}


/*
  Process usual UPDATE

  SYNOPSIS
    mysql_update()
    thd			thread handler
    fields		fields for update
    values		values of fields for update
    conds		WHERE clause expression
    order_num		number of elemen in ORDER BY clause
    order		ORDER BY clause list
    limit		limit clause
    handle_duplicates	how to handle duplicates

  RETURN
    0  - OK
    2  - privilege check and openning table passed, but we need to convert to
         multi-update because of view substitution
    1  - error
*/

int mysql_update(THD *thd,
                 TABLE_LIST *table_list,
                 List<Item> &fields,
		 List<Item> &values,
                 Item *conds,
                 uint order_num, ORDER *order,
		 ha_rows limit,
		 enum enum_duplicates handle_duplicates, bool ignore,
                 ha_rows *found_return, ha_rows *updated_return)
{
  bool		using_limit= limit != HA_POS_ERROR;
  bool		safe_update= test(thd->variables.option_bits & OPTION_SAFE_UPDATES);
  bool          used_key_is_modified= FALSE, transactional_table, will_batch;
  int           res;
  int           error= 1;
  int           loc_error;
  uint          used_index, dup_key_found;
  bool          need_sort= TRUE;
  bool          reverse= FALSE;
  bool          using_filesort;
  bool          read_removal= false;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint		want_privilege;
#endif
  ha_rows	updated, found;
  key_map	old_covering_keys;
  TABLE		*table;
  SQL_SELECT	*select= NULL;
  READ_RECORD	info;
  SELECT_LEX    *select_lex= &thd->lex->select_lex;
  ulonglong     id;
  List<Item> all_fields;
  THD::killed_state killed_status= THD::NOT_KILLED;
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &fields, &values);

  DBUG_ENTER("mysql_update");

  if (open_normal_and_derived_tables(thd, table_list, 0))
    DBUG_RETURN(1);

  if (table_list->multitable_view)
  {
    DBUG_ASSERT(table_list->view != 0);
    DBUG_PRINT("info", ("Switch to multi-update"));
    /* convert to multiupdate */
    DBUG_RETURN(2);
  }

  THD_STAGE_INFO(thd, stage_init);
  table= table_list->table;

  if (!table_list->updatable)
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "UPDATE");
    DBUG_RETURN(1);
  }

  /* Calculate "table->covering_keys" based on the WHERE */
  table->covering_keys= table->s->keys_in_use;
  table->quick_keys.clear_all();

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Force privilege re-checking for views after they have been opened. */
  want_privilege= (table_list->view ? UPDATE_ACL :
                   table_list->grant.want_privilege);
#endif
  if (mysql_prepare_update(thd, table_list, &conds, order_num, order))
    DBUG_RETURN(1);

  old_covering_keys= table->covering_keys;		// Keys used in WHERE
  /* Check the fields we are going to modify */
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table_list->grant.want_privilege= table->grant.want_privilege= want_privilege;
  table_list->register_want_access(want_privilege);
#endif
  if (setup_fields_with_no_wrap(thd, Ref_ptr_array(),
                                fields, MARK_COLUMNS_WRITE, 0, 0))
    DBUG_RETURN(1);                     /* purecov: inspected */
  if (table_list->view && check_fields(thd, fields))
  {
    DBUG_RETURN(1);
  }
  if (!table_list->updatable || check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "UPDATE");
    DBUG_RETURN(1);
  }

  if (update.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Check values */
  table_list->grant.want_privilege= table->grant.want_privilege=
    (SELECT_ACL & ~table->grant.privilege);
#endif
  if (setup_fields(thd, Ref_ptr_array(), values, MARK_COLUMNS_READ, 0, 0))
  {
    free_underlaid_joins(thd, select_lex);
    DBUG_RETURN(1);				/* purecov: inspected */
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(thd, all_fields, select_lex, select_lex->ref_pointer_array))
    DBUG_RETURN(1);

  if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
      update.function_defaults_apply(table))
    /*
      A column is to be set to its ON UPDATE function default only if other
      columns of the row are changing. To know this, we must be able to
      compare the "before" and "after" value of those columns
      (i.e. records_are_comparable() must be true below). Thus, we must read
      those columns:
    */
    bitmap_union(table->read_set, table->write_set);

  // Don't count on usage of 'only index' when calculating which key to use
  table->covering_keys.clear_all();

  /*
    This must be done before partitioning pruning, since prune_partitions()
    uses the table->write_set to determine may prune locks too.
  */
  if (table->triggers)
    table->triggers->mark_fields_used(TRG_EVENT_UPDATE);

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (table->part_info)
  {
    if (prune_partitions(thd, table, conds))
      DBUG_RETURN(1);
    if (table->all_partitions_pruned_away)
    {
      /* No matching records */
      if (thd->lex->describe)
      {
        error= explain_no_table(thd,
                                "No matching rows after partition pruning");
        goto exit_without_my_ok;
      }
      my_ok(thd);                            // No matching records
      DBUG_RETURN(0);
    }
  }
#endif
  if (lock_tables(thd, table_list, thd->lex->table_count, 0))
    DBUG_RETURN(1);

  // Must be done after lock_tables()
  if (conds)
  {
    COND_EQUAL *cond_equal= NULL;
    Item::cond_result result;
    if (table_list->check_option)
    {
      /*
        If this UPDATE is on a view with CHECK OPTION, Item_fields
        must not be replaced by constants. The reason is that when
        'conds' is optimized, 'check_option' is also optimized (it is
        part of 'conds'). Const replacement is fine for 'conds'
        because it is evaluated on a read row, but 'check_option' is
        evaluated on a row with updated fields and needs those updated
        values to be correct.

        Example:
        CREATE VIEW v1 ... WHERE fld < 2 WITH CHECK_OPTION
        UPDATE v1 SET fld=4 WHERE fld=1

        check_option is  "(fld < 2)"
        conds is         "(fld < 2) and (fld = 1)"

        optimize_cond() would propagate fld=1 to the first argument of
        the AND to create "(1 < 2) AND (fld = 1)". After this,
        check_option would be "(1 < 2)". But for check_option to work
        it must be evaluated with the *updated* value of fld: 4.
        Otherwise it will evaluate to true even when it should be
        false, which is the case for the UPDATE statement above.

        Thus, if there is a check_option, we do only the "safe" parts
        of optimize_cond(): Item_row -> Item_func_eq conversion (to
        enable range access) and removal of always true/always false
        predicates.

        An alternative to restricting this optimization of 'conds' in
        the presense of check_option: the Item-tree of 'check_option'
        could be cloned before optimizing 'conds' and thereby avoid
        const replacement. However, at the moment there is no such
        thing as Item::clone().
      */
      conds= build_equal_items(thd, conds, NULL, false,
                               select_lex->join_list, &cond_equal);
      conds= remove_eq_conds(thd, conds, &result);
    }
    else
      conds= optimize_cond(thd, conds, &cond_equal, select_lex->join_list,
                           true, &result);

    if (result == Item::COND_FALSE)
    {
      limit= 0;                                   // Impossible WHERE
      if (thd->lex->describe)
      {
        error= explain_no_table(thd, "Impossible WHERE");
        goto exit_without_my_ok;
      }
    }
    if (conds)
    {
      conds= substitute_for_best_equal_field(conds, cond_equal, 0);
      conds->update_used_tables();
    }
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /*
    Also try a second time after locking, to prune when subqueries and
    stored programs can be evaluated.
  */
  if (table->part_info)
  {
    if (prune_partitions(thd, table, conds))
      DBUG_RETURN(1);
    if (table->all_partitions_pruned_away)
    {
      /* No matching records */
      if (thd->lex->describe)
      {
        error= explain_no_table(thd,
                                "No matching rows after partition pruning");
        goto exit_without_my_ok;
      }
      my_ok(thd);                            // No matching records
      DBUG_RETURN(0);
    }
  }
#endif
  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->mark_columns_needed_for_update();
  select= make_select(table, 0, 0, conds, 0, &error);

  { // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(table);

    if (error || !limit ||
        (select && select->check_quick(thd, safe_update, limit)))
    {
      if (thd->lex->describe && !error && !thd->is_error())
      {
        error= explain_no_table(thd, "Impossible WHERE");
        goto exit_without_my_ok;
      }
      delete select;
      free_underlaid_joins(thd, select_lex);
      /*
        There was an error or the error was already sent by
        the quick select evaluation.
        TODO: Add error code output parameter to Item::val_xxx() methods.
        Currently they rely on the user checking DA for
        errors when unwinding the stack after calling Item::val_xxx().
      */
      if (error || thd->is_error())
      {
        DBUG_RETURN(1);				// Error in where
      }

      char buff[MYSQL_ERRMSG_SIZE];
      my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), 0, 0,
                  (ulong) thd->get_stmt_da()->current_statement_warn_count());
      my_ok(thd, 0, 0, buff);

      DBUG_PRINT("info",("0 records updated"));
      DBUG_RETURN(0);
    }
  } // Ends scope for optimizer trace wrapper

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
		 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      goto exit_without_my_ok;
    }
  }
  init_ftfuncs(thd, select_lex, 1);

  table->update_const_key_parts(conds);
  order= simple_remove_const(order, conds);
        
  used_index= get_index_for_order(order, table, select, limit,
                                  &need_sort, &reverse);
  if (need_sort)
  { // Assign table scan index to check below for modified key fields:
    used_index= table->file->key_used_on_scan;
  }
  if (used_index != MAX_KEY)
  { // Check if we are modifying a key that we are used to search with:
    used_key_is_modified= is_key_used(table, used_index, table->write_set);
  }
  else if (select && select->quick)
  {
    /*
      select->quick != NULL and used_index == MAX_KEY happens for index
      merge and should be handled in a different way.
    */
    used_key_is_modified= (!select->quick->unique_key_range() &&
                           select->quick->is_keys_used(table->write_set));
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  used_key_is_modified|= partition_key_modified(table, table->write_set);
#endif

  using_filesort= order && (need_sort||used_key_is_modified);
  if (thd->lex->describe)
  {
    const bool using_tmp_table= !using_filesort &&
                                (used_key_is_modified || order);
    error= explain_single_table_modification(thd, table, select, used_index,
                                             limit, using_tmp_table,
                                             using_filesort,
                                             true,
                                             used_key_is_modified);
    goto exit_without_my_ok;
  }

  if (used_key_is_modified || order)
  {
    /*
      We can't update table directly;  We must first search after all
      matching rows before updating the table!
    */

    if (used_index < MAX_KEY && old_covering_keys.is_set(used_index))
      table->set_keyread(true);

    /* note: We avoid sorting if we sort on the used index */
    if (using_filesort)
    {
      /*
	Doing an ORDER BY;  Let filesort find and sort the rows we are going
	to update
        NOTE: filesort will call table->prepare_for_position()
      */
      ha_rows examined_rows;
      ha_rows found_rows;
      Filesort fsort(order, limit, select);

      table->sort.io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
						    MYF(MY_FAE | MY_ZEROFILL));
      if ((table->sort.found_records= filesort(thd, table, &fsort, true,
                                               &examined_rows, &found_rows))
          == HA_POS_ERROR)
      {
        goto exit_without_my_ok;
      }
      thd->inc_examined_row_count(examined_rows);
      /*
	Filesort has already found and selected the rows we want to update,
	so we don't need the where clause
      */
      delete select;
      select= 0;
    }
    else
    {
      /*
	We are doing a search on a key that is updated. In this case
	we go trough the matching rows, save a pointer to them and
	update these in a separate loop based on the pointer.
      */
      table->prepare_for_position();

      IO_CACHE tempfile;
      if (open_cached_file(&tempfile, mysql_tmpdir,TEMP_PREFIX,
			   DISK_BUFFER_SIZE, MYF(MY_WME)))
        goto exit_without_my_ok;

      /* If quick select is used, initialize it before retrieving rows. */
      if (select && select->quick && select->quick->reset())
        goto exit_without_my_ok;
      table->file->try_semi_consistent_read(1);

      /*
        When we get here, we have one of the following options:
        A. used_index == MAX_KEY
           This means we should use full table scan, and start it with
           init_read_record call
        B. used_index != MAX_KEY
           B.1 quick select is used, start the scan with init_read_record
           B.2 quick select is not used, this is full index scan (with LIMIT)
               Full index scan must be started with init_read_record_idx
      */

      if (used_index == MAX_KEY || (select && select->quick))
        error= init_read_record(&info, thd, table, select, 0, 1, FALSE);
      else
        error= init_read_record_idx(&info, thd, table, 1, used_index, reverse);

      if (error)
        goto exit_without_my_ok;

      THD_STAGE_INFO(thd, stage_searching_rows_for_update);
      ha_rows tmp_limit= limit;

      while (!(error=info.read_record(&info)) && !thd->killed)
      {
        thd->inc_examined_row_count(1);
        bool skip_record= FALSE;
        if (select && select->skip_record(thd, &skip_record))
        {
          error= 1;
          table->file->unlock_row();
          break;
        }
        if (!skip_record)
	{
          if (table->file->was_semi_consistent_read())
	    continue;  /* repeat the read of the same row if it still exists */

	  table->file->position(table->record[0]);
	  if (my_b_write(&tempfile,table->file->ref,
			 table->file->ref_length))
	  {
	    error=1; /* purecov: inspected */
	    break; /* purecov: inspected */
	  }
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
	else
	  table->file->unlock_row();
      }
      if (thd->killed && !error)
	error= 1;				// Aborted
      limit= tmp_limit;
      table->file->try_semi_consistent_read(0);
      end_read_record(&info);
     
      /* Change select to use tempfile */
      if (select)
      {
        select->set_quick(NULL);
        if (select->free_cond)
          delete select->cond;
        select->cond= NULL;
      }
      else
      {
	select= new SQL_SELECT;
	select->head=table;
      }
      if (reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
	error=1; /* purecov: inspected */
      select->file=tempfile;			// Read row ptrs from this file
      if (error >= 0)
        goto exit_without_my_ok;
    }
    if (used_index < MAX_KEY && old_covering_keys.is_set(used_index))
      table->set_keyread(false);
  }

  if (ignore)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  
  if (select && select->quick && select->quick->reset())
    goto exit_without_my_ok;
  table->file->try_semi_consistent_read(1);
  if ((error= init_read_record(&info, thd, table, select, 0, 1, FALSE)))
    goto exit_without_my_ok;

  updated= found= 0;
  /*
    Generate an error (in TRADITIONAL mode) or warning
    when trying to set a NOT NULL field to NULL.
  */
  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields=0L;
  THD_STAGE_INFO(thd, stage_updating);

  transactional_table= table->file->has_transactions();
  thd->abort_on_warning= (!ignore && thd->is_strict_mode());

  if (table->triggers &&
      table->triggers->has_triggers(TRG_EVENT_UPDATE,
                                    TRG_ACTION_AFTER))
  {
    /*
      The table has AFTER UPDATE triggers that might access to subject 
      table and therefore might need update to be done immediately. 
      So we turn-off the batching.
    */ 
    (void) table->file->extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
    will_batch= FALSE;
  }
  else
    will_batch= !table->file->start_bulk_update();

  if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
      !ignore && !using_limit &&
      select && select->quick && select->quick->index != MAX_KEY &&
      check_constant_expressions(values))
    read_removal= table->check_read_removal(select->quick->index);

  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    thd->inc_examined_row_count(1);
    bool skip_record;
    if (!select || (!select->skip_record(thd, &skip_record) && !skip_record))
    {
      if (table->file->was_semi_consistent_read())
        continue;  /* repeat the read of the same row if it still exists */

      store_record(table,record[1]);
      if (fill_record_n_invoke_before_triggers(thd, fields, values, 0,
                                               table->triggers,
                                               TRG_EVENT_UPDATE))
        break; /* purecov: inspected */

      found++;

      if (!records_are_comparable(table) || compare_records(table))
      {
        if ((res= table_list->view_check_option(thd, ignore)) !=
            VIEW_CHECK_OK)
        {
          found--;
          if (res == VIEW_CHECK_SKIP)
            continue;
          else if (res == VIEW_CHECK_ERROR)
          {
            error= 1;
            break;
          }
        }

        /*
          In order to keep MySQL legacy behavior, we do this update *after*
          the CHECK OPTION test. Proper behavior is probably to throw an
          error, though.
        */
        update.set_function_defaults(table);

        if (will_batch)
        {
          /*
            Typically a batched handler can execute the batched jobs when:
            1) When specifically told to do so
            2) When it is not a good idea to batch anymore
            3) When it is necessary to send batch for other reasons
               (One such reason is when READ's must be performed)

            1) is covered by exec_bulk_update calls.
            2) and 3) is handled by the bulk_update_row method.
            
            bulk_update_row can execute the updates including the one
            defined in the bulk_update_row or not including the row
            in the call. This is up to the handler implementation and can
            vary from call to call.

            The dup_key_found reports the number of duplicate keys found
            in those updates actually executed. It only reports those if
            the extra call with HA_EXTRA_IGNORE_DUP_KEY have been issued.
            If this hasn't been issued it returns an error code and can
            ignore this number. Thus any handler that implements batching
            for UPDATE IGNORE must also handle this extra call properly.

            If a duplicate key is found on the record included in this
            call then it should be included in the count of dup_key_found
            and error should be set to 0 (only if these errors are ignored).
          */
          error= table->file->ha_bulk_update_row(table->record[1],
                                                 table->record[0],
                                                 &dup_key_found);
          limit+= dup_key_found;
          updated-= dup_key_found;
        }
        else
        {
          /* Non-batched update */
	  error= table->file->ha_update_row(table->record[1],
                                            table->record[0]);
        }
        if (!error || error == HA_ERR_RECORD_IS_THE_SAME)
	{
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            updated++;
          else
            error= 0;
	}
 	else if (!ignore ||
                 table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
	{
          /*
            If (ignore && error is ignorable) we don't have to
            do anything; otherwise...
          */
          myf flags= 0;

          if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
            flags|= ME_FATALERROR; /* Other handler errors are fatal */

	  table->file->print_error(error,MYF(flags));
	  error= 1;
	  break;
	}
      }

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, TRUE))
      {
        error= 1;
        break;
      }

      if (!--limit && using_limit)
      {
        /*
          We have reached end-of-file in most common situations where no
          batching has occurred and if batching was supposed to occur but
          no updates were made and finally when the batch execution was
          performed without error and without finding any duplicate keys.
          If the batched updates were performed with errors we need to
          check and if no error but duplicate key's found we need to
          continue since those are not counted for in limit.
        */
        if (will_batch &&
            ((error= table->file->exec_bulk_update(&dup_key_found)) ||
             dup_key_found))
        {
 	  if (error)
          {
            /* purecov: begin inspected */
            /*
              The handler should not report error of duplicate keys if they
              are ignored. This is a requirement on batching handlers.
            */
            table->file->print_error(error,MYF(0));
            error= 1;
            break;
            /* purecov: end */
          }
          /*
            Either an error was found and we are ignoring errors or there
            were duplicate keys found. In both cases we need to correct
            the counters and continue the loop.
          */
          limit= dup_key_found; //limit is 0 when we get here so need to +
          updated-= dup_key_found;
        }
        else
        {
	  error= -1;				// Simulate end of file
	  break;
        }
      }
    }
    else
      table->file->unlock_row();
    thd->get_stmt_da()->inc_current_row_for_warning();
    if (thd->is_error())
    {
      error= 1;
      break;
    }
  }
  table->auto_increment_field_not_null= FALSE;
  dup_key_found= 0;
  /*
    Caching the killed status to pass as the arg to query event constuctor;
    The cached value can not change whereas the killed status can
    (externally) since this point and change of the latter won't affect
    binlogging.
    It's assumed that if an error was set in combination with an effective 
    killed status then the error is due to killing.
  */
  killed_status= thd->killed; // get the status of the volatile 
  // simulated killing after the loop must be ineffective for binlogging
  DBUG_EXECUTE_IF("simulate_kill_bug27571",
                  {
                    thd->killed= THD::KILL_QUERY;
                  };);
  error= (killed_status == THD::NOT_KILLED)?  error : 1;
  
  if (error &&
      will_batch &&
      (loc_error= table->file->exec_bulk_update(&dup_key_found)))
    /*
      An error has occurred when a batched update was performed and returned
      an error indication. It cannot be an allowed duplicate key error since
      we require the batching handler to treat this as a normal behavior.

      Otherwise we simply remove the number of duplicate keys records found
      in the batched update.
    */
  {
    /* purecov: begin inspected */
    table->file->print_error(loc_error,MYF(ME_FATALERROR));
    error= 1;
    /* purecov: end */
  }
  else
    updated-= dup_key_found;
  if (will_batch)
    table->file->end_bulk_update();
  table->file->try_semi_consistent_read(0);

  if (read_removal)
  {
    /* Only handler knows how many records really was written */
    updated= table->file->end_read_removal();
    if (!records_are_comparable(table))
      found= updated;
  }

  if (!transactional_table && updated > 0)
    thd->transaction.stmt.mark_modified_non_trans_table();

  end_read_record(&info);
  delete select;
  THD_STAGE_INFO(thd, stage_end);
  (void) table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /*
    Invalidate the table in the query cache if something changed.
    This must be before binlog writing and ha_autocommit_...
  */
  if (updated)
  {
    query_cache_invalidate3(thd, table_list, 1);
  }
  
  /*
    error < 0 means really no error at all: we processed all rows until the
    last one without error. error > 0 means an error (e.g. unique key
    violation and no IGNORE or REPLACE). error == 0 is also an error (if
    preparing the record or invoking before triggers fails). See
    ha_autocommit_or_rollback(error>=0) and DBUG_RETURN(error>=0) below.
    Sometimes we want to binlog even if we updated no rows, in case user used
    it to be sure master and slave are in same state.
  */
  if ((error < 0) || thd->transaction.stmt.cannot_safely_rollback())
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query(), thd->query_length(),
                            transactional_table, FALSE, FALSE, errcode))
      {
        error=1;				// Rollback update
      }
    }
  }
  DBUG_ASSERT(transactional_table || !updated ||
              thd->transaction.stmt.cannot_safely_rollback());
  free_underlaid_joins(thd, select_lex);

  /* If LAST_INSERT_ID(X) was used, report X */
  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;

  if (error < 0)
  {
    char buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), (ulong) found,
                (ulong) updated,
                (ulong) thd->get_stmt_da()->current_statement_warn_count());
    my_ok(thd, (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
          id, buff);
    DBUG_PRINT("info",("%ld records updated", (long) updated));
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		/* calc cuted fields */
  thd->abort_on_warning= 0;
  *found_return= found;
  *updated_return= updated;
  DBUG_RETURN((error >= 0 || thd->is_error()) ? 1 : 0);

exit_without_my_ok:
  delete select;
  free_underlaid_joins(thd, select_lex);
  table->set_keyread(FALSE);
  thd->abort_on_warning= 0;
  DBUG_RETURN(error);
}

/*
  Prepare items in UPDATE statement

  SYNOPSIS
    mysql_prepare_update()
    thd			- thread handler
    table_list		- global/local table list
    conds		- conditions
    order_num		- number of ORDER BY list entries
    order		- ORDER BY clause list

  RETURN VALUE
    FALSE OK
    TRUE  error
*/
bool mysql_prepare_update(THD *thd, TABLE_LIST *table_list,
			 Item **conds, uint order_num, ORDER *order)
{
  Item *fake_conds= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  TABLE *table= table_list->table;
#endif
  List<Item> all_fields;
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  DBUG_ENTER("mysql_prepare_update");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table_list->grant.want_privilege= table->grant.want_privilege= 
    (SELECT_ACL & ~table->grant.privilege);
  table_list->register_want_access(SELECT_ACL);
#endif

  thd->lex->allow_sum_func= 0;

  if (setup_tables_and_check_access(thd, &select_lex->context, 
                                    &select_lex->top_join_list,
                                    table_list,
                                    &select_lex->leaf_tables,
                                    FALSE, UPDATE_ACL, SELECT_ACL) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      select_lex->setup_ref_array(thd, order_num) ||
      setup_order(thd, select_lex->ref_pointer_array,
		  table_list, all_fields, all_fields, order) ||
      setup_ftfuncs(select_lex))
    DBUG_RETURN(TRUE);

  /* Check that we are not using table that we are updating in a sub select */
  {
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 0)))
    {
      update_non_unique_table_error(table_list, "UPDATE", duplicate);
      DBUG_RETURN(TRUE);
    }
  }
  select_lex->fix_prepare_information(thd, conds, &fake_conds);
  DBUG_RETURN(FALSE);
}


/***************************************************************************
  Update multiple tables from join 
***************************************************************************/

/*
  Get table map for list of Item_field
*/

static table_map get_table_map(List<Item> *items)
{
  List_iterator_fast<Item> item_it(*items);
  Item_field *item;
  table_map map= 0;

  while ((item= (Item_field *) item_it++)) 
    map|= item->used_tables();
  DBUG_PRINT("info", ("table_map: 0x%08lx", (long) map));
  return map;
}

/**
  If one row is updated through two different aliases and the first
  update physically moves the row, the second update will error
  because the row is no longer located where expected. This function
  checks if the multiple-table update is about to do that and if so
  returns with an error.

  The following update operations physically moves rows:
    1) Update of a column in a clustered primary key
    2) Update of a column used to calculate which partition the row belongs to

  This function returns with an error if both of the following are
  true:

    a) A table in the multiple-table update statement is updated
       through multiple aliases (including views)
    b) At least one of the updates on the table from a) may physically
       moves the row. Note: Updating a column used to calculate which
       partition a row belongs to does not necessarily mean that the
       row is moved. The new value may or may not belong to the same
       partition.

  @param leaves               First leaf table
  @param tables_for_update    Map of tables that are updated

  @return
    true   if the update is unsafe, in which case an error message is also set,
    false  otherwise.
*/
static
bool unsafe_key_update(TABLE_LIST *leaves, table_map tables_for_update)
{
  TABLE_LIST *tl= leaves;

  for (tl= leaves; tl ; tl= tl->next_leaf)
  {
    if (tl->table->map & tables_for_update)
    {
      TABLE *table1= tl->table;
      bool primkey_clustered= (table1->file->primary_key_is_clustered() &&
                               table1->s->primary_key != MAX_KEY);

      bool table_partitioned= false;
#ifdef WITH_PARTITION_STORAGE_ENGINE
      table_partitioned= (table1->part_info != NULL);
#endif

      if (!table_partitioned && !primkey_clustered)
        continue;

      for (TABLE_LIST* tl2= tl->next_leaf; tl2 ; tl2= tl2->next_leaf)
      {
        /*
          Look at "next" tables only since all previous tables have
          already been checked
        */
        TABLE *table2= tl2->table;
        if (table2->map & tables_for_update && table1->s == table2->s)
        {
#ifdef WITH_PARTITION_STORAGE_ENGINE
          // A table is updated through two aliases
          if (table_partitioned &&
              (partition_key_modified(table1, table1->write_set) ||
               partition_key_modified(table2, table2->write_set)))
          {
            // Partitioned key is updated
            my_error(ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                     tl->belong_to_view ? tl->belong_to_view->alias
                                        : tl->alias,
                     tl2->belong_to_view ? tl2->belong_to_view->alias
                                         : tl2->alias);
            return true;
          }
#endif

          if (primkey_clustered)
          {
            // The primary key can cover multiple columns
            KEY key_info= table1->key_info[table1->s->primary_key];
            KEY_PART_INFO *key_part= key_info.key_part;
            KEY_PART_INFO *key_part_end= key_part +
              key_info.user_defined_key_parts;

            for (;key_part != key_part_end; ++key_part)
            {
              if (bitmap_is_set(table1->write_set, key_part->fieldnr-1) ||
                  bitmap_is_set(table2->write_set, key_part->fieldnr-1))
              {
                // Clustered primary key is updated
                my_error(ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                         tl->belong_to_view ? tl->belong_to_view->alias
                         : tl->alias,
                         tl2->belong_to_view ? tl2->belong_to_view->alias
                         : tl2->alias);
                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}


/**
  Check if there is enough privilege on specific table used by the
  main select list of multi-update directly or indirectly (through
  a view).

  @param[in]      thd                Thread context.
  @param[in]      table              Table list element for the table.
  @param[in]      tables_for_update  Bitmap with tables being updated.
  @param[in/out]  updated_arg        Set to true if table in question is
                                     updated, also set to true if it is
                                     a view and one of its underlying
                                     tables is updated. Should be
                                     initialized to false by the caller
                                     before a sequence of calls to this
                                     function.

  @note To determine which tables/views are updated we have to go from
        leaves to root since tables_for_update contains map of leaf
        tables being updated and doesn't include non-leaf tables
        (fields are already resolved to leaf tables).

  @retval false - Success, all necessary privileges on all tables are
                  present or might be present on column-level.
  @retval true  - Failure, some necessary privilege on some table is
                  missing.
*/

static bool multi_update_check_table_access(THD *thd, TABLE_LIST *table,
                                            table_map tables_for_update,
                                            bool *updated_arg)
{
  if (table->view)
  {
    bool updated= false;
    /*
      If it is a mergeable view then we need to check privileges on its
      underlying tables being merged (including views). We also need to
      check if any of them is updated in order to find if this view is
      updated.
      If it is a non-mergeable view then it can't be updated.
    */
    DBUG_ASSERT(table->merge_underlying_list ||
                (!table->updatable &&
                 !(table->table->map & tables_for_update)));

    for (TABLE_LIST *tbl= table->merge_underlying_list; tbl;
         tbl= tbl->next_local)
    {
      if (multi_update_check_table_access(thd, tbl, tables_for_update, &updated))
        return true;
    }
    if (check_table_access(thd, updated ? UPDATE_ACL: SELECT_ACL, table,
                           FALSE, 1, FALSE))
      return true;
    *updated_arg|= updated;
    /* We only need SELECT privilege for columns in the values list. */
    table->grant.want_privilege= SELECT_ACL & ~table->grant.privilege;
  }
  else
  {
    /* Must be a base or derived table. */
    const bool updated= table->table->map & tables_for_update;
    if (check_table_access(thd, updated ? UPDATE_ACL : SELECT_ACL, table,
                           FALSE, 1, FALSE))
      return true;
    *updated_arg|= updated;
    /* We only need SELECT privilege for columns in the values list. */
    if (!table->derived)
    {
      table->grant.want_privilege= SELECT_ACL & ~table->grant.privilege;
      table->table->grant.want_privilege= SELECT_ACL & ~table->table->grant.privilege;
    }
  }
  return false;
}


/*
  make update specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_update_prepare()
    thd         thread handler

  RETURN
    FALSE OK
    TRUE  Error
*/

int mysql_multi_update_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  TABLE_LIST *table_list= lex->query_tables;
  TABLE_LIST *tl, *leaves;
  List<Item> *fields= &lex->select_lex.item_list;
  table_map tables_for_update;
  bool update_view= 0;
  const bool using_lock_tables= thd->locked_tables_mode != LTM_NONE;
  bool original_multiupdate= (thd->lex->sql_command == SQLCOM_UPDATE_MULTI);
  DBUG_ENTER("mysql_multi_update_prepare");

  /* following need for prepared statements, to run next time multi-update */
  thd->lex->sql_command= SQLCOM_UPDATE_MULTI;

  /*
    Open tables and create derived ones, but do not lock and fill them yet.

    During prepare phase acquire only S metadata locks instead of SW locks to
    keep prepare of multi-UPDATE compatible with concurrent LOCK TABLES WRITE
    and global read lock.
  */
  if (original_multiupdate &&
      open_normal_and_derived_tables(thd, table_list,
                                     (thd->stmt_arena->is_stmt_prepare() ?
                                      MYSQL_OPEN_FORCE_SHARED_MDL : 0)))
    DBUG_RETURN(TRUE);
  /*
    setup_tables() need for VIEWs. JOIN::prepare() will call setup_tables()
    second time, but this call will do nothing (there are check for second
    call in setup_tables()).
  */

  if (setup_tables(thd, &lex->select_lex.context,
                   &lex->select_lex.top_join_list,
                   table_list, &lex->select_lex.leaf_tables,
                   FALSE))
    DBUG_RETURN(TRUE);

  if (setup_fields_with_no_wrap(thd, Ref_ptr_array(),
                                *fields, MARK_COLUMNS_WRITE, 0, 0))
    DBUG_RETURN(TRUE);

  /*
   Setting tl->updating= false for view as it is correctly set
   for tables below
  */
  for (tl= table_list; tl ; tl= tl->next_local)
  {
    if (tl->view)
    {
      update_view= 1;
      tl->updating= false;
    }
  }

  if (update_view && check_fields(thd, *fields))
  {
    DBUG_RETURN(TRUE);
  }

  thd->table_map_for_update= tables_for_update= get_table_map(fields);

  leaves= lex->select_lex.leaf_tables;

  if (unsafe_key_update(leaves, tables_for_update))
    DBUG_RETURN(true);

  /*
    Setup timestamp handling and locking mode
  */
  for (tl= leaves; tl; tl= tl->next_leaf)
  {
    TABLE *table= tl->table;

    /* if table will be updated then check that it is unique */
    if (table->map & tables_for_update)
    {
      if (!tl->updatable || check_key_in_view(thd, tl))
      {
        my_error(ER_NON_UPDATABLE_TABLE, MYF(0), tl->alias, "UPDATE");
        DBUG_RETURN(TRUE);
      }

      DBUG_PRINT("info",("setting table `%s` for update", tl->alias));
      /*
        If table will be updated we should not downgrade lock for it and
        leave it as is.
      */
    }
    else
    {
      DBUG_PRINT("info",("setting table `%s` for read-only", tl->alias));
      /*
        If we are using the binary log, we need TL_READ_NO_INSERT to get
        correct order of statements. Otherwise, we use a TL_READ lock to
        improve performance.
        We don't downgrade metadata lock from SW to SR in this case as
        there is no guarantee that the same ticket is not used by
        another table instance used by this statement which is going to
        be write-locked (for example, trigger to be invoked might try
        to update this table).
        Last argument routine_modifies_data for read_lock_type_for_table()
        is ignored, as prelocking placeholder will never be set here.
      */
      DBUG_ASSERT(tl->prelocking_placeholder == false);
      tl->lock_type= read_lock_type_for_table(thd, lex, tl, true);
      tl->updating= 0;
      /* Update TABLE::lock_type accordingly. */
      if (!tl->placeholder() && !using_lock_tables)
        tl->table->reginfo.lock_type= tl->lock_type;
    }
  }

  /*
    Check access privileges for tables being updated or read.
    Note that unlike in the above loop we need to iterate here not only
    through all leaf tables but also through all view hierarchy.
  */
  for (tl= table_list; tl; tl= tl->next_local)
  {
    bool not_used= false;
    if (multi_update_check_table_access(thd, tl, tables_for_update, &not_used))
      DBUG_RETURN(TRUE);
  }

  /* check single table update for view compound from several tables */
  for (tl= table_list; tl; tl= tl->next_local)
  {
    if (tl->effective_algorithm == VIEW_ALGORITHM_MERGE)
    {
      TABLE_LIST *for_update= 0;
      if (tl->check_single_table(&for_update, tables_for_update, tl))
      {
	my_error(ER_VIEW_MULTIUPDATE, MYF(0),
		 tl->view_db.str, tl->view_name.str);
	DBUG_RETURN(-1);
      }
    }
  }

  /* @todo: downgrade the metadata locks here. */

  /*
    Check that we are not using table that we are updating, but we should
    skip all tables of UPDATE SELECT itself
  */
  lex->select_lex.exclude_from_table_unique_test= TRUE;
  for (tl= leaves; tl; tl= tl->next_leaf)
  {
    if (tl->lock_type != TL_READ &&
        tl->lock_type != TL_READ_NO_INSERT)
    {
      TABLE_LIST *duplicate;
      if ((duplicate= unique_table(thd, tl, table_list, 0)))
      {
        update_non_unique_table_error(table_list, "UPDATE", duplicate);
        DBUG_RETURN(TRUE);
      }
    }
  }
  /*
    Set exclude_from_table_unique_test value back to FALSE. It is needed for
    further check in multi_update::prepare whether to use record cache.
  */
  lex->select_lex.exclude_from_table_unique_test= FALSE;
  DBUG_RETURN (FALSE);
}


/*
  Setup multi-update handling and call SELECT to do the join
*/

bool mysql_multi_update(THD *thd,
                        TABLE_LIST *table_list,
                        List<Item> *fields,
                        List<Item> *values,
                        Item *conds,
                        ulonglong options,
                        enum enum_duplicates handle_duplicates,
                        bool ignore,
                        SELECT_LEX_UNIT *unit,
                        SELECT_LEX *select_lex,
                        multi_update **result)
{
  bool res;
  DBUG_ENTER("mysql_multi_update");

  if (!(*result= new multi_update(table_list,
				 thd->lex->select_lex.leaf_tables,
				 fields, values,
				 handle_duplicates, ignore)))
  {
    DBUG_RETURN(TRUE);
  }

  thd->abort_on_warning= (!ignore && thd->is_strict_mode());

  if (thd->lex->describe)
    res= explain_multi_table_modification(thd, *result);
  else
  {
    List<Item> total_list;

    res= mysql_select(thd,
                      table_list, select_lex->with_wild,
                      total_list,
                      conds, (SQL_I_List<ORDER> *) NULL,
                      (SQL_I_List<ORDER> *)NULL, (Item *) NULL,
                      options | SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                      OPTION_SETUP_TABLES_DONE,
                      *result, unit, select_lex);

    DBUG_PRINT("info",("res: %d  report_error: %d",res, (int) thd->is_error()));
    res|= thd->is_error();
    if (unlikely(res))
    {
      /* If we had a another error reported earlier then this will be ignored */
      (*result)->send_error(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR));
      (*result)->abort_result_set();
    }
  }
  thd->abort_on_warning= 0;
  DBUG_RETURN(res);
}


multi_update::multi_update(TABLE_LIST *table_list,
			   TABLE_LIST *leaves_list,
			   List<Item> *field_list, List<Item> *value_list,
			   enum enum_duplicates handle_duplicates_arg,
                           bool ignore_arg)
  :all_tables(table_list), leaves(leaves_list), update_tables(0),
   tmp_tables(0), updated(0), found(0), fields(field_list),
   values(value_list), table_count(0), copy_field(0),
   handle_duplicates(handle_duplicates_arg), do_update(1), trans_safe(1),
   transactional_tables(0), ignore(ignore_arg), error_handled(0),
   update_operations(NULL)
{}


/*
  Connect fields with tables and create list of tables that are updated
*/

int multi_update::prepare(List<Item> &not_used_values,
			  SELECT_LEX_UNIT *lex_unit)
{
  TABLE_LIST *table_ref;
  SQL_I_List<TABLE_LIST> update;
  table_map tables_to_update;
  Item_field *item;
  List_iterator_fast<Item> field_it(*fields);
  List_iterator_fast<Item> value_it(*values);
  uint i, max_fields;
  uint leaf_table_count= 0;
  DBUG_ENTER("multi_update::prepare");

  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields=0L;
  THD_STAGE_INFO(thd, stage_updating_main_table);

  tables_to_update= get_table_map(fields);

  if (!tables_to_update)
  {
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
    DBUG_RETURN(1);
  }

  /*
    We gather the set of columns read during evaluation of SET expression in
    TABLE::tmp_set by pointing TABLE::read_set to it and then restore it after
    setup_fields().
  */
  for (table_ref= leaves; table_ref; table_ref= table_ref->next_leaf)
  {
    TABLE *table= table_ref->table;
    if (tables_to_update & table->map)
    {
      DBUG_ASSERT(table->read_set == &table->def_read_set);
      table->read_set= &table->tmp_set;
      bitmap_clear_all(table->read_set);
    }
  }

  /*
    We have to check values after setup_tables to get covering_keys right in
    reference tables
  */

  int error= setup_fields(thd, Ref_ptr_array(),
                          *values, MARK_COLUMNS_READ, 0, 0);

  for (table_ref= leaves; table_ref; table_ref= table_ref->next_leaf)
  {
    TABLE *table= table_ref->table;
    if (tables_to_update & table->map)
    {
      table->read_set= &table->def_read_set;
      bitmap_union(table->read_set, &table->tmp_set);
    }
  }
  
  if (error)
    DBUG_RETURN(1);    

  /*
    Save tables beeing updated in update_tables
    update_table->shared is position for table
    Don't use key read on tables that are updated
  */

  update.empty();
  for (table_ref= leaves; table_ref; table_ref= table_ref->next_leaf)
  {
    /* TODO: add support of view of join support */
    TABLE *table=table_ref->table;
    leaf_table_count++;
    if (tables_to_update & table->map)
    {
      TABLE_LIST *tl= (TABLE_LIST*) thd->memdup(table_ref,
						sizeof(*tl));
      if (!tl)
	DBUG_RETURN(1);
      update.link_in_list(tl, &tl->next_local);
      tl->shared= table_count++;
      table->no_keyread=1;
      table->covering_keys.clear_all();
      table->pos_in_table_list= tl;
      if (table->triggers &&
          table->triggers->has_triggers(TRG_EVENT_UPDATE,
                                        TRG_ACTION_AFTER))
      {
	/*
           The table has AFTER UPDATE triggers that might access to subject 
           table and therefore might need update to be done immediately. 
           So we turn-off the batching.
	*/ 
	(void) table->file->extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
      }
    }
  }


  table_count=  update.elements;
  update_tables= update.first;

  tmp_tables = (TABLE**) thd->calloc(sizeof(TABLE *) * table_count);
  tmp_table_param = (TMP_TABLE_PARAM*) thd->calloc(sizeof(TMP_TABLE_PARAM) *
						   table_count);
  fields_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  values_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);

  DBUG_ASSERT(update_operations == NULL);
  update_operations= (COPY_INFO**) thd->calloc(sizeof(COPY_INFO*) *
                                               table_count);

  if (thd->is_fatal_error)
    DBUG_RETURN(1);
  for (i=0 ; i < table_count ; i++)
  {
    fields_for_table[i]= new List_item;
    values_for_table[i]= new List_item;
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Split fields into fields_for_table[] and values_by_table[] */

  while ((item= (Item_field *) field_it++))
  {
    Item *value= value_it++;
    uint offset= item->field->table->pos_in_table_list->shared;
    fields_for_table[offset]->push_back(item);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Allocate copy fields */
  max_fields=0;
  for (i=0 ; i < table_count ; i++)
    set_if_bigger(max_fields, fields_for_table[i]->elements + leaf_table_count);
  copy_field= new Copy_field[max_fields];


  for (TABLE_LIST *ref= leaves; ref != NULL; ref= ref->next_leaf)
  {
    TABLE *table= ref->table;
    if (tables_to_update & table->map)
    {
      const uint position= table->pos_in_table_list->shared;
      List<Item> *cols= fields_for_table[position];
      List<Item> *vals= values_for_table[position];
      COPY_INFO *update=
        new (thd->mem_root) COPY_INFO(COPY_INFO::UPDATE_OPERATION, cols, vals);
      if (update == NULL ||
          update->add_function_default_columns(table, table->write_set))
        DBUG_RETURN(1);

      update_operations[position]= update;

      if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
          update->function_defaults_apply(table))
      {
        /*
          A column is to be set to its ON UPDATE function default only if
          other columns of the row are changing. To know this, we must be able
          to compare the "before" and "after" value of those columns. Thus, we
          must read those columns:
        */
        bitmap_union(table->read_set, table->write_set);
      }
      /* All needed columns must be marked before prune_partitions(). */
      if (table->triggers)
        table->triggers->mark_fields_used(TRG_EVENT_UPDATE);
    }
  }

  DBUG_RETURN(thd->is_fatal_error != 0);
}


/*
  Check if table is safe to update on fly

  SYNOPSIS
    safe_update_on_fly()
    thd                 Thread handler
    join_tab            How table is used in join
    all_tables          List of tables

  NOTES
    We can update the first table in join on the fly if we know that
    a row in this table will never be read twice. This is true under
    the following conditions:

    - No column is both written to and read in SET expressions.

    - We are doing a table scan and the data is in a separate file (MyISAM) or
      if we don't update a clustered key.

    - We are doing a range scan and we don't update the scan key or
      the primary key for a clustered table handler.

    - Table is not joined to itself.

    This function gets information about fields to be updated from
    the TABLE::write_set bitmap.

  WARNING
    This code is a bit dependent of how make_join_readinfo() works.

    The field table->tmp_set is used for keeping track of which fields are
    read during evaluation of the SET expression. See multi_update::prepare.

  RETURN
    0		Not safe to update
    1		Safe to update
*/

static bool safe_update_on_fly(THD *thd, JOIN_TAB *join_tab,
                               TABLE_LIST *table_ref, TABLE_LIST *all_tables)
{
  TABLE *table= join_tab->table;
  if (unique_table(thd, table_ref, all_tables, 0))
    return 0;
  switch (join_tab->type) {
  case JT_SYSTEM:
  case JT_CONST:
  case JT_EQ_REF:
    return TRUE;				// At most one matching row
  case JT_REF:
  case JT_REF_OR_NULL:
    return !is_key_used(table, join_tab->ref.key, table->write_set);
  case JT_ALL:
    if (bitmap_is_overlapping(&table->tmp_set, table->write_set))
      return FALSE;
    /* If range search on index */
    if (join_tab->quick)
      return !join_tab->quick->is_keys_used(table->write_set);
    /* If scanning in clustered key */
    if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
	table->s->primary_key < MAX_KEY)
      return !is_key_used(table, table->s->primary_key, table->write_set);
    return TRUE;
  default:
    break;					// Avoid compler warning
  }
  return FALSE;

}


/*
  Initialize table for multi table

  IMPLEMENTATION
    - Update first table in join on the fly, if possible
    - Create temporary tables to store changed values for all other tables
      that are updated (and main_table if the above doesn't hold).
*/

bool
multi_update::initialize_tables(JOIN *join)
{
  TABLE_LIST *table_ref;
  DBUG_ENTER("initialize_tables");

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    DBUG_RETURN(1);
  main_table=join->join_tab->table;
  table_to_update= 0;

  /* Any update has at least one pair (field, value) */
  DBUG_ASSERT(fields->elements);
  /*
   Only one table may be modified by UPDATE of an updatable view.
   For an updatable view first_table_for_update indicates this
   table.
   For a regular multi-update it refers to some updated table.
  */ 
  TABLE *first_table_for_update= ((Item_field *) fields->head())->field->table;

  /* Create a temporary table for keys to all tables, except main table */
  for (table_ref= update_tables; table_ref; table_ref= table_ref->next_local)
  {
    TABLE *table=table_ref->table;
    uint cnt= table_ref->shared;
    List<Item> temp_fields;
    ORDER     group;
    TMP_TABLE_PARAM *tmp_param;

    if (ignore)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table == main_table)			// First table in join
    {
      if (safe_update_on_fly(thd, join->join_tab, table_ref, all_tables))
      {
        table->mark_columns_needed_for_update();
	table_to_update= table;			// Update table on the fly
	continue;
      }
    }
    table->mark_columns_needed_for_update();

    /*
      enable uncacheable flag if we update a view with check option
      and check option has a subselect, otherwise, the check option
      can be evaluated after the subselect was freed as independent
      (See full_local in JOIN::join_free()).
    */
    if (table_ref->check_option && !join->select_lex->uncacheable)
    {
      SELECT_LEX_UNIT *tmp_unit;
      SELECT_LEX *sl;
      for (tmp_unit= join->select_lex->first_inner_unit();
           tmp_unit;
           tmp_unit= tmp_unit->next_unit())
      {
        for (sl= tmp_unit->first_select(); sl; sl= sl->next_select())
        {
          if (sl->master_unit()->item)
          {
            join->select_lex->uncacheable|= UNCACHEABLE_CHECKOPTION;
            goto loop_end;
          }
        }
      }
    }
loop_end:

    if (table == first_table_for_update && table_ref->check_option)
    {
      table_map unupdated_tables= table_ref->check_option->used_tables() &
                                  ~first_table_for_update->map;
      for (TABLE_LIST *tbl_ref =leaves;
           unupdated_tables && tbl_ref;
           tbl_ref= tbl_ref->next_leaf)
      {
        if (unupdated_tables & tbl_ref->table->map)
          unupdated_tables&= ~tbl_ref->table->map;
        else
          continue;
        if (unupdated_check_opt_tables.push_back(tbl_ref->table))
          DBUG_RETURN(1);
      }
    }

    tmp_param= tmp_table_param+cnt;

    /*
      Create a temporary table to store all fields that are changed for this
      table. The first field in the temporary table is a pointer to the
      original row so that we can find and update it. For the updatable
      VIEW a few following fields are rowids of tables used in the CHECK
      OPTION condition.
    */

    List_iterator_fast<TABLE> tbl_it(unupdated_check_opt_tables);
    TABLE *tbl= table;
    do
    {
      /*
        Signal each table (including tables referenced by WITH CHECK OPTION
        clause) for which we will store row position in the temporary table
        that we need a position to be read first.
      */
      tbl->prepare_for_position();

      Field_string *field= new Field_string(tbl->file->ref_length, 0,
                                            tbl->alias, &my_charset_bin);
      if (!field)
        DBUG_RETURN(1);
      field->init(tbl);
      /*
        The field will be converted to varstring when creating tmp table if
        table to be updated was created by mysql 4.1. Deny this.
      */
      field->can_alter_field_type= 0;
      Item_field *ifield= new Item_field((Field *) field);
      if (!ifield)
         DBUG_RETURN(1);
      ifield->maybe_null= 0;
      if (temp_fields.push_back(ifield))
        DBUG_RETURN(1);
    } while ((tbl= tbl_it++));

    temp_fields.concat(fields_for_table[cnt]);

    /* Make an unique key over the first field to avoid duplicated updates */
    memset(&group, 0, sizeof(group));
    group.direction= ORDER::ORDER_ASC;
    group.item= (Item**) temp_fields.head_ref();

    tmp_param->quick_group=1;
    tmp_param->field_count=temp_fields.elements;
    tmp_param->group_parts=1;
    tmp_param->group_length= table->file->ref_length;
    /* small table, ignore SQL_BIG_TABLES */
    my_bool save_big_tables= thd->variables.big_tables; 
    thd->variables.big_tables= FALSE;
    tmp_tables[cnt]=create_tmp_table(thd, tmp_param, temp_fields,
                                     (ORDER*) &group, 0, 0,
                                     TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR, "");
    thd->variables.big_tables= save_big_tables;
    if (!tmp_tables[cnt])
      DBUG_RETURN(1);
    tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  DBUG_RETURN(0);
}


multi_update::~multi_update()
{
  TABLE_LIST *table;
  for (table= update_tables ; table; table= table->next_local)
  {
    table->table->no_keyread= table->table->no_cache= 0;
    if (ignore)
      table->table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  }

  if (tmp_tables)
  {
    for (uint cnt = 0; cnt < table_count; cnt++)
    {
      if (tmp_tables[cnt])
      {
	free_tmp_table(thd, tmp_tables[cnt]);
	tmp_table_param[cnt].cleanup();
      }
    }
  }
  if (copy_field)
    delete [] copy_field;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		// Restore this setting
  DBUG_ASSERT(trans_safe || !updated ||
              thd->transaction.stmt.cannot_safely_rollback());

  if (update_operations != NULL)
    for (uint i= 0; i < table_count; i++)
      delete update_operations[i];
}


bool multi_update::send_data(List<Item> &not_used_values)
{
  TABLE_LIST *cur_table;
  DBUG_ENTER("multi_update::send_data");

  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    TABLE *table= cur_table->table;
    uint offset= cur_table->shared;
    /*
      Check if we are using outer join and we didn't find the row
      or if we have already updated this row in the previous call to this
      function.

      The same row may be presented here several times in a join of type
      UPDATE t1 FROM t1,t2 SET t1.a=t2.a

      In this case we will do the update for the first found row combination.
      The join algorithm guarantees that we will not find the a row in
      t1 several times.
    */
    if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
      continue;

    if (table == table_to_update)
    {
      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);
      if (fill_record_n_invoke_before_triggers(thd,
                                               *fields_for_table[offset],
                                               *values_for_table[offset],
                                               false, // ignore_errors
                                               table->triggers,
                                               TRG_EVENT_UPDATE))
	DBUG_RETURN(1);

      /*
        Reset the table->auto_increment_field_not_null as it is valid for
        only one row.
      */
      table->auto_increment_field_not_null= FALSE;
      found++;
      if (!records_are_comparable(table) || compare_records(table))
      {
        update_operations[offset]->set_function_defaults(table);

	int error;
        if ((error= cur_table->view_check_option(thd, ignore)) !=
            VIEW_CHECK_OK)
        {
          found--;
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            DBUG_RETURN(1);
        }
        if (!updated++)
        {
          /*
            Inform the main table that we are going to update the table even
            while we may be scanning it.  This will flush the read cache
            if it's used.
          */
          main_table->file->extra(HA_EXTRA_PREPARE_FOR_UPDATE);
        }
        if ((error=table->file->ha_update_row(table->record[1],
                                              table->record[0])) &&
            error != HA_ERR_RECORD_IS_THE_SAME)
        {
          updated--;
          if (!ignore ||
              table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
          {
            /*
              If (ignore && error == is ignorable) we don't have to
              do anything; otherwise...
            */
            myf flags= 0;

            if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
              flags|= ME_FATALERROR; /* Other handler errors are fatal */

            table->file->print_error(error,MYF(flags));
            DBUG_RETURN(1);
          }
        }
        else
        {
          if (error == HA_ERR_RECORD_IS_THE_SAME)
          {
            error= 0;
            updated--;
          }
          /* non-transactional or transactional table got modified   */
          /* either multi_update class' flag is raised in its branch */
          if (table->file->has_transactions())
            transactional_tables= TRUE;
          else
          {
            trans_safe= FALSE;
            thd->transaction.stmt.mark_modified_non_trans_table();
          }
        }
      }
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, TRUE))
        DBUG_RETURN(1);
    }
    else
    {
      int error;
      TABLE *tmp_table= tmp_tables[offset];
      /*
       For updatable VIEW store rowid of the updated table and
       rowids of tables used in the CHECK OPTION condition.
      */
      uint field_num= 0;
      List_iterator_fast<TABLE> tbl_it(unupdated_check_opt_tables);
      TABLE *tbl= table;
      do
      {
        tbl->file->position(tbl->record[0]);
        memcpy((char*) tmp_table->field[field_num]->ptr,
               (char*) tbl->file->ref, tbl->file->ref_length);
        /*
         For outer joins a rowid field may have no NOT_NULL_FLAG,
         so we have to reset NULL bit for this field.
         (set_notnull() resets NULL bit only if available).
        */
        tmp_table->field[field_num]->set_notnull();
        field_num++;
      } while ((tbl= tbl_it++));

      /* Store regular updated fields in the row. */
      fill_record(thd,
                  tmp_table->field + 1 + unupdated_check_opt_tables.elements,
                  *values_for_table[offset], 1, NULL);

      /* Write row, ignoring duplicated updates to a row */
      error= tmp_table->file->ha_write_row(tmp_table->record[0]);
      if (error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE)
      {
        if (error &&
            create_myisam_from_heap(thd, tmp_table,
                                         tmp_table_param[offset].start_recinfo,
                                         &tmp_table_param[offset].recinfo,
                                         error, TRUE, NULL))
        {
          do_update= 0;
	  DBUG_RETURN(1);			// Not a table_is_full error
	}
        found++;
      }
    }
  }
  DBUG_RETURN(0);
}


void multi_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  my_error(errcode, MYF(0), err);
}


void multi_update::abort_result_set()
{
  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->transaction.stmt.cannot_safely_rollback() && !updated))
    return;

  /* Something already updated so we have to invalidate cache */
  if (updated)
    query_cache_invalidate3(thd, update_tables, 1);
  /*
    If all tables that has been updated are trans safe then just do rollback.
    If not attempt to do remaining updates.
  */

  if (! trans_safe)
  {
    DBUG_ASSERT(thd->transaction.stmt.cannot_safely_rollback());
    if (do_update && table_count > 1)
    {
      /* Add warning here */
      /* 
         todo/fixme: do_update() is never called with the arg 1.
         should it change the signature to become argless?
      */
      (void) do_updates();
    }
  }
  if (thd->transaction.stmt.cannot_safely_rollback())
  {
    /*
      The query has to binlog because there's a modified non-transactional table
      either from the query's list or via a stored routine: bug#13270,23333
    */
    if (mysql_bin_log.is_open())
    {
      /*
        THD::killed status might not have been set ON at time of an error
        got caught and if happens later the killed error is written
        into repl event.
      */
      int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
      /* the error of binary logging is ignored */
      (void)thd->binlog_query(THD::ROW_QUERY_TYPE,
                        thd->query(), thd->query_length(),
                        transactional_tables, FALSE, FALSE, errcode);
    }
  }
  DBUG_ASSERT(trans_safe || !updated || thd->transaction.stmt.cannot_safely_rollback());
}


int multi_update::do_updates()
{
  TABLE_LIST *cur_table;
  int local_error= 0;
  ha_rows org_updated;
  TABLE *table, *tmp_table;
  List_iterator_fast<TABLE> check_opt_it(unupdated_check_opt_tables);
  DBUG_ENTER("multi_update::do_updates");

  do_update= 0;					// Don't retry this function
  if (!found)
    DBUG_RETURN(0);
  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    uint offset= cur_table->shared;

    table = cur_table->table;
    if (table == table_to_update)
      continue;					// Already updated
    org_updated= updated;
    tmp_table= tmp_tables[cur_table->shared];
    tmp_table->file->extra(HA_EXTRA_CACHE);	// Change to read cache
    if ((local_error= table->file->ha_rnd_init(0)))
      goto err;
    table->file->extra(HA_EXTRA_NO_CACHE);

    check_opt_it.rewind();
    while(TABLE *tbl= check_opt_it++)
    {
      if (tbl->file->ha_rnd_init(1))
        goto err;
      tbl->file->extra(HA_EXTRA_CACHE);
    }

    /*
      Setup copy functions to copy fields from temporary table
    */
    List_iterator_fast<Item> field_it(*fields_for_table[offset]);
    Field **field= tmp_table->field + 
                   1 + unupdated_check_opt_tables.elements; // Skip row pointers
    Copy_field *copy_field_ptr= copy_field, *copy_field_end;
    for ( ; *field ; field++)
    {
      Item_field *item= (Item_field* ) field_it++;
      (copy_field_ptr++)->set(item->field, *field, 0);
    }
    copy_field_end=copy_field_ptr;

    if ((local_error = tmp_table->file->ha_rnd_init(1)))
      goto err;

    for (;;)
    {
      if (thd->killed && trans_safe)
	goto err;
      if ((local_error=tmp_table->file->ha_rnd_next(tmp_table->record[0])))
      {
	if (local_error == HA_ERR_END_OF_FILE)
	  break;
	if (local_error == HA_ERR_RECORD_DELETED)
	  continue;				// May happen on dup key
	goto err;
      }

      /* call ha_rnd_pos() using rowids from temporary table */
      check_opt_it.rewind();
      TABLE *tbl= table;
      uint field_num= 0;
      do
      {
        if((local_error=
              tbl->file->ha_rnd_pos(tbl->record[0],
                                    (uchar *) tmp_table->field[field_num]->ptr)))
          goto err;
        field_num++;
      } while((tbl= check_opt_it++));

      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr=copy_field;
	   copy_field_ptr != copy_field_end;
	   copy_field_ptr++)
	(*copy_field_ptr->do_copy)(copy_field_ptr);

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_BEFORE, TRUE))
        goto err2;

      if (!records_are_comparable(table) || compare_records(table))
      {
        update_operations[offset]->set_function_defaults(table);
        int error;
        if ((error= cur_table->view_check_option(thd, ignore)) !=
            VIEW_CHECK_OK)
        {
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            goto err;
        }
        local_error= table->file->ha_update_row(table->record[1],
                                                table->record[0]);
        if (!local_error)
          updated++;
        else if (local_error == HA_ERR_RECORD_IS_THE_SAME)
          local_error= 0;
        else if (!ignore ||
                 table->file->is_fatal_error(local_error, HA_CHECK_DUP_KEY))
          goto err;
        else
          local_error= 0;
      }

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, TRUE))
        goto err2;
    }

    if (updated != org_updated)
    {
      if (table->file->has_transactions())
        transactional_tables= TRUE;
      else
      {
        trans_safe= FALSE;				// Can't do safe rollback
        thd->transaction.stmt.mark_modified_non_trans_table();
      }
    }
    (void) table->file->ha_rnd_end();
    (void) tmp_table->file->ha_rnd_end();
    check_opt_it.rewind();
    while (TABLE *tbl= check_opt_it++)
        tbl->file->ha_rnd_end();

  }
  DBUG_RETURN(0);

err:
  {
    table->file->print_error(local_error,MYF(ME_FATALERROR));
  }

err2:
  if (table->file->inited)
    (void) table->file->ha_rnd_end();
  if (tmp_table->file->inited)
    (void) tmp_table->file->ha_rnd_end();
  check_opt_it.rewind();
  while (TABLE *tbl= check_opt_it++)
  {
    if (tbl->file->inited)
      (void) tbl->file->ha_rnd_end();
  }

  if (updated != org_updated)
  {
    if (table->file->has_transactions())
      transactional_tables= TRUE;
    else
    {
      trans_safe= FALSE;
      thd->transaction.stmt.mark_modified_non_trans_table();
    }
  }
  DBUG_RETURN(1);
}


/* out: 1 if error, 0 if success */

bool multi_update::send_eof()
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  ulonglong id;
  THD::killed_state killed_status= THD::NOT_KILLED;
  DBUG_ENTER("multi_update::send_eof");
  THD_STAGE_INFO(thd, stage_updating_reference_tables);

  /* 
     Does updates for the last n - 1 tables, returns 0 if ok;
     error takes into account killed status gained in do_updates()
  */
  int local_error= thd->is_error();
  if (!local_error)
    local_error = (table_count) ? do_updates() : 0;
  /*
    if local_error is not set ON until after do_updates() then
    later carried out killing should not affect binlogging.
  */
  killed_status= (local_error == 0)? THD::NOT_KILLED : thd->killed;
  THD_STAGE_INFO(thd, stage_end);

  /* We must invalidate the query cache before binlog writing and
  ha_autocommit_... */

  if (updated)
  {
    query_cache_invalidate3(thd, update_tables, 1);
  }
  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transactional tables.
    
    The query has to binlog because there's a modified non-transactional table
    either from the query's list or via a stored routine: bug#13270,23333
  */

  if (local_error == 0 || thd->transaction.stmt.cannot_safely_rollback())
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (local_error == 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query(), thd->query_length(),
                            transactional_tables, FALSE, FALSE, errcode))
      {
	local_error= 1;				// Rollback update
      }
    }
  }
  DBUG_ASSERT(trans_safe || !updated || 
              thd->transaction.stmt.cannot_safely_rollback());

  if (local_error != 0)
    error_handled= TRUE; // to force early leave from ::send_error()

  if (local_error > 0) // if the above log write did not fail ...
  {
    /* Safety: If we haven't got an error before (can happen in do_updates) */
    my_message(ER_UNKNOWN_ERROR, "An error occured in multi-table update",
	       MYF(0));
    DBUG_RETURN(TRUE);
  }

  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;
  my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO),
              (ulong) found, (ulong) updated, (ulong) thd->cuted_fields);
  ::my_ok(thd, (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
          id, buff);
  DBUG_RETURN(FALSE);
}
