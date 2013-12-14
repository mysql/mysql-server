/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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


/* Copy data from a textfile to table */
/* 2006-12 Erik Wetterberg : LOAD XML added */

#include "sql_priv.h"
#include "unireg.h"
#include "sql_load.h"
#include "sql_load.h"
#include "sql_cache.h"                          // query_cache_*
#include "sql_base.h"          // fill_record_n_invoke_before_triggers
#include <my_dir.h>
#include "sql_view.h"                           // check_key_in_view
#include "sql_insert.h" // check_that_all_fields_are_given_values,
                        // prepare_triggers_for_insert_stmt,
                        // write_record
#include "sql_acl.h"    // INSERT_ACL, UPDATE_ACL
#include "log_event.h"  // Delete_file_log_event,
                        // Execute_load_query_log_event,
                        // LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F
#include <m_ctype.h>
#include "rpl_mi.h"
#include "sql_repl.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_show.h"
class XML_TAG {
public:
  int level;
  String field;
  String value;
  XML_TAG(int l, String f, String v);
};


XML_TAG::XML_TAG(int l, String f, String v)
{
  level= l;
  field.append(f);
  value.append(v);
}


class READ_INFO {
  File	file;
  uchar	*buffer,			/* Buffer for read text */
	*end_of_buff;			/* Data in bufferts ends here */
  uint	buff_length,			/* Length of buffert */
	max_length;			/* Max length of row */
  char	*field_term_ptr,*line_term_ptr,*line_start_ptr,*line_start_end;
  uint	field_term_length,line_term_length,enclosed_length;
  int	field_term_char,line_term_char,enclosed_char,escape_char;
  int	*stack,*stack_pos;
  bool	found_end_of_line,start_of_line,eof;
  bool  need_end_io_cache;
  IO_CACHE cache;
  NET *io_net;
  int level; /* for load xml */

public:
  bool error,line_cuted,found_null,enclosed;
  uchar	*row_start,			/* Found row starts here */
	*row_end;			/* Found row ends here */
  CHARSET_INFO *read_charset;

  READ_INFO(File file,uint tot_length,CHARSET_INFO *cs,
	    String &field_term,String &line_start,String &line_term,
	    String &enclosed,int escape,bool get_it_from_net, bool is_fifo);
  ~READ_INFO();
  int read_field();
  int read_fixed_length(void);
  int next_line(void);
  char unescape(char chr);
  int terminator(char *ptr,uint length);
  bool find_start_of_fields();
  /* load xml */
  List<XML_TAG> taglist;
  int read_value(int delim, String *val);
  int read_xml();
  int clear_level(int level);

  /*
    We need to force cache close before destructor is invoked to log
    the last read block
  */
  void end_io_cache()
  {
    ::end_io_cache(&cache);
    need_end_io_cache = 0;
  }

  /*
    Either this method, or we need to make cache public
    Arg must be set from mysql_load() since constructor does not see
    either the table or THD value
  */
  void set_io_cache_arg(void* arg) { cache.arg = arg; }
};

static int read_fixed_length(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
                             List<Item> &fields_vars, List<Item> &set_fields,
                             List<Item> &set_values, READ_INFO &read_info,
			     ulong skip_lines,
			     bool ignore_check_option_errors);
static int read_sep_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
                          List<Item> &fields_vars, List<Item> &set_fields,
                          List<Item> &set_values, READ_INFO &read_info,
			  String &enclosed, ulong skip_lines,
			  bool ignore_check_option_errors);

static int read_xml_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
                          List<Item> &fields_vars, List<Item> &set_fields,
                          List<Item> &set_values, READ_INFO &read_info,
                          String &enclosed, ulong skip_lines,
                          bool ignore_check_option_errors);

#ifndef EMBEDDED_LIBRARY
static bool write_execute_load_query_log_event(THD *thd, sql_exchange* ex,
                                               const char* db_arg, /* table's database */
                                               const char* table_name_arg,
                                               bool is_concurrent,
                                               enum enum_duplicates duplicates,
                                               bool ignore,
                                               bool transactional_table,
                                               int errocode);
#endif /* EMBEDDED_LIBRARY */

/*
  Execute LOAD DATA query

  SYNOPSYS
    mysql_load()
      thd - current thread
      ex  - sql_exchange object representing source file and its parsing rules
      table_list  - list of tables to which we are loading data
      fields_vars - list of fields and variables to which we read
                    data from file
      set_fields  - list of fields mentioned in set clause
      set_values  - expressions to assign to fields in previous list
      handle_duplicates - indicates whenever we should emit error or
                          replace row if we will meet duplicates.
      ignore -          - indicates whenever we should ignore duplicates
      read_file_from_client - is this LOAD DATA LOCAL ?

  RETURN VALUES
    TRUE - error / FALSE - success
*/

int mysql_load(THD *thd,sql_exchange *ex,TABLE_LIST *table_list,
	        List<Item> &fields_vars, List<Item> &set_fields,
                List<Item> &set_values,
                enum enum_duplicates handle_duplicates, bool ignore,
                bool read_file_from_client)
{
  char name[FN_REFLEN];
  File file;
  TABLE *table= NULL;
  int error= 0;
  String *field_term=ex->field_term,*escaped=ex->escaped;
  String *enclosed=ex->enclosed;
  bool is_fifo=0;
#ifndef EMBEDDED_LIBRARY
  LOAD_FILE_INFO lf_info;
  THD::killed_state killed_status= THD::NOT_KILLED;
  bool is_concurrent;
#endif
  char *db = table_list->db;			// This is never null
  /*
    If path for file is not defined, we will use the current database.
    If this is not set, we will use the directory where the table to be
    loaded is located
  */
  char *tdb= thd->db ? thd->db : db;		// Result is never null
  ulong skip_lines= ex->skip_lines;
  bool transactional_table;
  DBUG_ENTER("mysql_load");

  /*
    Bug #34283
    mysqlbinlog leaves tmpfile after termination if binlog contains
    load data infile, so in mixed mode we go to row-based for
    avoiding the problem.
  */
  thd->set_current_stmt_binlog_format_row_if_mixed();

#ifdef EMBEDDED_LIBRARY
  read_file_from_client  = 0; //server is always in the same process 
#endif

  if (escaped->length() > 1 || enclosed->length() > 1)
  {
    my_message(ER_WRONG_FIELD_TERMINATORS,ER(ER_WRONG_FIELD_TERMINATORS),
	       MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* Report problems with non-ascii separators */
  if (!escaped->is_ascii() || !enclosed->is_ascii() ||
      !field_term->is_ascii() ||
      !ex->line_term->is_ascii() || !ex->line_start->is_ascii())
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED,
                 ER(WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED));
  } 

  if (open_and_lock_tables(thd, table_list, TRUE, 0))
    DBUG_RETURN(TRUE);
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list,
                                    &thd->lex->select_lex.leaf_tables, FALSE,
                                    INSERT_ACL | UPDATE_ACL,
                                    INSERT_ACL | UPDATE_ACL))
     DBUG_RETURN(-1);
  if (!table_list->table ||               // do not suport join view
      !table_list->updatable ||           // and derived tables
      check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "LOAD");
    DBUG_RETURN(TRUE);
  }
  if (table_list->prepare_where(thd, 0, TRUE) ||
      table_list->prepare_check_option(thd))
  {
    DBUG_RETURN(TRUE);
  }
  /*
    Let us emit an error if we are loading data to table which is used
    in subselect in SET clause like we do it for INSERT.

    The main thing to fix to remove this restriction is to ensure that the
    table is marked to be 'used for insert' in which case we should never
    mark this table as 'const table' (ie, one that has only one row).
  */
  if (unique_table(thd, table_list, table_list->next_global, 0))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->table_name);
    DBUG_RETURN(TRUE);
  }

  table= table_list->table;
  transactional_table= table->file->has_transactions();
