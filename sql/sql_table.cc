/* Copyright (C) 2000-2004 MySQL AB

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

/* drop and alter of tables */

#include "mysql_priv.h"
#include <hash.h>
#include <myisam.h>
#include <my_dir.h>
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_show.h"

#ifdef __WIN__
#include <io.h>
#endif

int creating_table= 0;        // How many mysql_create_table are running

const char *primary_key_name="PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(TABLE *from,TABLE *to,
                                    List<create_field> &create, bool ignore,
				    uint order_num, ORDER *order,
				    ha_rows *copied,ha_rows *deleted);
static bool prepare_blob_field(THD *thd, create_field *sql_field);
static bool check_engine(THD *thd, const char *table_name,
                         HA_CREATE_INFO *create_info);                             
static int mysql_prepare_table(THD *thd, HA_CREATE_INFO *create_info,
                               List<create_field> *fields,
                               List<Key> *keys, bool tmp_table,
                               uint *db_options,
                               handler *file, KEY **key_info_buffer,
                               uint *key_count, int select_field_count);

#define MYSQL50_TABLE_NAME_PREFIX         "#mysql50#"
#define MYSQL50_TABLE_NAME_PREFIX_LENGTH  9


/*
  Translate a file name to a table name (WL #1324).

  SYNOPSIS
    filename_to_tablename()
      from                      The file name in my_charset_filename.
      to                OUT     The table name in system_charset_info.
      to_length                 The size of the table name buffer.

  RETURN
    Table name length.
*/

uint filename_to_tablename(const char *from, char *to, uint to_length)
{
  uint errors;
  uint res;
  DBUG_ENTER("filename_to_tablename");
  DBUG_PRINT("enter", ("from '%s'", from));

  if (!memcmp(from, tmp_file_prefix, tmp_file_prefix_length))
  {
    /* Temporary table name. */
    res= (strnmov(to, from, to_length) - to);
  }
  else
  {
    res= strconvert(&my_charset_filename, from,
                    system_charset_info,  to, to_length, &errors);
    if (errors) // Old 5.0 name
    {
      res= (strxnmov(to, to_length, MYSQL50_TABLE_NAME_PREFIX,  from, NullS) -
            to);
      sql_print_error("Invalid (old?) table or database name '%s'", from);
      /*
        TODO: add a stored procedure for fix table and database names,
        and mention its name in error log.
      */
    }
  }

  DBUG_PRINT("exit", ("to '%s'", to));
  DBUG_RETURN(res);
}


/*
  Translate a table name to a file name (WL #1324).

  SYNOPSIS
    tablename_to_filename()
      from                      The table name in system_charset_info.
      to                OUT     The file name in my_charset_filename.
      to_length                 The size of the file name buffer.

  RETURN
    File name length.
*/

uint tablename_to_filename(const char *from, char *to, uint to_length)
{
  uint errors, length;
  DBUG_ENTER("tablename_to_filename");
  DBUG_PRINT("enter", ("from '%s'", from));

  if (from[0] == '#' && !strncmp(from, MYSQL50_TABLE_NAME_PREFIX,
                                 MYSQL50_TABLE_NAME_PREFIX_LENGTH))
    DBUG_RETURN((uint) (strmake(to, from+MYSQL50_TABLE_NAME_PREFIX_LENGTH,
                                to_length-1) -
                        (from + MYSQL50_TABLE_NAME_PREFIX_LENGTH)));
  length= strconvert(system_charset_info, from,
                     &my_charset_filename, to, to_length, &errors);
  if (check_if_legal_tablename(to) &&
      length + 4 < to_length)
  {
    memcpy(to + length, "@@@", 4);
    length+= 3;
  }
  DBUG_PRINT("exit", ("to '%s'", to));
  DBUG_RETURN(length);
}


/*
  Creates path to a file: mysql_data_dir/db/table.ext

  SYNOPSIS
   build_table_filename()
     buff                       Where to write result in my_charset_filename.
     bufflen                    buff size
     db                         Database name in system_charset_info.
     table_name                 Table name in system_charset_info.
     ext                        File extension.
     flags                      FN_FROM_IS_TMP or FN_TO_IS_TMP or FN_IS_TMP
                                table_name is temporary, do not change.

  NOTES

    Uses database and table name, and extension to create
    a file name in mysql_data_dir. Database and table
    names are converted from system_charset_info into "fscs".
    Unless flags indicate a temporary table name.
    'db' is always converted.
    'ext' is not converted.

    The conversion suppression is required for ALTER TABLE. This
    statement creates intermediate tables. These are regular
    (non-temporary) tables with a temporary name. Their path names must
    be derivable from the table name. So we cannot use
    build_tmptable_filename() for them.

  RETURN
    path length
*/

uint build_table_filename(char *buff, size_t bufflen, const char *db,
                          const char *table_name, const char *ext, uint flags)
{
  uint length;
  char dbbuff[FN_REFLEN];
  char tbbuff[FN_REFLEN];
  DBUG_ENTER("build_table_filename");

  if (flags & FN_IS_TMP) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    strnmov(tbbuff, table_name, sizeof(tbbuff));
  else
    VOID(tablename_to_filename(table_name, tbbuff, sizeof(tbbuff)));

  VOID(tablename_to_filename(db, dbbuff, sizeof(dbbuff)));
  length= strxnmov(buff, bufflen, mysql_data_home, "/", dbbuff,
                   "/", tbbuff, ext, NullS) - buff;
  DBUG_PRINT("exit", ("buff: '%s'", buff));
  DBUG_RETURN(length);
}


/*
  Creates path to a file: mysql_tmpdir/#sql1234_12_1.ext

  SYNOPSIS
   build_tmptable_filename()
     thd                        The thread handle.
     buff                       Where to write result in my_charset_filename.
     bufflen                    buff size

  NOTES

    Uses current_pid, thread_id, and tmp_table counter to create
    a file name in mysql_tmpdir.

  RETURN
    path length
*/

uint build_tmptable_filename(THD* thd, char *buff, size_t bufflen)
{
  uint length;
  char tmp_table_name[tmp_file_prefix_length+22+22+22+3];
  DBUG_ENTER("build_tmptable_filename");

  my_snprintf(tmp_table_name, sizeof(tmp_table_name),
	      "%s%lx_%lx_%x",
	      tmp_file_prefix, current_pid,
	      thd->thread_id, thd->tmp_table++);

  strxnmov(buff, bufflen, mysql_tmpdir, "/", tmp_table_name, reg_ext, NullS);
  length= unpack_filename(buff, buff);
  DBUG_PRINT("exit", ("buff: '%s'", buff));
  DBUG_RETURN(length);
}

/*
  Return values for compare_tables().
  If you make compare_tables() non-static, move them to a header file.
*/
#define ALTER_TABLE_DATA_CHANGED  1
#define ALTER_TABLE_INDEX_CHANGED 2


/*
  SYNOPSIS
    mysql_copy_create_list()
    orig_create_list          Original list of created fields
    inout::new_create_list    Copy of original list

  RETURN VALUES
    FALSE                     Success
    TRUE                      Memory allocation error

  DESCRIPTION
    mysql_prepare_table destroys the create_list and in some cases we need
    this lists for more purposes. Thus we copy it specifically for use
    by mysql_prepare_table
*/

static int mysql_copy_create_list(List<create_field> *orig_create_list,
                                  List<create_field> *new_create_list)
{
  List_iterator<create_field> prep_field_it(*orig_create_list);
  create_field *prep_field;
  DBUG_ENTER("mysql_copy_create_list");

  while ((prep_field= prep_field_it++))
  {
    create_field *field= new create_field(*prep_field);
    if (!field || new_create_list->push_back(field))
    {
      mem_alloc_error(2);
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  SYNOPSIS
    mysql_copy_key_list()
    orig_key                  Original list of keys
    inout::new_key            Copy of original list

  RETURN VALUES
    FALSE                     Success
    TRUE                      Memory allocation error

  DESCRIPTION
    mysql_prepare_table destroys the key list and in some cases we need
    this lists for more purposes. Thus we copy it specifically for use
    by mysql_prepare_table
*/

static int mysql_copy_key_list(List<Key> *orig_key,
                               List<Key> *new_key)
{
  List_iterator<Key> prep_key_it(*orig_key);
  Key *prep_key;
  DBUG_ENTER("mysql_copy_key_list");

  while ((prep_key= prep_key_it++))
  {
    List<key_part_spec> prep_columns;
    List_iterator<key_part_spec> prep_col_it(prep_key->columns);
    key_part_spec *prep_col;
    Key *temp_key;

    while ((prep_col= prep_col_it++))
    {
      key_part_spec *prep_key_part;

      if (!(prep_key_part= new key_part_spec(*prep_col)))
      {
        mem_alloc_error(sizeof(key_part_spec));
        DBUG_RETURN(TRUE);
      }
      if (prep_columns.push_back(prep_key_part))
      {
        mem_alloc_error(2);
        DBUG_RETURN(TRUE);
      }
    }
    if (!(temp_key= new Key(prep_key->type, prep_key->name,
                            &prep_key->key_create_info,
                            prep_key->generated,
                            prep_columns)))
    {
      mem_alloc_error(sizeof(Key));
      DBUG_RETURN(TRUE);
    }
    if (new_key->push_back(temp_key))
    {
      mem_alloc_error(2);
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}

/*
--------------------------------------------------------------------------

   MODULE: DDL log
   -----------------

   This module is used to ensure that we can recover from crashes that occur
   in the middle of a meta-data operation in MySQL. E.g. DROP TABLE t1, t2;
   We need to ensure that both t1 and t2 are dropped and not only t1 and
   also that each table drop is entirely done and not "half-baked".

   To support this we create log entries for each meta-data statement in the
   ddl log while we are executing. These entries are dropped when the
   operation is completed.

   At recovery those entries that were not completed will be executed.

   There is only one ddl log in the system and it is protected by a mutex
   and there is a global struct that contains information about its current
   state.

   History:
   First version written in 2006 by Mikael Ronstrom
--------------------------------------------------------------------------
*/


typedef struct st_global_ddl_log
{
  /*
    We need to adjust buffer size to be able to handle downgrades/upgrades
    where IO_SIZE has changed. We'll set the buffer size such that we can
    handle that the buffer size was upto 4 times bigger in the version
    that wrote the DDL log.
  */
  char file_entry_buf[4*IO_SIZE];
  char file_name_str[FN_REFLEN];
  char *file_name;
  DDL_LOG_MEMORY_ENTRY *first_free;
  DDL_LOG_MEMORY_ENTRY *first_used;
  uint num_entries;
  File file_id;
  uint name_len;
  uint io_size;
  bool inited;
  bool recovery_phase;
} GLOBAL_DDL_LOG;

GLOBAL_DDL_LOG global_ddl_log;

pthread_mutex_t LOCK_gdl;

#define DDL_LOG_ENTRY_TYPE_POS 0
#define DDL_LOG_ACTION_TYPE_POS 1
#define DDL_LOG_PHASE_POS 2
#define DDL_LOG_NEXT_ENTRY_POS 4
#define DDL_LOG_NAME_POS 8

#define DDL_LOG_NUM_ENTRY_POS 0
#define DDL_LOG_NAME_LEN_POS 4
#define DDL_LOG_IO_SIZE_POS 8

/*
  Read one entry from ddl log file
  SYNOPSIS
    read_ddl_log_file_entry()
    entry_no                     Entry number to read
  RETURN VALUES
    TRUE                         Error
    FALSE                        Success
*/

static bool read_ddl_log_file_entry(uint entry_no)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  uint io_size= global_ddl_log.io_size;
  DBUG_ENTER("read_ddl_log_file_entry");

  if (my_pread(file_id, (byte*)file_entry_buf, io_size, io_size * entry_no,
               MYF(MY_WME)) != io_size)
    error= TRUE;
  DBUG_RETURN(error);
}


/*
  Write one entry from ddl log file
  SYNOPSIS
    write_ddl_log_file_entry()
    entry_no                     Entry number to read
  RETURN VALUES
    TRUE                         Error
    FALSE                        Success
*/

static bool write_ddl_log_file_entry(uint entry_no)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("write_ddl_log_file_entry");

  if (my_pwrite(file_id, (byte*)file_entry_buf,
                IO_SIZE, IO_SIZE * entry_no, MYF(MY_WME)) != IO_SIZE)
    error= TRUE;
  DBUG_RETURN(error);
}


/*
  Write ddl log header
  SYNOPSIS
    write_ddl_log_header()
  RETURN VALUES
    TRUE                      Error
    FALSE                     Success
*/

static bool write_ddl_log_header()
{
  uint16 const_var;
  bool error= FALSE;
  DBUG_ENTER("write_ddl_log_header");

  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NUM_ENTRY_POS],
            global_ddl_log.num_entries);
  const_var= FN_LEN;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_LEN_POS],
            const_var);
  const_var= IO_SIZE;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_IO_SIZE_POS],
            const_var);
  if (write_ddl_log_file_entry(0UL))
  {
    sql_print_error("Error writing ddl log header");
    DBUG_RETURN(TRUE);
  }
  VOID(sync_ddl_log());
  DBUG_RETURN(error);
}


/*
  Create ddl log file name
  SYNOPSIS
    create_ddl_log_file_name()
    file_name                   Filename setup
  RETURN VALUES
    NONE
*/

static inline void create_ddl_log_file_name(char *file_name)
{
  strxmov(file_name, mysql_data_home, "/", "ddl_log.log", NullS);
}


/*
  Read header of ddl log file
  SYNOPSIS
    read_ddl_log_header()
  RETURN VALUES
    > 0                  Last entry in ddl log
    0                    No entries in ddl log
  DESCRIPTION
    When we read the ddl log header we get information about maximum sizes
    of names in the ddl log and we also get information about the number
    of entries in the ddl log.
*/

static uint read_ddl_log_header()
{
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  char file_name[FN_REFLEN];
  uint entry_no;
  bool successful_open= FALSE;
  DBUG_ENTER("read_ddl_log_header");

  create_ddl_log_file_name(file_name);
  if ((global_ddl_log.file_id= my_open(file_name,
                                        O_RDWR | O_BINARY, MYF(MY_WME))) >= 0)
  {
    if (read_ddl_log_file_entry(0UL))
    {
      /* Write message into error log */
      sql_print_error("Failed to read ddl log file in recovery");
    }
    else
      successful_open= TRUE;
  }
  entry_no= uint4korr(&file_entry_buf[DDL_LOG_NUM_ENTRY_POS]);
  global_ddl_log.name_len= uint4korr(&file_entry_buf[DDL_LOG_NAME_LEN_POS]);
  if (successful_open)
  {
    global_ddl_log.io_size= uint4korr(&file_entry_buf[DDL_LOG_IO_SIZE_POS]);
    DBUG_ASSERT(global_ddl_log.io_size <=
                sizeof(global_ddl_log.file_entry_buf));
  }
  else
  {
    entry_no= 0;
  }
  global_ddl_log.first_free= NULL;
  global_ddl_log.first_used= NULL;
  global_ddl_log.num_entries= 0;
  VOID(pthread_mutex_init(&LOCK_gdl, MY_MUTEX_INIT_FAST));
  DBUG_RETURN(entry_no);
}


/*
  Read a ddl log entry
  SYNOPSIS
    read_ddl_log_entry()
    read_entry               Number of entry to read
    out:entry_info           Information from entry
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    Read a specified entry in the ddl log
*/

bool read_ddl_log_entry(uint read_entry, DDL_LOG_ENTRY *ddl_log_entry)
{
  char *file_entry_buf= (char*)&global_ddl_log.file_entry_buf;
  uint inx;
  uchar single_char;
  DBUG_ENTER("read_ddl_log_entry");

  if (read_ddl_log_file_entry(read_entry))
  {
    DBUG_RETURN(TRUE);
  }
  ddl_log_entry->entry_pos= read_entry;
  single_char= file_entry_buf[DDL_LOG_ENTRY_TYPE_POS];
  ddl_log_entry->entry_type= (enum ddl_log_entry_code)single_char;
  single_char= file_entry_buf[DDL_LOG_ACTION_TYPE_POS];
  ddl_log_entry->action_type= (enum ddl_log_action_code)single_char;
  ddl_log_entry->phase= file_entry_buf[DDL_LOG_PHASE_POS];
  ddl_log_entry->next_entry= uint4korr(&file_entry_buf[DDL_LOG_NEXT_ENTRY_POS]);
  ddl_log_entry->name= &file_entry_buf[DDL_LOG_NAME_POS];
  inx= DDL_LOG_NAME_POS + global_ddl_log.name_len;
  ddl_log_entry->from_name= &file_entry_buf[inx];
  inx+= global_ddl_log.name_len;
  ddl_log_entry->handler_name= &file_entry_buf[inx];
  DBUG_RETURN(FALSE);
}


/*
  Initialise ddl log
  SYNOPSIS
    init_ddl_log()

  DESCRIPTION
    Write the header of the ddl log file and length of names. Also set
    number of entries to zero.

  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
*/

static bool init_ddl_log()
{
  bool error= FALSE;
  char file_name[FN_REFLEN];
  DBUG_ENTER("init_ddl_log");

  if (global_ddl_log.inited)
    goto end;

  global_ddl_log.io_size= IO_SIZE;
  create_ddl_log_file_name(file_name);
  if ((global_ddl_log.file_id= my_create(file_name,
                                         CREATE_MODE,
                                         O_RDWR | O_TRUNC | O_BINARY,
                                         MYF(MY_WME))) < 0)
  {
    /* Couldn't create ddl log file, this is serious error */
    sql_print_error("Failed to open ddl log file");
    DBUG_RETURN(TRUE);
  }
  global_ddl_log.inited= TRUE;
  if (write_ddl_log_header())
  {
    VOID(my_close(global_ddl_log.file_id, MYF(MY_WME)));
    global_ddl_log.inited= FALSE;
    DBUG_RETURN(TRUE);
  }

end:
  DBUG_RETURN(FALSE);
}


/*
  Execute one action in a ddl log entry
  SYNOPSIS
    execute_ddl_log_action()
    ddl_log_entry              Information in action entry to execute
  RETURN VALUES
    TRUE                       Error
    FALSE                      Success
*/

static int execute_ddl_log_action(THD *thd, DDL_LOG_ENTRY *ddl_log_entry)
{
  bool frm_action= FALSE;
  LEX_STRING handler_name;
  handler *file= NULL;
  MEM_ROOT mem_root;
  int error= TRUE;
  char to_path[FN_REFLEN];
  char from_path[FN_REFLEN];
  char *par_ext= (char*)".par";
  handlerton *hton;
  DBUG_ENTER("execute_ddl_log_action");

  if (ddl_log_entry->entry_type == DDL_IGNORE_LOG_ENTRY_CODE)
  {
    DBUG_RETURN(FALSE);
  }
  handler_name.str= (char*)ddl_log_entry->handler_name;
  handler_name.length= strlen(ddl_log_entry->handler_name);
  init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0); 
  if (!strcmp(ddl_log_entry->handler_name, reg_ext))
    frm_action= TRUE;
  else
  {
    TABLE_SHARE dummy;

    hton= ha_resolve_by_name(thd, &handler_name);
    if (!hton)
    {
      my_error(ER_ILLEGAL_HA, MYF(0), ddl_log_entry->handler_name);
      goto error;
    }
    bzero(&dummy, sizeof(TABLE_SHARE));
    file= get_new_handler(&dummy, &mem_root, hton);
    if (!file)
    {
      mem_alloc_error(sizeof(handler));
      goto error;
    }
  }
  switch (ddl_log_entry->action_type)
  {
    case DDL_LOG_REPLACE_ACTION:
    case DDL_LOG_DELETE_ACTION:
    {
      if (ddl_log_entry->phase == 0)
      {
        if (frm_action)
        {
          strxmov(to_path, ddl_log_entry->name, reg_ext, NullS);
          if ((error= my_delete(to_path, MYF(MY_WME))))
          {
            if (my_errno != ENOENT)
              break;
          }
#ifdef WITH_PARTITION_STORAGE_ENGINE
          strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
          VOID(my_delete(to_path, MYF(MY_WME)));
#endif
        }
        else
        {
          if ((error= file->delete_table(ddl_log_entry->name)))
          {
            if (error != ENOENT && error != HA_ERR_NO_SUCH_TABLE)
              break;
          }
        }
        if ((deactivate_ddl_log_entry(ddl_log_entry->entry_pos)))
          break;
        VOID(sync_ddl_log());
        error= FALSE;
        if (ddl_log_entry->action_type == DDL_LOG_DELETE_ACTION)
          break;
      }
      DBUG_ASSERT(ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION);
      /*
        Fall through and perform the rename action of the replace
        action. We have already indicated the success of the delete
        action in the log entry by stepping up the phase.
      */
    }
    case DDL_LOG_RENAME_ACTION:
    {
      error= TRUE;
      if (frm_action)
      {
        strxmov(to_path, ddl_log_entry->name, reg_ext, NullS);
        strxmov(from_path, ddl_log_entry->from_name, reg_ext, NullS);
        if (my_rename(from_path, to_path, MYF(MY_WME)))
          break;
#ifdef WITH_PARTITION_STORAGE_ENGINE
        strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
        strxmov(from_path, ddl_log_entry->from_name, par_ext, NullS);
        VOID(my_rename(from_path, to_path, MYF(MY_WME)));
#endif
      }
      else
      {
        if (file->rename_table(ddl_log_entry->from_name,
                               ddl_log_entry->name))
          break;
      }
      if ((deactivate_ddl_log_entry(ddl_log_entry->entry_pos)))
        break;
      VOID(sync_ddl_log());
      error= FALSE;
      break;
    }
    default:
      DBUG_ASSERT(0);
      break;
  }
  delete file;
error:
  free_root(&mem_root, MYF(0)); 
  DBUG_RETURN(error);
}


/*
  Get a free entry in the ddl log
  SYNOPSIS
    get_free_ddl_log_entry()
    out:active_entry                A ddl log memory entry returned
  RETURN VALUES
    TRUE                       Error
    FALSE                      Success
*/

