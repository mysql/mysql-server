/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_priv.h"
#ifndef MYSQL_CLIENT
#include "unireg.h"
#endif
#include "my_global.h" // REQUIRED by log_event.h > m_string.h > my_bitmap.h
#include "log_event.h"
#ifndef MYSQL_CLIENT
#include "sql_cache.h"                       // QUERY_CACHE_FLAGS_SIZE
#include "sql_base.h"                       // close_tables_for_reopen
#include "key.h"                            // key_copy
#include "lock.h"                           // mysql_unlock_tables
#include "sql_parse.h"             // mysql_reset_thd_for_next_command
#include "rpl_rli.h"
#include "rpl_utility.h"
#endif
#include "log_event_old.h"
#include "rpl_record_old.h"
#include "transaction.h"

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

// Old implementation of do_apply_event()
int 
Old_rows_log_event::do_apply_event(Old_rows_log_event *ev, const Relay_log_info *rli)
{
  DBUG_ENTER("Old_rows_log_event::do_apply_event(st_relay_log_info*)");
  int error= 0;
  THD *ev_thd= ev->thd;
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
    DBUG_ASSERT(ev->get_flags(Old_rows_log_event::STMT_END_F));

    const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(ev_thd);
    ev_thd->clear_error();
    DBUG_RETURN(0);
  }

  /*
    'ev_thd' has been set by exec_relay_log_event(), just before calling
    do_apply_event(). We still check here to prevent future coding
    errors.
  */
  DBUG_ASSERT(rli->sql_thd == ev_thd);

  /*
    If there is no locks taken, this is the first binrow event seen
    after the table map events.  We should then lock all the tables
    used in the transaction and proceed with execution of the actual
    event.
  */
  if (!ev_thd->lock)
  {
    /*
      Lock_tables() reads the contents of ev_thd->lex, so they must be
      initialized.

      We also call the mysql_reset_thd_for_next_command(), since this
      is the logical start of the next "statement". Note that this
      call might reset the value of current_stmt_binlog_format, so
      we need to do any changes to that value after this function.
    */
    lex_start(ev_thd);
    mysql_reset_thd_for_next_command(ev_thd);

    /*
      This is a row injection, so we flag the "statement" as
      such. Note that this code is called both when the slave does row
      injections and when the BINLOG statement is used to do row
      injections.
    */
    ev_thd->lex->set_stmt_row_injection();

    if (open_and_lock_tables(ev_thd, rli->tables_to_lock, FALSE, 0))
    {
      uint actual_error= ev_thd->stmt_da->sql_errno();
      if (ev_thd->is_slave_error || ev_thd->is_fatal_error)
      {
        /*
          Error reporting borrowed from Query_log_event with many excessive
          simplifications (we don't honour --slave-skip-errors)
        */
        rli->report(ERROR_LEVEL, actual_error,
                    "Error '%s' on opening tables",
                    (actual_error ? ev_thd->stmt_da->message() :
                     "unexpected success or fatal error"));
        ev_thd->is_slave_error= 1;
      }
      const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
      DBUG_RETURN(actual_error);
    }

    /*
      When the open and locking succeeded, we check all tables to
      ensure that they still have the correct type.

      We can use a down cast here since we know that every table added
      to the tables_to_lock is a RPL_TABLE_LIST.
    */

    {
      RPL_TABLE_LIST *ptr= rli->tables_to_lock;
      for (uint i= 0 ; ptr&& (i< rli->tables_to_lock_count); 
           ptr= static_cast<RPL_TABLE_LIST*>(ptr->next_global), i++)
      {
        DBUG_ASSERT(ptr->m_tabledef_valid);
        TABLE *conv_table;
        if (!ptr->m_tabledef.compatible_with(thd, const_cast<Relay_log_info*>(rli),
                                             ptr->table, &conv_table))
        {
          ev_thd->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(ev_thd);
          DBUG_RETURN(Old_rows_log_event::ERR_BAD_TABLE_DEF);
        }
        DBUG_PRINT("debug", ("Table: %s.%s is compatible with master"
                             " - conv_table: %p",
                             ptr->table->s->db.str,
                             ptr->table->s->table_name.str, conv_table));
        ptr->m_conv_table= conv_table;
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
      Old_rows_log_event contain at least one row, so after processing one
      Old_rows_log_event, we can invalidate the query cache for the
      associated table.
     */
    TABLE_LIST *ptr= rli->tables_to_lock;
    for (uint i=0; ptr && (i < rli->tables_to_lock_count); ptr= ptr->next_global, i++)
      const_cast<Relay_log_info*>(rli)->m_table_map.set_table(ptr->table_id, ptr->table);
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
    ev_thd->set_time((time_t)ev->when);
    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (ev->get_flags(Old_rows_log_event::NO_FOREIGN_KEY_CHECKS_F))
        ev_thd->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        ev_thd->variables.option_bits&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (ev->get_flags(Old_rows_log_event::RELAXED_UNIQUE_CHECKS_F))
        ev_thd->variables.option_bits|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        ev_thd->variables.option_bits&= ~OPTION_RELAXED_UNIQUE_CHECKS;
    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(ev_thd->variables.option_bits) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

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
      if ((error= do_prepare_row(ev_thd, rli, table, row_start, &row_end)))
        break; // We should perform the after-row operation even in
               // the case of error

      DBUG_ASSERT(row_end != NULL); // cannot happen
      DBUG_ASSERT(row_end <= ev->m_rows_end);

      /* in_use can have been set to NULL in close_tables_for_reopen */
      THD* old_thd= table->in_use;
      if (!table->in_use)
        table->in_use= ev_thd;
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
  rli->report(ERROR_LEVEL, ev_thd->stmt_da->sql_errno(),
                    "Error in %s event: row application failed. %s",
                    ev->get_type_str(),
                    ev_thd->is_error() ? ev_thd->stmt_da->message() : "");
  thd->is_slave_error= 1;
  break;
      }

      row_start= row_end;
    }
    DBUG_EXECUTE_IF("stop_slave_middle_group",
                    const_cast<Relay_log_info*>(rli)->abort_slave= 1;);
    error= do_after_row_operations(table, error);
  }

  if (error)
  {                     /* error has occured during the transaction */
    rli->report(ERROR_LEVEL, ev_thd->stmt_da->sql_errno(),
                "Error in %s event: error during transaction execution "
                "on table %s.%s. %s",
                ev->get_type_str(), table->s->db.str,
                table->s->table_name.str,
                ev_thd->is_error() ? ev_thd->stmt_da->message() : "");

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
    ev_thd->reset_current_stmt_binlog_format_row();
    const_cast<Relay_log_info*>(rli)->cleanup_context(ev_thd, error);
    ev_thd->is_slave_error= 1;
    DBUG_RETURN(error);
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
  uchar saved_x[2]= {0, 0}, saved_filler[2]= {0, 0};

  if (table->s->null_bytes > 0)
  {
    for (int i = 0 ; i < 2 ; ++i)
    { 
      /* 
        If we have an X bit then we need to take care of it.
      */
      if (!(table->s->db_options_in_use & HA_OPTION_PACK_RECORD))
      {
        saved_x[i]= table->record[i][0];
        table->record[i][0]|= 1U;
      }
      
      /*
         If (last_null_bit_pos == 0 && null_bytes > 1), then:

         X bit (if any) + N nullable fields + M Field_bit fields = 8 bits 

         Ie, the entire byte is used.
      */
      if (table->s->last_null_bit_pos > 0)
      {
        saved_filler[i]= table->record[i][table->s->null_bytes - 1];
        table->record[i][table->s->null_bytes - 1]|=
          256U - (1U << table->s->last_null_bit_pos);
      }
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
      if (!(table->s->db_options_in_use & HA_OPTION_PACK_RECORD))
        table->record[i][0]= saved_x[i];

      if (table->s->last_null_bit_pos > 0)
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
    memcpy(table->record[0] + master_reclength,
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
        DBUG_PRINT("info",("rnd_pos() returns error %d",error));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
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
        DBUG_PRINT("info", ("index_read_idx() returns error %d", error));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
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
    memcpy(table->record[1], table->record[0], table->s->reclength);
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

      while ((error= table->file->index_next(table->record[1])))
      {
        /* We just skip records that has already been deleted */
        if (error == HA_ERR_RECORD_DELETED)
          continue;
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
  restart_rnd_next:
      error= table->file->rnd_next(table->record[1]);

      DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
      DBUG_DUMP("record[1]", table->record[1], table->s->reclength);

      switch (error) {
      case 0:
        break;

      /*
        If the record was deleted, we pick the next one without doing
        any comparisons.
      */
      case HA_ERR_RECORD_DELETED:
        goto restart_rnd_next;

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
  my_free(m_memory); // Free for multi_malloc
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
  my_free(m_memory);
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
  memcpy(table->record[0], m_after_image, table->s->reclength);
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


/**************************************************************************
	Rows_log_event member functions
**************************************************************************/

#ifndef MYSQL_CLIENT
Old_rows_log_event::Old_rows_log_event(THD *thd_arg, TABLE *tbl_arg, ulong tid,
                                       MY_BITMAP const *cols,
                                       bool is_transactional)
  : Log_event(thd_arg, 0, is_transactional),
    m_row_count(0),
    m_table(tbl_arg),
    m_table_id(tid),
    m_width(tbl_arg ? tbl_arg->s->fields : 1),
    m_rows_buf(0), m_rows_cur(0), m_rows_end(0), m_flags(0) 
#ifdef HAVE_REPLICATION
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL)
#endif
{

  // This constructor should not be reached.
  assert(0);

  /*
    We allow a special form of dummy event when the table, and cols
    are null and the table id is ~0UL.  This is a temporary
    solution, to be able to terminate a started statement in the
    binary log: the extraneous events will be removed in the future.
   */
  DBUG_ASSERT((tbl_arg && tbl_arg->s && tid != ~0UL) ||
              (!tbl_arg && !cols && tid == ~0UL));

  if (thd_arg->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS)
      set_flags(NO_FOREIGN_KEY_CHECKS_F);
  if (thd_arg->variables.option_bits & OPTION_RELAXED_UNIQUE_CHECKS)
      set_flags(RELAXED_UNIQUE_CHECKS_F);
  /* if bitmap_init fails, caught in is_valid() */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                          m_width,
                          false)))
  {
    /* Cols can be zero if this is a dummy binrows event */
    if (likely(cols != NULL))
    {
      memcpy(m_cols.bitmap, cols->bitmap, no_bytes_in_map(cols));
      create_last_word_mask(&m_cols);
    }
  }
  else
  {
    // Needed because bitmap_init() does not set it to null on failure
    m_cols.bitmap= 0;
  }
}
#endif


Old_rows_log_event::Old_rows_log_event(const char *buf, uint event_len,
                                       Log_event_type event_type,
                                       const Format_description_log_event
                                       *description_event)
  : Log_event(buf, description_event),
    m_row_count(0),
#ifndef MYSQL_CLIENT
    m_table(NULL),
#endif
    m_table_id(0), m_rows_buf(0), m_rows_cur(0), m_rows_end(0)
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL)
#endif
{
  DBUG_ENTER("Old_rows_log_event::Old_Rows_log_event(const char*,...)");
  uint8 const common_header_len= description_event->common_header_len;
  uint8 const post_header_len= description_event->post_header_len[event_type-1];

  DBUG_PRINT("enter",("event_len: %u  common_header_len: %d  "
		      "post_header_len: %d",
		      event_len, common_header_len,
		      post_header_len));

  const char *post_start= buf + common_header_len;
  DBUG_DUMP("post_header", (uchar*) post_start, post_header_len);
  post_start+= RW_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    m_table_id= (ulong) uint6korr(post_start);
    post_start+= RW_FLAGS_OFFSET;
  }

  m_flags= uint2korr(post_start);

  uchar const *const var_start=
    (const uchar *)buf + common_header_len + post_header_len;
  uchar const *const ptr_width= var_start;
  uchar *ptr_after_width= (uchar*) ptr_width;
  DBUG_PRINT("debug", ("Reading from %p", ptr_after_width));
  m_width = net_field_length(&ptr_after_width);
  DBUG_PRINT("debug", ("m_width=%lu", m_width));
  /* if bitmap_init fails, catched in is_valid() */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                          m_width,
                          false)))
  {
    DBUG_PRINT("debug", ("Reading from %p", ptr_after_width));
    memcpy(m_cols.bitmap, ptr_after_width, (m_width + 7) / 8);
    create_last_word_mask(&m_cols);
    ptr_after_width+= (m_width + 7) / 8;
    DBUG_DUMP("m_cols", (uchar*) m_cols.bitmap, no_bytes_in_map(&m_cols));
  }
  else
  {
    // Needed because bitmap_init() does not set it to null on failure
    m_cols.bitmap= NULL;
    DBUG_VOID_RETURN;
  }

  const uchar* const ptr_rows_data= (const uchar*) ptr_after_width;
  size_t const data_size= event_len - (ptr_rows_data - (const uchar *) buf);
  DBUG_PRINT("info",("m_table_id: %lu  m_flags: %d  m_width: %lu  data_size: %lu",
                     m_table_id, m_flags, m_width, (ulong) data_size));
  DBUG_DUMP("rows_data", (uchar*) ptr_rows_data, data_size);

  m_rows_buf= (uchar*) my_malloc(data_size, MYF(MY_WME));
  if (likely((bool)m_rows_buf))
  {
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
    m_curr_row= m_rows_buf;
#endif
    m_rows_end= m_rows_buf + data_size;
    m_rows_cur= m_rows_end;
    memcpy(m_rows_buf, ptr_rows_data, data_size);
  }
  else
    m_cols.bitmap= 0; // to not free it

  DBUG_VOID_RETURN;
}