#ifndef EMBEDDED_LIBRARY
  is_concurrent= (table_list->lock_type == TL_WRITE_CONCURRENT_INSERT);
#endif

  if (!fields_vars.elements)
  {
    Field **field;
    for (field=table->field; *field ; field++)
      fields_vars.push_back(new Item_field(*field));
    bitmap_set_all(table->write_set);
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    /*
      Let us also prepare SET clause, altough it is probably empty
      in this case.
    */
    if (setup_fields(thd, 0, set_fields, MARK_COLUMNS_WRITE, 0, 0) ||
        setup_fields(thd, 0, set_values, MARK_COLUMNS_READ, 0, 0))
      DBUG_RETURN(TRUE);
  }
  else
  {						// Part field list
    /* TODO: use this conds for 'WITH CHECK OPTIONS' */
    if (setup_fields(thd, 0, fields_vars, MARK_COLUMNS_WRITE, 0, 0) ||
        setup_fields(thd, 0, set_fields, MARK_COLUMNS_WRITE, 0, 0) ||
        check_that_all_fields_are_given_values(thd, table, table_list))
      DBUG_RETURN(TRUE);
    /*
      Check whenever TIMESTAMP field with auto-set feature specified
      explicitly.
    */
    if (table->timestamp_field)
    {
      if (bitmap_is_set(table->write_set,
                        table->timestamp_field->field_index))
        table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
      else
      {
        bitmap_set_bit(table->write_set,
                       table->timestamp_field->field_index);
      }
    }
    /* Fix the expressions in SET clause */
    if (setup_fields(thd, 0, set_values, MARK_COLUMNS_READ, 0, 0))
      DBUG_RETURN(TRUE);
  }

  prepare_triggers_for_insert_stmt(table);

  uint tot_length=0;
  bool use_blobs= 0, use_vars= 0;
  List_iterator_fast<Item> it(fields_vars);
  Item *item;

  while ((item= it++))
  {
    Item *real_item= item->real_item();

    if (real_item->type() == Item::FIELD_ITEM)
    {
      Field *field= ((Item_field*)real_item)->field;
      if (field->flags & BLOB_FLAG)
      {
        use_blobs= 1;
        tot_length+= 256;			// Will be extended if needed
      }
      else
        tot_length+= field->field_length;
    }
    else if (item->type() == Item::STRING_ITEM)
      use_vars= 1;
  }
  if (use_blobs && !ex->line_term->length() && !field_term->length())
  {
    my_message(ER_BLOBS_AND_NO_TERMINATED,ER(ER_BLOBS_AND_NO_TERMINATED),
	       MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (use_vars && !field_term->length() && !enclosed->length())
  {
    my_error(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* We can't give an error in the middle when using LOCAL files */
  if (read_file_from_client && handle_duplicates == DUP_ERROR)
    ignore= 1;

#ifndef EMBEDDED_LIBRARY
  if (read_file_from_client)
  {
    (void)net_request_file(&thd->net,ex->file_name);
    file = -1;
  }
  else
#endif
  {
#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
    ex->file_name+=dirname_length(ex->file_name);
#endif
    if (!dirname_length(ex->file_name))
    {
      strxnmov(name, FN_REFLEN-1, mysql_real_data_home, tdb, NullS);
      (void) fn_format(name, ex->file_name, name, "",
		       MY_RELATIVE_PATH | MY_UNPACK_FILENAME);
    }
    else
    {
      (void) fn_format(name, ex->file_name, mysql_real_data_home, "",
                       MY_RELATIVE_PATH | MY_UNPACK_FILENAME |
                       MY_RETURN_REAL_PATH);
    }

    if (thd->slave_thread)
    {
#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
      if (strncmp(active_mi->rli.slave_patternload_file, name, 
          active_mi->rli.slave_patternload_file_size))
      {
        /*
          LOAD DATA INFILE in the slave SQL Thread can only read from 
          --slave-load-tmpdir". This should never happen. Please, report a bug.
        */

        sql_print_error("LOAD DATA INFILE in the slave SQL Thread can only read from --slave-load-tmpdir. " \
                        "Please, report a bug.");
        my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--slave-load-tmpdir");
        DBUG_RETURN(TRUE);
      }
#else
      /*
        This is impossible and should never happen.
      */
      DBUG_ASSERT(FALSE); 
#endif
    }
    else if (!is_secure_file_path(name))
    {
      /* Read only allowed from within dir specified by secure_file_priv */
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
      DBUG_RETURN(TRUE);
    }

#if !defined(__WIN__) && ! defined(__NETWARE__)
    MY_STAT stat_info;
    if (!my_stat(name, &stat_info, MYF(MY_WME)))
      DBUG_RETURN(TRUE);

    // if we are not in slave thread, the file must be:
    if (!thd->slave_thread &&
        !((stat_info.st_mode & S_IFLNK) != S_IFLNK &&   // symlink
          ((stat_info.st_mode & S_IFREG) == S_IFREG ||  // regular file
           (stat_info.st_mode & S_IFIFO) == S_IFIFO)))  // named pipe
    {
      my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), name);
      DBUG_RETURN(TRUE);
    }
    if ((stat_info.st_mode & S_IFIFO) == S_IFIFO)
      is_fifo= 1;
#endif
    if ((file= mysql_file_open(key_file_load,
                               name, O_RDONLY, MYF(MY_WME))) < 0)

      DBUG_RETURN(TRUE);
  }

  COPY_INFO info;
  bzero((char*) &info,sizeof(info));
  info.ignore= ignore;
  info.handle_duplicates=handle_duplicates;
  info.escape_char= (escaped->length() && (ex->escaped_given() ||
                    !(thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)))
                    ? (*escaped)[0] : INT_MAX;

  READ_INFO read_info(file,tot_length,
                      ex->cs ? ex->cs : thd->variables.collation_database,
		      *field_term,*ex->line_start, *ex->line_term, *enclosed,
		      info.escape_char, read_file_from_client, is_fifo);
  if (read_info.error)
  {
    if (file >= 0)
      mysql_file_close(file, MYF(0));           // no files in net reading
    DBUG_RETURN(TRUE);				// Can't allocate buffers
  }

#ifndef EMBEDDED_LIBRARY
  if (mysql_bin_log.is_open())
  {
    lf_info.thd = thd;
    lf_info.wrote_create_file = 0;
    lf_info.last_pos_in_file = HA_POS_ERROR;
    lf_info.log_delayed= transactional_table;
    read_info.set_io_cache_arg((void*) &lf_info);
  }
#endif /*!EMBEDDED_LIBRARY*/

  thd->count_cuted_fields= CHECK_FIELD_WARN;		/* calc cuted fields */
  thd->cuted_fields=0L;
  /* Skip lines if there is a line terminator */
  if (ex->line_term->length() && ex->filetype != FILETYPE_XML)
  {
    /* ex->skip_lines needs to be preserved for logging */
    while (skip_lines > 0)
    {
      skip_lines--;
      if (read_info.next_line())
	break;
    }
  }

  if (!(error=test(read_info.error)))
  {

    table->next_number_field=table->found_next_number_field;
    if (ignore ||
	handle_duplicates == DUP_REPLACE)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (handle_duplicates == DUP_REPLACE &&
        (!table->triggers ||
         !table->triggers->has_delete_triggers()))
        table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
      table->file->ha_start_bulk_insert((ha_rows) 0);
    table->copy_blobs=1;

    thd->abort_on_warning= (!ignore &&
                            (thd->variables.sql_mode &
                             (MODE_STRICT_TRANS_TABLES |
                              MODE_STRICT_ALL_TABLES)));

    if (ex->filetype == FILETYPE_XML) /* load xml */
      error= read_xml_field(thd, info, table_list, fields_vars,
                            set_fields, set_values, read_info,
                            *(ex->line_term), skip_lines, ignore);
    else if (!field_term->length() && !enclosed->length())
      error= read_fixed_length(thd, info, table_list, fields_vars,
                               set_fields, set_values, read_info,
			       skip_lines, ignore);
    else
      error= read_sep_field(thd, info, table_list, fields_vars,
                            set_fields, set_values, read_info,
			    *enclosed, skip_lines, ignore);
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
        table->file->ha_end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno, MYF(0));
      error= 1;
    }
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    table->next_number_field=0;
  }
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  free_blobs(table);				/* if pack_blob was used */
  table->copy_blobs=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  /* 
     simulated killing in the middle of per-row loop
     must be effective for binlogging
  */
  DBUG_EXECUTE_IF("simulate_kill_bug27571",
                  {
                    error=1;
                    thd->killed= THD::KILL_QUERY;
                  };);