static bool get_free_ddl_log_entry(DDL_LOG_MEMORY_ENTRY **active_entry,
                                   bool *write_header)
{
  DDL_LOG_MEMORY_ENTRY *used_entry;
  DDL_LOG_MEMORY_ENTRY *first_used= global_ddl_log.first_used;
  DBUG_ENTER("get_free_ddl_log_entry");

  if (global_ddl_log.first_free == NULL)
  {
    if (!(used_entry= (DDL_LOG_MEMORY_ENTRY*)my_malloc(
                              sizeof(DDL_LOG_MEMORY_ENTRY), MYF(MY_WME))))
    {
      sql_print_error("Failed to allocate memory for ddl log free list");
      DBUG_RETURN(TRUE);
    }
    global_ddl_log.num_entries++;
    used_entry->entry_pos= global_ddl_log.num_entries;
    *write_header= TRUE;
  }
  else
  {
    used_entry= global_ddl_log.first_free;
    global_ddl_log.first_free= used_entry->next_log_entry;
    *write_header= FALSE;
  }
  /*
    Move from free list to used list
  */
  used_entry->next_log_entry= first_used;
  used_entry->prev_log_entry= NULL;
  global_ddl_log.first_used= used_entry;
  if (first_used)
    first_used->prev_log_entry= used_entry;

  *active_entry= used_entry;
  DBUG_RETURN(FALSE);
}


/*
  External interface methods for the DDL log Module
  ---------------------------------------------------
*/

/*
  SYNOPSIS
    write_ddl_log_entry()
    ddl_log_entry         Information about log entry
    out:entry_written     Entry information written into   

  RETURN VALUES
    TRUE                      Error
    FALSE                     Success

  DESCRIPTION
    A careful write of the ddl log is performed to ensure that we can
    handle crashes occurring during CREATE and ALTER TABLE processing.
*/

bool write_ddl_log_entry(DDL_LOG_ENTRY *ddl_log_entry,
                         DDL_LOG_MEMORY_ENTRY **active_entry)
{
  bool error, write_header;
  DBUG_ENTER("write_ddl_log_entry");

  if (init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  global_ddl_log.file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]=
                                    (char)DDL_LOG_ENTRY_CODE;
  global_ddl_log.file_entry_buf[DDL_LOG_ACTION_TYPE_POS]=
                                    (char)ddl_log_entry->action_type;
  global_ddl_log.file_entry_buf[DDL_LOG_PHASE_POS]= 0;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NEXT_ENTRY_POS],
            ddl_log_entry->next_entry);
  DBUG_ASSERT(strlen(ddl_log_entry->name) < FN_LEN);
  strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS],
          ddl_log_entry->name, FN_LEN - 1);
  if (ddl_log_entry->action_type == DDL_LOG_RENAME_ACTION ||
      ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION)
  {
    DBUG_ASSERT(strlen(ddl_log_entry->from_name) < FN_LEN);
    strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + FN_LEN],
          ddl_log_entry->from_name, FN_LEN - 1);
  }
  else
    global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + FN_LEN]= 0;
  DBUG_ASSERT(strlen(ddl_log_entry->handler_name) < FN_LEN);
  strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + (2*FN_LEN)],
          ddl_log_entry->handler_name, FN_LEN - 1);
  if (get_free_ddl_log_entry(active_entry, &write_header))
  {
    DBUG_RETURN(TRUE);
  }
  error= FALSE;
  if (write_ddl_log_file_entry((*active_entry)->entry_pos))
  {
    error= TRUE;
    sql_print_error("Failed to write entry_no = %u",
                    (*active_entry)->entry_pos);
  }
  if (write_header && !error)
  {
    VOID(sync_ddl_log());
    if (write_ddl_log_header())
      error= TRUE;
  }
  if (error)
    release_ddl_log_memory_entry(*active_entry);
  DBUG_RETURN(error);
}


/*
  Write final entry in the ddl log
  SYNOPSIS
    write_execute_ddl_log_entry()
    first_entry                    First entry in linked list of entries
                                   to execute, if 0 = NULL it means that
                                   the entry is removed and the entries
                                   are put into the free list.
    complete                       Flag indicating we are simply writing
                                   info about that entry has been completed
    in:out:active_entry            Entry to execute, 0 = NULL if the entry
                                   is written first time and needs to be
                                   returned. In this case the entry written
                                   is returned in this parameter
  RETURN VALUES
    TRUE                           Error
    FALSE                          Success

  DESCRIPTION
    This is the last write in the ddl log. The previous log entries have
    already been written but not yet synched to disk.
    We write a couple of log entries that describes action to perform.
    This entries are set-up in a linked list, however only when a first
    execute entry is put as the first entry these will be executed.
    This routine writes this first 
*/ 

bool write_execute_ddl_log_entry(uint first_entry,
                                 bool complete,
                                 DDL_LOG_MEMORY_ENTRY **active_entry)
{
  bool write_header= FALSE;
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("write_execute_ddl_log_entry");

  if (init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  if (!complete)
  {
    /*
      We haven't synched the log entries yet, we synch them now before
      writing the execute entry. If complete is true we haven't written
      any log entries before, we are only here to write the execute
      entry to indicate it is done.
    */
    VOID(sync_ddl_log());
    file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= (char)DDL_LOG_EXECUTE_CODE;
  }
  else
    file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= (char)DDL_IGNORE_LOG_ENTRY_CODE;
  file_entry_buf[DDL_LOG_ACTION_TYPE_POS]= 0; /* Ignored for execute entries */
  file_entry_buf[DDL_LOG_PHASE_POS]= 0;
  int4store(&file_entry_buf[DDL_LOG_NEXT_ENTRY_POS], first_entry);
  file_entry_buf[DDL_LOG_NAME_POS]= 0;
  file_entry_buf[DDL_LOG_NAME_POS + FN_LEN]= 0;
  file_entry_buf[DDL_LOG_NAME_POS + 2*FN_LEN]= 0;
  if (!(*active_entry))
  {
    if (get_free_ddl_log_entry(active_entry, &write_header))
    {
      DBUG_RETURN(TRUE);
    }
  }
  if (write_ddl_log_file_entry((*active_entry)->entry_pos))
  {
    sql_print_error("Error writing execute entry in ddl log");
    release_ddl_log_memory_entry(*active_entry);
    DBUG_RETURN(TRUE);
  }
  VOID(sync_ddl_log());
  if (write_header)
  {
    if (write_ddl_log_header())
    {
      release_ddl_log_memory_entry(*active_entry);
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  For complex rename operations we need to deactivate individual entries.
  SYNOPSIS
    deactivate_ddl_log_entry()
    entry_no                      Entry position of record to change
  RETURN VALUES
    TRUE                         Error
    FALSE                        Success
  DESCRIPTION
    During replace operations where we start with an existing table called
    t1 and a replacement table called t1#temp or something else and where
    we want to delete t1 and rename t1#temp to t1 this is not possible to
    do in a safe manner unless the ddl log is informed of the phases in
    the change.

    Delete actions are 1-phase actions that can be ignored immediately after
    being executed.
    Rename actions from x to y is also a 1-phase action since there is no
    interaction with any other handlers named x and y.
    Replace action where drop y and x -> y happens needs to be a two-phase
    action. Thus the first phase will drop y and the second phase will
    rename x -> y.
*/

bool deactivate_ddl_log_entry(uint entry_no)
{
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("deactivate_ddl_log_entry");

  if (!read_ddl_log_file_entry(entry_no))
  {
    if (file_entry_buf[DDL_LOG_ENTRY_TYPE_POS] == DDL_LOG_ENTRY_CODE)
    {
      if (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_DELETE_ACTION ||
          file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_RENAME_ACTION ||
          (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_REPLACE_ACTION &&
           file_entry_buf[DDL_LOG_PHASE_POS] == 1))
        file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= DDL_IGNORE_LOG_ENTRY_CODE;
      else if (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_REPLACE_ACTION)
      {
        DBUG_ASSERT(file_entry_buf[DDL_LOG_PHASE_POS] == 0);
        file_entry_buf[DDL_LOG_PHASE_POS]= 1;
      }
      else
      {
        DBUG_ASSERT(0);
      }
      if (write_ddl_log_file_entry(entry_no))
      {
        sql_print_error("Error in deactivating log entry. Position = %u",
                        entry_no);
        DBUG_RETURN(TRUE);
      }
    }
  }
  else
  {
    sql_print_error("Failed in reading entry before deactivating it");
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/*
  Sync ddl log file
  SYNOPSIS
    sync_ddl_log()
  RETURN VALUES
    TRUE                      Error
    FALSE                     Success
*/

bool sync_ddl_log()
{
  bool error= FALSE;
  DBUG_ENTER("sync_ddl_log");

  if ((!global_ddl_log.recovery_phase) &&
      init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  if (my_sync(global_ddl_log.file_id, MYF(0)))
  {
    /* Write to error log */
    sql_print_error("Failed to sync ddl log");
    error= TRUE;
  }
  DBUG_RETURN(error);
}


/*
  Release a log memory entry
  SYNOPSIS
    release_ddl_log_memory_entry()
    log_memory_entry                Log memory entry to release
  RETURN VALUES
    NONE
*/

void release_ddl_log_memory_entry(DDL_LOG_MEMORY_ENTRY *log_entry)
{
  DDL_LOG_MEMORY_ENTRY *first_free= global_ddl_log.first_free;
  DDL_LOG_MEMORY_ENTRY *next_log_entry= log_entry->next_log_entry;
  DDL_LOG_MEMORY_ENTRY *prev_log_entry= log_entry->prev_log_entry;
  DBUG_ENTER("release_ddl_log_memory_entry");

  global_ddl_log.first_free= log_entry;
  log_entry->next_log_entry= first_free;

  if (prev_log_entry)
    prev_log_entry->next_log_entry= next_log_entry;
  else
    global_ddl_log.first_used= next_log_entry;
  if (next_log_entry)
    next_log_entry->prev_log_entry= prev_log_entry;
  DBUG_VOID_RETURN;
}


/*
  Execute one entry in the ddl log. Executing an entry means executing
  a linked list of actions.
  SYNOPSIS
    execute_ddl_log_entry()
    first_entry                Reference to first action in entry
  RETURN VALUES
    TRUE                       Error
    FALSE                      Success
*/

bool execute_ddl_log_entry(THD *thd, uint first_entry)
{
  DDL_LOG_ENTRY ddl_log_entry;
  uint read_entry= first_entry;
  DBUG_ENTER("execute_ddl_log_entry");

  pthread_mutex_lock(&LOCK_gdl);
  do
  {
    if (read_ddl_log_entry(read_entry, &ddl_log_entry))
    {
      /* Write to error log and continue with next log entry */
      sql_print_error("Failed to read entry = %u from ddl log",
                      read_entry);
      break;
    }
    DBUG_ASSERT(ddl_log_entry.entry_type == DDL_LOG_ENTRY_CODE ||
                ddl_log_entry.entry_type == DDL_IGNORE_LOG_ENTRY_CODE);

    if (execute_ddl_log_action(thd, &ddl_log_entry))
    {
      /* Write to error log and continue with next log entry */
      sql_print_error("Failed to execute action for entry = %u from ddl log",
                      read_entry);
      break;
    }
    read_entry= ddl_log_entry.next_entry;
  } while (read_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(FALSE);
}


/*
  Execute the ddl log at recovery of MySQL Server
  SYNOPSIS
    execute_ddl_log_recovery()
  RETURN VALUES
    NONE
*/

void execute_ddl_log_recovery()
{
  uint num_entries, i;
  THD *thd;
  DDL_LOG_ENTRY ddl_log_entry;
  char file_name[FN_REFLEN];
  DBUG_ENTER("execute_ddl_log_recovery");

  /*
    Initialise global_ddl_log struct
  */
  bzero(global_ddl_log.file_entry_buf, sizeof(global_ddl_log.file_entry_buf));
  global_ddl_log.inited= FALSE;
  global_ddl_log.recovery_phase= TRUE;
  global_ddl_log.io_size= IO_SIZE;
  global_ddl_log.file_id= (File) -1;

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_VOID_RETURN;
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  num_entries= read_ddl_log_header();
  for (i= 1; i < num_entries + 1; i++)
  {
    if (read_ddl_log_entry(i, &ddl_log_entry))
    {
      sql_print_error("Failed to read entry no = %u from ddl log",
                       i);
      continue;
    }
    if (ddl_log_entry.entry_type == DDL_LOG_EXECUTE_CODE)
    {
      if (execute_ddl_log_entry(thd, ddl_log_entry.next_entry))
      {
        /* Real unpleasant scenario but we continue anyways.  */
        continue;
      }
    }
  }
  create_ddl_log_file_name(file_name);
  VOID(my_delete(file_name, MYF(0)));
  global_ddl_log.recovery_phase= FALSE;
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_VOID_RETURN;
}


/*
  Release all memory allocated to the ddl log
  SYNOPSIS
    release_ddl_log()
  RETURN VALUES
    NONE
*/

void release_ddl_log()
{
  DDL_LOG_MEMORY_ENTRY *free_list= global_ddl_log.first_free;
  DDL_LOG_MEMORY_ENTRY *used_list= global_ddl_log.first_used;
  DBUG_ENTER("release_ddl_log");

  pthread_mutex_lock(&LOCK_gdl);
  while (used_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= used_list->next_log_entry;
    my_free((char*)used_list, MYF(0));
    used_list= tmp;
  }
  while (free_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= free_list->next_log_entry;
    my_free((char*)free_list, MYF(0));
    free_list= tmp;
  }
  if (global_ddl_log.file_id >= 0)
  {
    VOID(my_close(global_ddl_log.file_id, MYF(MY_WME)));
    global_ddl_log.file_id= (File) -1;
  }
  global_ddl_log.inited= 0;
  pthread_mutex_unlock(&LOCK_gdl);
  VOID(pthread_mutex_destroy(&LOCK_gdl));
  DBUG_VOID_RETURN;
}


/*
---------------------------------------------------------------------------

  END MODULE DDL log
  --------------------

---------------------------------------------------------------------------
*/


/*
  SYNOPSIS
    mysql_write_frm()
    lpt                    Struct carrying many parameters needed for this
                           method
    flags                  Flags as defined below
      WFRM_INITIAL_WRITE        If set we need to prepare table before
                                creating the frm file
      WFRM_CREATE_HANDLER_FILES If set we need to create the handler file as
                                part of the creation of the frm file
      WFRM_PACK_FRM             If set we should pack the frm file and delete
                                the frm file

  RETURN VALUES
    TRUE                   Error
    FALSE                  Success

  DESCRIPTION
    A support method that creates a new frm file and in this process it
    regenerates the partition data. It works fine also for non-partitioned
    tables since it only handles partitioned data if it exists.
*/

bool mysql_write_frm(ALTER_PARTITION_PARAM_TYPE *lpt, uint flags)
{
  /*
    Prepare table to prepare for writing a new frm file where the
    partitions in add/drop state have temporarily changed their state
    We set tmp_table to avoid get errors on naming of primary key index.
  */
  int error= 0;
  char path[FN_REFLEN+1];
  char shadow_path[FN_REFLEN+1];
  char shadow_frm_name[FN_REFLEN+1];
  char frm_name[FN_REFLEN+1];
  DBUG_ENTER("mysql_write_frm");

  /*
    Build shadow frm file name
  */
  build_table_filename(shadow_path, sizeof(shadow_path), lpt->db,
                       lpt->table_name, "#", 0);
  strxmov(shadow_frm_name, shadow_path, reg_ext, NullS);
  if (flags & WFRM_WRITE_SHADOW)
  {
    if (mysql_copy_create_list(lpt->create_list,
                               &lpt->new_create_list) ||
        mysql_copy_key_list(lpt->key_list,
                            &lpt->new_key_list) ||
        mysql_prepare_table(lpt->thd, lpt->create_info,
                            &lpt->new_create_list,
                            &lpt->new_key_list,
                            /*tmp_table*/ 1,
                            &lpt->db_options,
                            lpt->table->file,
                            &lpt->key_info_buffer,
                            &lpt->key_count,
                            /*select_field_count*/ 0))
    {
      DBUG_RETURN(TRUE);
    }
#ifdef WITH_PARTITION_STORAGE_ENGINE
    {
      partition_info *part_info= lpt->table->part_info;
      char *part_syntax_buf;
      uint syntax_len;

      if (part_info)
      {
        if (!(part_syntax_buf= generate_partition_syntax(part_info,
                                                         &syntax_len,
                                                         TRUE, TRUE)))
        {
          DBUG_RETURN(TRUE);
        }
        part_info->part_info_string= part_syntax_buf;
        part_info->part_info_len= syntax_len;
      }
    }
#endif
    /* Write shadow frm file */
    lpt->create_info->table_options= lpt->db_options;
    if ((mysql_create_frm(lpt->thd, shadow_frm_name, lpt->db,
                          lpt->table_name, lpt->create_info,
                          lpt->new_create_list, lpt->key_count,
                          lpt->key_info_buffer, lpt->table->file)) ||
         lpt->table->file->create_handler_files(shadow_path, NULL,
                                                CHF_CREATE_FLAG,
                                                lpt->create_info))
    {
      my_delete(shadow_frm_name, MYF(0));
      error= 1;
      goto end;
    }
  }
  if (flags & WFRM_PACK_FRM)
  {
    /*
      We need to pack the frm file and after packing it we delete the
      frm file to ensure it doesn't get used. This is only used for
      handlers that have the main version of the frm file stored in the
      handler.
    */
    const void *data= 0;
    uint length= 0;
    if (readfrm(shadow_path, &data, &length) ||
        packfrm(data, length, &lpt->pack_frm_data, &lpt->pack_frm_len))
    {
      my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
      my_free((char*)lpt->pack_frm_data, MYF(MY_ALLOW_ZERO_PTR));
      mem_alloc_error(length);
      error= 1;
      goto end;
    }
    error= my_delete(shadow_frm_name, MYF(MY_WME));
  }
  if (flags & WFRM_INSTALL_SHADOW)
  {
#ifdef WITH_PARTITION_STORAGE_ENGINE
    partition_info *part_info= lpt->part_info;
#endif
    /*
      Build frm file name
    */
    build_table_filename(path, sizeof(path), lpt->db,
                         lpt->table_name, "", 0);
    strxmov(frm_name, path, reg_ext, NullS);
    /*
      When we are changing to use new frm file we need to ensure that we
      don't collide with another thread in process to open the frm file.
      We start by deleting the .frm file and possible .par file. Then we
      write to the DDL log that we have completed the delete phase by
      increasing the phase of the log entry. Next step is to rename the
      new .frm file and the new .par file to the real name. After
      completing this we write a new phase to the log entry that will
      deactivate it.
    */
    VOID(pthread_mutex_lock(&LOCK_open));
    if (my_delete(frm_name, MYF(MY_WME)) ||
#ifdef WITH_PARTITION_STORAGE_ENGINE
        lpt->table->file->create_handler_files(path, shadow_path,
                                               CHF_DELETE_FLAG, NULL) ||
        deactivate_ddl_log_entry(part_info->frm_log_entry->entry_pos) ||
        (sync_ddl_log(), FALSE) ||
#endif
#ifdef WITH_PARTITION_STORAGE_ENGINE
        my_rename(shadow_frm_name, frm_name, MYF(MY_WME)) ||
        lpt->table->file->create_handler_files(path, shadow_path,
                                               CHF_RENAME_FLAG, NULL))
#else
        my_rename(shadow_frm_name, frm_name, MYF(MY_WME)))
#endif
    {
      error= 1;
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
#ifdef WITH_PARTITION_STORAGE_ENGINE
    deactivate_ddl_log_entry(part_info->frm_log_entry->entry_pos);
    part_info->frm_log_entry= NULL;
    VOID(sync_ddl_log());
#endif
  }

end:
  DBUG_RETURN(error);
}


/*
  SYNOPSIS
    write_bin_log()
    thd                           Thread object
    clear_error                   is clear_error to be called
    query                         Query to log
    query_length                  Length of query

  RETURN VALUES
    NONE

  DESCRIPTION
    Write the binlog if open, routine used in multiple places in this
    file
*/

void write_bin_log(THD *thd, bool clear_error,
                   char const *query, ulong query_length)
{
  if (mysql_bin_log.is_open())
  {
    if (clear_error)
      thd->clear_error();
    thd->binlog_query(THD::STMT_QUERY_TYPE,
                      query, query_length, FALSE, FALSE);
  }
}


/*
 delete (drop) tables.

  SYNOPSIS
   mysql_rm_table()
   thd			Thread handle
   tables		List of tables to delete
   if_exists		If 1, don't give error if one table doesn't exists

  NOTES
    Will delete all tables that can be deleted and give a compact error
    messages for tables that could not be deleted.
    If a table is in use, we will wait for all users to free the table
    before dropping it

    Wait if global_read_lock (FLUSH TABLES WITH READ LOCK) is set.

  RETURN
    FALSE OK.  In this case ok packet is sent to user
    TRUE  Error

*/

bool mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
                    my_bool drop_temporary)
{
  bool error= FALSE, need_start_waiters= FALSE;
  DBUG_ENTER("mysql_rm_table");

  /* mark for close and remove all cached entries */

  if (!drop_temporary)
  {
    if ((error= wait_if_global_read_lock(thd, 0, 1)))
    {
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), tables->table_name);
      DBUG_RETURN(TRUE);
    }
    else
      need_start_waiters= TRUE;
  }

  /*
    Acquire LOCK_open after wait_if_global_read_lock(). If we would hold
    LOCK_open during wait_if_global_read_lock(), other threads could not
    close their tables. This would make a pretty deadlock.
  */
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  error= mysql_rm_table_part2(thd, tables, if_exists, drop_temporary, 0, 0);

  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  if (need_start_waiters)
    start_waiting_global_read_lock(thd);

  if (error)
    DBUG_RETURN(TRUE);
  send_ok(thd);
  DBUG_RETURN(FALSE);
}


/*
 delete (drop) tables.

  SYNOPSIS
    mysql_rm_table_part2_with_lock()
    thd			Thread handle
    tables		List of tables to delete
    if_exists		If 1, don't give error if one table doesn't exists
    dont_log_query	Don't write query to log files. This will also not
                        generate warnings if the handler files doesn't exists

 NOTES
   Works like documented in mysql_rm_table(), but don't check
   global_read_lock and don't send_ok packet to server.

 RETURN
  0	ok
  1	error
*/

int mysql_rm_table_part2_with_lock(THD *thd,
				   TABLE_LIST *tables, bool if_exists,
				   bool drop_temporary, bool dont_log_query)
{
  int error;
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  error= mysql_rm_table_part2(thd, tables, if_exists, drop_temporary, 1,
			      dont_log_query);

  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  return error;
}


/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    mysql_rm_table_part2()
    thd			Thread handler
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables
    drop_view		Allow to delete VIEW .frm
    dont_log_query	Don't write query to log files. This will also not
			generate warnings if the handler files doesn't exists  

  TODO:
    When logging to the binary log, we should log
    tmp_tables and transactional tables as separate statements if we
    are in a transaction;  This is needed to get these tables into the
    cached binary log that is only written on COMMIT.

   The current code only writes DROP statements that only uses temporary
   tables to the cache binary log.  This should be ok on most cases, but
   not all.

 RETURN
   0	ok
   1	Error
   -1	Thread was killed
*/

int mysql_rm_table_part2(THD *thd, TABLE_LIST *tables, bool if_exists,
			 bool drop_temporary, bool drop_view,
			 bool dont_log_query)
{
  TABLE_LIST *table;
  char path[FN_REFLEN], *alias;
  uint path_length;
  String wrong_tables;
  int error;
  int non_temp_tables_count= 0;
  bool some_tables_deleted=0, tmp_table_deleted=0, foreign_key_error=0;
  String built_query;
  DBUG_ENTER("mysql_rm_table_part2");

  LINT_INIT(alias);
  LINT_INIT(path_length);
  safe_mutex_assert_owner(&LOCK_open);

  if (thd->current_stmt_binlog_row_based && !dont_log_query)
  {
    built_query.set_charset(system_charset_info);
    if (if_exists)
      built_query.append("DROP TABLE IF EXISTS ");
    else
      built_query.append("DROP TABLE ");
  }
  /*
    If we have the table in the definition cache, we don't have to check the
    .frm file to find if the table is a normal table (not view) and what
    engine to use.
  */

  for (table= tables; table; table= table->next_local)
  {
    TABLE_SHARE *share;
    table->db_type= NULL;
    if ((share= get_cached_table_share(table->db, table->table_name)))
      table->db_type= share->db_type;
  }

  if (lock_table_names(thd, tables))
    DBUG_RETURN(1);

  /* Don't give warnings for not found errors, as we already generate notes */
  thd->no_warnings_for_error= 1;

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    handlerton *table_type;
    enum legacy_db_type frm_db_type;

    mysql_ha_flush(thd, table, MYSQL_HA_CLOSE_FINAL, TRUE);
    if (!close_temporary_table(thd, table))
    {
      tmp_table_deleted=1;
      continue;					// removed temporary table
    }

    /*
      If row-based replication is used and the table is not a
      temporary table, we add the table name to the drop statement
      being built.  The string always end in a comma and the comma
      will be chopped off before being written to the binary log.
      */
    if (thd->current_stmt_binlog_row_based && !dont_log_query)
    {
      non_temp_tables_count++;
      /*
        Don't write the database name if it is the current one (or if
        thd->db is NULL).
      */
      built_query.append("`");
      if (thd->db == NULL || strcmp(db,thd->db) != 0)
      {
        built_query.append(db);
        built_query.append("`.`");
      }

      built_query.append(table->table_name);
      built_query.append("`,");
    }

    error=0;
    table_type= table->db_type;
    if (!drop_temporary)
    {
      TABLE *locked_table;
      abort_locked_tables(thd, db, table->table_name);
      remove_table_from_cache(thd, db, table->table_name,
	                      RTFC_WAIT_OTHER_THREAD_FLAG |
			      RTFC_CHECK_KILLED_FLAG);
      /*
        If the table was used in lock tables, remember it so that
        unlock_table_names can free it
      */
      if ((locked_table= drop_locked_tables(thd, db, table->table_name)))
        table->table= locked_table;

      if (thd->killed)
      {
        thd->no_warnings_for_error= 0;
	DBUG_RETURN(-1);
      }
      alias= (lower_case_table_names == 2) ? table->alias : table->table_name;
      /* remove .frm file and engine files */
      path_length= build_table_filename(path, sizeof(path),
                                        db, alias, reg_ext, 0);
    }
    if (drop_temporary ||
        (table_type == NULL &&        
         (access(path, F_OK) &&
          ha_create_table_from_engine(thd, db, alias)) ||
         (!drop_view &&
          mysql_frm_type(thd, path, &frm_db_type) != FRMTYPE_TABLE)))
    {
      // Table was not found on disk and table can't be created from engine
      if (if_exists)
	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			    ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
			    table->table_name);
      else
        error= 1;
    }
    else
    {
      char *end;
      if (table_type == NULL)
      {
	mysql_frm_type(thd, path, &frm_db_type);
        table_type= ha_resolve_by_legacy_type(thd, frm_db_type);
      }
      // Remove extension for delete
      *(end= path + path_length - reg_ext_length)= '\0';
      error= ha_delete_table(thd, table_type, path, db, table->table_name,
                             !dont_log_query);
      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) && 
	  (if_exists || table_type == NULL))
	error= 0;
      if (error == HA_ERR_ROW_IS_REFERENCED)
      {
	/* the table is referenced by a foreign key constraint */
	foreign_key_error=1;
      }
      if (!error || error == ENOENT || error == HA_ERR_NO_SUCH_TABLE)
      {
        int new_error;
	/* Delete the table definition file */
	strmov(end,reg_ext);
	if (!(new_error=my_delete(path,MYF(MY_WME))))
        {
	  some_tables_deleted=1;
          new_error= Table_triggers_list::drop_all_triggers(thd, db,
                                                            table->table_name);
        }
        error|= new_error;
      }
    }
    if (error)
    {
      if (wrong_tables.length())
	wrong_tables.append(',');
      wrong_tables.append(String(table->table_name,system_charset_info));
    }
  }
  thd->tmp_table_used= tmp_table_deleted;
  error= 0;
  if (wrong_tables.length())
  {
    if (!foreign_key_error)
      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      wrong_tables.c_ptr());
    else
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    error= 1;
  }

  if (some_tables_deleted || tmp_table_deleted || !error)
  {
    query_cache_invalidate3(thd, tables, 0);
    if (!dont_log_query)
    {
      if (!thd->current_stmt_binlog_row_based ||
          non_temp_tables_count > 0 && !tmp_table_deleted)
      {
        /*
          In this case, we are either using statement-based
          replication or using row-based replication but have only
          deleted one or more non-temporary tables (and no temporary
          tables).  In this case, we can write the original query into
          the binary log.
         */
        write_bin_log(thd, !error, thd->query, thd->query_length);
      }
      else if (thd->current_stmt_binlog_row_based &&
               non_temp_tables_count > 0 &&
               tmp_table_deleted)
      {
        /*
          In this case we have deleted both temporary and
          non-temporary tables, so:
          - since we have deleted a non-temporary table we have to
            binlog the statement, but
          - since we have deleted a temporary table we cannot binlog
            the statement (since the table has not been created on the
            slave, this might cause the slave to stop).

          Instead, we write a built statement, only containing the
          non-temporary tables, to the binary log
        */
        built_query.chop();                  // Chop of the last comma
        built_query.append(" /* generated by server */");
        write_bin_log(thd, !error, built_query.ptr(), built_query.length());
      }
      /*
        The remaining cases are:
        - no tables where deleted and
        - only temporary tables where deleted and row-based
          replication is used.
        In both these cases, nothing should be written to the binary
        log.
      */
    }
  }

  unlock_table_names(thd, tables, (TABLE_LIST*) 0);
  thd->no_warnings_for_error= 0;
  DBUG_RETURN(error);
}


