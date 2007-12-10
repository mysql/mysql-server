
#include "mysql_priv.h"
#ifndef MYSQL_CLIENT
#include "rpl_rli.h"
#include "rpl_utility.h"
#endif
#include "log_event_old.h"
#include "rpl_record_old.h"

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

// Old implementation of do_apply_event()
int 
Old_rows_log_event::do_apply_event(Rows_log_event *ev, const Relay_log_info *rli)
{
  DBUG_ENTER("Rows_log_event::do_apply_event(st_relay_log_info*)");
  int error= 0;
  THD *thd= ev->thd;
  uchar const *row_start= ev->m_rows_buf;

  /*
    If m_table_id == ~0UL, then we have a dummy event that does not
    contain any data.  In that case, we just remove all tables in the
    tables_to_lock list, close the thread tables, and return with
    success.
   */
  if (ev->m_table_id == ~0UL)
  {
    /*
       This one is supposed to be set: just an extra check so that
       nothing strange has happened.
     */
    DBUG_ASSERT(ev->get_flags(Rows_log_event::STMT_END_F));

    const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
    close_thread_tables(thd);
    thd->clear_error();
    DBUG_RETURN(0);
  }

  /*
    'thd' has been set by exec_relay_log_event(), just before calling
    do_apply_event(). We still check here to prevent future coding
    errors.
  */
  DBUG_ASSERT(rli->sql_thd == thd);

  /*
    If there is no locks taken, this is the first binrow event seen
    after the table map events.  We should then lock all the tables
    used in the transaction and proceed with execution of the actual
    event.
  */
  if (!thd->lock)
  {
    bool need_reopen= 1; /* To execute the first lap of the loop below */

    /*
      lock_tables() reads the contents of thd->lex, so they must be
      initialized. Contrary to in
      Table_map_log_event::do_apply_event() we don't call
      mysql_init_query() as that may reset the binlog format.
    */
    lex_start(thd);

    while ((error= lock_tables(thd, rli->tables_to_lock,
                               rli->tables_to_lock_count, &need_reopen)))
    {
      if (!need_reopen)
      {
        if (thd->is_slave_error || thd->is_fatal_error)
        {
          /*
            Error reporting borrowed from Query_log_event with many excessive
            simplifications (we don't honour --slave-skip-errors)
          */
          uint actual_error= thd->net.last_errno;
          rli->report(ERROR_LEVEL, actual_error,
                      "Error '%s' in %s event: when locking tables",
                      (actual_error ? thd->net.last_error :
                       "unexpected success or fatal error"),
                      ev->get_type_str());
          thd->is_fatal_error= 1;
        }
        else
        {
          rli->report(ERROR_LEVEL, error,
                      "Error in %s event: when locking tables",
                      ev->get_type_str());
        }
        const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
        DBUG_RETURN(error);
      }

      /*
        So we need to reopen the tables.

        We need to flush the pending RBR event, since it keeps a
        pointer to an open table.

        ALTERNATIVE SOLUTION (not implemented): Extract a pointer to
        the pending RBR event and reset the table pointer after the
        tables has been reopened.

        NOTE: For this new scheme there should be no pending event:
        need to add code to assert that is the case.
       */
      thd->binlog_flush_pending_rows_event(false);
      TABLE_LIST *tables= rli->tables_to_lock;
      close_tables_for_reopen(thd, &tables);

      uint tables_count= rli->tables_to_lock_count;
      if ((error= open_tables(thd, &tables, &tables_count, 0)))
      {
        if (thd->is_slave_error || thd->is_fatal_error)
        {
          /*
            Error reporting borrowed from Query_log_event with many excessive
            simplifications (we don't honour --slave-skip-errors)
          */
          uint actual_error= thd->net.last_errno;
          rli->report(ERROR_LEVEL, actual_error,
                      "Error '%s' on reopening tables",
                      (actual_error ? thd->net.last_error :
                       "unexpected success or fatal error"));
          thd->is_slave_error= 1;
        }
        const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
        DBUG_RETURN(error);
      }
    }

    /*
      When the open and locking succeeded, we check all tables to
      ensure that they still have the correct type.

      We can use a down cast here since we know that every table added
      to the tables_to_lock is a RPL_TABLE_LIST.
    */

    {
      RPL_TABLE_LIST *ptr= rli->tables_to_lock;
      for ( ; ptr ; ptr= static_cast<RPL_TABLE_LIST*>(ptr->next_global))
      {
        if (ptr->m_tabledef.compatible_with(rli, ptr->table))
        {
          mysql_unlock_tables(thd, thd->lock);
          thd->lock= 0;
          thd->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
          DBUG_RETURN(Rows_log_event::ERR_BAD_TABLE_DEF);
        }
      }
    }

    /*
      ... and then we add all the tables to the table map and remove
      them from tables to lock.

      We also invalidate the query cache for all the tables, since
      they will now be changed.

      TODO [/Matz]: Maybe the query cache should not be invalidated
      here? It might be that a table is not changed, even though it
      was locked for the statement.  We do know that each
      Rows_log_event contain at least one row, so after processing one
      Rows_log_event, we can invalidate the query cache for the
      associated table.
     */
    for (TABLE_LIST *ptr= rli->tables_to_lock ; ptr ; ptr= ptr->next_global)
    {
      const_cast<Relay_log_info*>(rli)->m_table_map.set_table(ptr->table_id, ptr->table);
    }
#ifdef HAVE_QUERY_CACHE
    query_cache.invalidate_locked_for_write(rli->tables_to_lock);
#endif
  }

  TABLE* table= const_cast<Relay_log_info*>(rli)->m_table_map.get_table(ev->m_table_id);

  if (table)
  {
    /*
      table == NULL means that this table should not be replicated
      (this was set up by Table_map_log_event::do_apply_event()
      which tested replicate-* rules).
    */

    /*
      It's not needed to set_time() but
      1) it continues the property that "Time" in SHOW PROCESSLIST shows how
      much slave is behind
      2) it will be needed when we allow replication from a table with no
      TIMESTAMP column to a table with one.
      So we call set_time(), like in SBR. Presently it changes nothing.
    */
    thd->set_time((time_t)ev->when);
    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (ev->get_flags(Rows_log_event::NO_FOREIGN_KEY_CHECKS_F))
        thd->options|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        thd->options&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (ev->get_flags(Rows_log_event::RELAXED_UNIQUE_CHECKS_F))
        thd->options|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        thd->options&= ~OPTION_RELAXED_UNIQUE_CHECKS;
    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(thd->options) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    /*
      Now we are in a statement and will stay in a statement until we
      see a STMT_END_F.

      We set this flag here, before actually applying any rows, in
      case the SQL thread is stopped and we need to detect that we're
      inside a statement and halting abruptly might cause problems
      when restarting.
     */
    const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

    error= do_before_row_operations(table);
    while (error == 0 && row_start < ev->m_rows_end)
    {
      uchar const *row_end= NULL;
      if ((error= do_prepare_row(thd, rli, table, row_start, &row_end)))
        break; // We should perform the after-row operation even in
               // the case of error

      DBUG_ASSERT(row_end != NULL); // cannot happen
      DBUG_ASSERT(row_end <= ev->m_rows_end);

      /* in_use can have been set to NULL in close_tables_for_reopen */
      THD* old_thd= table->in_use;
      if (!table->in_use)
        table->in_use= thd;
      error= do_exec_row(table);
      table->in_use = old_thd;
      switch (error)
      {
        /* Some recoverable errors */
      case HA_ERR_RECORD_CHANGED:
      case HA_ERR_KEY_NOT_FOUND:  /* Idempotency support: OK if
                                           tuple does not exist */
  error= 0;
      case 0:
  break;

      default:
  rli->report(ERROR_LEVEL, thd->net.last_errno,
                    "Error in %s event: row application failed. %s",
                    ev->get_type_str(),
                    thd->net.last_error ? thd->net.last_error : "");
  thd->is_slave_error= 1;
  break;
      }

      row_start= row_end;
    }
    DBUG_EXECUTE_IF("STOP_SLAVE_after_first_Rows_event",
                    const_cast<Relay_log_info*>(rli)->abort_slave= 1;);
    error= do_after_row_operations(table, error);
    if (!ev->cache_stmt)
    {
      DBUG_PRINT("info", ("Marked that we need to keep log"));
      thd->options|= OPTION_KEEP_LOG;
    }
  }

  /*
    We need to delay this clear until the table def is no longer needed.
    The table def is needed in unpack_row().
  */
  if (rli->tables_to_lock && ev->get_flags(Rows_log_event::STMT_END_F))
    const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();

  if (error)
  {                     /* error has occured during the transaction */
    rli->report(ERROR_LEVEL, thd->net.last_errno,
                "Error in %s event: error during transaction execution "
                "on table %s.%s. %s",
                ev->get_type_str(), table->s->db.str,
                table->s->table_name.str,
                thd->net.last_error ? thd->net.last_error : "");

    /*
      If one day we honour --skip-slave-errors in row-based replication, and
      the error should be skipped, then we would clear mappings, rollback,
      close tables, but the slave SQL thread would not stop and then may
      assume the mapping is still available, the tables are still open...
      So then we should clear mappings/rollback/close here only if this is a
      STMT_END_F.
      For now we code, knowing that error is not skippable and so slave SQL
      thread is certainly going to stop.
      rollback at the caller along with sbr.
    */
    thd->reset_current_stmt_binlog_row_based();
    const_cast<Relay_log_info*>(rli)->cleanup_context(thd, error);
    thd->is_slave_error= 1;
    DBUG_RETURN(error);
  }

  /*
    This code would ideally be placed in do_update_pos() instead, but
    since we have no access to table there, we do the setting of
    last_event_start_time here instead.
  */
  if (table && (table->s->primary_key == MAX_KEY) &&
      !ev->cache_stmt && 
      ev->get_flags(Rows_log_event::STMT_END_F) == Rows_log_event::RLE_NO_FLAGS)
  {
    /*
      ------------ Temporary fix until WL#2975 is implemented ---------

      This event is not the last one (no STMT_END_F). If we stop now
      (in case of terminate_slave_thread()), how will we restart? We
      have to restart from Table_map_log_event, but as this table is
      not transactional, the rows already inserted will still be
      present, and idempotency is not guaranteed (no PK) so we risk
      that repeating leads to double insert. So we desperately try to
      continue, hope we'll eventually leave this buggy situation (by
      executing the final Rows_log_event). If we are in a hopeless
      wait (reached end of last relay log and nothing gets appended
      there), we timeout after one minute, and notify DBA about the
      problem.  When WL#2975 is implemented, just remove the member
      st_relay_log_info::last_event_start_time and all its occurences.
    */
    const_cast<Relay_log_info*>(rli)->last_event_start_time= my_time(0);
  }

  DBUG_RETURN(0);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

/*
  Check if there are more UNIQUE keys after the given key.
*/
static int
last_uniq_key(TABLE *table, uint keyno)
{
  while (++keyno < table->s->keys)
    if (table->key_info[keyno].flags & HA_NOSAME)
      return 0;
  return 1;
}

/*
  Compares table->record[0] and table->record[1]

  Returns TRUE if different.
*/
static bool record_compare(TABLE *table)
{
  /*
    Need to set the X bit and the filler bits in both records since
    there are engines that do not set it correctly.

    In addition, since MyISAM checks that one hasn't tampered with the
    record, it is necessary to restore the old bytes into the record
    after doing the comparison.

    TODO[record format ndb]: Remove it once NDB returns correct
    records. Check that the other engines also return correct records.
   */

  bool result= FALSE;
  uchar saved_x[2], saved_filler[2];

  if (table->s->null_bytes > 0)
  {
    for (int i = 0 ; i < 2 ; ++i)
    {
      saved_x[i]= table->record[i][0];
      saved_filler[i]= table->record[i][table->s->null_bytes - 1];
      table->record[i][0]|= 1U;
      table->record[i][table->s->null_bytes - 1]|=
        256U - (1U << table->s->last_null_bit_pos);
    }
  }

  if (table->s->blob_fields + table->s->varchar_fields == 0)
  {
    result= cmp_record(table,record[1]);
    goto record_compare_exit;
  }

  /* Compare null bits */
  if (memcmp(table->null_flags,
       table->null_flags+table->s->rec_buff_length,
       table->s->null_bytes))
  {
    result= TRUE;       // Diff in NULL value
    goto record_compare_exit;
  }

  /* Compare updated fields */
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_binary_offset(table->s->rec_buff_length))
    {
      result= TRUE;
      goto record_compare_exit;
    }
  }