#ifndef EMBEDDED_LIBRARY
  killed_status= (error == 0) ? THD::NOT_KILLED : thd->killed;
#endif

  /*
    We must invalidate the table in query cache before binlog writing and
    ha_autocommit_...
  */
  query_cache_invalidate3(thd, table_list, 0);
  if (error)
  {
    if (read_file_from_client)
      while (!read_info.next_line())
	;

#ifndef EMBEDDED_LIBRARY
    if (mysql_bin_log.is_open())
    {
      {
	/*
	  Make sure last block (the one which caused the error) gets
	  logged.  This is needed because otherwise after write of (to
	  the binlog, not to read_info (which is a cache))
	  Delete_file_log_event the bad block will remain in read_info
	  (because pre_read is not called at the end of the last
	  block; remember pre_read is called whenever a new block is
	  read from disk).  At the end of mysql_load(), the destructor
	  of read_info will call end_io_cache() which will flush
	  read_info, so we will finally have this in the binlog:

	  Append_block # The last successfull block
	  Delete_file
	  Append_block # The failing block
	  which is nonsense.
	  Or could also be (for a small file)
	  Create_file  # The failing block
	  which is nonsense (Delete_file is not written in this case, because:
	  Create_file has not been written, so Delete_file is not written, then
	  when read_info is destroyed end_io_cache() is called which writes
	  Create_file.
	*/
	read_info.end_io_cache();
	/* If the file was not empty, wrote_create_file is true */
	if (lf_info.wrote_create_file)
	{
          int errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

          /* since there is already an error, the possible error of
             writing binary log will be ignored */
	  if (thd->transaction.stmt.modified_non_trans_table)
            (void) write_execute_load_query_log_event(thd, ex,
                                                      table_list->db, 
                                                      table_list->table_name,
                                                      is_concurrent,
                                                      handle_duplicates, ignore,
                                                      transactional_table,
                                                      errcode);
	  else
	  {
	    Delete_file_log_event d(thd, db, transactional_table);
	    (void) mysql_bin_log.write(&d);
	  }
	}
      }
    }
#endif /*!EMBEDDED_LIBRARY*/
    error= -1;				// Error on read
    goto err;
  }
  sprintf(name, ER(ER_LOAD_INFO), (ulong) info.records, (ulong) info.deleted,
	  (ulong) (info.records - info.copied),
          (ulong) thd->warning_info->statement_warn_count());

  if (thd->transaction.stmt.modified_non_trans_table)
    thd->transaction.all.modified_non_trans_table= TRUE;
#ifndef EMBEDDED_LIBRARY
  if (mysql_bin_log.is_open())
  {
    /*
      We need to do the job that is normally done inside
      binlog_query() here, which is to ensure that the pending event
      is written before tables are unlocked and before any other
      events are written.  We also need to update the table map
      version for the binary log to mark that table maps are invalid
      after this point.
     */
    if (thd->is_current_stmt_binlog_format_row())
      error= thd->binlog_flush_pending_rows_event(TRUE, transactional_table);
    else
    {
      /*
        As already explained above, we need to call end_io_cache() or the last
        block will be logged only after Execute_load_query_log_event (which is
        wrong), when read_info is destroyed.
      */
      read_info.end_io_cache();
      if (lf_info.wrote_create_file)
      {
        int errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
        error= write_execute_load_query_log_event(thd, ex,
                                                  table_list->db, table_list->table_name,
                                                  is_concurrent,
                                                  handle_duplicates, ignore,
                                                  transactional_table,
                                                  errcode);
      }

      /*
        Flushing the IO CACHE while writing the execute load query log event
        may result in error (for instance, because the max_binlog_size has been 
        reached, and rotation of the binary log failed).
      */
      error= error || mysql_bin_log.get_log_file()->error;
    }
    if (error)
      goto err;
  }
#endif /*!EMBEDDED_LIBRARY*/

  /* ok to client sent only after binlog write and engine commit */
  my_ok(thd, info.copied + info.deleted, 0L, name);
err:
  DBUG_ASSERT(transactional_table || !(info.copied || info.deleted) ||
              thd->transaction.stmt.modified_non_trans_table);
  table->file->ha_release_auto_increment();
  table->auto_increment_field_not_null= FALSE;
  thd->abort_on_warning= 0;
  DBUG_RETURN(error);
}


#ifndef EMBEDDED_LIBRARY