/*
  Quickly remove a table.

  SYNOPSIS
    quick_rm_table()
      base                      The handlerton handle.
      db                        The database name.
      table_name                The table name.
      flags                     flags for build_table_filename().

  RETURN
    0           OK
    != 0        Error
*/

bool quick_rm_table(handlerton *base,const char *db,
                    const char *table_name, uint flags)
{
  char path[FN_REFLEN];
  bool error= 0;
  DBUG_ENTER("quick_rm_table");

  uint path_length= build_table_filename(path, sizeof(path),
                                         db, table_name, reg_ext, flags);
  if (my_delete(path,MYF(0)))
    error= 1; /* purecov: inspected */
  path[path_length - reg_ext_length]= '\0'; // Remove reg_ext
  DBUG_RETURN(ha_delete_table(current_thd, base, path, db, table_name, 0) ||
              error);
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keyws where all column are NOT NULL
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KEY *a, KEY *b)
{
  if (a->flags & HA_NOSAME)
  {
    if (!(b->flags & HA_NOSAME))
      return -1;
    if ((a->flags ^ b->flags) & (HA_NULL_PART_KEY | HA_END_SPACE_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a->flags & (HA_NULL_PART_KEY | HA_END_SPACE_KEY)) ? 1 : -1;
    }
    if (a->name == primary_key_name)
      return -1;
    if (b->name == primary_key_name)
      return 1;
  }
  else if (b->flags & HA_NOSAME)
    return 1;					// Prefer b

  if ((a->flags ^ b->flags) & HA_FULLTEXT)
  {
    return (a->flags & HA_FULLTEXT) ? 1 : -1;
  }
  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
	  (a->usable_key_parts > b->usable_key_parts) ? 1 :
	  0);
}

/*
  Check TYPELIB (set or enum) for duplicates

  SYNOPSIS
    check_duplicates_in_interval()
    set_or_name   "SET" or "ENUM" string for warning message
    name	  name of the checked column
    typelib	  list of values for the column

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    void
*/

void check_duplicates_in_interval(const char *set_or_name,
                                  const char *name, TYPELIB *typelib,
                                  CHARSET_INFO *cs)
{
  TYPELIB tmp= *typelib;
  const char **cur_value= typelib->type_names;
  unsigned int *cur_length= typelib->type_lengths;
  
  for ( ; tmp.count > 1; cur_value++, cur_length++)
  {
    tmp.type_names++;
    tmp.type_lengths++;
    tmp.count--;
    if (find_type2(&tmp, (const char*)*cur_value, *cur_length, cs))
    {
      push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DUPLICATED_VALUE_IN_TYPE,
			  ER(ER_DUPLICATED_VALUE_IN_TYPE),
			  name,*cur_value,set_or_name);
    }
  }
}


/*
  Check TYPELIB (set or enum) max and total lengths

  SYNOPSIS
    calculate_interval_lengths()
    cs            charset+collation pair of the interval
    typelib       list of values for the column
    max_length    length of the longest item
    tot_length    sum of the item lengths

  DESCRIPTION
    After this function call:
    - ENUM uses max_length
    - SET uses tot_length.

  RETURN VALUES
    void
*/
void calculate_interval_lengths(CHARSET_INFO *cs, TYPELIB *interval,
                                uint32 *max_length, uint32 *tot_length)
{
  const char **pos;
  uint *len;
  *max_length= *tot_length= 0;
  for (pos= interval->type_names, len= interval->type_lengths;
       *pos ; pos++, len++)
  {
    uint length= cs->cset->numchars(cs, *pos, *pos + *len);
    *tot_length+= length;
    set_if_bigger(*max_length, (uint32)length);
  }
}


/*
  Prepare a create_table instance for packing

  SYNOPSIS
    prepare_create_field()
    sql_field     field to prepare for packing
    blob_columns  count for BLOBs
    timestamps    count for timestamps
    table_flags   table flags

  DESCRIPTION
    This function prepares a create_field instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/

int prepare_create_field(create_field *sql_field, 
			 uint *blob_columns, 
			 int *timestamps, int *timestamps_with_niladic,
			 uint table_flags)
{
  DBUG_ENTER("prepare_field");

  /*
    This code came from mysql_prepare_table.
    Indent preserved to make patching easier
  */
  DBUG_ASSERT(sql_field->charset);

  switch (sql_field->sql_type) {
  case FIELD_TYPE_BLOB:
  case FIELD_TYPE_MEDIUM_BLOB:
  case FIELD_TYPE_TINY_BLOB:
  case FIELD_TYPE_LONG_BLOB:
    sql_field->pack_flag=FIELDFLAG_BLOB |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
  case FIELD_TYPE_GEOMETRY:
#ifdef HAVE_SPATIAL
    if (!(table_flags & HA_CAN_GEOMETRY))
    {
      my_printf_error(ER_CHECK_NOT_IMPLEMENTED, ER(ER_CHECK_NOT_IMPLEMENTED),
                      MYF(0), "GEOMETRY");
      DBUG_RETURN(1);
    }
    sql_field->pack_flag=FIELDFLAG_GEOM |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
#else
    my_printf_error(ER_FEATURE_DISABLED,ER(ER_FEATURE_DISABLED), MYF(0),
                    sym_group_geom.name, sym_group_geom.needed_define);
    DBUG_RETURN(1);
#endif /*HAVE_SPATIAL*/
  case MYSQL_TYPE_VARCHAR:
#ifndef QQ_ALL_HANDLERS_SUPPORT_VARCHAR
    if (table_flags & HA_NO_VARCHAR)
    {
      /* convert VARCHAR to CHAR because handler is not yet up to date */
      sql_field->sql_type=    MYSQL_TYPE_VAR_STRING;
      sql_field->pack_length= calc_pack_length(sql_field->sql_type,
                                               (uint) sql_field->length);
      if ((sql_field->length / sql_field->charset->mbmaxlen) >
          MAX_FIELD_CHARLENGTH)
      {
        my_printf_error(ER_TOO_BIG_FIELDLENGTH, ER(ER_TOO_BIG_FIELDLENGTH),
                        MYF(0), sql_field->field_name, MAX_FIELD_CHARLENGTH);
        DBUG_RETURN(1);
      }
    }
#endif
    /* fall through */
  case FIELD_TYPE_STRING:
    sql_field->pack_flag=0;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    break;
  case FIELD_TYPE_ENUM:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_INTERVAL;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::INTERVAL_FIELD;
    check_duplicates_in_interval("ENUM",sql_field->field_name,
                                 sql_field->interval,
                                 sql_field->charset);
    break;
  case FIELD_TYPE_SET:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_BITFIELD;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::BIT_FIELD;
    check_duplicates_in_interval("SET",sql_field->field_name,
                                 sql_field->interval,
                                 sql_field->charset);
    break;
  case FIELD_TYPE_DATE:			// Rest of string types
  case FIELD_TYPE_NEWDATE:
  case FIELD_TYPE_TIME:
  case FIELD_TYPE_DATETIME:
  case FIELD_TYPE_NULL:
    sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
    break;
  case FIELD_TYPE_BIT:
    /* 
      We have sql_field->pack_flag already set here, see mysql_prepare_table().
    */
    break;
  case FIELD_TYPE_NEWDECIMAL:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & ZEROFILL_FLAG ?
                           FIELDFLAG_ZEROFILL : 0) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  case FIELD_TYPE_TIMESTAMP:
    /* We should replace old TIMESTAMP fields with their newer analogs */
    if (sql_field->unireg_check == Field::TIMESTAMP_OLD_FIELD)
    {
      if (!*timestamps)
      {
        sql_field->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
        (*timestamps_with_niladic)++;
      }
      else
        sql_field->unireg_check= Field::NONE;
    }
    else if (sql_field->unireg_check != Field::NONE)
      (*timestamps_with_niladic)++;

    (*timestamps)++;
    /* fall-through */
  default:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & ZEROFILL_FLAG ?
                           FIELDFLAG_ZEROFILL : 0) |
                          f_settype((uint) sql_field->sql_type) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  }
  if (!(sql_field->flags & NOT_NULL_FLAG))
    sql_field->pack_flag|= FIELDFLAG_MAYBE_NULL;
  if (sql_field->flags & NO_DEFAULT_VALUE_FLAG)
    sql_field->pack_flag|= FIELDFLAG_NO_DEFAULT;
  DBUG_RETURN(0);
}

/*
  Preparation for table creation

  SYNOPSIS
    mysql_prepare_table()
      thd                       Thread object.
      create_info               Create information (like MAX_ROWS).
      fields                    List of fields to create.
      keys                      List of keys to create.
      tmp_table                 If a temporary table is to be created.
      db_options          INOUT Table options (like HA_OPTION_PACK_RECORD).
      file                      The handler for the new table.
      key_info_buffer     OUT   An array of KEY structs for the indexes.
      key_count           OUT   The number of elements in the array.
      select_field_count        The number of fields coming from a select table.

  DESCRIPTION
    Prepares the table and key structures for table creation.

  NOTES
    sets create_info->varchar if the table has a varchar

  RETURN VALUES
    0	ok
    -1	error
*/

static int mysql_prepare_table(THD *thd, HA_CREATE_INFO *create_info,
                               List<create_field> *fields,
                               List<Key> *keys, bool tmp_table,
                               uint *db_options,
                               handler *file, KEY **key_info_buffer,
                               uint *key_count, int select_field_count)
{
  const char	*key_name;
  create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<create_field> it(*fields),it2(*fields);
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("mysql_prepare_table");

  select_field_pos= fields->elements - select_field_count;
  null_fields=blob_columns=0;
  create_info->varchar= 0;
  max_key_length= file->max_key_length();

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    CHARSET_INFO *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;
    if (!sql_field->charset)
      sql_field->charset= create_info->default_table_charset;
    /*
      table_charset is set in ALTER TABLE if we want change character set
      for all varchar/char columns.
      But the table charset must not affect the BLOB fields, so don't
      allow to change my_charset_bin to somethig else.
    */
    if (create_info->table_charset && sql_field->charset != &my_charset_bin)
      sql_field->charset= create_info->table_charset;

    save_cs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
	!(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
						    MY_CS_BINSORT,MYF(0))))
    {
      char tmp[64];
      strmake(strmake(tmp, save_cs->csname, sizeof(tmp)-4),
              STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      DBUG_RETURN(-1);
    }

    if (sql_field->sql_type == FIELD_TYPE_SET ||
        sql_field->sql_type == FIELD_TYPE_ENUM)
    {
      uint32 dummy;
      CHARSET_INFO *cs= sql_field->charset;
      TYPELIB *interval= sql_field->interval;

      /*
        Create typelib from interval_list, and if necessary
        convert strings from client character set to the
        column character set.
      */
      if (!interval)
      {
        /*
          Create the typelib in prepared statement memory if we're
          executing one.
        */
        MEM_ROOT *stmt_root= thd->stmt_arena->mem_root;

        interval= sql_field->interval= typelib(stmt_root,
                                               sql_field->interval_list);
        List_iterator<String> it(sql_field->interval_list);
        String conv, *tmp;
        char comma_buf[2];
        int comma_length= cs->cset->wc_mb(cs, ',', (uchar*) comma_buf,
                                          (uchar*) comma_buf + 
                                          sizeof(comma_buf));
        DBUG_ASSERT(comma_length > 0);
        for (uint i= 0; (tmp= it++); i++)
        {
          uint lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy))
          {
            uint cnv_errs;
            conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);
            interval->type_names[i]= strmake_root(stmt_root, conv.ptr(),
                                                  conv.length());
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i],
                                       interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((uchar *)interval->type_names[i])[lengthsp]= '\0';
          if (sql_field->sql_type == FIELD_TYPE_SET)
          {
            if (cs->coll->instr(cs, interval->type_names[i], 
                                interval->type_lengths[i], 
                                comma_buf, comma_length, NULL, 0))
            {
              my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", tmp->ptr());
              DBUG_RETURN(-1);
            }
          }
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      /*
        Convert the default value from client character
        set into the column character set if necessary.
      */
      if (sql_field->def && cs != sql_field->def->collation.collation)
      {
        Query_arena backup_arena;
        bool need_to_change_arena= !thd->stmt_arena->is_conventional();
        if (need_to_change_arena)
        {
          /* Asser that we don't do that at every PS execute */
          DBUG_ASSERT(thd->stmt_arena->is_first_stmt_execute() ||
                      thd->stmt_arena->is_first_sp_execute());
          thd->set_n_backup_active_arena(thd->stmt_arena, &backup_arena);
        }

        sql_field->def= sql_field->def->safe_charset_converter(cs);

        if (need_to_change_arena)
          thd->restore_active_arena(thd->stmt_arena, &backup_arena);

        if (sql_field->def == NULL)
        {
          /* Could not convert */
          my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
          DBUG_RETURN(-1);
        }
      }

      if (sql_field->sql_type == FIELD_TYPE_SET)
      {
        uint32 field_length;
        if (sql_field->def != NULL)
        {
          char *not_used;
          uint not_used2;
          bool not_found= 0;
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }

            /* else, NULL is an allowed value */
            (void) find_set(interval, NULL, 0,
                            cs, &not_used, &not_used2, &not_found);
          }
          else /* not NULL */
          {
            (void) find_set(interval, def->ptr(), def->length(),
                            cs, &not_used, &not_used2, &not_found);
          }

          if (not_found)
          {
            my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
            DBUG_RETURN(-1);
          }
        }
        calculate_interval_lengths(cs, interval, &dummy, &field_length);
        sql_field->length= field_length + (interval->count - 1);
      }
      else  /* FIELD_TYPE_ENUM */
      {
        uint32 field_length;
        DBUG_ASSERT(sql_field->sql_type == FIELD_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }

            /* else, the defaults yield the correct length for NULLs. */
          } 
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (find_type2(interval, def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }
          }
        }
        calculate_interval_lengths(cs, interval, &field_length, &dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
    }

    if (sql_field->sql_type == FIELD_TYPE_BIT)
    { 
      sql_field->pack_flag= FIELDFLAG_NUMBER;
      if (file->ha_table_flags() & HA_CAN_BIT_FIELD)
        total_uneven_bit_length+= sql_field->length & 7;
      else
        sql_field->pack_flag|= FIELDFLAG_TREAT_BIT_AS_CHAR;
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(thd, sql_field))
      DBUG_RETURN(-1);

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      DBUG_RETURN(-1);
    }

    /* Check if we have used the same field name before */
    for (dup_no=0; (dup_field=it2++) != sql_field; dup_no++)
    {
      if (my_strcasecmp(system_charset_info,
			sql_field->field_name,
			dup_field->field_name) == 0)
      {
	/*
	  If this was a CREATE ... SELECT statement, accept a field
	  redefinition if we are changing a field in the SELECT part
	*/
	if (field_no < select_field_pos || dup_no >= select_field_pos)
	{
	  my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
	  DBUG_RETURN(-1);
	}
	else
	{
	  /* Field redefined */
	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
	  sql_field->create_length_to_internal_length();
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->unireg_check=	dup_field->unireg_check;
          /* 
            We're making one field from two, the result field will have
            dup_field->flags as flags. If we've incremented null_fields
            because of sql_field->flags, decrement it back.
          */
          if (!(sql_field->flags & NOT_NULL_FLAG))
            null_fields--;
	  sql_field->flags=		dup_field->flags;
          sql_field->interval=          dup_field->interval;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }
    /* Don't pack rows in old tables if the user has requested this */
    if ((sql_field->flags & BLOB_FLAG) ||
	sql_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED)
      (*db_options)|= HA_OPTION_PACK_RECORD;
    it2.rewind();
  }

  /* record_offset will be increased with 'length-of-null-bits' later */
  record_offset= 0;
  null_fields+= total_uneven_bit_length;

  it.rewind();
  while ((sql_field=it++))
  {
    DBUG_ASSERT(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns, 
			     &timestamps, &timestamps_with_niladic,
			     file->ha_table_flags()))
      DBUG_RETURN(-1);
    if (sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      create_info->varchar= 1;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    record_offset+= sql_field->pack_length;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment &&
      (file->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    DBUG_RETURN(-1);
  }

  if (blob_columns && (file->ha_table_flags() & HA_NO_BLOBS))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    DBUG_RETURN(-1);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(*keys), key_iterator2(*keys);
  uint key_parts=0, fk_key_count=0;
  bool primary_key=0,unique_key=0;
  Key *key, *key2;
  uint tmp, key_number;
  /* special marker for keys to be ignored */
  static char ignore_key[1];

  /* Calculate number of key segements */
  *key_count= 0;

  while ((key=key_iterator++))
  {
    DBUG_PRINT("info", ("key name: '%s'  type: %d", key->name ? key->name :
                        "(none)" , key->type));
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      foreign_key *fk_key= (foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name ?  fk_key->name : "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(-1);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(-1);
    }
    if (key->name && strlen(key->name) > NAME_LEN)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
    key_iterator2.rewind ();
    if (key->type != Key::FOREIGN_KEY)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.
        */
        if ((key2->type != Key::FOREIGN_KEY &&
             key2->name != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->name= ignore_key;
          else
          {
            key2->name= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->name && !tmp_table && (key->type != Key::PRIMARY) &&
	!my_strcasecmp(system_charset_info,key->name,primary_key_name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
  }
  tmp=file->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    DBUG_RETURN(-1);
  }

  (*key_info_buffer)= key_info= (KEY*) sql_calloc(sizeof(KEY) * (*key_count));
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!*key_info_buffer || ! key_part_info)
    DBUG_RETURN(-1);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint key_length=0;
    key_part_spec *column;

    if (key->name == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name == ignore_key);
      if (!key)
	break;
    }

    switch (key->type) {
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FULLTEXT:
	key_info->flags= HA_FULLTEXT;
	if ((key_info->parser_name= &key->key_create_info.parser_name)->str)
          key_info->flags|= HA_USES_PARSER;
        else
          key_info->parser_name= 0;
	break;
    case Key::SPATIAL:
#ifdef HAVE_SPATIAL
	key_info->flags= HA_SPATIAL;
	break;
#else
	my_error(ER_FEATURE_DISABLED, MYF(0),
                 sym_group_geom.name, sym_group_geom.needed_define);
	DBUG_RETURN(-1);
#endif
    case Key::FOREIGN_KEY:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->key_parts=(uint8) key->columns.elements;
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;
    key_info->algorithm= key->key_create_info.algorithm;

    if (key->type == Key::FULLTEXT)
    {
      if (!(file->ha_table_flags() & HA_CAN_FULLTEXT))
      {
	my_message(ER_TABLE_CANT_HANDLE_FT, ER(ER_TABLE_CANT_HANDLE_FT),
                   MYF(0));
	DBUG_RETURN(-1);
      }
    }
    /*
       Make SPATIAL to be RTREE by default
       SPATIAL only on BLOB or at least BINARY, this
       actually should be replaced by special GEOM type
       in near future when new frm file is ready
       checking for proper key parts number:
    */

    /* TODO: Add proper checks if handler supports key_type and algorithm */
    if (key_info->flags & HA_SPATIAL)
    {
      if (!(file->ha_table_flags() & HA_CAN_RTREEKEYS))
      {
        my_message(ER_TABLE_CANT_HANDLE_SPKEYS, ER(ER_TABLE_CANT_HANDLE_SPKEYS),
                   MYF(0));
        DBUG_RETURN(-1);
      }
      if (key_info->key_parts != 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "SPATIAL INDEX");
	DBUG_RETURN(-1);
      }
    }
    else if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
#ifdef HAVE_RTREE_KEYS
      if ((key_info->key_parts & 1) == 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "RTREE INDEX");
	DBUG_RETURN(-1);
      }
      /* TODO: To be deleted */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
      DBUG_RETURN(-1);
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               sym_group_rtree.name, sym_group_rtree.needed_define);
      DBUG_RETURN(-1);
#endif
    }

    /* Take block size from key part or table part */
    /*
      TODO: Add warning if block size changes. We can't do it here, as
      this may depend on the size of the key
    */
    key_info->block_size= (key->key_create_info.block_size ?
                           key->key_create_info.block_size :
                           create_info->key_block_size);

    if (key_info->block_size)
      key_info->flags|= HA_USES_BLOCK_SIZE;

    List_iterator<key_part_spec> cols(key->columns), cols2(key->columns);
    CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint length;
      key_part_spec *dup_column;

      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(system_charset_info,
			   column->field_name,
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name);
	DBUG_RETURN(-1);
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->field_name, dup_column->field_name))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name);
	  DBUG_RETURN(-1);
	}
      }
      cols2.rewind();
      if (key->type == Key::FULLTEXT)
      {
	if ((sql_field->sql_type != MYSQL_TYPE_STRING &&
	     sql_field->sql_type != MYSQL_TYPE_VARCHAR &&
	     !f_is_blob(sql_field->pack_flag)) ||
	    sql_field->charset == &my_charset_bin ||
	    sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
	    (ft_key_charset && sql_field->charset != ft_key_charset))
	{
	    my_error(ER_BAD_FT_COLUMN, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	}
	ft_key_charset=sql_field->charset;
	/*
	  for fulltext keys keyseg length is 1 for blobs (it's ignored in ft
	  code anyway, and 0 (set to column width later) for char's. it has
	  to be correct col width for char's, as char data are not prefixed
	  with length (unlike blobs, where ft code takes data length from a
	  data prefix, ignoring column->length).
	*/
	column->length=test(f_is_blob(sql_field->pack_flag));
      }
      else
      {
	column->length*= sql_field->charset->mbmaxlen;

	if (f_is_blob(sql_field->pack_flag) ||
            (f_is_geom(sql_field->pack_flag) && key->type != Key::SPATIAL))
	{
	  if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	  }
          if (f_is_geom(sql_field->pack_flag) && sql_field->geom_type ==
              Field::GEOM_POINT)
            column->length= 21;
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	  }
	}