record_compare_exit:
  /*
    Restore the saved bytes.

    TODO[record format ndb]: Remove this code once NDB returns the
    correct record format.
  */
  if (table->s->null_bytes > 0)
  {
    for (int i = 0 ; i < 2 ; ++i)
    {
      table->record[i][0]= saved_x[i];
      table->record[i][table->s->null_bytes - 1]= saved_filler[i];
    }
  }

  return result;
}

/*
  Copy "extra" columns from record[1] to record[0].

  Copy the extra fields that are not present on the master but are
  present on the slave from record[1] to record[0].  This is used
  after fetching a record that are to be updated, either inside
  replace_record() or as part of executing an update_row().
 */
static int
copy_extra_record_fields(TABLE *table,
                         size_t master_reclength,
                         my_ptrdiff_t master_fields)
{
  DBUG_ENTER("copy_extra_record_fields(table, master_reclen, master_fields)");
  DBUG_PRINT("info", ("Copying to 0x%lx "
                      "from field %lu at offset %lu "
                      "to field %d at offset %lu",
                      (long) table->record[0],
                      (ulong) master_fields, (ulong) master_reclength,
                      table->s->fields, table->s->reclength));
  /*
    Copying the extra fields of the slave that does not exist on
    master into record[0] (which are basically the default values).
  */

  if (table->s->fields < (uint) master_fields)
    DBUG_RETURN(0);

 DBUG_ASSERT(master_reclength <= table->s->reclength);
  if (master_reclength < table->s->reclength)
    bmove_align(table->record[0] + master_reclength,
                table->record[1] + master_reclength,
                table->s->reclength - master_reclength);
    
  /*
    Bit columns are special.  We iterate over all the remaining
    columns and copy the "extra" bits to the new record.  This is
    not a very good solution: it should be refactored on
    opportunity.

    REFACTORING SUGGESTION (Matz).  Introduce a member function
    similar to move_field_offset() called copy_field_offset() to
    copy field values and implement it for all Field subclasses. Use
    this function to copy data from the found record to the record
    that are going to be inserted.

    The copy_field_offset() function need to be a virtual function,
    which in this case will prevent copying an entire range of
    fields efficiently.
  */
  {
    Field **field_ptr= table->field + master_fields;
    for ( ; *field_ptr ; ++field_ptr)
    {
      /*
        Set the null bit according to the values in record[1]
       */
      if ((*field_ptr)->maybe_null() &&
          (*field_ptr)->is_null_in_record(reinterpret_cast<uchar*>(table->record[1])))
        (*field_ptr)->set_null();
      else
        (*field_ptr)->set_notnull();

      /*
        Do the extra work for special columns.
       */
      switch ((*field_ptr)->real_type())
      {
      default:
        /* Nothing to do */
        break;

      case MYSQL_TYPE_BIT:
        Field_bit *f= static_cast<Field_bit*>(*field_ptr);
        if (f->bit_len > 0)
        {
          my_ptrdiff_t const offset= table->record[1] - table->record[0];
          uchar const bits=
            get_rec_bits(f->bit_ptr + offset, f->bit_ofs, f->bit_len);
          set_rec_bits(bits, f->bit_ptr, f->bit_ofs, f->bit_len);
        }
        break;
      }
    }
  }
  DBUG_RETURN(0);                                     // All OK
}