/* Not a very useful function; just to avoid duplication of code */
static bool write_execute_load_query_log_event(THD *thd, sql_exchange* ex,
                                               const char* db_arg,  /* table's database */
                                               const char* table_name_arg,
                                               bool is_concurrent,
                                               enum enum_duplicates duplicates,
                                               bool ignore,
                                               bool transactional_table,
                                               int errcode)
{
  char                *load_data_query,
                      *end,
                      *fname_start,
                      *fname_end,
                      *p= NULL;
  size_t               pl= 0;
  List<Item>           fv;
  Item                *item;
  String              *str;
  String               pfield, pfields;
  int                  n;
  const char          *tbl= table_name_arg;
  const char          *tdb= (thd->db != NULL ? thd->db : db_arg);
  String              string_buf;
  if (!thd->db || strcmp(db_arg, thd->db))
  {
    /*
      If used database differs from table's database,
      prefix table name with database name so that it
      becomes a FQ name.
     */
    string_buf.set_charset(system_charset_info);
    append_identifier(thd, &string_buf, db_arg, strlen(db_arg));
    string_buf.append(".");
  }
  append_identifier(thd, &string_buf, table_name_arg,
                    strlen(table_name_arg));
  tbl= string_buf.c_ptr_safe();
  Load_log_event       lle(thd, ex, tdb, tbl, fv, is_concurrent,
                           duplicates, ignore, transactional_table);

  /*
    force in a LOCAL if there was one in the original.
  */
  if (thd->lex->local_file)
    lle.set_fname_outside_temp_buf(ex->file_name, strlen(ex->file_name));

  /*
    prepare fields-list and SET if needed; print_query won't do that for us.
  */
  if (!thd->lex->field_list.is_empty())
  {
    List_iterator<Item>  li(thd->lex->field_list);

    pfields.append(" (");
    n= 0;

    while ((item= li++))
    {
      if (n++)
        pfields.append(", ");
      if (item->type() == Item::FIELD_ITEM)
        append_identifier(thd, &pfields, item->name, strlen(item->name));
      else
        item->print(&pfields, QT_ORDINARY);
    }
    pfields.append(")");
  }

  if (!thd->lex->update_list.is_empty())
  {
    List_iterator<Item> lu(thd->lex->update_list);
    List_iterator<String> ls(thd->lex->load_set_str_list);

    pfields.append(" SET ");
    n= 0;

    while ((item= lu++))
    {
      str= ls++;
      if (n++)
        pfields.append(", ");
      append_identifier(thd, &pfields, item->name, strlen(item->name));
      // Extract exact Item value
      str->copy();
      pfields.append((char *)str->ptr());
      str->free();
    }
    /*
      Clear the SET string list once the SET command is reconstructed
      as we donot require the list anymore.
    */
    thd->lex->load_set_str_list.empty();
  }

  p= pfields.c_ptr_safe();
  pl= strlen(p);

  if (!(load_data_query= (char *)thd->alloc(lle.get_query_buffer_length() + 1 + pl)))
    return TRUE;

  lle.print_query(FALSE, (const char *) ex->cs?ex->cs->csname:NULL,
                  load_data_query, &end,
                  (char **)&fname_start, (char **)&fname_end);

  strcpy(end, p);
  end += pl;

  Execute_load_query_log_event
    e(thd, load_data_query, end-load_data_query,
      (uint) ((char*) fname_start - load_data_query - 1),
      (uint) ((char*) fname_end - load_data_query),
      (duplicates == DUP_REPLACE) ? LOAD_DUP_REPLACE :
      (ignore ? LOAD_DUP_IGNORE : LOAD_DUP_ERROR),
      transactional_table, FALSE, FALSE, errcode);
  return mysql_bin_log.write(&e);
}

#endif

/****************************************************************************
** Read of rows of fixed size + optional garage + optonal newline
****************************************************************************/

static int
read_fixed_length(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
                  List<Item> &fields_vars, List<Item> &set_fields,
                  List<Item> &set_values, READ_INFO &read_info,
                  ulong skip_lines, bool ignore_check_option_errors)
{
  List_iterator_fast<Item> it(fields_vars);
  Item_field *sql_field;
  TABLE *table= table_list->table;
  bool err;
  DBUG_ENTER("read_fixed_length");

  while (!read_info.read_fixed_length())
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      DBUG_RETURN(1);
    }
    if (skip_lines)
    {
      /*
	We could implement this with a simple seek if:
	- We are not using DATA INFILE LOCAL
	- escape character is  ""
	- line starting prefix is ""
      */
      skip_lines--;
      continue;
    }
    it.rewind();
    uchar *pos=read_info.row_start;
#ifdef HAVE_purify
    read_info.row_end[0]=0;
#endif

    restore_record(table, s->default_values);
    /*
      There is no variables in fields_vars list in this format so
      this conversion is safe.
    */
    while ((sql_field= (Item_field*) it++))
    {
      Field *field= sql_field->field;                  
      if (field == table->next_number_field)
        table->auto_increment_field_not_null= TRUE;
      /*
        No fields specified in fields_vars list can be null in this format.
        Mark field as not null, we should do this for each row because of
        restore_record...
      */
      field->set_notnull();

      if (pos == read_info.row_end)
      {
        thd->cuted_fields++;			/* Not enough fields */
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_WARN_TOO_FEW_RECORDS,
                            ER(ER_WARN_TOO_FEW_RECORDS),
                            thd->warning_info->current_row_for_warning());
        if (!field->maybe_null() && field->type() == FIELD_TYPE_TIMESTAMP)
            ((Field_timestamp*) field)->set_time();
      }
      else
      {
	uint length;
	uchar save_chr;
	if ((length=(uint) (read_info.row_end-pos)) >
	    field->field_length)
	  length=field->field_length;
	save_chr=pos[length]; pos[length]='\0'; // Safeguard aganst malloc
        field->store((char*) pos,length,read_info.read_charset);
	pos[length]=save_chr;
	if ((pos+=length) > read_info.row_end)
	  pos= read_info.row_end;	/* Fills rest with space */
      }
    }
    if (pos != read_info.row_end)
    {
      thd->cuted_fields++;			/* To long row */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER(ER_WARN_TOO_MANY_RECORDS),
                          thd->warning_info->current_row_for_warning());
    }

    if (thd->killed ||
        fill_record_n_invoke_before_triggers(thd, set_fields, set_values,
                                             ignore_check_option_errors,
                                             table->triggers,
                                             TRG_EVENT_INSERT))
      DBUG_RETURN(1);

    switch (table_list->view_check_option(thd,
                                          ignore_check_option_errors)) {
    case VIEW_CHECK_SKIP:
      read_info.next_line();
      goto continue_loop;
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(-1);
    }

    err= write_record(thd, table, &info);
    table->auto_increment_field_not_null= FALSE;
    if (err)
      DBUG_RETURN(1);
   
    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      thd->cuted_fields++;			/* To long row */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER(ER_WARN_TOO_MANY_RECORDS),
                          thd->warning_info->current_row_for_warning());
    }
    thd->warning_info->inc_current_row_for_warning();
continue_loop:;
  }
  DBUG_RETURN(test(read_info.error));
}