Old_rows_log_event::~Old_rows_log_event()
{
  if (m_cols.bitmap == m_bitbuf) // no my_malloc happened
    m_cols.bitmap= 0; // so no my_free in bitmap_free
  bitmap_free(&m_cols); // To pair with bitmap_init().
  my_free(m_rows_buf);
}


int Old_rows_log_event::get_data_size()
{
  uchar buf[sizeof(m_width)+1];
  uchar *end= net_store_length(buf, (m_width + 7) / 8);

  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  return 6 + no_bytes_in_map(&m_cols) + (end - buf) +
                  (m_rows_cur - m_rows_buf););
  int data_size= ROWS_HEADER_LEN;
  data_size+= no_bytes_in_map(&m_cols);
  data_size+= (uint) (end - buf);

  data_size+= (uint) (m_rows_cur - m_rows_buf);
  return data_size;
}


#ifndef MYSQL_CLIENT
int Old_rows_log_event::do_add_row_data(uchar *row_data, size_t length)
{
  /*
    When the table has a primary key, we would probably want, by default, to
    log only the primary key value instead of the entire "before image". This
    would save binlog space. TODO
  */
  DBUG_ENTER("Old_rows_log_event::do_add_row_data");
  DBUG_PRINT("enter", ("row_data: 0x%lx  length: %lu", (ulong) row_data,
                       (ulong) length));
  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
  DBUG_DUMP("row_data", row_data, min(length, 32));
#endif

  DBUG_ASSERT(m_rows_buf <= m_rows_cur);
  DBUG_ASSERT(!m_rows_buf || (m_rows_end && m_rows_buf < m_rows_end));
  DBUG_ASSERT(m_rows_cur <= m_rows_end);

  /* The cast will always work since m_rows_cur <= m_rows_end */
  if (static_cast<size_t>(m_rows_end - m_rows_cur) <= length)
  {
    size_t const block_size= 1024;
    my_ptrdiff_t const cur_size= m_rows_cur - m_rows_buf;
    my_ptrdiff_t const new_alloc= 
        block_size * ((cur_size + length + block_size - 1) / block_size);

    uchar* const new_buf= (uchar*)my_realloc((uchar*)m_rows_buf, (uint) new_alloc,
                                           MYF(MY_ALLOW_ZERO_PTR|MY_WME));
    if (unlikely(!new_buf))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);

    /* If the memory moved, we need to move the pointers */
    if (new_buf != m_rows_buf)
    {
      m_rows_buf= new_buf;
      m_rows_cur= m_rows_buf + cur_size;
    }

    /*
       The end pointer should always be changed to point to the end of
       the allocated memory.
    */
    m_rows_end= m_rows_buf + new_alloc;
  }

  DBUG_ASSERT(m_rows_cur + length <= m_rows_end);
  memcpy(m_rows_cur, row_data, length);
  m_rows_cur+= length;
  m_row_count++;
  DBUG_RETURN(0);
}
#endif


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Old_rows_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Old_rows_log_event::do_apply_event(Relay_log_info*)");
  int error= 0;

  /*
    If m_table_id == ~0UL, then we have a dummy event that does not
    contain any data.  In that case, we just remove all tables in the
    tables_to_lock list, close the thread tables, and return with
    success.
   */
  if (m_table_id == ~0UL)
  {
    /*
       This one is supposed to be set: just an extra check so that
       nothing strange has happened.
     */
    DBUG_ASSERT(get_flags(STMT_END_F));

    const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
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
    /*
      lock_tables() reads the contents of thd->lex, so they must be
      initialized. Contrary to in
      Table_map_log_event::do_apply_event() we don't call
      mysql_init_query() as that may reset the binlog format.
    */
    lex_start(thd);

    if ((error= lock_tables(thd, rli->tables_to_lock,
                               rli->tables_to_lock_count, 0)))
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
                    get_type_str());
        thd->is_fatal_error= 1;
      }
      else
      {
        rli->report(ERROR_LEVEL, error,
                    "Error in %s event: when locking tables",
                    get_type_str());
      }
      const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
      DBUG_RETURN(error);
    }

    /*
      When the open and locking succeeded, we check all tables to
      ensure that they still have the correct type.

      We can use a down cast here since we know that every table added
      to the tables_to_lock is a RPL_TABLE_LIST.
    */

    {
      RPL_TABLE_LIST *ptr= rli->tables_to_lock;
      for (uint i= 0 ; ptr&& (i< rli->tables_to_lock_count);
           ptr= static_cast<RPL_TABLE_LIST*>(ptr->next_global), i++)
      {
        TABLE *conv_table;
        if (ptr->m_tabledef.compatible_with(thd, const_cast<Relay_log_info*>(rli),
                                            ptr->table, &conv_table))
        {
          thd->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
          DBUG_RETURN(ERR_BAD_TABLE_DEF);
        }
        ptr->m_conv_table= conv_table;
      }
    }

    /*
      ... and then we add all the tables to the table map but keep
      them in the tables to lock list.


      We also invalidate the query cache for all the tables, since
      they will now be changed.

      TODO [/Matz]: Maybe the query cache should not be invalidated
      here? It might be that a table is not changed, even though it
      was locked for the statement.  We do know that each
      Old_rows_log_event contain at least one row, so after processing one
      Old_rows_log_event, we can invalidate the query cache for the
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

  TABLE* 
    table= 
    m_table= const_cast<Relay_log_info*>(rli)->m_table_map.get_table(m_table_id);

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
    thd->set_time((time_t)when);
    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (get_flags(NO_FOREIGN_KEY_CHECKS_F))
        thd->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        thd->variables.option_bits&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (get_flags(RELAXED_UNIQUE_CHECKS_F))
        thd->variables.option_bits|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        thd->variables.option_bits&= ~OPTION_RELAXED_UNIQUE_CHECKS;
    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(thd->variables.option_bits) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    /*
      Now we are in a statement and will stay in a statement until we
      see a STMT_END_F.

      We set this flag here, before actually applying any rows, in
      case the SQL thread is stopped and we need to detect that we're
      inside a statement and halting abruptly might cause problems
      when restarting.
     */
    const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

     if ( m_width == table->s->fields && bitmap_is_set_all(&m_cols))
      set_flags(COMPLETE_ROWS_F);

    /* 
      Set tables write and read sets.
      
      Read_set contains all slave columns (in case we are going to fetch
      a complete record from slave)
      
      Write_set equals the m_cols bitmap sent from master but it can be 
      longer if slave has extra columns. 
     */ 

    DBUG_PRINT_BITSET("debug", "Setting table's write_set from: %s", &m_cols);
    
    bitmap_set_all(table->read_set);
    bitmap_set_all(table->write_set);
    if (!get_flags(COMPLETE_ROWS_F))
      bitmap_intersect(table->write_set,&m_cols);

    // Do event specific preparations 
    
    error= do_before_row_operations(rli);

    // row processing loop

    while (error == 0 && m_curr_row < m_rows_end)
    {
      /* in_use can have been set to NULL in close_tables_for_reopen */
      THD* old_thd= table->in_use;
      if (!table->in_use)
        table->in_use= thd;

      error= do_exec_row(rli);

      DBUG_PRINT("info", ("error: %d", error));
      DBUG_ASSERT(error != HA_ERR_RECORD_DELETED);

      table->in_use = old_thd;
      switch (error)
      {
      case 0:
	break;

      /* Some recoverable errors */
      case HA_ERR_RECORD_CHANGED:
      case HA_ERR_KEY_NOT_FOUND:	/* Idempotency support: OK if
                                           tuple does not exist */
        error= 0;
        break;

      default:
	rli->report(ERROR_LEVEL, thd->net.last_errno,
                    "Error in %s event: row application failed. %s",
                    get_type_str(),
                    thd->net.last_error ? thd->net.last_error : "");
       thd->is_slave_error= 1;
	break;
      }

      /*
       If m_curr_row_end  was not set during event execution (e.g., because
       of errors) we can't proceed to the next row. If the error is transient
       (i.e., error==0 at this point) we must call unpack_current_row() to set 
       m_curr_row_end.
      */ 
   
      DBUG_PRINT("info", ("error: %d", error));
      DBUG_PRINT("info", ("curr_row: 0x%lu; curr_row_end: 0x%lu; rows_end: 0x%lu",
                          (ulong) m_curr_row, (ulong) m_curr_row_end, (ulong) m_rows_end));

      if (!m_curr_row_end && !error)
        unpack_current_row(rli);
  
      // at this moment m_curr_row_end should be set
      DBUG_ASSERT(error || m_curr_row_end != NULL); 
      DBUG_ASSERT(error || m_curr_row < m_curr_row_end);
      DBUG_ASSERT(error || m_curr_row_end <= m_rows_end);
  
      m_curr_row= m_curr_row_end;
 
    } // row processing loop

    DBUG_EXECUTE_IF("stop_slave_middle_group",
                    const_cast<Relay_log_info*>(rli)->abort_slave= 1;);
    error= do_after_row_operations(rli, error);
  } // if (table)

  if (error)
  {                     /* error has occured during the transaction */
    rli->report(ERROR_LEVEL, thd->net.last_errno,
                "Error in %s event: error during transaction execution "
                "on table %s.%s. %s",
                get_type_str(), table->s->db.str,
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
    thd->reset_current_stmt_binlog_format_row();
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
      !use_trans_cache() && get_flags(STMT_END_F) == RLE_NO_FLAGS)
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
      executing the final Old_rows_log_event). If we are in a hopeless
      wait (reached end of last relay log and nothing gets appended
      there), we timeout after one minute, and notify DBA about the
      problem.  When WL#2975 is implemented, just remove the member
      Relay_log_info::last_event_start_time and all its occurrences.
    */
    const_cast<Relay_log_info*>(rli)->last_event_start_time= my_time(0);
  }

  if (get_flags(STMT_END_F))
  {
    /*
      This is the end of a statement or transaction, so close (and
      unlock) the tables we opened when processing the
      Table_map_log_event starting the statement.

      OBSERVER.  This will clear *all* mappings, not only those that
      are open for the table. There is not good handle for on-close
      actions for tables.

      NOTE. Even if we have no table ('table' == 0) we still need to be
      here, so that we increase the group relay log position. If we didn't, we
      could have a group relay log position which lags behind "forever"
      (assume the last master's transaction is ignored by the slave because of
      replicate-ignore rules).
    */
    int binlog_error= thd->binlog_flush_pending_rows_event(TRUE);

    /*
      If this event is not in a transaction, the call below will, if some
      transactional storage engines are involved, commit the statement into
      them and flush the pending event to binlog.
      If this event is in a transaction, the call will do nothing, but a
      Xid_log_event will come next which will, if some transactional engines
      are involved, commit the transaction and flush the pending event to the
      binlog.
    */
    if ((error= (binlog_error ? trans_rollback_stmt(thd) : trans_commit_stmt(thd))))
      rli->report(ERROR_LEVEL, error,
                  "Error in %s event: commit of row events failed, "
                  "table `%s`.`%s`",
                  get_type_str(), m_table->s->db.str,
                  m_table->s->table_name.str);
    error|= binlog_error;

    /*
      Now what if this is not a transactional engine? we still need to
      flush the pending event to the binlog; we did it with
      thd->binlog_flush_pending_rows_event(). Note that we imitate
      what is done for real queries: a call to
      ha_autocommit_or_rollback() (sometimes only if involves a
      transactional engine), and a call to be sure to have the pending
      event flushed.
    */

    thd->reset_current_stmt_binlog_format_row();
    const_cast<Relay_log_info*>(rli)->cleanup_context(thd, 0);
  }

  DBUG_RETURN(error);
}