/*
  Replace the provided record in the database.

  SYNOPSIS
      replace_record()
      thd    Thread context for writing the record.
      table  Table to which record should be written.
      master_reclength
             Offset to first column that is not present on the master,
             alternatively the length of the record on the master
             side.

  RETURN VALUE
      Error code on failure, 0 on success.

  DESCRIPTION
      Similar to how it is done in mysql_insert(), we first try to do
      a ha_write_row() and of that fails due to duplicated keys (or
      indices), we do an ha_update_row() or a ha_delete_row() instead.
 */
static int
replace_record(THD *thd, TABLE *table,
               ulong const master_reclength,
               uint const master_fields)
{
  DBUG_ENTER("replace_record");
  DBUG_ASSERT(table != NULL && thd != NULL);

  int error;
  int keynum;
  auto_afree_ptr<char> key(NULL);

#ifndef DBUG_OFF
  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_PRINT_BITSET("debug", "write_set = %s", table->write_set);
  DBUG_PRINT_BITSET("debug", "read_set = %s", table->read_set);
#endif

  while ((error= table->file->ha_write_row(table->record[0])))
  {
    if (error == HA_ERR_LOCK_DEADLOCK || error == HA_ERR_LOCK_WAIT_TIMEOUT)
    {
      table->file->print_error(error, MYF(0)); /* to check at exec_relay_log_event */
      DBUG_RETURN(error);
    }
    if ((keynum= table->file->get_dup_key(error)) < 0)
    {
      table->file->print_error(error, MYF(0));
      /*
        We failed to retrieve the duplicate key
        - either because the error was not "duplicate key" error
        - or because the information which key is not available
      */
      DBUG_RETURN(error);
    }

    /*
       We need to retrieve the old row into record[1] to be able to
       either update or delete the offending record.  We either:

       - use rnd_pos() with a row-id (available as dupp_row) to the
         offending row, if that is possible (MyISAM and Blackhole), or else

       - use index_read_idx() with the key that is duplicated, to
         retrieve the offending row.
     */
    if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
    {
      error= table->file->rnd_pos(table->record[1], table->file->dup_ref);
      if (error)
      {
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
    }
    else
    {
      if (table->file->extra(HA_EXTRA_FLUSH_CACHE))
      {
        DBUG_RETURN(my_errno);
      }

      if (key.get() == NULL)
      {
        key.assign(static_cast<char*>(my_alloca(table->s->max_unique_length)));
        if (key.get() == NULL)
          DBUG_RETURN(ENOMEM);
      }

      key_copy((uchar*)key.get(), table->record[0], table->key_info + keynum,
               0);
      error= table->file->index_read_idx_map(table->record[1], keynum,
                                             (const uchar*)key.get(),
                                             HA_WHOLE_KEY,
                                             HA_READ_KEY_EXACT);
      if (error)
      {
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
    }

    /*
       Now, table->record[1] should contain the offending row.  That
       will enable us to update it or, alternatively, delete it (so
       that we can insert the new row afterwards).

       First we copy the columns into table->record[0] that are not
       present on the master from table->record[1], if there are any.
    */
    copy_extra_record_fields(table, master_reclength, master_fields);

    /*
       REPLACE is defined as either INSERT or DELETE + INSERT.  If
       possible, we can replace it with an UPDATE, but that will not
       work on InnoDB if FOREIGN KEY checks are necessary.

       I (Matz) am not sure of the reason for the last_uniq_key()
       check as, but I'm guessing that it's something along the
       following lines.

       Suppose that we got the duplicate key to be a key that is not
       the last unique key for the table and we perform an update:
       then there might be another key for which the unique check will
       fail, so we're better off just deleting the row and inserting
       the correct row.
     */
    if (last_uniq_key(table, keynum) &&
        !table->file->referenced_by_foreign_key())
    {
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
        table->file->print_error(error, MYF(0));
      else
        error= 0;
      DBUG_RETURN(error);
    }
    else
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
      {
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
      /* Will retry ha_write_row() with the offending row removed. */
    }
  }

  DBUG_RETURN(error);
}

/**
  Find the row given by 'key', if the table has keys, or else use a table scan
  to find (and fetch) the row.

  If the engine allows random access of the records, a combination of
  position() and rnd_pos() will be used.

  @param table Pointer to table to search
  @param key   Pointer to key to use for search, if table has key

  @pre <code>table->record[0]</code> shall contain the row to locate
  and <code>key</code> shall contain a key to use for searching, if
  the engine has a key.

  @post If the return value is zero, <code>table->record[1]</code>
  will contain the fetched row and the internal "cursor" will refer to
  the row. If the return value is non-zero,
  <code>table->record[1]</code> is undefined.  In either case,
  <code>table->record[0]</code> is undefined.

  @return Zero if the row was successfully fetched into
  <code>table->record[1]</code>, error code otherwise.
 */

static int find_and_fetch_row(TABLE *table, uchar *key)
{
  DBUG_ENTER("find_and_fetch_row(TABLE *table, uchar *key, uchar *record)");
  DBUG_PRINT("enter", ("table: 0x%lx, key: 0x%lx  record: 0x%lx",
           (long) table, (long) key, (long) table->record[1]));

  DBUG_ASSERT(table->in_use != NULL);

  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);

  if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      table->s->primary_key < MAX_KEY)
  {
    /*
      Use a more efficient method to fetch the record given by
      table->record[0] if the engine allows it.  We first compute a
      row reference using the position() member function (it will be
      stored in table->file->ref) and the use rnd_pos() to position
      the "cursor" (i.e., record[0] in this case) at the correct row.

      TODO: Add a check that the correct record has been fetched by
      comparing with the original record. Take into account that the
      record on the master and slave can be of different
      length. Something along these lines should work:

      ADD>>>  store_record(table,record[1]);
              int error= table->file->rnd_pos(table->record[0], table->file->ref);
      ADD>>>  DBUG_ASSERT(memcmp(table->record[1], table->record[0],
                                 table->s->reclength) == 0);

    */
    table->file->position(table->record[0]);
    int error= table->file->rnd_pos(table->record[0], table->file->ref);
    /*
      rnd_pos() returns the record in table->record[0], so we have to
      move it to table->record[1].
     */
    bmove_align(table->record[1], table->record[0], table->s->reclength);
    DBUG_RETURN(error);
  }

  /* We need to retrieve all fields */
  /* TODO: Move this out from this function to main loop */
  table->use_all_columns();

  if (table->s->keys > 0)
  {
    int error;
    /* We have a key: search the table using the index */
    if (!table->file->inited && (error= table->file->ha_index_init(0, FALSE)))
      DBUG_RETURN(error);

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
    DBUG_DUMP("table->record[0]", table->record[0], table->s->reclength);
    DBUG_DUMP("table->record[1]", table->record[1], table->s->reclength);
#endif

    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    my_ptrdiff_t const pos=
      table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
    table->record[1][pos]= 0xFF;
    if ((error= table->file->index_read_map(table->record[1], key, HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT)))
    {
      table->file->print_error(error, MYF(0));
      table->file->ha_index_end();
      DBUG_RETURN(error);
    }

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
    DBUG_DUMP("table->record[0]", table->record[0], table->s->reclength);
    DBUG_DUMP("table->record[1]", table->record[1], table->s->reclength);
#endif
    /*
      Below is a minor "optimization".  If the key (i.e., key number
      0) has the HA_NOSAME flag set, we know that we have found the
      correct record (since there can be no duplicates); otherwise, we
      have to compare the record with the one found to see if it is
      the correct one.

      CAVEAT! This behaviour is essential for the replication of,
      e.g., the mysql.proc table since the correct record *shall* be
      found using the primary key *only*.  There shall be no
      comparison of non-PK columns to decide if the correct record is
      found.  I can see no scenario where it would be incorrect to
      chose the row to change only using a PK or an UNNI.
    */
    if (table->key_info->flags & HA_NOSAME)
    {
      table->file->ha_index_end();
      DBUG_RETURN(0);
    }

    while (record_compare(table))
    {
      int error;

      /*
        We need to set the null bytes to ensure that the filler bit
        are all set when returning.  There are storage engines that
        just set the necessary bits on the bytes and don't set the
        filler bits correctly.

        TODO[record format ndb]: Remove this code once NDB returns the
        correct record format.
      */
      if (table->s->null_bytes > 0)
      {
        table->record[1][table->s->null_bytes - 1]|=
          256U - (1U << table->s->last_null_bit_pos);
      }

      if ((error= table->file->index_next(table->record[1])))
      {
  table->file->print_error(error, MYF(0));
        table->file->ha_index_end();
  DBUG_RETURN(error);
      }
    }

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    table->file->ha_index_end();
  }
  else
  {
    int restart_count= 0; // Number of times scanning has restarted from top
    int error;

    /* We don't have a key: search the table using rnd_next() */
    if ((error= table->file->ha_rnd_init(1)))
      return error;

    /* Continue until we find the right record or have made a full loop */
    do
    {
      error= table->file->rnd_next(table->record[1]);

      DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
      DBUG_DUMP("record[1]", table->record[1], table->s->reclength);

      switch (error) {
      case 0:
      case HA_ERR_RECORD_DELETED:
  break;

      case HA_ERR_END_OF_FILE:
  if (++restart_count < 2)
    table->file->ha_rnd_init(1);
  break;

      default:
  table->file->print_error(error, MYF(0));
        DBUG_PRINT("info", ("Record not found"));
        table->file->ha_rnd_end();
  DBUG_RETURN(error);
      }
    }
    while (restart_count < 2 && record_compare(table));

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    DBUG_PRINT("info", ("Record %sfound", restart_count == 2 ? "not " : ""));
    table->file->ha_rnd_end();

    DBUG_ASSERT(error == HA_ERR_END_OF_FILE || error == 0);
    DBUG_RETURN(error);
  }

  DBUG_RETURN(0);
}