#ifdef HAVE_SPATIAL
	if (key->type == Key::SPATIAL)
	{
	  if (!column->length)
	  {
	    /*
              4 is: (Xmin,Xmax,Ymin,Ymax), this is for 2D case
              Lately we'll extend this code to support more dimensions
	    */
	    column->length= 4*sizeof(double);
	  }
	}
#endif
	if (!(sql_field->flags & NOT_NULL_FLAG))
	{
	  if (key->type == Key::PRIMARY)
	  {
	    /* Implicitly set primary key fields to NOT NULL for ISO conf. */
	    sql_field->flags|= NOT_NULL_FLAG;
	    sql_field->pack_flag&= ~FIELDFLAG_MAYBE_NULL;
            null_fields--;
	  }
	  else
          {
            key_info->flags|= HA_NULL_PART_KEY;
            if (!(file->ha_table_flags() & HA_NULL_IN_KEY))
            {
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name);
              DBUG_RETURN(-1);
            }
            if (key->type == Key::SPATIAL)
            {
              my_message(ER_SPATIAL_CANT_HAVE_NULL,
                         ER(ER_SPATIAL_CANT_HAVE_NULL), MYF(0));
              DBUG_RETURN(-1);
            }
          }
	}
	if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
	{
	  if (column_nr == 0 || (file->ha_table_flags() & HA_AUTO_PART_KEY))
	    auto_increment--;			// Field is used
	}
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      length= sql_field->key_length;

      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
	  if ((length=column->length) > max_key_length ||
	      length > file->max_key_part_length())
	  {
	    length=min(max_key_length, file->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[MYSQL_ERRMSG_SIZE];
	      my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
			  length);
	      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      DBUG_RETURN(-1);
	    }
	  }
	}
	else if (!f_is_geom(sql_field->pack_flag) &&
		  (column->length > length ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  DBUG_RETURN(-1);
	}
	else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name);
	  DBUG_RETURN(-1);
      }
      if (length > file->max_key_part_length() && key->type != Key::FULLTEXT)
      {
        length= file->max_key_part_length();
        /* Align key length to multibyte char boundary */
        length-= length % sql_field->charset->mbmaxlen;
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
		      length);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  DBUG_RETURN(-1);
	}
      }
      key_part_info->length=(uint16) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == MYSQL_TYPE_STRING ||
	    sql_field->sql_type == MYSQL_TYPE_VARCHAR ||
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if (column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB) ||
            sql_field->sql_type == MYSQL_TYPE_VARCHAR)
	  key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
	else
	  key_info->flags|= HA_PACK_KEY;
      }
      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_message(ER_MULTIPLE_PRI_KEY, ER(ER_MULTIPLE_PRI_KEY),
                       MYF(0));
	    DBUG_RETURN(-1);
	  }
	  key_name=primary_key_name;
	  primary_key=1;
	}
	else if (!(key_name = key->name))
	  key_name=make_unique_key_name(sql_field->field_name,
					*key_info_buffer, key_info);
	if (check_if_keyname_exists(key_name, *key_info_buffer, key_info))
	{
	  my_error(ER_DUP_KEYNAME, MYF(0), key_name);
	  DBUG_RETURN(-1);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(-1);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    if (key_length > max_key_length && key->type != Key::FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      DBUG_RETURN(-1);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (file->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  /* Sort keys in optimized order */
  qsort((gptr) *key_info_buffer, *key_count, sizeof(KEY),
	(qsort_cmp) sort_keys);
  create_info->null_bits= null_fields;

  DBUG_RETURN(0);
}


/*
  Set table default charset, if not set

  SYNOPSIS
    set_table_default_charset()
    create_info        Table create information

  DESCRIPTION
    If the table character set was not given explicitely,
    let's fetch the database default character set and
    apply it to the table.
*/

static void set_table_default_charset(THD *thd,
				      HA_CREATE_INFO *create_info, char *db)
{
  if (!create_info->default_table_charset)
  {
    HA_CREATE_INFO db_info;
    char path[FN_REFLEN];
    /* Abuse build_table_filename() to build the path to the db.opt file */
    build_table_filename(path, sizeof(path), db, "", MY_DB_OPT_FILE, 0);
    load_db_opt(thd, path, &db_info);
    create_info->default_table_charset= db_info.default_table_charset;
  }
}


/*
  Extend long VARCHAR fields to blob & prepare field if it's a blob

  SYNOPSIS
    prepare_blob_field()
    sql_field		Field to check

  RETURN
    0	ok
    1	Error (sql_field can't be converted to blob)
        In this case the error is given
*/

static bool prepare_blob_field(THD *thd, create_field *sql_field)
{
  DBUG_ENTER("prepare_blob_field");

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    /* Convert long VARCHAR columns to TEXT or BLOB */
    char warn_buff[MYSQL_ERRMSG_SIZE];

    if (sql_field->def || (thd->variables.sql_mode & (MODE_STRICT_TRANS_TABLES |
                                                      MODE_STRICT_ALL_TABLES)))
    {
      my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
               MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
      DBUG_RETURN(1);
    }
    sql_field->sql_type= FIELD_TYPE_BLOB;
    sql_field->flags|= BLOB_FLAG;
    sprintf(warn_buff, ER(ER_AUTO_CONVERT), sql_field->field_name,
            (sql_field->charset == &my_charset_bin) ? "VARBINARY" : "VARCHAR",
            (sql_field->charset == &my_charset_bin) ? "BLOB" : "TEXT");
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_AUTO_CONVERT,
                 warn_buff);
  }
    
  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == FIELD_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->sql_type= get_blob_type_from_length(sql_field->length);
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  DBUG_RETURN(0);
}


/*
  Preparation of create_field for SP function return values.
  Based on code used in the inner loop of mysql_prepare_table() above

  SYNOPSIS
    sp_prepare_create_field()
    thd			Thread object
    sql_field		Field to prepare

  DESCRIPTION
    Prepares the field structures for field creation.

*/

void sp_prepare_create_field(THD *thd, create_field *sql_field)
{
  if (sql_field->sql_type == FIELD_TYPE_SET ||
      sql_field->sql_type == FIELD_TYPE_ENUM)
  {
    uint32 field_length, dummy;
    if (sql_field->sql_type == FIELD_TYPE_SET)
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval, &dummy, 
                                 &field_length);
      sql_field->length= field_length + 
                         (sql_field->interval->count - 1);
    }
    else /* FIELD_TYPE_ENUM */
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval,
                                 &field_length, &dummy);
      sql_field->length= field_length;
    }
    set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
  }

  if (sql_field->sql_type == FIELD_TYPE_BIT)
  {
    sql_field->pack_flag= FIELDFLAG_NUMBER |
                          FIELDFLAG_TREAT_BIT_AS_CHAR;
  }
  sql_field->create_length_to_internal_length();
  DBUG_ASSERT(sql_field->def == 0);
  /* Can't go wrong as sql_field->def is not defined */
  (void) prepare_blob_field(thd, sql_field);
}


/*
  Copy HA_CREATE_INFO struct
  SYNOPSIS
    copy_create_info()
    lex_create_info         The create_info struct setup by parser
  RETURN VALUES
    > 0                     A pointer to a copy of the lex_create_info
    0                       Memory allocation error
  DESCRIPTION
  Allocate memory for copy of HA_CREATE_INFO structure from parser
  to ensure we can reuse the parser struct in stored procedures
  and prepared statements.
*/

static HA_CREATE_INFO *copy_create_info(HA_CREATE_INFO *lex_create_info)
{
  HA_CREATE_INFO *create_info;
  if (!(create_info= (HA_CREATE_INFO*)sql_alloc(sizeof(HA_CREATE_INFO))))
    mem_alloc_error(sizeof(HA_CREATE_INFO));
  else
    memcpy((void*)create_info, (void*)lex_create_info, sizeof(HA_CREATE_INFO));
  return create_info;
}


/*
  Create a table

  SYNOPSIS
    mysql_create_table_internal()
    thd			Thread object
    db			Database
    table_name		Table name
    lex_create_info	Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create
    internal_tmp_table  Set to 1 if this is an internal temporary table
			(From ALTER TABLE)
    select_field_count  
    use_copy_create_info Should we make a copy of create info (we do this
                         when this is called from sql_parse.cc where we
                         want to ensure lex object isn't manipulated.

  DESCRIPTION
    If one creates a temporary table, this is automatically opened

    no_log is needed for the case of CREATE ... SELECT,
    as the logging will be done later in sql_insert.cc
    select_field_count is also used for CREATE ... SELECT,
    and must be zero for standard create of table.

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_table_internal(THD *thd,
                                const char *db, const char *table_name,
                                HA_CREATE_INFO *lex_create_info,
                                List<create_field> &fields,
                                List<Key> &keys,bool internal_tmp_table,
                                uint select_field_count,
                                bool use_copy_create_info)
{
  char		path[FN_REFLEN];
  uint          path_length;
  const char	*alias;
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  HA_CREATE_INFO *create_info;
  handler	*file;
  bool		error= TRUE;
  DBUG_ENTER("mysql_create_table_internal");
  DBUG_PRINT("enter", ("db: '%s'  table: '%s'  tmp: %d",
                       db, table_name, internal_tmp_table));

  if (use_copy_create_info)
  {
    if (!(create_info= copy_create_info(lex_create_info)))
    {
      DBUG_RETURN(TRUE);
    }
  }
  else
    create_info= lex_create_info;
 
  /* Check for duplicate fields and check type of table to create */
  if (!fields.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (check_engine(thd, table_name, create_info))
    DBUG_RETURN(TRUE);
  db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  alias= table_case_name(create_info, table_name);
  if (!(file= get_new_handler((TABLE_SHARE*) 0, thd->mem_root,
                              create_info->db_type)))
  {
    mem_alloc_error(sizeof(handler));
    DBUG_RETURN(TRUE);
  }
#ifdef WITH_PARTITION_STORAGE_ENGINE
  partition_info *part_info= thd->work_part_info;

  if (!part_info && create_info->db_type->partition_flags &&
      (create_info->db_type->partition_flags() & HA_USE_AUTO_PARTITION))
  {
    /*
      Table is not defined as a partitioned table but the engine handles
      all tables as partitioned. The handler will set up the partition info
      object with the default settings.
    */
    thd->work_part_info= part_info= new partition_info();
    if (!part_info)
    {
      mem_alloc_error(sizeof(partition_info));
      DBUG_RETURN(TRUE);
    }
    file->set_auto_partitions(part_info);
    part_info->default_engine_type= create_info->db_type;
    part_info->is_auto_partitioned= TRUE;
  }
  if (part_info)
  {
    /*
      The table has been specified as a partitioned table.
      If this is part of an ALTER TABLE the handler will be the partition
      handler but we need to specify the default handler to use for
      partitions also in the call to check_partition_info. We transport
      this information in the default_db_type variable, it is either
      DB_TYPE_DEFAULT or the engine set in the ALTER TABLE command.

      Check that we don't use foreign keys in the table since it won't
      work even with InnoDB beneath it.
    */
    List_iterator<Key> key_iterator(keys);
    Key *key;
    handlerton *part_engine_type= create_info->db_type;
    char *part_syntax_buf;
    uint syntax_len;
    handlerton *engine_type;
    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    {
      my_error(ER_PARTITION_NO_TEMPORARY, MYF(0));
      goto err;
    }
    while ((key= key_iterator++))
    {
      if (key->type == Key::FOREIGN_KEY &&
          !part_info->is_auto_partitioned)
      {
        my_error(ER_CANNOT_ADD_FOREIGN, MYF(0));
        goto err;
      }
    }
    if ((part_engine_type == &partition_hton) &&
        part_info->default_engine_type)
    {
      /*
        This only happens at ALTER TABLE.
        default_engine_type was assigned from the engine set in the ALTER
        TABLE command.
      */
      ;
    }
    else
    {
      if (create_info->used_fields & HA_CREATE_USED_ENGINE)
      {
        part_info->default_engine_type= create_info->db_type;
      }
      else
      {
        if (part_info->default_engine_type == NULL)
        {
          part_info->default_engine_type= ha_checktype(thd,
                                          DB_TYPE_DEFAULT, 0, 0);
        }
      }
    }
    DBUG_PRINT("info", ("db_type = %d",
                         ha_legacy_type(part_info->default_engine_type)));
    if (part_info->check_partition_info(thd, &engine_type, file, create_info))
      goto err;
    part_info->default_engine_type= engine_type;

    /*
      We reverse the partitioning parser and generate a standard format
      for syntax stored in frm file.
    */
    if (!(part_syntax_buf= generate_partition_syntax(part_info,
                                                     &syntax_len,
                                                     TRUE, TRUE)))
      goto err;
    part_info->part_info_string= part_syntax_buf;
    part_info->part_info_len= syntax_len;
    if ((!(engine_type->partition_flags &&
           engine_type->partition_flags() & HA_CAN_PARTITION)) ||
        create_info->db_type == &partition_hton)
    {
      /*
        The handler assigned to the table cannot handle partitioning.
        Assign the partition handler as the handler of the table.
      */
      DBUG_PRINT("info", ("db_type: %d",
                          ha_legacy_type(create_info->db_type)));
      delete file;
      create_info->db_type= &partition_hton;
      if (!(file= get_ha_partition(part_info)))
      {
        DBUG_RETURN(TRUE);
      }
      /*
        If we have default number of partitions or subpartitions we
        might require to set-up the part_info object such that it
        creates a proper .par file. The current part_info object is
        only used to create the frm-file and .par-file.
      */
      if (part_info->use_default_no_partitions &&
          part_info->no_parts &&
          (int)part_info->no_parts !=
          file->get_default_no_partitions(create_info))
      {
        uint i;
        List_iterator<partition_element> part_it(part_info->partitions);
        part_it++;
        DBUG_ASSERT(thd->lex->sql_command != SQLCOM_CREATE_TABLE);
        for (i= 1; i < part_info->partitions.elements; i++)
          (part_it++)->part_state= PART_TO_BE_DROPPED;
      }
      else if (part_info->is_sub_partitioned() &&
               part_info->use_default_no_subpartitions &&
               part_info->no_subparts &&
               (int)part_info->no_subparts !=
                 file->get_default_no_partitions(create_info))
      {
        DBUG_ASSERT(thd->lex->sql_command != SQLCOM_CREATE_TABLE);
        part_info->no_subparts= file->get_default_no_partitions(create_info);
      }
    }
    else if (create_info->db_type != engine_type)
    {
      /*
        We come here when we don't use a partitioned handler.
        Since we use a partitioned table it must be "native partitioned".
        We have switched engine from defaults, most likely only specified
        engines in partition clauses.
      */
      delete file;
      if (!(file= get_new_handler((TABLE_SHARE*) 0, thd->mem_root,
                                  engine_type)))
      {
        mem_alloc_error(sizeof(handler));
        DBUG_RETURN(TRUE);
      }
    }
  }
#endif

  set_table_default_charset(thd, create_info, (char*) db);

  if (mysql_prepare_table(thd, create_info, &fields,
			  &keys, internal_tmp_table, &db_options, file,
			  &key_info_buffer, &key_count,
			  select_field_count))
    goto err;

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    path_length= build_tmptable_filename(thd, path, sizeof(path));
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, path);
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else  
  {
 #ifdef FN_DEVCHAR
    /* check if the table name contains FN_DEVCHAR when defined */
    const char *start= alias;
    while (*start != '\0')
    {
      if (*start == FN_DEVCHAR)
      {
        my_error(ER_WRONG_TABLE_NAME, MYF(0), alias);
        DBUG_RETURN(TRUE);
      }
      start++;
    }	  
#endif
    path_length= build_table_filename(path, sizeof(path), db, alias, reg_ext,
                                      internal_tmp_table ? FN_IS_TMP : 0);
  }

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      find_temporary_table(thd, db, table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          alias);
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    goto err;
  }

  VOID(pthread_mutex_lock(&LOCK_open));
  if (!internal_tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (!access(path,F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto unlock_and_end;
    }
    DBUG_ASSERT(get_cached_table_share(db, alias) == 0);
  }

  /*
    Check that table with given name does not already
    exist in any storage engine. In such a case it should
    be discovered and the error ER_TABLE_EXISTS_ERROR be returned
    unless user specified CREATE TABLE IF EXISTS
    The LOCK_open mutex has been locked to make sure no
    one else is attempting to discover the table. Since
    it's not on disk as a frm file, no one could be using it!
  */
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    char dbbuff[FN_REFLEN];
    char tbbuff[FN_REFLEN];
    bool create_if_not_exists =
      create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS;
    VOID(tablename_to_filename(db, dbbuff, sizeof(dbbuff)));
    VOID(tablename_to_filename(table_name, tbbuff, sizeof(tbbuff)));
    if (ha_table_exists_in_engine(thd, dbbuff, tbbuff))
    {
      DBUG_PRINT("info", ("Table with same name already existed in handler"));

      if (create_if_not_exists)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto unlock_and_end;
    }
  }

  thd->proc_info="creating table";
  create_info->table_existed= 0;		// Mark that table is created

  if (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)
    create_info->data_file_name= create_info->index_file_name= 0;
  create_info->table_options=db_options;

  path[path_length - reg_ext_length]= '\0'; // Remove .frm extension
  if (rea_create_table(thd, path, db, table_name, create_info, fields,
                       key_count, key_info_buffer, file))
    goto unlock_and_end;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /* Open table and put in temporary table list */
    if (!(open_temporary_table(thd, path, db, table_name, 1)))
    {
      (void) rm_temporary_table(create_info->db_type, path);
      goto unlock_and_end;
    }
    thd->tmp_table_used= 1;
  }

  /*
    Don't write statement if:
    - It is an internal temporary table,
    - Row-based logging is used and it we are creating a temporary table, or
    - The binary log is not open.
    Otherwise, the statement shall be binlogged.
   */
  if (!internal_tmp_table &&
      (!thd->current_stmt_binlog_row_based ||
       (thd->current_stmt_binlog_row_based &&
        !(create_info->options & HA_LEX_CREATE_TMP_TABLE))))
    write_bin_log(thd, TRUE, thd->query, thd->query_length);
  error= FALSE;
