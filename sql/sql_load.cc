/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Copy data from a textfile to table */

#include "mysql_priv.h"
#include <my_dir.h>
#include <m_ctype.h>
#include "sql_repl.h"

class READ_INFO {
  File	file;
  byte	*buffer,			/* Buffer for read text */
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

public:
  bool error,line_cuted,found_null,enclosed;
  byte	*row_start,			/* Found row starts here */
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
			     List<Item> &fields, READ_INFO &read_info,
			     ulong skip_lines,
			     bool ignore_check_option_errors);
static int read_sep_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
			  List<Item> &fields, READ_INFO &read_info,
			  String &enclosed, ulong skip_lines,
			  bool ignore_check_option_errors);

int mysql_load(THD *thd,sql_exchange *ex,TABLE_LIST *table_list,
	       List<Item> &fields, enum enum_duplicates handle_duplicates,
	       bool read_file_from_client,thr_lock_type lock_type,
	       bool ignore_check_option_errors)
{
  char name[FN_REFLEN];
  File file;
  TABLE *table;
  int error;
  String *field_term=ex->field_term,*escaped=ex->escaped;
  String *enclosed=ex->enclosed;
  Item *unused_conds;
  bool is_fifo=0;
#ifndef EMBEDDED_LIBRARY
  LOAD_FILE_INFO lf_info;
#endif
  char *db = table_list->db;			// This is never null
  /*
    If path for file is not defined, we will use the current database.
    If this is not set, we will use the directory where the table to be
    loaded is located
  */
  char *tdb= thd->db ? thd->db : db;		// Result is never null
  ulong skip_lines= ex->skip_lines;
  int res;
  bool transactional_table, log_delayed;
  DBUG_ENTER("mysql_load");

#ifdef EMBEDDED_LIBRARY
  read_file_from_client  = 0; //server is always in the same process 
#endif

  if (escaped->length() > 1 || enclosed->length() > 1)
  {
    my_message(ER_WRONG_FIELD_TERMINATORS,ER(ER_WRONG_FIELD_TERMINATORS),
	       MYF(0));
    DBUG_RETURN(-1);
  }
  table_list->lock_type= lock_type;
  if ((res= open_and_lock_tables(thd, table_list)))
    DBUG_RETURN(res);
  if (setup_tables(thd, table_list, &unused_conds))
     DBUG_RETURN(-1);
  if (!table_list->updatable || check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "LOAD");
    DBUG_RETURN(-1);
  }
  table= table_list->table;
  transactional_table= table->file->has_transactions();
  log_delayed= (transactional_table || table->tmp_table);

  if (!fields.elements)
  {
    Field **field;
    for (field=table->field; *field ; field++)
      fields.push_back(new Item_field(*field));
  }
  else
  {						// Part field list
    thd->dupp_field=0;
    /* TODO: use this conds for 'WITH CHECK OPTIONS' */
    Item *unused_conds= 0;
    if (setup_tables(thd, table_list, &unused_conds) ||
	setup_fields(thd, 0, table_list, fields, 1, 0, 0))
      DBUG_RETURN(-1);
    if (thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dupp_field->field_name);
      DBUG_RETURN(-1);
    }
    if (check_that_all_fields_are_given_values(thd, table))
      DBUG_RETURN(1);
  }

  uint tot_length=0;
  bool use_blobs=0,use_timestamp=0;
  List_iterator_fast<Item> it(fields);

  Item_field *field;
  while ((field=(Item_field*) it++))
  {
    if (field->field->flags & BLOB_FLAG)
    {
      use_blobs=1;
      tot_length+=256;				// Will be extended if needed
    }
    else
      tot_length+=field->field->field_length;
    if (!field_term->length() && !(field->field->flags & NOT_NULL_FLAG))
      field->field->set_notnull();
    if (field->field == table->timestamp_field)
      use_timestamp=1;
  }
  if (use_blobs && !ex->line_term->length() && !field_term->length())
  {
    my_message(ER_BLOBS_AND_NO_TERMINATED,ER(ER_BLOBS_AND_NO_TERMINATED),
	       MYF(0));
    DBUG_RETURN(-1);
  }

  /* We can't give an error in the middle when using LOCAL files */
  if (read_file_from_client && handle_duplicates == DUP_ERROR)
    handle_duplicates=DUP_IGNORE;

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
      strxnmov(name, FN_REFLEN, mysql_real_data_home, tdb, NullS);
      (void) fn_format(name, ex->file_name, name, "",
		       MY_RELATIVE_PATH | MY_UNPACK_FILENAME);
    }
    else
    {
      (void) fn_format(name, ex->file_name, mysql_real_data_home, "",
		       MY_RELATIVE_PATH | MY_UNPACK_FILENAME);
#if !defined(__WIN__) && !defined(OS2) && ! defined(__NETWARE__)
      MY_STAT stat_info;
      if (!my_stat(name,&stat_info,MYF(MY_WME)))
	DBUG_RETURN(-1);

      // if we are not in slave thread, the file must be:
      if (!thd->slave_thread &&
	  !((stat_info.st_mode & S_IROTH) == S_IROTH &&  // readable by others
#ifndef __EMX__
	    (stat_info.st_mode & S_IFLNK) != S_IFLNK && // and not a symlink
#endif
	    ((stat_info.st_mode & S_IFREG) == S_IFREG ||
	     (stat_info.st_mode & S_IFIFO) == S_IFIFO)))
      {
	my_error(ER_TEXTFILE_NOT_READABLE,MYF(0),name);
	DBUG_RETURN(-1);
      }
      if ((stat_info.st_mode & S_IFIFO) == S_IFIFO)
	is_fifo = 1;
#endif
    }
    if ((file=my_open(name,O_RDONLY,MYF(MY_WME))) < 0)
      DBUG_RETURN(-1);
  }

  COPY_INFO info;
  bzero((char*) &info,sizeof(info));
  info.handle_duplicates=handle_duplicates;
  info.escape_char=escaped->length() ? (*escaped)[0] : INT_MAX;

  READ_INFO read_info(file,tot_length,thd->variables.collation_database,
		      *field_term,*ex->line_start, *ex->line_term, *enclosed,
		      info.escape_char, read_file_from_client, is_fifo);
  if (read_info.error)
  {
    if	(file >= 0)
      my_close(file,MYF(0));			// no files in net reading
    DBUG_RETURN(-1);				// Can't allocate buffers
  }