Log_event::enum_skip_reason
Old_rows_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1 and this event does not end a
    statement, then we should not start executing on the next event.
    Otherwise, we defer the decision to the normal skipping logic.
  */
  if (rli->slave_skip_counter == 1 && !get_flags(STMT_END_F))
    return Log_event::EVENT_SKIP_IGNORE;
  else
    return Log_event::do_shall_skip(rli);
}

int
Old_rows_log_event::do_update_pos(Relay_log_info *rli)
{
  DBUG_ENTER("Old_rows_log_event::do_update_pos");
  int error= 0;

  DBUG_PRINT("info", ("flags: %s",
                      get_flags(STMT_END_F) ? "STMT_END_F " : ""));

  if (get_flags(STMT_END_F))
  {
    /*
      Indicate that a statement is finished.
      Step the group log position if we are not in a transaction,
      otherwise increase the event log position.
     */
    rli->stmt_done(log_pos, when);
    /*
      Clear any errors in thd->net.last_err*. It is not known if this is
      needed or not. It is believed that any errors that may exist in
      thd->net.last_err* are allowed. Examples of errors are "key not
      found", which is produced in the test case rpl_row_conflicts.test
    */
    thd->clear_error();
  }
  else
  {
    rli->inc_event_relay_log_pos();
  }

  DBUG_RETURN(error);
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */


#ifndef MYSQL_CLIENT
bool Old_rows_log_event::write_data_header(IO_CACHE *file)
{
  uchar buf[ROWS_HEADER_LEN];	// No need to init the buffer

  // This method should not be reached.
  assert(0);

  DBUG_ASSERT(m_table_id != ~0UL);
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, m_table_id);
                    int2store(buf + 4, m_flags);
                    return (my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + RW_MAPID_OFFSET, (ulonglong)m_table_id);
  int2store(buf + RW_FLAGS_OFFSET, m_flags);
  return (my_b_safe_write(file, buf, ROWS_HEADER_LEN));
}


