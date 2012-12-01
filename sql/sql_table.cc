/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* drop and alter of tables */

#include "mysql_priv.h"
#include <hash.h>
#include <myisam.h>
#include <my_dir.h>
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_show.h"
#include "debug_sync.h"

#ifdef __WIN__
#include <io.h>
#endif

int creating_table= 0;        // How many mysql_create_table are running

const char *primary_key_name="PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(TABLE *from,TABLE *to,
                                    List<Create_field> &create, bool ignore,
				    uint order_num, ORDER *order,
				    ha_rows *copied,ha_rows *deleted,
                                    enum enum_enable_or_disable keys_onoff,
                                    bool error_if_not_empty);

static bool prepare_blob_field(THD *thd, Create_field *sql_field);
static bool check_engine(THD *, const char *, HA_CREATE_INFO *);
static int
mysql_prepare_create_table(THD *thd, HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                           uint *db_options,
                           handler *file, KEY **key_info_buffer,
                           uint *key_count, int select_field_count);
static bool
mysql_prepare_alter_table(THD *thd, TABLE *table,
                          HA_CREATE_INFO *create_info,
                          Alter_info *alter_info);

#ifndef DBUG_OFF

/* Wait until we get a 'mysql_kill' signal */

static void wait_for_kill_signal(THD *thd)
{
  while (thd->killed == 0)
    sleep(1);
  // Reset signal and continue as if nothing happend
  thd->killed= THD::NOT_KILLED;
}
#endif


/**
  @brief Helper function for explain_filename
  @param thd          Thread handle
  @param to_p         Explained name in system_charset_info
  @param end_p        End of the to_p buffer
  @param name         Name to be converted
  @param name_len     Length of the name, in bytes
*/
static char* add_identifier(THD* thd, char *to_p, const char * end_p,
                            const char* name, uint name_len)
{
  uint res;
  uint errors;
  const char *conv_name;
  char tmp_name[FN_REFLEN];
  char conv_string[FN_REFLEN];
  int quote;

  DBUG_ENTER("add_identifier");
  if (!name[name_len])
    conv_name= name;
  else
  {
    strnmov(tmp_name, name, name_len);
    tmp_name[name_len]= 0;
    conv_name= tmp_name;
  }
  res= strconvert(&my_charset_filename, conv_name, system_charset_info,
                  conv_string, FN_REFLEN, &errors);
  if (!res || errors)
  {
    DBUG_PRINT("error", ("strconvert of '%s' failed with %u (errors: %u)", conv_name, res, errors));
    conv_name= name;
  }
  else
  {
    DBUG_PRINT("info", ("conv '%s' -> '%s'", conv_name, conv_string));
    conv_name= conv_string;
  }

  quote = thd ? get_quote_char_for_identifier(thd, conv_name, res - 1) : '"';

  if (quote != EOF && (end_p - to_p > 2))
  {
    *(to_p++)= (char) quote;
    while (*conv_name && (end_p - to_p - 1) > 0)
    {
      uint length= my_mbcharlen(system_charset_info, *conv_name);
      if (!length)
        length= 1;
      if (length == 1 && *conv_name == (char) quote)
      { 
        if ((end_p - to_p) < 3)
          break;
        *(to_p++)= (char) quote;
        *(to_p++)= *(conv_name++);
      }
      else if (((long) length) < (end_p - to_p))
      {
        to_p= strnmov(to_p, conv_name, length);
        conv_name+= length;
      }
      else
        break;                               /* string already filled */
    }
    if (end_p > to_p) {
      *(to_p++)= (char) quote;
      if (end_p > to_p)
	*to_p= 0; /* terminate by NUL, but do not include it in the count */
    }
  }
  else
    to_p= strnmov(to_p, conv_name, end_p - to_p);
  DBUG_RETURN(to_p);
}


/**
  @brief Explain a path name by split it to database, table etc.
  
  @details Break down the path name to its logic parts
  (database, table, partition, subpartition).
  filename_to_tablename cannot be used on partitions, due to the #P# part.
  There can be up to 6 '#', #P# for partition, #SP# for subpartition
  and #TMP# or #REN# for temporary or renamed partitions.
  This should be used when something should be presented to a user in a
  diagnostic, error etc. when it would be useful to know what a particular
  file [and directory] means. Such as SHOW ENGINE STATUS, error messages etc.

   @param      thd          Thread handle
   @param      from         Path name in my_charset_filename
                            Null terminated in my_charset_filename, normalized
                            to use '/' as directory separation character.
   @param      to           Explained name in system_charset_info
   @param      to_length    Size of to buffer
   @param      explain_mode Requested output format.
                            EXPLAIN_ALL_VERBOSE ->
                            [Database `db`, ]Table `tbl`[,[ Temporary| Renamed]
                            Partition `p` [, Subpartition `sp`]]
                            EXPLAIN_PARTITIONS_VERBOSE -> `db`.`tbl`
                            [[ Temporary| Renamed] Partition `p`
                            [, Subpartition `sp`]]
                            EXPLAIN_PARTITIONS_AS_COMMENT -> `db`.`tbl` |*
                            [,[ Temporary| Renamed] Partition `p`
                            [, Subpartition `sp`]] *|
                            (| is really a /, and it is all in one line)

   @retval     Length of returned string
*/

uint explain_filename(THD* thd,
		      const char *from,
                      char *to,
                      uint to_length,
                      enum_explain_filename_mode explain_mode)
{
  uint res= 0;
  char *to_p= to;
  char *end_p= to_p + to_length;
  const char *db_name= NULL;
  int  db_name_len= 0;
  const char *table_name;
  int  table_name_len= 0;
  const char *part_name= NULL;
  int  part_name_len= 0;
  const char *subpart_name= NULL;
  int  subpart_name_len= 0;
  enum enum_file_name_type {NORMAL, TEMP, RENAMED} name_type= NORMAL;
  const char *tmp_p;
  DBUG_ENTER("explain_filename");
  DBUG_PRINT("enter", ("from '%s'", from));
  tmp_p= from;
  table_name= from;
  /*
    If '/' then take last directory part as database.
    '/' is the directory separator, not FN_LIB_CHAR
  */
  while ((tmp_p= strchr(tmp_p, '/')))
  {
    db_name= table_name;
    /* calculate the length */
    db_name_len= tmp_p - db_name;
    tmp_p++;
    table_name= tmp_p;
  }
  tmp_p= table_name;
  while (!res && (tmp_p= strchr(tmp_p, '#')))
  {
    tmp_p++;
    switch (tmp_p[0]) {
    case 'P':
    case 'p':
      if (tmp_p[1] == '#')
        part_name= tmp_p + 2;
      else
        res= 1;
      tmp_p+= 2;
      break;
    case 'S':
    case 's':
      if ((tmp_p[1] == 'P' || tmp_p[1] == 'p') && tmp_p[2] == '#')
      {
        part_name_len= tmp_p - part_name - 1;
        subpart_name= tmp_p + 3;
      }
      else
        res= 2;
      tmp_p+= 3;
      break;
    case 'T':
    case 't':
      if ((tmp_p[1] == 'M' || tmp_p[1] == 'm') &&
          (tmp_p[2] == 'P' || tmp_p[2] == 'p') &&
          tmp_p[3] == '#' && !tmp_p[4])
        name_type= TEMP;
      else
        res= 3;
      tmp_p+= 4;
      break;
    case 'R':
    case 'r':
      if ((tmp_p[1] == 'E' || tmp_p[1] == 'e') &&
          (tmp_p[2] == 'N' || tmp_p[2] == 'n') &&
          tmp_p[3] == '#' && !tmp_p[4])
        name_type= RENAMED;
      else
        res= 4;
      tmp_p+= 4;
      break;
    default:
      res= 5;
    }
  }
  if (res)
  {
    /* Better to give something back if we fail parsing, than nothing at all */
    DBUG_PRINT("info", ("Error in explain_filename: %u", res));
    sql_print_warning("Invalid (old?) table or database name '%s'", from);
    DBUG_RETURN(my_snprintf(to, to_length,
                            "<result %u when explaining filename '%s'>",
                            res, from));
  }
  if (part_name)
  {
    table_name_len= part_name - table_name - 3;
    if (subpart_name)
      subpart_name_len= strlen(subpart_name);
    else
      part_name_len= strlen(part_name);
    if (name_type != NORMAL)
    {
      if (subpart_name)
        subpart_name_len-= 5;
      else
        part_name_len-= 5;
    }
  }
  else
    table_name_len= strlen(table_name);
  if (db_name)
  {
    if (explain_mode == EXPLAIN_ALL_VERBOSE)
    {
      to_p= strnmov(to_p, ER(ER_DATABASE_NAME), end_p - to_p);
      *(to_p++)= ' ';
      to_p= add_identifier(thd, to_p, end_p, db_name, db_name_len);
      to_p= strnmov(to_p, ", ", end_p - to_p);
    }
    else
    {
      to_p= add_identifier(thd, to_p, end_p, db_name, db_name_len);
      to_p= strnmov(to_p, ".", end_p - to_p);
    }
  }
  if (explain_mode == EXPLAIN_ALL_VERBOSE)
  {
    to_p= strnmov(to_p, ER(ER_TABLE_NAME), end_p - to_p);
    *(to_p++)= ' ';
    to_p= add_identifier(thd, to_p, end_p, table_name, table_name_len);
  }
  else
    to_p= add_identifier(thd, to_p, end_p, table_name, table_name_len);
  if (part_name)
  {
    if (explain_mode == EXPLAIN_PARTITIONS_AS_COMMENT)
      to_p= strnmov(to_p, " /* ", end_p - to_p);
    else if (explain_mode == EXPLAIN_PARTITIONS_VERBOSE)
      to_p= strnmov(to_p, " ", end_p - to_p);
    else
      to_p= strnmov(to_p, ", ", end_p - to_p);
    if (name_type != NORMAL)
    {
      if (name_type == TEMP)
        to_p= strnmov(to_p, ER(ER_TEMPORARY_NAME), end_p - to_p);
      else
        to_p= strnmov(to_p, ER(ER_RENAMED_NAME), end_p - to_p);
      to_p= strnmov(to_p, " ", end_p - to_p);
    }
    to_p= strnmov(to_p, ER(ER_PARTITION_NAME), end_p - to_p);
    *(to_p++)= ' ';
    to_p= add_identifier(thd, to_p, end_p, part_name, part_name_len);
    if (subpart_name)
    {
      to_p= strnmov(to_p, ", ", end_p - to_p);
      to_p= strnmov(to_p, ER(ER_SUBPARTITION_NAME), end_p - to_p);
      *(to_p++)= ' ';
      to_p= add_identifier(thd, to_p, end_p, subpart_name, subpart_name_len);
    }
    if (explain_mode == EXPLAIN_PARTITIONS_AS_COMMENT)
      to_p= strnmov(to_p, " */", end_p - to_p);
  }
  DBUG_PRINT("exit", ("to '%s'", to));
  DBUG_RETURN(to_p - to);
}


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
  size_t res;
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


/**
  Check if given string begins with "#mysql50#" prefix
  
  @param   name          string to check cut 
  
  @retval
    FALSE  no prefix found
  @retval
    TRUE   prefix found
*/

bool check_mysql50_prefix(const char *name)
{
  return (name[0] == '#' && 
         !strncmp(name, MYSQL50_TABLE_NAME_PREFIX,
                  MYSQL50_TABLE_NAME_PREFIX_LENGTH));
}


/**
  Check if given string begins with "#mysql50#" prefix, cut it if so.
  
  @param   from          string to check and cut 
  @param   to[out]       buffer for result string
  @param   to_length     its size
  
  @retval
    0      no prefix found
  @retval
    non-0  result string length
*/

uint check_n_cut_mysql50_prefix(const char *from, char *to, uint to_length)
{
  if (check_mysql50_prefix(from))
    return (uint) (strmake(to, from + MYSQL50_TABLE_NAME_PREFIX_LENGTH,
                           to_length - 1) - to);
  return 0;
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

  if ((length= check_n_cut_mysql50_prefix(from, to, to_length)))
  {
    /*
      Check if the name supplied is a valid mysql 5.0 name and 
      make the name a zero length string if it's not.
      Note that just returning zero length is not enough : 
      a lot of places don't check the return value and expect 
      a zero terminated string.
    */  
    if (check_table_name(to, length, TRUE))
    {
      to[0]= 0;
      length= 0;
    }
    DBUG_RETURN(length);
  }
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
                                This may be the same as table_name.
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
  char dbbuff[FN_REFLEN];
  char tbbuff[FN_REFLEN];
  DBUG_ENTER("build_table_filename");
  DBUG_PRINT("enter", ("db: '%s'  table_name: '%s'  ext: '%s'  flags: %x",
                       db, table_name, ext, flags));

  if (flags & FN_IS_TMP) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    strnmov(tbbuff, table_name, sizeof(tbbuff));
  else
    VOID(tablename_to_filename(table_name, tbbuff, sizeof(tbbuff)));

  VOID(tablename_to_filename(db, dbbuff, sizeof(dbbuff)));

  char *end = buff + bufflen;
  /* Don't add FN_ROOTDIR if mysql_data_home already includes it */
  char *pos = strnmov(buff, mysql_data_home, bufflen);
  size_t rootdir_len= strlen(FN_ROOTDIR);
  if (pos - rootdir_len >= buff &&
      memcmp(pos - rootdir_len, FN_ROOTDIR, rootdir_len) != 0)
    pos= strnmov(pos, FN_ROOTDIR, end - pos);
  pos= strxnmov(pos, end - pos, dbbuff, FN_ROOTDIR, NullS);
#ifdef USE_SYMDIR
  unpack_dirname(buff, buff);
  pos= strend(buff);
#endif
  pos= strxnmov(pos, end - pos, tbbuff, ext, NullS);

  DBUG_PRINT("exit", ("buff: '%s'", buff));
  DBUG_RETURN(pos - buff);
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
  DBUG_ENTER("build_tmptable_filename");

  char *p= strnmov(buff, mysql_tmpdir, bufflen);
  my_snprintf(p, bufflen - (p - buff), "/%s%lx_%lx_%x%s",
              tmp_file_prefix, current_pid,
              thd->thread_id, thd->tmp_table++, reg_ext);

  if (lower_case_table_names)
  {
    /* Convert all except tmpdir to lower case */
    my_casedn_str(files_charset_info, p);
  }

  size_t length= unpack_filename(buff, buff);
  DBUG_PRINT("exit", ("buff: '%s'", buff));
  DBUG_RETURN(length);
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


struct st_global_ddl_log
{
  char file_name_str[FN_REFLEN];
  char *file_name;
  DDL_LOG_MEMORY_ENTRY *first_free;
  DDL_LOG_MEMORY_ENTRY *first_used;
  uint num_entries;
  File file_id;
  uint name_len;
  uint io_size;
  bool inited;
  bool do_release;
  bool recovery_phase;
  st_global_ddl_log() : inited(false), do_release(false) {}
};

st_global_ddl_log global_ddl_log;

pthread_mutex_t LOCK_gdl;

#define DDL_LOG_ENTRY_TYPE_POS 0
#define DDL_LOG_ACTION_TYPE_POS 1
#define DDL_LOG_PHASE_POS 2
#define DDL_LOG_NEXT_ENTRY_POS 4
#define DDL_LOG_NAME_POS 8

#define DDL_LOG_NUM_ENTRY_POS 0
#define DDL_LOG_NAME_LEN_POS 4
#define DDL_LOG_IO_SIZE_POS 8
#define DDL_LOG_HEADER_SIZE 12

/**
  Read one entry from ddl log file.
  @param[out]  file_entry_buf  Buffer to read into
  @param       entry_no        Entry number to read
  @param       size            Number of bytes of the entry to read

  @return Operation status
    @retval true   Error
    @retval false  Success
*/

static bool read_ddl_log_file_entry(uchar *file_entry_buf,
                                    uint entry_no,
                                    uint size)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  uint io_size= global_ddl_log.io_size;
  DBUG_ENTER("read_ddl_log_file_entry");
  DBUG_ASSERT(io_size >= size);

  if (my_pread(file_id, file_entry_buf, size, io_size * entry_no,
               MYF(MY_WME)) != size)
    error= TRUE;
  DBUG_RETURN(error);
}


/**
  Write one entry to ddl log file.

  @param  file_entry_buf  Buffer to write
  @param  entry_no        Entry number to write
  @param  size            Number of bytes of the entry to write

  @return Operation status
    @retval true   Error
    @retval false  Success
*/

static bool write_ddl_log_file_entry(uchar *file_entry_buf,
                                     uint entry_no,
                                     uint size)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  uint io_size= global_ddl_log.io_size;
  DBUG_ENTER("write_ddl_log_file_entry");
  DBUG_ASSERT(io_size >= size);

  if (my_pwrite(file_id, file_entry_buf, size,
                io_size * entry_no, MYF(MY_WME)) != size)
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
  uchar file_entry_buf[DDL_LOG_HEADER_SIZE];
  DBUG_ENTER("write_ddl_log_header");
  DBUG_ASSERT((DDL_LOG_NAME_POS + 3 * global_ddl_log.name_len)
                <= global_ddl_log.io_size);

  int4store(&file_entry_buf[DDL_LOG_NUM_ENTRY_POS],
            global_ddl_log.num_entries);
  const_var= global_ddl_log.name_len;
  int4store(&file_entry_buf[DDL_LOG_NAME_LEN_POS],
            (ulong) const_var);
  const_var= global_ddl_log.io_size;
  int4store(&file_entry_buf[DDL_LOG_IO_SIZE_POS],
            (ulong) const_var);
  if (write_ddl_log_file_entry(file_entry_buf, 0UL, DDL_LOG_HEADER_SIZE))
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
  char file_entry_buf[DDL_LOG_HEADER_SIZE];
  char file_name[FN_REFLEN];
  uint entry_no;
  bool successful_open= FALSE;
  DBUG_ENTER("read_ddl_log_header");
  DBUG_ASSERT(global_ddl_log.io_size <= IO_SIZE);

  create_ddl_log_file_name(file_name);
  if ((global_ddl_log.file_id= my_open(file_name,
                                        O_RDWR | O_BINARY, MYF(0))) >= 0)
  {
    if (read_ddl_log_file_entry((uchar *) file_entry_buf, 0UL,
                                DDL_LOG_HEADER_SIZE))
    {
      /* Write message into error log */
      sql_print_error("Failed to read ddl log file in recovery");
    }
    else
      successful_open= TRUE;
  }
  if (successful_open)
  {
    entry_no= uint4korr(&file_entry_buf[DDL_LOG_NUM_ENTRY_POS]);
    global_ddl_log.name_len= uint4korr(&file_entry_buf[DDL_LOG_NAME_LEN_POS]);
    global_ddl_log.io_size= uint4korr(&file_entry_buf[DDL_LOG_IO_SIZE_POS]);
  }
  else
  {
    entry_no= 0;
  }
  global_ddl_log.first_free= NULL;
  global_ddl_log.first_used= NULL;
  global_ddl_log.num_entries= 0;
  VOID(pthread_mutex_init(&LOCK_gdl, MY_MUTEX_INIT_FAST));
  global_ddl_log.do_release= true;
  DBUG_RETURN(entry_no);
}