#ifndef EMBEDDED_LIBRARY
  if (mysql_bin_log.is_open())
  {
    lf_info.thd = thd;
    lf_info.ex = ex;
    lf_info.db = db;
    lf_info.table_name = table_list->real_name;
    lf_info.fields = &fields;
    lf_info.handle_dup = handle_duplicates;
    lf_info.wrote_create_file = 0;
    lf_info.last_pos_in_file = HA_POS_ERROR;
    lf_info.log_delayed= log_delayed;
    read_info.set_io_cache_arg((void*) &lf_info);
  }
#endif /*!EMBEDDED_LIBRARY*/

  restore_record(table,default_values);

  thd->count_cuted_fields= CHECK_FIELD_WARN;		/* calc cuted fields */
  thd->cuted_fields=0L;
  /* Skip lines if there is a line terminator */
  if (ex->line_term->length())
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
    if (use_timestamp)
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

    table->next_number_field=table->found_next_number_field;
    if (handle_duplicates == DUP_IGNORE ||
	handle_duplicates == DUP_REPLACE)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    ha_enable_transaction(thd, FALSE); 
    table->file->start_bulk_insert((ha_rows) 0);
    table->copy_blobs=1;

    thd->no_trans_update= 0;
    thd->abort_on_warning= (handle_duplicates != DUP_IGNORE &&
                            (thd->variables.sql_mode &
                             (MODE_STRICT_TRANS_TABLES |
                              MODE_STRICT_ALL_TABLES)));

    if (!field_term->length() && !enclosed->length())
      error= read_fixed_length(thd, info, table_list, fields,read_info,
			       skip_lines, ignore_check_option_errors);
    else
      error= read_sep_field(thd, info, table_list, fields, read_info,
			    *enclosed, skip_lines,
			    ignore_check_option_errors);
    if (table->file->end_bulk_insert())
      error=1;					/* purecov: inspected */
    ha_enable_transaction(thd, TRUE);
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->next_number_field=0;
  }
  if (file >= 0)
    my_close(file,MYF(0));
  free_blobs(table);				/* if pack_blob was used */
  table->copy_blobs=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  /*
    We must invalidate the table in query cache before binlog writing and
    ha_autocommit_...
  */
  query_cache_invalidate3(thd, table_list, 0);

  if (error)
  {
    if (transactional_table)
      ha_autocommit_or_rollback(thd,error);

    if (read_file_from_client)
      while (!read_info.next_line())
	;

#ifndef EMBEDDED_LIBRARY
    if (mysql_bin_log.is_open())
    {
      /*
        Make sure last block (the one which caused the error) gets logged.
        This is needed because otherwise after write of
        (to the binlog, not to read_info (which is a cache))
        Delete_file_log_event the bad block will remain in read_info (because
        pre_read is not called at the end of the last block; remember pre_read
        is called whenever a new block is read from disk).
        At the end of mysql_load(), the destructor of read_info will call
        end_io_cache() which will flush read_info, so we will finally have
        this in the binlog:
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
        Delete_file_log_event d(thd, db, log_delayed);
        mysql_bin_log.write(&d);
      }
    }
#endif /*!EMBEDDED_LIBRARY*/
    error= -1;				// Error on read
    goto err;
  }
  sprintf(name, ER(ER_LOAD_INFO), (ulong) info.records, (ulong) info.deleted,
	  (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
  send_ok(thd,info.copied+info.deleted,0L,name);

  if (!log_delayed)
    thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
#ifndef EMBEDDED_LIBRARY
  if (mysql_bin_log.is_open())
  {
    /*
      As already explained above, we need to call end_io_cache() or the last
      block will be logged only after Execute_load_log_event (which is wrong),
      when read_info is destroyed.
    */
    read_info.end_io_cache(); 
    if (lf_info.wrote_create_file)
    {
      Execute_load_log_event e(thd, db, log_delayed);
      mysql_bin_log.write(&e);
    }
  }
#endif /*!EMBEDDED_LIBRARY*/
  if (transactional_table)
    error=ha_autocommit_or_rollback(thd,error); 

err:
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  thd->abort_on_warning= 0;  
  DBUG_RETURN(error);
}

/****************************************************************************
** Read of rows of fixed size + optional garage + optonal newline
****************************************************************************/

static int
read_fixed_length(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
		  List<Item> &fields, READ_INFO &read_info, ulong skip_lines,
		  bool ignore_check_option_errors)
{
  List_iterator_fast<Item> it(fields);
  Item_field *sql_field;
  TABLE *table= table_list->table;
  ulonglong id;
  bool no_trans_update;
  DBUG_ENTER("read_fixed_length");

  id= 0;
  
  /* No fields can be null in this format. mark all fields as not null */
  while ((sql_field= (Item_field*) it++))
      sql_field->field->set_notnull();

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
    byte *pos=read_info.row_start;
#ifdef HAVE_purify
    read_info.row_end[0]=0;
#endif
    no_trans_update= !table->file->has_transactions();
    while ((sql_field= (Item_field*) it++))
    {
      Field *field= sql_field->field;                  
      if (pos == read_info.row_end)
      {
        thd->cuted_fields++;			/* Not enough fields */
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                            ER_WARN_TOO_FEW_RECORDS, 
                            ER(ER_WARN_TOO_FEW_RECORDS), thd->row_count);
	      field->reset();
      }
      else
      {
	uint length;
	byte save_chr;
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
                          ER(ER_WARN_TOO_MANY_RECORDS), thd->row_count); 
    }

    switch(table_list->view_check_option(thd,
					 ignore_check_option_errors))
    {
    case VIEW_CHECK_SKIP:
      read_info.next_line();
      goto continue_loop;
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(-1);
    }

    if (thd->killed || write_record(thd,table,&info))
      DBUG_RETURN(1);
    thd->no_trans_update= no_trans_update;
   
    /*
      If auto_increment values are used, save the first one
       for LAST_INSERT_ID() and for the binary/update log.
       We can't use insert_id() as we don't want to touch the
       last_insert_id_used flag.
    */
    if (!id && thd->insert_id_used)
      id= thd->last_insert_id;
    if (table->next_number_field)
      table->next_number_field->reset();	// Clear for next record
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      thd->cuted_fields++;			/* To long row */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                          ER_WARN_TOO_MANY_RECORDS, 
                          ER(ER_WARN_TOO_MANY_RECORDS), thd->row_count); 
    }
    thd->row_count++;