/**********************************************************
  Row handling primitives for Write_rows_log_event_old
 **********************************************************/

int Write_rows_log_event_old::do_before_row_operations(TABLE *table)
{
  int error= 0;

  /*
    We are using REPLACE semantics and not INSERT IGNORE semantics
    when writing rows, that is: new rows replace old rows.  We need to
    inform the storage engine that it should use this behaviour.
  */

  /* Tell the storage engine that we are using REPLACE semantics. */
  thd->lex->duplicates= DUP_REPLACE;

  /*
    Pretend we're executing a REPLACE command: this is needed for
    InnoDB and NDB Cluster since they are not (properly) checking the
    lex->duplicates flag.
  */
  thd->lex->sql_command= SQLCOM_REPLACE;
  /* 
     Do not raise the error flag in case of hitting to an unique attribute
  */
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  /* 
     NDB specific: update from ndb master wrapped as Write_rows
  */
  /*
    so that the event should be applied to replace slave's row
  */
  table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  /* 
     NDB specific: if update from ndb master wrapped as Write_rows
     does not find the row it's assumed idempotent binlog applying
     is taking place; don't raise the error.
  */
  table->file->extra(HA_EXTRA_IGNORE_NO_KEY);
  /*
    TODO: the cluster team (Tomas?) says that it's better if the engine knows
    how many rows are going to be inserted, then it can allocate needed memory
    from the start.
  */
  table->file->ha_start_bulk_insert(0);
  /*
    We need TIMESTAMP_NO_AUTO_SET otherwise ha_write_row() will not use fill
    any TIMESTAMP column with data from the row but instead will use
    the event's current time.
    As we replicate from TIMESTAMP to TIMESTAMP and slave has no extra
    columns, we know that all TIMESTAMP columns on slave will receive explicit
    data from the row, so TIMESTAMP_NO_AUTO_SET is ok.
    When we allow a table without TIMESTAMP to be replicated to a table having
    more columns including a TIMESTAMP column, or when we allow a TIMESTAMP
    column to be replicated into a BIGINT column and the slave's table has a
    TIMESTAMP column, then the slave's TIMESTAMP column will take its value
    from set_time() which we called earlier (consistent with SBR). And then in
    some cases we won't want TIMESTAMP_NO_AUTO_SET (will require some code to
    analyze if explicit data is provided for slave's TIMESTAMP columns).
  */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  return error;
}