/**
  Set ddl log entry struct from buffer
  @param read_entry      Entry number
  @param file_entry_buf  Buffer to use
  @param ddl_log_entry   Entry to be set

  @note Pointers in ddl_log_entry will point into file_entry_buf!
*/

static void set_ddl_log_entry_from_buf(uint read_entry,
                                       uchar *file_entry_buf,
                                       DDL_LOG_ENTRY *ddl_log_entry)
{
  uint inx;
  uchar single_char;
  DBUG_ENTER("set_ddl_log_entry_from_buf");
  ddl_log_entry->entry_pos= read_entry;
  single_char= file_entry_buf[DDL_LOG_ENTRY_TYPE_POS];
  ddl_log_entry->entry_type= (enum ddl_log_entry_code)single_char;
  single_char= file_entry_buf[DDL_LOG_ACTION_TYPE_POS];
  ddl_log_entry->action_type= (enum ddl_log_action_code)single_char;
  ddl_log_entry->phase= file_entry_buf[DDL_LOG_PHASE_POS];
  ddl_log_entry->next_entry= uint4korr(&file_entry_buf[DDL_LOG_NEXT_ENTRY_POS]);
  ddl_log_entry->name= (char*) &file_entry_buf[DDL_LOG_NAME_POS];
  inx= DDL_LOG_NAME_POS + global_ddl_log.name_len;
  ddl_log_entry->from_name= (char*) &file_entry_buf[inx];
  inx+= global_ddl_log.name_len;
  ddl_log_entry->handler_name= (char*) &file_entry_buf[inx];
  DBUG_VOID_RETURN;
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
  char file_name[FN_REFLEN];
  DBUG_ENTER("init_ddl_log");

  if (global_ddl_log.inited)
    goto end;

  global_ddl_log.io_size= IO_SIZE;
  global_ddl_log.name_len= FN_LEN;
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
#ifdef WITH_PARTITION_STORAGE_ENGINE
  char *par_ext= (char*)".par";
#endif
  handlerton *hton;
  DBUG_ENTER("execute_ddl_log_action");

  if (ddl_log_entry->entry_type == DDL_IGNORE_LOG_ENTRY_CODE)
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("ddl_log",
             ("execute type %c next %u name '%s' from_name '%s' handler '%s'",
             ddl_log_entry->action_type,
             ddl_log_entry->next_entry,
             ddl_log_entry->name,
             ddl_log_entry->from_name,
             ddl_log_entry->handler_name));
  handler_name.str= (char*)ddl_log_entry->handler_name;
  handler_name.length= strlen(ddl_log_entry->handler_name);
  init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0); 
  if (!strcmp(ddl_log_entry->handler_name, reg_ext))
    frm_action= TRUE;
  else
  {
    plugin_ref plugin= ha_resolve_by_name(thd, &handler_name);
    if (!plugin)
    {
      my_error(ER_ILLEGAL_HA, MYF(0), ddl_log_entry->handler_name);
      goto error;
    }
    hton= plugin_data(plugin, handlerton*);
    file= get_new_handler((TABLE_SHARE*)0, &mem_root, hton);
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
          if ((error= file->ha_delete_table(ddl_log_entry->name)))
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
        if (file->ha_rename_table(ddl_log_entry->from_name,
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

  safe_mutex_assert_owner(&LOCK_gdl);
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
  char file_entry_buf[IO_SIZE];
  DBUG_ENTER("write_ddl_log_entry");

  if (init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]=
                                    (char)DDL_LOG_ENTRY_CODE;
  file_entry_buf[DDL_LOG_ACTION_TYPE_POS]=
                                    (char)ddl_log_entry->action_type;
  file_entry_buf[DDL_LOG_PHASE_POS]= 0;
  int4store(&file_entry_buf[DDL_LOG_NEXT_ENTRY_POS],
            ddl_log_entry->next_entry);
  DBUG_ASSERT(strlen(ddl_log_entry->name) < global_ddl_log.name_len);
  strmake(&file_entry_buf[DDL_LOG_NAME_POS], ddl_log_entry->name,
          global_ddl_log.name_len - 1);
  if (ddl_log_entry->action_type == DDL_LOG_RENAME_ACTION ||
      ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION)
  {
    DBUG_ASSERT(strlen(ddl_log_entry->from_name) < global_ddl_log.name_len);
    strmake(&file_entry_buf[DDL_LOG_NAME_POS + global_ddl_log.name_len],
            ddl_log_entry->from_name, global_ddl_log.name_len - 1);
  }
  else
    file_entry_buf[DDL_LOG_NAME_POS + global_ddl_log.name_len]= 0;
  DBUG_ASSERT(strlen(ddl_log_entry->handler_name) < global_ddl_log.name_len);
  strmake(&file_entry_buf[DDL_LOG_NAME_POS + (2*global_ddl_log.name_len)],
          ddl_log_entry->handler_name, global_ddl_log.name_len - 1);
  if (get_free_ddl_log_entry(active_entry, &write_header))
  {
    DBUG_RETURN(TRUE);
  }
  error= FALSE;
  DBUG_PRINT("ddl_log",
             ("write type %c next %u name '%s' from_name '%s' handler '%s'",
             (char) file_entry_buf[DDL_LOG_ACTION_TYPE_POS],
             ddl_log_entry->next_entry,
             (char*) &file_entry_buf[DDL_LOG_NAME_POS],
             (char*) &file_entry_buf[DDL_LOG_NAME_POS +
                                     global_ddl_log.name_len],
             (char*) &file_entry_buf[DDL_LOG_NAME_POS +
                                     (2*global_ddl_log.name_len)]));
  if (write_ddl_log_file_entry((uchar*) file_entry_buf,
                               (*active_entry)->entry_pos, IO_SIZE))
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
  char file_entry_buf[IO_SIZE];
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
  file_entry_buf[DDL_LOG_NAME_POS + global_ddl_log.name_len]= 0;
  file_entry_buf[DDL_LOG_NAME_POS + 2*global_ddl_log.name_len]= 0;
  if (!(*active_entry))
  {
    if (get_free_ddl_log_entry(active_entry, &write_header))
    {
      DBUG_RETURN(TRUE);
    }
  }
  if (write_ddl_log_file_entry((uchar*) file_entry_buf,
                               (*active_entry)->entry_pos,
                               IO_SIZE))
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
  uchar file_entry_buf[DDL_LOG_NAME_POS];
  DBUG_ENTER("deactivate_ddl_log_entry");


  /*
    Only need to read and write the first bytes of the entry, where
    ENTRY_TYPE, ACTION_TYPE and PHASE reside. Using DDL_LOG_NAME_POS
    to include all info except for the names.
  */
  if (!read_ddl_log_file_entry(file_entry_buf, entry_no, DDL_LOG_NAME_POS))
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
      if (write_ddl_log_file_entry(file_entry_buf, entry_no, DDL_LOG_NAME_POS))
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
  safe_mutex_assert_owner(&LOCK_gdl);

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
  uchar file_entry_buf[IO_SIZE];
  DBUG_ENTER("execute_ddl_log_entry");

  pthread_mutex_lock(&LOCK_gdl);
  do
  {
    if (read_ddl_log_file_entry(file_entry_buf, read_entry, IO_SIZE))
    {
      /* Print the error to the log and continue with next log entry */
      sql_print_error("Failed to read entry = %u from ddl log",
                      read_entry);
      break;
    }
    set_ddl_log_entry_from_buf(read_entry, file_entry_buf, &ddl_log_entry);
    DBUG_ASSERT(ddl_log_entry.entry_type == DDL_LOG_ENTRY_CODE ||
                ddl_log_entry.entry_type == DDL_IGNORE_LOG_ENTRY_CODE);

    if (execute_ddl_log_action(thd, &ddl_log_entry))
    {
      /* Print the error to the log and continue with next log entry */
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
  Close the ddl log
  SYNOPSIS
    close_ddl_log()
  RETURN VALUES
    NONE
*/

static void close_ddl_log()
{
  DBUG_ENTER("close_ddl_log");
  if (global_ddl_log.file_id >= 0)
  {
    VOID(my_close(global_ddl_log.file_id, MYF(MY_WME)));
    global_ddl_log.file_id= (File) -1;
  }
  DBUG_VOID_RETURN;
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
  uchar *file_entry_buf;
  uint io_size;
  char file_name[FN_REFLEN];
  DBUG_ENTER("execute_ddl_log_recovery");

  /*
    Initialise global_ddl_log struct
  */
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
  io_size= global_ddl_log.io_size;
  file_entry_buf= (uchar*) my_malloc(io_size, MYF(0));
  if (!file_entry_buf)
  {
    sql_print_error("Failed to allocate buffer for recover ddl log");
    DBUG_VOID_RETURN;
  }
  for (i= 1; i < num_entries + 1; i++)
  {
    if (read_ddl_log_file_entry(file_entry_buf, i, io_size))
    {
      sql_print_error("Failed to read entry no = %u from ddl log",
                       i);
      continue;
    }

    set_ddl_log_entry_from_buf(i, file_entry_buf, &ddl_log_entry);
    if (ddl_log_entry.entry_type == DDL_LOG_EXECUTE_CODE)
    {
      if (execute_ddl_log_entry(thd, ddl_log_entry.next_entry))
      {
        /* Real unpleasant scenario but we continue anyways.  */
        continue;
      }
    }
  }
  close_ddl_log();
  create_ddl_log_file_name(file_name);
  VOID(my_delete(file_name, MYF(0)));
  global_ddl_log.recovery_phase= FALSE;
  delete thd;
  my_free(file_entry_buf, MYF(0));
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
  DDL_LOG_MEMORY_ENTRY *free_list;
  DDL_LOG_MEMORY_ENTRY *used_list;
  DBUG_ENTER("release_ddl_log");

  if (!global_ddl_log.do_release)
    DBUG_VOID_RETURN;

  pthread_mutex_lock(&LOCK_gdl);
  free_list= global_ddl_log.first_free;
  used_list= global_ddl_log.first_used;
  while (used_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= used_list->next_log_entry;
    my_free(used_list, MYF(0));
    used_list= tmp;
  }
  while (free_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= free_list->next_log_entry;
    my_free(free_list, MYF(0));
    free_list= tmp;
  }
  close_ddl_log();
  global_ddl_log.inited= 0;
  pthread_mutex_unlock(&LOCK_gdl);
  VOID(pthread_mutex_destroy(&LOCK_gdl));
  global_ddl_log.do_release= false;
  DBUG_VOID_RETURN;
}


/*
---------------------------------------------------------------------------

  END MODULE DDL log
  --------------------

---------------------------------------------------------------------------
*/


/**
   @brief construct a temporary shadow file name.

   @details Make a shadow file name used by ALTER TABLE to construct the
   modified table (with keeping the original). The modified table is then
   moved back as original table. The name must start with the temp file
   prefix so it gets filtered out by table files listing routines. 
    
   @param[out] buff      buffer to receive the constructed name
   @param      bufflen   size of buff
   @param      lpt       alter table data structure

   @retval     path length
*/

uint build_table_shadow_filename(char *buff, size_t bufflen, 
                                 ALTER_PARTITION_PARAM_TYPE *lpt)
{
  char tmp_name[FN_REFLEN];
  my_snprintf (tmp_name, sizeof (tmp_name), "%s-%s", tmp_file_prefix,
               lpt->table_name);
  return build_table_filename(buff, bufflen, lpt->db, tmp_name, "", FN_IS_TMP);
}


/*
  SYNOPSIS
    mysql_write_frm()
    lpt                    Struct carrying many parameters needed for this
                           method
    flags                  Flags as defined below
      WFRM_INITIAL_WRITE        If set we need to prepare table before
                                creating the frm file
      WFRM_INSTALL_SHADOW       If set we should install the new frm
      WFRM_KEEP_SHARE           If set we know that the share is to be
                                retained and thus we should ensure share
                                object is correct, if not set we don't
                                set the new partition syntax string since
                                we know the share object is destroyed.
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
#ifdef WITH_PARTITION_STORAGE_ENGINE
  char *part_syntax_buf;
  uint syntax_len;
#endif
  DBUG_ENTER("mysql_write_frm");

  /*
    Build shadow frm file name
  */
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1, lpt);
  strxmov(shadow_frm_name, shadow_path, reg_ext, NullS);
  if (flags & WFRM_WRITE_SHADOW)
  {
    if (mysql_prepare_create_table(lpt->thd, lpt->create_info,
                                   lpt->alter_info,
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
                          lpt->alter_info->create_list, lpt->key_count,
                          lpt->key_info_buffer, lpt->table->file)) ||
        lpt->table->file->ha_create_handler_files(shadow_path, NULL,
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
    uchar *data;
    size_t length;
    if (readfrm(shadow_path, &data, &length) ||
        packfrm(data, length, &lpt->pack_frm_data, &lpt->pack_frm_len))
    {
      my_free(data, MYF(MY_ALLOW_ZERO_PTR));
      my_free(lpt->pack_frm_data, MYF(MY_ALLOW_ZERO_PTR));
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
    build_table_filename(path, sizeof(path) - 1, lpt->db,
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
        lpt->table->file->ha_create_handler_files(path, shadow_path,
                                                  CHF_DELETE_FLAG, NULL) ||
        deactivate_ddl_log_entry(part_info->frm_log_entry->entry_pos) ||
        (sync_ddl_log(), FALSE) ||
#endif
#ifdef WITH_PARTITION_STORAGE_ENGINE
        my_rename(shadow_frm_name, frm_name, MYF(MY_WME)) ||
        lpt->table->file->ha_create_handler_files(path, shadow_path,
                                                  CHF_RENAME_FLAG, NULL))
#else
        my_rename(shadow_frm_name, frm_name, MYF(MY_WME)))
#endif
    {
      error= 1;
      goto err;
    }
#ifdef WITH_PARTITION_STORAGE_ENGINE
    if (part_info && (flags & WFRM_KEEP_SHARE))
    {
      TABLE_SHARE *share= lpt->table->s;
      char *tmp_part_syntax_str;
      if (!(part_syntax_buf= generate_partition_syntax(part_info,
                                                       &syntax_len,
                                                       TRUE, TRUE)))
      {
        error= 1;
        goto err;
      }
      if (share->partition_info_buffer_size < syntax_len + 1)
      {
        share->partition_info_buffer_size= syntax_len+1;
        if (!(tmp_part_syntax_str= (char*) strmake_root(&share->mem_root,
                                                        part_syntax_buf,
                                                        syntax_len)))
        {
          error= 1;
          goto err;
        }
        share->partition_info= tmp_part_syntax_str;
      }
      else
        memcpy((char*) share->partition_info, part_syntax_buf, syntax_len + 1);
      share->partition_info_len= part_info->part_info_len= syntax_len;
      part_info->part_info_string= part_syntax_buf;
    }
#endif

err:
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

int write_bin_log(THD *thd, bool clear_error,
                  char const *query, ulong query_length)
{
  int error= 0;
  if (mysql_bin_log.is_open())
  {
    int errcode= 0;
    if (clear_error)
      thd->clear_error();
    else
      errcode= query_error_code(thd, TRUE);
    error= thd->binlog_query(THD::STMT_QUERY_TYPE,
                             query, query_length, FALSE, FALSE, errcode);
  }
  return error;
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
  Drop_table_error_handler err_handler(thd->get_internal_handler());
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
  thd->push_internal_handler(&err_handler);
  error= mysql_rm_table_part2(thd, tables, if_exists, drop_temporary, 0, 0);
  thd->pop_internal_handler();


  if (need_start_waiters)
    start_waiting_global_read_lock(thd);

  if (error)
    DBUG_RETURN(TRUE);
  my_ok(thd);
  DBUG_RETURN(FALSE);
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
  char path[FN_REFLEN + 1], *alias;
  uint path_length;
  String wrong_tables;
  int error= 0;
  int non_temp_tables_count= 0;
  bool some_tables_deleted=0, tmp_table_deleted=0, foreign_key_error=0;
  String built_query;
  String built_tmp_query;
  DBUG_ENTER("mysql_rm_table_part2");

  LINT_INIT(alias);
  LINT_INIT(path_length);

  if (thd->current_stmt_binlog_row_based && !dont_log_query)
  {
    built_query.set_charset(system_charset_info);
    if (if_exists)
      built_query.append("DROP TABLE IF EXISTS ");
    else
      built_query.append("DROP TABLE ");
  }

  mysql_ha_rm_tables(thd, tables, FALSE);

  pthread_mutex_lock(&LOCK_open);

  /* Disable drop of enabled log tables, must be done before name locking */
  for (table= tables; table; table= table->next_local)
  {
    if (check_if_log_table(table->db_length, table->db,
                           table->table_name_length, table->table_name, 1))
    {
      my_error(ER_BAD_LOG_STATEMENT, MYF(0), "DROP");
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(1);
    }
  }

  if (!drop_temporary && lock_table_names_exclusively(thd, tables))
  {
    pthread_mutex_unlock(&LOCK_open);
    DBUG_RETURN(1);
  }

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    int db_len= table->db_length;
    handlerton *table_type;
    enum legacy_db_type frm_db_type= DB_TYPE_UNKNOWN;

    DBUG_PRINT("table", ("table_l: '%s'.'%s'  table: 0x%lx  s: 0x%lx",
                         table->db, table->table_name, (long) table->table,
                         table->table ? (long) table->table->s : (long) -1));

    error= drop_temporary_table(thd, table);

    switch (error) {
    case  0:
      // removed temporary table
      tmp_table_deleted= 1;
      if (thd->variables.binlog_format == BINLOG_FORMAT_MIXED &&
          thd->current_stmt_binlog_row_based)
      {
        if (built_tmp_query.is_empty()) 
        {
          built_tmp_query.set_charset(system_charset_info);
          built_tmp_query.append("DROP TEMPORARY TABLE IF EXISTS ");
        }

        if (thd->db == NULL || strcmp(db,thd->db) != 0)
        {
          append_identifier(thd, &built_tmp_query, db, db_len);
          built_tmp_query.append(".");
        }
        append_identifier(thd, &built_tmp_query, table->table_name,
                          strlen(table->table_name));
        built_tmp_query.append(",");
      }

      continue;
    case -1:
      DBUG_ASSERT(thd->in_sub_stmt);
      error= 1;
      goto err_with_placeholders;
    default:
      // temporary table not found
      error= 0;
    }

    /*
      If row-based replication is used and the table is not a
      temporary table, we add the table name to the drop statement
      being built.  The string always end in a comma and the comma
      will be chopped off before being written to the binary log.
      */
    if (!drop_temporary && thd->current_stmt_binlog_row_based && !dont_log_query)
    {
      non_temp_tables_count++;
      /*
        Don't write the database name if it is the current one (or if
        thd->db is NULL).
      */
      if (thd->db == NULL || strcmp(db,thd->db) != 0)
      {
        append_identifier(thd, &built_query, db, db_len);
        built_query.append(".");
      }
      append_identifier(thd, &built_query, table->table_name,
                        strlen(table->table_name));
      built_query.append(",");
    }

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
        error= -1;
        goto err_with_placeholders;
      }
      alias= (lower_case_table_names == 2) ? table->alias : table->table_name;
      /* remove .frm file and engine files */
      path_length= build_table_filename(path, sizeof(path) - 1, db, alias,
                                        reg_ext,
                                        table->internal_tmp_table ?
                                        FN_IS_TMP : 0);
    }
    DEBUG_SYNC(thd, "rm_table_part2_before_delete_table");
    if (drop_temporary ||
        ((access(path, F_OK) &&
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
      /*
        Cannot use the db_type from the table, since that might have changed
        while waiting for the exclusive name lock. We are under LOCK_open,
        so reading from the frm-file is safe.
      */
      if (frm_db_type == DB_TYPE_UNKNOWN)
      {
        mysql_frm_type(thd, path, &frm_db_type);
        DBUG_PRINT("info", ("frm_db_type %d from %s", frm_db_type, path));
      }
      table_type= ha_resolve_by_legacy_type(thd, frm_db_type);
      // Remove extension for delete
      *(end= path + path_length - reg_ext_length)= '\0';
      DBUG_PRINT("info", ("deleting table of type %d",
                          (table_type ? table_type->db_type : 0)));
      error= ha_delete_table(thd, table_type, path, db, table->table_name,
                             !dont_log_query);

      /* No error if non existent table and 'IF EXIST' clause or view */
      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) && 
	  (if_exists || table_type == NULL))
      {
	error= 0;
        thd->clear_error();
      }
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
    DBUG_PRINT("table", ("table: 0x%lx  s: 0x%lx", (long) table->table,
                         table->table ? (long) table->table->s : (long) -1));
  }
  /*
    It's safe to unlock LOCK_open: we have an exclusive lock
    on the table name.
  */
  pthread_mutex_unlock(&LOCK_open);
  DEBUG_SYNC(thd, "rm_table_part2_before_binlog");
  thd->thread_specific_used|= tmp_table_deleted;
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
          (non_temp_tables_count > 0 && !tmp_table_deleted))
      {
        /*
          In this case, we are either using statement-based
          replication or using row-based replication but have only
          deleted one or more non-temporary tables (and no temporary
          tables).  In this case, we can write the original query into
          the binary log.
         */
        error |= write_bin_log(thd, !error, thd->query(), thd->query_length());
      }
      else if (thd->current_stmt_binlog_row_based &&
               tmp_table_deleted)
      {
        if (non_temp_tables_count > 0)
        {
          /*
            In this case we have deleted both temporary and
            non-temporary tables, so:
            - since we have deleted a non-temporary table we have to
              binlog the statement, but
            - since we have deleted a temporary table we cannot binlog
              the statement (since the table may have not been created on the
              slave - check "if" branch below, this might cause the slave to 
              stop).

            Instead, we write a built statement, only containing the
            non-temporary tables, to the binary log
          */
          built_query.chop();                  // Chop of the last comma
          built_query.append(" /* generated by server */");
          error|= write_bin_log(thd, !error, built_query.ptr(), built_query.length());
        }

        /*
          One needs to always log any temporary table drop, if:
            1. thread logging format is mixed mode; AND
            2. current statement logging format is set to row.
        */
        if (thd->variables.binlog_format == BINLOG_FORMAT_MIXED)
        {
          /*
            In this case we have deleted some temporary tables but we are using
            row based logging for the statement. However, thread uses mixed mode
            format, thence we need to log the dropping as we cannot tell for
            sure whether the create was logged as statement previously or not, ie,
            before switching to row mode.
          */
          built_tmp_query.chop();                  // Chop of the last comma
          built_tmp_query.append(" /* generated by server */");
          error|= write_bin_log(thd, !error, built_tmp_query.ptr(), built_tmp_query.length());
        }
      }

      /*
        The remaining cases are:
        - no tables were deleted and
        - only temporary tables were deleted and row-based
          replication is used.
        In both these cases, nothing should be written to the binary
        log.
      */
    }
  }
  pthread_mutex_lock(&LOCK_open);
err_with_placeholders:
  unlock_table_names(thd, tables, (TABLE_LIST*) 0);
  pthread_mutex_unlock(&LOCK_open);
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
  char path[FN_REFLEN + 1];
  bool error= 0;
  DBUG_ENTER("quick_rm_table");

  uint path_length= build_table_filename(path, sizeof(path) - 1,
                                         db, table_name, reg_ext, flags);
  if (my_delete(path,MYF(0)))
    error= 1; /* purecov: inspected */
  path[path_length - reg_ext_length]= '\0'; // Remove reg_ext
  if (!(flags & FRM_ONLY))
    error|= ha_delete_table(current_thd, base, path, db, table_name, 0);
  DBUG_RETURN(error);
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keys where all column are NOT NULL
  - UNIQUE keys that don't contain partial segments
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KEY *a, KEY *b)
{
  ulong a_flags= a->flags, b_flags= b->flags;
  
  if (a_flags & HA_NOSAME)
  {
    if (!(b_flags & HA_NOSAME))
      return -1;
    if ((a_flags ^ b_flags) & (HA_NULL_PART_KEY | HA_END_SPACE_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a_flags & (HA_NULL_PART_KEY | HA_END_SPACE_KEY)) ? 1 : -1;
    }
    if (a->name == primary_key_name)
      return -1;
    if (b->name == primary_key_name)
      return 1;
    /* Sort keys don't containing partial segments before others */
    if ((a_flags ^ b_flags) & HA_KEY_HAS_PART_KEY_SEG)
      return (a_flags & HA_KEY_HAS_PART_KEY_SEG) ? 1 : -1;
  }
  else if (b_flags & HA_NOSAME)
    return 1;					// Prefer b

  if ((a_flags ^ b_flags) & HA_FULLTEXT)
  {
    return (a_flags & HA_FULLTEXT) ? 1 : -1;
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
    dup_val_count  returns count of duplicate elements

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    0             ok
    1             Error
*/

bool check_duplicates_in_interval(const char *set_or_name,
                                  const char *name, TYPELIB *typelib,
                                  CHARSET_INFO *cs, unsigned int *dup_val_count)
{
  TYPELIB tmp= *typelib;
  const char **cur_value= typelib->type_names;
  unsigned int *cur_length= typelib->type_lengths;
  *dup_val_count= 0;  
  
  for ( ; tmp.count > 1; cur_value++, cur_length++)
  {
    tmp.type_names++;
    tmp.type_lengths++;
    tmp.count--;
    if (find_type2(&tmp, (const char*)*cur_value, *cur_length, cs))
    {
      if ((current_thd->variables.sql_mode &
         (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES)))
      {
        my_error(ER_DUPLICATED_VALUE_IN_TYPE, MYF(0),
                 name,*cur_value,set_or_name);
        return 1;
      }
      push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DUPLICATED_VALUE_IN_TYPE,
			  ER(ER_DUPLICATED_VALUE_IN_TYPE),
			  name,*cur_value,set_or_name);
      (*dup_val_count)++;
    }
  }
  return 0;
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
    size_t length= cs->cset->numchars(cs, *pos, *pos + *len);
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
    This function prepares a Create_field instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/

int prepare_create_field(Create_field *sql_field, 
			 uint *blob_columns, 
			 int *timestamps, int *timestamps_with_niladic,
			 longlong table_flags)
{
  unsigned int dup_val_count;
  DBUG_ENTER("prepare_field");

  /*
    This code came from mysql_prepare_create_table.
    Indent preserved to make patching easier
  */
  DBUG_ASSERT(sql_field->charset);

  switch (sql_field->sql_type) {
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
    sql_field->pack_flag=FIELDFLAG_BLOB |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
  case MYSQL_TYPE_GEOMETRY:
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
                        MYF(0), sql_field->field_name,
                        static_cast<ulong>(MAX_FIELD_CHARLENGTH));
        DBUG_RETURN(1);
      }
    }
#endif
    /* fall through */
  case MYSQL_TYPE_STRING:
    sql_field->pack_flag=0;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    break;
  case MYSQL_TYPE_ENUM:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_INTERVAL;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::INTERVAL_FIELD;
    if (check_duplicates_in_interval("ENUM",sql_field->field_name,
                                     sql_field->interval,
                                     sql_field->charset, &dup_val_count))
      DBUG_RETURN(1);
    break;
  case MYSQL_TYPE_SET:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_BITFIELD;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::BIT_FIELD;
    if (check_duplicates_in_interval("SET",sql_field->field_name,
                                     sql_field->interval,
                                     sql_field->charset, &dup_val_count))
      DBUG_RETURN(1);
    /* Check that count of unique members is not more then 64 */
    if (sql_field->interval->count -  dup_val_count > sizeof(longlong)*8)
    {
       my_error(ER_TOO_BIG_SET, MYF(0), sql_field->field_name);
       DBUG_RETURN(1);
    }
    break;
  case MYSQL_TYPE_DATE:			// Rest of string types
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_NULL:
    sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
    break;
  case MYSQL_TYPE_BIT:
    /* 
      We have sql_field->pack_flag already set here, see
      mysql_prepare_create_table().
    */
    break;
  case MYSQL_TYPE_NEWDECIMAL:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & ZEROFILL_FLAG ?
                           FIELDFLAG_ZEROFILL : 0) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  case MYSQL_TYPE_TIMESTAMP:
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
    mysql_prepare_create_table()
      thd                       Thread object.
      create_info               Create information (like MAX_ROWS).
      alter_info                List of columns and indexes to create
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
    FALSE    OK
    TRUE     error
*/

static int
mysql_prepare_create_table(THD *thd, HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                           uint *db_options,
                           handler *file, KEY **key_info_buffer,
                           uint *key_count, int select_field_count)
{
  const char	*key_name;
  Create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<Create_field> it(alter_info->create_list);
  List_iterator<Create_field> it2(alter_info->create_list);
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("mysql_prepare_create_table");

  select_field_pos= alter_info->create_list.elements - select_field_count;
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
      char tmp[65];
      strmake(strmake(tmp, save_cs->csname, sizeof(tmp)-4),
              STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      DBUG_RETURN(TRUE);
    }

    /*
      Convert the default value from client character
      set into the column character set if necessary.
    */
    if (sql_field->def && 
        save_cs != sql_field->def->collation.collation &&
        (sql_field->sql_type == MYSQL_TYPE_VAR_STRING ||
         sql_field->sql_type == MYSQL_TYPE_STRING ||
         sql_field->sql_type == MYSQL_TYPE_SET ||
         sql_field->sql_type == MYSQL_TYPE_ENUM))
    {
      /*
        Starting from 5.1 we work here with a copy of Create_field
        created by the caller, not with the instance that was
        originally created during parsing. It's OK to create
        a temporary item and initialize with it a member of the
        copy -- this item will be thrown away along with the copy
        at the end of execution, and thus not introduce a dangling
        pointer in the parsed tree of a prepared statement or a
        stored procedure statement.
      */
      sql_field->def= sql_field->def->safe_charset_converter(save_cs);

      if (sql_field->def == NULL)
      {
        /* Could not convert */
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(TRUE);
      }
    }

    if (sql_field->sql_type == MYSQL_TYPE_SET ||
        sql_field->sql_type == MYSQL_TYPE_ENUM)
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
          Create the typelib in runtime memory - we will free the
          occupied memory at the same time when we free this
          sql_field -- at the end of execution.
        */
        interval= sql_field->interval= typelib(thd->mem_root,
                                               sql_field->interval_list);
        List_iterator<String> int_it(sql_field->interval_list);
        String conv, *tmp;
        char comma_buf[2];
        int comma_length= cs->cset->wc_mb(cs, ',', (uchar*) comma_buf,
                                          (uchar*) comma_buf + 
                                          sizeof(comma_buf));
        DBUG_ASSERT(comma_length > 0);
        for (uint i= 0; (tmp= int_it++); i++)
        {
          size_t lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy))
          {
            uint cnv_errs;
            conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);
            interval->type_names[i]= strmake_root(thd->mem_root, conv.ptr(),
                                                  conv.length());
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i],
                                       interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((uchar *)interval->type_names[i])[lengthsp]= '\0';
          if (sql_field->sql_type == MYSQL_TYPE_SET)
          {
            if (cs->coll->instr(cs, interval->type_names[i], 
                                interval->type_lengths[i], 
                                comma_buf, comma_length, NULL, 0))
            {
              my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", tmp->ptr());
              DBUG_RETURN(TRUE);
            }
          }
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      if (sql_field->sql_type == MYSQL_TYPE_SET)
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
              DBUG_RETURN(TRUE);
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
            DBUG_RETURN(TRUE);
          }
        }
        calculate_interval_lengths(cs, interval, &dummy, &field_length);
        sql_field->length= field_length + (interval->count - 1);
      }
      else  /* MYSQL_TYPE_ENUM */
      {
        uint32 field_length;
        DBUG_ASSERT(sql_field->sql_type == MYSQL_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(TRUE);
            }

            /* else, the defaults yield the correct length for NULLs. */
          } 
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (find_type2(interval, def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(TRUE);
            }
          }
        }
        calculate_interval_lengths(cs, interval, &field_length, &dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
    }

    if (sql_field->sql_type == MYSQL_TYPE_BIT)
    { 
      sql_field->pack_flag= FIELDFLAG_NUMBER;
      if (file->ha_table_flags() & HA_CAN_BIT_FIELD)
        total_uneven_bit_length+= sql_field->length & 7;
      else
        sql_field->pack_flag|= FIELDFLAG_TREAT_BIT_AS_CHAR;
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(thd, sql_field))
      DBUG_RETURN(TRUE);

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      DBUG_RETURN(TRUE);
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
	  DBUG_RETURN(TRUE);
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
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->create_length_to_internal_length();
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
	(sql_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED))
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
      DBUG_RETURN(TRUE);
    if (sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      create_info->varchar= TRUE;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    record_offset+= sql_field->pack_length;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment &&
      (file->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (blob_columns && (file->ha_table_flags() & HA_NO_BLOBS))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(alter_info->key_list);
  List_iterator<Key> key_iterator2(alter_info->key_list);
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
    LEX_STRING key_name_str;
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      Foreign_key *fk_key= (Foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name ?  fk_key->name : "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(TRUE);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(TRUE);
    }
    key_name_str.str= (char*) key->name;
    key_name_str.length= key->name ? strlen(key->name) : 0;
    if (check_string_char_length(&key_name_str, "", NAME_CHAR_LEN,
                                 system_charset_info, 1))
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name);
      DBUG_RETURN(TRUE);
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
      DBUG_RETURN(TRUE);
    }
  }
  tmp=file->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    DBUG_RETURN(TRUE);
  }

  (*key_info_buffer)= key_info= (KEY*) sql_calloc(sizeof(KEY) * (*key_count));
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!*key_info_buffer || ! key_part_info)
    DBUG_RETURN(TRUE);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint key_length=0;
    Key_part_spec *column;

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
	DBUG_RETURN(TRUE);
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
	DBUG_RETURN(TRUE);
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
        DBUG_RETURN(TRUE);
      }
      if (key_info->key_parts != 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "SPATIAL INDEX");
	DBUG_RETURN(TRUE);
      }
    }
    else if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