unlock_and_end:
  VOID(pthread_mutex_unlock(&LOCK_open));

err:
  thd->proc_info="After create";
  delete file;
  DBUG_RETURN(error);

warn:
  error= FALSE;
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  create_info->table_existed= 1;		// Mark that table existed
  goto unlock_and_end;
}


/*
  Database locking aware wrapper for mysql_create_table_internal(),
*/

bool mysql_create_table(THD *thd, const char *db, const char *table_name,
                        HA_CREATE_INFO *create_info,
                        List<create_field> &fields,
                        List<Key> &keys,bool internal_tmp_table,
                        uint select_field_count,
                        bool use_copy_create_info)
{
  bool result;
  DBUG_ENTER("mysql_create_table");

  /* Wait for any database locks */
  pthread_mutex_lock(&LOCK_lock_db);
  while (!thd->killed &&
         hash_search(&lock_db_cache,(byte*) db, strlen(db)))
  {
    wait_for_condition(thd, &LOCK_lock_db, &COND_refresh);
    pthread_mutex_lock(&LOCK_lock_db);
  }

  if (thd->killed)
  {
    pthread_mutex_unlock(&LOCK_lock_db);
    DBUG_RETURN(TRUE);
  }
  creating_table++;
  pthread_mutex_unlock(&LOCK_lock_db);

  result= mysql_create_table_internal(thd, db, table_name, create_info,
                                      fields, keys, internal_tmp_table,
                                      select_field_count,
                                      use_copy_create_info);

  pthread_mutex_lock(&LOCK_lock_db);
  if (!--creating_table && creating_database)
    pthread_cond_signal(&COND_refresh);
  pthread_mutex_unlock(&LOCK_lock_db);
  DBUG_RETURN(result);
}


/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(const char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      my_strcasecmp(system_charset_info,field_name,primary_key_name))
    return (char*) field_name;			// Use fieldname
  buff_end=strmake(buff,field_name, sizeof(buff)-4);

  /*
    Only 3 chars + '\0' left, so need to limit to 2 digit
    This is ok as we can't have more than 100 keys anyway
  */
  for (uint i=2 ; i< 100; i++)
  {
    *buff_end= '_';
    int10_to_str(i, buff_end+1, 10);
    if (!check_if_keyname_exists(buff,start,end))
      return sql_strdup(buff);
  }
  return (char*) "not_specified";		// Should never happen
}


/****************************************************************************
** Alter a table definition
****************************************************************************/


/*
  Rename a table.

  SYNOPSIS
    mysql_rename_table()
      base                      The handlerton handle.
      old_db                    The old database name.
      old_name                  The old table name.
      new_db                    The new database name.
      new_name                  The new table name.
      flags                     flags for build_table_filename().
                                FN_FROM_IS_TMP old_name is temporary.
                                FN_TO_IS_TMP   new_name is temporary.

  RETURN
    0           OK
    != 0        Error
*/

bool
mysql_rename_table(handlerton *base, const char *old_db,
                   const char *old_name, const char *new_db,
                   const char *new_name, uint flags)
{
  THD *thd= current_thd;
  char from[FN_REFLEN], to[FN_REFLEN], lc_from[FN_REFLEN], lc_to[FN_REFLEN];
  char *from_base= from, *to_base= to;
  char tmp_name[NAME_LEN+1];
  handler *file;
  int error=0;
  DBUG_ENTER("mysql_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_db, old_name, new_db, new_name));

  file= (base == NULL ? 0 :
         get_new_handler((TABLE_SHARE*) 0, thd->mem_root, base));

  build_table_filename(from, sizeof(from), old_db, old_name, "",
                       flags & FN_FROM_IS_TMP);
  build_table_filename(to, sizeof(to), new_db, new_name, "",
                       flags & FN_TO_IS_TMP);

  /*
    If lower_case_table_names == 2 (case-preserving but case-insensitive
    file system) and the storage is not HA_FILE_BASED, we need to provide
    a lowercase file name, but we leave the .frm in mixed case.
   */
  if (lower_case_table_names == 2 && file &&
      !(file->ha_table_flags() & HA_FILE_BASED))
  {
    strmov(tmp_name, old_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_from, sizeof(lc_from), old_db, tmp_name, "",
                         flags & FN_FROM_IS_TMP);
    from_base= lc_from;

    strmov(tmp_name, new_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_to, sizeof(lc_to), new_db, tmp_name, "",
                         flags & FN_TO_IS_TMP);
    to_base= lc_to;
  }

  if (!file || !(error=file->rename_table(from_base, to_base)))
  {
    if (rename_file_ext(from,to,reg_ext))
    {
      error=my_errno;
      /* Restore old file name */
      if (file)
        file->rename_table(to_base, from_base);
    }
  }
  delete file;
  if (error == HA_ERR_WRONG_COMMAND)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER TABLE");
  else if (error)
    my_error(ER_ERROR_ON_RENAME, MYF(0), from, to, error);
  DBUG_RETURN(error != 0);
}


/*
  Force all other threads to stop using the table

  SYNOPSIS
    wait_while_table_is_used()
    thd			Thread handler
    table		Table to remove from cache
    function		HA_EXTRA_PREPARE_FOR_DELETE if table is to be deleted
			HA_EXTRA_FORCE_REOPEN if table is not be used
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

static void wait_while_table_is_used(THD *thd,TABLE *table,
				     enum ha_extra_function function)
{
  DBUG_ENTER("wait_while_table_is_used");
  DBUG_PRINT("enter", ("table: '%s'  share: 0x%lx  db_stat: %u  version: %u",
                       table->s->table_name.str, (ulong) table->s,
                       table->db_stat, table->s->version));

  VOID(table->file->extra(function));
  /* Mark all tables that are in use as 'old' */
  mysql_lock_abort(thd, table, TRUE);	/* end threads waiting on lock */

  /* Wait until all there are no other threads that has this table open */
  remove_table_from_cache(thd, table->s->db.str,
                          table->s->table_name.str,
                          RTFC_WAIT_OTHER_THREAD_FLAG);
  DBUG_VOID_RETURN;
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    thd			Thread handler
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void close_cached_table(THD *thd, TABLE *table)
{
  DBUG_ENTER("close_cached_table");

  wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_DELETE);
  /* Close lock if this is not got with LOCK TABLES */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  thd->open_tables=unlink_open_table(thd,thd->open_tables,table);

  /* When lock on LOCK_open is freed other threads can continue */
  broadcast_refresh();
  DBUG_VOID_RETURN;
}

static int send_check_errmsg(THD *thd, TABLE_LIST* table,
			     const char* operator_name, const char* errmsg)

{
  Protocol *protocol= thd->protocol;
  protocol->prepare_for_resend();
  protocol->store(table->alias, system_charset_info);
  protocol->store((char*) operator_name, system_charset_info);
  protocol->store(STRING_WITH_LEN("error"), system_charset_info);
  protocol->store(errmsg, system_charset_info);
  thd->clear_error();
  if (protocol->write())
    return -1;
  return 1;
}


static int prepare_for_restore(THD* thd, TABLE_LIST* table,
			       HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("prepare_for_restore");

  if (table->table) // do not overwrite existing tables on restore
  {
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				  "table exists, will not overwrite on restore"
				  ));
  }
  else
  {
    char* backup_dir= thd->lex->backup_dir;
    char src_path[FN_REFLEN], dst_path[FN_REFLEN], uname[FN_REFLEN];
    char* table_name= table->table_name;
    char* db= table->db;

    VOID(tablename_to_filename(table->table_name, uname, sizeof(uname)));

    if (fn_format_relative_to_data_home(src_path, uname, backup_dir, reg_ext))
      DBUG_RETURN(-1); // protect buffer overflow

    build_table_filename(dst_path, sizeof(dst_path),
                         db, table_name, reg_ext, 0);

    if (lock_and_wait_for_table_name(thd,table))
      DBUG_RETURN(-1);

    if (my_copy(src_path, dst_path, MYF(MY_WME)))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed copying .frm file"));
    }
    if (mysql_truncate(thd, table, 1))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed generating table from .frm file"));
    }
  }

  /*
    Now we should be able to open the partially restored table
    to finish the restore in the handler later on
  */
  pthread_mutex_lock(&LOCK_open);
  if (reopen_name_locked_table(thd, table))
  {
    unlock_table_name(thd, table);
    pthread_mutex_unlock(&LOCK_open);
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
                                  "Failed to open partially restored table"));
  }
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(0);
}


static int prepare_for_repair(THD *thd, TABLE_LIST *table_list,
			      HA_CHECK_OPT *check_opt)
{
  int error= 0;
  TABLE tmp_table, *table;
  TABLE_SHARE *share;
  char from[FN_REFLEN],tmp[FN_REFLEN+32];
  const char **ext;
  MY_STAT stat_info;
  DBUG_ENTER("prepare_for_repair");

  if (!(check_opt->sql_flags & TT_USEFRM))
    DBUG_RETURN(0);

  if (!(table= table_list->table))		/* if open_ltable failed */
  {
    char key[MAX_DBKEY_LENGTH];
    uint key_length;

    key_length= create_table_def_key(thd, key, table_list, 0);
    pthread_mutex_lock(&LOCK_open);
    if (!(share= (get_table_share(thd, table_list, key, key_length, 0,
                                  &error))))
    {
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(0);				// Can't open frm file
    }

    if (open_table_from_share(thd, share, "", 0, 0, 0, &tmp_table, FALSE))
    {
      release_table_share(share, RELEASE_NORMAL);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(0);                           // Out of memory
    }
    table= &tmp_table;
    pthread_mutex_unlock(&LOCK_open);
  }
  /*
    REPAIR TABLE ... USE_FRM for temporary tables makes little sense.
  */
  if (table->s->tmp_table)
  {
    error= send_check_errmsg(thd, table_list, "repair",
			     "Cannot repair temporary table from .frm file");
    goto end;
  }

  /*
    User gave us USE_FRM which means that the header in the index file is
    trashed.
    In this case we will try to fix the table the following way:
    - Rename the data file to a temporary name
    - Truncate the table
    - Replace the new data file with the old one
    - Run a normal repair using the new index file and the old data file
  */

  /*
    Check if this is a table type that stores index and data separately,
    like ISAM or MyISAM
  */
  ext= table->file->bas_ext();
  if (!ext[0] || !ext[1])
    goto end;					// No data file

  // Name of data file
  strxmov(from, table->s->normalized_path.str, ext[1], NullS);
  if (!my_stat(from, &stat_info, MYF(0)))
    goto end;				// Can't use USE_FRM flag

  my_snprintf(tmp, sizeof(tmp), "%s-%lx_%lx",
	      from, current_pid, thd->thread_id);

  /* If we could open the table, close it */
  if (table_list->table)
  {
    pthread_mutex_lock(&LOCK_open);
    close_cached_table(thd, table);
    pthread_mutex_unlock(&LOCK_open);
  }
  if (lock_and_wait_for_table_name(thd,table_list))
  {
    error= -1;
    goto end;
  }
  if (my_rename(from, tmp, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed renaming data file");
    goto end;
  }
  if (mysql_truncate(thd, table_list, 1))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed generating table from .frm file");
    goto end;
  }
  if (my_rename(tmp, from, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed restoring .MYD file");
    goto end;
  }

  /*
    Now we should be able to open the partially repaired table
    to finish the repair in the handler later on.
  */
  pthread_mutex_lock(&LOCK_open);
  if (reopen_name_locked_table(thd, table_list))
  {
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
                             "Failed to open partially repaired table");
    goto end;
  }
  pthread_mutex_unlock(&LOCK_open);

end:
  if (table == &tmp_table)
  {
    pthread_mutex_lock(&LOCK_open);
    closefrm(table, 1);				// Free allocated memory
    pthread_mutex_unlock(&LOCK_open);
  }
  DBUG_RETURN(error);
}



/*
  RETURN VALUES
    FALSE Message sent to net (admin operation went ok)
    TRUE  Message should be sent by caller 
          (admin operation or network communication failed)
*/
static bool mysql_admin_table(THD* thd, TABLE_LIST* tables,
                              HA_CHECK_OPT* check_opt,
                              const char *operator_name,
                              thr_lock_type lock_type,
                              bool open_for_modify,
                              bool no_warnings_for_error,
                              uint extra_open_options,
                              int (*prepare_func)(THD *, TABLE_LIST *,
                                                  HA_CHECK_OPT *),
                              int (handler::*operator_func)(THD *,
                                                            HA_CHECK_OPT *),
                              int (view_operator_func)(THD *, TABLE_LIST*))
{
  TABLE_LIST *table, *save_next_global, *save_next_local;
  SELECT_LEX *select= &thd->lex->select_lex;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  LEX *lex= thd->lex;
  int result_code;
  DBUG_ENTER("mysql_admin_table");

  if (end_active_trans(thd))
    DBUG_RETURN(1);
  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Op", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_type", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_text", 255));
  item->maybe_null = 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  mysql_ha_flush(thd, tables, MYSQL_HA_CLOSE_FINAL, FALSE);
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    char* db = table->db;
    bool fatal_error=0;

    strxmov(table_name, db, ".", table->table_name, NullS);
    thd->open_options|= extra_open_options;
    table->lock_type= lock_type;
    /* open only one table from local list of command */
    save_next_global= table->next_global;
    table->next_global= 0;
    save_next_local= table->next_local;
    table->next_local= 0;
    select->table_list.first= (byte*)table;
    /*
      Time zone tables and SP tables can be add to lex->query_tables list,
      so it have to be prepared.
      TODO: Investigate if we can put extra tables into argument instead of
      using lex->query_tables
    */
    lex->query_tables= table;
    lex->query_tables_last= &table->next_global;
    lex->query_tables_own_last= 0;
    thd->no_warnings_for_error= no_warnings_for_error;
    if (view_operator_func == NULL)
      table->required_type=FRMTYPE_TABLE;
    open_and_lock_tables(thd, table);
    thd->no_warnings_for_error= 0;
    table->next_global= save_next_global;
    table->next_local= save_next_local;
    thd->open_options&= ~extra_open_options;

    if (prepare_func)
    {
      switch ((*prepare_func)(thd, table, check_opt)) {
      case  1:           // error, message written to net
        ha_autocommit_or_rollback(thd, 1);
        close_thread_tables(thd);
        continue;
      case -1:           // error, message could be written to net
        goto err;
      default:           // should be 0 otherwise
        ;
      }
    }

    /*
      CHECK TABLE command is only command where VIEW allowed here and this
      command use only temporary teble method for VIEWs resolving => there
      can't be VIEW tree substitition of join view => if opening table
      succeed then table->table will have real TABLE pointer as value (in
      case of join view substitution table->table can be 0, but here it is
      impossible)
    */
    if (!table->table)
    {
      char buf[ERRMSGSIZE+ERRMSGSIZE+2];
      const char *err_msg;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      if (!(err_msg=thd->net.last_error))
	err_msg=ER(ER_CHECK_NO_SUCH_TABLE);
      /* if it was a view will check md5 sum */
      if (table->view &&
          view_checksum(thd, table) == HA_ADMIN_WRONG_CHECKSUM)
      {
        strxmov(buf, err_msg, "; ", ER(ER_VIEW_CHECKSUM), NullS);
        err_msg= (const char *)buf;
      }
      protocol->store(err_msg, system_charset_info);
      lex->cleanup_after_one_table_open();
      thd->clear_error();
      /*
        View opening can be interrupted in the middle of process so some
        tables can be left opening
      */
      ha_autocommit_or_rollback(thd, 1);
      close_thread_tables(thd);
      lex->reset_query_tables_list(FALSE);
      if (protocol->write())
	goto err;
      continue;
    }

    if (table->view)
    {
      result_code= (*view_operator_func)(thd, table);
      goto send_result;
    }

    table->table->pos_in_table_list= table;
    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      char buff[FN_REFLEN + MYSQL_ERRMSG_SIZE];
      uint length;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length= my_snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                          table_name);
      protocol->store(buff, length, system_charset_info);
      ha_autocommit_or_rollback(thd, 0);
      close_thread_tables(thd);
      lex->reset_query_tables_list(FALSE);
      table->table=0;				// For query cache
      if (protocol->write())
	goto err;
      continue;
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->s->version &&
        !table->table->s->log_table)
    {
      pthread_mutex_lock(&LOCK_open);
      const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(thd,table->table, TRUE);
      remove_table_from_cache(thd, table->table->s->db.str,
                              table->table->s->table_name.str,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      thd->exit_cond(old_message);
      if (thd->killed)
	goto err;
      /* Flush entries in the query cache involving this table. */
      query_cache_invalidate3(thd, table->table, 0);
      open_for_modify= 0;
    }

    if (table->table->s->crashed && operator_func == &handler::ha_check)
    {
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("warning"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is marked as crashed"),
                      system_charset_info);
      if (protocol->write())
        goto err;
    }

    if (operator_func == &handler::ha_repair)
    {
      if ((table->table->file->check_old_types() == HA_ADMIN_NEEDS_ALTER) ||
          (table->table->file->ha_check_for_upgrade(check_opt) ==
           HA_ADMIN_NEEDS_ALTER))
      {
        ha_autocommit_or_rollback(thd, 1);
        close_thread_tables(thd);
        tmp_disable_binlog(thd); // binlogging is done by caller if wanted
        result_code= mysql_recreate_table(thd, table, 0);
        reenable_binlog(thd);
        goto send_result;
      }

    }

    result_code = (table->table->file->*operator_func)(thd, check_opt);

send_result:

    lex->cleanup_after_one_table_open();
    thd->clear_error();  // these errors shouldn't get client
    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);
    protocol->store(operator_name, system_charset_info);

send_result_message:

    DBUG_PRINT("info", ("result_code: %d", result_code));
    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint length=my_snprintf(buf, ERRMSGSIZE,
				ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	protocol->store(STRING_WITH_LEN("note"), system_charset_info);
	protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_NOT_BASE_TABLE:
      {
        char buf[ERRMSGSIZE+20];
        uint length= my_snprintf(buf, ERRMSGSIZE,
                                 ER(ER_BAD_TABLE_ERROR), table_name);
        protocol->store(STRING_WITH_LEN("note"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_OK:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("OK"), system_charset_info);
      break;

    case HA_ADMIN_FAILED:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation failed"),
                      system_charset_info);
      break;

    case HA_ADMIN_REJECT:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation need committed state"),
                      system_charset_info);
      open_for_modify= FALSE;
      break;

    case HA_ADMIN_ALREADY_DONE:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is already up to date"),
                      system_charset_info);
      break;

    case HA_ADMIN_CORRUPT:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Corrupt"), system_charset_info);
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Invalid argument"),
                      system_charset_info);
      break;

    case HA_ADMIN_TRY_ALTER:
    {
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER TABLE,
        reopen the table and do ha_innobase::analyze() on it.
      */
      ha_autocommit_or_rollback(thd, 0);
      close_thread_tables(thd);
      TABLE_LIST *save_next_local= table->next_local,
                 *save_next_global= table->next_global;
      table->next_local= table->next_global= 0;
      tmp_disable_binlog(thd); // binlogging is done by caller if wanted
      result_code= mysql_recreate_table(thd, table, 0);
      reenable_binlog(thd);
      ha_autocommit_or_rollback(thd, 0);
      close_thread_tables(thd);
      if (!result_code) // recreation went ok
      {
        if ((table->table= open_ltable(thd, table, lock_type)) &&
            ((result_code= table->table->file->analyze(thd, check_opt)) > 0))
          result_code= 0; // analyze went ok
      }
      if (result_code) // either mysql_recreate_table or analyze failed
      {
        const char *err_msg;
        if ((err_msg= thd->net.last_error))
        {
          if (!thd->vio_ok())
          {
            sql_print_error(err_msg);
          }
          else
          {
            /* Hijack the row already in-progress. */
            protocol->store(STRING_WITH_LEN("error"), system_charset_info);
            protocol->store(err_msg, system_charset_info);
            (void)protocol->write();
            /* Start off another row for HA_ADMIN_FAILED */
            protocol->prepare_for_resend();
            protocol->store(table_name, system_charset_info);
            protocol->store(operator_name, system_charset_info);
          }
        }
      }
      result_code= result_code ? HA_ADMIN_FAILED : HA_ADMIN_OK;
      table->next_local= save_next_local;
      table->next_global= save_next_global;
      goto send_result_message;
    }
    case HA_ADMIN_WRONG_CHECKSUM:
    {
      protocol->store(STRING_WITH_LEN("note"), system_charset_info);
      protocol->store(ER(ER_VIEW_CHECKSUM), strlen(ER(ER_VIEW_CHECKSUM)),
                      system_charset_info);
      break;
    }

    case HA_ADMIN_NEEDS_UPGRADE:
    case HA_ADMIN_NEEDS_ALTER:
    {
      char buf[ERRMSGSIZE];
      uint length;

      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length=my_snprintf(buf, ERRMSGSIZE, ER(ER_TABLE_NEEDS_UPGRADE), table->table_name);
      protocol->store(buf, length, system_charset_info);
      fatal_error=1;
      break;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[ERRMSGSIZE+20];
        uint length=my_snprintf(buf, ERRMSGSIZE,
                                "Unknown - internal error %d during operation",
                                result_code);
        protocol->store(STRING_WITH_LEN("error"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
        fatal_error=1;
        break;
      }
    }
    if (table->table)
    {
      /* in the below check we do not refresh the log tables */
      if (fatal_error)
        table->table->s->version=0;               // Force close of table
      else if (open_for_modify && !table->table->s->log_table)
      {
        if (table->table->s->tmp_table)
          table->table->file->info(HA_STATUS_CONST);
        else
        {
          pthread_mutex_lock(&LOCK_open);
          remove_table_from_cache(thd, table->table->s->db.str,
                                  table->table->s->table_name.str, RTFC_NO_FLAG);
          pthread_mutex_unlock(&LOCK_open);
        }
        /* May be something modified consequently we have to invalidate cache */
        query_cache_invalidate3(thd, table->table, 0);
      }
    }
    ha_autocommit_or_rollback(thd, 0);
    close_thread_tables(thd);
    table->table=0;				// For query cache
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(FALSE);

 err:
  ha_autocommit_or_rollback(thd, 1);
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(TRUE);
}