int Write_rows_log_event_old::do_after_row_operations(TABLE *table, int error)
{
  int local_error= 0;
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
  /*
    reseting the extra with 
    table->file->extra(HA_EXTRA_NO_IGNORE_NO_KEY); 
    fires bug#27077
    todo: explain or fix
  */
  if ((local_error= table->file->ha_end_bulk_insert()))
  {
    table->file->print_error(local_error, MYF(0));
  }
  return error? error : local_error;
}

int
Write_rows_log_event_old::do_prepare_row(THD *thd_arg,
                                         Relay_log_info const *rli,
                                         TABLE *table,
                                         uchar const *row_start,
                                         uchar const **row_end)
{
  DBUG_ASSERT(table != NULL);
  DBUG_ASSERT(row_start && row_end);

  int error;
  error= unpack_row_old(const_cast<Relay_log_info*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->write_set, PRE_GA_WRITE_ROWS_EVENT);
  bitmap_copy(table->read_set, table->write_set);
  return error;
}

int Write_rows_log_event_old::do_exec_row(TABLE *table)
{
  DBUG_ASSERT(table != NULL);
  int error= replace_record(thd, table, m_master_reclength, m_width);
  return error;
}

/**********************************************************
  Row handling primitives for Delete_rows_log_event_old
 **********************************************************/