#ifdef HAVE_RTREE_KEYS
      if ((key_info->key_parts & 1) == 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "RTREE INDEX");
	DBUG_RETURN(TRUE);
      }
      /* TODO: To be deleted */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
      DBUG_RETURN(TRUE);
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               sym_group_rtree.name, sym_group_rtree.needed_define);
      DBUG_RETURN(TRUE);
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

    List_iterator<Key_part_spec> cols(key->columns), cols2(key->columns);
    CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint length;
      Key_part_spec *dup_column;

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
	DBUG_RETURN(TRUE);
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->field_name, dup_column->field_name))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name);
	  DBUG_RETURN(TRUE);
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

        if (key->type == Key::SPATIAL)
        {
          if (column->length)
          {
            my_error(ER_WRONG_SUB_KEY, MYF(0));
            DBUG_RETURN(TRUE);
          }

          if (!f_is_geom(sql_field->pack_flag))
          {
            my_error(ER_WRONG_ARGUMENTS, MYF(0), "SPATIAL INDEX");
            DBUG_RETURN(TRUE);
          }
        }

	if (f_is_blob(sql_field->pack_flag) ||
            (f_is_geom(sql_field->pack_flag) && key->type != Key::SPATIAL))
	{
	  if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name);
	    DBUG_RETURN(TRUE);
	  }
          if (f_is_geom(sql_field->pack_flag) && sql_field->geom_type ==
              Field::GEOM_POINT)
            column->length= 25;
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name);
	    DBUG_RETURN(TRUE);
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
              DBUG_RETURN(TRUE);
            }
            if (key->type == Key::SPATIAL)
            {
              my_message(ER_SPATIAL_CANT_HAVE_NULL,
                         ER(ER_SPATIAL_CANT_HAVE_NULL), MYF(0));
              DBUG_RETURN(TRUE);
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
              /* Align key length to multibyte char boundary */
              length-= length % sql_field->charset->mbmaxlen;
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      DBUG_RETURN(TRUE);
	    }
	  }
	}
	else if (!f_is_geom(sql_field->pack_flag) &&
		  (column->length > length ||
                   !Field::type_can_have_key_part (sql_field->sql_type) ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  DBUG_RETURN(TRUE);
	}
	else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name);
	  DBUG_RETURN(TRUE);
      }
      if (length > file->max_key_part_length() && key->type != Key::FULLTEXT)
      {
        length= file->max_key_part_length();
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
		      length);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
          /* Align key length to multibyte char boundary */
          length-= length % sql_field->charset->mbmaxlen;
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  DBUG_RETURN(TRUE);
	}
      }
      key_part_info->length=(uint16) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
          !((create_info->table_options & HA_OPTION_NO_PACK_KEYS)) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == MYSQL_TYPE_STRING ||
	    sql_field->sql_type == MYSQL_TYPE_VARCHAR ||
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if ((column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB)) ||
            sql_field->sql_type == MYSQL_TYPE_VARCHAR)
	  key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
	else
	  key_info->flags|= HA_PACK_KEY;
      }
      /* Check if the key segment is partial, set the key flag accordingly */
      if (length != sql_field->key_length)
        key_info->flags|= HA_KEY_HAS_PART_KEY_SEG;

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
	    DBUG_RETURN(TRUE);
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
	  DBUG_RETURN(TRUE);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(TRUE);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    if (key_length > max_key_length && key->type != Key::FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      DBUG_RETURN(TRUE);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (file->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  /* Sort keys in optimized order */
  my_qsort((uchar*) *key_info_buffer, *key_count, sizeof(KEY),
	   (qsort_cmp) sort_keys);
  create_info->null_bits= null_fields;

  /* Check fields. */
  it.rewind();
  while ((sql_field=it++))
  {
    Field::utype type= (Field::utype) MTYP_TYPENR(sql_field->unireg_check);

    if (thd->variables.sql_mode & MODE_NO_ZERO_DATE &&
        !sql_field->def &&
        sql_field->sql_type == MYSQL_TYPE_TIMESTAMP &&
        (sql_field->flags & NOT_NULL_FLAG) &&
        (type == Field::NONE || type == Field::TIMESTAMP_UN_FIELD))
    {
      /*
        An error should be reported if:
          - NO_ZERO_DATE SQL mode is active;
          - there is no explicit DEFAULT clause (default column value);
          - this is a TIMESTAMP column;
          - the column is not NULL;
          - this is not the DEFAULT CURRENT_TIMESTAMP column.

        In other words, an error should be reported if
          - NO_ZERO_DATE SQL mode is active;
          - the column definition is equivalent to
            'column_name TIMESTAMP DEFAULT 0'.
      */

      my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
      DBUG_RETURN(TRUE);
    }
  }

  DBUG_RETURN(FALSE);
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
  /*
    If the table character set was not given explicitly,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (!create_info->default_table_charset)
  {
    HA_CREATE_INFO db_info;

    load_db_opt_by_name(thd, db, &db_info);

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

static bool prepare_blob_field(THD *thd, Create_field *sql_field)
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
               static_cast<ulong>(MAX_FIELD_VARCHARLENGTH /
                                  sql_field->charset->mbmaxlen));
      DBUG_RETURN(1);
    }
    sql_field->sql_type= MYSQL_TYPE_BLOB;
    sql_field->flags|= BLOB_FLAG;
    my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_AUTO_CONVERT), sql_field->field_name,
            (sql_field->charset == &my_charset_bin) ? "VARBINARY" : "VARCHAR",
            (sql_field->charset == &my_charset_bin) ? "BLOB" : "TEXT");
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_AUTO_CONVERT,
                 warn_buff);
  }

  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == FIELD_TYPE_BLOB ||
        sql_field->sql_type == FIELD_TYPE_TINY_BLOB ||
        sql_field->sql_type == FIELD_TYPE_MEDIUM_BLOB)
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
  Preparation of Create_field for SP function return values.
  Based on code used in the inner loop of mysql_prepare_create_table()
  above.

  SYNOPSIS
    sp_prepare_create_field()
    thd			Thread object
    sql_field		Field to prepare

  DESCRIPTION
    Prepares the field structures for field creation.

*/

void sp_prepare_create_field(THD *thd, Create_field *sql_field)
{
  if (sql_field->sql_type == MYSQL_TYPE_SET ||
      sql_field->sql_type == MYSQL_TYPE_ENUM)
  {
    uint32 field_length, dummy;
    if (sql_field->sql_type == MYSQL_TYPE_SET)
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval, &dummy, 
                                 &field_length);
      sql_field->length= field_length + 
                         (sql_field->interval->count - 1);
    }
    else /* MYSQL_TYPE_ENUM */
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval,
                                 &field_length, &dummy);
      sql_field->length= field_length;
    }
    set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
  }

  if (sql_field->sql_type == MYSQL_TYPE_BIT)
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
  Write CREATE TABLE binlog

  SYNOPSIS
    write_create_table_bin_log()
    thd               Thread object
    create_info       Create information
    internal_tmp_table  Set to 1 if this is an internal temporary table

  DESCRIPTION
    This function only is called in mysql_create_table_no_lock and
    mysql_create_table

  RETURN VALUES
    NONE
 */