static int
read_sep_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
               List<Item> &fields_vars, List<Item> &set_fields,
               List<Item> &set_values, READ_INFO &read_info,
	       String &enclosed, ulong skip_lines,
	       bool ignore_check_option_errors)
{
  List_iterator_fast<Item> it(fields_vars);
  Item *item;
  TABLE *table= table_list->table;
  uint enclosed_length;
  bool err;
  DBUG_ENTER("read_sep_field");

  enclosed_length=enclosed.length();

  for (;;it.rewind())
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      DBUG_RETURN(1);
    }

    restore_record(table, s->default_values);

    while ((item= it++))
    {
      uint length;
      uchar *pos;
      Item *real_item;

      if (read_info.read_field())
	break;

      /* If this line is to be skipped we don't want to fill field or var */
      if (skip_lines)
        continue;

      pos=read_info.row_start;
      length=(uint) (read_info.row_end-pos);

      real_item= item->real_item();

      if ((!read_info.enclosed &&
	  (enclosed_length && length == 4 &&
           !memcmp(pos, STRING_WITH_LEN("NULL")))) ||
	  (length == 1 && read_info.found_null))
      {

        if (real_item->type() == Item::FIELD_ITEM)
        {
          Field *field= ((Item_field *)real_item)->field;
          if (field->reset())
          {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0), field->field_name,
                     thd->warning_info->current_row_for_warning());
            DBUG_RETURN(1);
          }
          field->set_null();
          if (!field->maybe_null())
          {
            if (field->type() == MYSQL_TYPE_TIMESTAMP)
              ((Field_timestamp*) field)->set_time();
            else if (field != table->next_number_field)
              field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                 ER_WARN_NULL_TO_NOTNULL, 1);
          }
	}
        else if (item->type() == Item::STRING_ITEM)
        {
          ((Item_user_var_as_out_param *)item)->set_null_value(
                                                  read_info.read_charset);
        }
        else
        {
          my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
          DBUG_RETURN(1);
        }

	continue;
      }

      if (real_item->type() == Item::FIELD_ITEM)
      {
        Field *field= ((Item_field *)real_item)->field;
        field->set_notnull();
        read_info.row_end[0]=0;			// Safe to change end marker
        if (field == table->next_number_field)
          table->auto_increment_field_not_null= TRUE;
        field->store((char*) pos, length, read_info.read_charset);
      }
      else if (item->type() == Item::STRING_ITEM)
      {
        ((Item_user_var_as_out_param *)item)->set_value((char*) pos, length,
                                                        read_info.read_charset);
      }
      else
      {
        my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
        DBUG_RETURN(1);
      }
    }

    if (thd->is_error())
      read_info.error= 1;

    if (read_info.error)
      break;
    if (skip_lines)
    {
      skip_lines--;
      continue;
    }
    if (item)
    {
      /* Have not read any field, thus input file is simply ended */
      if (item == fields_vars.head())
	break;
      for (; item ; item= it++)
      {
        Item *real_item= item->real_item();
        if (real_item->type() == Item::FIELD_ITEM)
        {
          Field *field= ((Item_field *)real_item)->field;
          if (field->reset())
          {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0),field->field_name,
                     thd->warning_info->current_row_for_warning());
            DBUG_RETURN(1);
          }
          if (!field->maybe_null() && field->type() == FIELD_TYPE_TIMESTAMP)
              ((Field_timestamp*) field)->set_time();
          /*
            QQ: We probably should not throw warning for each field.
            But how about intention to always have the same number
            of warnings in THD::cuted_fields (and get rid of cuted_fields
            in the end ?)
          */
          thd->cuted_fields++;
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_WARN_TOO_FEW_RECORDS,
                              ER(ER_WARN_TOO_FEW_RECORDS),
                              thd->warning_info->current_row_for_warning());
        }
        else if (item->type() == Item::STRING_ITEM)
        {
          ((Item_user_var_as_out_param *)item)->set_null_value(
                                                  read_info.read_charset);
        }
        else
        {
          my_error(ER_LOAD_DATA_INVALID_COLUMN, MYF(0), item->full_name());
          DBUG_RETURN(1);
        }
      }
    }

    if (thd->killed ||
        fill_record_n_invoke_before_triggers(thd, set_fields, set_values,
                                             ignore_check_option_errors,
                                             table->triggers,
                                             TRG_EVENT_INSERT))
      DBUG_RETURN(1);

    switch (table_list->view_check_option(thd,
                                          ignore_check_option_errors)) {
    case VIEW_CHECK_SKIP:
      read_info.next_line();
      goto continue_loop;
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(-1);
    }

    err= write_record(thd, table, &info);
    table->auto_increment_field_not_null= FALSE;
    if (err)
      DBUG_RETURN(1);
    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      thd->cuted_fields++;			/* To long row */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_TOO_MANY_RECORDS, ER(ER_WARN_TOO_MANY_RECORDS),
                          thd->warning_info->current_row_for_warning());
      if (thd->killed)
        DBUG_RETURN(1);
    }
    thd->warning_info->inc_current_row_for_warning();
continue_loop:;
  }
  DBUG_RETURN(test(read_info.error));
}


/****************************************************************************
** Read rows in xml format
****************************************************************************/
static int
read_xml_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
               List<Item> &fields_vars, List<Item> &set_fields,
               List<Item> &set_values, READ_INFO &read_info,
               String &row_tag, ulong skip_lines,
               bool ignore_check_option_errors)
{
  List_iterator_fast<Item> it(fields_vars);
  Item *item;
  TABLE *table= table_list->table;
  bool no_trans_update_stmt;
  CHARSET_INFO *cs= read_info.read_charset;
  DBUG_ENTER("read_xml_field");
  
  no_trans_update_stmt= !table->file->has_transactions();
  
  for ( ; ; it.rewind())
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      DBUG_RETURN(1);
    }
    
    // read row tag and save values into tag list
    if (read_info.read_xml())
      break;
    
    List_iterator_fast<XML_TAG> xmlit(read_info.taglist);
    xmlit.rewind();
    XML_TAG *tag= NULL;
    
#ifndef DBUG_OFF
    DBUG_PRINT("read_xml_field", ("skip_lines=%d", (int) skip_lines));
    while ((tag= xmlit++))
    {
      DBUG_PRINT("read_xml_field", ("got tag:%i '%s' '%s'",
                                    tag->level, tag->field.c_ptr(),
                                    tag->value.c_ptr()));
    }