continue_loop:;
  }
  if (id && !read_info.error)
    thd->insert_id(id);			// For binary/update log
  DBUG_RETURN(test(read_info.error));
}



static int
read_sep_field(THD *thd, COPY_INFO &info, TABLE_LIST *table_list,
	       List<Item> &fields, READ_INFO &read_info,
	       String &enclosed, ulong skip_lines,
	       bool ignore_check_option_errors)
{
  List_iterator_fast<Item> it(fields);
  Item_field *sql_field;
  TABLE *table= table_list->table;
  uint enclosed_length;
  ulonglong id;
  bool no_trans_update;
  DBUG_ENTER("read_sep_field");

  enclosed_length=enclosed.length();
  id= 0;
  no_trans_update= !table->file->has_transactions();

  for (;;it.rewind())
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      DBUG_RETURN(1);
    }
    while ((sql_field=(Item_field*) it++))
    {
      uint length;
      byte *pos;

      if (read_info.read_field())
	break;
      pos=read_info.row_start;
      length=(uint) (read_info.row_end-pos);
      Field *field=sql_field->field;

      if (!read_info.enclosed &&
	  (enclosed_length && length == 4 && !memcmp(pos,"NULL",4)) ||
	  (length == 1 && read_info.found_null))
      {
	field->reset();
	field->set_null();
	if (!field->maybe_null())
	{
	  if (field->type() == FIELD_TYPE_TIMESTAMP)
	    ((Field_timestamp*) field)->set_time();
	  else if (field != table->next_number_field)      
	    field->set_warning((uint) MYSQL_ERROR::WARN_LEVEL_WARN, 
			       ER_WARN_NULL_TO_NOTNULL, 1);
	}
	continue;
      }
      field->set_notnull();
      read_info.row_end[0]=0;			// Safe to change end marker
      field->store((char*) read_info.row_start,length,read_info.read_charset);
    }
    if (read_info.error)
      break;
    if (skip_lines)
    {
      if (!--skip_lines)
	thd->cuted_fields= 0L;			// Reset warnings
      continue;
    }
    if (sql_field)
    {						// Last record
      if (sql_field == (Item_field*) fields.head())
	break;
      for (; sql_field ; sql_field=(Item_field*) it++)
      {
	sql_field->field->set_null();
	sql_field->field->reset();
	thd->cuted_fields++;
 	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                	    ER_WARN_TOO_FEW_RECORDS,
                	    ER(ER_WARN_TOO_FEW_RECORDS), thd->row_count);
      }
    }

    switch(table_list->view_check_option(thd,
					 ignore_check_option_errors))
    {
    case VIEW_CHECK_SKIP:
      read_info.next_line();
      goto continue_loop;
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(-1);
    }


    if (thd->killed || write_record(thd, table, &info))
      DBUG_RETURN(1);
    /*
      If auto_increment values are used, save the first one
       for LAST_INSERT_ID() and for the binary/update log.
       We can't use insert_id() as we don't want to touch the
       last_insert_id_used flag.
    */
    if (!id && thd->insert_id_used)
      id= thd->last_insert_id;
    if (table->next_number_field)
      table->next_number_field->reset();	// Clear for next record
    thd->no_trans_update= no_trans_update;
    if (read_info.next_line())			// Skip to next line
      break;
    if (read_info.line_cuted)
    {
      thd->cuted_fields++;			/* To long row */
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 
                          ER_WARN_TOO_MANY_RECORDS, ER(ER_WARN_TOO_MANY_RECORDS), 
                          thd->row_count);   
      if (thd->killed)
        DBUG_RETURN(1);
    }
    thd->row_count++;