bool Old_rows_log_event::write_data_body(IO_CACHE*file)
{
  /*
     Note that this should be the number of *bits*, not the number of
     bytes.
  */
  uchar sbuf[sizeof(m_width)];
  my_ptrdiff_t const data_size= m_rows_cur - m_rows_buf;

  // This method should not be reached.
  assert(0);

  bool res= false;
  uchar *const sbuf_end= net_store_length(sbuf, (size_t) m_width);
  DBUG_ASSERT(static_cast<size_t>(sbuf_end - sbuf) <= sizeof(sbuf));

  DBUG_DUMP("m_width", sbuf, (size_t) (sbuf_end - sbuf));
  res= res || my_b_safe_write(file, sbuf, (size_t) (sbuf_end - sbuf));

  DBUG_DUMP("m_cols", (uchar*) m_cols.bitmap, no_bytes_in_map(&m_cols));
  res= res || my_b_safe_write(file, (uchar*) m_cols.bitmap,
                              no_bytes_in_map(&m_cols));
  DBUG_DUMP("rows", m_rows_buf, data_size);
  res= res || my_b_safe_write(file, m_rows_buf, (size_t) data_size);

  return res;

}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Old_rows_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  char const *const flagstr=
    get_flags(STMT_END_F) ? " flags: STMT_END_F" : "";
  size_t bytes= my_snprintf(buf, sizeof(buf),
                               "table_id: %lu%s", m_table_id, flagstr);
  protocol->store(buf, bytes, &my_charset_bin);
}
#endif