int Delete_rows_log_event_old::do_before_row_operations(TABLE *table)
{
  DBUG_ASSERT(m_memory == NULL);

  if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      table->s->primary_key < MAX_KEY)
  {
    /*
      We don't need to allocate any memory for m_after_image and
      m_key since they are not used.
    */
    return 0;
  }

  int error= 0;

  if (table->s->keys > 0)
  {
    m_memory= (uchar*) my_multi_malloc(MYF(MY_WME),
                                       &m_after_image,
                                       (uint) table->s->reclength,
                                       &m_key,
                                       (uint) table->key_info->key_length,
                                       NullS);
  }
  else
  {
    m_after_image= (uchar*) my_malloc(table->s->reclength, MYF(MY_WME));
    m_memory= (uchar*)m_after_image;
    m_key= NULL;
  }
  if (!m_memory)
    return HA_ERR_OUT_OF_MEM;

  return error;
}

int Delete_rows_log_event_old::do_after_row_operations(TABLE *table, int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  table->file->ha_index_or_rnd_end();
  my_free(m_memory, MYF(MY_ALLOW_ZERO_PTR)); // Free for multi_malloc
  m_memory= NULL;
  m_after_image= NULL;
  m_key= NULL;

  return error;
}

int
Delete_rows_log_event_old::do_prepare_row(THD *thd_arg,
                                          Relay_log_info const *rli,
                                          TABLE *table,
                                          uchar const *row_start,
                                          uchar const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  error= unpack_row_old(const_cast<Relay_log_info*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->read_set, PRE_GA_DELETE_ROWS_EVENT);
  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}