static inline int write_create_table_bin_log(THD *thd,
                                             const HA_CREATE_INFO *create_info,
                                             bool internal_tmp_table)
{
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
    return write_bin_log(thd, TRUE, thd->query(), thd->query_length());
  return 0;
}


/*
  Create a table

  SYNOPSIS
    mysql_create_table_no_lock()
    thd			Thread object
    db			Database
    table_name		Table name
    create_info	        Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create
    internal_tmp_table  Set to 1 if this is an internal temporary table
			(From ALTER TABLE)
    select_field_count

  DESCRIPTION
    If one creates a temporary table, this is automatically opened

    Note that this function assumes that caller already have taken
    name-lock on table being created or used some other way to ensure
    that concurrent operations won't intervene. mysql_create_table()
    is a wrapper that can be used for this.

    no_log is needed for the case of CREATE ... SELECT,
    as the logging will be done later in sql_insert.cc
    select_field_count is also used for CREATE ... SELECT,
    and must be zero for standard create of table.

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_table_no_lock(THD *thd,
                                const char *db, const char *table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                bool internal_tmp_table,
                                uint select_field_count)
{
  char		path[FN_REFLEN + 1];
  uint          path_length;
  const char	*alias;
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  handler	*file;
  bool		error= TRUE;
  DBUG_ENTER("mysql_create_table_no_lock");
  DBUG_PRINT("enter", ("db: '%s'  table: '%s'  tmp: %d",
                       db, table_name, internal_tmp_table));


  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
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
    List_iterator<Key> key_iterator(alter_info->key_list);
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
        my_error(ER_FOREIGN_KEY_ON_PARTITIONED, MYF(0));
        goto err;
      }
    }
    if ((part_engine_type == partition_hton) &&
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
    DBUG_PRINT("info", ("db_type = %s create_info->db_type = %s",
             ha_resolve_storage_engine_name(part_info->default_engine_type),
             ha_resolve_storage_engine_name(create_info->db_type)));
    if (part_info->check_partition_info(thd, &engine_type, file,
                                        create_info, TRUE))
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
        create_info->db_type == partition_hton)
    {
      /*
        The handler assigned to the table cannot handle partitioning.
        Assign the partition handler as the handler of the table.
      */
      DBUG_PRINT("info", ("db_type: %s",
                        ha_resolve_storage_engine_name(create_info->db_type)));
      delete file;
      create_info->db_type= partition_hton;
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

  if (mysql_prepare_create_table(thd, create_info, alter_info,
                                 internal_tmp_table,
                                 &db_options, file,
                                 &key_info_buffer, &key_count,
                                 select_field_count))
    goto err;

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    path_length= build_tmptable_filename(thd, path, sizeof(path));
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else  
  {
    path_length= build_table_filename(path, sizeof(path) - 1, db, alias, reg_ext,
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
      error= write_create_table_bin_log(thd, create_info, internal_tmp_table);
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
    /*
      We don't assert here, but check the result, because the table could be
      in the table definition cache and in the same time the .frm could be
      missing from the disk, in case of manual intervention which deletes
      the .frm file. The user has to use FLUSH TABLES; to clear the cache.
      Then she could create the table. This case is pretty obscure and
      therefore we don't introduce a new error message only for it.
    */
    if (get_cached_table_share(db, table_name))
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);
      goto unlock_and_end;
    }
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
    bool create_if_not_exists =
      create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS;
    int retcode = ha_table_exists_in_engine(thd, db, table_name);
    DBUG_PRINT("info", ("exists_in_engine: %u",retcode));
    switch (retcode)
    {
      case HA_ERR_NO_SUCH_TABLE:
        /* Normal case, no table exists. we can go and create it */
        break;
      case HA_ERR_TABLE_EXIST:
        DBUG_PRINT("info", ("Table existed in handler"));

        if (create_if_not_exists)
          goto warn;
        my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
        goto unlock_and_end;
        break;
      default:
        DBUG_PRINT("info", ("error: %u from storage engine", retcode));
        my_error(retcode, MYF(0),table_name);
        goto unlock_and_end;
    }
  }

  thd_proc_info(thd, "creating table");
  create_info->table_existed= 0;		// Mark that table is created

#ifdef HAVE_READLINK
  if (test_if_data_home_dir(create_info->data_file_name))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "DATA DIRECTORY");
    goto unlock_and_end;
  }
  if (test_if_data_home_dir(create_info->index_file_name))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "INDEX DIRECTORY");
    goto unlock_and_end;
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (check_partition_dirs(thd->lex->part_info))
  {
    goto unlock_and_end;
  }
#endif /* WITH_PARTITION_STORAGE_ENGINE */

  if (!my_use_symdir || (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE))
#endif /* HAVE_READLINK */
  {
    if (create_info->data_file_name)
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "DATA DIRECTORY");
    if (create_info->index_file_name)
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "INDEX DIRECTORY");
    create_info->data_file_name= create_info->index_file_name= 0;
  }
  create_info->table_options=db_options;

  path[path_length - reg_ext_length]= '\0'; // Remove .frm extension
  if (rea_create_table(thd, path, db, table_name,
                       create_info, alter_info->create_list,
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
    thd->thread_specific_used= TRUE;
  }

  error= write_create_table_bin_log(thd, create_info, internal_tmp_table);
unlock_and_end:
  VOID(pthread_mutex_unlock(&LOCK_open));

err:
  thd_proc_info(thd, "After create");
  delete file;
  DBUG_RETURN(error);

warn:
  error= FALSE;
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  create_info->table_existed= 1;		// Mark that table existed
  error= write_create_table_bin_log(thd, create_info, internal_tmp_table);
  goto unlock_and_end;
}


/*
  Database and name-locking aware wrapper for mysql_create_table_no_lock(),
*/

bool mysql_create_table(THD *thd, const char *db, const char *table_name,
                        HA_CREATE_INFO *create_info,
                        Alter_info *alter_info,
                        bool internal_tmp_table,
                        uint select_field_count)
{
  TABLE *name_lock= 0;
  bool result;
  DBUG_ENTER("mysql_create_table");

  /* Wait for any database locks */
  pthread_mutex_lock(&LOCK_lock_db);
  while (!thd->killed &&
         hash_search(&lock_db_cache,(uchar*) db, strlen(db)))
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

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (lock_table_name_if_not_cached(thd, db, table_name, &name_lock))
    {
      result= TRUE;
      goto unlock;
    }
    if (!name_lock)
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                            ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                            table_name);
        create_info->table_existed= 1;
        result= FALSE;
        write_create_table_bin_log(thd, create_info, internal_tmp_table);
      }
      else
      {
        my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
        result= TRUE;
      }
      goto unlock;
    }
  }

  result= mysql_create_table_no_lock(thd, db, table_name, create_info,
                                     alter_info,
                                     internal_tmp_table,
                                     select_field_count);

unlock:
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(thd, name_lock, FALSE);
    pthread_mutex_unlock(&LOCK_open);
  }
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
                                NO_FRM_RENAME  Don't rename the FRM file
                                but only the table in the storage engine.

  RETURN
    FALSE   OK
    TRUE    Error
*/