#endif
    
    restore_record(table, s->default_values);
    
    while ((item= it++))
    {
      /* If this line is to be skipped we don't want to fill field or var */
      if (skip_lines)
        continue;
      
      /* find field in tag list */
      xmlit.rewind();
      tag= xmlit++;
      
      while(tag && strcmp(tag->field.c_ptr(), item->name) != 0)
        tag= xmlit++;
      
      if (!tag) // found null
      {
        if (item->type() == Item::FIELD_ITEM)
        {
          Field *field= ((Item_field *) item)->field;
          field->reset();
          field->set_null();
          if (field == table->next_number_field)
            table->auto_increment_field_not_null= TRUE;
          if (!field->maybe_null())
          {
            if (field->type() == FIELD_TYPE_TIMESTAMP)
              ((Field_timestamp *) field)->set_time();
            else if (field != table->next_number_field)
              field->set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                                 ER_WARN_NULL_TO_NOTNULL, 1);
          }
        }
        else
          ((Item_user_var_as_out_param *) item)->set_null_value(cs);
        continue;
      }

      if (item->type() == Item::FIELD_ITEM)
      {

        Field *field= ((Item_field *)item)->field;
        field->set_notnull();
        if (field == table->next_number_field)
          table->auto_increment_field_not_null= TRUE;
        field->store((char *) tag->value.ptr(), tag->value.length(), cs);
      }
      else
        ((Item_user_var_as_out_param *) item)->set_value(
                                                 (char *) tag->value.ptr(), 
                                                 tag->value.length(), cs);
    }
    
    if (read_info.error)
      break;
    
    if (skip_lines)
    {
      skip_lines--;
      continue;
    }
    
    if (item)
    {
      /* Have not read any field, thus input file is simply ended */
      if (item == fields_vars.head())
        break;
      
      for ( ; item; item= it++)
      {
        if (item->type() == Item::FIELD_ITEM)
        {
          /*
            QQ: We probably should not throw warning for each field.
            But how about intention to always have the same number
            of warnings in THD::cuted_fields (and get rid of cuted_fields
            in the end ?)
          */
          thd->cuted_fields++;
          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_WARN_TOO_FEW_RECORDS,
                              ER(ER_WARN_TOO_FEW_RECORDS),
                              thd->warning_info->current_row_for_warning());
        }
        else
          ((Item_user_var_as_out_param *)item)->set_null_value(cs);
      }
    }

    if (thd->killed ||
        fill_record_n_invoke_before_triggers(thd, set_fields, set_values,
                                             ignore_check_option_errors,
                                             table->triggers,
                                             TRG_EVENT_INSERT))
      DBUG_RETURN(1);

    switch (table_list->view_check_option(thd,
                                          ignore_check_option_errors)) {
    case VIEW_CHECK_SKIP:
      read_info.next_line();
      goto continue_loop;
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(-1);
    }
    
    if (write_record(thd, table, &info))
      DBUG_RETURN(1);
    
    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    thd->transaction.stmt.modified_non_trans_table= no_trans_update_stmt;
    thd->warning_info->inc_current_row_for_warning();
    continue_loop:;
  }
  DBUG_RETURN(test(read_info.error) || thd->is_error());
} /* load xml end */


/* Unescape all escape characters, mark \N as null */

char
READ_INFO::unescape(char chr)
{
  /* keep this switch synchornous with the ESCAPE_CHARS macro */
  switch(chr) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'r': return '\r';
  case 'b': return '\b';
  case '0': return 0;				// Ascii null
  case 'Z': return '\032';			// Win32 end of file
  case 'N': found_null=1;

    /* fall through */
  default:  return chr;
  }
}


/*
  Read a line using buffering
  If last line is empty (in line mode) then it isn't outputed
*/


READ_INFO::READ_INFO(File file_par, uint tot_length, CHARSET_INFO *cs,
		     String &field_term, String &line_start, String &line_term,
		     String &enclosed_par, int escape, bool get_it_from_net,
		     bool is_fifo)
  :file(file_par), buff_length(tot_length), escape_char(escape),
   found_end_of_line(false), eof(false), need_end_io_cache(false),
   error(false), line_cuted(false), found_null(false), read_charset(cs)
{
  field_term_ptr=(char*) field_term.ptr();
  field_term_length= field_term.length();
  line_term_ptr=(char*) line_term.ptr();
  line_term_length= line_term.length();
  level= 0; /* for load xml */
  if (line_start.length() == 0)
  {
    line_start_ptr=0;
    start_of_line= 0;
  }
  else
  {
    line_start_ptr=(char*) line_start.ptr();
    line_start_end=line_start_ptr+line_start.length();
    start_of_line= 1;
  }
  /* If field_terminator == line_terminator, don't use line_terminator */
  if (field_term_length == line_term_length &&
      !memcmp(field_term_ptr,line_term_ptr,field_term_length))
  {
    line_term_length=0;
    line_term_ptr=(char*) "";
  }
  enclosed_char= (enclosed_length=enclosed_par.length()) ?
    (uchar) enclosed_par[0] : INT_MAX;
  field_term_char= field_term_length ? (uchar) field_term_ptr[0] : INT_MAX;
  line_term_char= line_term_length ? (uchar) line_term_ptr[0] : INT_MAX;


  /* Set of a stack for unget if long terminators */
  uint length= max(cs->mbmaxlen, max(field_term_length, line_term_length)) + 1;
  set_if_bigger(length,line_start.length());
  stack=stack_pos=(int*) sql_alloc(sizeof(int)*length);

  if (!(buffer=(uchar*) my_malloc(buff_length+1,MYF(0))))
    error=1; /* purecov: inspected */
  else
  {
    end_of_buff=buffer+buff_length;
    if (init_io_cache(&cache,(get_it_from_net) ? -1 : file, 0,
		      (get_it_from_net) ? READ_NET :
		      (is_fifo ? READ_FIFO : READ_CACHE),0L,1,
		      MYF(MY_WME)))
    {
      my_free(buffer); /* purecov: inspected */
      buffer= NULL;
      error=1;
    }
    else
    {
      /*
	init_io_cache() will not initialize read_function member
	if the cache is READ_NET. So we work around the problem with a
	manual assignment
      */
      need_end_io_cache = 1;

#ifndef EMBEDDED_LIBRARY
      if (get_it_from_net)
	cache.read_function = _my_b_net_read;

      if (mysql_bin_log.is_open())
	cache.pre_read = cache.pre_close =
	  (IO_CACHE_CALLBACK) log_loaded_block;
#endif
    }
  }
}


READ_INFO::~READ_INFO()
{
  if (need_end_io_cache)
    ::end_io_cache(&cache);

  if (buffer != NULL)
    my_free(buffer);
  List_iterator<XML_TAG> xmlit(taglist);
  XML_TAG *t;
  while ((t= xmlit++))
    delete(t);
}


#define GET (stack_pos != stack ? *--stack_pos : my_b_get(&cache))
#define PUSH(A) *(stack_pos++)=(A)


inline int READ_INFO::terminator(char *ptr,uint length)
{
  int chr=0;					// Keep gcc happy
  uint i;
  for (i=1 ; i < length ; i++)
  {
    if ((chr=GET) != *++ptr)
    {
      break;
    }
  }
  if (i == length)
    return 1;
  PUSH(chr);
  while (i-- > 1)
    PUSH((uchar) *--ptr);
  return 0;
}