int Delete_rows_log_event_old::do_exec_row(TABLE *table)
{
  int error;
  DBUG_ASSERT(table != NULL);

  if (!(error= ::find_and_fetch_row(table, m_key)))
  { 
    /*
      Now we should have the right row to delete.  We are using
      record[0] since it is guaranteed to point to a record with the
      correct value.
    */
    error= table->file->ha_delete_row(table->record[0]);
  }
  return error;
}

/**********************************************************
  Row handling primitives for Update_rows_log_event_old
 **********************************************************/

int Update_rows_log_event_old::do_before_row_operations(TABLE *table)
{
  DBUG_ASSERT(m_memory == NULL);

  int error= 0;

  if (table->s->keys > 0)
  {
    m_memory= (uchar*) my_multi_malloc(MYF(MY_WME),
                                       &m_after_image,
                                       (uint) table->s->reclength,
                                       &m_key,
                                       (uint) table->key_info->key_length,
                                       NullS);
  }
  else
  {
    m_after_image= (uchar*) my_malloc(table->s->reclength, MYF(MY_WME));
    m_memory= m_after_image;
    m_key= NULL;
  }
  if (!m_memory)
    return HA_ERR_OUT_OF_MEM;

  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return error;
}

int Update_rows_log_event_old::do_after_row_operations(TABLE *table, int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  table->file->ha_index_or_rnd_end();
  my_free(m_memory, MYF(MY_ALLOW_ZERO_PTR));
  m_memory= NULL;
  m_after_image= NULL;
  m_key= NULL;

  return error;
}