bool
mysql_rename_table(handlerton *base, const char *old_db,
                   const char *old_name, const char *new_db,
                   const char *new_name, uint flags)
{
  THD *thd= current_thd;
  char from[FN_REFLEN + 1], to[FN_REFLEN + 1],
    lc_from[FN_REFLEN + 1], lc_to[FN_REFLEN + 1];
  char *from_base= from, *to_base= to;
  char tmp_name[NAME_LEN+1];
  handler *file;
  int error=0;
  DBUG_ENTER("mysql_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_db, old_name, new_db, new_name));

  file= (base == NULL ? 0 :
         get_new_handler((TABLE_SHARE*) 0, thd->mem_root, base));

  build_table_filename(from, sizeof(from) - 1, old_db, old_name, "",
                       flags & FN_FROM_IS_TMP);
  build_table_filename(to, sizeof(to) - 1, new_db, new_name, "",
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
    build_table_filename(lc_from, sizeof(lc_from) - 1, old_db, tmp_name, "",
                         flags & FN_FROM_IS_TMP);
    from_base= lc_from;

    strmov(tmp_name, new_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_to, sizeof(lc_to) - 1, new_db, tmp_name, "",
                         flags & FN_TO_IS_TMP);
    to_base= lc_to;
  }

  if (!file || !(error=file->ha_rename_table(from_base, to_base)))
  {
    if (!(flags & NO_FRM_RENAME) && rename_file_ext(from,to,reg_ext))
    {
      error=my_errno;
      /* Restore old file name */
      if (file)
        file->ha_rename_table(to_base, from_base);
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
    function            HA_EXTRA_PREPARE_FOR_DROP if table is to be deleted
                        HA_EXTRA_FORCE_REOPEN if table is not be used
                        HA_EXTRA_PREPARE_FOR_RENAME if table is to be renamed
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void wait_while_table_is_used(THD *thd, TABLE *table,
                              enum ha_extra_function function)
{
  DBUG_ENTER("wait_while_table_is_used");
  DBUG_PRINT("enter", ("table: '%s'  share: 0x%lx  db_stat: %u  version: %lu",
                       table->s->table_name.str, (ulong) table->s,
                       table->db_stat, table->s->version));

  safe_mutex_assert_owner(&LOCK_open);

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

  wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
  /* Close lock if this is not got with LOCK TABLES */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  unlink_open_table(thd, table, TRUE);

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
    char src_path[FN_REFLEN], dst_path[FN_REFLEN + 1], uname[FN_REFLEN];
    char* table_name= table->table_name;
    char* db= table->db;

    VOID(tablename_to_filename(table->table_name, uname, sizeof(uname) - 1));

    if (fn_format_relative_to_data_home(src_path, uname, backup_dir, reg_ext))
      DBUG_RETURN(-1); // protect buffer overflow

    build_table_filename(dst_path, sizeof(dst_path) - 1,
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
  if (reopen_name_locked_table(thd, table, TRUE))
  {
    unlock_table_name(thd, table);
    pthread_mutex_unlock(&LOCK_open);
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
                                  "Failed to open partially restored table"));
  }
  /* A MERGE table must not come here. */
  DBUG_ASSERT(!table->table || !table->table->child_l);
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

  if (table->s->frm_version != FRM_VER_TRUE_VARCHAR)
  {
    error= send_check_errmsg(thd, table_list, "repair",
                             "Failed repairing incompatible .frm file");
    goto end;
  }

  /*
    Check if this is a table type that stores index and data separately,
    like ISAM or MyISAM. We assume fixed order of engine file name
    extentions array. First element of engine file name extentions array
    is meta/index file extention. Second element - data file extention. 
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
  if (reopen_name_locked_table(thd, table_list, TRUE))
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
  TABLE_LIST *table;
  SELECT_LEX *select= &thd->lex->select_lex;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  LEX *lex= thd->lex;
  int result_code;
  DBUG_ENTER("mysql_admin_table");

  if (end_active_trans(thd))
    DBUG_RETURN(1);
  field_list.push_back(item = new Item_empty_string("Table", NAME_CHAR_LEN*2));
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

  mysql_ha_rm_tables(thd, tables, FALSE);

  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    char* db = table->db;
    bool fatal_error=0;

    DBUG_PRINT("admin", ("table: '%s'.'%s'", table->db, table->table_name));
    DBUG_PRINT("admin", ("extra_open_options: %u", extra_open_options));
    strxmov(table_name, db, ".", table->table_name, NullS);
    thd->open_options|= extra_open_options;
    table->lock_type= lock_type;
    /* open only one table from local list of command */
    {
      TABLE_LIST *save_next_global, *save_next_local;
      save_next_global= table->next_global;
      table->next_global= 0;
      save_next_local= table->next_local;
      table->next_local= 0;
      select->table_list.first= table;
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
#ifdef WITH_PARTITION_STORAGE_ENGINE
      if (table->table)
      {
        /*
          Set up which partitions that should be processed
          if ALTER TABLE t ANALYZE/CHECK/OPTIMIZE/REPAIR PARTITION ..
        */
        Alter_info *alter_info= &lex->alter_info;

        if (alter_info->flags & ALTER_ADMIN_PARTITION)
        {
          if (!table->table->part_info)
          {
            my_error(ER_PARTITION_MGMT_ON_NONPARTITIONED, MYF(0));
            DBUG_RETURN(TRUE);
          }
          uint no_parts_found;
          uint no_parts_opt= alter_info->partition_names.elements;
          no_parts_found= set_part_state(alter_info, table->table->part_info,
                                         PART_CHANGED);
          if (no_parts_found != no_parts_opt &&
              (!(alter_info->flags & ALTER_ALL_PARTITION)))
          {
            char buff[FN_REFLEN + MYSQL_ERRMSG_SIZE];
            size_t length;
            DBUG_PRINT("admin", ("sending non existent partition error"));
            protocol->prepare_for_resend();
            protocol->store(table_name, system_charset_info);
            protocol->store(operator_name, system_charset_info);
            protocol->store(STRING_WITH_LEN("error"), system_charset_info);
            length= my_snprintf(buff, sizeof(buff),
                                ER(ER_DROP_PARTITION_NON_EXISTENT),
                                table_name);
            protocol->store(buff, length, system_charset_info);
            if(protocol->write())
              goto err;
            my_eof(thd);
            goto err;
          }
        }
      }
#endif
    }
    DBUG_PRINT("admin", ("table: 0x%lx", (long) table->table));

    if (prepare_func)
    {
      DBUG_PRINT("admin", ("calling prepare_func"));
      switch ((*prepare_func)(thd, table, check_opt)) {
      case  1:           // error, message written to net
        ha_autocommit_or_rollback(thd, 1);
        end_trans(thd, ROLLBACK);
        close_thread_tables(thd);
        DBUG_PRINT("admin", ("simple error, admin next table"));
        continue;
      case -1:           // error, message could be written to net
        /* purecov: begin inspected */
        DBUG_PRINT("admin", ("severe error, stop"));
        goto err;
        /* purecov: end */
      default:           // should be 0 otherwise
        DBUG_PRINT("admin", ("prepare_func succeeded"));
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
      DBUG_PRINT("admin", ("open table failed"));
      if (!thd->warn_list.elements)
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                     ER_CHECK_NO_SUCH_TABLE, ER(ER_CHECK_NO_SUCH_TABLE));
      /* if it was a view will check md5 sum */
      if (table->view &&
          view_checksum(thd, table) == HA_ADMIN_WRONG_CHECKSUM)
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                     ER_VIEW_CHECKSUM, ER(ER_VIEW_CHECKSUM));
      if (thd->main_da.is_error() && 
          (thd->main_da.sql_errno() == ER_NO_SUCH_TABLE ||
           thd->main_da.sql_errno() == ER_FILE_NOT_FOUND))
        /* A missing table is just issued as a failed command */
        result_code= HA_ADMIN_FAILED;
      else
        /* Default failure code is corrupt table */
        result_code= HA_ADMIN_CORRUPT;
      goto send_result;
    }

    if (table->view)
    {
      DBUG_PRINT("admin", ("calling view_operator_func"));
      result_code= (*view_operator_func)(thd, table);
      goto send_result;
    }

    if (table->schema_table)
    {
      result_code= HA_ADMIN_NOT_IMPLEMENTED;
      goto send_result;
    }

    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      /* purecov: begin inspected */
      char buff[FN_REFLEN + MYSQL_ERRMSG_SIZE];
      size_t length;
      DBUG_PRINT("admin", ("sending error message"));
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length= my_snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                          table_name);
      protocol->store(buff, length, system_charset_info);
      ha_autocommit_or_rollback(thd, 0);
      end_trans(thd, COMMIT);
      close_thread_tables(thd);
      lex->reset_query_tables_list(FALSE);
      table->table=0;				// For query cache
      if (protocol->write())
	goto err;
      thd->main_da.reset_diagnostics_area();
      continue;
      /* purecov: end */
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->s->version)
    {
      DBUG_PRINT("admin", ("removing table from cache"));
      pthread_mutex_lock(&LOCK_open);
      const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(thd,table->table, TRUE);
      remove_table_from_cache(thd, table->table->s->db.str,
                              table->table->s->table_name.str,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      thd->exit_cond(old_message);
      DBUG_EXECUTE_IF("wait_in_mysql_admin_table", wait_for_kill_signal(thd););
      if (thd->killed)
	goto err;
      /* Flush entries in the query cache involving this table. */
      query_cache_invalidate3(thd, table->table, 0);
      open_for_modify= 0;
    }

    if (table->table->s->crashed && operator_func == &handler::ha_check)
    {
      /* purecov: begin inspected */
      DBUG_PRINT("admin", ("sending crashed warning"));
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("warning"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is marked as crashed"),
                      system_charset_info);
      if (protocol->write())
        goto err;
      /* purecov: end */
    }

    if (operator_func == &handler::ha_repair &&
        !(check_opt->sql_flags & TT_USEFRM))
    {
      if ((table->table->file->check_old_types() == HA_ADMIN_NEEDS_ALTER) ||
          (table->table->file->ha_check_for_upgrade(check_opt) ==
           HA_ADMIN_NEEDS_ALTER))
      {
        DBUG_PRINT("admin", ("recreating table"));
        ha_autocommit_or_rollback(thd, 1);
        close_thread_tables(thd);
        tmp_disable_binlog(thd); // binlogging is done by caller if wanted
        result_code= mysql_recreate_table(thd, table);
        reenable_binlog(thd);
        /*
          mysql_recreate_table() can push OK or ERROR.
          Clear 'OK' status. If there is an error, keep it:
          we will store the error message in a result set row 
          and then clear.
        */
        if (thd->main_da.is_ok())
          thd->main_da.reset_diagnostics_area();
        goto send_result;
      }
    }

    DBUG_PRINT("admin", ("calling operator_func '%s'", operator_name));
    result_code = (table->table->file->*operator_func)(thd, check_opt);
    DBUG_PRINT("admin", ("operator_func returned: %d", result_code));

send_result:

    lex->cleanup_after_one_table_open();
    thd->clear_error();  // these errors shouldn't get client
    {
      List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
      MYSQL_ERROR *err;
      while ((err= it++))
      {
        protocol->prepare_for_resend();
        protocol->store(table_name, system_charset_info);
        protocol->store((char*) operator_name, system_charset_info);
        protocol->store(warning_level_names[err->level].str,
                        warning_level_names[err->level].length,
                        system_charset_info);
        protocol->store(err->msg, system_charset_info);
        if (protocol->write())
          goto err;
      }
      mysql_reset_errors(thd, true);
    }
    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);
    protocol->store(operator_name, system_charset_info);

send_result_message:

    DBUG_PRINT("info", ("result_code: %d", result_code));
    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
       char buf[MYSQL_ERRMSG_SIZE];
       size_t length=my_snprintf(buf, sizeof(buf),
				ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	protocol->store(STRING_WITH_LEN("note"), system_charset_info);
	protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_NOT_BASE_TABLE:
      {
        char buf[MYSQL_ERRMSG_SIZE];
        size_t length= my_snprintf(buf, sizeof(buf),
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
      uint save_flags;
      Alter_info *alter_info= &lex->alter_info;

      /* Store the original value of alter_info->flags */
      save_flags= alter_info->flags;
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER TABLE,
        reopen the table and do ha_innobase::analyze() on it.
        We have to end the row, so analyze could return more rows.
      */
      protocol->store(STRING_WITH_LEN("note"), system_charset_info);
      if(alter_info->flags & ALTER_ADMIN_PARTITION)
      {
        protocol->store(STRING_WITH_LEN(
        "Table does not support optimize on partitions. All partitions "
        "will be rebuilt and analyzed."),system_charset_info);
      }
      else
      {
        protocol->store(STRING_WITH_LEN(
        "Table does not support optimize, doing recreate + analyze instead"),
        system_charset_info);
      }
      if (protocol->write())
        goto err;
      ha_autocommit_or_rollback(thd, 0);
      close_thread_tables(thd);
      DBUG_PRINT("info", ("HA_ADMIN_TRY_ALTER, trying analyze..."));
      TABLE_LIST *save_next_local= table->next_local,
                 *save_next_global= table->next_global;
      table->next_local= table->next_global= 0;
      tmp_disable_binlog(thd); // binlogging is done by caller if wanted
      result_code= mysql_recreate_table(thd, table);
      reenable_binlog(thd);
      /*
        mysql_recreate_table() can push OK or ERROR.
        Clear 'OK' status. If there is an error, keep it:
        we will store the error message in a result set row 
        and then clear.
      */
      if (thd->main_da.is_ok())
        thd->main_da.reset_diagnostics_area();
      ha_autocommit_or_rollback(thd, 0);
      close_thread_tables(thd);
      if (!result_code) // recreation went ok
      {
        /*
          Reset the ALTER_ADMIN_PARTITION bit in alter_info->flags
          to force analyze on all partitions.
         */
        alter_info->flags &= ~(ALTER_ADMIN_PARTITION);
        if ((table->table= open_ltable(thd, table, lock_type, 0)) &&
            ((result_code= table->table->file->ha_analyze(thd, check_opt)) > 0))
          result_code= 0; // analyze went ok
        alter_info->flags= save_flags;
      }
      /* Start a new row for the final status row */
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      if (result_code) // either mysql_recreate_table or analyze failed
      {
        DBUG_ASSERT(thd->is_error());
        if (thd->is_error())
        {
          const char *err_msg= thd->main_da.message();
          if (!thd->vio_ok())
          {
            sql_print_error("%s", err_msg);
          }
          else
          {
            /* Hijack the row already in-progress. */
            protocol->store(STRING_WITH_LEN("error"), system_charset_info);
            protocol->store(err_msg, system_charset_info);
            if (protocol->write())
              goto err;
            /* Start off another row for HA_ADMIN_FAILED */
            protocol->prepare_for_resend();
            protocol->store(table_name, system_charset_info);
            protocol->store(operator_name, system_charset_info);
          }
          thd->clear_error();
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
      char buf[MYSQL_ERRMSG_SIZE];
      size_t length;

      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length=my_snprintf(buf, sizeof(buf), ER(ER_TABLE_NEEDS_UPGRADE),
                         table->table_name);
      protocol->store(buf, length, system_charset_info);
      fatal_error=1;
      break;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[MYSQL_ERRMSG_SIZE];
        size_t length=my_snprintf(buf, sizeof(buf),
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
      if (fatal_error)
        table->table->s->version=0;               // Force close of table
      else if (open_for_modify)
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
    end_trans(thd, COMMIT);
    close_thread_tables(thd);
    table->table=0;				// For query cache
    if (protocol->write())
      goto err;
  }

  my_eof(thd);
  DBUG_RETURN(FALSE);

err:
  ha_autocommit_or_rollback(thd, 1);
  end_trans(thd, ROLLBACK);
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(TRUE);
}


bool mysql_backup_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_backup_table");
  WARN_DEPRECATED(thd, "6.0", "BACKUP TABLE",
                  "MySQL Administrator (mysqldump, mysql)");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"backup", TL_READ, 0, 0, 0, 0,
				&handler::ha_backup, 0));
}


bool mysql_restore_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_restore_table");
  WARN_DEPRECATED(thd, "6.0", "RESTORE TABLE",
                  "MySQL Administrator (mysqldump, mysql)");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"restore", TL_WRITE, 1, 1, 0,
				&prepare_for_restore,
				&handler::ha_restore, 0));
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
				&handler::ha_optimize, 0));
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
  if (!key_cache->key_cache_inited)
  {
    my_error(ER_UNKNOWN_KEY_CACHE, MYF(0), key_cache_name->str);
    DBUG_RETURN(TRUE);
  }
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
  /*
    We cannot allow concurrent inserts. The storage engine reads
    directly from the index file, bypassing the cache. It could read
    outdated information if parallel inserts into cache blocks happen.
  */
  DBUG_RETURN(mysql_admin_table(thd, tables, 0,
				"preload_keys", TL_READ_NO_INSERT, 0, 0, 0, 0,
				&handler::preload_keys, 0));
}



/**
  @brief          Create frm file based on I_S table

  @param[in]      thd                      thread handler
  @param[in]      schema_table             I_S table           
  @param[in]      dst_path                 path where frm should be created
  @param[in]      create_info              Create info

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/


bool mysql_create_like_schema_frm(THD* thd, TABLE_LIST* schema_table,
                                  char *dst_path, HA_CREATE_INFO *create_info)
{
  HA_CREATE_INFO local_create_info;
  Alter_info alter_info;
  bool tmp_table= (create_info->options & HA_LEX_CREATE_TMP_TABLE);
  uint keys= schema_table->table->s->keys;
  uint db_options= 0;
  DBUG_ENTER("mysql_create_like_schema_frm");

  bzero((char*) &local_create_info, sizeof(local_create_info));
  local_create_info.db_type= schema_table->table->s->db_type();
  local_create_info.row_type= schema_table->table->s->row_type;
  local_create_info.default_table_charset=default_charset_info;
  alter_info.flags= (ALTER_CHANGE_COLUMN | ALTER_RECREATE);
  schema_table->table->use_all_columns();
  if (mysql_prepare_alter_table(thd, schema_table->table,
                                &local_create_info, &alter_info))
    DBUG_RETURN(1);
  if (mysql_prepare_create_table(thd, &local_create_info, &alter_info,
                                 tmp_table, &db_options,
                                 schema_table->table->file,
                                 &schema_table->table->s->key_info, &keys, 0))
    DBUG_RETURN(1);
  local_create_info.max_rows= 0;
  if (mysql_create_frm(thd, dst_path, NullS, NullS,
                       &local_create_info, alter_info.create_list,
                       keys, schema_table->table->s->key_info,
                       schema_table->table->file))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Create a table identical to the specified table

  SYNOPSIS
    mysql_create_like_table()
    thd		Thread object
    table       Table list element for target table
    src_table   Table list element for source table
    create_info Create info

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_like_table(THD* thd, TABLE_LIST* table, TABLE_LIST* src_table,
                             HA_CREATE_INFO *create_info)
{
  TABLE *name_lock= 0;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN + 1];
  uint dst_path_length;
  char *db= table->db;
  char *table_name= table->table_name;
  int  err;
  bool res= TRUE;
  uint not_used;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  char tmp_path[FN_REFLEN];
#endif
  char ts_name[FN_LEN + 1];
  myf flags= MY_DONT_OVERWRITE_FILE;
  DBUG_ENTER("mysql_create_like_table");


  /*
    By opening source table we guarantee that it exists and no concurrent
    DDL operation will mess with it. Later we also take an exclusive
    name-lock on target table name, which makes copying of .frm file,
    call to ha_create_table() and binlogging atomic against concurrent DML
    and DDL operations on target table. Thus by holding both these "locks"
    we ensure that our statement is properly isolated from all concurrent
    operations which matter.
  */
  if (open_tables(thd, &src_table, &not_used, 0))
    DBUG_RETURN(TRUE);

  /*
    For bug#25875, Newly created table through CREATE TABLE .. LIKE
                   has no ndb_dd attributes;
    Add something to get possible tablespace info from src table,
    it can get valid tablespace name only for disk-base ndb table
  */
  if ((src_table->table->file->get_tablespace_name(thd, ts_name, FN_LEN)))
  {
    create_info->tablespace= ts_name;
    create_info->storage_media= HA_SM_DISK;
  }

  strxmov(src_path, src_table->table->s->path.str, reg_ext, NullS);

  DBUG_EXECUTE_IF("sleep_create_like_before_check_if_exists", my_sleep(6000000););

  /*
    Check that destination tables does not exist. Note that its name
    was already checked when it was added to the table list.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (src_table->table->file->ht == partition_hton)
    {
      my_error(ER_PARTITION_NO_TEMPORARY, MYF(0));
      goto err;
    }
    if (find_temporary_table(thd, db, table_name))
      goto table_exists;
    dst_path_length= build_tmptable_filename(thd, dst_path, sizeof(dst_path));
    create_info->table_options|= HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
    if (lock_table_name_if_not_cached(thd, db, table_name, &name_lock))
      goto err;
    if (!name_lock)
      goto table_exists;
    dst_path_length= build_table_filename(dst_path, sizeof(dst_path) - 1,
                                          db, table_name, reg_ext, 0);
    if (!access(dst_path, F_OK))
      goto table_exists;
  }

  DBUG_EXECUTE_IF("sleep_create_like_before_copy", my_sleep(6000000););

  if (opt_sync_frm && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
    flags|= MY_SYNC;

  /*
    Create a new table by copying from source table
    and sync the new table if the flag MY_SYNC is set

    Altough exclusive name-lock on target table protects us from concurrent
    DML and DDL operations on it we still want to wrap .FRM creation and call
    to ha_create_table() in critical section protected by LOCK_open in order
    to provide minimal atomicity against operations which disregard name-locks,
    like I_S implementation, for example. This is a temporary and should not
    be copied. Instead we should fix our code to always honor name-locks.

    Also some engines (e.g. NDB cluster) require that LOCK_open should be held
    during the call to ha_create_table(). See bug #28614 for more info.
  */
  VOID(pthread_mutex_lock(&LOCK_open));
  if (src_table->schema_table)
  {
    if (mysql_create_like_schema_frm(thd, src_table, dst_path, create_info))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }
  else if (my_copy(src_path, dst_path, flags))
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_FILE,MYF(0),dst_path,my_errno);
    VOID(pthread_mutex_unlock(&LOCK_open));
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
  */
  if (src_table->table->file->ht == partition_hton)
  {
    fn_format(tmp_path, dst_path, reg_ext, ".par", MYF(MY_REPLACE_EXT));
    strmov(dst_path, tmp_path);
    fn_format(tmp_path, src_path, reg_ext, ".par", MYF(MY_REPLACE_EXT));
    strmov(src_path, tmp_path);
    my_copy(src_path, dst_path, MYF(MY_DONT_OVERWRITE_FILE));
  }
#endif

  DBUG_EXECUTE_IF("sleep_create_like_before_ha_create", my_sleep(6000000););

  dst_path[dst_path_length - reg_ext_length]= '\0';  // Remove .frm
  if (thd->variables.keep_files_on_create)
    create_info->options|= HA_CREATE_KEEP_FILES;
  err= ha_create_table(thd, dst_path, db, table_name, create_info, 1);
  VOID(pthread_mutex_unlock(&LOCK_open));

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (err || !open_temporary_table(thd, dst_path, db, table_name, 1))
    {
      (void) rm_temporary_table(create_info->db_type,
				dst_path); /* purecov: inspected */
      goto err;     /* purecov: inspected */
    }
    thd->thread_specific_used= TRUE;
  }
  else if (err)
  {
    (void) quick_rm_table(create_info->db_type, db,
			  table_name, 0); /* purecov: inspected */
    goto err;	    /* purecov: inspected */
  }

goto binlog;

table_exists:
  if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(warn_buff, sizeof(warn_buff),
		ER(ER_TABLE_EXISTS_ERROR), table_name);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_TABLE_EXISTS_ERROR,warn_buff);
  }
  else
  {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);
    goto err;
  }

binlog:
  DBUG_EXECUTE_IF("sleep_create_like_before_binlogging", my_sleep(6000000););

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
    */
    if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (src_table->table->s->tmp_table)               // Case 2
      {
        char buf[2048];
        String query(buf, sizeof(buf), system_charset_info);
        query.length(0);  // Have to zero it since constructor doesn't

        /*
          Here we open the destination table, on which we already have
          name-lock. This is needed for store_create_info() to work.
          The table will be closed by unlink_open_table() at the end
          of this function.
        */
        table->table= name_lock;
        VOID(pthread_mutex_lock(&LOCK_open));
        if (reopen_name_locked_table(thd, table, FALSE))
        {
          VOID(pthread_mutex_unlock(&LOCK_open));
          goto err;
        }
        VOID(pthread_mutex_unlock(&LOCK_open));

       /*
         The condition avoids a crash as described in BUG#48506. Other
         binlogging problems related to CREATE TABLE IF NOT EXISTS LIKE
         when the existing object is a view will be solved by BUG 47442.
       */
        if (!table->view)
        {
          IF_DBUG(int result=)
            store_create_info(thd, table, &query,
                              create_info, FALSE /* show_database */);

          DBUG_ASSERT(result == 0); // store_create_info() always return 0
          if (write_bin_log(thd, TRUE, query.ptr(), query.length()))
            goto err;
        }
      }
      else                                      // Case 1
        if (write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
          goto err;
    }
    /*
      Case 3 and 4 does nothing under RBR
    */
  }
  else if (write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
    goto err;

  res= FALSE;

err:
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(thd, name_lock, FALSE);
    pthread_mutex_unlock(&LOCK_open);
  }
  DBUG_RETURN(res);
}


bool mysql_analyze_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  DBUG_ENTER("mysql_analyze_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"analyze", lock_type, 1, 0, 0, 0,
				&handler::ha_analyze, 0));
}