#ifdef MYSQL_CLIENT
void Old_rows_log_event::print_helper(FILE *file,
                                      PRINT_EVENT_INFO *print_event_info,
                                      char const *const name)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  IO_CACHE *const body= &print_event_info->body_cache;
  if (!print_event_info->short_form)
  {
    bool const last_stmt_event= get_flags(STMT_END_F);
    print_header(head, print_event_info, !last_stmt_event);
    my_b_printf(head, "\t%s: table id %lu%s\n",
                name, m_table_id,
                last_stmt_event ? " flags: STMT_END_F" : "");
    print_base64(body, print_event_info, !last_stmt_event);
  }

  if (get_flags(STMT_END_F))
  {
    copy_event_cache_to_file_and_reinit(head, file);
    copy_event_cache_to_file_and_reinit(body, file);
  }
}
#endif


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
/**
  Write the current row into event's table.

  The row is located in the row buffer, pointed by @c m_curr_row member.
  Number of columns of the row is stored in @c m_width member (it can be 
  different from the number of columns in the table to which we insert). 
  Bitmap @c m_cols indicates which columns are present in the row. It is assumed 
  that event's table is already open and pointed by @c m_table.

  If the same record already exists in the table it can be either overwritten 
  or an error is reported depending on the value of @c overwrite flag 
  (error reporting not yet implemented). Note that the matching record can be
  different from the row we insert if we use primary keys to identify records in
  the table.

  The row to be inserted can contain values only for selected columns. The 
  missing columns are filled with default values using @c prepare_record() 
  function. If a matching record is found in the table and @c overwritte is
  true, the missing columns are taken from it.

  @param  rli   Relay log info (needed for row unpacking).
  @param  overwrite  
                Shall we overwrite if the row already exists or signal 
                error (currently ignored).

  @returns Error code on failure, 0 on success.

  This method, if successful, sets @c m_curr_row_end pointer to point at the
  next row in the rows buffer. This is done when unpacking the row to be 
  inserted.

  @note If a matching record is found, it is either updated using 
  @c ha_update_row() or first deleted and then new record written.
*/ 