bool mysql_backup_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_backup_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"backup", TL_READ, 0, 0, 0, 0,
				&handler::backup, 0));
}


bool mysql_restore_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_restore_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"restore", TL_WRITE, 1, 1, 0,
				&prepare_for_restore,
				&handler::restore, 0));
}


bool mysql_repair_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_repair_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"repair", TL_WRITE, 1,
                                test(check_opt->sql_flags & TT_USEFRM),
                                HA_OPEN_FOR_REPAIR,
				&prepare_for_repair,
				&handler::ha_repair, 0));
}


bool mysql_optimize_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_optimize_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"optimize", TL_WRITE, 1,0,0,0,
				&handler::optimize, 0));
}


/*
  Assigned specified indexes for a table into key cache

  SYNOPSIS
    mysql_assign_to_keycache()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
   FALSE ok
   TRUE  error
*/

bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* tables,
			     LEX_STRING *key_cache_name)
{
  HA_CHECK_OPT check_opt;
  KEY_CACHE *key_cache;
  DBUG_ENTER("mysql_assign_to_keycache");

  check_opt.init();
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(key_cache_name)))
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    my_error(ER_UNKNOWN_KEY_CACHE, MYF(0), key_cache_name->str);
    DBUG_RETURN(TRUE);
  }
  pthread_mutex_unlock(&LOCK_global_system_variables);
  check_opt.key_cache= key_cache;
  DBUG_RETURN(mysql_admin_table(thd, tables, &check_opt,
				"assign_to_keycache", TL_READ_NO_INSERT, 0, 0,
				0, 0, &handler::assign_to_keycache, 0));
}


/*
  Reassign all tables assigned to a key cache to another key cache

  SYNOPSIS
    reassign_keycache_tables()
    thd		Thread object
    src_cache	Reference to the key cache to clean up
    dest_cache	New key cache

  NOTES
    This is called when one sets a key cache size to zero, in which
    case we have to move the tables associated to this key cache to
    the "default" one.

    One has to ensure that one never calls this function while
    some other thread is changing the key cache. This is assured by
    the caller setting src_cache->in_init before calling this function.

    We don't delete the old key cache as there may still be pointers pointing
    to it for a while after this function returns.

 RETURN VALUES
    0	  ok
*/

int reassign_keycache_tables(THD *thd, KEY_CACHE *src_cache,
			     KEY_CACHE *dst_cache)
{
  DBUG_ENTER("reassign_keycache_tables");

  DBUG_ASSERT(src_cache != dst_cache);
  DBUG_ASSERT(src_cache->in_init);
  src_cache->param_buff_size= 0;		// Free key cache
  ha_resize_key_cache(src_cache);
  ha_change_key_cache(src_cache, dst_cache);
  DBUG_RETURN(0);
}


/*
  Preload specified indexes for a table into key cache

  SYNOPSIS
    mysql_preload_keys()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
    FALSE ok
    TRUE  error
*/

bool mysql_preload_keys(THD* thd, TABLE_LIST* tables)
{
  DBUG_ENTER("mysql_preload_keys");
  DBUG_RETURN(mysql_admin_table(thd, tables, 0,
				"preload_keys", TL_READ, 0, 0, 0, 0,
				&handler::preload_keys, 0));
}


/*
  Create a table identical to the specified table

  SYNOPSIS
    mysql_create_like_table()
    thd		Thread object
    table	Table list (one table only)
    create_info Create info
    table_ident Src table_ident

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_like_table(THD* thd, TABLE_LIST* table,
                             HA_CREATE_INFO *lex_create_info,
                             Table_ident *table_ident)
{
  TABLE *tmp_table;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN], tmp_path[FN_REFLEN];
  uint dst_path_length;
  char *db= table->db;
  char *table_name= table->table_name;
  char *src_db;
  char *src_table= table_ident->table.str;
  int  err;
  bool res= TRUE;
  enum legacy_db_type not_used;
  HA_CREATE_INFO *create_info;

  TABLE_LIST src_tables_list;
  DBUG_ENTER("mysql_create_like_table");

  if (!(create_info= copy_create_info(lex_create_info)))
  {
    DBUG_RETURN(TRUE);
  }
  DBUG_ASSERT(table_ident->db.str); /* Must be set in the parser */
  src_db= table_ident->db.str;

  /*
    Validate the source table
  */
  if (table_ident->table.length > NAME_LEN ||
      (table_ident->table.length &&
       check_table_name(src_table,table_ident->table.length)))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), src_table);
    DBUG_RETURN(TRUE);
  }
  if (!src_db || check_db_name(src_db))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), src_db ? src_db : "NULL");
    DBUG_RETURN(-1);
  }

  bzero((gptr)&src_tables_list, sizeof(src_tables_list));
  src_tables_list.db= src_db;
  src_tables_list.table_name= src_table;

  if (lock_and_wait_for_table_name(thd, &src_tables_list))
    goto err;

  if ((tmp_table= find_temporary_table(thd, src_db, src_table)))
    strxmov(src_path, tmp_table->s->path.str, reg_ext, NullS);
  else
  {
    build_table_filename(src_path, sizeof(src_path),
                         src_db, src_table, reg_ext, 0);
    /* Resolve symlinks (for windows) */
    unpack_filename(src_path, src_path);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, src_path);
    if (access(src_path, F_OK))
    {
      my_error(ER_BAD_TABLE_ERROR, MYF(0), src_table);
      goto err;
    }
  }

  /* 
     create like should be not allowed for Views, Triggers, ... 
  */
  if (mysql_frm_type(thd, src_path, &not_used) != FRMTYPE_TABLE)
  {
    my_error(ER_WRONG_OBJECT, MYF(0), src_db, src_table, "BASE TABLE");
    goto err;
  }

  /*
    Validate the destination table

    skip the destination table name checking as this is already
    validated.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (find_temporary_table(thd, db, table_name))
      goto table_exists;
    dst_path_length= build_tmptable_filename(thd, dst_path, sizeof(dst_path));
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, dst_path);
    create_info->table_options|= HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
    dst_path_length= build_table_filename(dst_path, sizeof(dst_path),
                                          db, table_name, reg_ext, 0);
    if (!access(dst_path, F_OK))
      goto table_exists;
  }

  /*
    Create a new table by copying from source table
  */
  if (my_copy(src_path, dst_path, MYF(MY_DONT_OVERWRITE_FILE)))
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_FILE,MYF(0),dst_path,my_errno);
    goto err;
  }

  /*
    As mysql_truncate don't work on a new table at this stage of
    creation, instead create the table directly (for both normal
    and temporary tables).
  */
#ifdef WITH_PARTITION_STORAGE_ENGINE
  /*
    For partitioned tables we need to copy the .par file as well since
    it is used in open_table_def to even be able to create a new handler.
    There is no way to find out here if the original table is a
    partitioned table so we copy the file and ignore any errors.
  */
  fn_format(tmp_path, dst_path, reg_ext, ".par", MYF(MY_REPLACE_EXT));
  strmov(dst_path, tmp_path);
  fn_format(tmp_path, src_path, reg_ext, ".par", MYF(MY_REPLACE_EXT));
  strmov(src_path, tmp_path);
  my_copy(src_path, dst_path, MYF(MY_DONT_OVERWRITE_FILE));
#endif
  dst_path[dst_path_length - reg_ext_length]= '\0';  // Remove .frm
  err= ha_create_table(thd, dst_path, db, table_name, create_info, 1);

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (err || !open_temporary_table(thd, dst_path, db, table_name, 1))
    {
      (void) rm_temporary_table(create_info->db_type,
				dst_path); /* purecov: inspected */
      goto err;     /* purecov: inspected */
    }
  }
  else if (err)
  {
    (void) quick_rm_table(create_info->db_type, db,
			  table_name, 0); /* purecov: inspected */
    goto err;	    /* purecov: inspected */
  }

  /*
    We have to write the query before we unlock the tables.
  */
  if (thd->current_stmt_binlog_row_based)
  {
    /*
       Since temporary tables are not replicated under row-based
       replication, CREATE TABLE ... LIKE ... needs special
       treatement.  We have four cases to consider, according to the
       following decision table:

           ==== ========= ========= ==============================
           Case    Target    Source Write to binary log
           ==== ========= ========= ==============================
           1       normal    normal Original statement
           2       normal temporary Generated statement
           3    temporary    normal Nothing
           4    temporary temporary Nothing
           ==== ========= ========= ==============================

       The variable 'tmp_table' below is used to see if the source
       table is a temporary table: if it is set, then the source table
       was a temporary table and we can take apropriate actions.
    */
    if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (tmp_table)                            // Case 2
      {
        char buf[2048];
        String query(buf, sizeof(buf), system_charset_info);
        query.length(0);  // Have to zero it since constructor doesn't
        TABLE *table_ptr;
        int error;

        /*
          Let's open and lock the table: it will be closed (and
          unlocked) by close_thread_tables() at the end of the
          statement anyway.
         */
        if (!(table_ptr= open_ltable(thd, table, TL_READ_NO_INSERT)))
          goto err;

        int result= store_create_info(thd, table, &query, create_info);

        DBUG_ASSERT(result == 0); // store_create_info() always return 0
        write_bin_log(thd, TRUE, query.ptr(), query.length());
      }
      else                                      // Case 1
        write_bin_log(thd, TRUE, thd->query, thd->query_length);
    }
    /*
      Case 3 and 4 does nothing under RBR
    */
  }
  else
    write_bin_log(thd, TRUE, thd->query, thd->query_length);

  res= FALSE;
  goto err;

table_exists:
  if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(warn_buff, sizeof(warn_buff),
		ER(ER_TABLE_EXISTS_ERROR), table_name);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_TABLE_EXISTS_ERROR,warn_buff);
    res= FALSE;
  }
  else
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);

err:
  pthread_mutex_lock(&LOCK_open);
  unlock_table_name(thd, &src_tables_list);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(res);
}


bool mysql_analyze_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  DBUG_ENTER("mysql_analyze_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"analyze", lock_type, 1, 0, 0, 0,
				&handler::analyze, 0));
}


bool mysql_check_table(THD* thd, TABLE_LIST* tables,HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  DBUG_ENTER("mysql_check_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"check", lock_type,
				0, HA_OPEN_FOR_REPAIR, 0, 0,
				&handler::ha_check, &view_checksum));
}


/* table_list should contain just one table */
static int
mysql_discard_or_import_tablespace(THD *thd,
                                   TABLE_LIST *table_list,
                                   enum tablespace_op_type tablespace_op)
{
  TABLE *table;
  my_bool discard;
  int error;
  DBUG_ENTER("mysql_discard_or_import_tablespace");

  /*
    Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
    ALTER TABLE
  */

  thd->proc_info="discard_or_import_tablespace";

  discard= test(tablespace_op == DISCARD_TABLESPACE);

 /*
   We set this flag so that ha_innobase::open and ::external_lock() do
   not complain when we lock the table
 */
  thd->tablespace_op= TRUE;
  if (!(table=open_ltable(thd,table_list,TL_WRITE)))
  {
    thd->tablespace_op=FALSE;
    DBUG_RETURN(-1);
  }

  error=table->file->discard_or_import_tablespace(discard);

  thd->proc_info="end";

  if (error)
    goto err;

  /*
    The 0 in the call below means 'not in a transaction', which means
    immediate invalidation; that is probably what we wish here
  */
  query_cache_invalidate3(thd, table_list, 0);

  /* The ALTER TABLE is always in its own transaction */
  error = ha_commit_stmt(thd);
  if (ha_commit(thd))
    error=1;
  if (error)
    goto err;
  write_bin_log(thd, FALSE, thd->query, thd->query_length);

err:
  ha_autocommit_or_rollback(thd, error);
  close_thread_tables(thd);
  thd->tablespace_op=FALSE;
  
  if (error == 0)
  {
    send_ok(thd);
    DBUG_RETURN(0);
  }

  table->file->print_error(error, MYF(0));
    
  DBUG_RETURN(-1);
}


/*
  SYNOPSIS
    compare_tables()
      table                     The original table.
      create_list               The fields for the new table.
      key_info_buffer           An array of KEY structs for the new indexes.
      key_count                 The number of elements in the array.
      create_info               Create options for the new table.
      alter_info                Alter options.
      order_num                 Number of order list elements.
      index_drop_buffer   OUT   An array of offsets into table->key_info.
      index_drop_count    OUT   The number of elements in the array.
      index_add_buffer    OUT   An array of offsets into key_info_buffer.
      index_add_count     OUT   The number of elements in the array.

  DESCRIPTION
    'table' (first argument) contains information of the original
    table, which includes all corresponding parts that the new
    table has in arguments create_list, key_list and create_info.

    By comparing the changes between the original and new table
    we can determine how much it has changed after ALTER TABLE
    and whether we need to make a copy of the table, or just change
    the .frm file.

    If there are no data changes, but index changes, 'index_drop_buffer'
    and/or 'index_add_buffer' are populated with offsets into
    table->key_info or key_info_buffer respectively for the indexes
    that need to be dropped and/or (re-)created.

  RETURN VALUES
    0                           No copy needed
    ALTER_TABLE_DATA_CHANGED    Data changes, copy needed
    ALTER_TABLE_INDEX_CHANGED   Index changes, copy might be needed
*/