bool mysql_check_table(THD* thd, TABLE_LIST* tables,HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  DBUG_ENTER("mysql_check_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"check", lock_type,
				0, 0, HA_OPEN_FOR_REPAIR, 0,
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

  thd_proc_info(thd, "discard_or_import_tablespace");

  discard= test(tablespace_op == DISCARD_TABLESPACE);

 /*
   We set this flag so that ha_innobase::open and ::external_lock() do
   not complain when we lock the table
 */
  thd->tablespace_op= TRUE;
  if (!(table=open_ltable(thd, table_list, TL_WRITE, 0)))
  {
    thd->tablespace_op=FALSE;
    DBUG_RETURN(-1);
  }

  error= table->file->ha_discard_or_import_tablespace(discard);

  thd_proc_info(thd, "end");

  if (error)
    goto err;

  /*
    The 0 in the call below means 'not in a transaction', which means
    immediate invalidation; that is probably what we wish here
  */
  query_cache_invalidate3(thd, table_list, 0);

  /* The ALTER TABLE is always in its own transaction */
  error = ha_autocommit_or_rollback(thd, 0);
  if (end_active_trans(thd))
    error=1;
  if (error)
    goto err;
  error= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

err:
  ha_autocommit_or_rollback(thd, error);
  thd->tablespace_op=FALSE;
  
  if (error == 0)
  {
    my_ok(thd);
    DBUG_RETURN(0);
  }

  table->file->print_error(error, MYF(0));
    
  DBUG_RETURN(-1);
}

/**
  @brief Check if both DROP and CREATE are present for an index in ALTER TABLE
 
  @details Checks if any index is being modified (present as both DROP INDEX 
    and ADD INDEX) in the current ALTER TABLE statement. Needed for disabling 
    online ALTER TABLE.
  
  @param table       The table being altered
  @param alter_info  The ALTER TABLE structure
  @return presence of index being altered  
  @retval FALSE  No such index
  @retval TRUE   Have at least 1 index modified
*/

static bool
is_index_maintenance_unique (TABLE *table, Alter_info *alter_info)
{
  List_iterator<Key> key_it(alter_info->key_list);
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  Key *key;

  while ((key= key_it++))
  {
    if (key->name)
    {
      Alter_drop *drop;

      drop_it.rewind();
      while ((drop= drop_it++))
      {
        if (drop->type == Alter_drop::KEY &&
            !my_strcasecmp(system_charset_info, key->name, drop->name))
          return TRUE;
      }
    }
  }
  return FALSE;
}


/*
  SYNOPSIS
    compare_tables()
      table                     The original table.
      alter_info                Alter options, fields and keys for the new
                                table.
      create_info               Create options for the new table.
      order_num                 Number of order list elements.
      need_copy_table     OUT   Result of the comparison. Undefined if error.
                                Otherwise is one of:
                                ALTER_TABLE_METADATA_ONLY  No copy needed
                                ALTER_TABLE_DATA_CHANGED   Data changes,
                                                           copy needed
                                ALTER_TABLE_INDEX_CHANGED  Index changes,
                                                           copy might be needed
      key_info_buffer     OUT   An array of KEY structs for new indexes
      index_drop_buffer   OUT   An array of offsets into table->key_info.
      index_drop_count    OUT   The number of elements in the array.
      index_add_buffer    OUT   An array of offsets into key_info_buffer.
      index_add_count     OUT   The number of elements in the array.
      candidate_key_count OUT   The number of candidate keys in original table.

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
    TRUE   error
    FALSE  success
*/

static
bool
compare_tables(TABLE *table,
               Alter_info *alter_info,
               HA_CREATE_INFO *create_info,
               uint order_num,
               enum_alter_table_change_level *need_copy_table,
               KEY **key_info_buffer,
               uint **index_drop_buffer, uint *index_drop_count,
               uint **index_add_buffer, uint *index_add_count,
               uint *candidate_key_count)
{
  Field **f_ptr, *field;
  uint changes= 0, tmp;
  uint key_count;
  List_iterator_fast<Create_field> new_field_it, tmp_new_field_it;
  Create_field *new_field, *tmp_new_field;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  THD *thd= table->in_use;
  /*
    Remember if the new definition has new VARCHAR column;
    create_info->varchar will be reset in mysql_prepare_create_table.
  */
  bool varchar= create_info->varchar;
  bool not_nullable= true;
  DBUG_ENTER("compare_tables");

  /*
    Create a copy of alter_info.
    To compare the new and old table definitions, we need to "prepare"
    the new definition - transform it from parser output to a format
    that describes the final table layout (all column defaults are
    initialized, duplicate columns are removed). This is done by
    mysql_prepare_create_table.  Unfortunately,
    mysql_prepare_create_table performs its transformations
    "in-place", that is, modifies the argument.  Since we would
    like to keep compare_tables() idempotent (not altering any
    of the arguments) we create a copy of alter_info here and
    pass it to mysql_prepare_create_table, then use the result
    to evaluate possibility of fast ALTER TABLE, and then
    destroy the copy.
  */
  Alter_info tmp_alter_info(*alter_info, thd->mem_root);
  uint db_options= 0; /* not used */

  /* Create the prepared information. */
  if (mysql_prepare_create_table(thd, create_info,
                                 &tmp_alter_info,
                                 (table->s->tmp_table != NO_TMP_TABLE),
                                 &db_options,
                                 table->file, key_info_buffer,
                                 &key_count, 0))
    DBUG_RETURN(1);
  /* Allocate result buffers. */
  if (! (*index_drop_buffer=
         (uint*) thd->alloc(sizeof(uint) * table->s->keys)) ||
      ! (*index_add_buffer=
         (uint*) thd->alloc(sizeof(uint) * tmp_alter_info.key_list.elements)))
    DBUG_RETURN(1);
  
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
  if (table->s->fields != alter_info->create_list.elements ||
      table->s->db_type() != create_info->db_type ||
      table->s->tmp_table ||
      create_info->used_fields & HA_CREATE_USED_ENGINE ||
      create_info->used_fields & HA_CREATE_USED_CHARSET ||
      create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET ||
      (table->s->row_type != create_info->row_type) ||
      create_info->used_fields & HA_CREATE_USED_PACK_KEYS ||
      create_info->used_fields & HA_CREATE_USED_MAX_ROWS ||
      (alter_info->flags & (ALTER_RECREATE | ALTER_FOREIGN_KEY)) ||
      order_num ||
      !table->s->mysql_version ||
      (table->s->frm_version < FRM_VER_TRUE_VARCHAR && varchar))
  {
    *need_copy_table= ALTER_TABLE_DATA_CHANGED;
    DBUG_RETURN(0);
  }

  /*
    Use transformed info to evaluate possibility of fast ALTER TABLE
    but use the preserved field to persist modifications.
  */
  new_field_it.init(alter_info->create_list);
  tmp_new_field_it.init(tmp_alter_info.create_list);

  /*
    Go through fields and check if the original ones are compatible
    with new table.
  */
  for (f_ptr= table->field, new_field= new_field_it++,
       tmp_new_field= tmp_new_field_it++;
       (field= *f_ptr);
       f_ptr++, new_field= new_field_it++,
       tmp_new_field= tmp_new_field_it++)
  {
    /* Make sure we have at least the default charset in use. */
    if (!new_field->charset)
      new_field->charset= create_info->default_table_charset;

    /* Check that NULL behavior is same for old and new fields */
    if ((tmp_new_field->flags & NOT_NULL_FLAG) !=
	(uint) (field->flags & NOT_NULL_FLAG))
    {
      *need_copy_table= ALTER_TABLE_DATA_CHANGED;
      DBUG_RETURN(0);
    }

    /* Don't pack rows in old tables if the user has requested this. */
    if (create_info->row_type == ROW_TYPE_DYNAMIC ||
	(tmp_new_field->flags & BLOB_FLAG) ||
	(tmp_new_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED))
      create_info->table_options|= HA_OPTION_PACK_RECORD;

    /* Check if field was renamed */
    field->flags&= ~FIELD_IS_RENAMED;
    if (my_strcasecmp(system_charset_info,
		      field->field_name,
		      tmp_new_field->field_name))
      field->flags|= FIELD_IS_RENAMED;      

    /* Evaluate changes bitmap and send to check_if_incompatible_data() */
    if (!(tmp= field->is_equal(tmp_new_field)))
    {
      *need_copy_table= ALTER_TABLE_DATA_CHANGED;
      DBUG_RETURN(0);
    }
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
  KEY *new_key_end= *key_info_buffer + key_count;

  DBUG_PRINT("info", ("index count old: %d  new: %d",
                      table->s->keys, key_count));
  /*
    Step through all keys of the old table and search matching new keys.
  */
  *index_drop_count= 0;
  *index_add_count= 0;
  *candidate_key_count= 0;
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    KEY_PART_INFO *table_part;
    KEY_PART_INFO *table_part_end= table_key->key_part + table_key->key_parts;
    KEY_PART_INFO *new_part;

   /*
      Check if key is a candidate key, i.e. a unique index with no index
      fields nullable, then key is either already primary key or could
      be promoted to primary key if the original primary key is dropped.
      Count all candidate keys.
    */
    not_nullable= true;
    for (table_part= table_key->key_part;
         table_part < table_part_end;
         table_part++)
    {
      not_nullable= not_nullable && (! table_part->field->maybe_null());
    }
    if ((table_key->flags & HA_NOSAME) && not_nullable)
      (*candidate_key_count)++;

    /* Search a new key with the same name. */
    for (new_key= *key_info_buffer; new_key < new_key_end; new_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (new_key >= new_key_end)
    {
      /* Key not found. Add the offset of the key to the drop buffer. */
      (*index_drop_buffer)[(*index_drop_count)++]= table_key - table->key_info;
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
    (*index_drop_buffer)[(*index_drop_count)++]= table_key - table->key_info;
    (*index_add_buffer)[(*index_add_count)++]= new_key - *key_info_buffer;
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
  for (new_key= *key_info_buffer; new_key < new_key_end; new_key++)
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
      (*index_add_buffer)[(*index_add_count)++]= new_key - *key_info_buffer;
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
  {
    *need_copy_table= ALTER_TABLE_DATA_CHANGED;
    DBUG_RETURN(0);
  }

  if (*index_drop_count || *index_add_count)
  {
    *need_copy_table= ALTER_TABLE_INDEX_CHANGED;
    DBUG_RETURN(0);
  }

  *need_copy_table= ALTER_TABLE_METADATA_ONLY; // Tables are compatible
  DBUG_RETURN(0);
}


/*
  Manages enabling/disabling of indexes for ALTER TABLE

  SYNOPSIS
    alter_table_manage_keys()
      table                  Target table
      indexes_were_disabled  Whether the indexes of the from table
                             were disabled
      keys_onoff             ENABLE | DISABLE | LEAVE_AS_IS

  RETURN VALUES
    FALSE  OK
    TRUE   Error
*/

static
bool alter_table_manage_keys(TABLE *table, int indexes_were_disabled,
                             enum enum_enable_or_disable keys_onoff)
{
  int error= 0;
  DBUG_ENTER("alter_table_manage_keys");
  DBUG_PRINT("enter", ("table=%p were_disabled=%d on_off=%d",
             table, indexes_were_disabled, keys_onoff));

  switch (keys_onoff) {
  case ENABLE:
    error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
    break;
  case LEAVE_AS_IS:
    if (!indexes_were_disabled)
      break;
    /* fall-through: disabled indexes */
  case DISABLE:
    error= table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if (error == HA_ERR_WRONG_COMMAND)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                        table->s->table_name.str);
    error= 0;
  } else if (error)
    table->file->print_error(error, MYF(0));

  DBUG_RETURN(error);
}


/**
  Prepare column and key definitions for CREATE TABLE in ALTER TABLE.

  This function transforms parse output of ALTER TABLE - lists of
  columns and keys to add, drop or modify into, essentially,
  CREATE TABLE definition - a list of columns and keys of the new
  table. While doing so, it also performs some (bug not all)
  semantic checks.

  This function is invoked when we know that we're going to
  perform ALTER TABLE via a temporary table -- i.e. fast ALTER TABLE
  is not possible, perhaps because the ALTER statement contains
  instructions that require change in table data, not only in
  table definition or indexes.

  @param[in,out]  thd         thread handle. Used as a memory pool
                              and source of environment information.
  @param[in]      table       the source table, open and locked
                              Used as an interface to the storage engine
                              to acquire additional information about
                              the original table.
  @param[in,out]  create_info A blob with CREATE/ALTER TABLE
                              parameters
  @param[in,out]  alter_info  Another blob with ALTER/CREATE parameters.
                              Originally create_info was used only in
                              CREATE TABLE and alter_info only in ALTER TABLE.
                              But since ALTER might end-up doing CREATE,
                              this distinction is gone and we just carry
                              around two structures.

  @return
    Fills various create_info members based on information retrieved
    from the storage engine.
    Sets create_info->varchar if the table has a VARCHAR column.
    Prepares alter_info->create_list and alter_info->key_list with
    columns and keys of the new table.
  @retval TRUE   error, out of memory or a semantical error in ALTER
                 TABLE instructions
  @retval FALSE  success
*/

static bool
mysql_prepare_alter_table(THD *thd, TABLE *table,
                          HA_CREATE_INFO *create_info,
                          Alter_info *alter_info)
{
  /* New column definitions are added here */
  List<Create_field> new_create_list;
  /* New key definitions are added here */
  List<Key> new_key_list;
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  List_iterator<Create_field> def_it(alter_info->create_list);
  List_iterator<Alter_column> alter_it(alter_info->alter_list);
  List_iterator<Key> key_it(alter_info->key_list);
  List_iterator<Create_field> find_it(new_create_list);
  List_iterator<Create_field> field_it(new_create_list);
  List<Key_part_spec> key_parts;
  uint db_create_options= (table->s->db_create_options
                           & ~(HA_OPTION_PACK_RECORD));
  uint used_fields= create_info->used_fields;
  KEY *key_info=table->key_info;
  bool rc= TRUE;

  DBUG_ENTER("mysql_prepare_alter_table");

  create_info->varchar= FALSE;
  /* Let new create options override the old ones */
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS))
    create_info->min_rows= table->s->min_rows;
  if (!(used_fields & HA_CREATE_USED_MAX_ROWS))
    create_info->max_rows= table->s->max_rows;
  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    create_info->avg_row_length= table->s->avg_row_length;
  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->s->table_charset;
  if (!(used_fields & HA_CREATE_USED_AUTO) && table->found_next_number_field)
  {
    /* Table has an autoincrement, copy value to new table */
    table->file->info(HA_STATUS_AUTO);
    create_info->auto_increment_value= table->file->stats.auto_increment_value;
  }
  if (!(used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE))
    create_info->key_block_size= table->s->key_block_size;

  if (!create_info->tablespace && create_info->storage_media != HA_SM_MEMORY)
  {
    char *tablespace= static_cast<char *>(thd->alloc(FN_LEN + 1));
    /*
       Regular alter table of disk stored table (no tablespace/storage change)
       Copy tablespace name
    */
    if (tablespace &&
        (table->file->get_tablespace_name(thd, tablespace, FN_LEN)))
      create_info->tablespace= tablespace;
  }
  restore_record(table, s->default_values);     // Empty record for DEFAULT
  Create_field *def;

  /*
    First collect all fields from table which isn't in drop_list
  */
  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (field->type() == MYSQL_TYPE_STRING)
      create_info->varchar= TRUE;
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
      /*
        ALTER TABLE DROP COLUMN always changes table data even in cases
        when new version of the table has the same structure as the old
        one.
      */
      alter_info->change_level= ALTER_TABLE_DATA_CHANGED;
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
	new_create_list.push_back(def);
	def_it.remove();
      }
    }
    else
    {
      /*
        This field was not dropped and not changed, add it to the list
        for the new table.
      */
      def= new Create_field(field, field);
      new_create_list.push_back(def);
      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
      {
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
      }
      if (alter)
      {
	if (def->sql_type == MYSQL_TYPE_BLOB)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), def->change);
          goto err;
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
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table->s->table_name.str);
      goto err;
    }
    /*
      Check that the DATE/DATETIME not null field we are going to add is
      either has a default value or the '0000-00-00' is allowed by the
      set sql mode.
      If the '0000-00-00' value isn't allowed then raise the error_if_not_empty
      flag to allow ALTER TABLE only if the table to be altered is empty.
    */
    if ((def->sql_type == MYSQL_TYPE_DATE ||
         def->sql_type == MYSQL_TYPE_NEWDATE ||
         def->sql_type == MYSQL_TYPE_DATETIME) &&
         !alter_info->datetime_field &&
         !(~def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)) &&
         thd->variables.sql_mode & MODE_NO_ZERO_DATE)
    {
        alter_info->datetime_field= def;
        alter_info->error_if_not_empty= TRUE;
    }
    if (!def->after)
      new_create_list.push_back(def);
    else if (def->after == first_keyword)
    {
      new_create_list.push_front(def);
      /*
        Re-ordering columns in table can't be done using in-place algorithm
        as it always changes table data.
      */
      alter_info->change_level= ALTER_TABLE_DATA_CHANGED;
    }
    else
    {
      Create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(system_charset_info,def->after, find->field_name))
	  break;
      }
      if (!find)
      {
	my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table->s->table_name.str);
        goto err;
      }
      find_it.after(def);			// Put element after this
      /*
        Re-ordering columns in table can't be done using in-place algorithm
        as it always changes table data.
      */
      alter_info->change_level= ALTER_TABLE_DATA_CHANGED;
    }
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             alter_info->alter_list.head()->name, table->s->table_name.str);
    goto err;
  }
  if (!new_create_list.elements)
  {
    my_message(ER_CANT_REMOVE_ALL_FIELDS, ER(ER_CANT_REMOVE_ALL_FIELDS),
               MYF(0));
    goto err;
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */

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
      Create_field *cfield;
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
            /* spatial keys can't have sub-key length */
            (key_info->flags & HA_SPATIAL) ||
            (cfield->field->field_length == key_part_length &&
             !f_is_blob(key_part->key_type)) ||
	    (cfield->length && (cfield->length < key_part_length /
                                key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new Key_part_spec(cfield->field_name,
					    key_part_length));
    }
    if (key_parts.elements)
    {
      KEY_CREATE_INFO key_create_info;
      Key *key;
      enum Key::Keytype key_type;
      bzero((char*) &key_create_info, sizeof(key_create_info));

      key_create_info.algorithm= key_info->algorithm;
      if (key_info->flags & HA_USES_BLOCK_SIZE)
        key_create_info.block_size= key_info->block_size;
      if (key_info->flags & HA_USES_PARSER)
        key_create_info.parser_name= *plugin_name(key_info->parser);

      if (key_info->flags & HA_SPATIAL)
        key_type= Key::SPATIAL;
      else if (key_info->flags & HA_NOSAME)
      {
        if (! my_strcasecmp(system_charset_info, key_name, primary_key_name))
          key_type= Key::PRIMARY;
        else
          key_type= Key::UNIQUE;
      }
      else if (key_info->flags & HA_FULLTEXT)
        key_type= Key::FULLTEXT;
      else
        key_type= Key::MULTIPLE;

      key= new Key(key_type, key_name,
                   &key_create_info,
                   test(key_info->flags & HA_GENERATED_KEY),
                   key_parts);
      new_key_list.push_back(key);
    }
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type != Key::FOREIGN_KEY)
        new_key_list.push_back(key);
      if (key->name &&
	  !my_strcasecmp(system_charset_info,key->name,primary_key_name))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
        goto err;
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

  rc= FALSE;
  alter_info->create_list.swap(new_create_list);
  alter_info->key_list.swap(new_key_list);