int Update_rows_log_event_old::do_prepare_row(THD *thd_arg,
                                              Relay_log_info const *rli,
                                              TABLE *table,
                                              uchar const *row_start,
                                              uchar const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  /* record[0] is the before image for the update */
  error= unpack_row_old(const_cast<Relay_log_info*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->read_set, PRE_GA_UPDATE_ROWS_EVENT);
  row_start = *row_end;
  /* m_after_image is the after image for the update */
  error= unpack_row_old(const_cast<Relay_log_info*>(rli),
                        table, m_width, m_after_image,
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->write_set, PRE_GA_UPDATE_ROWS_EVENT);

  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_DUMP("m_after_image", m_after_image, table->s->reclength);

  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}

int Update_rows_log_event_old::do_exec_row(TABLE *table)
{
  DBUG_ASSERT(table != NULL);

  int error= ::find_and_fetch_row(table, m_key);
  if (error)
    return error;

  /*
    We have to ensure that the new record (i.e., the after image) is
    in record[0] and the old record (i.e., the before image) is in
    record[1].  This since some storage engines require this (for
    example, the partition engine).

    Since find_and_fetch_row() puts the fetched record (i.e., the old
    record) in record[1], we can keep it there. We put the new record
    (i.e., the after image) into record[0], and copy the fields that
    are on the slave (i.e., in record[1]) into record[0], effectively
    overwriting the default values that where put there by the
    unpack_row() function.
  */
  bmove_align(table->record[0], m_after_image, table->s->reclength);
  copy_extra_record_fields(table, m_master_reclength, m_width);

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row has is in record[0].
    We also have copied the original values already in the slave's
    database into the after image delivered from the master.
  */
  error= table->file->ha_update_row(table->record[1], table->record[0]);
  if (error == HA_ERR_RECORD_IS_THE_SAME)
    error= 0;

  return error;
}

#endif