int
Old_rows_log_event::write_row(const Relay_log_info *const rli,
                              const bool overwrite)
{
  DBUG_ENTER("write_row");
  DBUG_ASSERT(m_table != NULL && thd != NULL);

  TABLE *table= m_table;  // pointer to event's table
  int error;
  int keynum;
  auto_afree_ptr<char> key(NULL);

  /* fill table->record[0] with default values */

  if ((error= prepare_record(table, m_width,
                             TRUE /* check if columns have def. values */)))
    DBUG_RETURN(error);
  
  /* unpack row into table->record[0] */
  error= unpack_current_row(rli); // TODO: how to handle errors?

#ifndef DBUG_OFF
  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_PRINT_BITSET("debug", "write_set = %s", table->write_set);
  DBUG_PRINT_BITSET("debug", "read_set = %s", table->read_set);
#endif

  /* 
    Try to write record. If a corresponding record already exists in the table,
    we try to change it using ha_update_row() if possible. Otherwise we delete
    it and repeat the whole process again. 

    TODO: Add safety measures against infinite looping. 
   */

  while ((error= table->file->ha_write_row(table->record[0])))
  {
    if (error == HA_ERR_LOCK_DEADLOCK || error == HA_ERR_LOCK_WAIT_TIMEOUT)
    {
      table->file->print_error(error, MYF(0)); /* to check at exec_relay_log_event */
      DBUG_RETURN(error);
    }
    if ((keynum= table->file->get_dup_key(error)) < 0)
    {
      DBUG_PRINT("info",("Can't locate duplicate key (get_dup_key returns %d)",keynum));
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
      DBUG_PRINT("info",("Locating offending record using rnd_pos()"));
      error= table->file->rnd_pos(table->record[1], table->file->dup_ref);
      if (error)
      {
        DBUG_PRINT("info",("rnd_pos() returns error %d",error));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
    }
    else
    {
      DBUG_PRINT("info",("Locating offending record using index_read_idx()"));

      if (table->file->extra(HA_EXTRA_FLUSH_CACHE))
      {
        DBUG_PRINT("info",("Error when setting HA_EXTRA_FLUSH_CACHE"));
        DBUG_RETURN(my_errno);
      }

      if (key.get() == NULL)
      {
        key.assign(static_cast<char*>(my_alloca(table->s->max_unique_length)));
        if (key.get() == NULL)
        {
          DBUG_PRINT("info",("Can't allocate key buffer"));
          DBUG_RETURN(ENOMEM);
        }
      }

      key_copy((uchar*)key.get(), table->record[0], table->key_info + keynum,
               0);
      error= table->file->index_read_idx_map(table->record[1], keynum,
                                             (const uchar*)key.get(),
                                             HA_WHOLE_KEY,
                                             HA_READ_KEY_EXACT);
      if (error)
      {
        DBUG_PRINT("info",("index_read_idx() returns error %d", error));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
    }

    /*
       Now, record[1] should contain the offending row.  That
       will enable us to update it or, alternatively, delete it (so
       that we can insert the new row afterwards).
     */

    /*
      If row is incomplete we will use the record found to fill 
      missing columns.  
    */
    if (!get_flags(COMPLETE_ROWS_F))
    {
      restore_record(table,record[1]);
      error= unpack_current_row(rli);
    }

#ifndef DBUG_OFF
    DBUG_PRINT("debug",("preparing for update: before and after image"));
    DBUG_DUMP("record[1] (before)", table->record[1], table->s->reclength);
    DBUG_DUMP("record[0] (after)", table->record[0], table->s->reclength);
#endif

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
      DBUG_PRINT("info",("Updating row using ha_update_row()"));
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      switch (error) {
                
      case HA_ERR_RECORD_IS_THE_SAME:
        DBUG_PRINT("info",("ignoring HA_ERR_RECORD_IS_THE_SAME error from"
                           " ha_update_row()"));
        error= 0;
      
      case 0:
        break;
        
      default:    
        DBUG_PRINT("info",("ha_update_row() returns error %d",error));
        table->file->print_error(error, MYF(0));
      }
      
      DBUG_RETURN(error);
    }
    else
    {
      DBUG_PRINT("info",("Deleting offending row and trying to write new one again"));
      if ((error= table->file->ha_delete_row(table->record[1])))
      {
        DBUG_PRINT("info",("ha_delete_row() returns error %d",error));
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
      /* Will retry ha_write_row() with the offending row removed. */
    }
  }

  DBUG_RETURN(error);
}


/**
  Locate the current row in event's table.

  The current row is pointed by @c m_curr_row. Member @c m_width tells how many 
  columns are there in the row (this can be differnet from the number of columns 
  in the table). It is assumed that event's table is already open and pointed 
  by @c m_table.

  If a corresponding record is found in the table it is stored in 
  @c m_table->record[0]. Note that when record is located based on a primary 
  key, it is possible that the record found differs from the row being located.

  If no key is specified or table does not have keys, a table scan is used to 
  find the row. In that case the row should be complete and contain values for
  all columns. However, it can still be shorter than the table, i.e. the table 
  can contain extra columns not present in the row. It is also possible that 
  the table has fewer columns than the row being located. 

  @returns Error code on failure, 0 on success. 
  
  @post In case of success @c m_table->record[0] contains the record found. 
  Also, the internal "cursor" of the table is positioned at the record found.

  @note If the engine allows random access of the records, a combination of
  @c position() and @c rnd_pos() will be used. 
 */

int Old_rows_log_event::find_row(const Relay_log_info *rli)
{
  DBUG_ENTER("find_row");

  DBUG_ASSERT(m_table && m_table->in_use != NULL);

  TABLE *table= m_table;
  int error;

  /* unpack row - missing fields get default values */

  // TODO: shall we check and report errors here?
  prepare_record(table, m_width, FALSE /* don't check errors */); 
  error= unpack_current_row(rli); 

#ifndef DBUG_OFF
  DBUG_PRINT("info",("looking for the following record"));
  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
#endif

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
    DBUG_PRINT("info",("locating record using primary key (position)"));
    int error= table->file->rnd_pos_by_record(table->record[0]);
    if (error)
    {
      DBUG_PRINT("info",("rnd_pos returns error %d",error));
      if (error == HA_ERR_RECORD_DELETED)
        error= HA_ERR_KEY_NOT_FOUND;
      table->file->print_error(error, MYF(0));
    }
    DBUG_RETURN(error);
  }

  // We can't use position() - try other methods.
  
  /* 
    We need to retrieve all fields
    TODO: Move this out from this function to main loop 
   */
  table->use_all_columns();

  /*
    Save copy of the record in table->record[1]. It might be needed 
    later if linear search is used to find exact match.
   */ 
  store_record(table,record[1]);    

  if (table->s->keys > 0)
  {
    DBUG_PRINT("info",("locating record using primary key (index_read)"));

    /* We have a key: search the table using the index */
    if (!table->file->inited && (error= table->file->ha_index_init(0, FALSE)))
    {
      DBUG_PRINT("info",("ha_index_init returns error %d",error));
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(error);
    }

    /* Fill key data for the row */

    DBUG_ASSERT(m_key);
    key_copy(m_key, table->record[0], table->key_info, 0);

    /*
      Don't print debug messages when running valgrind since they can
      trigger false warnings.
     */
#ifndef HAVE_purify
    DBUG_DUMP("key data", m_key, table->key_info->key_length);
#endif

    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    my_ptrdiff_t const pos=
      table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
    table->record[0][pos]= 0xFF;
    
    if ((error= table->file->index_read_map(table->record[0], m_key, 
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT)))
    {
      DBUG_PRINT("info",("no record matching the key found in the table"));
      if (error == HA_ERR_RECORD_DELETED)
        error= HA_ERR_KEY_NOT_FOUND;
      table->file->print_error(error, MYF(0));
      table->file->ha_index_end();
      DBUG_RETURN(error);
    }

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
    DBUG_PRINT("info",("found first matching record")); 
    DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
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
      /* Unique does not have non nullable part */
      if (!(table->key_info->flags & (HA_NULL_PART_KEY)))
      {
        table->file->ha_index_end();
        DBUG_RETURN(0);
      }
      else
      {
        KEY *keyinfo= table->key_info;
        /*
          Unique has nullable part. We need to check if there is any field in the
          BI image that is null and part of UNNI.
        */
        bool null_found= FALSE;
        for (uint i=0; i < keyinfo->key_parts && !null_found; i++)
        {
          uint fieldnr= keyinfo->key_part[i].fieldnr - 1;
          Field **f= table->field+fieldnr;
          null_found= (*f)->is_null();
        }

        if (!null_found)
        {
          table->file->ha_index_end();
          DBUG_RETURN(0);
        }

        /* else fall through to index scan */
      }
    }

    /*
      In case key is not unique, we still have to iterate over records found
      and find the one which is identical to the row given. A copy of the 
      record we are looking for is stored in record[1].
     */ 
    DBUG_PRINT("info",("non-unique index, scanning it to find matching record")); 

    while (record_compare(table))
    {
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
        table->record[0][table->s->null_bytes - 1]|=
          256U - (1U << table->s->last_null_bit_pos);
      }

      while ((error= table->file->index_next(table->record[0])))
      {
        /* We just skip records that has already been deleted */
        if (error == HA_ERR_RECORD_DELETED)
          continue;
        DBUG_PRINT("info",("no record matching the given row found"));
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
    DBUG_PRINT("info",("locating record using table scan (rnd_next)"));

    int restart_count= 0; // Number of times scanning has restarted from top

    /* We don't have a key: search the table using rnd_next() */
    if ((error= table->file->ha_rnd_init(1)))
    {
      DBUG_PRINT("info",("error initializing table scan"
                         " (ha_rnd_init returns %d)",error));
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(error);
    }

    /* Continue until we find the right record or have made a full loop */
    do
    {
  restart_rnd_next:
      error= table->file->rnd_next(table->record[0]);

      switch (error) {

      case 0:
        break;

      case HA_ERR_RECORD_DELETED:
        goto restart_rnd_next;

      case HA_ERR_END_OF_FILE:
        if (++restart_count < 2)
          table->file->ha_rnd_init(1);
        break;

      default:
        DBUG_PRINT("info", ("Failed to get next record"
                            " (rnd_next returns %d)",error));
        table->file->print_error(error, MYF(0));
        table->file->ha_rnd_end();
        DBUG_RETURN(error);
      }
    }
    while (restart_count < 2 && record_compare(table));
    
    /* 
      Note: above record_compare will take into accout all record fields 
      which might be incorrect in case a partial row was given in the event
     */

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    if (restart_count == 2)
      DBUG_PRINT("info", ("Record not found"));
    else
      DBUG_DUMP("record found", table->record[0], table->s->reclength);
    table->file->ha_rnd_end();

    DBUG_ASSERT(error == HA_ERR_END_OF_FILE || error == 0);
    DBUG_RETURN(error);
  }

  DBUG_RETURN(0);
}

#endif


/**************************************************************************
	Write_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Write_rows_log_event_old::Write_rows_log_event_old(THD *thd_arg,
                                                   TABLE *tbl_arg,
                                                   ulong tid_arg,
                                                   MY_BITMAP const *cols,
                                                   bool is_transactional)
  : Old_rows_log_event(thd_arg, tbl_arg, tid_arg, cols, is_transactional)
{

  // This constructor should not be reached.
  assert(0);

}
#endif


/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Write_rows_log_event_old::Write_rows_log_event_old(const char *buf,
                                                   uint event_len,
                                                   const Format_description_log_event
                                                   *description_event)
: Old_rows_log_event(buf, event_len, PRE_GA_WRITE_ROWS_EVENT,
                     description_event)
{
}
#endif


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int 
Write_rows_log_event_old::do_before_row_operations(const Slave_reporting_capability *const)
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
  m_table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  /* 
     NDB specific: update from ndb master wrapped as Write_rows
  */
  /*
    so that the event should be applied to replace slave's row
  */
  m_table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  /* 
     NDB specific: if update from ndb master wrapped as Write_rows
     does not find the row it's assumed idempotent binlog applying
     is taking place; don't raise the error.
  */
  m_table->file->extra(HA_EXTRA_IGNORE_NO_KEY);
  /*
    TODO: the cluster team (Tomas?) says that it's better if the engine knows
    how many rows are going to be inserted, then it can allocate needed memory
    from the start.
  */
  m_table->file->ha_start_bulk_insert(0);
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
  m_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  return error;
}