static uint compare_tables(TABLE *table, List<create_field> *create_list,
                           KEY *key_info_buffer, uint key_count,
                           HA_CREATE_INFO *create_info,
                           ALTER_INFO *alter_info, uint order_num,
                           uint *index_drop_buffer, uint *index_drop_count,
                           uint *index_add_buffer, uint *index_add_count,
                           bool varchar)
{
  Field **f_ptr, *field;
  uint changes= 0, tmp;
  List_iterator_fast<create_field> new_field_it(*create_list);
  create_field *new_field;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  DBUG_ENTER("compare_tables");

  /*
    Some very basic checks. If number of fields changes, or the
    handler, we need to run full ALTER TABLE. In the future
    new fields can be added and old dropped without copy, but
    not yet.

    Test also that engine was not given during ALTER TABLE, or
    we are force to run regular alter table (copy).
    E.g. ALTER TABLE tbl_name ENGINE=MyISAM.

    For the following ones we also want to run regular alter table:
    ALTER TABLE tbl_name ORDER BY ..
    ALTER TABLE tbl_name CONVERT TO CHARACTER SET ..

    At the moment we can't handle altering temporary tables without a copy.
    We also test if OPTIMIZE TABLE was given and was mapped to alter table.
    In that case we always do full copy.

    There was a bug prior to mysql-4.0.25. Number of null fields was
    calculated incorrectly. As a result frm and data files gets out of
    sync after fast alter table. There is no way to determine by which
    mysql version (in 4.0 and 4.1 branches) table was created, thus we
    disable fast alter table for all tables created by mysql versions
    prior to 5.0 branch.
    See BUG#6236.
  */
  if (table->s->fields != create_list->elements ||
      table->s->db_type != create_info->db_type ||
      table->s->tmp_table ||
      create_info->used_fields & HA_CREATE_USED_ENGINE ||
      create_info->used_fields & HA_CREATE_USED_CHARSET ||
      create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET ||
      (alter_info->flags & (ALTER_RECREATE | ALTER_FOREIGN_KEY)) ||
      order_num ||
      !table->s->mysql_version ||
      (table->s->frm_version < FRM_VER_TRUE_VARCHAR && varchar))
    DBUG_RETURN(ALTER_TABLE_DATA_CHANGED);

  /*
    Go through fields and check if the original ones are compatible
    with new table.
  */
  for (f_ptr= table->field, new_field= new_field_it++;
       (field= *f_ptr); f_ptr++, new_field= new_field_it++)
  {
    /* Make sure we have at least the default charset in use. */
    if (!new_field->charset)
      new_field->charset= create_info->default_table_charset;

    /* Check that NULL behavior is same for old and new fields */
    if ((new_field->flags & NOT_NULL_FLAG) !=
	(uint) (field->flags & NOT_NULL_FLAG))
      DBUG_RETURN(ALTER_TABLE_DATA_CHANGED);

    /* Don't pack rows in old tables if the user has requested this. */
    if (create_info->row_type == ROW_TYPE_DYNAMIC ||
	(new_field->flags & BLOB_FLAG) ||
	new_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED)
      create_info->table_options|= HA_OPTION_PACK_RECORD;

    /* Check if field was renamed */
    field->flags&= ~FIELD_IS_RENAMED;
    if (my_strcasecmp(system_charset_info,
		      field->field_name,
		      new_field->field_name))
      field->flags|= FIELD_IS_RENAMED;      

    /* Evaluate changes bitmap and send to check_if_incompatible_data() */
    if (!(tmp= field->is_equal(new_field)))
      DBUG_RETURN(ALTER_TABLE_DATA_CHANGED);
    // Clear indexed marker
    field->flags&= ~FIELD_IN_ADD_INDEX;
    changes|= tmp;
  }

  /*
    Go through keys and check if the original ones are compatible
    with new table.
  */
  KEY *table_key;
  KEY *table_key_end= table->key_info + table->s->keys;
  KEY *new_key;
  KEY *new_key_end= key_info_buffer + key_count;

  DBUG_PRINT("info", ("index count old: %d  new: %d",
                      table->s->keys, key_count));
  /*
    Step through all keys of the old table and search matching new keys.
  */
  *index_drop_count= 0;
  *index_add_count= 0;
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    KEY_PART_INFO *table_part;
    KEY_PART_INFO *table_part_end= table_key->key_part + table_key->key_parts;
    KEY_PART_INFO *new_part;

    /* Search a new key with the same name. */
    for (new_key= key_info_buffer; new_key < new_key_end; new_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (new_key >= new_key_end)
    {
      /* Key not found. Add the offset of the key to the drop buffer. */
      index_drop_buffer[(*index_drop_count)++]= table_key - table->key_info;
      DBUG_PRINT("info", ("index dropped: '%s'", table_key->name));
      continue;
    }

    /* Check that the key types are compatible between old and new tables. */
    if ((table_key->algorithm != new_key->algorithm) ||
	((table_key->flags & HA_KEYFLAG_MASK) !=
         (new_key->flags & HA_KEYFLAG_MASK)) ||
        (table_key->key_parts != new_key->key_parts))
      goto index_changed;

    /*
      Check that the key parts remain compatible between the old and
      new tables.
    */
    for (table_part= table_key->key_part, new_part= new_key->key_part;
         table_part < table_part_end;
         table_part++, new_part++)
    {
      /*
	Key definition has changed if we are using a different field or
	if the used key part length is different. We know that the fields
        did not change. Comparing field numbers is sufficient.
      */
      if ((table_part->length != new_part->length) ||
          (table_part->fieldnr - 1 != new_part->fieldnr))
	goto index_changed;
    }
    continue;

  index_changed:
    /* Key modified. Add the offset of the key to both buffers. */
    index_drop_buffer[(*index_drop_count)++]= table_key - table->key_info;
    index_add_buffer[(*index_add_count)++]= new_key - key_info_buffer;
    key_part= new_key->key_part;
    end= key_part + new_key->key_parts;
    for(; key_part != end; key_part++)
    {
      // Mark field to be part of new key 
      field= table->field[key_part->fieldnr];
      field->flags|= FIELD_IN_ADD_INDEX;
    }
    DBUG_PRINT("info", ("index changed: '%s'", table_key->name));
  }
  /*end of for (; table_key < table_key_end;) */

  /*
    Step through all keys of the new table and find matching old keys.
  */
  for (new_key= key_info_buffer; new_key < new_key_end; new_key++)
  {
    /* Search an old key with the same name. */
    for (table_key= table->key_info; table_key < table_key_end; table_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (table_key >= table_key_end)
    {
      /* Key not found. Add the offset of the key to the add buffer. */
      index_add_buffer[(*index_add_count)++]= new_key - key_info_buffer;
      key_part= new_key->key_part;
      end= key_part + new_key->key_parts;
      for(; key_part != end; key_part++)
      {
        // Mark field to be part of new key 
        field= table->field[key_part->fieldnr];
        field->flags|= FIELD_IN_ADD_INDEX;
      }
      DBUG_PRINT("info", ("index added: '%s'", new_key->name));
    }
  }

  /* Check if changes are compatible with current handler without a copy */
  if (table->file->check_if_incompatible_data(create_info, changes))
    DBUG_RETURN(ALTER_TABLE_DATA_CHANGED);

  if (*index_drop_count || *index_add_count)
    DBUG_RETURN(ALTER_TABLE_INDEX_CHANGED);

  DBUG_RETURN(0); // Tables are compatible
}


/*
  Alter table
*/

bool mysql_alter_table(THD *thd,char *new_db, char *new_name,
                       HA_CREATE_INFO *lex_create_info,
                       TABLE_LIST *table_list,
                       List<create_field> &fields, List<Key> &keys,
                       uint order_num, ORDER *order, bool ignore,
                       ALTER_INFO *alter_info, bool do_send_ok)
{
  TABLE *table,*new_table=0;
  int error;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN];
  char new_alias_buff[FN_REFLEN], *table_name, *db, *new_alias, *alias;
  char index_file[FN_REFLEN], data_file[FN_REFLEN];
  char path[FN_REFLEN];
  char reg_path[FN_REFLEN+1];
  ha_rows copied,deleted;
  uint db_create_options, used_fields;
  handlerton *old_db_type, *new_db_type;
  HA_CREATE_INFO *create_info;
  uint need_copy_table= 0;
  bool no_table_reopen= FALSE, varchar= FALSE;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  uint fast_alter_partition= 0;
  bool partition_changed= FALSE;
#endif
  List<create_field> prepared_create_list;
  List<Key>          prepared_key_list;
  bool need_lock_for_indexes= TRUE;
  uint db_options= 0;
  uint key_count;
  KEY  *key_info_buffer;
  uint index_drop_count;
  uint *index_drop_buffer;
  uint index_add_count;
  uint *index_add_buffer;
  bool committed= 0;
  DBUG_ENTER("mysql_alter_table");

  LINT_INIT(index_add_count);
  LINT_INIT(index_drop_count);
  LINT_INIT(index_add_buffer);
  LINT_INIT(index_drop_buffer);

  thd->proc_info="init";
  if (!(create_info= copy_create_info(lex_create_info)))
  {
    DBUG_RETURN(TRUE);
  }
  table_name=table_list->table_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;
  db=table_list->db;
  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;
  build_table_filename(reg_path, sizeof(reg_path), db, table_name, reg_ext, 0);
  build_table_filename(path, sizeof(path), db, table_name, "", 0);

  used_fields=create_info->used_fields;

  mysql_ha_flush(thd, table_list, MYSQL_HA_CLOSE_FINAL, FALSE);

  /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER TABLE */
  if (alter_info->tablespace_op != NO_TABLESPACE_OP)
    DBUG_RETURN(mysql_discard_or_import_tablespace(thd,table_list,
						   alter_info->tablespace_op));
  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(TRUE);
  table->use_all_columns();

  /* Check that we are not trying to rename to an existing table */
  if (new_name)
  {
    DBUG_PRINT("info", ("new_db.new_name: '%s'.'%s'", new_db, new_name));
    strmov(new_name_buff,new_name);
    strmov(new_alias= new_alias_buff, new_name);
    if (lower_case_table_names)
    {
      if (lower_case_table_names != 2)
      {
	my_casedn_str(files_charset_info, new_name_buff);
	new_alias= new_name;			// Create lower case table name
      }
      my_casedn_str(files_charset_info, new_name);
    }
    if (new_db == db &&
	!my_strcasecmp(table_alias_charset, new_name_buff, table_name))
    {
      /*
	Source and destination table names are equal: make later check
	easier.
      */
      new_alias= new_name= table_name;
    }
    else
    {
      if (table->s->tmp_table != NO_TMP_TABLE)
      {
	if (find_temporary_table(thd,new_db,new_name_buff))
	{
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name_buff);
	  DBUG_RETURN(TRUE);
	}
      }
      else
      {
        build_table_filename(new_name_buff, sizeof(new_name_buff),
                             new_db, new_name_buff, reg_ext, 0);
        if (!access(new_name_buff, F_OK))
	{
	  /* Table will be closed in do_command() */
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
	  DBUG_RETURN(TRUE);
	}
      }
    }
  }
  else
  {
    new_alias= (lower_case_table_names == 2) ? alias : table_name;
    new_name= table_name;
  }

  old_db_type= table->s->db_type;
  if (!create_info->db_type)
  {
#ifdef WITH_PARTITION_STORAGE_ENGINE
    if (table->part_info &&
        create_info->used_fields & HA_CREATE_USED_ENGINE)
    {
      /*
        This case happens when the user specified
        ENGINE = x where x is a non-existing storage engine
        We set create_info->db_type to default_engine_type
        to ensure we don't change underlying engine type
        due to a erroneously given engine name.
      */
      create_info->db_type= table->part_info->default_engine_type;
    }
    else
#endif
      create_info->db_type= old_db_type;
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (prep_alter_part_table(thd, table, alter_info, create_info, old_db_type,
                            &partition_changed, &fast_alter_partition))
  {
    DBUG_RETURN(TRUE);
  }
#endif
  if (check_engine(thd, new_name, create_info))
    DBUG_RETURN(TRUE);
  new_db_type= create_info->db_type;
  if (create_info->row_type == ROW_TYPE_NOT_USED)
    create_info->row_type= table->s->row_type;

  DBUG_PRINT("info", ("old type: %s  new type: %s",
             ha_resolve_storage_engine_name(old_db_type),
             ha_resolve_storage_engine_name(new_db_type)));
  if (ha_check_storage_engine_flag(old_db_type, HTON_ALTER_NOT_SUPPORTED) ||
      ha_check_storage_engine_flag(new_db_type, HTON_ALTER_NOT_SUPPORTED))
  {
    DBUG_PRINT("info", ("doesn't support alter"));
    my_error(ER_ILLEGAL_HA, MYF(0), table_name);
    DBUG_RETURN(TRUE);
  }
  
  thd->proc_info="setup";
  if (!(alter_info->flags & ~(ALTER_RENAME | ALTER_KEYS_ONOFF)) &&
      !table->s->tmp_table) // no need to touch frm
  {
    error=0;
    if (new_name != table_name || new_db != db)
    {
      thd->proc_info="rename";
      VOID(pthread_mutex_lock(&LOCK_open));
      /* Then do a 'simple' rename of the table */
      error=0;
      if (!access(new_name_buff,F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name);
	error= -1;
      }
      else
      {
	*fn_ext(new_name)=0;
        table->s->version= 0;                   // Force removal of table def
	close_cached_table(thd, table);
	if (mysql_rename_table(old_db_type,db,table_name,new_db,new_alias, 0))
	  error= -1;
        else if (Table_triggers_list::change_table_name(thd, db, table_name,
                                                        new_db, new_alias))
        {
          VOID(mysql_rename_table(old_db_type, new_db, new_alias, db,
                                  table_name, 0));
          error= -1;
        }
      }
      VOID(pthread_mutex_unlock(&LOCK_open));
    }

    if (!error)
    {
      switch (alter_info->keys_onoff) {
      case LEAVE_AS_IS:
        break;
      case ENABLE:
        VOID(pthread_mutex_lock(&LOCK_open));
        wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
        VOID(pthread_mutex_unlock(&LOCK_open));
        error= table->file->enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
        /* COND_refresh will be signaled in close_thread_tables() */
        break;
      case DISABLE:
        VOID(pthread_mutex_lock(&LOCK_open));
        wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
        VOID(pthread_mutex_unlock(&LOCK_open));
        error=table->file->disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
        /* COND_refresh will be signaled in close_thread_tables() */
        break;
      }
    }

    if (error == HA_ERR_WRONG_COMMAND)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
      error=0;
    }
    if (!error)
    {
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
      if (do_send_ok)
        send_ok(thd);
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    table_list->table=0;				// For query cache
    query_cache_invalidate3(thd, table_list, 0);
    DBUG_RETURN(error);
  }

  /* Full alter table */

  /* Let new create options override the old ones */
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS))
    create_info->min_rows= table->s->min_rows;
  if (!(used_fields & HA_CREATE_USED_MAX_ROWS))
    create_info->max_rows= table->s->max_rows;
  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    create_info->avg_row_length= table->s->avg_row_length;
  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->s->table_charset;
  if (!(used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE))
    create_info->key_block_size= table->s->key_block_size;

  restore_record(table, s->default_values);     // Empty record for DEFAULT
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  List_iterator<create_field> def_it(fields);
  List_iterator<Alter_column> alter_it(alter_info->alter_list);
  List<create_field> create_list;		// Add new fields here
  List<Key> key_list;				// Add new keys here
  create_field *def;

  /*
    First collect all fields from table which isn't in drop_list
  */

  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (field->type() == MYSQL_TYPE_STRING)
      varchar= TRUE;
    /* Check if field should be dropped */
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(system_charset_info,field->field_name, drop->name))
      {
	/* Reset auto_increment value if it was dropped */
	if (MTYP_TYPENR(field->unireg_check) == Field::NEXT_NUMBER &&
	    !(used_fields & HA_CREATE_USED_AUTO))
	{
	  create_info->auto_increment_value=0;
	  create_info->used_fields|=HA_CREATE_USED_AUTO;
	}
	break;
      }
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }
    /* Check if field is changed */
    def_it.rewind();
    while ((def=def_it++))
    {
      if (def->change &&
	  !my_strcasecmp(system_charset_info,field->field_name, def->change))
	break;
    }
    if (def)
    {						// Field is changed
      def->field=field;
      if (!def->after)
      {
	create_list.push_back(def);
	def_it.remove();
      }
    }
    else
    {
      /*
        This field was not dropped and not changed, add it to the list
        for the new table.
      */
      create_list.push_back(def=new create_field(field,field));
      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
      {
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
      }
      if (alter)
      {
	if (def->sql_type == FIELD_TYPE_BLOB)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), def->change);
	  DBUG_RETURN(TRUE);
	}
	if ((def->def=alter->def))              // Use new default
          def->flags&= ~NO_DEFAULT_VALUE_FLAG;
        else
          def->flags|= NO_DEFAULT_VALUE_FLAG;
	alter_it.remove();
      }
    }
  }
  def_it.rewind();
  List_iterator<create_field> find_it(create_list);
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table_name);
      DBUG_RETURN(TRUE);
    }
    if (!def->after)
      create_list.push_back(def);
    else if (def->after == first_keyword)
      create_list.push_front(def);
    else
    {
      create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(system_charset_info,def->after, find->field_name))
	  break;
      }
      if (!find)
      {
	my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table_name);
	DBUG_RETURN(TRUE);
      }
      find_it.after(def);			// Put element after this
    }
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             alter_info->alter_list.head()->name, table_name);
    DBUG_RETURN(TRUE);
  }
  if (!create_list.elements)
  {
    my_message(ER_CANT_REMOVE_ALL_FIELDS, ER(ER_CANT_REMOVE_ALL_FIELDS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */

  List_iterator<Key> key_it(keys);
  List_iterator<create_field> field_it(create_list);
  List<key_part_spec> key_parts;

  KEY *key_info=table->key_info;
  for (uint i=0 ; i < table->s->keys ; i++,key_info++)
  {
    char *key_name= key_info->name;
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(system_charset_info,key_name, drop->name))
	break;
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      const char *key_part_name=key_part->field->field_name;
      create_field *cfield;
      field_it.rewind();
      while ((cfield=field_it++))
      {
	if (cfield->change)
	{
	  if (!my_strcasecmp(system_charset_info, key_part_name,
			     cfield->change))
	    break;
	}
	else if (!my_strcasecmp(system_charset_info,
				key_part_name, cfield->field_name))
	  break;
      }
      if (!cfield)
	continue;				// Field is removed
      uint key_part_length=key_part->length;
      if (cfield->field)			// Not new field
      {
        /*
          If the field can't have only a part used in a key according to its
          new type, or should not be used partially according to its
          previous type, or the field length is less than the key part
          length, unset the key part length.

          We also unset the key part length if it is the same as the
          old field's length, so the whole new field will be used.

          BLOBs may have cfield->length == 0, which is why we test it before
          checking whether cfield->length < key_part_length (in chars).
         */
        if (!Field::type_can_have_key_part(cfield->field->type()) ||
            !Field::type_can_have_key_part(cfield->sql_type) ||
            (cfield->field->field_length == key_part_length &&
             !f_is_blob(key_part->key_type)) ||
	    (cfield->length && (cfield->length < key_part_length /
                                key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new key_part_spec(cfield->field_name,
					    key_part_length));
    }
    if (key_parts.elements)
    {
      KEY_CREATE_INFO key_create_info;
      bzero((char*) &key_create_info, sizeof(key_create_info));

      key_create_info.algorithm= key_info->algorithm;
      if (key_info->flags & HA_USES_BLOCK_SIZE)
        key_create_info.block_size= key_info->block_size;
      if (key_info->flags & HA_USES_PARSER)
        key_create_info.parser_name= *key_info->parser_name;

      key_list.push_back(new Key(key_info->flags & HA_SPATIAL ? Key::SPATIAL :
				 (key_info->flags & HA_NOSAME ?
				 (!my_strcasecmp(system_charset_info,
						 key_name, primary_key_name) ?
				  Key::PRIMARY	: Key::UNIQUE) :
				  (key_info->flags & HA_FULLTEXT ?
				   Key::FULLTEXT : Key::MULTIPLE)),
				 key_name,
                                 &key_create_info,
                                 test(key_info->flags & HA_GENERATED_KEY),
				 key_parts));
    }
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type != Key::FOREIGN_KEY)
	key_list.push_back(key);
      if (key->name &&
	  !my_strcasecmp(system_charset_info,key->name,primary_key_name))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
	DBUG_RETURN(TRUE);
      }
    }
  }

  if (alter_info->drop_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->drop_list.head()->name);
    goto err;
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->alter_list.head()->name);
    goto err;
  }

  db_create_options= table->s->db_create_options & ~(HA_OPTION_PACK_RECORD);
  my_snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  /* Safety fix for innodb */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);
  if (new_db_type != old_db_type && !table->file->can_switch_engines()) {
    my_error(ER_ROW_IS_REFERENCED, MYF(0));
    goto err;
  }
  create_info->db_type=new_db_type;
  if (!create_info->comment.str)
  {
    create_info->comment.str= table->s->comment.str;
    create_info->comment.length= table->s->comment.length;
  }

  table->file->update_create_info(create_info);
  if ((create_info->table_options &
       (HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS)) ||
      (used_fields & HA_CREATE_USED_PACK_KEYS))
    db_create_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
  if (create_info->table_options &
      (HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM))
    db_create_options&= ~(HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM);
  if (create_info->table_options &
      (HA_OPTION_DELAY_KEY_WRITE | HA_OPTION_NO_DELAY_KEY_WRITE))
    db_create_options&= ~(HA_OPTION_DELAY_KEY_WRITE |
			  HA_OPTION_NO_DELAY_KEY_WRITE);
  create_info->table_options|= db_create_options;

  if (table->s->tmp_table)
    create_info->options|=HA_LEX_CREATE_TMP_TABLE;

  set_table_default_charset(thd, create_info, db);

  {
    /*
      For some purposes we need prepared table structures and translated
      key descriptions with proper default key name assignment.

      Unfortunately, mysql_prepare_table() modifies the field and key
      lists. mysql_create_table() needs the unmodified lists. Hence, we
      need to copy the lists and all their elements. The lists contain
      pointers to the elements only.

      We cannot copy conditionally because the partition code always
      needs prepared lists and compare_tables() needs them and is almost
      always called.
    */

    /* Copy fields. */
    List_iterator<create_field> prep_field_it(create_list);
    create_field *prep_field;
    while ((prep_field= prep_field_it++))
      prepared_create_list.push_back(new create_field(*prep_field));

    /* Copy keys and key parts. */
    List_iterator<Key> prep_key_it(key_list);
    Key *prep_key;
    while ((prep_key= prep_key_it++))
    {
      List<key_part_spec> prep_columns;
      List_iterator<key_part_spec> prep_col_it(prep_key->columns);
      key_part_spec *prep_col;

      while ((prep_col= prep_col_it++))
        prep_columns.push_back(new key_part_spec(*prep_col));
      prepared_key_list.push_back(new Key(prep_key->type, prep_key->name,
                                          &prep_key->key_create_info,
                                          prep_key->generated, prep_columns));
    }

    /* Create the prepared information. */
    if (mysql_prepare_table(thd, create_info, &prepared_create_list,
                            &prepared_key_list,
                            (table->s->tmp_table != NO_TMP_TABLE), &db_options,
                            table->file, &key_info_buffer, &key_count, 0))
      goto err;
  }

  if (thd->variables.old_alter_table
      || (table->s->db_type != create_info->db_type)
#ifdef WITH_PARTITION_STORAGE_ENGINE
      || partition_changed
#endif
     )
    need_copy_table= 1;
  else
  {
    /* Try to optimize ALTER TABLE. Allocate result buffers. */
    if (! (index_drop_buffer=
           (uint*) thd->alloc(sizeof(uint) * table->s->keys)) ||
        ! (index_add_buffer=
           (uint*) thd->alloc(sizeof(uint) * prepared_key_list.elements)))
      goto err;
    /* Check how much the tables differ. */
    need_copy_table= compare_tables(table, &prepared_create_list,
                                    key_info_buffer, key_count,
                                    create_info, alter_info, order_num,
                                    index_drop_buffer, &index_drop_count,
                                    index_add_buffer, &index_add_count,
                                    varchar);
  }

  /*
    If there are index changes only, try to do them online. "Index
    changes only" means also that the handler for the table does not
    change. The table is open and locked. The handler can be accessed.
  */
  if (need_copy_table == ALTER_TABLE_INDEX_CHANGED)
  {
    int   pk_changed= 0;
    ulong alter_flags= 0;
    ulong needed_online_flags= 0;
    ulong needed_fast_flags= 0;
    KEY   *key;
    uint  *idx_p;
    uint  *idx_end_p;

    if (table->s->db_type->alter_table_flags)
      alter_flags= table->s->db_type->alter_table_flags(alter_info->flags);
    DBUG_PRINT("info", ("alter_flags: %lu", alter_flags));
    /* Check dropped indexes. */
    for (idx_p= index_drop_buffer, idx_end_p= idx_p + index_drop_count;
         idx_p < idx_end_p;
         idx_p++)
    {
      key= table->key_info + *idx_p;
      DBUG_PRINT("info", ("index dropped: '%s'", key->name));
      if (key->flags & HA_NOSAME)
      {
        /* Unique key. Check for "PRIMARY". */
        if (! my_strcasecmp(system_charset_info,
                            key->name, primary_key_name))
        {
          /* Primary key. */
          needed_online_flags|=  HA_ONLINE_DROP_PK_INDEX;
          needed_fast_flags|= HA_ONLINE_DROP_PK_INDEX_NO_WRITES;
          pk_changed++;
        }
        else
        {
          /* Non-primary unique key. */
          needed_online_flags|=  HA_ONLINE_DROP_UNIQUE_INDEX;
          needed_fast_flags|= HA_ONLINE_DROP_UNIQUE_INDEX_NO_WRITES;
        }
      }
      else
      {
        /* Non-unique key. */
        needed_online_flags|=  HA_ONLINE_DROP_INDEX;
        needed_fast_flags|= HA_ONLINE_DROP_INDEX_NO_WRITES;
      }
    }

    /* Check added indexes. */
    for (idx_p= index_add_buffer, idx_end_p= idx_p + index_add_count;
         idx_p < idx_end_p;
         idx_p++)
    {
      key= key_info_buffer + *idx_p;
      DBUG_PRINT("info", ("index added: '%s'", key->name));
      if (key->flags & HA_NOSAME)
      {
        /* Unique key. Check for "PRIMARY". */
        if (! my_strcasecmp(system_charset_info,
                            key->name, primary_key_name))
        {
          /* Primary key. */
          needed_online_flags|=  HA_ONLINE_ADD_PK_INDEX;
          needed_fast_flags|= HA_ONLINE_ADD_PK_INDEX_NO_WRITES;
          pk_changed++;
        }
        else
        {
          /* Non-primary unique key. */
          needed_online_flags|=  HA_ONLINE_ADD_UNIQUE_INDEX;
          needed_fast_flags|= HA_ONLINE_ADD_UNIQUE_INDEX_NO_WRITES;
        }
      }
      else
      {
        /* Non-unique key. */
        needed_online_flags|=  HA_ONLINE_ADD_INDEX;
        needed_fast_flags|= HA_ONLINE_ADD_INDEX_NO_WRITES;
      }
    }

    /*
      Online or fast add/drop index is possible only if
      the primary key is not added and dropped in the same statement.
      Otherwise we have to recreate the table.
      need_copy_table is no-zero at this place.
    */
    if ( pk_changed < 2 )
    {
      if ((alter_flags & needed_online_flags) == needed_online_flags)
      {
        /* All required online flags are present. */
        need_copy_table= 0;
        need_lock_for_indexes= FALSE;
      }
      else if ((alter_flags & needed_fast_flags) == needed_fast_flags)
      {
        /* All required fast flags are present. */
        need_copy_table= 0;
      }
    }
    DBUG_PRINT("info", ("need_copy_table: %u  need_lock: %d",
                        need_copy_table, need_lock_for_indexes));
  }

  /*
    better have a negative test here, instead of positive, like
    alter_info->flags & ALTER_ADD_COLUMN|ALTER_ADD_INDEX|...
    so that ALTER TABLE won't break when somebody will add new flag
  */
  if (!need_copy_table)
    create_info->frm_only= 1;

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (fast_alter_partition)
  {
    DBUG_RETURN(fast_alter_partition_table(thd, table, alter_info,
                                           create_info, table_list,
                                           &create_list, &key_list,
                                           db, table_name,
                                           fast_alter_partition));
  }