int READ_INFO::read_field()
{
  int chr,found_enclosed_char;
  uchar *to,*new_buffer;

  found_null=0;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  /* Skip until we find 'line_start' */

  if (start_of_line)
  {						// Skip until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }
  if ((chr=GET) == my_b_EOF)
  {
    found_end_of_line=eof=1;
    return 1;
  }
  to=buffer;
  if (chr == enclosed_char)
  {
    found_enclosed_char=enclosed_char;
    *to++=(uchar) chr;				// If error
  }
  else
  {
    found_enclosed_char= INT_MAX;
    PUSH(chr);
  }

  for (;;)
  {
    while ( to < end_of_buff)
    {
      chr = GET;
      if (chr == my_b_EOF)
	goto found_eof;
      if (chr == escape_char)
      {
	if ((chr=GET) == my_b_EOF)
	{
	  *to++= (uchar) escape_char;
	  goto found_eof;
	}
        /*
          When escape_char == enclosed_char, we treat it like we do for
          handling quotes in SQL parsing -- you can double-up the
          escape_char to include it literally, but it doesn't do escapes
          like \n. This allows: LOAD DATA ... ENCLOSED BY '"' ESCAPED BY '"'
          with data like: "fie""ld1", "field2"
         */
        if (escape_char != enclosed_char || chr == escape_char)
        {
          *to++ = (uchar) unescape((char) chr);
          continue;
        }
        PUSH(chr);
        chr= escape_char;
      }
#ifdef ALLOW_LINESEPARATOR_IN_STRINGS
      if (chr == line_term_char)
#else
      if (chr == line_term_char && found_enclosed_char == INT_MAX)
#endif
      {
	if (terminator(line_term_ptr,line_term_length))
	{					// Maybe unexpected linefeed
	  enclosed=0;
	  found_end_of_line=1;
	  row_start=buffer;
	  row_end=  to;
	  return 0;
	}
      }
      if (chr == found_enclosed_char)
      {
	if ((chr=GET) == found_enclosed_char)
	{					// Remove dupplicated
	  *to++ = (uchar) chr;
	  continue;
	}
	// End of enclosed field if followed by field_term or line_term
	if (chr == my_b_EOF ||
	    (chr == line_term_char && terminator(line_term_ptr,
						line_term_length)))
	{					// Maybe unexpected linefeed
	  enclosed=1;
	  found_end_of_line=1;
	  row_start=buffer+1;
	  row_end=  to;
	  return 0;
	}
	if (chr == field_term_char &&
	    terminator(field_term_ptr,field_term_length))
	{
	  enclosed=1;
	  row_start=buffer+1;
	  row_end=  to;
	  return 0;
	}
	/*
	  The string didn't terminate yet.
	  Store back next character for the loop
	*/
	PUSH(chr);
	/* copy the found term character to 'to' */
	chr= found_enclosed_char;
      }
      else if (chr == field_term_char && found_enclosed_char == INT_MAX)
      {
	if (terminator(field_term_ptr,field_term_length))
	{
	  enclosed=0;
	  row_start=buffer;
	  row_end=  to;
	  return 0;
	}
      }
#ifdef USE_MB
      if (my_mbcharlen(read_charset, chr) > 1 &&
          to + my_mbcharlen(read_charset, chr) <= end_of_buff)
      {
        uchar* p= (uchar*) to;
        int ml, i;
        *to++ = chr;

        ml= my_mbcharlen(read_charset, chr);

        for (i= 1; i < ml; i++) 
        {
          chr= GET;
          if (chr == my_b_EOF)
          {
            /*
             Need to back up the bytes already ready from illformed
             multi-byte char 
            */
            to-= i;
            goto found_eof;
          }
          *to++ = chr;
        }
        if (my_ismbchar(read_charset,
                        (const char *)p,
                        (const char *)to))
          continue;
        for (i= 0; i < ml; i++)
          PUSH((uchar) *--to);
        chr= GET;
      }
#endif
      *to++ = (uchar) chr;
    }
    /*
    ** We come here if buffer is too small. Enlarge it and continue
    */
    if (!(new_buffer=(uchar*) my_realloc((char*) buffer,buff_length+1+IO_SIZE,
					MYF(MY_WME))))
      return (error=1);
    to=new_buffer + (to-buffer);
    buffer=new_buffer;
    buff_length+=IO_SIZE;
    end_of_buff=buffer+buff_length;
  }

found_eof:
  enclosed=0;
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return 0;
}

/*
  Read a row with fixed length.

  NOTES
    The row may not be fixed size on disk if there are escape
    characters in the file.

  IMPLEMENTATION NOTE
    One can't use fixed length with multi-byte charset **

  RETURN
    0  ok
    1  error
*/

int READ_INFO::read_fixed_length()
{
  int chr;
  uchar *to;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  if (start_of_line)
  {						// Skip until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }

  to=row_start=buffer;
  while (to < end_of_buff)
  {
    if ((chr=GET) == my_b_EOF)
      goto found_eof;
    if (chr == escape_char)
    {
      if ((chr=GET) == my_b_EOF)
      {
	*to++= (uchar) escape_char;
	goto found_eof;
      }
      *to++ =(uchar) unescape((char) chr);
      continue;
    }
    if (chr == line_term_char)
    {
      if (terminator(line_term_ptr,line_term_length))
      {						// Maybe unexpected linefeed
	found_end_of_line=1;
	row_end=  to;
	return 0;
      }
    }
    *to++ = (uchar) chr;
  }
  row_end=to;					// Found full line
  return 0;

found_eof:
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return to == buffer ? 1 : 0;
}


int READ_INFO::next_line()
{
  line_cuted=0;
  start_of_line= line_start_ptr != 0;
  if (found_end_of_line || eof)
  {
    found_end_of_line=0;
    return eof;
  }
  found_end_of_line=0;
  if (!line_term_length)
    return 0;					// No lines
  for (;;)
  {
    int chr = GET;
#ifdef USE_MB
   if (my_mbcharlen(read_charset, chr) > 1)
   {
       for (uint i=1;
            chr != my_b_EOF && i<my_mbcharlen(read_charset, chr);
            i++)
	   chr = GET;
       if (chr == escape_char)
	   continue;
   }
#endif
   if (chr == my_b_EOF)
   {
      eof=1;
      return 1;
    }
    if (chr == escape_char)
    {
      line_cuted=1;
      if (GET == my_b_EOF)
	return 1;
      continue;
    }
    if (chr == line_term_char && terminator(line_term_ptr,line_term_length))
      return 0;
    line_cuted=1;
  }
}


bool READ_INFO::find_start_of_fields()
{
  int chr;
 try_again:
  do
  {
    if ((chr=GET) == my_b_EOF)
    {
      found_end_of_line=eof=1;
      return 1;
    }
  } while ((char) chr != line_start_ptr[0]);
  for (char *ptr=line_start_ptr+1 ; ptr != line_start_end ; ptr++)
  {
    chr=GET;					// Eof will be checked later
    if ((char) chr != *ptr)
    {						// Can't be line_start
      PUSH(chr);
      while (--ptr != line_start_ptr)
      {						// Restart with next char
	PUSH((uchar) *ptr);
      }
      goto try_again;
    }
  }
  return 0;
}


/*
  Clear taglist from tags with a specified level
*/
int READ_INFO::clear_level(int level_arg)
{
  DBUG_ENTER("READ_INFO::read_xml clear_level");
  List_iterator<XML_TAG> xmlit(taglist);
  xmlit.rewind();
  XML_TAG *tag;
  
  while ((tag= xmlit++))
  {
     if(tag->level >= level_arg)
     {
       xmlit.remove();
       delete tag;
     }
  }
  DBUG_RETURN(0);
}


/*
  Convert an XML entity to Unicode value.
  Return -1 on error;
*/
static int
my_xml_entity_to_char(const char *name, uint length)
{
  if (length == 2)
  {
    if (!memcmp(name, "gt", length))
      return '>';
    if (!memcmp(name, "lt", length))
      return '<';
  }
  else if (length == 3)
  {
    if (!memcmp(name, "amp", length))
      return '&';
  }
  else if (length == 4)
  {
    if (!memcmp(name, "quot", length))
      return '"';
    if (!memcmp(name, "apos", length))
      return '\'';
  }
  return -1;
}


/**
  @brief Convert newline, linefeed, tab to space
  
  @param chr    character
  
  @details According to the "XML 1.0" standard,
           only space (#x20) characters, carriage returns,
           line feeds or tabs are considered as spaces.
           Convert all of them to space (#x20) for parsing simplicity.
*/
static int
my_tospace(int chr)
{
  return (chr == '\t' || chr == '\r' || chr == '\n') ? ' ' : chr;
}


/*
  Read an xml value: handle multibyte and xml escape
*/
int READ_INFO::read_value(int delim, String *val)
{
  int chr;
  String tmp;

  for (chr= GET; my_tospace(chr) != delim && chr != my_b_EOF;)
  {
#ifdef USE_MB
    if (my_mbcharlen(read_charset, chr) > 1)
    {
      DBUG_PRINT("read_xml",("multi byte"));
      int i, ml= my_mbcharlen(read_charset, chr);
      for (i= 1; i < ml; i++) 
      {
        val->append(chr);
        /*
          Don't use my_tospace() in the middle of a multi-byte character
          TODO: check that the multi-byte sequence is valid.
        */
        chr= GET; 
        if (chr == my_b_EOF)
          return chr;
      }
    }
#endif
    if(chr == '&')
    {
      tmp.length(0);
      for (chr= my_tospace(GET) ; chr != ';' ; chr= my_tospace(GET))
      {
        if (chr == my_b_EOF)
          return chr;
        tmp.append(chr);
      }
      if ((chr= my_xml_entity_to_char(tmp.ptr(), tmp.length())) >= 0)
        val->append(chr);
      else
      {
        val->append('&');
        val->append(tmp);
        val->append(';'); 
      }
    }
    else
      val->append(chr);
    chr= GET;
  }            
  return my_tospace(chr);
}


/*
  Read a record in xml format
  tags and attributes are stored in taglist
  when tag set in ROWS IDENTIFIED BY is closed, we are ready and return
*/
int READ_INFO::read_xml()
{
  DBUG_ENTER("READ_INFO::read_xml");
  int chr, chr2, chr3;
  int delim= 0;
  String tag, attribute, value;
  bool in_tag= false;
  
  tag.length(0);
  attribute.length(0);
  value.length(0);
  
  for (chr= my_tospace(GET); chr != my_b_EOF ; )
  {
    switch(chr){
    case '<':  /* read tag */
        /* TODO: check if this is a comment <!-- comment -->  */
      chr= my_tospace(GET);
      if(chr == '!')
      {
        chr2= GET;
        chr3= GET;
        
        if(chr2 == '-' && chr3 == '-')
        {
          chr2= 0;
          chr3= 0;
          chr= my_tospace(GET);
          
          while(chr != '>' || chr2 != '-' || chr3 != '-')
          {
            if(chr == '-')
            {
              chr3= chr2;
              chr2= chr;
            }
            else if (chr2 == '-')
            {
              chr2= 0;
              chr3= 0;
            }
            chr= my_tospace(GET);
            if (chr == my_b_EOF)
              goto found_eof;
          }
          break;
        }
      }
      
      tag.length(0);
      while(chr != '>' && chr != ' ' && chr != '/' && chr != my_b_EOF)
      {
        if(chr != delim) /* fix for the '<field name =' format */
          tag.append(chr);
        chr= my_tospace(GET);
      }
      
      // row tag should be in ROWS IDENTIFIED BY '<row>' - stored in line_term 
      if((tag.length() == line_term_length -2) &&
         (strncmp(tag.c_ptr_safe(), line_term_ptr + 1, tag.length()) == 0))
      {
        DBUG_PRINT("read_xml", ("start-of-row: %i %s %s", 
                                level,tag.c_ptr_safe(), line_term_ptr));
      }
      
      if(chr == ' ' || chr == '>')
      {
        level++;
        clear_level(level + 1);
      }
      
      if (chr == ' ')
        in_tag= true;
      else 
        in_tag= false;
      break;
      
    case ' ': /* read attribute */
      while(chr == ' ')  /* skip blanks */
        chr= my_tospace(GET);
      
      if(!in_tag)
        break;
      
      while(chr != '=' && chr != '/' && chr != '>' && chr != my_b_EOF)
      {
        attribute.append(chr);
        chr= my_tospace(GET);
      }
      break;
      
    case '>': /* end tag - read tag value */
      in_tag= false;
      chr= read_value('<', &value);
      if(chr == my_b_EOF)
        goto found_eof;
      
      /* save value to list */
      if(tag.length() > 0 && value.length() > 0)
      {
        DBUG_PRINT("read_xml", ("lev:%i tag:%s val:%s",
                                level,tag.c_ptr_safe(), value.c_ptr_safe()));
        taglist.push_front( new XML_TAG(level, tag, value));
      }
      tag.length(0);
      value.length(0);
      attribute.length(0);
      break;
      
    case '/': /* close tag */
      level--;
      chr= my_tospace(GET);
      if(chr != '>')   /* if this is an empty tag <tag   /> */
        tag.length(0); /* we should keep tag value          */
      while(chr != '>' && chr != my_b_EOF)
      {
        tag.append(chr);
        chr= my_tospace(GET);
      }
      
      if((tag.length() == line_term_length -2) &&
         (strncmp(tag.c_ptr_safe(), line_term_ptr + 1, tag.length()) == 0))
      {
         DBUG_PRINT("read_xml", ("found end-of-row %i %s", 
                                 level, tag.c_ptr_safe()));
         DBUG_RETURN(0); //normal return
      }
      chr= my_tospace(GET);
      break;   
      
    case '=': /* attribute name end - read the value */
      //check for tag field and attribute name
      if(!memcmp(tag.c_ptr_safe(), STRING_WITH_LEN("field")) &&
         !memcmp(attribute.c_ptr_safe(), STRING_WITH_LEN("name")))
      {
        /*
          this is format <field name="xx">xx</field>
          where actual fieldname is in attribute
        */
        delim= my_tospace(GET);
        tag.length(0);
        attribute.length(0);
        chr= '<'; /* we pretend that it is a tag */
        level--;
        break;
      }
      
      //check for " or '
      chr= GET;
      if (chr == my_b_EOF)
        goto found_eof;
      if(chr == '"' || chr == '\'')
      {
        delim= chr;
      }
      else
      {
        delim= ' '; /* no delimiter, use space */
        PUSH(chr);
      }
      
      chr= read_value(delim, &value);
      if(attribute.length() > 0 && value.length() > 0)
      {
        DBUG_PRINT("read_xml", ("lev:%i att:%s val:%s\n",
                                level + 1,
                                attribute.c_ptr_safe(),
                                value.c_ptr_safe()));
        taglist.push_front(new XML_TAG(level + 1, attribute, value));
      }
      attribute.length(0);
      value.length(0);
      if (chr != ' ')
        chr= my_tospace(GET);
      break;
    
    default:
      chr= my_tospace(GET);
    } /* end switch */
  } /* end while */
  
found_eof:
  DBUG_PRINT("read_xml",("Found eof"));
  eof= 1;
  DBUG_RETURN(1);
}