continue_loop:;
  }
  if (id && !read_info.error)
    thd->insert_id(id);			// For binary/update log
  DBUG_RETURN(test(read_info.error));
}


/* Unescape all escape characters, mark \N as null */

char
READ_INFO::unescape(char chr)
{
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
  :file(file_par),escape_char(escape)
{
  read_charset= cs;
  field_term_ptr=(char*) field_term.ptr();
  field_term_length= field_term.length();
  line_term_ptr=(char*) line_term.ptr();
  line_term_length= line_term.length();
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
  error=eof=found_end_of_line=found_null=line_cuted=0;
  buff_length=tot_length;


  /* Set of a stack for unget if long terminators */
  uint length=max(field_term_length,line_term_length)+1;
  set_if_bigger(length,line_start.length());
  stack=stack_pos=(int*) sql_alloc(sizeof(int)*length);

  if (!(buffer=(byte*) my_malloc(buff_length+1,MYF(0))))
    error=1; /* purecov: inspected */
  else
  {
    end_of_buff=buffer+buff_length;
    if (init_io_cache(&cache,(get_it_from_net) ? -1 : file, 0,
		      (get_it_from_net) ? READ_NET :
		      (is_fifo ? READ_FIFO : READ_CACHE),0L,1,
		      MYF(MY_WME)))
    {
      my_free((gptr) buffer,MYF(0)); /* purecov: inspected */
      error=1;
    }
    else 
    {
      /*
	init_io_cache() will not initialize read_function member
	if the cache is READ_NET. The reason is explained in
	mysys/mf_iocache.c. So we work around the problem with a
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
  if (!error)
  {
    if (need_end_io_cache)
      ::end_io_cache(&cache);
    my_free((gptr) buffer,MYF(0));
    error=1;
  }
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
  byte *to,*new_buffer;

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
    *to++=(byte) chr;				// If error
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
#ifdef USE_MB
      if ((my_mbcharlen(read_charset, chr) > 1) &&
          to+my_mbcharlen(read_charset, chr) <= end_of_buff)
      {
	  uchar* p = (uchar*)to;
	  *to++ = chr;
	  int ml = my_mbcharlen(read_charset, chr);
	  int i;
	  for (i=1; i<ml; i++) {
	      chr = GET;
	      if (chr == my_b_EOF)
		  goto found_eof;
	      *to++ = chr;
	  }
	  if (my_ismbchar(read_charset,
                          (const char *)p,
                          (const char *)to))
	    continue;
	  for (i=0; i<ml; i++)
	    PUSH((uchar) *--to);
	  chr = GET;
      }
#endif
      if (chr == my_b_EOF)
	goto found_eof;
      if (chr == escape_char)
      {
	if ((chr=GET) == my_b_EOF)
	{
	  *to++= (byte) escape_char;
	  goto found_eof;
	}
	*to++ = (byte) unescape((char) chr);
	continue;
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
	  *to++ = (byte) chr;
	  continue;
	}
	// End of enclosed field if followed by field_term or line_term
	if (chr == my_b_EOF ||
	    chr == line_term_char && terminator(line_term_ptr,
						line_term_length))
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
      *to++ = (byte) chr;
    }
    /*
    ** We come here if buffer is too small. Enlarge it and continue
    */
    if (!(new_buffer=(byte*) my_realloc((char*) buffer,buff_length+1+IO_SIZE,
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
  byte *to;
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
	*to++= (byte) escape_char;
	goto found_eof;
      }
      *to++ =(byte) unescape((char) chr);
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
    *to++ = (byte) chr;
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
       for (int i=1;
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