int 
Write_rows_log_event_old::do_after_row_operations(const Slave_reporting_capability *const,
                                                  int error)
{
  int local_error= 0;
  m_table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  m_table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
  /*
    reseting the extra with 
    table->file->extra(HA_EXTRA_NO_IGNORE_NO_KEY); 
    fires bug#27077
    todo: explain or fix
  */
  if ((local_error= m_table->file->ha_end_bulk_insert()))
  {
    m_table->file->print_error(local_error, MYF(0));
  }
  return error? error : local_error;
}


int 
Write_rows_log_event_old::do_exec_row(const Relay_log_info *const rli)
{
  DBUG_ASSERT(m_table != NULL);
  int error= write_row(rli, TRUE /* overwrite */);
  
  if (error && !thd->net.last_errno)
    thd->net.last_errno= error;
      
  return error; 
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */


#ifdef MYSQL_CLIENT
void Write_rows_log_event_old::print(FILE *file,
                                     PRINT_EVENT_INFO* print_event_info)
{
  Old_rows_log_event::print_helper(file, print_event_info, "Write_rows_old");
}
#endif


/**************************************************************************
	Delete_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */

#ifndef MYSQL_CLIENT
Delete_rows_log_event_old::Delete_rows_log_event_old(THD *thd_arg,
                                                     TABLE *tbl_arg,
                                                     ulong tid,
                                                     MY_BITMAP const *cols,
                                                     bool is_transactional)
  : Old_rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional),
    m_after_image(NULL), m_memory(NULL)
{

  // This constructor should not be reached.
  assert(0);

}
#endif /* #if !defined(MYSQL_CLIENT) */