#endif

  /*
    Handling of symlinked tables:
    If no rename:
      Create new data file and index file on the same disk as the
      old data and index files.
      Copy data.
      Rename new data file over old data file and new index file over
      old index file.
      Symlinks are not changed.

   If rename:
      Create new data file and index file on the same disk as the
      old data and index files.  Create also symlinks to point at
      the new tables.
      Copy data.
      At end, rename temporary tables and symlinks to temporary table
      to final table name.
      Remove old table and old symlinks

    If rename is made to another database:
      Create new tables in new database.
      Copy data.
      Remove old table and symlinks.
  */
  if (!strcmp(db, new_db))		// Ignore symlink if db changed
  {
    if (create_info->index_file_name)
    {
      /* Fix index_file_name to have 'tmp_name' as basename */
      strmov(index_file, tmp_name);
      create_info->index_file_name=fn_same(index_file,
					   create_info->index_file_name,
					   1);
    }
    if (create_info->data_file_name)
    {
      /* Fix data_file_name to have 'tmp_name' as basename */
      strmov(data_file, tmp_name);
      create_info->data_file_name=fn_same(data_file,
					  create_info->data_file_name,
					  1);
    }
  }
  else
    create_info->data_file_name=create_info->index_file_name=0;

  /*
    Create a table with a temporary name.
    With create_info->frm_only == 1 this creates a .frm file only.
    We don't log the statement, it will be logged later.
  */
  tmp_disable_binlog(thd);
  error= mysql_create_table(thd, new_db, tmp_name,
                            create_info,create_list,key_list,1,0,0);
  reenable_binlog(thd);
  if (error)
    DBUG_RETURN(error);

  /* Open the table if we need to copy the data. */
  if (need_copy_table)
  {
    if (table->s->tmp_table)
    {
      TABLE_LIST tbl;
      bzero((void*) &tbl, sizeof(tbl));
      tbl.db= new_db;
      tbl.table_name= tbl.alias= tmp_name;
      /* Table is in thd->temporary_tables */
      new_table= open_table(thd, &tbl, thd->mem_root, (bool*) 0,
                            MYSQL_LOCK_IGNORE_FLUSH);
    }
    else
    {
      char path[FN_REFLEN];
      /* table is a normal table: Create temporary table in same directory */
      build_table_filename(path, sizeof(path), new_db, tmp_name, "",
                           FN_IS_TMP);
      new_table=open_temporary_table(thd, path, new_db, tmp_name,0);
    }
    if (!new_table)
      goto err1;
  }

  /* Copy the data if necessary. */
  thd->count_cuted_fields= CHECK_FIELD_WARN;	// calc cuted fields
  thd->cuted_fields=0L;
  thd->proc_info="copy to tmp table";
  copied=deleted=0;
  if (new_table && !(new_table->file->ha_table_flags() & HA_NO_COPY_ON_ALTER))
  {
    /* We don't want update TIMESTAMP fields during ALTER TABLE. */
    new_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    new_table->next_number_field=new_table->found_next_number_field;
    error=copy_data_between_tables(table, new_table, create_list, ignore,
				   order_num, order, &copied, &deleted);
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  /* If we did not need to copy, we might still need to add/drop indexes. */
  if (! new_table)
  {
    uint          *key_numbers;
    uint          *keyno_p;
    KEY           *key_info;
    KEY           *key;
    uint          *idx_p;
    uint          *idx_end_p;
    KEY_PART_INFO *key_part;
    KEY_PART_INFO *part_end;
    DBUG_PRINT("info", ("No new_table, checking add/drop index"));

    table->file->prepare_for_alter();
    if (index_add_count)
    {
#ifdef XXX_TO_BE_DONE_LATER_BY_WL3020_AND_WL1892
      if (! need_lock_for_indexes)
      {
        /* Downgrade the write lock. */
        mysql_lock_downgrade_write(thd, table, TL_WRITE_ALLOW_WRITE);
      }

      /* Create a new .frm file for crash recovery. */
      /* TODO: Must set INDEX_TO_BE_ADDED flags in the frm file. */
      VOID(pthread_mutex_lock(&LOCK_open));
      error= (mysql_create_frm(thd, reg_path, db, table_name,
                               create_info, prepared_create_list, key_count,
                               key_info_buffer, table->file) ||
              table->file->create_handler_files(reg_path, NULL, CHF_INDEX_FLAG,
                                                create_info));
      VOID(pthread_mutex_unlock(&LOCK_open));
      if (error)
        goto err1;
#endif

      /* The add_index() method takes an array of KEY structs. */
      key_info= (KEY*) thd->alloc(sizeof(KEY) * index_add_count);
      key= key_info;
      for (idx_p= index_add_buffer, idx_end_p= idx_p + index_add_count;
           idx_p < idx_end_p;
           idx_p++, key++)
      {
        /* Copy the KEY struct. */
        *key= key_info_buffer[*idx_p];
        /* Fix the key parts. */
        part_end= key->key_part + key->key_parts;
        for (key_part= key->key_part; key_part < part_end; key_part++)
          key_part->field= table->field[key_part->fieldnr];
      }
      /* Add the indexes. */
      if ((error= table->file->add_index(table, key_info, index_add_count)))
      {
        /*
          Exchange the key_info for the error message. If we exchange
          key number by key name in the message later, we need correct info.
        */
        KEY *save_key_info= table->key_info;
        table->key_info= key_info;
        table->file->print_error(error, MYF(0));
        table->key_info= save_key_info;
        goto err1;
      }
    }
    /*end of if (index_add_count)*/

    if (index_drop_count)
    {
#ifdef XXX_TO_BE_DONE_LATER_BY_WL3020_AND_WL1892
      /* Create a new .frm file for crash recovery. */
      /* TODO: Must set INDEX_IS_ADDED in the frm file. */
      /* TODO: Must set INDEX_TO_BE_DROPPED in the frm file. */
      VOID(pthread_mutex_lock(&LOCK_open));
      error= (mysql_create_frm(thd, reg_path, db, table_name,
                               create_info, prepared_create_list, key_count,
                               key_info_buffer, table->file) ||
              table->file->create_handler_files(reg_path, NULL, CHF_INDEX_FLAG,
                                                create_info));
      VOID(pthread_mutex_unlock(&LOCK_open));
      if (error)
        goto err1;

      if (! need_lock_for_indexes)
      {
        LOCK_PARAM_TYPE lpt;

        lpt.thd= thd;
        lpt.table= table;
        lpt.db= db;
        lpt.table_name= table_name;
        lpt.create_info= create_info;
        lpt.create_list= &create_list;
        lpt.key_count= key_count;
        lpt.key_info_buffer= key_info_buffer;
        abort_and_upgrade_lock(lpt);
      }
#endif

      /* The prepare_drop_index() method takes an array of key numbers. */
      key_numbers= (uint*) thd->alloc(sizeof(uint) * index_drop_count);
      keyno_p= key_numbers;
      /* Get the number of each key. */
      for (idx_p= index_drop_buffer, idx_end_p= idx_p + index_drop_count;
           idx_p < idx_end_p;
           idx_p++, keyno_p++)
        *keyno_p= *idx_p;
      /*
        Tell the handler to prepare for drop indexes.
        This re-numbers the indexes to get rid of gaps.
      */
      if ((error= table->file->prepare_drop_index(table, key_numbers,
                                                  index_drop_count)))
      {
        table->file->print_error(error, MYF(0));
        goto err1;
      }

#ifdef XXX_TO_BE_DONE_LATER_BY_WL3020
      if (! need_lock_for_indexes)
      {
        /* Downgrade the lock again. */
        if (table->reginfo.lock_type == TL_WRITE_ALLOW_READ)
        {
          LOCK_PARAM_TYPE lpt;

          lpt.thd= thd;
          lpt.table= table;
          lpt.db= db;
          lpt.table_name= table_name;
          lpt.create_info= create_info;
          lpt.create_list= &create_list;
          lpt.key_count= key_count;
          lpt.key_info_buffer= key_info_buffer;
          close_open_tables_and_downgrade(lpt);
        }
      }
#endif

      /* Tell the handler to finally drop the indexes. */
      if ((error= table->file->final_drop_index(table)))
      {
        table->file->print_error(error, MYF(0));
        goto err1;
      }
    }
    /*end of if (index_drop_count)*/

    /*
      The final .frm file is already created as a temporary file
      and will be renamed to the original table name later.
    */

    /* Need to commit before a table is unlocked (NDB requirement). */
    DBUG_PRINT("info", ("Committing before unlocking table"));
    if (ha_commit_stmt(thd) || ha_commit(thd))
      goto err1;
    committed= 1;
  }
  /*end of if (! new_table) for add/drop index*/

  if (table->s->tmp_table != NO_TMP_TABLE)
  {
    /* We changed a temporary table */
    if (error)
      goto err1;
    /* Close lock if this is a transactional table */
    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock=0;
    }
    /* Remove link to old table and rename the new one */
    close_temporary_table(thd, table, 1, 1);
    /* Should pass the 'new_name' as we store table name in the cache */
    if (rename_temporary_table(thd, new_table, new_db, new_name))
      goto err1;
    /* We don't replicate alter table statement on temporary tables */
    if (!thd->current_stmt_binlog_row_based)
      write_bin_log(thd, TRUE, thd->query, thd->query_length);
    goto end_temporary;
  }

  if (new_table)
  {
    /* close temporary table that will be the new table */
    intern_close_table(new_table);
    my_free((gptr) new_table,MYF(0));
  }
  VOID(pthread_mutex_lock(&LOCK_open));
  if (error)
  {
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }

  /*
    Data is copied.  Now we rename the old table to a temp name,
    rename the new one to the old name, remove all entries from the old table
    from the cache, free all locks, close the old table and remove it.
  */

  thd->proc_info="rename result table";
  my_snprintf(old_name, sizeof(old_name), "%s2-%lx-%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, old_name);
  if (new_name != table_name || new_db != db)
  {
    if (!access(new_name_buff,F_OK))
    {
      error=1;
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name_buff);
      VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }

#if !defined( __WIN__)
  if (table->file->has_transactions())
#endif
  {
    /*
      Win32 and InnoDB can't drop a table that is in use, so we must
      close the original table at before doing the rename
    */
    table->s->version= 0;                	// Force removal of table def
    close_cached_table(thd, table);
    table=0;					// Marker that table is closed
    no_table_reopen= TRUE;
  }
#if !defined( __WIN__)
  else
    table->file->extra(HA_EXTRA_FORCE_REOPEN);	// Don't use this file anymore
#endif


  error=0;
  if (!need_copy_table)
    new_db_type=old_db_type= NULL; // this type cannot happen in regular ALTER
  if (mysql_rename_table(old_db_type, db, table_name, db, old_name,
                         FN_TO_IS_TMP))
  {
    error=1;
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
  }
  else if (mysql_rename_table(new_db_type,new_db,tmp_name,new_db,
			      new_alias, FN_FROM_IS_TMP) ||
           (new_name != table_name || new_db != db) && // we also do rename
           Table_triggers_list::change_table_name(thd, db, table_name,
                                                  new_db, new_alias))
  {
    /* Try to get everything back. */
    error=1;
    VOID(quick_rm_table(new_db_type,new_db,new_alias, 0));
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
    VOID(mysql_rename_table(old_db_type, db, old_name, db, alias,
                            FN_FROM_IS_TMP));
  }
  if (error)
  {
    /*
      This shouldn't happen.  We solve this the safe way by
      closing the locked table.
    */
    if (table)
    {
      table->s->version= 0;            	        // Force removal of table def
      close_cached_table(thd,table);
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  if (! need_copy_table)
  {
    if (! table)
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      if (! (table= open_ltable(thd, table_list, TL_WRITE_ALLOW_READ)))
        goto err;
      VOID(pthread_mutex_lock(&LOCK_open));
    }
    /* Tell the handler that a new frm file is in place. */
    if (table->file->create_handler_files(path, NULL, CHF_INDEX_FLAG,
                                          create_info))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }
  if (thd->lock || new_name != table_name || no_table_reopen)  // True if WIN32
  {
    /*
      Not table locking or alter table with rename.
      Free locks and remove old table
    */
    if (table)
    {
      table->s->version= 0;              	// Force removal of table def
      close_cached_table(thd,table);
    }
    VOID(quick_rm_table(old_db_type, db, old_name, FN_IS_TMP));
  }
  else
  {
    /*
      Using LOCK TABLES without rename.
      This code is never executed on WIN32!
      Remove old renamed table, reopen table and get new locks
    */
    if (table)
    {
      VOID(table->file->extra(HA_EXTRA_FORCE_REOPEN)); // Use new file
      /* Mark in-use copies old */
      remove_table_from_cache(thd,db,table_name,RTFC_NO_FLAG);
      /* end threads waiting on lock */
      mysql_lock_abort(thd,table, TRUE);
    }
    VOID(quick_rm_table(old_db_type, db, old_name, FN_IS_TMP));
    if (close_data_tables(thd,db,table_name) ||
	reopen_tables(thd,1,0))
    {						// This shouldn't happen
      if (table)
      {
        table->s->version= 0;                   // Force removal of table def
	close_cached_table(thd,table);		// Remove lock for table
      }
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  broadcast_refresh();
  /*
    The ALTER TABLE is always in its own transaction.
    Commit must not be called while LOCK_open is locked. It could call
    wait_if_global_read_lock(), which could create a deadlock if called
    with LOCK_open.
  */
  if (!committed)
  {
    error = ha_commit_stmt(thd);
    if (ha_commit(thd))
      error=1;
    if (error)
      goto err;
  }
  thd->proc_info="end";

  ha_binlog_log_query(thd, create_info->db_type, LOGCOM_ALTER_TABLE,
                      thd->query, thd->query_length,
                      db, table_name);

  DBUG_ASSERT(!(mysql_bin_log.is_open() && thd->current_stmt_binlog_row_based &&
                (create_info->options & HA_LEX_CREATE_TMP_TABLE)));
  write_bin_log(thd, TRUE, thd->query, thd->query_length);
  /*
    TODO RONM: This problem needs to handled for Berkeley DB partitions
    as well
  */
  if (ha_check_storage_engine_flag(old_db_type,HTON_FLUSH_AFTER_RENAME))
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown.
    */
    char path[FN_REFLEN];
    build_table_filename(path, sizeof(path), new_db, table_name, "", 0);
    table=open_temporary_table(thd, path, new_db, tmp_name,0);
    if (table)
    {
      intern_close_table(table);
      my_free((char*) table, MYF(0));
    }
    else
      sql_print_warning("Could not open table %s.%s after rename\n",
                        new_db,table_name);
    ha_flush_logs(old_db_type);
  }
  table_list->table=0;				// For query cache
  query_cache_invalidate3(thd, table_list, 0);

end_temporary:
  my_snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
	      (ulong) (copied + deleted), (ulong) deleted,
	      (ulong) thd->cuted_fields);
  if (do_send_ok)
    send_ok(thd,copied+deleted,0L,tmp_name);
  thd->some_tables_deleted=0;
  DBUG_RETURN(FALSE);

 err1:
  if (new_table)
  {
    /* close_temporary_table() frees the new_table pointer. */
    close_temporary_table(thd, new_table, 1, 1);
  }
  else
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));

 err:
  DBUG_RETURN(TRUE);
}
/* mysql_alter_table */

static int
copy_data_between_tables(TABLE *from,TABLE *to,
			 List<create_field> &create,
                         bool ignore,
			 uint order_num, ORDER *order,
			 ha_rows *copied,
			 ha_rows *deleted)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  THD *thd= current_thd;
  uint length;
  SORT_FIELD *sortorder;
  READ_RECORD info;
  TABLE_LIST   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows;
  bool auto_increment_field_copied= 0;
  ulong save_sql_mode;
  ulonglong prev_insert_id;
  DBUG_ENTER("copy_data_between_tables");

  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.
    
    This needs to be done before external_lock
  */
  error= ha_enable_transaction(thd, FALSE);
  if (error)
    DBUG_RETURN(-1);
  
  if (!(copy= new Copy_field[to->s->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  if (to->file->ha_external_lock(thd, F_WRLCK))
    DBUG_RETURN(-1);

  /* We can abort alter table for any table type */
  thd->no_trans_update= 0;
  thd->abort_on_warning= !ignore && test(thd->variables.sql_mode &
                                         (MODE_STRICT_TRANS_TABLES |
                                          MODE_STRICT_ALL_TABLES));

  from->file->info(HA_STATUS_VARIABLE);
  to->file->ha_start_bulk_insert(from->file->stats.records);

  save_sql_mode= thd->variables.sql_mode;

  List_iterator<create_field> it(create);
  create_field *def;
  copy_end=copy;
  for (Field **ptr=to->field ; *ptr ; ptr++)
  {
    def=it++;
    if (def->field)
    {
      if (*ptr == to->next_number_field)
      {
        auto_increment_field_copied= TRUE;
        /*
          If we are going to copy contents of one auto_increment column to
          another auto_increment column it is sensible to preserve zeroes.
          This condition also covers case when we are don't actually alter
          auto_increment column.
        */
        if (def->field == from->found_next_number_field)
          thd->variables.sql_mode|= MODE_NO_AUTO_VALUE_ON_ZERO;
      }
      (copy_end++)->set(*ptr,def->field,0);
    }

  }

  found_count=delete_count=0;

  if (order)
  {
    from->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
					      MYF(MY_FAE | MY_ZEROFILL));
    bzero((char*) &tables,sizeof(tables));
    tables.table= from;
    tables.alias= tables.table_name= from->s->table_name.str;
    tables.db=    from->s->db.str;
    error=1;

    if (thd->lex->select_lex.setup_ref_array(thd, order_num) ||
	setup_order(thd, thd->lex->select_lex.ref_pointer_array,
		    &tables, fields, all_fields, order) ||
	!(sortorder=make_unireg_sortorder(order, &length)) ||
	(from->sort.found_records = filesort(thd, from, sortorder, length,
					     (SQL_SELECT *) 0, HA_POS_ERROR, 1,
					     &examined_rows)) ==
	HA_POS_ERROR)
      goto err;
  };

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();
  init_read_record(&info, thd, from, (SQL_SELECT *) 0, 1,1);
  if (ignore)
    to->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  thd->row_count= 0;
  restore_record(to, s->default_values);        // Create empty record
  while (!(error=info.read_record(&info)))
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      error= 1;
      break;
    }
    thd->row_count++;
    if (to->next_number_field)
    {
      if (auto_increment_field_copied)
        to->auto_increment_field_not_null= TRUE;
      else
        to->next_number_field->reset();
    }
    
    for (Copy_field *copy_ptr=copy ; copy_ptr != copy_end ; copy_ptr++)
    {
      copy_ptr->do_copy(copy_ptr);
    }
    prev_insert_id= to->file->next_insert_id;
    if ((error=to->file->ha_write_row((byte*) to->record[0])))
    {
      if (!ignore ||
          to->file->is_fatal_error(error, HA_CHECK_DUP))
      {
         if (!to->file->is_fatal_error(error, HA_CHECK_DUP))
         {
           uint key_nr= to->file->get_dup_key(error);
           if ((int) key_nr >= 0)
           {
             const char *err_msg= ER(ER_DUP_ENTRY);
             if (key_nr == 0 &&
                 (to->key_info[0].key_part[0].field->flags &
                  AUTO_INCREMENT_FLAG))
               err_msg= ER(ER_DUP_ENTRY_AUTOINCREMENT_CASE);
             to->file->print_keydup_error(key_nr, err_msg);
             break;
           }
         }

	to->file->print_error(error,MYF(0));
	break;
      }
      to->file->restore_auto_increment(prev_insert_id);
      delete_count++;
    }
    else
      found_count++;
  }
  end_read_record(&info);
  free_io_cache(from);
  delete [] copy;				// This is never 0

  if (to->file->ha_end_bulk_insert() && error <= 0)
  {
    to->file->print_error(my_errno,MYF(0));
    error=1;
  }
  to->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  ha_enable_transaction(thd,TRUE);

  /*
    Ensure that the new table is saved properly to disk so that we
    can do a rename
  */
  if (ha_commit_stmt(thd))
    error=1;
  if (ha_commit(thd))
    error=1;

 err:
  thd->variables.sql_mode= save_sql_mode;
  thd->abort_on_warning= 0;
  free_io_cache(from);
  *copied= found_count;
  *deleted=delete_count;
  to->file->ha_release_auto_increment();
  if (to->file->ha_external_lock(thd,F_UNLCK))
    error=1;
  DBUG_RETURN(error > 0 ? -1 : 0);
}


/*
  Recreates tables by calling mysql_alter_table().

  SYNOPSIS
    mysql_recreate_table()
    thd			Thread handler
    tables		Tables to recreate
    do_send_ok          If we should send_ok() or leave it to caller

 RETURN
    Like mysql_alter_table().
*/
bool mysql_recreate_table(THD *thd, TABLE_LIST *table_list,
                          bool do_send_ok)
{
  DBUG_ENTER("mysql_recreate_table");
  LEX *lex= thd->lex;
  HA_CREATE_INFO create_info;
  lex->create_list.empty();
  lex->key_list.empty();
  lex->col_list.empty();
  lex->alter_info.reset();
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type= 0;
  create_info.row_type=ROW_TYPE_NOT_USED;
  create_info.default_table_charset=default_charset_info;
  /* Force alter table to recreate table */
  lex->alter_info.flags= (ALTER_CHANGE_COLUMN | ALTER_RECREATE);
  DBUG_RETURN(mysql_alter_table(thd, NullS, NullS, &create_info,
                                table_list, lex->create_list,
                                lex->key_list, 0, (ORDER *) 0,
                                0, &lex->alter_info, do_send_ok));
}


bool mysql_checksum_table(THD *thd, TABLE_LIST *tables,
                          HA_CHECK_OPT *check_opt)
{
  TABLE_LIST *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_checksum_table");

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_int("Checksum",(longlong) 1,21));
  item->maybe_null= 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    TABLE *t;

    strxmov(table_name, table->db ,".", table->table_name, NullS);

    t= table->table= open_ltable(thd, table, TL_READ);
    thd->clear_error();			// these errors shouldn't get client

    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);

    if (!t)
    {
      /* Table didn't exist */
      protocol->store_null();
      thd->clear_error();
    }
    else
    {
      t->pos_in_table_list= table;

      if (t->file->ha_table_flags() & HA_HAS_CHECKSUM &&
	  !(check_opt->flags & T_EXTEND))
	protocol->store((ulonglong)t->file->checksum());
      else if (!(t->file->ha_table_flags() & HA_HAS_CHECKSUM) &&
	       (check_opt->flags & T_QUICK))
	protocol->store_null();
      else
      {
	/* calculating table's checksum */
	ha_checksum crc= 0;
        uchar null_mask=256 -  (1 << t->s->last_null_bit_pos);

        t->use_all_columns();

	if (t->file->ha_rnd_init(1))
	  protocol->store_null();
	else
	{
	  for (;;)
	  {
	    ha_checksum row_crc= 0;
            int error= t->file->rnd_next(t->record[0]);
            if (unlikely(error))
            {
              if (error == HA_ERR_RECORD_DELETED)
                continue;
              break;
            }
	    if (t->s->null_bytes)
            {
              /* fix undefined null bits */
              t->record[0][t->s->null_bytes-1] |= null_mask;
              if (!(t->s->db_create_options & HA_OPTION_PACK_RECORD))
                t->record[0][0] |= 1;

	      row_crc= my_checksum(row_crc, t->record[0], t->s->null_bytes);
            }

	    for (uint i= 0; i < t->s->fields; i++ )
	    {
	      Field *f= t->field[i];
	      if ((f->type() == FIELD_TYPE_BLOB) ||
                  (f->type() == MYSQL_TYPE_VARCHAR))
	      {
		String tmp;
		f->val_str(&tmp);
		row_crc= my_checksum(row_crc, (byte*) tmp.ptr(), tmp.length());
	      }
	      else
		row_crc= my_checksum(row_crc, (byte*) f->ptr,
				     f->pack_length());
	    }

	    crc+= row_crc;
	  }
	  protocol->store((ulonglong)crc);
          t->file->ha_rnd_end();
	}
      }
      thd->clear_error();
      close_thread_tables(thd);
      table->table=0;				// For query cache
    }
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(FALSE);

 err:
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(TRUE);
}

static bool check_engine(THD *thd, const char *table_name,
                         HA_CREATE_INFO *create_info)
{
  handlerton **new_engine= &create_info->db_type;
  handlerton *req_engine= *new_engine;
  bool no_substitution=
        test(thd->variables.sql_mode & MODE_NO_ENGINE_SUBSTITUTION);
  if (!(*new_engine= ha_checktype(thd, ha_legacy_type(req_engine),
                                  no_substitution, 1)))
    return TRUE;

  if (req_engine && req_engine != *new_engine)
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_WARN_USING_OTHER_HANDLER,
                       ER(ER_WARN_USING_OTHER_HANDLER),
                       ha_resolve_storage_engine_name(*new_engine),
                       table_name);
  }
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE &&
      ha_check_storage_engine_flag(*new_engine, HTON_TEMPORARY_NOT_SUPPORTED))
  {
    if (create_info->used_fields & HA_CREATE_USED_ENGINE)
    {
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               hton2plugin[(*new_engine)->slot]->name.str, "TEMPORARY");
      *new_engine= 0;
      return TRUE;
    }
    *new_engine= &myisam_hton;
  }
  return FALSE;
}