err:
  DBUG_RETURN(rc);
}


/*
  Alter table

  SYNOPSIS
    mysql_alter_table()
      thd              Thread handle
      new_db           If there is a RENAME clause
      new_name         If there is a RENAME clause
      create_info      Information from the parsing phase about new
                       table properties.
      table_list       The table to change.
      alter_info       Lists of fields, keys to be changed, added
                       or dropped.
      order_num        How many ORDER BY fields has been specified.
      order            List of fields to ORDER BY.
      ignore           Whether we have ALTER IGNORE TABLE

  DESCRIPTION
    This is a veery long function and is everything but the kitchen sink :)
    It is used to alter a table and not only by ALTER TABLE but also
    CREATE|DROP INDEX are mapped on this function.

    When the ALTER TABLE statement just does a RENAME or ENABLE|DISABLE KEYS,
    or both, then this function short cuts its operation by renaming
    the table and/or enabling/disabling the keys. In this case, the FRM is
    not changed, directly by mysql_alter_table. However, if there is a
    RENAME + change of a field, or an index, the short cut is not used.
    See how `create_list` is used to generate the new FRM regarding the
    structure of the fields. The same is done for the indices of the table.

    Important is the fact, that this function tries to do as little work as
    possible, by finding out whether a intermediate table is needed to copy
    data into and when finishing the altering to use it as the original table.
    For this reason the function compare_tables() is called, which decides
    based on all kind of data how similar are the new and the original
    tables.

  RETURN VALUES
    FALSE  OK
    TRUE   Error
*/

bool mysql_alter_table(THD *thd,char *new_db, char *new_name,
                       HA_CREATE_INFO *create_info,
                       TABLE_LIST *table_list,
                       Alter_info *alter_info,
                       uint order_num, ORDER *order, bool ignore)
{
  TABLE *table, *new_table= 0, *name_lock= 0;
  int error= 0;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN + 1];
  char new_alias_buff[FN_REFLEN], *table_name, *db, *new_alias, *alias;
  char index_file[FN_REFLEN], data_file[FN_REFLEN];
  char path[FN_REFLEN + 1];
  char reg_path[FN_REFLEN+1];
  ha_rows copied,deleted;
  handlerton *old_db_type, *new_db_type, *save_old_db_type;
  legacy_db_type table_type;
  frm_type_enum frm_type;
  enum_alter_table_change_level need_copy_table= ALTER_TABLE_METADATA_ONLY;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  uint fast_alter_partition= 0;
  bool partition_changed= FALSE;
#endif
  bool need_lock_for_indexes= TRUE;
  KEY  *key_info_buffer;
  uint index_drop_count= 0;
  uint *index_drop_buffer= NULL;
  uint index_add_count= 0;
  uint *index_add_buffer= NULL;
  uint candidate_key_count= 0;
  bool no_pk;
  DBUG_ENTER("mysql_alter_table");

  /*
    Check if we attempt to alter mysql.slow_log or
    mysql.general_log table and return an error if
    it is the case.
    TODO: this design is obsolete and will be removed.
  */
  if (table_list && table_list->db && table_list->table_name)
  {
    int table_kind= 0;

    table_kind= check_if_log_table(table_list->db_length, table_list->db,
                                   table_list->table_name_length,
                                   table_list->table_name, 0);

    if (table_kind)
    {
      /* Disable alter of enabled log tables */
      if (logger.is_log_table_enabled(table_kind))
      {
        my_error(ER_BAD_LOG_STATEMENT, MYF(0), "ALTER");
        DBUG_RETURN(TRUE);
      }

      /* Disable alter of log tables to unsupported engine */
      if ((create_info->used_fields & HA_CREATE_USED_ENGINE) &&
          (!create_info->db_type || /* unknown engine */
           !(create_info->db_type->flags & HTON_SUPPORT_LOG_TABLES)))
      {
        my_error(ER_UNSUPORTED_LOG_ENGINE, MYF(0));
        DBUG_RETURN(TRUE);
      }

#ifdef WITH_PARTITION_STORAGE_ENGINE
      if (alter_info->flags & ALTER_PARTITION)
      {
        my_error(ER_WRONG_USAGE, MYF(0), "PARTITION", "log table");
        DBUG_RETURN(TRUE);
      }
#endif
    }
  }

  /*
    Assign variables table_name, new_name, db, new_db, path, reg_path
    to simplify further comparisions: we want to see if it's a RENAME
    later just by comparing the pointers, avoiding the need for strcmp.
  */
  thd_proc_info(thd, "init");
  table_name=table_list->table_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;
  db=table_list->db;
  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;
  build_table_filename(reg_path, sizeof(reg_path) - 1, db, table_name, reg_ext, 0);
  build_table_filename(path, sizeof(path) - 1, db, table_name, "", 0);

  mysql_ha_rm_tables(thd, table_list, FALSE);

  /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER TABLE */
  if (alter_info->tablespace_op != NO_TABLESPACE_OP)
    /* Conditionally writes to binlog. */
    DBUG_RETURN(mysql_discard_or_import_tablespace(thd,table_list,
						   alter_info->tablespace_op));
  strxnmov(new_name_buff, sizeof (new_name_buff) - 1, mysql_data_home, "/", db, 
           "/", table_name, reg_ext, NullS);
  (void) unpack_filename(new_name_buff, new_name_buff);
  /*
    If this is just a rename of a view, short cut to the
    following scenario: 1) lock LOCK_open 2) do a RENAME
    2) unlock LOCK_open.
    This is a copy-paste added to make sure
    ALTER (sic:) TABLE .. RENAME works for views. ALTER VIEW is handled
    as an independent branch in mysql_execute_command. The need
    for a copy-paste arose because the main code flow of ALTER TABLE
    ... RENAME tries to use open_ltable, which does not work for views
    (open_ltable was never modified to merge table lists of child tables
    into the main table list, like open_tables does).
    This code is wrong and will be removed, please do not copy.
  */
  frm_type= mysql_frm_type(thd, new_name_buff, &table_type);
  /* Rename a view */
  /* Sic: there is a race here */
  if (frm_type == FRMTYPE_VIEW && !(alter_info->flags & ~ALTER_RENAME))
  {
    /*
      The following branch handles "ALTER VIEW v1 /no arguments/;"
      This feature is not documented one. 
      However, before "OPTIMIZE TABLE t1;" was implemented, 
      ALTER TABLE with no alter_specifications was used to force-rebuild
      the table. That's why this grammar is allowed. That's why we ignore
      it for views. So just do nothing in such a case.
    */
    if (!new_name)
    {
      my_ok(thd);
      DBUG_RETURN(FALSE);
    }

    /*
      Avoid problems with a rename on a table that we have locked or
      if the user is trying to to do this in a transcation context
    */

    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      DBUG_RETURN(TRUE);
    }

    if (wait_if_global_read_lock(thd,0,1))
      DBUG_RETURN(TRUE);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (lock_table_names(thd, table_list))
    {
      error= 1;
      goto view_err;
    }
    
    if (!do_rename(thd, table_list, new_db, new_name, new_name, 1))
    {
      if (mysql_bin_log.is_open())
      {
        thd->clear_error();
        Query_log_event qinfo(thd, thd->query(), thd->query_length(),
                              0, FALSE, 0);
        if ((error= mysql_bin_log.write(&qinfo)))
          goto view_err_unlock;
      }
      my_ok(thd);
    }

view_err_unlock:
    unlock_table_names(thd, table_list, (TABLE_LIST*) 0);

view_err:
    pthread_mutex_unlock(&LOCK_open);
    start_waiting_global_read_lock(thd);
    DBUG_RETURN(error);
  }

  if (!(table= open_n_lock_single_table(thd, table_list, TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(TRUE);
  table->use_all_columns();

  /*
    Prohibit changing of the UNION list of a non-temporary MERGE table
    under LOCK tables. It would be quite difficult to reuse a shrinked
    set of tables from the old table or to open a new TABLE object for
    an extended list and verify that they belong to locked tables.
  */
  if (thd->locked_tables &&
      (create_info->used_fields & HA_CREATE_USED_UNION) &&
      (table->s->tmp_table == NO_TMP_TABLE))
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(TRUE);
  }

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
        if (lock_table_name_if_not_cached(thd, new_db, new_name, &name_lock))
          DBUG_RETURN(TRUE);
        if (!name_lock)
        {
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
	  DBUG_RETURN(TRUE);
        }

        build_table_filename(new_name_buff, sizeof(new_name_buff) - 1,
                             new_db, new_name_buff, reg_ext, 0);
        if (!access(new_name_buff, F_OK))
	{
	  /* Table will be closed in do_command() */
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
          goto err;
	}
      }
    }
  }
  else
  {
    new_alias= (lower_case_table_names == 2) ? alias : table_name;
    new_name= table_name;
  }

  old_db_type= table->s->db_type();
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

  if (check_engine(thd, new_name, create_info))
    goto err;
  new_db_type= create_info->db_type;

  if ((new_db_type != old_db_type ||
       alter_info->flags & ALTER_PARTITION) &&
      !table->file->can_switch_engines())
  {
    my_error(ER_ROW_IS_REFERENCED, MYF(0));
    goto err;
  }

  /*
   If this is an ALTER TABLE and no explicit row type specified reuse
   the table's row type.
   Note : this is the same as if the row type was specified explicitly.
  */
  if (create_info->row_type == ROW_TYPE_NOT_USED)
  {
    /* ALTER TABLE without explicit row type */
    create_info->row_type= table->s->row_type;
  }
  else
  {
    /* ALTER TABLE with specific row type */
    create_info->used_fields |= HA_CREATE_USED_ROW_FORMAT;
  }

  DBUG_PRINT("info", ("old type: %s  new type: %s",
             ha_resolve_storage_engine_name(old_db_type),
             ha_resolve_storage_engine_name(new_db_type)));
  if (ha_check_storage_engine_flag(old_db_type, HTON_ALTER_NOT_SUPPORTED) ||
      ha_check_storage_engine_flag(new_db_type, HTON_ALTER_NOT_SUPPORTED))
  {
    DBUG_PRINT("info", ("doesn't support alter"));
    my_error(ER_ILLEGAL_HA, MYF(0), table_name);
    goto err;
  }
  
  thd_proc_info(thd, "setup");
  if (!(alter_info->flags & ~(ALTER_RENAME | ALTER_KEYS_ONOFF)) &&
      !table->s->tmp_table) // no need to touch frm
  {
    switch (alter_info->keys_onoff) {
    case LEAVE_AS_IS:
      break;
    case ENABLE:
      /*
        wait_while_table_is_used() ensures that table being altered is
        opened only by this thread and that TABLE::TABLE_SHARE::version
        of TABLE object corresponding to this table is 0.
        The latter guarantees that no DML statement will open this table
        until ALTER TABLE finishes (i.e. until close_thread_tables())
        while the fact that the table is still open gives us protection
        from concurrent DDL statements.
      */
      VOID(pthread_mutex_lock(&LOCK_open));
      wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_EXECUTE_IF("sleep_alter_enable_indexes", my_sleep(6000000););
      error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    case DISABLE:
      VOID(pthread_mutex_lock(&LOCK_open));
      wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
      VOID(pthread_mutex_unlock(&LOCK_open));
      error=table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    default:
      DBUG_ASSERT(FALSE);
      error= 0;
      break;
    }
    if (error == HA_ERR_WRONG_COMMAND)
    {
      error= 0;
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
    }

    /*
      Unlike to the above case close_cached_table() below will remove ALL
      instances of TABLE from table cache (it will also remove table lock
      held by this thread). So to make actual table renaming and writing
      to binlog atomic we have to put them into the same critical section
      protected by LOCK_open mutex. This also removes gap for races between
      access() and mysql_rename_table() calls.
    */

    if (!error && (new_name != table_name || new_db != db))
    {
      thd_proc_info(thd, "rename");

      /*
        Workaround InnoDB ending the transaction when the table instance
        is unlocked/closed (close_cached_table below), otherwise the trx
        state will differ between the server and storage engine layers.
      */
      ha_autocommit_or_rollback(thd, 0);

      VOID(pthread_mutex_lock(&LOCK_open));
      /*
        Then do a 'simple' rename of the table. First we need to close all
        instances of 'source' table.
      */
      close_cached_table(thd, table);
      /*
        Then, we want check once again that target table does not exist.
        Actually the order of these two steps does not matter since
        earlier we took name-lock on the target table, so we do them
        in this particular order only to be consistent with 5.0, in which
        we don't take this name-lock and where this order really matters.
        TODO: Investigate if we need this access() check at all.
      */
      if (!access(new_name_buff,F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name);
	error= -1;
      }
      else
      {
	*fn_ext(new_name)=0;
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
    }
    else
      VOID(pthread_mutex_lock(&LOCK_open));

    if (error == HA_ERR_WRONG_COMMAND)
    {
      error= 0;
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
    }

    if (!error)
    {
      error= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
      if (!error)
        my_ok(thd);
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    if (name_lock)
      unlink_open_table(thd, name_lock, FALSE);
    VOID(pthread_mutex_unlock(&LOCK_open));
    table_list->table= NULL;                    // For query cache
    query_cache_invalidate3(thd, table_list, 0);
    DBUG_RETURN(error);
  }

  /* We have to do full alter table. */

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (prep_alter_part_table(thd, table, alter_info, create_info, old_db_type,
                            &partition_changed, &fast_alter_partition))
    goto err;
#endif
  /*
    If the old table had partitions and we are doing ALTER TABLE ...
    engine= <new_engine>, the new table must preserve the original
    partitioning. That means that the new engine is still the
    partitioning engine, not the engine specified in the parser.
    This is discovered  in prep_alter_part_table, which in such case
    updates create_info->db_type.
    Now we need to update the stack copy of create_info->db_type,
    as otherwise we won't be able to correctly move the files of the
    temporary table to the result table files.
  */
  new_db_type= create_info->db_type;

  if (is_index_maintenance_unique (table, alter_info))
    need_copy_table= ALTER_TABLE_DATA_CHANGED;

  if (mysql_prepare_alter_table(thd, table, create_info, alter_info))
    goto err;
  
  if (need_copy_table == ALTER_TABLE_METADATA_ONLY)
    need_copy_table= alter_info->change_level;

  set_table_default_charset(thd, create_info, db);

  if (thd->variables.old_alter_table
      || (table->s->db_type() != create_info->db_type)
#ifdef WITH_PARTITION_STORAGE_ENGINE
      || partition_changed
#endif
     )
    need_copy_table= ALTER_TABLE_DATA_CHANGED;
  else
  {
    enum_alter_table_change_level need_copy_table_res=ALTER_TABLE_METADATA_ONLY;
    /* Check how much the tables differ. */
    if (compare_tables(table, alter_info,
                       create_info, order_num,
                       &need_copy_table_res,
                       &key_info_buffer,
                       &index_drop_buffer, &index_drop_count,
                       &index_add_buffer, &index_add_count,
                       &candidate_key_count))
      goto err;
   
    DBUG_EXECUTE_IF("alter_table_only_metadata_change", {
      if (need_copy_table_res != ALTER_TABLE_METADATA_ONLY)
        goto err; });
    DBUG_EXECUTE_IF("alter_table_only_index_change", {
      if (need_copy_table_res != ALTER_TABLE_INDEX_CHANGED)
        goto err; });
   
    if (need_copy_table == ALTER_TABLE_METADATA_ONLY)
      need_copy_table= need_copy_table_res;
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

    alter_flags= table->file->alter_table_flags(alter_info->flags);
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
        /* 
           Unique key. Check for "PRIMARY". 
           or if dropping last unique key
        */
        if ((uint) (key - table->key_info) == table->s->primary_key)
        {
          DBUG_PRINT("info", ("Dropping primary key"));
          /* Primary key. */
          needed_online_flags|=  HA_ONLINE_DROP_PK_INDEX;
          needed_fast_flags|= HA_ONLINE_DROP_PK_INDEX_NO_WRITES;
          pk_changed++;
          candidate_key_count--;
        }
        else
        {
          KEY_PART_INFO *part_end= key->key_part + key->key_parts;
          bool is_candidate_key= true;

          /* Non-primary unique key. */
          needed_online_flags|=  HA_ONLINE_DROP_UNIQUE_INDEX;
          needed_fast_flags|= HA_ONLINE_DROP_UNIQUE_INDEX_NO_WRITES;

          /*
            Check if all fields in key are declared
            NOT NULL and adjust candidate_key_count
          */
          for (KEY_PART_INFO *key_part= key->key_part;
               key_part < part_end;
               key_part++)
            is_candidate_key=
              (is_candidate_key && 
               (! table->field[key_part->fieldnr-1]->maybe_null()));
          if (is_candidate_key)
            candidate_key_count--;
        }
      }
      else
      {
        /* Non-unique key. */
        needed_online_flags|=  HA_ONLINE_DROP_INDEX;
        needed_fast_flags|= HA_ONLINE_DROP_INDEX_NO_WRITES;
      }
    }
    no_pk= ((table->s->primary_key == MAX_KEY) ||
            (needed_online_flags & HA_ONLINE_DROP_PK_INDEX));
    /* Check added indexes. */
    for (idx_p= index_add_buffer, idx_end_p= idx_p + index_add_count;
         idx_p < idx_end_p;
         idx_p++)
    {
      key= key_info_buffer + *idx_p;
      DBUG_PRINT("info", ("index added: '%s'", key->name));
      if (key->flags & HA_NOSAME)
      {
        /* Unique key */

        KEY_PART_INFO *part_end= key->key_part + key->key_parts;    
        bool is_candidate_key= true;

        /*
          Check if all fields in key are declared
          NOT NULL
         */
        for (KEY_PART_INFO *key_part= key->key_part;
             key_part < part_end;
             key_part++)
          is_candidate_key=
            (is_candidate_key && 
             (! table->field[key_part->fieldnr]->maybe_null()));

        /*
           Check for "PRIMARY"
           or if adding first unique key
           defined on non-nullable fields
        */

        if ((!my_strcasecmp(system_charset_info,
                            key->name, primary_key_name)) ||
            (no_pk && candidate_key_count == 0 && is_candidate_key))
        {
          DBUG_PRINT("info", ("Adding primary key"));
          /* Primary key. */
          needed_online_flags|=  HA_ONLINE_ADD_PK_INDEX;
          needed_fast_flags|= HA_ONLINE_ADD_PK_INDEX_NO_WRITES;
          pk_changed++;
          no_pk= false;
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

    if ((candidate_key_count > 0) && 
        (needed_online_flags & HA_ONLINE_DROP_PK_INDEX))
    {
      /*
        Dropped primary key when there is some other unique 
        not null key that should be converted to primary key
      */
      needed_online_flags|=  HA_ONLINE_ADD_PK_INDEX;
      needed_fast_flags|= HA_ONLINE_ADD_PK_INDEX_NO_WRITES;
      pk_changed= 2;
    }

    DBUG_PRINT("info", ("needed_online_flags: 0x%lx, needed_fast_flags: 0x%lx",
                        needed_online_flags, needed_fast_flags));
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
        need_copy_table= ALTER_TABLE_METADATA_ONLY;
        need_lock_for_indexes= FALSE;
      }
      else if ((alter_flags & needed_fast_flags) == needed_fast_flags)
      {
        /* All required fast flags are present. */
        need_copy_table= ALTER_TABLE_METADATA_ONLY;
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
  if (need_copy_table == ALTER_TABLE_METADATA_ONLY)
    create_info->frm_only= 1;

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (fast_alter_partition)
  {
    DBUG_ASSERT(!name_lock);
    DBUG_RETURN(fast_alter_partition_table(thd, table, alter_info,
                                           create_info, table_list,
                                           db, table_name,
                                           fast_alter_partition));
  }
#endif

  my_snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  /* Safety fix for innodb */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);

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
      At end, rename intermediate tables, and symlinks to intermediate
      table, to final table name.
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

  DEBUG_SYNC(thd, "alter_table_before_create_table_no_lock");
  /*
    Create a table with a temporary name.
    With create_info->frm_only == 1 this creates a .frm file only.
    We don't log the statement, it will be logged later.
  */
  tmp_disable_binlog(thd);
  error= mysql_create_table_no_lock(thd, new_db, tmp_name,
                                    create_info,
                                    alter_info,
                                    1, 0);
  reenable_binlog(thd);
  if (error)
    goto err;

  /* Open the table if we need to copy the data. */
  DBUG_PRINT("info", ("need_copy_table: %u", need_copy_table));
  if (need_copy_table != ALTER_TABLE_METADATA_ONLY)
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
      char path[FN_REFLEN + 1];
      /* table is a normal table: Create temporary table in same directory */
      build_table_filename(path, sizeof(path) - 1, new_db, tmp_name, "",
                           FN_IS_TMP);
      /* Open our intermediate table */
      new_table=open_temporary_table(thd, path, new_db, tmp_name,0);
    }
    if (!new_table)
      goto err1;
    /*
      Note: In case of MERGE table, we do not attach children. We do not
      copy data for MERGE tables. Only the children have data.
    */
  }

  /* Copy the data if necessary. */
  thd->count_cuted_fields= CHECK_FIELD_WARN;	// calc cuted fields
  thd->cuted_fields=0L;
  copied=deleted=0;
  /*
    We do not copy data for MERGE tables. Only the children have data.
    MERGE tables have HA_NO_COPY_ON_ALTER set.
  */
  if (new_table && !(new_table->file->ha_table_flags() & HA_NO_COPY_ON_ALTER))
  {
    /* We don't want update TIMESTAMP fields during ALTER TABLE. */
    new_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    new_table->next_number_field=new_table->found_next_number_field;
    thd_proc_info(thd, "copy to tmp table");
    error= copy_data_between_tables(table, new_table,
                                    alter_info->create_list, ignore,
                                    order_num, order, &copied, &deleted,
                                    alter_info->keys_onoff,
                                    alter_info->error_if_not_empty);
  }
  else
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
    VOID(pthread_mutex_unlock(&LOCK_open));
    thd_proc_info(thd, "manage keys");
    alter_table_manage_keys(table, table->file->indexes_are_disabled(),
                            alter_info->keys_onoff);
    error= ha_autocommit_or_rollback(thd, 0);
    if (end_active_trans(thd))
      error= 1;
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

    table->file->ha_prepare_for_alter();
    if (index_add_count)
    {
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
    if (ha_autocommit_or_rollback(thd, 0) || end_active_trans(thd))
      goto err1;
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
    /*
      If LOCK TABLES list is not empty and contains this table,
      unlock the table and remove the table from this list.
    */
    mysql_lock_remove(thd, thd->locked_tables, table, FALSE);
    /* Remove link to old table and rename the new one */
    close_temporary_table(thd, table, 1, 1);
    /* Should pass the 'new_name' as we store table name in the cache */
    if (rename_temporary_table(thd, new_table, new_db, new_name))
      goto err1;
    /* We don't replicate alter table statement on temporary tables */
    if (!thd->current_stmt_binlog_row_based &&
        write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
      DBUG_RETURN(TRUE);
    goto end_temporary;
  }

  if (new_table)
  {
    /*
      Close the intermediate table that will be the new table.
      Note that MERGE tables do not have their children attached here.
    */
    intern_close_table(new_table);
    my_free(new_table,MYF(0));
  }
  DEBUG_SYNC(thd, "alter_table_before_rename_result_table");
  VOID(pthread_mutex_lock(&LOCK_open));
  if (error)
  {
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }

  /*
    Data is copied. Now we:
    1) Wait until all other threads close old version of table.
    2) Close instances of table open by this thread and replace them
       with exclusive name-locks.
    3) Rename the old table to a temp name, rename the new one to the
       old name.
    4) If we are under LOCK TABLES and don't do ALTER TABLE ... RENAME
       we reopen new version of table.
    5) Write statement to the binary log.
    6) If we are under LOCK TABLES and do ALTER TABLE ... RENAME we
       remove name-locks from list of open tables and table cache.
    7) If we are not not under LOCK TABLES we rely on close_thread_tables()
       call to remove name-locks from table cache and list of open table.
  */

  thd_proc_info(thd, "rename result table");
  my_snprintf(old_name, sizeof(old_name), "%s2-%lx-%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, old_name);

  wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_RENAME);
  close_data_files_and_morph_locks(thd, db, table_name);

  error=0;
  save_old_db_type= old_db_type;

  /*
    This leads to the storage engine (SE) not being notified for renames in
    mysql_rename_table(), because we just juggle with the FRM and nothing
    more. If we have an intermediate table, then we notify the SE that
    it should become the actual table. Later, we will recycle the old table.
    However, in case of ALTER TABLE RENAME there might be no intermediate
    table. This is when the old and new tables are compatible, according to
    compare_table(). Then, we need one additional call to
    mysql_rename_table() with flag NO_FRM_RENAME, which does nothing else but
    actual rename in the SE and the FRM is not touched. Note that, if the
    table is renamed and the SE is also changed, then an intermediate table
    is created and the additional call will not take place.
  */
  if (need_copy_table == ALTER_TABLE_METADATA_ONLY)
  {
    DBUG_ASSERT(new_db_type == old_db_type);
    /* This type cannot happen in regular ALTER. */
    new_db_type= old_db_type= NULL;
  }
  if (mysql_rename_table(old_db_type, db, table_name, db, old_name,
                         FN_TO_IS_TMP))
  {
    error=1;
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP));
  }
  else if (mysql_rename_table(new_db_type, new_db, tmp_name, new_db,
                              new_alias, FN_FROM_IS_TMP) ||
           ((new_name != table_name || new_db != db) && // we also do rename
           (need_copy_table != ALTER_TABLE_METADATA_ONLY ||
            mysql_rename_table(save_old_db_type, db, table_name, new_db,
                               new_alias, NO_FRM_RENAME)) &&
           Table_triggers_list::change_table_name(thd, db, table_name,
                                                  new_db, new_alias)))
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
    /* This shouldn't happen. But let us play it safe. */
    goto err_with_placeholders;
  }

  if (need_copy_table == ALTER_TABLE_METADATA_ONLY)
  {
    /*
      Now we have to inform handler that new .FRM file is in place.
      To do this we need to obtain a handler object for it.
      NO need to tamper with MERGE tables. The real open is done later.
    */
    TABLE *t_table;
    if (new_name != table_name || new_db != db)
    {
      table_list->alias= new_name;
      table_list->table_name= new_name;
      table_list->table_name_length= strlen(new_name);
      table_list->db= new_db;
      table_list->db_length= strlen(new_db);
      table_list->table= name_lock;
      if (reopen_name_locked_table(thd, table_list, FALSE))
        goto err_with_placeholders;
      t_table= table_list->table;
    }
    else
    {
      if (reopen_table(table))
        goto err_with_placeholders;
      t_table= table;
    }
    /* Tell the handler that a new frm file is in place. */
    if (t_table->file->ha_create_handler_files(path, NULL, CHF_INDEX_FLAG,
                                               create_info))
      goto err_with_placeholders;
    if (thd->locked_tables && new_name == table_name && new_db == db)
    {
      /*
        We are going to reopen table down on the road, so we have to restore
        state of the TABLE object which we used for obtaining of handler
        object to make it suitable for reopening.
      */
      DBUG_ASSERT(t_table == table);
      table->open_placeholder= 1;
      close_handle_and_leave_table_as_lock(table);
    }
  }

  VOID(quick_rm_table(old_db_type, db, old_name, FN_IS_TMP));

  if (thd->locked_tables && new_name == table_name && new_db == db)
  {
    thd->in_lock_tables= 1;
    error= reopen_tables(thd, 1, 1);
    thd->in_lock_tables= 0;
    if (error)
      goto err_with_placeholders;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));

  thd_proc_info(thd, "end");

  DBUG_EXECUTE_IF("sleep_alter_before_main_binlog", my_sleep(6000000););
  DEBUG_SYNC(thd, "alter_table_before_main_binlog");

  ha_binlog_log_query(thd, create_info->db_type, LOGCOM_ALTER_TABLE,
                      thd->query(), thd->query_length(),
                      db, table_name);

  DBUG_ASSERT(!(mysql_bin_log.is_open() &&
                thd->current_stmt_binlog_row_based &&
                (create_info->options & HA_LEX_CREATE_TMP_TABLE)));
  if (write_bin_log(thd, TRUE, thd->query(), thd->query_length()))
    DBUG_RETURN(TRUE);

  if (ha_check_storage_engine_flag(old_db_type, HTON_FLUSH_AFTER_RENAME))
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown. But we do not need to attach MERGE children.
    */
    char path[FN_REFLEN];
    TABLE *t_table;
    build_table_filename(path + 1, sizeof(path) - 1, new_db, table_name, "", 0);
    t_table= open_temporary_table(thd, path, new_db, tmp_name, 0);
    if (t_table)
    {
      intern_close_table(t_table);
      my_free(t_table, MYF(0));
    }
    else
      sql_print_warning("Could not open table %s.%s after rename\n",
                        new_db,table_name);
    ha_flush_logs(old_db_type);
  }
  table_list->table=0;				// For query cache
  query_cache_invalidate3(thd, table_list, 0);

  if (thd->locked_tables && (new_name != table_name || new_db != db))
  {
    /*
      If are we under LOCK TABLES and did ALTER TABLE with RENAME we need
      to remove placeholders for the old table and for the target table
      from the list of open tables and table cache. If we are not under
      LOCK TABLES we can rely on close_thread_tables() doing this job.
    */
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(thd, table, FALSE);
    unlink_open_table(thd, name_lock, FALSE);
    pthread_mutex_unlock(&LOCK_open);
  }