/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Delete_rows_log_event_old::Delete_rows_log_event_old(const char *buf,
                                                     uint event_len,
                                                     const Format_description_log_event
                                                     *description_event)
  : Old_rows_log_event(buf, event_len, PRE_GA_DELETE_ROWS_EVENT,
                       description_event),
    m_after_image(NULL), m_memory(NULL)
{
}
#endif


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int 
Delete_rows_log_event_old::do_before_row_operations(const Slave_reporting_capability *const)
{
  if ((m_table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      m_table->s->primary_key < MAX_KEY)
  {
    /*
      We don't need to allocate any memory for m_key since it is not used.
    */
    return 0;
  }

  if (m_table->s->keys > 0)
  {
    // Allocate buffer for key searches
    m_key= (uchar*)my_malloc(m_table->key_info->key_length, MYF(MY_WME));
    if (!m_key)
      return HA_ERR_OUT_OF_MEM;
  }
  return 0;
}


int 
Delete_rows_log_event_old::do_after_row_operations(const Slave_reporting_capability *const,
                                                   int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  my_free(m_key);
  m_key= NULL;

  return error;
}


int Delete_rows_log_event_old::do_exec_row(const Relay_log_info *const rli)
{
  int error;
  DBUG_ASSERT(m_table != NULL);

  if (!(error= find_row(rli))) 
  { 
    /*
      Delete the record found, located in record[0]
    */
    error= m_table->file->ha_delete_row(m_table->record[0]);
  }
  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */


#ifdef MYSQL_CLIENT
void Delete_rows_log_event_old::print(FILE *file,
                                      PRINT_EVENT_INFO* print_event_info)
{
  Old_rows_log_event::print_helper(file, print_event_info, "Delete_rows_old");
}
#endif


/**************************************************************************
	Update_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Update_rows_log_event_old::Update_rows_log_event_old(THD *thd_arg,
                                                     TABLE *tbl_arg,
                                                     ulong tid,
                                                     MY_BITMAP const *cols,
                                                     bool is_transactional)
  : Old_rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional),
    m_after_image(NULL), m_memory(NULL)
{

  // This constructor should not be reached.
  assert(0);
}
#endif /* !defined(MYSQL_CLIENT) */


/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Update_rows_log_event_old::Update_rows_log_event_old(const char *buf,
                                                     uint event_len,
                                                     const
                                                     Format_description_log_event
                                                     *description_event)
  : Old_rows_log_event(buf, event_len, PRE_GA_UPDATE_ROWS_EVENT,
                       description_event),
    m_after_image(NULL), m_memory(NULL)
{
}
#endif


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int 
Update_rows_log_event_old::do_before_row_operations(const Slave_reporting_capability *const)
{
  if (m_table->s->keys > 0)
  {
    // Allocate buffer for key searches
    m_key= (uchar*)my_malloc(m_table->key_info->key_length, MYF(MY_WME));
    if (!m_key)
      return HA_ERR_OUT_OF_MEM;
  }

  m_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return 0;
}


int 
Update_rows_log_event_old::do_after_row_operations(const Slave_reporting_capability *const,
                                                   int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  my_free(m_key); // Free for multi_malloc
  m_key= NULL;

  return error;
}


int 
Update_rows_log_event_old::do_exec_row(const Relay_log_info *const rli)
{
  DBUG_ASSERT(m_table != NULL);

  int error= find_row(rli); 
  if (error)
  {
    /*
      We need to read the second image in the event of error to be
      able to skip to the next pair of updates
    */
    m_curr_row= m_curr_row_end;
    unpack_current_row(rli);
    return error;
  }

  /*
    This is the situation after locating BI:

    ===|=== before image ====|=== after image ===|===
       ^                     ^
       m_curr_row            m_curr_row_end

    BI found in the table is stored in record[0]. We copy it to record[1]
    and unpack AI to record[0].
   */

  store_record(m_table,record[1]);

  m_curr_row= m_curr_row_end;
  error= unpack_current_row(rli); // this also updates m_curr_row_end

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row is in record[0].
  */
#ifndef HAVE_purify
  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
  DBUG_PRINT("info",("Updating row in table"));
  DBUG_DUMP("old record", m_table->record[1], m_table->s->reclength);
  DBUG_DUMP("new values", m_table->record[0], m_table->s->reclength);
#endif

  error= m_table->file->ha_update_row(m_table->record[1], m_table->record[0]);
  if (error == HA_ERR_RECORD_IS_THE_SAME)
    error= 0;

  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */


#ifdef MYSQL_CLIENT
void Update_rows_log_event_old::print(FILE *file,
                                      PRINT_EVENT_INFO* print_event_info)
{
  Old_rows_log_event::print_helper(file, print_event_info, "Update_rows_old");
}
#endif