end_temporary:
  my_snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
	      (ulong) (copied + deleted), (ulong) deleted,
	      (ulong) thd->cuted_fields);
  my_ok(thd, copied + deleted, 0L, tmp_name);
  thd->some_tables_deleted=0;
  DBUG_RETURN(FALSE);

err1:
  if (new_table)
  {
    /* close_temporary_table() frees the new_table pointer. */
    close_temporary_table(thd, new_table, 1, 1);
  }
  else
    VOID(quick_rm_table(new_db_type, new_db, tmp_name, 
                        create_info->frm_only
                        ? FN_IS_TMP | FRM_ONLY
                        : FN_IS_TMP));

err:
  /*
    No default value was provided for a DATE/DATETIME field, the
    current sql_mode doesn't allow the '0000-00-00' value and
    the table to be altered isn't empty.
    Report error here.
  */
  if (alter_info->error_if_not_empty && thd->row_count)
  {
    const char *f_val= 0;
    enum enum_mysql_timestamp_type t_type= MYSQL_TIMESTAMP_DATE;
    switch (alter_info->datetime_field->sql_type)
    {
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
        f_val= "0000-00-00";
        t_type= MYSQL_TIMESTAMP_DATE;
        break;
      case MYSQL_TYPE_DATETIME:
        f_val= "0000-00-00 00:00:00";
        t_type= MYSQL_TIMESTAMP_DATETIME;
        break;
      default:
        /* Shouldn't get here. */
        DBUG_ASSERT(0);
    }
    bool save_abort_on_warning= thd->abort_on_warning;
    thd->abort_on_warning= TRUE;
    make_truncated_value_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                                 f_val, strlength(f_val), t_type,
                                 alter_info->datetime_field->field_name);
    thd->abort_on_warning= save_abort_on_warning;
  }
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(thd, name_lock, FALSE);
    pthread_mutex_unlock(&LOCK_open);
  }
  DBUG_RETURN(TRUE);

err_with_placeholders:
  /*
    An error happened while we were holding exclusive name-lock on table
    being altered. To be safe under LOCK TABLES we should remove placeholders
    from list of open tables list and table cache.
  */
  unlink_open_table(thd, table, FALSE);
  if (name_lock)
    unlink_open_table(thd, name_lock, FALSE);
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(TRUE);
}
/* mysql_alter_table */

static int
copy_data_between_tables(TABLE *from,TABLE *to,
			 List<Create_field> &create,
                         bool ignore,
			 uint order_num, ORDER *order,
			 ha_rows *copied,
			 ha_rows *deleted,
                         enum enum_enable_or_disable keys_onoff,
                         bool error_if_not_empty)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  THD *thd= current_thd;
  uint length= 0;
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

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(to, from->file->indexes_are_disabled(), keys_onoff);

  /* We can abort alter table for any table type */
  thd->abort_on_warning= !ignore && test(thd->variables.sql_mode &
                                         (MODE_STRICT_TRANS_TABLES |
                                          MODE_STRICT_ALL_TABLES));

  from->file->info(HA_STATUS_VARIABLE);
  to->file->ha_start_bulk_insert(from->file->stats.records);

  save_sql_mode= thd->variables.sql_mode;

  List_iterator<Create_field> it(create);
  Create_field *def;
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
    if (to->s->primary_key != MAX_KEY && to->file->primary_key_is_clustered())
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      my_snprintf(warn_buff, sizeof(warn_buff), 
                  "ORDER BY ignored as there is a user-defined clustered index"
                  " in the table '%-.192s'", from->s->table_name.str);
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                   warn_buff);
    }
    else
    {
      from->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
                                                MYF(MY_FAE | MY_ZEROFILL));
      bzero((char *) &tables, sizeof(tables));
      tables.table= from;
      tables.alias= tables.table_name= from->s->table_name.str;
      tables.db= from->s->db.str;
      error= 1;

      if (thd->lex->select_lex.setup_ref_array(thd, order_num) ||
          setup_order(thd, thd->lex->select_lex.ref_pointer_array,
                      &tables, fields, all_fields, order) ||
          !(sortorder= make_unireg_sortorder(order, &length, NULL)) ||
          (from->sort.found_records= filesort(thd, from, sortorder, length,
                                              (SQL_SELECT *) 0, HA_POS_ERROR,
                                              1, &examined_rows)) ==
          HA_POS_ERROR)
        goto err;
    }
  };

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();
  init_read_record(&info, thd, from, (SQL_SELECT *) 0, 1, 1, FALSE);
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
    /* Return error if source table isn't empty. */
    if (error_if_not_empty)
    {
      error= 1;
      break;
    }
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
    error=to->file->ha_write_row(to->record[0]);
    to->auto_increment_field_not_null= FALSE;
    if (error)
    {
      if (!ignore ||
          to->file->is_fatal_error(error, HA_CHECK_DUP))
      {
         if (!to->file->is_fatal_error(error, HA_CHECK_DUP))
         {
           uint key_nr= to->file->get_dup_key(error);
           if ((int) key_nr >= 0)
           {
             const char *err_msg= ER(ER_DUP_ENTRY_WITH_KEY_NAME);
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

  if (ha_enable_transaction(thd, TRUE))
  {
    error= 1;
    goto err;
  }
  
  /*
    Ensure that the new table is saved properly to disk so that we
    can do a rename
  */
  if (ha_autocommit_or_rollback(thd, 0))
    error=1;
  if (end_active_trans(thd))
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

 RETURN
    Like mysql_alter_table().
*/
bool mysql_recreate_table(THD *thd, TABLE_LIST *table_list)
{
  HA_CREATE_INFO create_info;
  Alter_info alter_info;

  DBUG_ENTER("mysql_recreate_table");
  DBUG_ASSERT(!table_list->next_global);
  /*
    table_list->table has been closed and freed. Do not reference
    uninitialized data. open_tables() could fail.
  */
  table_list->table= NULL;

  bzero((char*) &create_info, sizeof(create_info));
  create_info.row_type=ROW_TYPE_NOT_USED;
  create_info.default_table_charset=default_charset_info;
  /* Force alter table to recreate table */
  alter_info.flags= (ALTER_CHANGE_COLUMN | ALTER_RECREATE);
  DBUG_RETURN(mysql_alter_table(thd, NullS, NullS, &create_info,
                                table_list, &alter_info, 0,
                                (ORDER *) 0, 0));
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
  field_list.push_back(item= new Item_int("Checksum", (longlong) 1,
                                          MY_INT64_NUM_DECIMAL_DIGITS));
  item->maybe_null= 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  /* Open one table after the other to keep lock time as short as possible. */
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    TABLE *t;

    strxmov(table_name, table->db ,".", table->table_name, NullS);

    t= table->table= open_n_lock_single_table(thd, table, TL_READ);
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
            if (thd->killed)
            {
              /* 
                 we've been killed; let handler clean up, and remove the 
                 partial current row from the recordset (embedded lib) 
              */
              t->file->ha_rnd_end();
              thd->protocol->remove_last_row();
              goto err;
            }
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

             /*
               BLOB and VARCHAR have pointers in their field, we must convert
               to string; GEOMETRY is implemented on top of BLOB.
               BIT may store its data among NULL bits, convert as well.
             */
              switch (f->type()) {
                case MYSQL_TYPE_BLOB:
                case MYSQL_TYPE_VARCHAR:
                case MYSQL_TYPE_GEOMETRY:
                case MYSQL_TYPE_BIT:
                {
                  String tmp;
                  f->val_str(&tmp);
                  row_crc= my_checksum(row_crc, (uchar*) tmp.ptr(),
                           tmp.length());
                  break;
                }
                default:
                  row_crc= my_checksum(row_crc, f->ptr, f->pack_length());
                  break;
	      }
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

  my_eof(thd);
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
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
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
               ha_resolve_storage_engine_name(*new_engine), "TEMPORARY");
      *new_engine= 0;
      return TRUE;
    }
    *new_engine= myisam_hton;
  }
  return FALSE;
}
