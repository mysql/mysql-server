/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_table.h"

#include "auth_common.h"        // check_fk_parent_table_access
#include "unireg.h"
#include "debug_sync.h"
#include "sql_rename.h" // do_rename
#include "sql_parse.h"                        // test_if_data_home_dir
#include "sql_cache.h"                          // query_cache_*
#include "sql_base.h"   // open_table_uncached, lock_table_names
#include "lock.h"       // mysql_unlock_tables
#include "strfunc.h"    // find_type2, find_set
#include "sql_view.h" // view_checksum 
#include "sql_truncate.h"                       // regenerate_locked_table 
#include "sql_partition.h"                      // mem_alloc_error,
                                                // generate_partition_syntax,
                                                // NOT_A_PARTITION_ID
#include "partition_info.h"                     // partition_info
#include "sql_db.h"                             // load_db_opt_by_name
#include "sql_time.h"                  // make_truncated_value_warning
#include "records.h"             // init_read_record, end_read_record
#include "filesort.h"            // filesort_free_buffers
#include "sql_select.h"                // setup_order,
                                       // make_unireg_sortorder
#include "sql_handler.h"               // mysql_ha_rm_tables
#include "discover.h"                  // readfrm
#include "log_event.h"                 // Query_log_event
#include <hash.h>
#include <myisam.h>
#include <my_dir.h>
#include "sp_head.h"
#include "sp.h"
#include "sql_parse.h"
#include "sql_show.h"
#include "transaction.h"
#include "datadict.h"  // dd_frm_type()
#include "sql_resolver.h"              // setup_order
#include "table_cache.h"
#include "sql_trigger.h"               // change_trigger_table_name
#include <mysql/psi/mysql_table.h>
#include "partitioning/partition_handler.h" // Partition_handler
#include "log.h"
#include "binlog.h"
#include "sql_tablespace.h"            // check_tablespace_name())
#include "item_timefunc.h"             // Item_func_now_local

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <algorithm>
using std::max;
using std::min;
using binary_log::checksum_crc32;

#define ER_THD_OR_DEFAULT(thd,X) ((thd) ? ER_THD(thd, X) : ER_DEFAULT(X))

const char *primary_key_name="PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(PSI_stage_progress *psi,
                                    TABLE *from,TABLE *to,
                                    List<Create_field> &create,
				    ha_rows *copied,ha_rows *deleted,
                                    Alter_info::enum_enable_or_disable keys_onoff,
                                    Alter_table_ctx *alter_ctx);

static bool prepare_blob_field(THD *thd, Create_field *sql_field);
static void sp_prepare_create_field(THD *thd, Create_field *sql_field);
static bool check_engine(THD *thd, const char *db_name,
                         const char *table_name,
                         HA_CREATE_INFO *create_info);

static int
mysql_prepare_create_table(THD *thd, const char *error_schema_name,
                           const char *error_table_name,
                           HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                           uint *db_options,
                           handler *file, KEY **key_info_buffer,
                           uint *key_count, int select_field_count);
static uint blob_length_by_type(enum_field_types type);


/**
  @brief Helper function for explain_filename
  @param thd          Thread handle
  @param to_p         Explained name in system_charset_info
  @param end_p        End of the to_p buffer
  @param name         Name to be converted
  @param name_len     Length of the name, in bytes
*/
static char* add_identifier(THD* thd, char *to_p, const char * end_p,
                            const char* name, size_t name_len)
{
  size_t res;
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
    my_stpnmov(tmp_name, name, name_len);
    tmp_name[name_len]= 0;
    conv_name= tmp_name;
  }
  res= strconvert(&my_charset_filename, conv_name, system_charset_info,
                  conv_string, FN_REFLEN, &errors);
  if (!res || errors)
  {
    DBUG_PRINT("error", ("strconvert of '%s' failed with %u (errors: %u)", conv_name,
                         static_cast<uint>(res), errors));
    conv_name= name;
  }
  else
  {
    DBUG_PRINT("info", ("conv '%s' -> '%s'", conv_name, conv_string));
    conv_name= conv_string;
  }

  quote = thd ? get_quote_char_for_identifier(thd, conv_name, res - 1) : '`';

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
        to_p= my_stpnmov(to_p, conv_name, length);
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
    to_p= my_stpnmov(to_p, conv_name, end_p - to_p);
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

size_t explain_filename(THD* thd,
                        const char *from,
                        char *to,
                        size_t to_length,
                        enum_explain_filename_mode explain_mode)
{
  char *to_p= to;
  char *end_p= to_p + to_length;
  const char *db_name= NULL;
  size_t  db_name_len= 0;
  const char *table_name;
  size_t  table_name_len= 0;
  const char *part_name= NULL;
  size_t  part_name_len= 0;
  const char *subpart_name= NULL;
  size_t  subpart_name_len= 0;
  enum enum_part_name_type {NORMAL, TEMP, RENAMED} part_type= NORMAL;

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
  /* Look if there are partition tokens in the table name. */
  while ((tmp_p= strchr(tmp_p, '#')))
  {
    tmp_p++;
    switch (tmp_p[0]) {
    case 'P':
    case 'p':
      if (tmp_p[1] == '#')
      {
        part_name= tmp_p + 2;
        tmp_p+= 2;
      }
      break;
    case 'S':
    case 's':
      if ((tmp_p[1] == 'P' || tmp_p[1] == 'p') && tmp_p[2] == '#')
      {
        part_name_len= tmp_p - part_name - 1;
        subpart_name= tmp_p + 3;
        tmp_p+= 3;
      }
      break;
    case 'T':
    case 't':
      if ((tmp_p[1] == 'M' || tmp_p[1] == 'm') &&
          (tmp_p[2] == 'P' || tmp_p[2] == 'p') &&
          tmp_p[3] == '#' && !tmp_p[4])
      {
        part_type= TEMP;
        tmp_p+= 4;
      }
      break;
    case 'R':
    case 'r':
      if ((tmp_p[1] == 'E' || tmp_p[1] == 'e') &&
          (tmp_p[2] == 'N' || tmp_p[2] == 'n') &&
          tmp_p[3] == '#' && !tmp_p[4])
      {
        part_type= RENAMED;
        tmp_p+= 4;
      }
      break;
    default:
      /* Not partition name part. */
      ;
    }
  }
  if (part_name)
  {
    table_name_len= part_name - table_name - 3;
    if (subpart_name)
      subpart_name_len= strlen(subpart_name);
    else
      part_name_len= strlen(part_name);
    if (part_type != NORMAL)
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
      to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_DATABASE_NAME),
                                            end_p - to_p);
      *(to_p++)= ' ';
      to_p= add_identifier(thd, to_p, end_p, db_name, db_name_len);
      to_p= my_stpncpy(to_p, ", ", end_p - to_p);
    }
    else
    {
      to_p= add_identifier(thd, to_p, end_p, db_name, db_name_len);
      to_p= my_stpncpy(to_p, ".", end_p - to_p);
    }
  }
  if (explain_mode == EXPLAIN_ALL_VERBOSE)
  {
    to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_TABLE_NAME), end_p - to_p);
    *(to_p++)= ' ';
    to_p= add_identifier(thd, to_p, end_p, table_name, table_name_len);
  }
  else
    to_p= add_identifier(thd, to_p, end_p, table_name, table_name_len);
  if (part_name)
  {
    if (explain_mode == EXPLAIN_PARTITIONS_AS_COMMENT)
      to_p= my_stpncpy(to_p, " /* ", end_p - to_p);
    else if (explain_mode == EXPLAIN_PARTITIONS_VERBOSE)
      to_p= my_stpncpy(to_p, " ", end_p - to_p);
    else
      to_p= my_stpncpy(to_p, ", ", end_p - to_p);
    if (part_type != NORMAL)
    {
      if (part_type == TEMP)
        to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_TEMPORARY_NAME),
                      end_p - to_p);
      else
        to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_RENAMED_NAME),
                      end_p - to_p);
      to_p= my_stpncpy(to_p, " ", end_p - to_p);
    }
    to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_PARTITION_NAME),
                  end_p - to_p);
    *(to_p++)= ' ';
    to_p= add_identifier(thd, to_p, end_p, part_name, part_name_len);
    if (subpart_name)
    {
      to_p= my_stpncpy(to_p, ", ", end_p - to_p);
      to_p= my_stpncpy(to_p, ER_THD_OR_DEFAULT(thd, ER_SUBPARTITION_NAME),
                    end_p - to_p);
      *(to_p++)= ' ';
      to_p= add_identifier(thd, to_p, end_p, subpart_name, subpart_name_len);
    }
    if (explain_mode == EXPLAIN_PARTITIONS_AS_COMMENT)
      to_p= my_stpncpy(to_p, " */", end_p - to_p);
  }
  DBUG_PRINT("exit", ("to '%s'", to));
  DBUG_RETURN(static_cast<size_t>(to_p - to));
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

size_t filename_to_tablename(const char *from, char *to, size_t to_length
#ifndef DBUG_OFF
                           , bool stay_quiet
#endif /* DBUG_OFF */
                           )
{
  uint errors;
  size_t res;
  DBUG_ENTER("filename_to_tablename");
  DBUG_PRINT("enter", ("from '%s'", from));

  if (strlen(from) >= tmp_file_prefix_length &&
      !memcmp(from, tmp_file_prefix, tmp_file_prefix_length))
  {
    /* Temporary table name. */
    res= (my_stpnmov(to, from, to_length) - to);
  }
  else
  {
    res= strconvert(&my_charset_filename, from,
                    system_charset_info,  to, to_length, &errors);
    if (errors) // Old 5.0 name
    {
      res= (strxnmov(to, to_length, MYSQL50_TABLE_NAME_PREFIX,  from, NullS) -
            to);
#ifndef DBUG_OFF
      if (!stay_quiet) {
#endif /* DBUG_OFF */
        sql_print_error("Invalid (old?) table or database name '%s'", from);
#ifndef DBUG_OFF
      }
#endif /* DBUG_OFF */
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

size_t check_n_cut_mysql50_prefix(const char *from, char *to, size_t to_length)
{
  if (check_mysql50_prefix(from))
    return static_cast<size_t>(strmake(to, from + MYSQL50_TABLE_NAME_PREFIX_LENGTH,
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

size_t tablename_to_filename(const char *from, char *to, size_t to_length)
{
  uint errors;
  size_t length;
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
    if (check_table_name(to, length, TRUE) != IDENT_NAME_OK)
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
  @brief Creates path to a file: mysql_data_dir/db/table.ext

  @param buff                   Where to write result in my_charset_filename.
                                This may be the same as table_name.
  @param bufflen                buff size
  @param db                     Database name in system_charset_info.
  @param table_name             Table name in system_charset_info.
  @param ext                    File extension.
  @param flags                  FN_FROM_IS_TMP or FN_TO_IS_TMP or FN_IS_TMP
                                table_name is temporary, do not change.
  @param was_truncated          points to location that will be
                                set to true if path was truncated,
                                to false otherwise.

  @note
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

  @return
    path length
*/

size_t build_table_filename(char *buff, size_t bufflen, const char *db,
                            const char *table_name, const char *ext,
                            uint flags, bool *was_truncated)
{
  char tbbuff[FN_REFLEN], dbbuff[FN_REFLEN];
  size_t tab_len, db_len;
  DBUG_ENTER("build_table_filename");
  DBUG_PRINT("enter", ("db: '%s'  table_name: '%s'  ext: '%s'  flags: %x",
                       db, table_name, ext, flags));

  if (flags & FN_IS_TMP) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    tab_len= my_stpnmov(tbbuff, table_name, sizeof(tbbuff)) - tbbuff;
  else
    tab_len= tablename_to_filename(table_name, tbbuff, sizeof(tbbuff));

  db_len= tablename_to_filename(db, dbbuff, sizeof(dbbuff));

  char *end = buff + bufflen;
  /* Don't add FN_ROOTDIR if mysql_data_home already includes it */
  char *pos = my_stpnmov(buff, mysql_data_home, bufflen);
  size_t rootdir_len= strlen(FN_ROOTDIR);
  if (pos - rootdir_len >= buff &&
      memcmp(pos - rootdir_len, FN_ROOTDIR, rootdir_len) != 0)
    pos= my_stpnmov(pos, FN_ROOTDIR, end - pos);
  else
      rootdir_len= 0;
  pos= strxnmov(pos, end - pos, dbbuff, FN_ROOTDIR, NullS);
  pos= strxnmov(pos, end - pos, tbbuff, ext, NullS);

  /**
    Mark OUT param if path gets truncated.
    Most of functions which invoke this function are sure that the
    path will not be truncated. In case some functions are not sure,
    we can use 'was_truncated' OUTPARAM
  */
  *was_truncated= false;
  if (pos == end &&
      (bufflen < mysql_data_home_len + rootdir_len + db_len +
                 strlen(FN_ROOTDIR) + tab_len + strlen(ext)))
    *was_truncated= true;

  DBUG_PRINT("exit", ("buff: '%s'", buff));
  DBUG_RETURN(pos - buff);
}


/**
  Create path to a temporary table mysql_tmpdir/#sql1234_12_1
  (i.e. to its .FRM file but without an extension).

  @param thd      The thread handle.
  @param buff     Where to write result in my_charset_filename.
  @param bufflen  buff size

  @note
    Uses current_pid, thread_id, and tmp_table counter to create
    a file name in mysql_tmpdir.

  @return Path length.
*/

size_t build_tmptable_filename(THD* thd, char *buff, size_t bufflen)
{
  DBUG_ENTER("build_tmptable_filename");

  char *p= my_stpnmov(buff, mysql_tmpdir, bufflen);
  DBUG_ASSERT(sizeof(my_thread_id) == 4);
  my_snprintf(p, bufflen - (p - buff), "/%s%lx_%lx_%x",
              tmp_file_prefix, current_pid,
              thd->thread_id(), thd->tmp_table++);

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
  bool do_release;
  bool recovery_phase;
  st_global_ddl_log() : inited(false), do_release(false) {}
};

st_global_ddl_log global_ddl_log;

mysql_mutex_t LOCK_gdl;

#define DDL_LOG_ENTRY_TYPE_POS 0
#define DDL_LOG_ACTION_TYPE_POS 1
#define DDL_LOG_PHASE_POS 2
#define DDL_LOG_NEXT_ENTRY_POS 4
#define DDL_LOG_NAME_POS 8

#define DDL_LOG_NUM_ENTRY_POS 0
#define DDL_LOG_NAME_LEN_POS 4
#define DDL_LOG_IO_SIZE_POS 8

/**
  Read one entry from ddl log file.

  @param entry_no                     Entry number to read

  @return Operation status
    @retval true   Error
    @retval false  Success
*/

static bool read_ddl_log_file_entry(uint entry_no)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  uchar *file_entry_buf= (uchar*)global_ddl_log.file_entry_buf;
  uint io_size= global_ddl_log.io_size;
  DBUG_ENTER("read_ddl_log_file_entry");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (mysql_file_pread(file_id, file_entry_buf, io_size, io_size * entry_no,
                       MYF(MY_WME)) != io_size)
    error= TRUE;
  DBUG_RETURN(error);
}


/**
  Write one entry to ddl log file.

  @param entry_no                     Entry number to write

  @return Operation status
    @retval true   Error
    @retval false  Success
*/

static bool write_ddl_log_file_entry(uint entry_no)
{
  bool error= FALSE;
  File file_id= global_ddl_log.file_id;
  uchar *file_entry_buf= (uchar*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("write_ddl_log_file_entry");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (mysql_file_pwrite(file_id, file_entry_buf,
                        IO_SIZE, IO_SIZE * entry_no, MYF(MY_WME)) != IO_SIZE)
    error= TRUE;
  DBUG_RETURN(error);
}


/**
  Sync the ddl log file.

  @return Operation status
    @retval FALSE  Success
    @retval TRUE   Error
*/

static bool sync_ddl_log_file()
{
  DBUG_ENTER("sync_ddl_log_file");
  DBUG_RETURN(mysql_file_sync(global_ddl_log.file_id, MYF(MY_WME)));
}


/**
  Write ddl log header.

  @return Operation status
    @retval TRUE                      Error
    @retval FALSE                     Success
*/

static bool write_ddl_log_header()
{
  uint16 const_var;
  DBUG_ENTER("write_ddl_log_header");

  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NUM_ENTRY_POS],
            global_ddl_log.num_entries);
  const_var= FN_REFLEN;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_LEN_POS],
            (ulong) const_var);
  const_var= IO_SIZE;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_IO_SIZE_POS],
            (ulong) const_var);
  if (write_ddl_log_file_entry(0UL))
  {
    sql_print_error("Error writing ddl log header");
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(sync_ddl_log_file());
}


/**
  Create ddl log file name.
  @param file_name                   Filename setup
*/

static inline void create_ddl_log_file_name(char *file_name)
{
  strxmov(file_name, mysql_data_home, "/", "ddl_log.log", NullS);
}


/**
  Read header of ddl log file.

  When we read the ddl log header we get information about maximum sizes
  of names in the ddl log and we also get information about the number
  of entries in the ddl log.

  @return Last entry in ddl log (0 if no entries)
*/

static uint read_ddl_log_header()
{
  uchar *file_entry_buf= (uchar*)global_ddl_log.file_entry_buf;
  char file_name[FN_REFLEN];
  uint entry_no;
  bool successful_open= FALSE;
  DBUG_ENTER("read_ddl_log_header");

  mysql_mutex_init(key_LOCK_gdl, &LOCK_gdl, MY_MUTEX_INIT_SLOW);
  mysql_mutex_lock(&LOCK_gdl);
  create_ddl_log_file_name(file_name);
  if ((global_ddl_log.file_id= mysql_file_open(key_file_global_ddl_log,
                                               file_name,
                                               O_RDWR | O_BINARY, MYF(0))) >= 0)
  {
    if (read_ddl_log_file_entry(0UL))
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
  global_ddl_log.do_release= true;
  mysql_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(entry_no);
}


/**
  Convert from ddl_log_entry struct to file_entry_buf binary blob.

  @param ddl_log_entry   filled in ddl_log_entry struct.
*/

static void set_global_from_ddl_log_entry(const DDL_LOG_ENTRY *ddl_log_entry)
{
  mysql_mutex_assert_owner(&LOCK_gdl);
  global_ddl_log.file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]=
                                    (char)DDL_LOG_ENTRY_CODE;
  global_ddl_log.file_entry_buf[DDL_LOG_ACTION_TYPE_POS]=
                                    (char)ddl_log_entry->action_type;
  global_ddl_log.file_entry_buf[DDL_LOG_PHASE_POS]= 0;
  int4store(&global_ddl_log.file_entry_buf[DDL_LOG_NEXT_ENTRY_POS],
            ddl_log_entry->next_entry);
  DBUG_ASSERT(strlen(ddl_log_entry->name) < FN_REFLEN);
  strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS],
          ddl_log_entry->name, FN_REFLEN - 1);
  if (ddl_log_entry->action_type == DDL_LOG_RENAME_ACTION ||
      ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION ||
      ddl_log_entry->action_type == DDL_LOG_EXCHANGE_ACTION)
  {
    DBUG_ASSERT(strlen(ddl_log_entry->from_name) < FN_REFLEN);
    strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + FN_REFLEN],
          ddl_log_entry->from_name, FN_REFLEN - 1);
  }
  else
    global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + FN_REFLEN]= 0;
  DBUG_ASSERT(strlen(ddl_log_entry->handler_name) < FN_REFLEN);
  strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + (2*FN_REFLEN)],
          ddl_log_entry->handler_name, FN_REFLEN - 1);
  if (ddl_log_entry->action_type == DDL_LOG_EXCHANGE_ACTION)
  {
    DBUG_ASSERT(strlen(ddl_log_entry->tmp_name) < FN_REFLEN);
    strmake(&global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + (3*FN_REFLEN)],
          ddl_log_entry->tmp_name, FN_REFLEN - 1);
  }
  else
    global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS + (3*FN_REFLEN)]= 0;
}


/**
  Convert from file_entry_buf binary blob to ddl_log_entry struct.

  @param[out] ddl_log_entry   struct to fill in.

  @note Strings (names) are pointing to the global_ddl_log structure,
  so LOCK_gdl needs to be hold until they are read or copied.
*/

static void set_ddl_log_entry_from_global(DDL_LOG_ENTRY *ddl_log_entry,
                                          const uint read_entry)
{
  char *file_entry_buf= (char*) global_ddl_log.file_entry_buf;
  uint inx;
  uchar single_char;

  mysql_mutex_assert_owner(&LOCK_gdl);
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
  if (ddl_log_entry->action_type == DDL_LOG_EXCHANGE_ACTION)
  {
    inx+= global_ddl_log.name_len;
    ddl_log_entry->tmp_name= &file_entry_buf[inx];
  }
  else
    ddl_log_entry->tmp_name= NULL;
}


/**
  Read a ddl log entry.

  Read a specified entry in the ddl log.

  @param read_entry               Number of entry to read
  @param[out] entry_info          Information from entry

  @return Operation status
    @retval TRUE                     Error
    @retval FALSE                    Success
*/

static bool read_ddl_log_entry(uint read_entry, DDL_LOG_ENTRY *ddl_log_entry)
{
  DBUG_ENTER("read_ddl_log_entry");

  if (read_ddl_log_file_entry(read_entry))
  {
    DBUG_RETURN(TRUE);
  }
  set_ddl_log_entry_from_global(ddl_log_entry, read_entry);
  DBUG_RETURN(FALSE);
}


/**
  Initialise ddl log.

  Write the header of the ddl log file and length of names. Also set
  number of entries to zero.

  @return Operation status
    @retval TRUE                     Error
    @retval FALSE                    Success
*/

static bool init_ddl_log()
{
  char file_name[FN_REFLEN];
  DBUG_ENTER("init_ddl_log");

  if (global_ddl_log.inited)
    goto end;

  global_ddl_log.io_size= IO_SIZE;
  global_ddl_log.name_len= FN_REFLEN;
  create_ddl_log_file_name(file_name);
  if ((global_ddl_log.file_id= mysql_file_create(key_file_global_ddl_log,
                                                 file_name, CREATE_MODE,
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
    (void) mysql_file_close(global_ddl_log.file_id, MYF(MY_WME));
    global_ddl_log.inited= FALSE;
    DBUG_RETURN(TRUE);
  }

end:
  DBUG_RETURN(FALSE);
}


/**
  Sync ddl log file.

  @return Operation status
    @retval TRUE        Error
    @retval FALSE       Success
*/

static bool sync_ddl_log_no_lock()
{
  DBUG_ENTER("sync_ddl_log_no_lock");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if ((!global_ddl_log.recovery_phase) &&
      init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(sync_ddl_log_file());
}


/**
  @brief Deactivate an individual entry.

  @details For complex rename operations we need to deactivate individual
  entries.

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

  @param entry_no     Entry position of record to change

  @return Operation status
    @retval TRUE      Error
    @retval FALSE     Success
*/

static bool deactivate_ddl_log_entry_no_lock(uint entry_no)
{
  uchar *file_entry_buf= (uchar*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("deactivate_ddl_log_entry_no_lock");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (!read_ddl_log_file_entry(entry_no))
  {
    if (file_entry_buf[DDL_LOG_ENTRY_TYPE_POS] == DDL_LOG_ENTRY_CODE)
    {
      /*
        Log entry, if complete mark it done (IGNORE).
        Otherwise increase the phase by one.
      */
      if (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_DELETE_ACTION ||
          file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_RENAME_ACTION ||
          (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_REPLACE_ACTION &&
           file_entry_buf[DDL_LOG_PHASE_POS] == 1) ||
          (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_EXCHANGE_ACTION &&
           file_entry_buf[DDL_LOG_PHASE_POS] >= EXCH_PHASE_TEMP_TO_FROM))
        file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= DDL_IGNORE_LOG_ENTRY_CODE;
      else if (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_REPLACE_ACTION)
      {
        DBUG_ASSERT(file_entry_buf[DDL_LOG_PHASE_POS] == 0);
        file_entry_buf[DDL_LOG_PHASE_POS]= 1;
      }
      else if (file_entry_buf[DDL_LOG_ACTION_TYPE_POS] == DDL_LOG_EXCHANGE_ACTION)
      {
        DBUG_ASSERT(file_entry_buf[DDL_LOG_PHASE_POS] <=
                                                 EXCH_PHASE_FROM_TO_NAME);
        file_entry_buf[DDL_LOG_PHASE_POS]++;
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


/**
  Execute one action in a ddl log entry

  @param ddl_log_entry              Information in action entry to execute

  @return Operation status
    @retval TRUE                       Error
    @retval FALSE                      Success
*/

static bool execute_ddl_log_action(THD *thd, DDL_LOG_ENTRY *ddl_log_entry)
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

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (ddl_log_entry->entry_type == DDL_IGNORE_LOG_ENTRY_CODE)
  {
    DBUG_RETURN(FALSE);
  }
  DBUG_PRINT("ddl_log",
             ("execute type %c next %u name '%s' from_name '%s' handler '%s'"
              " tmp_name '%s'",
             ddl_log_entry->action_type,
             ddl_log_entry->next_entry,
             ddl_log_entry->name,
             ddl_log_entry->from_name,
             ddl_log_entry->handler_name,
             ddl_log_entry->tmp_name));
  handler_name.str= (char*)ddl_log_entry->handler_name;
  handler_name.length= strlen(ddl_log_entry->handler_name);
  init_sql_alloc(key_memory_gdl, &mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (!strcmp(ddl_log_entry->handler_name, reg_ext))
    frm_action= TRUE;
  else
  {
    plugin_ref plugin= ha_resolve_by_name(thd, &handler_name, FALSE);
    if (!plugin)
    {
      my_error(ER_ILLEGAL_HA, MYF(0), ddl_log_entry->handler_name);
      goto error;
    }
    hton= plugin_data<handlerton*>(plugin);
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
          if ((error= mysql_file_delete(key_file_frm, to_path, MYF(MY_WME))))
          {
            if (my_errno() != ENOENT)
              break;
          }
          DBUG_ASSERT(strcmp("partition", ddl_log_entry->handler_name));
          strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
          if (access(to_path, F_OK) == 0)
          {
            (void) mysql_file_delete(key_file_partition_ddl_log,
                                     to_path,
                                     MYF(MY_WME));
          }
        }
        else
        {
          if ((error= file->ha_delete_table(ddl_log_entry->name)))
          {
            if (error != ENOENT && error != HA_ERR_NO_SUCH_TABLE)
              break;
          }
        }
        if ((deactivate_ddl_log_entry_no_lock(ddl_log_entry->entry_pos)))
          break;
        (void) sync_ddl_log_no_lock();
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
    // Fall through
    case DDL_LOG_RENAME_ACTION:
    {
      error= TRUE;
      if (frm_action)
      {
        strxmov(to_path, ddl_log_entry->name, reg_ext, NullS);
        strxmov(from_path, ddl_log_entry->from_name, reg_ext, NullS);
        if (mysql_file_rename(key_file_frm, from_path, to_path, MYF(MY_WME)))
          break;
        DBUG_ASSERT(strcmp("partition", ddl_log_entry->handler_name));
        strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
        strxmov(from_path, ddl_log_entry->from_name, par_ext, NullS);
        if (access(from_path, F_OK) == 0)
        {
          (void) mysql_file_rename(key_file_partition_ddl_log,
                                   from_path,
                                   to_path,
                                   MYF(MY_WME));
        }
      }
      else
      {
        if (file->ha_rename_table(ddl_log_entry->from_name,
                                  ddl_log_entry->name))
          break;
      }
      if ((deactivate_ddl_log_entry_no_lock(ddl_log_entry->entry_pos)))
        break;
      (void) sync_ddl_log_no_lock();
      error= FALSE;
      break;
    }
    case DDL_LOG_EXCHANGE_ACTION:
    {
      /* We hold LOCK_gdl, so we can alter global_ddl_log.file_entry_buf */
      char *file_entry_buf= (char*)&global_ddl_log.file_entry_buf;
      /* not yet implemented for frm */
      DBUG_ASSERT(!frm_action);
      /*
        Using a case-switch here to revert all currently done phases,
        since it will fall through until the first phase is undone.
      */
      switch (ddl_log_entry->phase) {
        case EXCH_PHASE_TEMP_TO_FROM:
          /* tmp_name -> from_name possibly done */
          (void) file->ha_rename_table(ddl_log_entry->from_name,
                                       ddl_log_entry->tmp_name);
          /* decrease the phase and sync */
          file_entry_buf[DDL_LOG_PHASE_POS]--;
          if (write_ddl_log_file_entry(ddl_log_entry->entry_pos))
            break;
          if (sync_ddl_log_no_lock())
            break;
          /* fall through */
        case EXCH_PHASE_FROM_TO_NAME:
          /* from_name -> name possibly done */
          (void) file->ha_rename_table(ddl_log_entry->name,
                                       ddl_log_entry->from_name);
          /* decrease the phase and sync */
          file_entry_buf[DDL_LOG_PHASE_POS]--;
          if (write_ddl_log_file_entry(ddl_log_entry->entry_pos))
            break;
          if (sync_ddl_log_no_lock())
            break;
          /* fall through */
        case EXCH_PHASE_NAME_TO_TEMP:
          /* name -> tmp_name possibly done */
          (void) file->ha_rename_table(ddl_log_entry->tmp_name,
                                       ddl_log_entry->name);
          /* disable the entry and sync */
          file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= DDL_IGNORE_LOG_ENTRY_CODE;
          if (write_ddl_log_file_entry(ddl_log_entry->entry_pos))
            break;
          if (sync_ddl_log_no_lock())
            break;
          error= FALSE;
          break;
        default:
          DBUG_ASSERT(0);
          break;
      }

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


/**
  Get a free entry in the ddl log

  @param[out] active_entry     A ddl log memory entry returned

  @return Operation status
    @retval TRUE               Error
    @retval FALSE              Success
*/

static bool get_free_ddl_log_entry(DDL_LOG_MEMORY_ENTRY **active_entry,
                                   bool *write_header)
{
  DDL_LOG_MEMORY_ENTRY *used_entry;
  DDL_LOG_MEMORY_ENTRY *first_used= global_ddl_log.first_used;
  DBUG_ENTER("get_free_ddl_log_entry");

  if (global_ddl_log.first_free == NULL)
  {
    if (!(used_entry= (DDL_LOG_MEMORY_ENTRY*)my_malloc(key_memory_DDL_LOG_MEMORY_ENTRY,
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
  used_entry->next_active_log_entry= NULL;
  global_ddl_log.first_used= used_entry;
  if (first_used)
    first_used->prev_log_entry= used_entry;

  *active_entry= used_entry;
  DBUG_RETURN(FALSE);
}


/**
  Execute one entry in the ddl log.
  
  Executing an entry means executing a linked list of actions.

  @param first_entry           Reference to first action in entry

  @return Operation status
    @retval TRUE               Error
    @retval FALSE              Success
*/

static bool execute_ddl_log_entry_no_lock(THD *thd, uint first_entry)
{
  DDL_LOG_ENTRY ddl_log_entry;
  uint read_entry= first_entry;
  bool error;
  DBUG_ENTER("execute_ddl_log_entry_no_lock");

  mysql_mutex_assert_owner(&LOCK_gdl);
  do
  {
    if (read_ddl_log_entry(read_entry, &ddl_log_entry))
    {
      /* Write to error log and continue with next log entry */
      sql_print_error("Failed to read entry = %u from ddl log",
                      read_entry);
      error= true;
      break;
    }
    DBUG_ASSERT(ddl_log_entry.entry_type == DDL_LOG_ENTRY_CODE ||
                ddl_log_entry.entry_type == DDL_IGNORE_LOG_ENTRY_CODE);

    if ((error= execute_ddl_log_action(thd, &ddl_log_entry)))
    {
      /* Write to error log and continue with next log entry */
      sql_print_error("Failed to execute action for entry = %u from ddl log",
                      read_entry);
      break;
    }
    read_entry= ddl_log_entry.next_entry;
  } while (read_entry);
  DBUG_RETURN(error);
}


/*
  External interface methods for the DDL log Module
  ---------------------------------------------------
*/

/**
  Write a ddl log entry.

  A careful write of the ddl log is performed to ensure that we can
  handle crashes occurring during CREATE and ALTER TABLE processing.

  @param ddl_log_entry         Information about log entry
  @param[out] entry_written    Entry information written into   

  @return Operation status
    @retval TRUE               Error
    @retval FALSE              Success
*/

bool write_ddl_log_entry(DDL_LOG_ENTRY *ddl_log_entry,
                         DDL_LOG_MEMORY_ENTRY **active_entry)
{
  bool error, write_header;
  DBUG_ENTER("write_ddl_log_entry");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  set_global_from_ddl_log_entry(ddl_log_entry);
  if (get_free_ddl_log_entry(active_entry, &write_header))
  {
    DBUG_RETURN(TRUE);
  }
  error= FALSE;
  DBUG_PRINT("ddl_log",
             ("write type %c next %u name '%s' from_name '%s' handler '%s'"
              " tmp_name '%s'",
             global_ddl_log.file_entry_buf[DDL_LOG_ACTION_TYPE_POS],
             ddl_log_entry->next_entry,
             &global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS],
             &global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS
                                            + FN_REFLEN],
             &global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS
                                            + (2*FN_REFLEN)],
             &global_ddl_log.file_entry_buf[DDL_LOG_NAME_POS
                                            + (3*FN_REFLEN)]));
  if (write_ddl_log_file_entry((*active_entry)->entry_pos))
  {
    error= TRUE;
    sql_print_error("Failed to write entry_no = %u",
                    (*active_entry)->entry_pos);
  }
  if (write_header && !error)
  {
    (void) sync_ddl_log_no_lock();
    if (write_ddl_log_header())
      error= TRUE;
  }
  if (error)
    release_ddl_log_memory_entry(*active_entry);
  DBUG_RETURN(error);
}


/**
  @brief Write final entry in the ddl log.

  @details This is the last write in the ddl log. The previous log entries
  have already been written but not yet synched to disk.
  We write a couple of log entries that describes action to perform.
  This entries are set-up in a linked list, however only when a first
  execute entry is put as the first entry these will be executed.
  This routine writes this first.

  @param first_entry               First entry in linked list of entries
                                   to execute, if 0 = NULL it means that
                                   the entry is removed and the entries
                                   are put into the free list.
  @param complete                  Flag indicating we are simply writing
                                   info about that entry has been completed
  @param[in,out] active_entry      Entry to execute, 0 = NULL if the entry
                                   is written first time and needs to be
                                   returned. In this case the entry written
                                   is returned in this parameter

  @return Operation status
    @retval TRUE                   Error
    @retval FALSE                  Success
*/ 

bool write_execute_ddl_log_entry(uint first_entry,
                                 bool complete,
                                 DDL_LOG_MEMORY_ENTRY **active_entry)
{
  bool write_header= FALSE;
  char *file_entry_buf= (char*)global_ddl_log.file_entry_buf;
  DBUG_ENTER("write_execute_ddl_log_entry");

  mysql_mutex_assert_owner(&LOCK_gdl);
  if (init_ddl_log())
  {
    DBUG_RETURN(TRUE);
  }
  if (!complete)
  {
    /*
      We haven't synced the log entries yet, we sync them now before
      writing the execute entry. If complete is true we haven't written
      any log entries before, we are only here to write the execute
      entry to indicate it is done.
    */
    (void) sync_ddl_log_no_lock();
    file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= (char)DDL_LOG_EXECUTE_CODE;
  }
  else
    file_entry_buf[DDL_LOG_ENTRY_TYPE_POS]= (char)DDL_IGNORE_LOG_ENTRY_CODE;
  file_entry_buf[DDL_LOG_ACTION_TYPE_POS]= 0; /* Ignored for execute entries */
  file_entry_buf[DDL_LOG_PHASE_POS]= 0;
  int4store(&file_entry_buf[DDL_LOG_NEXT_ENTRY_POS], first_entry);
  file_entry_buf[DDL_LOG_NAME_POS]= 0;
  file_entry_buf[DDL_LOG_NAME_POS + FN_REFLEN]= 0;
  file_entry_buf[DDL_LOG_NAME_POS + 2*FN_REFLEN]= 0;
  if (!(*active_entry))
  {
    if (get_free_ddl_log_entry(active_entry, &write_header))
    {
      DBUG_RETURN(TRUE);
    }
    write_header= TRUE;
  }
  if (write_ddl_log_file_entry((*active_entry)->entry_pos))
  {
    sql_print_error("Error writing execute entry in ddl log");
    release_ddl_log_memory_entry(*active_entry);
    DBUG_RETURN(TRUE);
  }
  (void) sync_ddl_log_no_lock();
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


/**
  Deactivate an individual entry.

  @details see deactivate_ddl_log_entry_no_lock.

  @param entry_no     Entry position of record to change

  @return Operation status
    @retval TRUE      Error
    @retval FALSE     Success
*/

bool deactivate_ddl_log_entry(uint entry_no)
{
  bool error;
  DBUG_ENTER("deactivate_ddl_log_entry");

  mysql_mutex_lock(&LOCK_gdl);
  error= deactivate_ddl_log_entry_no_lock(entry_no);
  mysql_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(error);
}


/**
  Sync ddl log file.

  @return Operation status
    @retval TRUE        Error
    @retval FALSE       Success
*/

bool sync_ddl_log()
{
  bool error;
  DBUG_ENTER("sync_ddl_log");

  mysql_mutex_lock(&LOCK_gdl);
  error= sync_ddl_log_no_lock();
  mysql_mutex_unlock(&LOCK_gdl);

  DBUG_RETURN(error);
}


/**
  Release a log memory entry.
  @param log_memory_entry                Log memory entry to release
*/

void release_ddl_log_memory_entry(DDL_LOG_MEMORY_ENTRY *log_entry)
{
  DDL_LOG_MEMORY_ENTRY *first_free= global_ddl_log.first_free;
  DDL_LOG_MEMORY_ENTRY *next_log_entry= log_entry->next_log_entry;
  DDL_LOG_MEMORY_ENTRY *prev_log_entry= log_entry->prev_log_entry;
  DBUG_ENTER("release_ddl_log_memory_entry");

  mysql_mutex_assert_owner(&LOCK_gdl);
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


/**
  Execute one entry in the ddl log.
  
  Executing an entry means executing a linked list of actions.

  @param first_entry           Reference to first action in entry

  @return Operation status
    @retval TRUE               Error
    @retval FALSE              Success
*/

bool execute_ddl_log_entry(THD *thd, uint first_entry)
{
  bool error;
  DBUG_ENTER("execute_ddl_log_entry");

  mysql_mutex_lock(&LOCK_gdl);
  error= execute_ddl_log_entry_no_lock(thd, first_entry);
  mysql_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(error);
}


/**
  Close the ddl log.
*/

static void close_ddl_log()
{
  DBUG_ENTER("close_ddl_log");
  if (global_ddl_log.file_id >= 0)
  {
    (void) mysql_file_close(global_ddl_log.file_id, MYF(MY_WME));
    global_ddl_log.file_id= (File) -1;
  }
  DBUG_VOID_RETURN;
}


/**
  Execute the ddl log at recovery of MySQL Server.
*/

void execute_ddl_log_recovery()
{
  uint num_entries, i;
  THD *thd;
  DDL_LOG_ENTRY ddl_log_entry;
  char file_name[FN_REFLEN];
  static char recover_query_string[]= "INTERNAL DDL LOG RECOVER IN PROGRESS";
  DBUG_ENTER("execute_ddl_log_recovery");

  /*
    Initialise global_ddl_log struct
  */
  memset(global_ddl_log.file_entry_buf, 0, sizeof(global_ddl_log.file_entry_buf));
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

  thd->set_query(recover_query_string, strlen(recover_query_string));

  /* this also initialize LOCK_gdl */
  num_entries= read_ddl_log_header();
  mysql_mutex_lock(&LOCK_gdl);
  for (i= 1; i < num_entries + 1; i++)
  {
    if (read_ddl_log_entry(i, &ddl_log_entry))
    {
      sql_print_error("Failed to read entry no = %u from ddl log", i);
      continue;
    }
    if (ddl_log_entry.entry_type == DDL_LOG_EXECUTE_CODE)
    {
      if (execute_ddl_log_entry_no_lock(thd, ddl_log_entry.next_entry))
      {
        /* Real unpleasant scenario but we continue anyways.  */
        continue;
      }
    }
  }
  close_ddl_log();
  create_ddl_log_file_name(file_name);
  (void) mysql_file_delete(key_file_global_ddl_log, file_name, MYF(0));
  global_ddl_log.recovery_phase= FALSE;
  mysql_mutex_unlock(&LOCK_gdl);
  thd->reset_query();
  delete thd;
  DBUG_VOID_RETURN;
}


/**
  Release all memory allocated to the ddl log.
*/

void release_ddl_log()
{
  DDL_LOG_MEMORY_ENTRY *free_list;
  DDL_LOG_MEMORY_ENTRY *used_list;
  DBUG_ENTER("release_ddl_log");

  if (!global_ddl_log.do_release)
    DBUG_VOID_RETURN;

  mysql_mutex_lock(&LOCK_gdl);
  free_list= global_ddl_log.first_free;
  used_list= global_ddl_log.first_used;
  while (used_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= used_list->next_log_entry;
    my_free(used_list);
    used_list= tmp;
  }
  while (free_list)
  {
    DDL_LOG_MEMORY_ENTRY *tmp= free_list->next_log_entry;
    my_free(free_list);
    free_list= tmp;
  }
  close_ddl_log();
  global_ddl_log.inited= 0;
  mysql_mutex_unlock(&LOCK_gdl);
  mysql_mutex_destroy(&LOCK_gdl);
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

size_t build_table_shadow_filename(char *buff, size_t bufflen,
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
  int error= false;
  char path[FN_REFLEN+1];
  char shadow_path[FN_REFLEN+1];
  char shadow_frm_name[FN_REFLEN+1];
  char frm_name[FN_REFLEN+1];
  char *part_syntax_buf;
  uint syntax_len;
  handler *new_handler= lpt->table->file;
  DBUG_ENTER("mysql_write_frm");

  if (flags & (WFRM_WRITE_SHADOW | WFRM_INSTALL_SHADOW))
  {
    handlerton *db_type_default= lpt->part_info->default_engine_type;
    if (db_type_default != lpt->create_info->db_type &&
        db_type_default->partition_flags)
    {
      /* Use the new storage engine that natively supports partitioning! */
      lpt->create_info->db_type= lpt->part_info->default_engine_type;
    }
    if (lpt->table->file->ht != lpt->create_info->db_type)
    {
      DBUG_ASSERT(lpt->create_info->db_type->partition_flags != NULL);
      new_handler= get_new_handler(NULL, lpt->thd->mem_root,
                                   lpt->create_info->db_type);
      if (new_handler == NULL)
      {
        DBUG_RETURN(true);
      }
    }
  }
  /*
    Build shadow frm file name
  */
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1, lpt);
  strxmov(shadow_frm_name, shadow_path, reg_ext, NullS);
  if (flags & WFRM_WRITE_SHADOW)
  {
    Partition_handler *part_handler= new_handler->get_partition_handler();
    partition_info *old_part_info= NULL;
    /*
      Make sure the new part_info is used for the new definition. If it is
      not fixed yet, then it is only a meta data change and the current
      part_info can still be used.
    */
    if (part_handler != NULL && lpt->part_info != lpt->table->part_info &&
        lpt->part_info->fixed == true)
    {
      old_part_info= lpt->table->part_info;
      part_handler->set_part_info(lpt->part_info, false);
    }

    if (mysql_prepare_create_table(lpt->thd, lpt->db,
                                   lpt->table_name,
                                   lpt->create_info,
                                   lpt->alter_info,
                                   /*tmp_table*/ 1,
                                   &lpt->db_options,
                                   new_handler,
                                   &lpt->key_info_buffer,
                                   &lpt->key_count,
                                   /*select_field_count*/ 0) == 0)
    {
      partition_info *part_info= lpt->part_info;
      if (part_info)
      {
        sql_mode_t sql_mode_backup= lpt->thd->variables.sql_mode;
        lpt->thd->variables.sql_mode&= ~(MODE_ANSI_QUOTES);
        part_syntax_buf= generate_partition_syntax(part_info,
                                                   &syntax_len,
                                                   TRUE, TRUE,
                                                   lpt->create_info,
                                                   lpt->alter_info,
                                                   NULL);
        lpt->thd->variables.sql_mode= sql_mode_backup;
        if (part_syntax_buf == NULL)
        {
          error= true;
          goto end;
        }
        part_info->part_info_string= part_syntax_buf;
        part_info->part_info_len= syntax_len;
      }
      /* Write shadow frm file */

      lpt->create_info->table_options= lpt->db_options;
      if (mysql_create_frm(lpt->thd, shadow_frm_name, lpt->db,
                           lpt->table_name, lpt->create_info,
                           lpt->alter_info->create_list, lpt->key_count,
                           lpt->key_info_buffer, new_handler) ||
          new_handler->ha_create_handler_files(shadow_path, NULL,
                                               CHF_CREATE_FLAG,
                                               lpt->create_info))
      {
        mysql_file_delete(key_file_frm, shadow_frm_name, MYF(0));
        error= true;
      }
    }
    else
    {
      error= true;
    }
    /* Revert to the old_part_info which the open table is based on. */
    if (old_part_info != NULL)
    {
      part_handler->set_part_info(old_part_info, false);
    }
    if (error)
    {
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
      my_free(data);
      my_free(lpt->pack_frm_data);
      mem_alloc_error(length);
      error= true;
      goto end;
    }
    error= mysql_file_delete(key_file_frm, shadow_frm_name, MYF(MY_WME));
  }
  if (flags & WFRM_INSTALL_SHADOW)
  {
    partition_info *part_info= lpt->part_info;
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
    if (mysql_file_delete(key_file_frm, frm_name, MYF(MY_WME)) ||
        lpt->table->file->ha_create_handler_files(path, shadow_path,
                                                  CHF_DELETE_FLAG, NULL) ||
        deactivate_ddl_log_entry(part_info->frm_log_entry->entry_pos) ||
        (sync_ddl_log(), FALSE) ||
        mysql_file_rename(key_file_frm,
                          shadow_frm_name, frm_name, MYF(MY_WME)) ||
        new_handler->ha_create_handler_files(path, shadow_path,
                                             CHF_RENAME_FLAG, NULL))
    {
      error= true;
      deactivate_ddl_log_entry(part_info->frm_log_entry->entry_pos);
      part_info->frm_log_entry= NULL;
      (void) sync_ddl_log();
      goto end;
    }
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
    is_trans                      if the event changes either
                                  a trans or non-trans engine.

  RETURN VALUES
    NONE

  DESCRIPTION
    Write the binlog if open, routine used in multiple places in this
    file
*/

int write_bin_log(THD *thd, bool clear_error,
                  const char *query, size_t query_length, bool is_trans)
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
                             query, query_length, is_trans, FALSE, FALSE,
                             errcode);
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

    Wait if global_read_lock (FLUSH TABLES WITH READ LOCK) is set, but
    not if under LOCK TABLES.

  RETURN
    FALSE OK.  In this case ok packet is sent to user
    TRUE  Error

*/

bool mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
                    my_bool drop_temporary)
{
  bool error;
  Drop_table_error_handler err_handler;
  TABLE_LIST *table;
  /*
    The following flags will be used to check if this statement will be split
  */
  uint have_non_tmp_table= 0;
  uint have_trans_tmp_table= 0;
  uint have_non_trans_tmp_table= 0;

  DBUG_ENTER("mysql_rm_table");

  // DROP table is not allowed in the XA_IDLE or XA_PREPARED transaction states.
  if (thd->get_transaction()->xid_state()->check_xa_idle_or_prepared(true))
  {
    DBUG_RETURN(true);
  }

  /* Disable drop of enabled log tables, must be done before name locking */
  for (table= tables; table; table= table->next_local)
  {
    if (query_logger.check_if_log_table(table, true))
    {
      my_error(ER_BAD_LOG_STATEMENT, MYF(0), "DROP");
      DBUG_RETURN(true);
    }
    /*
      Here we are sure that the tmp table exists and will set the flag based on
      table transactional type.
    */
    if (is_temporary_table(table))
    {
      if (table->table->s->tmp_table == TRANSACTIONAL_TMP_TABLE)
        have_trans_tmp_table= 1;
      else if (table->table->s->tmp_table == NON_TRANSACTIONAL_TMP_TABLE)
        have_non_trans_tmp_table= 1;
    }
  }

  if (!drop_temporary)
  {
    if (!thd->locked_tables_mode)
    {
      if (lock_table_names(thd, tables, NULL,
                           thd->variables.lock_wait_timeout, 0))
        DBUG_RETURN(true);
      for (table= tables; table; table= table->next_local)
      {
        if (is_temporary_table(table))
          continue;

        tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table->db, table->table_name,
                         false);
        /* Here we are sure that a non-tmp table exists */
        have_non_tmp_table= 1;
      }
    }
    else
    {
      for (table= tables; table; table= table->next_local)
        if (is_temporary_table(table))
        {
          /*
            A temporary table.

            Don't try to find a corresponding MDL lock or assign it
            to table->mdl_request.ticket. There can't be metadata
            locks for temporary tables: they are local to the session.

            Later in this function we release the MDL lock only if
            table->mdl_requeset.ticket is not NULL. Thus here we
            ensure that we won't release the metadata lock on the base
            table locked with LOCK TABLES as a side effect of temporary
            table drop.
          */
          DBUG_ASSERT(table->mdl_request.ticket == NULL);
        }
        else
        {
          /*
            Not a temporary table.

            Since 'tables' list can't contain duplicates (this is ensured
            by parser) it is safe to cache pointer to the TABLE instances
            in its elements.
          */
          table->table= find_table_for_mdl_upgrade(thd, table->db,
                                                   table->table_name, false);
          if (!table->table)
            DBUG_RETURN(true);
          table->mdl_request.ticket= table->table->mdl_ticket;
          /* Here we are sure that a non-tmp table exists */
          have_non_tmp_table= 1;
        }
    }
  }

  /*
    DROP TABLE statements mixing non-temporary and temporary tables or
    transactional and non-transactional temporary tables are unsafe to execute
    if GTID_NEXT is set to GTID_GROUP because these statements will be split to
    be sent to binlog and there is only one GTID is available to log multiple
    statements.
    See comments in the beginning of mysql_rm_table_no_locks() for more info.
  */
  if (thd->variables.gtid_next.type == GTID_GROUP &&
      (have_non_tmp_table + have_trans_tmp_table +
       have_non_trans_tmp_table > 1))
  {
    DBUG_PRINT("err",("have_non_tmp_table: %d, have_trans_tmp_table: %d, "
                      "have_non_trans_tmp_table: %d", have_non_tmp_table,
                      have_trans_tmp_table, have_non_trans_tmp_table));
    my_error(ER_GTID_UNSAFE_BINLOG_SPLITTABLE_STATEMENT_AND_GTID_GROUP, MYF(0));
    DBUG_RETURN(true);
  }

  /* mark for close and remove all cached entries */
  thd->push_internal_handler(&err_handler);
  error= mysql_rm_table_no_locks(thd, tables, if_exists, drop_temporary,
                                 false, false);
  thd->pop_internal_handler();

  if (error)
    DBUG_RETURN(TRUE);

  if (thd->lex->drop_temporary && thd->in_multi_stmt_transaction_mode())
  {
    /*
      When autocommit is disabled, dropping temporary table sets this flag
      to start transaction in any case (regardless of binlog=on/off,
      binlog format and transactional/non-transactional engine) to make
      behavior consistent.
    */
    thd->server_status|= SERVER_STATUS_IN_TRANS;
  }
  my_ok(thd);
  DBUG_RETURN(FALSE);
}


/**
  Execute the drop of a normal or temporary table.

  @param  thd             Thread handler
  @param  tables          Tables to drop
  @param  if_exists       If set, don't give an error if table doesn't exists.
                          In this case we give an warning of level 'NOTE'
  @param  drop_temporary  Only drop temporary tables
  @param  drop_view       Allow to delete VIEW .frm
  @param  dont_log_query  Don't write query to log files. This will also not
                          generate warnings if the handler files doesn't exists

  @retval  0  ok
  @retval  1  Error
  @retval -1  Thread was killed

  @note This function assumes that metadata locks have already been taken.
        It is also assumed that the tables have been removed from TDC.

  @note This function assumes that temporary tables to be dropped have
        been pre-opened using corresponding table list elements.

  @todo When logging to the binary log, we should log
        tmp_tables and transactional tables as separate statements if we
        are in a transaction;  This is needed to get these tables into the
        cached binary log that is only written on COMMIT.
        The current code only writes DROP statements that only uses temporary
        tables to the cache binary log.  This should be ok on most cases, but
        not all.
*/

int mysql_rm_table_no_locks(THD *thd, TABLE_LIST *tables, bool if_exists,
                            bool drop_temporary, bool drop_view,
                            bool dont_log_query)
{
  TABLE_LIST *table;
  char path[FN_REFLEN + 1];
  const char *alias= NULL;
  size_t path_length= 0;
  String wrong_tables;
  int error= 0;
  int non_temp_tables_count= 0;
  bool foreign_key_error=0;
  bool non_tmp_error= 0;
  bool trans_tmp_table_deleted= 0, non_trans_tmp_table_deleted= 0;
  bool non_tmp_table_deleted= 0;
  bool have_nonexistent_tmp_table= 0;
  bool is_drop_tmp_if_exists_with_no_defaultdb= 0;
  String built_query;
  String built_trans_tmp_query, built_non_trans_tmp_query;
  String nonexistent_tmp_tables;
  DBUG_ENTER("mysql_rm_table_no_locks");

  /*
    Prepares the drop statements that will be written into the binary
    log as follows:

    1 - If we are not processing a "DROP TEMPORARY" it prepares a
    "DROP".

    2 - A "DROP" may result in a "DROP TEMPORARY" but the opposite is
    not true.

    3 - If the current format is row, the IF EXISTS token needs to be
    appended because one does not know if CREATE TEMPORARY was previously
    written to the binary log.

    4 - Add the IF_EXISTS token if necessary, i.e. if_exists is TRUE.

    5 - For temporary tables, there is a need to differentiate tables
    in transactional and non-transactional storage engines. For that,
    reason, two types of drop statements are prepared.

    The need to different the type of tables when dropping a temporary
    table stems from the fact that such drop does not commit an ongoing
    transaction and changes to non-transactional tables must be written
    ahead of the transaction in some circumstances.

    6 - At the time of writing 'DROP TEMPORARY TABLE IF EXISTS'
    statements into the binary log if the default database specified in
    thd->db is present then they are binlogged as
    'USE `default_db`; DROP TEMPORARY TABLE IF EXISTS `t1`;'
    otherwise they will be binlogged with the actual database name to
    which the table belongs to.
    'DROP TEMPORARY TABLE IF EXISTS `actual_db`.`t1`
  */
  if (!dont_log_query)
  {
    if (!drop_temporary)
    {
      built_query.set_charset(system_charset_info);
      if (if_exists)
        built_query.append("DROP TABLE IF EXISTS ");
      else
        built_query.append("DROP TABLE ");
    }

    if (thd->is_current_stmt_binlog_format_row() || if_exists)
    {
      /*
        If default database doesnot exist in those cases set
        'is_drop_tmp_if_exists_with_no_defaultdb flag to 'true' so that the
        'DROP TEMPORARY TABLE IF EXISTS' command is logged with a qualified
        table name.
      */
      if (thd->db().str != NULL && check_db_dir_existence(thd->db().str))
        is_drop_tmp_if_exists_with_no_defaultdb= true;
      built_trans_tmp_query.set_charset(system_charset_info);
      built_trans_tmp_query.append("DROP TEMPORARY TABLE IF EXISTS ");
      built_non_trans_tmp_query.set_charset(system_charset_info);
      built_non_trans_tmp_query.append("DROP TEMPORARY TABLE IF EXISTS ");
    }
    else
    {
      built_trans_tmp_query.set_charset(system_charset_info);
      built_trans_tmp_query.append("DROP TEMPORARY TABLE ");
      built_non_trans_tmp_query.set_charset(system_charset_info);
      built_non_trans_tmp_query.append("DROP TEMPORARY TABLE ");
    }
    nonexistent_tmp_tables.set_charset(system_charset_info);
  }

  for (table= tables; table; table= table->next_local)
  {
    bool is_trans;
    const char *db= table->db;
    size_t db_len= table->db_length;
    handlerton *table_type;
    enum legacy_db_type frm_db_type= DB_TYPE_UNKNOWN;

    DBUG_PRINT("table", ("table_l: '%s'.'%s'  table: 0x%lx  s: 0x%lx",
                         table->db, table->table_name, (long) table->table,
                         table->table ? (long) table->table->s : (long) -1));

    /*
      If we are in locked tables mode and are dropping a temporary table,
      the ticket should be NULL to ensure that we don't release a lock
      on a base table later.
    */
    DBUG_ASSERT(!(thd->locked_tables_mode &&
                  table->open_type != OT_BASE_ONLY &&
                  find_temporary_table(thd, table) &&
                  table->mdl_request.ticket != NULL));

    thd->add_to_binlog_accessed_dbs(table->db);

    /*
      drop_temporary_table may return one of the following error codes:
      .  0 - a temporary table was successfully dropped.
      .  1 - a temporary table was not found.
      . -1 - a temporary table is used by an outer statement.
    */
    if (table->open_type == OT_BASE_ONLY)
      error= 1;
    else if ((error= drop_temporary_table(thd, table, &is_trans)) == -1)
    {
      DBUG_ASSERT(thd->in_sub_stmt);
      goto err;
    }

    if ((drop_temporary && if_exists) || !error)
    {
      /*
        This handles the case of temporary tables. We have the following cases:

          . "DROP TEMPORARY" was executed and a temporary table was affected
          (i.e. drop_temporary && !error) or the if_exists was specified (i.e.
          drop_temporary && if_exists).

          . "DROP" was executed but a temporary table was affected (.i.e
          !error).
      */
      if (!dont_log_query)
      {
        /*
          If there is an error, we don't know the type of the engine
          at this point. So, we keep it in the nonexistent_tmp_table list.
        */
        if (error == 1)
          have_nonexistent_tmp_table= true;
        else if (is_trans)
          trans_tmp_table_deleted= TRUE;
        else
          non_trans_tmp_table_deleted= TRUE;

        String *built_ptr_query=
          (error == 1 ? &nonexistent_tmp_tables :
           (is_trans ? &built_trans_tmp_query : &built_non_trans_tmp_query));
        /*
          Write the database name if it is not the current one or if
          thd->db is NULL or 'IF EXISTS' clause is present in 'DROP TEMPORARY'
          query.
        */
        if (thd->db().str == NULL || strcmp(db, thd->db().str) != 0
            || is_drop_tmp_if_exists_with_no_defaultdb )
        {
          append_identifier(thd, built_ptr_query, db, db_len,
                            system_charset_info, thd->charset());
          built_ptr_query->append(".");
        }
        append_identifier(thd, built_ptr_query, table->table_name,
                           strlen(table->table_name), system_charset_info,
                           thd->charset());
        built_ptr_query->append(",");
      }
      /*
        This means that a temporary table was droped and as such there
        is no need to proceed with the code that tries to drop a regular
        table.
      */
      if (!error) continue;
    }
    else if (!drop_temporary)
    {
      non_temp_tables_count++;

      if (thd->locked_tables_mode)
      {
        if (wait_while_table_is_used(thd, table->table, HA_EXTRA_FORCE_REOPEN))
        {
          error= -1;
          goto err;
        }
        close_all_tables_for_name(thd, table->table->s, true, NULL);
        table->table= 0;
      }

      /* Check that we have an exclusive lock on the table to be dropped. */
      DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                     table->db, table->table_name,
                                     MDL_EXCLUSIVE));
      if (thd->killed)
      {
        error= -1;
        goto err;
      }
      alias= (lower_case_table_names == 2) ? table->alias : table->table_name;
      /* remove .frm file and engine files */
      path_length= build_table_filename(path, sizeof(path) - 1, db, alias,
                                        reg_ext,
                                        table->internal_tmp_table ?
                                        FN_IS_TMP : 0);

      /*
        This handles the case where a "DROP" was executed and a regular
        table "may be" dropped as drop_temporary is FALSE and error is
        TRUE. If the error was FALSE a temporary table was dropped and
        regardless of the status of drop_tempoary a "DROP TEMPORARY"
        must be used.
      */
      if (!dont_log_query)
      {
        /*
          Note that unless if_exists is TRUE or a temporary table was deleted, 
          there is no means to know if the statement should be written to the
          binary log. See further information on this variable in what follows.
        */
        non_tmp_table_deleted= (if_exists ? TRUE : non_tmp_table_deleted);
        /*
          Don't write the database name if it is the current one (or if
          thd->db is NULL).
        */
        if (thd->db().str == NULL || strcmp(db, thd->db().str) != 0)
        {
          append_identifier(thd, &built_query, db, db_len,
                            system_charset_info, thd->charset());
          built_query.append(".");
        }
        append_identifier(thd, &built_query, table->table_name,
                          strlen(table->table_name), system_charset_info,
                          thd->charset());
        built_query.append(",");
      }
    }
    DEBUG_SYNC(thd, "rm_table_no_locks_before_delete_table");
    DBUG_EXECUTE_IF("sleep_before_no_locks_delete_table",
                    my_sleep(100000););
    error= 0;
    if (drop_temporary ||
        ((access(path, F_OK) &&
          ha_create_table_from_engine(thd, db, alias)) ||
         (!drop_view &&
          dd_frm_type(thd, path, &frm_db_type) != FRMTYPE_TABLE)))
    {
      /*
        One of the following cases happened:
          . "DROP TEMPORARY" but a temporary table was not found.
          . "DROP" but table was not found on disk and table can't be
            created from engine.
          . ./sql/datadict.cc +32 /Alfranio - TODO: We need to test this.
      */
      if (if_exists)
      {
        String tbl_name;
        tbl_name.append(String(db,system_charset_info));
        tbl_name.append('.');
        tbl_name.append(String(table->table_name,system_charset_info));

        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
                            tbl_name.c_ptr());
      }
      else
      {
        non_tmp_error = (drop_temporary ? non_tmp_error : TRUE);
        error= 1;
      }
    }
    else
    {
      char *end;
      if (frm_db_type == DB_TYPE_UNKNOWN)
      {
        dd_frm_type(thd, path, &frm_db_type);
        DBUG_PRINT("info", ("frm_db_type %d from %s", frm_db_type, path));
      }
      table_type= ha_resolve_by_legacy_type(thd, frm_db_type);
      if (frm_db_type != DB_TYPE_UNKNOWN && !table_type)
      {
        my_error(ER_STORAGE_ENGINE_NOT_LOADED, MYF(0), db, table->table_name);
        wrong_tables.mem_free();
        error= 1;
        goto err;
      }
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
        foreign_key_error= 1;
      }
      if (!error || error == ENOENT || error == HA_ERR_NO_SUCH_TABLE)
      {
        int new_error;
        /* Delete the table definition file */
        my_stpcpy(end,reg_ext);
        if (!(new_error= mysql_file_delete(key_file_frm, path, MYF(MY_WME))))
        {
          non_tmp_table_deleted= TRUE;
          new_error= drop_all_triggers(thd, db, table->table_name);
        }
        error|= new_error;
        /* Invalidate even if we failed to delete the .FRM file. */
        query_cache.invalidate_single(thd, table, FALSE);
      }
       non_tmp_error= error ? TRUE : non_tmp_error;
    }
    if (error)
    {
      if (error == HA_ERR_TOO_MANY_CONCURRENT_TRXS)
      {
        my_error(HA_ERR_TOO_MANY_CONCURRENT_TRXS, MYF(0));
        wrong_tables.mem_free();
        error= 1;
        goto err;
      }

      if (wrong_tables.length())
        wrong_tables.append(',');

      wrong_tables.append(String(db,system_charset_info));
      wrong_tables.append('.');
      wrong_tables.append(String(table->table_name,system_charset_info));
    }
    DBUG_PRINT("table", ("table: 0x%lx  s: 0x%lx", (long) table->table,
                         table->table ? (long) table->table->s : (long) -1));

    DBUG_EXECUTE_IF("bug43138",
                    my_printf_error(ER_BAD_TABLE_ERROR,
                                    ER(ER_BAD_TABLE_ERROR), MYF(0),
                                    table->table_name););
#ifdef HAVE_PSI_TABLE_INTERFACE
    if (drop_temporary && likely(error == 0))
      PSI_TABLE_CALL(drop_table_share)
        (true, table->db, static_cast<int>(table->db_length),
         table->table_name, static_cast<int>(table->table_name_length));
#endif
  }
  DEBUG_SYNC(thd, "rm_table_no_locks_before_binlog");
  thd->thread_specific_used|= (trans_tmp_table_deleted ||
                               non_trans_tmp_table_deleted);
  error= 0;
err:
  if (wrong_tables.length())
  {
    if (!foreign_key_error)
      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      wrong_tables.c_ptr());
    else
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    error= 1;
  }

  if (have_nonexistent_tmp_table || non_trans_tmp_table_deleted ||
      trans_tmp_table_deleted || non_tmp_table_deleted)
  {
    if (have_nonexistent_tmp_table || non_trans_tmp_table_deleted ||
        trans_tmp_table_deleted)
      thd->get_transaction()->mark_dropped_temp_table(Transaction_ctx::STMT);

    /*
      The statement may contain up to three types of temporary tables:
      transactional, non-transactional, and non-existent tables.

      The statement is logged using up to two statements:
      non-transactional and transactional tables are logged in
      different statements.

      The non-existing tables are logged together with transactional
      ones, if any transactional tables exist or if there is only
      non-existing tables; otherwise are logged together with
      non-transactional ones.

      This logic ensures that:
      - On master, transactional and non-transactional tables are
        written to different statements.
      - Therefore, slave will never see statements containing both
        transactional and non-transactional tables.
      - Since non-existing temporary tables are logged together with
        whatever type of temporary tables that exist, the slave thus
        writes any statement as just one statement. I.e., the slave
        never splits a statement into two.  This is crucial when GTIDs
        are enabled, since otherwise the statement, which already has
        a GTID, would need two different GTIDs.
    */
    if (!dont_log_query && mysql_bin_log.is_open())
    {
      if (non_trans_tmp_table_deleted)
      {
        /*
          Add the list of nonexistent tmp tables here only if there is no
          trans tmp table deleted.
        */
        if (!trans_tmp_table_deleted && have_nonexistent_tmp_table)
          built_non_trans_tmp_query.append(nonexistent_tmp_tables);
        /* Chop off the last comma */
        built_non_trans_tmp_query.chop();
        built_non_trans_tmp_query.append(" /* generated by server */");
        error |= thd->binlog_query(THD::STMT_QUERY_TYPE,
                                   built_non_trans_tmp_query.ptr(),
                                   built_non_trans_tmp_query.length(),
                                   false/*is_trans*/, false/*direct*/,
                                   is_drop_tmp_if_exists_with_no_defaultdb/*suppress_use*/,
                                   0)/*errcode*/;
      }
      if (trans_tmp_table_deleted ||
          (have_nonexistent_tmp_table && !non_trans_tmp_table_deleted))
      {
        /*
          When multiple tables are dropped, the tables are classified
          in the following categories:

          - non-temporary
          - temporary transactional
          - temporary non-transactional

          The statement is split into one statement for each of the
          categories that some table belongs to. So if tables belong
          to one category, then just one statement is written; if
          tables belong to two or three categories, then two or three
          statements are written.

          There must be one Gtid_log_event or Anonymous_log_event for
          each DDL statement.  Therefore, a commit is issued after
          each statement.  However, when GTID_MODE=OFF, it is possible
          for a DROP TEMPORARY to occur in the middle of a
          transaction.  In this case, there must *not* be a commit,
          since that would commit the transaction. DROP TEMPORARY is
          not supposed to have an implicit commit when executed in a
          transaction.

          So we use the following logic:

          - If we are not in a transaction, always generate a commit
            for a split statement.

          - If we are in a transaction, do not generate a commit for a
            split statement.

            The transaction case only happens for DROP TEMPORARY,
            since DROP without TEMPORARY has an implicit commit, i.e.,
            commits any ongoing transaction before it starts to
            execute.  So we only need to check the condition for
            ongoing transaction for this call to mysql_bin_log.commit,
            and not for the call inside the code block 'if
            (non_tmp_table_deleted)'.
        */
        if (!thd->in_active_multi_stmt_transaction() &&
            non_trans_tmp_table_deleted)
        {
          DBUG_PRINT("info", ("mysql_rm_table_no_locks commit point 1"));
          thd->is_commit_in_middle_of_statement= true;
          error |= mysql_bin_log.commit(thd, true);
          thd->is_commit_in_middle_of_statement= false;
        }
        if (have_nonexistent_tmp_table)
          built_trans_tmp_query.append(nonexistent_tmp_tables);
        /* Chop off the last comma */
        built_trans_tmp_query.chop();
        built_trans_tmp_query.append(" /* generated by server */");
        error |= thd->binlog_query(THD::STMT_QUERY_TYPE,
                                   built_trans_tmp_query.ptr(),
                                   built_trans_tmp_query.length(),
                                   true/*is_trans*/, false/*direct*/,
                                   is_drop_tmp_if_exists_with_no_defaultdb/*suppress_use*/,
                                   0/*errcode*/);
      }
      /*
        When the DROP TABLE command is used to drop a single table and if that
        command fails then the query cannot generate 'partial results'. In
        that case the query will not be written to the binary log.
      */
      if (non_tmp_table_deleted &&
          (thd->lex->select_lex->table_list.elements > 1 || !error))
      {
        /// @see comment for mysql_bin_log.commit above.
        if (non_trans_tmp_table_deleted || trans_tmp_table_deleted ||
            have_nonexistent_tmp_table)
        {
          DBUG_PRINT("info", ("mysql_rm_table_no_locks commit point 1"));
          thd->is_commit_in_middle_of_statement= true;
          error |= mysql_bin_log.commit(thd, true);
          thd->is_commit_in_middle_of_statement= false;
        }
        /* Chop off the last comma */
        built_query.chop();
        built_query.append(" /* generated by server */");
        int error_code = (non_tmp_error ?
          (foreign_key_error ? ER_ROW_IS_REFERENCED : ER_BAD_TABLE_ERROR) : 0);
        error |= thd->binlog_query(THD::STMT_QUERY_TYPE,
                                   built_query.ptr(),
                                   built_query.length(),
                                   true/*is_trans*/, false/*direct*/,
                                   false/*suppress_use*/,
                                   error_code);
      }
    }
    else if (error)
    {
      /*
        We do not care the returned value, since it goes ahead
        with error branch in any case.
      */
      (void) commit_owned_gtid_by_partial_command(thd);
    }
  }

  if (!drop_temporary)
  {
    /*
      Under LOCK TABLES we should release meta-data locks on the tables
      which were dropped.

      Leave LOCK TABLES mode if we managed to drop all tables which were
      locked. Additional check for 'non_temp_tables_count' is to avoid
      leaving LOCK TABLES mode if we have dropped only temporary tables.
    */
    if (thd->locked_tables_mode)
    {
      if (thd->lock && thd->lock->table_count == 0 && non_temp_tables_count > 0)
      {
        thd->locked_tables_list.unlock_locked_tables(thd);
        goto end;
      }
      for (table= tables; table; table= table->next_local)
      {
        /* Drop locks for all successfully dropped tables. */
        if (table->table == NULL && table->mdl_request.ticket)
        {
          /*
            Under LOCK TABLES we may have several instances of table open
            and locked and therefore have to remove several metadata lock
            requests associated with them.
          */
          thd->mdl_context.release_all_locks_for_name(table->mdl_request.ticket);
        }
      }
    }
    /*
      Rely on the caller to implicitly commit the transaction
      and release metadata locks.
    */
  }

end:
  DBUG_RETURN(error);
}


/**
  Quickly remove a table.

  @param thd         Thread context.
  @param base        The handlerton handle.
  @param db          The database name.
  @param table_name  The table name.
  @param flags       Flags for build_table_filename() as well as describing
                     if handler files / .FRM should be deleted as well.

  @return False in case of success, True otherwise.
*/

bool quick_rm_table(THD *thd, handlerton *base, const char *db,
                    const char *table_name, uint flags)
{
  char path[FN_REFLEN + 1];
  bool error= 0;
  DBUG_ENTER("quick_rm_table");

  size_t path_length= build_table_filename(path, sizeof(path) - 1,
                                           db, table_name, reg_ext, flags);
  if (mysql_file_delete(key_file_frm, path, MYF(0)))
    error= 1; /* purecov: inspected */
  path[path_length - reg_ext_length]= '\0'; // Remove reg_ext
  if (flags & NO_HA_TABLE)
  {
    handler *file= get_new_handler((TABLE_SHARE*) 0, thd->mem_root, base);
    if (!file)
      DBUG_RETURN(true);
    (void) file->ha_create_handler_files(path, NULL, CHF_DELETE_FLAG, NULL);
    delete file;
  }
  if (!(flags & (FRM_ONLY|NO_HA_TABLE)))
    error|= ha_delete_table(current_thd, base, path, db, table_name, 0);
  DBUG_RETURN(error);
}

/*
   Sort keys according to the following properties, in decreasing order of
   importance:
   - PRIMARY KEY
   - UNIQUE with all columns NOT NULL
   - UNIQUE without partial segments
   - UNIQUE
   - without fulltext columns
   - without virtual generated columns

   This allows us to
   - check for duplicate key values faster (PK and UNIQUE are first)
   - prioritize PKs
   - be sure that, if there is no PK, the set of UNIQUE keys candidate for
   promotion starts at number 0, and we can choose #0 as PK (it is required
   that PK has number 0).
*/

static int sort_keys(KEY *a, KEY *b)
{
  ulong a_flags= a->flags, b_flags= b->flags;
  
  if (a_flags & HA_NOSAME)
  {
    if (!(b_flags & HA_NOSAME))
      return -1;
    if ((a_flags ^ b_flags) & HA_NULL_PART_KEY)
    {
      /* Sort NOT NULL keys before other keys */
      return (a_flags & HA_NULL_PART_KEY) ? 1 : -1;
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

  if ((a_flags ^ b_flags) & HA_VIRTUAL_GEN_KEY)
  {
    return (a_flags & HA_VIRTUAL_GEN_KEY) ? 1 : -1;
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
                                  const CHARSET_INFO *cs, uint *dup_val_count)
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
    if (find_type2(&tmp, *cur_value, *cur_length, cs))
    {
      THD *thd= current_thd;
      ErrConvString err(*cur_value, *cur_length, cs);
      if (current_thd->is_strict_mode())
      {
        my_error(ER_DUPLICATED_VALUE_IN_TYPE, MYF(0),
                 name, err.ptr(), set_or_name);
        return 1;
      }
      push_warning_printf(thd,Sql_condition::SL_NOTE,
                          ER_DUPLICATED_VALUE_IN_TYPE,
                          ER(ER_DUPLICATED_VALUE_IN_TYPE),
                          name, err.ptr(), set_or_name);
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
static void calculate_interval_lengths(const CHARSET_INFO *cs,
                                       TYPELIB *interval,
                                       size_t *max_length,
                                       size_t *tot_length)
{
  const char **pos;
  uint *len;
  *max_length= *tot_length= 0;
  for (pos= interval->type_names, len= interval->type_lengths;
       *pos ; pos++, len++)
  {
    size_t length= cs->cset->numchars(cs, *pos, *pos + *len);
    *tot_length+= length;
    set_if_bigger(*max_length, length);
  }
}


/*
  Prepare a create_table instance for packing

  SYNOPSIS
    prepare_create_field()
    sql_field     field to prepare for packing
    blob_columns  count for BLOBs
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
  case MYSQL_TYPE_JSON:
    // JSON fields are stored as BLOBs.
    sql_field->pack_flag=FIELDFLAG_JSON |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;                        // Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
  case MYSQL_TYPE_VARCHAR:
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
  case MYSQL_TYPE_TIME2:
  case MYSQL_TYPE_DATETIME2:
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
  case MYSQL_TYPE_TIMESTAMP2:
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


static TYPELIB *create_typelib(MEM_ROOT *mem_root,
                               Create_field *field_def,
                               List<String> *src)
{
  const CHARSET_INFO *cs= field_def->charset;

  if (!src->elements)
    return NULL;

  TYPELIB *result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
  result->count= src->elements;
  result->name= "";
  if (!(result->type_names=(const char **)
        alloc_root(mem_root,(sizeof(char *)+sizeof(int))*(result->count+1))))
    return NULL;
  result->type_lengths= (uint*)(result->type_names + result->count+1);
  List_iterator<String> it(*src);
  String conv;
  for (uint i=0; i < result->count; i++)
  {
    size_t dummy;
    size_t length;
    String *tmp= it++;

    if (String::needs_conversion(tmp->length(), tmp->charset(),
                                 cs, &dummy))
    {
      uint cnv_errs;
      conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);

      length= conv.length();
      result->type_names[i]= strmake_root(mem_root, conv.ptr(),
                                          length);
    }
    else
    {
      length= tmp->length();
      result->type_names[i]= strmake_root(mem_root, tmp->ptr(), length);
    }

    // Strip trailing spaces.
    length= cs->cset->lengthsp(cs, result->type_names[i], length);
    result->type_lengths[i]= length;
    ((uchar *)result->type_names[i])[length]= '\0';
  }
  result->type_names[result->count]= 0;
  result->type_lengths[result->count]= 0;

  return result;
}


/**
  Prepare an instance of Create_field for field creation
  (fill all necessary attributes).

  @param[in]  thd          Thread handle
  @param[in]  sp           The current SP
  @param[in]  field_type   Field type
  @param[out] field_def    An instance of create_field to be filled

  @return Error status.
*/

bool fill_field_definition(THD *thd,
                           sp_head *sp,
                           enum enum_field_types field_type,
                           Create_field *field_def)
{
  LEX *lex= thd->lex;
  LEX_STRING cmt = { 0, 0 };
  uint unused1= 0;

  if (field_def->init(thd, (char*) "", field_type, lex->length, lex->dec,
                      lex->type, (Item*) 0, (Item*) 0, &cmt, 0,
                      &lex->interval_list,
                      lex->charset ? lex->charset :
                                     thd->variables.collation_database,
                      lex->uint_geom_type, NULL))
  {
    return true;
  }

  if (field_def->interval_list.elements)
  {
    field_def->interval= create_typelib(sp->get_current_mem_root(),
                                        field_def,
                                        &field_def->interval_list);
  }

  sp_prepare_create_field(thd, field_def);

  return prepare_create_field(field_def, &unused1, HA_CAN_GEOMETRY);
}

/*
  Get character set from field object generated by parser using
  default values when not set.

  SYNOPSIS
    get_sql_field_charset()
    sql_field                 The sql_field object
    create_info               Info generated by parser

  RETURN VALUES
    cs                        Character set
*/

const CHARSET_INFO* get_sql_field_charset(Create_field *sql_field,
                                          HA_CREATE_INFO *create_info)
{
  const CHARSET_INFO *cs= sql_field->charset;

  if (!cs)
    cs= create_info->default_table_charset;
  /*
    table_charset is set only in ALTER TABLE t1 CONVERT TO CHARACTER SET csname
    if we want change character set for all varchar/char columns.
    But the table charset must not affect the BLOB fields, so don't
    allow to change my_charset_bin to somethig else.
  */
  if (create_info->table_charset && cs != &my_charset_bin)
    cs= create_info->table_charset;
  return cs;
}


/**
   Modifies the first column definition whose SQL type is TIMESTAMP
   by adding the features DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP.

   @param column_definitions The list of column definitions, in the physical
                             order in which they appear in the table.
 */
void promote_first_timestamp_column(List<Create_field> *column_definitions)
{
  List_iterator<Create_field> it(*column_definitions);
  Create_field *column_definition;

  while ((column_definition= it++) != NULL)
  {
    if (column_definition->sql_type == MYSQL_TYPE_TIMESTAMP ||      // TIMESTAMP
        column_definition->sql_type == MYSQL_TYPE_TIMESTAMP2 || //  ms TIMESTAMP
        column_definition->unireg_check == Field::TIMESTAMP_OLD_FIELD) // Legacy
    {
      if ((column_definition->flags & NOT_NULL_FLAG) != 0 && // NOT NULL,
          column_definition->def == NULL &&            // no constant default,
          column_definition->gcol_info == NULL &&      // not a generated column
          column_definition->unireg_check == Field::NONE) // no function default
      {
        DBUG_PRINT("info", ("First TIMESTAMP column '%s' was promoted to "
                            "DEFAULT CURRENT_TIMESTAMP ON UPDATE "
                            "CURRENT_TIMESTAMP",
                            column_definition->field_name
                            ));
        column_definition->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
      }
      return;
    }
  }
}


/**
  Check if there is a duplicate key. Report a warning for every duplicate key.

  @param thd                Thread context.
  @param error_schema_name  Schema name of the table used for error reporting.
  @param error_table_name   Table name used for error reporting.
  @param key                Key to be checked.
  @param key_info           Key meta-data info.
  @param alter_info         List of columns and indexes to create.

  @retval false           Ok.
  @retval true            Error.
*/
static bool check_duplicate_key(THD *thd, const char *error_schema_name,
                                const char *error_table_name,
                                Key *key, KEY *key_info,
                                Alter_info *alter_info)
{
  /*
    We only check for duplicate indexes if it is requested and the
    key is not auto-generated.

    Check is requested if the key was explicitly created or altered
    (Index is altered/column associated with it is dropped) by the user
    (unless it's a foreign key).
  */
  if (!key->key_create_info.check_for_duplicate_indexes || key->generated)
    return false;

  List_iterator<Key> key_list_iterator(alter_info->key_list);
  List_iterator<Key_part_spec> key_column_iterator(key->columns);
  Key *k;

  while ((k= key_list_iterator++))
  {
    // Looking for a similar key...

    if (k == key)
    {
      /*
        Since the duplicate index might exist before or after
        the modified key in the list, we continue the 
        comparison with rest of the keys in case of DROP COLUMN
        operation.
      */
      if (alter_info->flags & Alter_info::ALTER_DROP_COLUMN) 
        continue;
      else
        break;
    }

    if (k->generated ||
        (key->type != k->type) ||
        (key->key_create_info.algorithm != k->key_create_info.algorithm) ||
        (key->columns.elements != k->columns.elements))
    {
      // Keys are different.
      continue;
    }

    /*
      Keys 'key' and 'k' might be identical.
      Check that the keys have identical columns in the same order.
    */

    List_iterator<Key_part_spec> k_column_iterator(k->columns);

    bool all_columns_are_identical= true;

    key_column_iterator.rewind();

    for (uint i= 0; i < key->columns.elements; ++i)
    {
      Key_part_spec *c1= key_column_iterator++;
      Key_part_spec *c2= k_column_iterator++;
  
      DBUG_ASSERT(c1 && c2);

      if (my_strcasecmp(system_charset_info,
                        c1->field_name.str, c2->field_name.str) ||
          (c1->length != c2->length))
      {
        all_columns_are_identical= false;
        break;
      }
    }

    // Report a warning if we have two identical keys.

    if (all_columns_are_identical)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_DUP_INDEX, ER(ER_DUP_INDEX),
                          key_info->name,
                          error_schema_name,
                          error_table_name);
      if (thd->is_error())
      {
        // An error was reported.
        return true;
      }
      break;
    }
  }
  return false;
}


/*
  Preparation for table creation

  SYNOPSIS
    mysql_prepare_create_table()
      thd                       Thread object.
      error_schema_name         Schema name of the table to create/alter,only
                                used for error reporting.
      error_table_name          Name of table to create/alter, only used for
                                error reporting.
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
mysql_prepare_create_table(THD *thd, const char *error_schema_name,
                           const char *error_table_name,
                           HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                           uint *db_options,
                           handler *file, KEY **key_info_buffer,
                           uint *key_count, int select_field_count)
{
  const char	*key_name;
  Create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  size_t	record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<Create_field> it(alter_info->create_list);
  List_iterator<Create_field> it2(alter_info->create_list);
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("mysql_prepare_create_table");

  LEX_STRING* connect_string = &create_info->connect_string;
  if (connect_string->length != 0 &&
      connect_string->length > CONNECT_STRING_MAXLEN &&
      (system_charset_info->cset->charpos(system_charset_info,
                                          connect_string->str,
                                          (connect_string->str +
                                           connect_string->length),
                                          CONNECT_STRING_MAXLEN)
      < connect_string->length))
  {
    my_error(ER_WRONG_STRING_LENGTH, MYF(0),
             connect_string->str, "CONNECTION", CONNECT_STRING_MAXLEN);
    DBUG_RETURN(TRUE);
  }

  select_field_pos= alter_info->create_list.elements - select_field_count;
  null_fields=blob_columns=0;
  create_info->varchar= 0;
  max_key_length= file->max_key_length();

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    const CHARSET_INFO *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;
    /* Set field charset. */
    save_cs= sql_field->charset= get_sql_field_charset(sql_field,
                                                       create_info);
    if (sql_field->flags & BINCMP_FLAG)
    {
      // e.g. CREATE TABLE t1 (a CHAR(1) BINARY);
      if (!(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
                                                      MY_CS_BINSORT,MYF(0))))
      {
        char tmp[65];
        strmake(strmake(tmp, save_cs->csname, sizeof(tmp)-4),
                STRING_WITH_LEN("_bin"));
        my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
        DBUG_RETURN(TRUE);
      }
      /*
        Now that we have sql_field->charset set properly,
        we don't need the BINCMP_FLAG any longer.
      */
      sql_field->flags&= ~BINCMP_FLAG;
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
      size_t dummy;
      const CHARSET_INFO *cs= sql_field->charset;
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
        char comma_buf[4]; /* 4 bytes for utf32 */
        int comma_length= cs->cset->wc_mb(cs, ',', (uchar*) comma_buf,
                                          (uchar*) comma_buf + 
                                          sizeof(comma_buf));
        DBUG_ASSERT(comma_length > 0);
        for (uint i= 0; (tmp= int_it++); i++)
        {
          size_t lengthsp;
          size_t dummy2;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy2))
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
              ErrConvString err(tmp->ptr(), tmp->length(), cs);
              my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", err.ptr());
              DBUG_RETURN(TRUE);
            }
          }
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      if (sql_field->sql_type == MYSQL_TYPE_SET)
      {
        size_t field_length;
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
        size_t field_length;
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

          /*
            If we are replacing a BIT field, revert the increment
            of total_uneven_bit_length that was done above.
          */
          if (sql_field->sql_type == MYSQL_TYPE_BIT &&
              file->ha_table_flags() & HA_CAN_BIT_FIELD)
            total_uneven_bit_length-= sql_field->length & 7;

	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;

          /*
            If we are replacing a field with a BIT field, we need
            to initialize pack_flag. Note that we do not need to
            increment total_uneven_bit_length here as this dup_field
            has already been processed.
          */
          if (sql_field->sql_type == MYSQL_TYPE_BIT)
          {
            sql_field->pack_flag= FIELDFLAG_NUMBER;
            if (!(file->ha_table_flags() & HA_CAN_BIT_FIELD))
              sql_field->pack_flag|= FIELDFLAG_TREAT_BIT_AS_CHAR;
          }

	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
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
	  sql_field->create_length_to_internal_length();
          sql_field->interval=          dup_field->interval;
          sql_field->gcol_info=         dup_field->gcol_info;
          sql_field->stored_in_db=      dup_field->stored_in_db;
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

  bool has_vgc= false;
  it.rewind();
  while ((sql_field=it++))
  {
    DBUG_ASSERT(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns, 
			     file->ha_table_flags()))
      DBUG_RETURN(TRUE);
    if (sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      create_info->varchar= TRUE;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    /*
      For now skip fields that are not physically stored in the database
      (generated fields) and update their offset later 
      (see the next loop).
    */
    if (sql_field->stored_in_db)
      record_offset+= sql_field->pack_length;
    else
      has_vgc= true;
  }
  /* Update generated fields' offset*/
  if (has_vgc)
  {
    it.rewind();
    while ((sql_field=it++))
    {
      if (!sql_field->stored_in_db)
      {
        sql_field->offset= record_offset;
        record_offset+= sql_field->pack_length;
      }
    }
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

  /*
   CREATE TABLE[with auto_increment column] SELECT is unsafe as the rows
   inserted in the created table depends on the order of the rows fetched
   from the select tables. This order may differ on master and slave. We
   therefore mark it as unsafe.
  */
  if (select_field_count > 0 && auto_increment)
  thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_SELECT_AUTOINC);

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
    DBUG_PRINT("info", ("key name: '%s'  type: %d", key->name.str ? key->name.str :
                        "(none)" , key->type));
    if (key->type == KEYTYPE_FOREIGN)
    {
      fk_key_count++;
      if (((Foreign_key *)key)->validate(alter_info->create_list))
        DBUG_RETURN(TRUE);
      Foreign_key *fk_key= (Foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name.str ? fk_key->name.str :
                                     "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(TRUE);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp && key->type != KEYTYPE_SPATIAL)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(TRUE);
    }

    LEX_CSTRING key_name_cstr= {key->name.str, key->name.length};
    if (check_string_char_length(key_name_cstr, "", NAME_CHAR_LEN,
                                 system_charset_info, 1))
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name.str);
      DBUG_RETURN(TRUE);
    }
    key_iterator2.rewind ();
    if (key->type != KEYTYPE_FOREIGN)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.

          KEYTYPE_SPATIAL and KEYTYPE_FULLTEXT cannot be used as
          supporting keys for foreign key constraints even if the
          generated key is prefix of such a key.
        */
        if ((key2->type != KEYTYPE_FOREIGN &&
             key2->type != KEYTYPE_SPATIAL &&
             key2->type != KEYTYPE_FULLTEXT &&
             key2->name.str != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->name.str= ignore_key;
          else
          {
            key2->name.str= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name.str != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->name.str && !tmp_table && (key->type != KEYTYPE_PRIMARY) &&
	!my_strcasecmp(system_charset_info, key->name.str, primary_key_name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
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
    size_t key_length=0;
    Key_part_spec *column;

    if (key->name.str == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name.str == ignore_key);
      if (!key)
	break;
    }

    switch (key->type) {
    case KEYTYPE_MULTIPLE:
	key_info->flags= 0;
	break;
    case KEYTYPE_FULLTEXT:
	key_info->flags= HA_FULLTEXT;
	if ((key_info->parser_name= &key->key_create_info.parser_name)->str)
          key_info->flags|= HA_USES_PARSER;
        else
          key_info->parser_name= 0;
	break;
    case KEYTYPE_SPATIAL:
	key_info->flags= HA_SPATIAL;
	break;
    case KEYTYPE_FOREIGN:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->algorithm= key->key_create_info.algorithm;
    key_info->user_defined_key_parts=(uint8) key->columns.elements;
    key_info->actual_key_parts= key_info->user_defined_key_parts;
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;

    if (key->type == KEYTYPE_FULLTEXT)
    {
      key_info->algorithm= HA_KEY_ALG_FULLTEXT;
      if (!(file->ha_table_flags() & HA_CAN_FULLTEXT))
      {
        if (is_ha_partition_handlerton(file->ht))
        {
          my_message(ER_FULLTEXT_NOT_SUPPORTED_WITH_PARTITIONING,
                     ER(ER_FULLTEXT_NOT_SUPPORTED_WITH_PARTITIONING),
                     MYF(0));
          DBUG_RETURN(TRUE);
        }
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
      if (key_info->user_defined_key_parts != 1)
      {
        my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), 1);
        DBUG_RETURN(TRUE);
      }
    }
    else if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
      if ((key_info->user_defined_key_parts & 1) == 1)
      {
        my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), 1);
        DBUG_RETURN(TRUE);
      }
      /* TODO: To be deleted */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
      DBUG_RETURN(TRUE);
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
    const CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      Key_part_spec *dup_column;

      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(system_charset_info,
			   column->field_name.str,
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
	DBUG_RETURN(TRUE);
      }
      if (sql_field->is_virtual_gcol())
      {
        const char *errmsg= NULL;
        if (key->type == KEYTYPE_FULLTEXT)
          errmsg= "Fulltext index on virtual generated column";
        else if (key->type == KEYTYPE_SPATIAL)
          errmsg= "Spatial index on virtual generated column";
        if (errmsg)
        {
          my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, MYF(0), errmsg);
          DBUG_RETURN(TRUE);
        }
        key_info->flags|= HA_VIRTUAL_GEN_KEY;
        /* Check if the storage engine supports indexes on virtual columns. */
        if (!(file->ha_table_flags() & HA_CAN_INDEX_VIRTUAL_GENERATED_COLUMN))
        {
          my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
                   ha_resolve_storage_engine_name(file->ht),
                   "Index on virtual generated column");
          DBUG_RETURN(TRUE);
        }
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->field_name.str, dup_column->field_name.str))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name.str);
	  DBUG_RETURN(TRUE);
	}
      }
      cols2.rewind();
      if (key->type == KEYTYPE_FULLTEXT)
      {
	if ((sql_field->sql_type != MYSQL_TYPE_STRING &&
	     sql_field->sql_type != MYSQL_TYPE_VARCHAR &&
	     !f_is_blob(sql_field->pack_flag)) ||
	    sql_field->charset == &my_charset_bin ||
	    sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
	    (ft_key_charset && sql_field->charset != ft_key_charset))
	{
	    my_error(ER_BAD_FT_COLUMN, MYF(0), column->field_name.str);
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
	column->length= MY_TEST(f_is_blob(sql_field->pack_flag));
      }
      else
      {
	column->length*= sql_field->charset->mbmaxlen;

        if (key->type == KEYTYPE_SPATIAL)
        {
          if (column->length)
          {
            my_error(ER_WRONG_SUB_KEY, MYF(0));
            DBUG_RETURN(TRUE);
          }
          if (!f_is_geom(sql_field->pack_flag))
          {
            my_error(ER_SPATIAL_MUST_HAVE_GEOM_COL, MYF(0));
            DBUG_RETURN(TRUE);
          }
        }

        // JSON columns cannot be used as keys.
        if (f_is_json(sql_field->pack_flag))
        {
          my_error(ER_JSON_USED_AS_KEY, MYF(0), column->field_name.str);
          DBUG_RETURN(TRUE);
        }

	if (f_is_blob(sql_field->pack_flag) ||
            (f_is_geom(sql_field->pack_flag) && key->type != KEYTYPE_SPATIAL))
	{
	  if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name.str);
	    DBUG_RETURN(TRUE);
	  }
          if (f_is_geom(sql_field->pack_flag) && sql_field->geom_type ==
              Field::GEOM_POINT)
            column->length= MAX_LEN_GEOM_POINT_FIELD;
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name.str);
	    DBUG_RETURN(TRUE);
	  }
	}
	if (key->type == KEYTYPE_SPATIAL)
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
        /*
          Set NO_DEFAULT_VALUE_FLAG for the PRIMARY KEY column if default
          values is not explicitly provided for the column in CREATE TABLE
          statement and it is not an AUTO_INCREMENT field.

          Default values for TIMESTAMP/DATETIME needs special handling as:

         a) If default is explicitly specified (lets say this as case 1) :
              DEFAULT CURRENT_TIMESTAMP
              DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
            MySQL does not set sql_field->def flag , but sets
            Field::TIMESTAMP_DN_FIELD/TIMESTAMP_DNUN_FIELD to the unireg_check.
            These flags are also set during timestamp column promotion (case2)

            When explicit_defaults_for_timestamp is not set, the behavior
            expected in both case1 and case2 is to retain the defaults even
            when the column participates in PRIMARY KEY. When
            explicit_defaults_for_timestamp is set, the promotion logic
            is disabled and the above mentioned flags are not used implicitly.

         b) If explicit_defaults_for_timestamp variable is not set:
             Default value assigned due to first timestamp column promotion is
             retained.
             Default constant value assigned due to implicit promotion of second
             timestamp column is removed.
        */
        if (key->type == KEYTYPE_PRIMARY && !sql_field->def &&
            !(sql_field->flags & AUTO_INCREMENT_FLAG) &&
            !(real_type_with_now_as_default(sql_field->sql_type) &&
              (sql_field->unireg_check == Field::TIMESTAMP_DN_FIELD ||
               sql_field->unireg_check == Field::TIMESTAMP_DNUN_FIELD)))
        {
          sql_field->flags|= NO_DEFAULT_VALUE_FLAG;
          sql_field->pack_flag|= FIELDFLAG_NO_DEFAULT;
        }
        /*
          Emitting error when field is a part of primary key and is
          explicitly requested to be NULL by the user.
        */
        if ((sql_field->flags & EXPLICIT_NULL_FLAG) &&
            (key->type == KEYTYPE_PRIMARY))
        {
          my_error(ER_PRIMARY_CANT_HAVE_NULL, MYF(0));
          DBUG_RETURN(true);
        }
        // Primary key on virtual generated column is not supported.
        if (key->type == KEYTYPE_PRIMARY &&
            !sql_field->stored_in_db)
        {
          /* Primary key fields must always be physically stored. */
          my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, MYF(0),
                   "Defining a virtual generated column as primary key");
          DBUG_RETURN(TRUE);
        }

	if (!(sql_field->flags & NOT_NULL_FLAG))
	{
	  if (key->type == KEYTYPE_PRIMARY)
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
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name.str);
              DBUG_RETURN(TRUE);
            }
            if (key->type == KEYTYPE_SPATIAL)
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
      size_t key_part_length= sql_field->key_length;

      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
          key_part_length= column->length;
          /*
            There is a possibility that the given prefix length is less
            than the engine max key part length, but still greater
            than the BLOB field max size. We handle this case
            using the max_field_size variable below.
          */
          size_t max_field_size= blob_length_by_type(sql_field->sql_type);
	  if (key_part_length > max_field_size ||
              key_part_length > max_key_length ||
	      key_part_length > file->max_key_part_length(create_info))
	  {
            // Given prefix length is too large, adjust it.
	    key_part_length= min(max_key_length,
                                 file->max_key_part_length(create_info));
	    if (max_field_size)
              key_part_length= min(key_part_length, max_field_size);
	    if (key->type == KEYTYPE_MULTIPLE)
	    {
	      /* not a critical problem */
	      push_warning_printf(thd, Sql_condition::SL_WARNING,
		                  ER_TOO_LONG_KEY, ER(ER_TOO_LONG_KEY),
                                  key_part_length);
              /* Align key length to multibyte char boundary */
              key_part_length-= key_part_length % sql_field->charset->mbmaxlen;
              /*
               If SQL_MODE is STRICT, then report error, else report warning
               and continue execution.
              */
              if (thd->is_error())
                DBUG_RETURN(true);
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY, MYF(0), key_part_length);
	      DBUG_RETURN(TRUE);
	    }
	  }
	}
        // Catch invalid use of partial keys 
	else if (!f_is_geom(sql_field->pack_flag) &&
                 // is the key partial? 
                 column->length != key_part_length &&
                 // is prefix length bigger than field length? 
                 (column->length > key_part_length ||
                  // can the field have a partial key? 
                  !Field::type_can_have_key_part (sql_field->sql_type) ||
                  // a packed field can't be used in a partial key
                  f_is_packed(sql_field->pack_flag) ||
                  // does the storage engine allow prefixed search?
                  ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
                   // and is this a 'unique' key?
                   (key_info->flags & HA_NOSAME))))
        {         
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  DBUG_RETURN(TRUE);
	}
	else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  key_part_length= column->length;
      }
      else if (key_part_length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name.str);
	  DBUG_RETURN(TRUE);
      }
      if (key_part_length > file->max_key_part_length(create_info) &&
          key->type != KEYTYPE_FULLTEXT)
      {
        key_part_length= file->max_key_part_length(create_info);
	if (key->type == KEYTYPE_MULTIPLE)
	{
	  /* not a critical problem */
	  push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_TOO_LONG_KEY, ER(ER_TOO_LONG_KEY),
                              key_part_length);
          /* Align key length to multibyte char boundary */
          key_part_length-= key_part_length % sql_field->charset->mbmaxlen;
          /*
            If SQL_MODE is STRICT, then report error, else report warning
            and continue execution.
          */
          if (thd->is_error())
            DBUG_RETURN(true);
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY, MYF(0), key_part_length);
	  DBUG_RETURN(TRUE);
	}
      }
      key_part_info->length= (uint16) key_part_length;
      /* Use packed keys for long strings on the first column */
      if ((create_info->db_type->flags & HTON_SUPPORTS_PACKED_KEYS) &&
          !((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
          !((create_info->table_options & HA_OPTION_NO_PACK_KEYS)) &&
	  (key_part_length >= KEY_DEFAULT_PACK_LENGTH &&
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

      /*
         Check if the key segment is partial, set the key flag
         accordingly. The key segment for a POINT column is NOT considered
         partial if key_length==MAX_LEN_GEOM_POINT_FIELD.
         Note that fulltext indexes ignores prefixes.
      */
      if (key->type != KEYTYPE_FULLTEXT &&
          key_part_length != sql_field->key_length &&
          !(sql_field->sql_type == MYSQL_TYPE_GEOMETRY &&
            sql_field->geom_type == Field::GEOM_POINT &&
            key_part_length == MAX_LEN_GEOM_POINT_FIELD))
        {
          key_info->flags|= HA_KEY_HAS_PART_KEY_SEG;
        }

      key_length+= key_part_length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == KEYTYPE_PRIMARY)
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
	else if (!(key_name= key->name.str))
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
    key_info->actual_flags= key_info->flags;
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(TRUE);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    if (key_length > max_key_length && key->type != KEYTYPE_FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      if (thd->is_error())  // May be silenced - see Bug#20629014
        DBUG_RETURN(true);
    }
    if (validate_comment_length(thd, key->key_create_info.comment.str,
                                &key->key_create_info.comment.length,
                                INDEX_COMMENT_MAXLEN,
                                ER_TOO_LONG_INDEX_COMMENT,
                                key_info->name))
       DBUG_RETURN(true);
    key_info->comment.length= key->key_create_info.comment.length;
    if (key_info->comment.length > 0)
    {
      key_info->flags|= HA_USES_COMMENT;
      key_info->comment.str= key->key_create_info.comment.str;
    }

    // Check if a duplicate index is defined.
  if (check_duplicate_key(thd, error_schema_name, error_table_name,
                          key, key_info, alter_info))
      DBUG_RETURN(true);

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

  /*
    Check if  STRICT SQL mode is active and server is not started with
    --explicit-defaults-for-timestamp. Below check was added to prevent implicit
    default 0 value of timestamp. When explicit-defaults-for-timestamp server
    option is removed, whole set of check can be removed.
  */
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE &&
      !thd->variables.explicit_defaults_for_timestamp)
  {
    while ((sql_field=it++))
    {
      Field::utype type= (Field::utype) MTYP_TYPENR(sql_field->unireg_check);

      if (!sql_field->def &&
          !sql_field->gcol_info &&
          is_timestamp_type(sql_field->sql_type) &&
          (sql_field->flags & NOT_NULL_FLAG) &&
          (type == Field::NONE || type == Field::TIMESTAMP_UN_FIELD))
      {
        /*
          An error should be reported if:
            - there is no explicit DEFAULT clause (default column value);
            - this is a TIMESTAMP column;
            - the column is not NULL;
            - this is not the DEFAULT CURRENT_TIMESTAMP column.
          And from checks before while loop,
            - STRICT SQL mode is active;
            - server is not started with --explicit-defaults-for-timestamp

          In other words, an error should be reported if
            - STRICT SQL mode is active;
            - the column definition is equivalent to
              'column_name TIMESTAMP DEFAULT 0'.
        */

        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(TRUE);
      }
    }
  }

  {
    LEX_STRING*	compress = &create_info->compress;

    if (compress->length != 0 &&
	compress->length > TABLE_COMMENT_MAXLEN &&
	system_charset_info->cset->charpos(system_charset_info,
					   compress->str,
					   compress->str + compress->length,
					   TABLE_COMMENT_MAXLEN)
	< compress->length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
	       compress->str, "COMPRESSION", TABLE_COMMENT_MAXLEN);
      DBUG_RETURN(TRUE);
    }
  }

  {
    LEX_STRING*	encrypt_type = &create_info->encrypt_type;

    if (encrypt_type->length != 0 &&
	encrypt_type->length > TABLE_COMMENT_MAXLEN &&
	system_charset_info->cset->charpos(system_charset_info,
					   encrypt_type->str,
					   encrypt_type->str
					   + encrypt_type->length,
					   TABLE_COMMENT_MAXLEN)
	< encrypt_type->length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
	       encrypt_type->str, "ENCRYPTION", TABLE_COMMENT_MAXLEN);
      DBUG_RETURN(TRUE);
    }
  }

  DBUG_RETURN(FALSE);
}

/**
  @brief check comment length of table, column, index and partition

  @details If comment length is more than the standard length
    truncate it and store the comment length upto the standard
    comment length size

  @param          thd             Thread handle
  @param          comment_str     Comment string
  @param[in,out]  comment_len     Comment length
  @param          max_len         Maximum allowed comment length
  @param          err_code        Error message
  @param          comment_name    Type of comment

  @return Operation status
    @retval       true            Error found
    @retval       false           On success
*/

bool validate_comment_length(THD *thd, const char *comment_str,
                             size_t *comment_len, uint max_len,
                             uint err_code, const char *comment_name)
{
  size_t length= 0;
  DBUG_ENTER("validate_comment_length");
  size_t tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                                     comment_str,
                                                     comment_str +
                                                     *comment_len,
                                                     max_len);
  if (tmp_len < *comment_len)
  {
    if (thd->is_strict_mode())
    {
      my_error(err_code, MYF(0),
               comment_name, static_cast<ulong>(max_len));
      DBUG_RETURN(true);
    }
    char warn_buff[MYSQL_ERRMSG_SIZE];
    length= my_snprintf(warn_buff, sizeof(warn_buff), ER(err_code),
                        comment_name, static_cast<ulong>(max_len));
    /* do not push duplicate warnings */
    if (!thd->get_stmt_da()->has_sql_condition(warn_buff, length)) 
      push_warning(thd, Sql_condition::SL_WARNING,
                   err_code, warn_buff);
    *comment_len= tmp_len;
  }
  DBUG_RETURN(false);
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

    if (sql_field->def || thd->is_strict_mode())
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
    push_warning(thd, Sql_condition::SL_NOTE, ER_AUTO_CONVERT,
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

static void sp_prepare_create_field(THD *thd, Create_field *sql_field)
{
  if (sql_field->sql_type == MYSQL_TYPE_SET ||
      sql_field->sql_type == MYSQL_TYPE_ENUM)
  {
    size_t field_length, dummy;
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


/**
  Create a table

  @param thd                 Thread object
  @param db                  Database
  @param table_name          Table name
  @param error_table_name    The real table name in case table_name is a temporary
                             table (ALTER). Only used for error messages.
  @param path                Path to table (i.e. to its .FRM file without
                             the extension).
  @param create_info         Create information (like MAX_ROWS)
  @param alter_info          Description of fields and keys for new table
  @param internal_tmp_table  Set to true if this is an internal temporary table
                             (From ALTER TABLE)
  @param select_field_count  Number of fields coming from SELECT part of
                             CREATE TABLE ... SELECT statement. Must be zero
                             for standard create of table.
  @param no_ha_table         Indicates that only .FRM file (and PAR file if table
                             is partitioned) needs to be created and not a table
                             in the storage engine.
  @param[out] is_trans       Identifies the type of engine where the table
                             was created: either trans or non-trans.
  @param[out] key_info       Array of KEY objects describing keys in table
                             which was created.
  @param[out] key_count      Number of keys in table which was created.

  If one creates a temporary table, this is automatically opened

  Note that this function assumes that caller already have taken
  exclusive metadata lock on table being created or used some other
  way to ensure that concurrent operations won't intervene.
  mysql_create_table() is a wrapper that can be used for this.

  @retval false OK
  @retval true  error
*/

static
bool create_table_impl(THD *thd,
                       const char *db, const char *table_name,
                       const char *error_table_name,
                       const char *path,
                       HA_CREATE_INFO *create_info,
                       Alter_info *alter_info,
                       bool internal_tmp_table,
                       uint select_field_count,
                       bool no_ha_table,
                       bool *is_trans,
                       KEY **key_info,
                       uint *key_count)
{
  const char	*alias;
  uint		db_options;
  handler	*file;
  bool		error= TRUE;
  bool		is_whitelisted_table;
  bool		prepare_error;
  Key_length_error_handler error_handler;

  DBUG_ENTER("create_table_impl");
  DBUG_PRINT("enter", ("db: '%s'  table: '%s'  tmp: %d",
                       db, table_name, internal_tmp_table));


  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (check_engine(thd, db, table_name, create_info))
    DBUG_RETURN(TRUE);

  // Check if new table creation is disallowed by the storage engine.
  if (!internal_tmp_table &&
      ha_is_storage_engine_disabled(create_info->db_type))
  {
    /*
      If table creation is disabled for the engine then substitute the engine
      for the table with the default engine only if sql mode
      NO_ENGINE_SUBSTITUTION is disabled.
    */
    handlerton *new_engine= NULL;
    if (is_engine_substitution_allowed(thd))
      new_engine= ha_default_handlerton(thd);

    /*
      Proceed with the engine substitution only if,
      1. The disabled engine and the default engine are not the same.
      2. The default engine is not in the disabled engines list.
      else report an error.
    */
    if (new_engine && create_info->db_type &&
        new_engine != create_info->db_type &&
        !ha_is_storage_engine_disabled(new_engine))
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_DISABLED_STORAGE_ENGINE,
                          ER(ER_DISABLED_STORAGE_ENGINE),
                          ha_resolve_storage_engine_name(create_info->db_type));

      create_info->db_type= new_engine;

      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_USING_OTHER_HANDLER,
                          ER(ER_WARN_USING_OTHER_HANDLER),
                          ha_resolve_storage_engine_name(create_info->db_type),
                          table_name);
    }
    else
    {
      my_error(ER_DISABLED_STORAGE_ENGINE, MYF(0),
               ha_resolve_storage_engine_name(create_info->db_type));
      DBUG_RETURN(true);
    }
  }

  set_table_default_charset(thd, create_info, (char*) db);

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
  partition_info *part_info= thd->work_part_info;

  if (!part_info && create_info->db_type->partition_flags &&
      (create_info->db_type->partition_flags() & HA_USE_AUTO_PARTITION))
  {
    Partition_handler *part_handler= file->get_partition_handler();
    DBUG_ASSERT(part_handler != NULL);

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
    part_handler->set_auto_partitions(part_info);
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
    */
    Key *key;
    handlerton *part_engine_type= create_info->db_type;
    char *part_syntax_buf;
    uint syntax_len;
    handlerton *engine_type;
    List_iterator<partition_element> part_it(part_info->partitions);
    partition_element *part_elem;

    while ((part_elem= part_it++))
    {
      if (part_elem->part_comment)
      {
        size_t comment_len= strlen(part_elem->part_comment);
        if (validate_comment_length(thd, part_elem->part_comment,
                                     &comment_len,
                                     TABLE_PARTITION_COMMENT_MAXLEN,
                                     ER_TOO_LONG_TABLE_PARTITION_COMMENT,
                                     part_elem->partition_name))
          DBUG_RETURN(true);
        part_elem->part_comment[comment_len]= '\0';
      }
      if (part_elem->subpartitions.elements)
      {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        partition_element *subpart_elem;
        while ((subpart_elem= sub_it++))
        {
          if (subpart_elem->part_comment)
          {
            size_t comment_len= strlen(subpart_elem->part_comment);
            if (validate_comment_length(thd, subpart_elem->part_comment,
                                         &comment_len,
                                         TABLE_PARTITION_COMMENT_MAXLEN,
                                         ER_TOO_LONG_TABLE_PARTITION_COMMENT,
                                         subpart_elem->partition_name))
              DBUG_RETURN(true);
            subpart_elem->part_comment[comment_len]= '\0';
          }
        }
      }
    } 
    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    {
      my_error(ER_PARTITION_NO_TEMPORARY, MYF(0));
      goto err;
    }
    if (is_ha_partition_handlerton(part_engine_type) &&
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
                                        create_info, FALSE))
      goto err;
    part_info->default_engine_type= engine_type;

    {
      /*
        We reverse the partitioning parser and generate a standard format
        for syntax stored in frm file.
      */
      sql_mode_t sql_mode_backup= thd->variables.sql_mode;
      thd->variables.sql_mode&= ~(MODE_ANSI_QUOTES);
      part_syntax_buf= generate_partition_syntax(part_info,
                                                 &syntax_len,
                                                 TRUE, TRUE,
                                                 create_info,
                                                 alter_info,
                                                 NULL);
      thd->variables.sql_mode= sql_mode_backup;
      if (part_syntax_buf == NULL)
      {
        goto err;
      }
    }
    part_info->part_info_string= part_syntax_buf;
    part_info->part_info_len= syntax_len;
    if (!engine_type->partition_flags ||
        is_ha_partition_handlerton(create_info->db_type))
    {
      /*
        The handler assigned to the table cannot handle partitioning.
        Assign the partition handler as the handler of the table.
      */
      DBUG_PRINT("info", ("db_type: %s",
                        ha_resolve_storage_engine_name(create_info->db_type)));
      LEX_CSTRING engine_name= {C_STRING_WITH_LEN("partition")};
      plugin_ref plugin= ha_resolve_by_name_raw(thd, engine_name);
      if (!plugin)
      {
        goto no_partitioning;
      }
      create_info->db_type= plugin_data<handlerton*>(plugin);
      DBUG_ASSERT(create_info->db_type->flags & HTON_NOT_USER_SELECTABLE);
      delete file;
      if (!(file= get_new_handler(NULL, thd->mem_root, create_info->db_type)))
      {
        mem_alloc_error(sizeof(handler));
        DBUG_RETURN(true);
      }
      if (file->ht != create_info->db_type)
      {
	DBUG_ASSERT(0);
        goto no_partitioning;
      }
      Partition_handler *part_handler= file->get_partition_handler();
      if (!part_handler)
      {
        DBUG_ASSERT(0);
        goto no_partitioning;
      }
      part_handler->set_part_info(part_info, false);

      /*
        Re-run the initialize_partition after setting the part_info,
        to create the partition's handlers.
      */
      if (part_handler->initialize_partition(thd->mem_root))
        goto no_partitioning;
      /* Re-read the table flags */
      file->init();

      /*
        If we have default number of partitions or subpartitions we
        might require to set-up the part_info object such that it
        creates a proper .par file. The current part_info object is
        only used to create the frm-file and .par-file.
      */
      if (part_info->use_default_num_partitions &&
          part_info->num_parts &&
          (int)part_info->num_parts !=
          part_handler->get_default_num_partitions(create_info))
      {
        uint i;
        List_iterator<partition_element> part_it(part_info->partitions);
        part_it++;
        DBUG_ASSERT(thd->lex->sql_command != SQLCOM_CREATE_TABLE);
        for (i= 1; i < part_info->partitions.elements; i++)
          (part_it++)->part_state= PART_TO_BE_DROPPED;
      }
      else if (part_info->is_sub_partitioned() &&
               part_info->use_default_num_subpartitions &&
               part_info->num_subparts &&
               (int)part_info->num_subparts !=
                 part_handler->get_default_num_partitions(create_info))
      {
        DBUG_ASSERT(thd->lex->sql_command != SQLCOM_CREATE_TABLE);
        part_info->num_subparts=
          part_handler->get_default_num_partitions(create_info);
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
      create_info->db_type= engine_type;
    }
    /*
      Unless table's storage engine supports partitioning natively
      don't allow foreign keys on partitioned tables (they won't
      work work even with InnoDB beneath of partitioning engine).
      If storage engine handles partitioning natively (like NDB)
      foreign keys support is possible, so we let the engine decide.
    */
    if (is_ha_partition_handlerton(create_info->db_type))
    {
      List_iterator_fast<Key> key_iterator(alter_info->key_list);
      while ((key= key_iterator++))
      {
        if (key->type == KEYTYPE_FOREIGN)
        {
          my_error(ER_FOREIGN_KEY_ON_PARTITIONED, MYF(0));
          goto err;
        }
      }
    }
  }

  /*
    System tables residing in mysql database and created
    by innodb engine could be created with any supported
    innodb page size ( 4k,8k,16K).  We have a index size
    limit depending upon the page size, but for system
    tables which are whitelisted we can skip this check,
    since the innodb engine ensures that the index size
    will be supported.
  */
  is_whitelisted_table = (file->ht->db_type == DB_TYPE_INNODB) ?
    ha_is_supported_system_table(file->ht, db, error_table_name) : false;
  if (is_whitelisted_table) thd->push_internal_handler(&error_handler);

  prepare_error= mysql_prepare_create_table(thd, db, error_table_name,
					    create_info, alter_info,
					    internal_tmp_table,
					    &db_options, file,
					    key_info, key_count,
					    select_field_count);

  if (is_whitelisted_table) thd->pop_internal_handler();
  if (prepare_error) goto err;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      find_temporary_table(thd, db, table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          alias);
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    goto err;
  }

  if (!internal_tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    char frm_name[FN_REFLEN+1];
    strxnmov(frm_name, sizeof(frm_name) - 1, path, reg_ext, NullS);

    if (!access(frm_name, F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto err;
    }
    /*
      We don't assert here, but check the result, because the table could be
      in the table definition cache and in the same time the .frm could be
      missing from the disk, in case of manual intervention which deletes
      the .frm file. The user has to use FLUSH TABLES; to clear the cache.
      Then she could create the table. This case is pretty obscure and
      therefore we don't introduce a new error message only for it.
    */
    mysql_mutex_lock(&LOCK_open);
    if (get_cached_table_share(thd, db, table_name))
    {
      mysql_mutex_unlock(&LOCK_open);
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);
      goto err;
    }
    mysql_mutex_unlock(&LOCK_open);
  }

  /*
    Check that table with given name does not already
    exist in any storage engine. In such a case it should
    be discovered and the error ER_TABLE_EXISTS_ERROR be returned
    unless user specified CREATE TABLE IF EXISTS
    An exclusive metadata lock ensures that no
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
        goto err;
        break;
      default:
        DBUG_PRINT("info", ("error: %u from storage engine", retcode));
        my_error(retcode, MYF(0),table_name);
        goto err;
    }
  }

  THD_STAGE_INFO(thd, stage_creating_table);

  {
    size_t dirlen;
    char   dirpath[FN_REFLEN];

    /*
      data_file_name and index_file_name include the table name without
      extension. Mostly this does not refer to an existing file. When
      comparing data_file_name or index_file_name against the data
      directory, we try to resolve all symbolic links. On some systems,
      we use realpath(3) for the resolution. This returns ENOENT if the
      resolved path does not refer to an existing file. my_realpath()
      does then copy the requested path verbatim, without symlink
      resolution. Thereafter the comparison can fail even if the
      requested path is within the data directory. E.g. if symlinks to
      another file system are used. To make realpath(3) return the
      resolved path, we strip the table name and compare the directory
      path only. If the directory doesn't exist either, table creation
      will fail anyway.
    */
    if (create_info->data_file_name)
    {
      dirname_part(dirpath, create_info->data_file_name, &dirlen);
      if (test_if_data_home_dir(dirpath))
      {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "DATA DIRECTORY");
        goto err;
      }
    }
    if (create_info->index_file_name)
    {
      dirname_part(dirpath, create_info->index_file_name, &dirlen);
      if (test_if_data_home_dir(dirpath))
      {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "INDEX DIRECTORY");
        goto err;
      }
    }
  }

  if (check_partition_dirs(thd->lex->part_info))
  {
    goto err;
  }

  if (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)
  {
    if (create_info->data_file_name)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "DATA DIRECTORY");
    if (create_info->index_file_name)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "INDEX DIRECTORY");
    create_info->data_file_name= create_info->index_file_name= 0;
  }
  create_info->table_options=db_options;

  /*
    Create .FRM (and .PAR file for partitioned table).
    If "no_ha_table" is false also create table in storage engine.
  */
  if (rea_create_table(thd, path, db, table_name,
                       create_info, alter_info->create_list,
                       *key_count, *key_info, file, no_ha_table))
    goto err;

  if (!no_ha_table && create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /*
      Open a table (skipping table cache) and add it into
      THD::temporary_tables list.
    */

    TABLE *table= open_table_uncached(thd, path, db, table_name, true, true);

    if (!table)
    {
      (void) rm_temporary_table(create_info->db_type, path);
      goto err;
    }

    if (is_trans != NULL)
      *is_trans= table->file->has_transactions();

    thd->thread_specific_used= TRUE;
  }
  else if (part_info && no_ha_table)
  {
    /*
      For partitioned tables we can't find some problems with table
      until table is opened. Therefore in order to disallow creation
      of corrupted tables we have to try to open table as the part
      of its creation process.
      In cases when both .FRM and SE part of table are created table
      is implicitly open in ha_create_table() call.
      In cases when we create .FRM without SE part we have to open
      table explicitly.
    */
    TABLE table;
    TABLE_SHARE share;

    init_tmp_table_share(thd, &share, db, 0, table_name, path);

    bool result= (open_table_def(thd, &share, 0) ||
                  open_table_from_share(thd, &share, "", 0, (uint) READ_ALL,
                                        0, &table, true));
    /*
      Assert that the change list is empty as no partition function currently
      needs to modify item tree. May need call THD::rollback_item_tree_changes
      later before calling closefrm if the change list is not empty.
    */
    DBUG_ASSERT(thd->change_list.is_empty());
    if (!result)
      (void) closefrm(&table, 0);

    free_table_share(&share);

    if (result)
    {
      char frm_name[FN_REFLEN + 1];
      strxnmov(frm_name, sizeof(frm_name) - 1, path, reg_ext, NullS);
      (void) mysql_file_delete(key_file_frm, frm_name, MYF(0));
      (void) file->ha_create_handler_files(path, NULL, CHF_DELETE_FLAG,
                                           create_info);
      goto err;
    }
  }

  error= FALSE;
err:
  THD_STAGE_INFO(thd, stage_after_create);
  delete file;
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      thd->in_multi_stmt_transaction_mode() && !error)
  {
    /*
      When autocommit is disabled, creating temporary table sets this
      flag to start transaction in any case (regardless of binlog=on/off,
      binlog format and transactional/non-transactional engine) to make
      behavior consistent.
    */
    thd->server_status|= SERVER_STATUS_IN_TRANS;
  }
  DBUG_RETURN(error);

warn:
  error= FALSE;
  push_warning_printf(thd, Sql_condition::SL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  goto err;
no_partitioning:
  my_error(ER_FEATURE_NOT_AVAILABLE, MYF(0), "partitioning",
           "--skip-partition", "-DWITH_PARTITION_STORAGE_ENGINE=1");
  goto err;
}


/**
  Simple wrapper around create_table_impl() to be used
  in various version of CREATE TABLE statement.
*/
bool mysql_create_table_no_lock(THD *thd,
                                const char *db, const char *table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                uint select_field_count,
                                bool *is_trans)
{
  KEY *not_used_1;
  uint not_used_2;
  char path[FN_REFLEN + 1];

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    build_tmptable_filename(thd, path, sizeof(path));
  else
  {
    bool was_truncated;
    const char *alias= table_case_name(create_info, table_name);
    build_table_filename(path, sizeof(path) - 1 - reg_ext_length, 
                         db, alias, "", 0, &was_truncated);
    // Check truncation, will lead to overflow when adding extension
    if (was_truncated)
    {
      my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(path) - 1, path);
      return true;
    }
  }

  return create_table_impl(thd, db, table_name, table_name, path, create_info,
                           alter_info, false, select_field_count, false,
                           is_trans, &not_used_1, &not_used_2);
}


/**
  Implementation of SQLCOM_CREATE_TABLE.

  Take the metadata locks (including a shared lock on the affected
  schema) and create the table. Is written to be called from
  mysql_execute_command(), to which it delegates the common parts
  with other commands (i.e. implicit commit before and after,
  close of thread tables.
*/

bool mysql_create_table(THD *thd, TABLE_LIST *create_table,
                        HA_CREATE_INFO *create_info,
                        Alter_info *alter_info)
{
  bool result;
  bool is_trans= FALSE;
  uint not_used;
  DBUG_ENTER("mysql_create_table");

  /*
    Open or obtain "X" MDL lock on the table being created.
    To check the existence of table, lock of type "S" is obtained on the table
    and then it is upgraded to "X" if table does not exists.
  */
  if (open_tables(thd, &thd->lex->query_tables, &not_used, 0))
  {
    result= TRUE;
    goto end;
  }

  /* Got lock. */
  DEBUG_SYNC(thd, "locked_table_name");

  /*
    Promote first timestamp column, when explicit_defaults_for_timestamp
    is not set
  */
  if (!thd->variables.explicit_defaults_for_timestamp)
    promote_first_timestamp_column(&alter_info->create_list);

  result= mysql_create_table_no_lock(thd, create_table->db,
                                     create_table->table_name, create_info,
                                     alter_info, 0, &is_trans);
  /*
    Don't write statement if:
    - Table creation has failed
    - Row-based logging is used and we are creating a temporary table
    Otherwise, the statement shall be binlogged.
  */
  if (!result)
  {
    /*
      CREATE TEMPORARY TABLE doesn't terminate a transaction. Calling
      stmt.mark_created_temp_table() guarantees the transaction can be binlogged
      correctly.
    */
    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
      thd->get_transaction()->mark_created_temp_table(Transaction_ctx::STMT);

    if (!thd->is_current_stmt_binlog_format_row() ||
        (thd->is_current_stmt_binlog_format_row() &&
         !(create_info->options & HA_LEX_CREATE_TMP_TABLE)))
    {
      thd->add_to_binlog_accessed_dbs(create_table->db);
      result= write_bin_log(thd, true,
                            thd->query().str, thd->query().length, is_trans);
    }
  }

end:
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


/**
  Rename a table.

  @param base      The handlerton handle.
  @param old_db    The old database name.
  @param old_name  The old table name.
  @param new_db    The new database name.
  @param new_name  The new table name.
  @param flags     flags
                   FN_FROM_IS_TMP old_name is temporary.
                   FN_TO_IS_TMP   new_name is temporary.
                   NO_FRM_RENAME  Don't rename the FRM file
                                  but only the table in the storage engine.
                   NO_HA_TABLE    Don't rename table in engine.
                   NO_FK_CHECKS   Don't check FK constraints during rename.

  @return false    OK
  @return true     Error
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
  ulonglong save_bits= thd->variables.option_bits;
  size_t length;
  bool was_truncated;
  DBUG_ENTER("mysql_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_db, old_name, new_db, new_name));

  // Temporarily disable foreign key checks
  if (flags & NO_FK_CHECKS) 
    thd->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;

  file= (base == NULL ? 0 :
         get_new_handler((TABLE_SHARE*) 0, thd->mem_root, base));

  build_table_filename(from, sizeof(from) - 1, old_db, old_name, "",
                       flags & FN_FROM_IS_TMP);
  length= build_table_filename(to, sizeof(to) - 1, new_db, new_name, "",
                               flags & FN_TO_IS_TMP, &was_truncated);
  // Check if we hit FN_REFLEN bytes along with file extension.
  if (was_truncated || length+reg_ext_length > FN_REFLEN)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(to)-1, to);
    DBUG_RETURN(TRUE);
  }

  /*
    If lower_case_table_names == 2 (case-preserving but case-insensitive
    file system) and the storage is not HA_FILE_BASED, we need to provide
    a lowercase file name, but we leave the .frm in mixed case.
   */
  if (lower_case_table_names == 2 && file &&
      !(file->ha_table_flags() & HA_FILE_BASED))
  {
    my_stpcpy(tmp_name, old_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_from, sizeof(lc_from) - 1, old_db, tmp_name, "",
                         flags & FN_FROM_IS_TMP);
    from_base= lc_from;

    my_stpcpy(tmp_name, new_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_to, sizeof(lc_to) - 1, new_db, tmp_name, "",
                         flags & FN_TO_IS_TMP);
    to_base= lc_to;
  }

  if (flags & NO_HA_TABLE)
  {
    if (rename_file_ext(from,to,reg_ext))
      error= my_errno();
    (void) file->ha_create_handler_files(to, from, CHF_RENAME_FLAG, NULL);
  }
  else if (!file || !(error=file->ha_rename_table(from_base, to_base)))
  {
    if (!(flags & NO_FRM_RENAME) && rename_file_ext(from,to,reg_ext))
    {
      error=my_errno();
      /* Restore old file name */
      if (file)
        file->ha_rename_table(to_base, from_base);
    }
  }
  delete file;
  if (error == HA_ERR_WRONG_COMMAND)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER TABLE");
  else if (error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_RENAME, MYF(0), from, to,
             error, my_strerror(errbuf, sizeof(errbuf), error));
  }

#ifdef HAVE_PSI_TABLE_INTERFACE
  /*
    Remove the old table share from the pfs table share array. The new table
    share will be created when the renamed table is first accessed.
   */
  if (likely(error == 0))
  {
    my_bool temp_table= (my_bool)is_prefix(old_name, tmp_file_prefix);
    PSI_TABLE_CALL(drop_table_share)
      (temp_table, old_db, static_cast<int>(strlen(old_db)),
       old_name, static_cast<int>(strlen(old_name)));
  }
#endif

  // Restore options bits to the original value
  thd->variables.option_bits= save_bits;

  DBUG_RETURN(error != 0);
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
  HA_CREATE_INFO local_create_info;
  Alter_info local_alter_info;
  Alter_table_ctx local_alter_ctx; // Not used
  bool res= TRUE;
  bool is_trans= FALSE;
  uint not_used;
  Tablespace_hash_set tablespace_set(PSI_INSTRUMENT_ME);

  DBUG_ENTER("mysql_create_like_table");

  /*
    We the open source table to get its description in HA_CREATE_INFO
    and Alter_info objects. This also acquires a shared metadata lock
    on this table which ensures that no concurrent DDL operation will
    mess with it.
    Also in case when we create non-temporary table open_tables()
    call obtains an exclusive metadata lock on target table ensuring
    that we can safely perform table creation.
    Thus by holding both these locks we ensure that our statement is
    properly isolated from all concurrent operations which matter.
  */
  if (open_tables(thd, &thd->lex->query_tables, &not_used, 0))
    goto err;
  src_table->table->use_all_columns();

  DEBUG_SYNC(thd, "create_table_like_after_open");

  /*
    During open_tables(), the target tablespace name(s) for a table being
    created or altered should be locked. However, for 'CREATE TABLE ... LIKE',
    the source table is not being created, yet its tablespace name should be
    locked since it is used as the target tablespace name for the table being
    created. The  target tablespace name cannot be set before open_tables()
    (which is how we handle this for e.g. CREATE TABLE ... TABLESPACE ...'),
    since before open_tables(), the source table itself is not locked, which
    means that a DDL operation may sneak in and change the tablespace of the
    source table *after* we retrieved it from the .FRM file of the source
    table, and *before* the source table itself is locked. Thus, we lock the
    target tablespace here in a separate mdl lock acquisition phase after
    open_tables(). Since the table is already opened (and locked), we retrieve
    the tablespace name from the table share instead of reading it from the
    .FRM file.
  */

  // Add the tablespace name, if used.
  if (src_table->table->s->tablespace &&
      strlen(src_table->table->s->tablespace) > 0)
  {
    DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                  src_table->db, src_table->table_name, MDL_SHARED));

    if (tablespace_set.insert(
          const_cast<char*>(src_table->table->s->tablespace)))
      DBUG_RETURN(true);
  }

  // Add tablespace names used under partition/subpartition definitions.
  if (fill_partition_tablespace_names(
        src_table->table->part_info, &tablespace_set))
    DBUG_RETURN(true);

  /*
    After we have identified the tablespace names, we iterate
    over the names and acquire MDL lock for each of them.
  */
  if (lock_tablespace_names(thd,
                            &tablespace_set,
                            thd->variables.lock_wait_timeout))
  {
    DBUG_RETURN(true);
  }

  /* Fill HA_CREATE_INFO and Alter_info with description of source table. */
  local_create_info.db_type= src_table->table->s->db_type();
  local_create_info.row_type= src_table->table->s->row_type;
  if (mysql_prepare_alter_table(thd, src_table->table, &local_create_info,
                                &local_alter_info, &local_alter_ctx))
    goto err;
  /* Partition info is not handled by mysql_prepare_alter_table() call. */
  if (src_table->table->part_info)
    thd->work_part_info= src_table->table->part_info->get_clone();

  /*
    Adjust description of source table before using it for creation of
    target table.

    Similarly to SHOW CREATE TABLE we ignore MAX_ROWS attribute of
    temporary table which represents I_S table.
  */
  if (src_table->schema_table)
    local_create_info.max_rows= 0;
  /* Set IF NOT EXISTS option as in the CREATE TABLE LIKE statement. */
  local_create_info.options|= create_info->options&HA_LEX_CREATE_IF_NOT_EXISTS;
  /* Replace type of source table with one specified in the statement. */
  local_create_info.options&= ~HA_LEX_CREATE_TMP_TABLE;
  local_create_info.options|= create_info->options & HA_LEX_CREATE_TMP_TABLE;
  /* Reset auto-increment counter for the new table. */
  local_create_info.auto_increment_value= 0;
  /*
    Do not inherit values of DATA and INDEX DIRECTORY options from
    the original table. This is documented behavior.
  */
  local_create_info.data_file_name= local_create_info.index_file_name= NULL;
  local_create_info.alias= create_info->alias;

  if ((res= mysql_create_table_no_lock(thd, table->db, table->table_name,
                                       &local_create_info, &local_alter_info,
                                       0, &is_trans)))
    goto err;

  /*
    Ensure that table or view does not exist and we have an exclusive lock on
    target table if we are creating non-temporary table. In LOCK TABLES mode
    the only way the table is locked, is if it already exists (since you cannot
    LOCK TABLE a non-existing table). And the only way we then can end up here
    is if IF EXISTS was used.
  */
  DBUG_ASSERT(table->table || table->is_view() ||
              (create_info->options & HA_LEX_CREATE_TMP_TABLE) ||
              (thd->locked_tables_mode != LTM_LOCK_TABLES &&
               thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                  table->db, table->table_name,
                                  MDL_EXCLUSIVE)) ||
              (thd->locked_tables_mode == LTM_LOCK_TABLES &&
               (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS) &&
               thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                  table->db, table->table_name,
                                  MDL_SHARED_NO_WRITE)));

  DEBUG_SYNC(thd, "create_table_like_before_binlog");

  /*
    CREATE TEMPORARY TABLE doesn't terminate a transaction. Calling
    stmt.mark_created_temp_table() guarantees the transaction can be binlogged
    correctly.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    thd->get_transaction()->mark_created_temp_table(Transaction_ctx::STMT);

  /*
    We have to write the query before we unlock the tables.
  */
  if (!thd->is_current_stmt_binlog_disabled() &&
      thd->is_current_stmt_binlog_format_row())
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
        Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);
        bool new_table= FALSE; // Whether newly created table is open.

        /*
          The condition avoids a crash as described in BUG#48506. Other
          binlogging problems related to CREATE TABLE IF NOT EXISTS LIKE
          when the existing object is a view will be solved by BUG 47442.
        */
        if (!table->is_view())
        {
          if (!table->table)
          {
            /*
              In order for store_create_info() to work we need to open
              destination table if it is not already open (i.e. if it
              has not existed before). We don't need acquire metadata
              lock in order to do this as we already hold exclusive
              lock on this table. The table will be closed by
              close_thread_table() at the end of this branch.
            */
            if (open_table(thd, table, &ot_ctx))
              goto err;
            new_table= TRUE;
          }

          /*
            After opening a MERGE table add the children to the query list of
            tables, so that children tables info can be used on "CREATE TABLE"
            statement generation by the binary log.
            Note that placeholders don't have the handler open.
          */
          if (table->table->file->extra(HA_EXTRA_ADD_CHILDREN_LIST))
            goto err;

          /*
            As the reference table is temporary and may not exist on slave, we must
            force the ENGINE to be present into CREATE TABLE.
          */
          create_info->used_fields|= HA_CREATE_USED_ENGINE;

          int result MY_ATTRIBUTE((unused))=
            store_create_info(thd, table, &query,
                              create_info, TRUE /* show_database */);

          DBUG_ASSERT(result == 0); // store_create_info() always return 0
          if (write_bin_log(thd, TRUE, query.ptr(), query.length()))
            goto err;

          if (new_table)
          {
            DBUG_ASSERT(thd->open_tables == table->table);
            /*
              When opening the table, we ignored the locked tables
              (MYSQL_OPEN_GET_NEW_TABLE). Now we can close the table
              without risking to close some locked table.
            */
            close_thread_table(thd, &thd->open_tables);
          }
        }
      }
      else                                      // Case 1
        if (write_bin_log(thd, true, thd->query().str, thd->query().length))
          goto err;
    }
    /*
      Case 3 and 4 does nothing under RBR
    */
  }
  else if (write_bin_log(thd, true,
                         thd->query().str, thd->query().length, is_trans))
    goto err;

err:
  DBUG_RETURN(res);
}


/**
  Class utilizing RAII for correct set/reset of the
  THD::tablespace_op flag. The destructor will reset
  the flag when a stack allocated instance goes out
  of scope.
 */

class Tablespace_op_flag_handler
{
private:
  THD *m_thd;
public:
  Tablespace_op_flag_handler(THD *thd): m_thd(thd)
  {
    m_thd->tablespace_op= true;
  }
  ~Tablespace_op_flag_handler()
  {
    m_thd->tablespace_op= false;
  }
};

/* table_list should contain just one table */
int mysql_discard_or_import_tablespace(THD *thd,
                                       TABLE_LIST *table_list,
                                       bool discard)
{
  Alter_table_prelocking_strategy alter_prelocking_strategy;
  int error;
  DBUG_ENTER("mysql_discard_or_import_tablespace");

  /*
    Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
    ALTER TABLE
  */

   /*
     DISCARD/IMPORT TABLESPACE do not respect ALGORITHM and LOCK clauses.
   */
  if (thd->lex->alter_info.requested_lock !=
      Alter_info::ALTER_TABLE_LOCK_DEFAULT)
  {
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
             "LOCK=NONE/SHARED/EXCLUSIVE",
             "LOCK=DEFAULT");
    DBUG_RETURN(true);
  }
  else if (thd->lex->alter_info.requested_algorithm !=
           Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT)
  {
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
             "ALGORITHM=COPY/INPLACE",
             "ALGORITHM=DEFAULT");
    DBUG_RETURN(true);
  }

  THD_STAGE_INFO(thd, stage_discard_or_import_tablespace);

  /*
    Set thd->tablespace_op, and reset it when the variable leaves scope.
    We set this flag so that ha_innobase::open and ::external_lock() do
    not complain when we lock the table
  */
  Tablespace_op_flag_handler set_tablespace_op(thd);

  /*
    Adjust values of table-level and metadata which was set in parser
    for the case general ALTER TABLE.
  */
  table_list->mdl_request.set_type(MDL_EXCLUSIVE);
  table_list->lock_type= TL_WRITE;
  /* Do not open views. */
  table_list->required_type= FRMTYPE_TABLE;

  if (open_and_lock_tables(thd, table_list, 0, &alter_prelocking_strategy))
  {
    /* purecov: begin inspected */
    DBUG_RETURN(-1);
    /* purecov: end */
  }

  if (table_list->table->part_info)
  {
    /*
      If not ALL is mentioned and there is at least one specified
      [sub]partition name, use the specified [sub]partitions only.
    */
    if (thd->lex->alter_info.partition_names.elements > 0 &&
        !(thd->lex->alter_info.flags & Alter_info::ALTER_ALL_PARTITION))
    {
      table_list->partition_names= &thd->lex->alter_info.partition_names;
      /* Set all [named] partitions as used. */
      if (table_list->table->part_info->set_partition_bitmaps(table_list))
        DBUG_RETURN(-1);
    }
  }
  else
  {
    if (thd->lex->alter_info.partition_names.elements > 0 ||
        thd->lex->alter_info.flags & Alter_info::ALTER_ALL_PARTITION)
    {
      /* Don't allow DISCARD/IMPORT PARTITION on a nonpartitioned table */
      my_error(ER_PARTITION_MGMT_ON_NONPARTITIONED, MYF(0));
      DBUG_RETURN(true);
    }
  }

  /*
    Under LOCK TABLES we need to upgrade SNRW metadata lock to X lock
    before doing discard or import of tablespace.

    Skip this step for temporary tables as metadata locks are not
    applicable for them.
  */
  if (table_list->table->s->tmp_table == NO_TMP_TABLE &&
      (thd->locked_tables_mode == LTM_LOCK_TABLES ||
       thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES) &&
      thd->mdl_context.upgrade_shared_lock(table_list->table->mdl_ticket,
                                           MDL_EXCLUSIVE,
                                           thd->variables.lock_wait_timeout))
  {
    DBUG_RETURN(-1);
  }

  error= table_list->table->file->ha_discard_or_import_tablespace(discard);

  THD_STAGE_INFO(thd, stage_end);

  if (error)
    goto err;

  /*
    The 0 in the call below means 'not in a transaction', which means
    immediate invalidation; that is probably what we wish here
  */
  query_cache.invalidate(thd, table_list, FALSE);

  /* The ALTER TABLE is always in its own transaction */
  error= trans_commit_stmt(thd);
  if (trans_commit_implicit(thd))
    error=1;
  if (error)
    goto err;
  error= write_bin_log(thd, false, thd->query().str, thd->query().length);

err:
  if (table_list->table->s->tmp_table == NO_TMP_TABLE &&
      (thd->locked_tables_mode == LTM_LOCK_TABLES ||
       thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES))
  {
    table_list->table->mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);
  }

  if (error == 0)
  {
    my_ok(thd);
    DBUG_RETURN(0);
  }

  table_list->table->file->print_error(error, MYF(0));

  DBUG_RETURN(-1);
}


/**
  Check if key is a candidate key, i.e. a unique index with no index
  fields partial, nullable or virtual generated.
*/

static bool is_candidate_key(KEY *key)
{
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *key_part_end= key->key_part + key->user_defined_key_parts;

  if (!(key->flags & HA_NOSAME) || (key->flags & HA_NULL_PART_KEY))
    return false;

  if (key->flags & HA_VIRTUAL_GEN_KEY)
    return false;

  for (key_part= key->key_part; key_part < key_part_end; key_part++)
  {
    if (key_part->key_part_flag & HA_PART_KEY_SEG)
      return false;
  }

  return true;
}


/**
  Get Create_field object for newly created table by field index.

  @param alter_info  Alter_info describing newly created table.
  @param idx         Field index.
*/

static Create_field *get_field_by_index(Alter_info *alter_info, uint idx)
{
  List_iterator_fast<Create_field> field_it(alter_info->create_list);
  uint field_idx= 0;
  Create_field *field;

  while ((field= field_it++) && field_idx < idx)
  { field_idx++; }

  return field;
}


/**
  Look-up KEY object by index name using case-insensitive comparison.

  @param key_name   Index name.
  @param key_start  Start of array of KEYs for table.
  @param key_end    End of array of KEYs for table.

  @note Skips indexes which are marked as renamed.
  @note Case-insensitive comparison is necessary to correctly
        handle renaming of keys.

  @retval non-NULL - pointer to KEY object for index found.
  @retval NULL     - no index with such name found (or it is marked
                     as renamed).
*/

static KEY* find_key_ci(const char *key_name, KEY *key_start, KEY *key_end)
{
  for (KEY *key= key_start; key < key_end; key++)
  {
    /* Skip already renamed keys. */
    if (! (key->flags & HA_KEY_RENAMED) &&
        ! my_strcasecmp(system_charset_info, key_name, key->name))
      return key;
  }
  return NULL;
}


/**
  Look-up KEY object by index name using case-sensitive comparison.

  @param key_name   Index name.
  @param key_start  Start of array of KEYs for table.
  @param key_end    End of array of KEYs for table.

  @note Skips indexes which are marked as renamed.
  @note Case-sensitive comparison is necessary to correctly
        handle: ALTER TABLE t1 DROP KEY x, ADD KEY X(c).
        where new and old index are identical except case
        of their names (in this case index still needs
        to be re-created to keep case of the name in .FRM
        and storage-engine in sync).

  @retval non-NULL - pointer to KEY object for index found.
  @retval NULL     - no index with such name found (or it is marked
                     as renamed).
*/

static KEY* find_key_cs(const char *key_name, KEY *key_start, KEY *key_end)
{
  for (KEY *key= key_start; key < key_end; key++)
  {
    /* Skip renamed keys. */
    if (! (key->flags & HA_KEY_RENAMED) && ! strcmp(key_name, key->name))
      return key;
  }
  return NULL;
}


/**
  Check if index has changed in a new version of table (ignore
  possible rename of index). Also changes to the comment field
  of the key is marked with a flag in the ha_alter_info.

  @param[in/out]  ha_alter_info  Structure describing changes to be done
                                 by ALTER TABLE and holding data used
                                 during in-place alter.
  @param          table_key      Description of key in old version of table.
  @param          new_key        Description of key in new version of table.

  @returns True - if index has changed, false -otherwise.
*/

static bool has_index_def_changed(Alter_inplace_info *ha_alter_info,
                                  const KEY *table_key,
                                  const KEY *new_key)
{
  const KEY_PART_INFO *key_part, *new_part, *end;
  const Create_field *new_field;
  Alter_info *alter_info= ha_alter_info->alter_info;

  /* Check that the key types are compatible between old and new tables. */
  if ((table_key->algorithm != new_key->algorithm) ||
      ((table_key->flags & HA_KEYFLAG_MASK) !=
       (new_key->flags & HA_KEYFLAG_MASK)) ||
      (table_key->user_defined_key_parts != new_key->user_defined_key_parts))
    return true;

  /*
    If an index comment is added/dropped/changed, then mark it for a
    fast/INPLACE alteration.
  */
  if ((table_key->comment.length != new_key->comment.length) ||
      (table_key->comment.length && strcmp(table_key->comment.str,
                                           new_key->comment.str)))
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_INDEX_COMMENT;

  /*
    Check that the key parts remain compatible between the old and
    new tables.
  */
  end= table_key->key_part + table_key->user_defined_key_parts;
  for (key_part= table_key->key_part, new_part= new_key->key_part;
       key_part < end;
       key_part++, new_part++)
  {

    new_field= get_field_by_index(alter_info, new_part->fieldnr);

    /*
      If there is a change in index length due to column expansion
      like varchar(X) changed to varchar(X + N) and has a compatible
      packed data representation, we mark it for fast/INPLACE change
      in index definition. Some engines like InnoDB supports INPLACE
      alter for such cases.

      In other cases, key definition has changed if we are using a
      different field or if the used key part length is different, or
      key part direction has changed.
    */
    if (key_part->length != new_part->length &&
        ha_alter_info->alter_info->flags == Alter_info::ALTER_CHANGE_COLUMN &&
        (key_part->field->is_equal((Create_field *)new_field) == IS_EQUAL_PACK_LENGTH))
    {
      ha_alter_info->handler_flags|=
          Alter_inplace_info::ALTER_COLUMN_INDEX_LENGTH;
    }
    else if (key_part->length != new_part->length)
      return true;

    /*
      For prefix keys KEY_PART_INFO::field points to cloned Field
      object with adjusted length. So below we have to check field
      indexes instead of simply comparing pointers to Field objects.
    */
    if (! new_field->field ||
        new_field->field->field_index != key_part->fieldnr - 1)
      return true;

    /*
      Key definition has changed, if the key is converted from a
      non-prefixed key to a prefixed key or vice-versa. This
      is because InnoDB treats prefix keys differently from
      full-column keys. Ignoring BLOBs since the key_length()
      is not set correctly and also the prefix is ignored
      for FULLTEXT keys.
      Ex: When the column length is increased but the key part
      length remains the same.
    */
    if (!(new_field->flags & BLOB_FLAG) &&
        (table_key->algorithm != HA_KEY_ALG_FULLTEXT))
    {
      bool old_part_key_seg= (key_part->key_part_flag & HA_PART_KEY_SEG);
      bool new_part_key_seg= (new_field->key_length != new_part->length);

      if (old_part_key_seg ^ new_part_key_seg)
        return true;
    }
  }

  return false;
}


static int compare_uint(const uint *s, const uint *t)
{
  return (*s < *t) ? -1 : ((*s > *t) ? 1 : 0);
}


/**
   Lock the list of tables which are direct or indirect parents in
   foreign key with cascading actions for the table being altered.
   This prevents DML operations from being performed on the list of
   tables which otherwise may break the 'CASCADE' FK constraint of
   the table being altered.

   @param thd        Thread handler.
   @param table      The table which is altered.

   @retval false     Ok.
   @retval true      Error.
*/

static bool lock_fk_dependent_tables(THD *thd, TABLE *table)
{
  MDL_request_list mdl_requests;
  List <st_handler_tablename> fk_table_list;
  List_iterator<st_handler_tablename> fk_table_list_it(fk_table_list);
  st_handler_tablename *tbl_name;

  table->file->get_cascade_foreign_key_table_list(thd, &fk_table_list);

  while ((tbl_name= fk_table_list_it++))
  {
    MDL_request *table_mdl_request= new (thd->mem_root) MDL_request;

    if (table_mdl_request == NULL)
      return true;

    MDL_REQUEST_INIT(table_mdl_request,
                     MDL_key::TABLE, tbl_name->db,tbl_name->tablename,
                     MDL_SHARED_READ_ONLY, MDL_STATEMENT);
    mdl_requests.push_front(table_mdl_request);
  }
  
  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
    return true;

  return false;
}


/**
   Compare original and new versions of a table and fill Alter_inplace_info
   describing differences between those versions.

   @param          thd                Thread
   @param          table              The original table.
   @param          varchar            Indicates that new definition has new
                                      VARCHAR column.
   @param[in/out]  ha_alter_info      Data structure which already contains
                                      basic information about create options,
                                      field and keys for the new version of
                                      table and which should be completed with
                                      more detailed information needed for
                                      in-place ALTER.

   First argument 'table' contains information of the original
   table, which includes all corresponding parts that the new
   table has in arguments create_list, key_list and create_info.

   Compare the changes between the original and new table definitions.
   The result of this comparison is then passed to SE which determines
   whether it can carry out these changes in-place.

   Mark any changes detected in the ha_alter_flags.
   We generally try to specify handler flags only if there are real
   changes. But in cases when it is cumbersome to determine if some
   attribute has really changed we might choose to set flag
   pessimistically, for example, relying on parser output only.

   If there are no data changes, but index changes, 'index_drop_buffer'
   and/or 'index_add_buffer' are populated with offsets into
   table->key_info or key_info_buffer respectively for the indexes
   that need to be dropped and/or (re-)created.

   Note that this function assumes that it is OK to change Alter_info
   and HA_CREATE_INFO which it gets. It is caller who is responsible
   for creating copies for this structures if he needs them unchanged.

   @retval true  error
   @retval false success
*/

static bool fill_alter_inplace_info(THD *thd,
                                    TABLE *table,
                                    bool varchar,
                                    Alter_inplace_info *ha_alter_info)
{
  Field **f_ptr, *field;
  List_iterator_fast<Create_field> new_field_it;
  Create_field *new_field;
  uint candidate_key_count= 0;
  Alter_info *alter_info= ha_alter_info->alter_info;
  DBUG_ENTER("fill_alter_inplace_info");

  /* Allocate result buffers. */
  if (! (ha_alter_info->index_drop_buffer=
          (KEY**) thd->alloc(sizeof(KEY*) * table->s->keys)) ||
      ! (ha_alter_info->index_add_buffer=
          (uint*) thd->alloc(sizeof(uint) *
                            alter_info->key_list.elements)) ||
      ! (ha_alter_info->index_rename_buffer=
          (KEY_PAIR*) thd->alloc(sizeof(KEY_PAIR) *
                                 alter_info->alter_rename_key_list.elements)))
    DBUG_RETURN(true);

  /* First we setup ha_alter_flags based on what was detected by parser. */

  /*
    Comparing new and old default values of column is cumbersome.
    So instead of using such a comparison for detecting if default
    has really changed we rely on flags set by parser to get an
    approximate value for storage engine flag.
  */
  if (alter_info->flags & (Alter_info::ALTER_CHANGE_COLUMN |
                           Alter_info::ALTER_CHANGE_COLUMN_DEFAULT))
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_COLUMN_DEFAULT;
  if (alter_info->flags & Alter_info::ADD_FOREIGN_KEY)
    ha_alter_info->handler_flags|= Alter_inplace_info::ADD_FOREIGN_KEY;
  if (alter_info->flags & Alter_info::DROP_FOREIGN_KEY)
    ha_alter_info->handler_flags|= Alter_inplace_info::DROP_FOREIGN_KEY;
  if (alter_info->flags & Alter_info::ALTER_OPTIONS)
    ha_alter_info->handler_flags|= Alter_inplace_info::CHANGE_CREATE_OPTION;
  if (alter_info->flags & Alter_info::ALTER_RENAME)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_RENAME;
  /* Check partition changes */
  if (alter_info->flags & Alter_info::ALTER_ADD_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::ADD_PARTITION;
  if (alter_info->flags & Alter_info::ALTER_DROP_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::DROP_PARTITION;
  if (alter_info->flags & Alter_info::ALTER_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_PARTITION;
  if (alter_info->flags & Alter_info::ALTER_COALESCE_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::COALESCE_PARTITION;
  if (alter_info->flags & Alter_info::ALTER_REORGANIZE_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::REORGANIZE_PARTITION;
  if (alter_info->flags & Alter_info::ALTER_TABLE_REORG)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_TABLE_REORG;
  if (alter_info->flags & Alter_info::ALTER_REMOVE_PARTITIONING)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_REMOVE_PARTITIONING;
  if (alter_info->flags & Alter_info::ALTER_ALL_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_ALL_PARTITION;
  /* Check for: ALTER TABLE FORCE, ALTER TABLE ENGINE and OPTIMIZE TABLE. */
  if (alter_info->flags & Alter_info::ALTER_RECREATE)
    ha_alter_info->handler_flags|= Alter_inplace_info::RECREATE_TABLE;
  if (alter_info->flags & Alter_info::ALTER_UPGRADE_PARTITIONING)
    ha_alter_info->handler_flags|=
      Alter_inplace_info::ALTER_UPGRADE_PARTITIONING;
  if (alter_info->with_validation == Alter_info::ALTER_WITH_VALIDATION)
    ha_alter_info->handler_flags|= Alter_inplace_info::VALIDATE_VIRTUAL_COLUMN;

  /*
    If we altering table with old VARCHAR fields we will be automatically
    upgrading VARCHAR column types.
  */
  if (table->s->frm_version < FRM_VER_TRUE_VARCHAR && varchar)
    ha_alter_info->handler_flags|=  Alter_inplace_info::ALTER_STORED_COLUMN_TYPE;

  /*
    Go through fields in old version of table and detect changes to them.
    We don't want to rely solely on Alter_info flags for this since:
    a) new definition of column can be fully identical to the old one
       despite the fact that this column is mentioned in MODIFY clause.
    b) even if new column type differs from its old column from metadata
       point of view, it might be identical from storage engine point
       of view (e.g. when ENUM('a','b') is changed to ENUM('a','b',c')).
    c) flags passed to storage engine contain more detailed information
       about nature of changes than those provided from parser.
  */
  uint old_field_index_without_vgc= 0;
  for (f_ptr= table->field; (field= *f_ptr); f_ptr++)
  {
    /* Clear marker for renamed or dropped field
    which we are going to set later. */
    field->flags&= ~(FIELD_IS_RENAMED | FIELD_IS_DROPPED);

    /* Use transformed info to evaluate flags for storage engine. */
    uint new_field_index= 0;
    uint new_field_index_without_vgc= 0;
    new_field_it.init(alter_info->create_list);
    while ((new_field= new_field_it++))
    {
      if (new_field->field == field)
        break;
      if (new_field->stored_in_db)
        new_field_index_without_vgc++;
      new_field_index++;
    }

    if (new_field)
    {
      /* Field is not dropped. Evaluate changes bitmap for it. */

      /*
        Check if type of column has changed to some incompatible type.
      */
      switch (field->is_equal(new_field))
      {
      case IS_EQUAL_NO:
        /* New column type is incompatible with old one. */
        if (field->is_virtual_gcol())
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ALTER_VIRTUAL_COLUMN_TYPE;
        else
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ALTER_STORED_COLUMN_TYPE;
        break;
      case IS_EQUAL_YES:
        /*
          New column is the same as the old one or the fully compatible with
          it (for example, ENUM('a','b') was changed to ENUM('a','b','c')).
          Such a change if any can ALWAYS be carried out by simply updating
          data-dictionary without even informing storage engine.
          No flag is set in this case.
        */
        break;
      case IS_EQUAL_PACK_LENGTH:
        /*
          New column type differs from the old one, but has compatible packed
          data representation. Depending on storage engine, such a change can
          be carried out by simply updating data dictionary without changing
          actual data (for example, VARCHAR(300) is changed to VARCHAR(400)).
        */
        ha_alter_info->handler_flags|= Alter_inplace_info::
                                         ALTER_COLUMN_EQUAL_PACK_LENGTH;
        break;
      default:
        DBUG_ASSERT(0);
      }

      // Conversion to and from generated column is supported if stored:
      if (field->is_gcol() != new_field->is_gcol())
      {
        DBUG_ASSERT((field->is_gcol() && !field->is_virtual_gcol()) ||
                    (new_field->is_gcol() && !new_field->is_virtual_gcol()));
        ha_alter_info->handler_flags|=
          Alter_inplace_info::ALTER_STORED_COLUMN_TYPE;
      }

      // Modification of generation expression is supported:
      if (field->is_gcol() && new_field->is_gcol())
      {
        // Modification of storage attribute is not supported
        DBUG_ASSERT(field->is_virtual_gcol() == new_field->is_virtual_gcol());
        if (!field->gcol_expr_is_equal(new_field))
        {
          if (field->is_virtual_gcol())
            ha_alter_info->handler_flags|=
              Alter_inplace_info::ALTER_VIRTUAL_COLUMN_TYPE;
          else
            ha_alter_info->handler_flags|=
              Alter_inplace_info::ALTER_STORED_COLUMN_TYPE;
        }
      }

      bool field_renamed;
      /*
        InnoDB data dictionary is case sensitive so we should use
        string case sensitive comparison between fields.
        Note: strcmp branch is to be removed in future when we fix it
        in InnoDB.
      */
      if (ha_alter_info->create_info->db_type->db_type == DB_TYPE_INNODB)
        field_renamed= strcmp(field->field_name, new_field->field_name);
      else
	field_renamed= my_strcasecmp(system_charset_info, field->field_name,
                                     new_field->field_name);

      /* Check if field was renamed */
      if (field_renamed)
      {
        field->flags|= FIELD_IS_RENAMED;
        ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_COLUMN_NAME;
      }

      /* Check that NULL behavior is same for old and new fields */
      if ((new_field->flags & NOT_NULL_FLAG) !=
          (uint) (field->flags & NOT_NULL_FLAG))
      {
        if (new_field->flags & NOT_NULL_FLAG)
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE;
        else
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ALTER_COLUMN_NULLABLE;
      }

      /*
        We do not detect changes to default values in this loop.
        See comment above for more details.
      */

      /*
        Detect changes in column order.

        Note that a stored column can't become virtual and vice versa
        thanks to check in mysql_prepare_alter_table().
      */
      if (field->stored_in_db)
      {
        if (old_field_index_without_vgc != new_field_index_without_vgc)
          ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_STORED_COLUMN_ORDER;
      }
      else
      {
        if (field->field_index != new_field_index)
          ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER;
      }

      /* Detect changes in storage type of column */
      if (new_field->field_storage_type() != field->field_storage_type())
        ha_alter_info->handler_flags|=
          Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE;

      /* Detect changes in column format of column */
      if (new_field->column_format() != field->column_format())
        ha_alter_info->handler_flags|=
          Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT;

      /*
        We don't have easy way to detect change in generation expression.
        So we always assume that it has changed if generated column was
        mentioned in CHANGE/MODIFY COLUMN clause of ALTER TABLE.
      */
      if (new_field->change)
      {
        if (new_field->is_virtual_gcol())
            ha_alter_info->handler_flags|=
              Alter_inplace_info::ALTER_VIRTUAL_GCOL_EXPR;
        else if (new_field->gcol_info)
            ha_alter_info->handler_flags|=
              Alter_inplace_info::ALTER_STORED_GCOL_EXPR;
      }
    }
    else
    {
      /*
        Field is not present in new version of table and therefore was dropped.
      */
      DBUG_ASSERT(alter_info->flags & Alter_info::ALTER_DROP_COLUMN);
      if (field->is_virtual_gcol())
        ha_alter_info->handler_flags|=
          Alter_inplace_info::DROP_VIRTUAL_COLUMN;
      else
        ha_alter_info->handler_flags|=
          Alter_inplace_info::DROP_STORED_COLUMN;
      field->flags|= FIELD_IS_DROPPED;
    }
    if (field->stored_in_db)
      old_field_index_without_vgc++;
  }

  if (alter_info->flags & Alter_info::ALTER_ADD_COLUMN)
  {
    new_field_it.init(alter_info->create_list);
    while ((new_field= new_field_it++))
    {
      if (!new_field->field)
      {
        /*
          Field is not present in old version of table and therefore was added.
        */
        if (new_field->is_virtual_gcol())
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ADD_VIRTUAL_COLUMN;
        else if (new_field->gcol_info)
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ADD_STORED_GENERATED_COLUMN;
        else
          ha_alter_info->handler_flags|=
            Alter_inplace_info::ADD_STORED_BASE_COLUMN;
      }
    }
    /* One of these should be set since Alter_info::ALTER_ADD_COLUMN was set. */
    DBUG_ASSERT(ha_alter_info->handler_flags &
                (Alter_inplace_info::ADD_VIRTUAL_COLUMN |
                 Alter_inplace_info::ADD_STORED_BASE_COLUMN |
                 Alter_inplace_info::ADD_STORED_GENERATED_COLUMN));
  }

  /*
    Go through keys and check if the original ones are compatible
    with new table.
  */
  KEY *table_key;
  KEY *table_key_end= table->key_info + table->s->keys;
  KEY *new_key;
  KEY *new_key_end=
    ha_alter_info->key_info_buffer + ha_alter_info->key_count;

  DBUG_PRINT("info", ("index count old: %d  new: %d",
                      table->s->keys, ha_alter_info->key_count));

  /*
    First, we need to handle keys being renamed, otherwise code handling
    dropping/addition of keys might be confused in some situations.
  */
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
    table_key->flags&= ~HA_KEY_RENAMED;
  for (new_key= ha_alter_info->key_info_buffer;
       new_key < new_key_end; new_key++)
    new_key->flags&= ~HA_KEY_RENAMED;

  List_iterator_fast<Alter_rename_key> rename_key_it(alter_info->
                                                     alter_rename_key_list);
  Alter_rename_key *rename_key;

  while ((rename_key= rename_key_it++))
  {
    table_key= find_key_ci(rename_key->old_name, table->key_info, table_key_end);
    new_key= find_key_ci(rename_key->new_name, ha_alter_info->key_info_buffer,
                         new_key_end);

    table_key->flags|= HA_KEY_RENAMED;
    new_key->flags|= HA_KEY_RENAMED;

    if (! has_index_def_changed(ha_alter_info, table_key, new_key))
    {
      /* Key was not modified but still was renamed. */
      ha_alter_info->handler_flags|= Alter_inplace_info::RENAME_INDEX;
      ha_alter_info->add_renamed_key(table_key, new_key);
    }
    else
    {
      /* Key was modified. */
      ha_alter_info->add_modified_key(table_key, new_key);
    }
  }

  /*
    Step through all keys of the old table and search matching new keys.
  */
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    /* Skip renamed keys. */
    if (table_key->flags & HA_KEY_RENAMED)
      continue;

    new_key= find_key_cs(table_key->name, ha_alter_info->key_info_buffer,
                         new_key_end);

    if (new_key == NULL)
    {
      /* Matching new key not found. This means the key should be dropped. */
      ha_alter_info->add_dropped_key(table_key);
    }
    else if (has_index_def_changed(ha_alter_info, table_key, new_key))
    {
      /* Key was modified. */
      ha_alter_info->add_modified_key(table_key, new_key);
    }
  }

  /*
    Step through all keys of the new table and find matching old keys.
  */
  for (new_key= ha_alter_info->key_info_buffer;
       new_key < new_key_end;
       new_key++)
  {
    /* Skip renamed keys. */
    if (new_key->flags & HA_KEY_RENAMED)
      continue;

    if (! find_key_cs(new_key->name, table->key_info, table_key_end))
    {
      /* Matching old key not found. This means the key should be added. */
      ha_alter_info->add_added_key(new_key);
    }
  }

  /*
    Sort index_add_buffer according to how key_info_buffer is sorted.
    I.e. with primary keys first - see sort_keys().
  */
  my_qsort(ha_alter_info->index_add_buffer,
           ha_alter_info->index_add_count,
           sizeof(uint), (qsort_cmp) compare_uint);

  /* Now let us calculate flags for storage engine API. */

  /* Count all existing candidate keys. */
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    /*
      Check if key is a candidate key, This key is either already primary key
      or could be promoted to primary key if the original primary key is
      dropped.
      In MySQL one is allowed to create primary key with partial fields (i.e.
      primary key which is not considered candidate). For simplicity we count
      such key as a candidate key here.
    */
    if (((uint) (table_key - table->key_info) == table->s->primary_key) ||
        is_candidate_key(table_key))
      candidate_key_count++;
  }

  /* Figure out what kind of indexes we are dropping. */
  KEY **dropped_key;
  KEY **dropped_key_end= ha_alter_info->index_drop_buffer +
                         ha_alter_info->index_drop_count;

  for (dropped_key= ha_alter_info->index_drop_buffer;
       dropped_key < dropped_key_end; dropped_key++)
  {
    table_key= *dropped_key;

    if (table_key->flags & HA_NOSAME)
    {
      /*
        Unique key. Check for PRIMARY KEY. Also see comment about primary
        and candidate keys above.
      */
      if ((uint) (table_key - table->key_info) == table->s->primary_key)
      {
        ha_alter_info->handler_flags|= Alter_inplace_info::DROP_PK_INDEX;
        candidate_key_count--;
      }
      else
      {
        ha_alter_info->handler_flags|= Alter_inplace_info::DROP_UNIQUE_INDEX;
        if (is_candidate_key(table_key))
          candidate_key_count--;
      }
    }
    else
      ha_alter_info->handler_flags|= Alter_inplace_info::DROP_INDEX;
  }

  /* Now figure out what kind of indexes we are adding. */
  for (uint add_key_idx= 0; add_key_idx < ha_alter_info->index_add_count; add_key_idx++)
  {
    new_key= ha_alter_info->key_info_buffer + ha_alter_info->index_add_buffer[add_key_idx];

    if (new_key->flags & HA_NOSAME)
    {
      bool is_pk= !my_strcasecmp(system_charset_info, new_key->name, primary_key_name);

      if ((!(new_key->flags & HA_KEY_HAS_PART_KEY_SEG) &&
           !(new_key->flags & HA_NULL_PART_KEY)) ||
          is_pk)
      {
        /* Candidate key or primary key! */
        if (candidate_key_count == 0 || is_pk)
          ha_alter_info->handler_flags|= Alter_inplace_info::ADD_PK_INDEX;
        else
          ha_alter_info->handler_flags|= Alter_inplace_info::ADD_UNIQUE_INDEX;
        candidate_key_count++;
      }
      else
      {
        ha_alter_info->handler_flags|= Alter_inplace_info::ADD_UNIQUE_INDEX;
      }
    }
    else
    {
      if (new_key->flags & HA_SPATIAL)
      {
        ha_alter_info->handler_flags|= Alter_inplace_info::ADD_SPATIAL_INDEX;
      }
      else
      {
        ha_alter_info->handler_flags|= Alter_inplace_info::ADD_INDEX;
      }
    }
  }

  DBUG_RETURN(false);
}


/**
  Mark fields participating in newly added indexes in TABLE object which
  corresponds to new version of altered table.

  @param ha_alter_info  Alter_inplace_info describing in-place ALTER.
  @param altered_table  TABLE object for new version of TABLE in which
                        fields should be marked.
*/

static void update_altered_table(const Alter_inplace_info &ha_alter_info,
                                 TABLE *altered_table)
{
  uint field_idx, add_key_idx;
  KEY *key;
  KEY_PART_INFO *end, *key_part;

  /*
    Clear marker for all fields, as we are going to set it only
    for fields which participate in new indexes.
  */
  for (field_idx= 0; field_idx < altered_table->s->fields; ++field_idx)
    altered_table->field[field_idx]->flags&= ~FIELD_IN_ADD_INDEX;

  /*
    Go through array of newly added indexes and mark fields
    participating in them.
  */
  for (add_key_idx= 0; add_key_idx < ha_alter_info.index_add_count;
       add_key_idx++)
  {
    key= ha_alter_info.key_info_buffer +
         ha_alter_info.index_add_buffer[add_key_idx];

    end= key->key_part + key->user_defined_key_parts;
    for (key_part= key->key_part; key_part < end; key_part++)
      altered_table->field[key_part->fieldnr]->flags|= FIELD_IN_ADD_INDEX;
  }
}


/**
  Initialize TABLE::field for the new table with appropriate
  column defaults. Can be default values from TABLE_SHARE or
  function defaults from Create_field.

  @param altered_table  TABLE object for the new version of the table.
  @param create         Create_field containing function defaults.
*/

static void set_column_defaults(TABLE *altered_table,
                                List<Create_field> &create)
{
  // Initialize TABLE::field default values
  restore_record(altered_table, s->default_values);

  List_iterator<Create_field> iter(create);
  for (uint i= 0; i < altered_table->s->fields; ++i)
  {
    const Create_field *definition= iter++;
    if (definition->field == NULL) // this column didn't exist in old table.
      altered_table->field[i]->evaluate_insert_default_function();
  }
}


/**
  Compare two tables to see if their metadata are compatible.
  One table specified by a TABLE instance, the other using Alter_info
  and HA_CREATE_INFO.

  @param[in]  table          The first table.
  @param[in]  alter_info     Alter options, fields and keys for the
                             second table.
  @param[in]  create_info    Create options for the second table.
  @param[out] metadata_equal Result of comparison.

  @retval true   error
  @retval false  success
*/

bool mysql_compare_tables(TABLE *table,
                          Alter_info *alter_info,
                          HA_CREATE_INFO *create_info,
                          bool *metadata_equal)
{
  DBUG_ENTER("mysql_compare_tables");

  uint changes= IS_EQUAL_NO;
  uint key_count;
  List_iterator_fast<Create_field> tmp_new_field_it;
  THD *thd= table->in_use;
  *metadata_equal= false;

  /*
    Create a copy of alter_info.
    To compare definitions, we need to "prepare" the definition - transform it
    from parser output to a format that describes the table layout (all column
    defaults are initialized, duplicate columns are removed). This is done by
    mysql_prepare_create_table.  Unfortunately, mysql_prepare_create_table
    performs its transformations "in-place", that is, modifies the argument.
    Since we would like to keep mysql_compare_tables() idempotent (not altering
    any of the arguments) we create a copy of alter_info here and pass it to
    mysql_prepare_create_table, then use the result to compare the tables, and
    then destroy the copy.
  */
  Alter_info tmp_alter_info(*alter_info, thd->mem_root);
  uint db_options= 0; /* not used */
  KEY *key_info_buffer= NULL;

  /* Create the prepared information. */
  if (mysql_prepare_create_table(thd, "", "",
                                 create_info, &tmp_alter_info,
                                 (table->s->tmp_table != NO_TMP_TABLE),
                                 &db_options,
                                 table->file, &key_info_buffer,
                                 &key_count, 0))
    DBUG_RETURN(true);

  /* Some very basic checks. */
  if (table->s->fields != alter_info->create_list.elements ||
      table->s->db_type() != create_info->db_type ||
      table->s->tmp_table ||
      (table->s->row_type != create_info->row_type))
    DBUG_RETURN(false);

  /* Go through fields and check if they are compatible. */
  tmp_new_field_it.init(tmp_alter_info.create_list);
  for (Field **f_ptr= table->field; *f_ptr; f_ptr++)
  {
    Field *field= *f_ptr;
    Create_field *tmp_new_field= tmp_new_field_it++;

    /* Check that NULL behavior is the same. */
    if ((tmp_new_field->flags & NOT_NULL_FLAG) !=
	(uint) (field->flags & NOT_NULL_FLAG))
      DBUG_RETURN(false);

    /*
      mysql_prepare_alter_table() clears HA_OPTION_PACK_RECORD bit when
      preparing description of existing table. In ALTER TABLE it is later
      updated to correct value by create_table_impl() call.
      So to get correct value of this bit in this function we have to
      mimic behavior of create_table_impl().
    */
    if (create_info->row_type == ROW_TYPE_DYNAMIC ||
	(tmp_new_field->flags & BLOB_FLAG) ||
	(tmp_new_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED))
      create_info->table_options|= HA_OPTION_PACK_RECORD;

    /* Check if field was renamed */
    if (my_strcasecmp(system_charset_info,
		      field->field_name,
		      tmp_new_field->field_name))
      DBUG_RETURN(false);

    /* Evaluate changes bitmap and send to check_if_incompatible_data() */
    uint field_changes= field->is_equal(tmp_new_field);
    if (field_changes != IS_EQUAL_YES)
      DBUG_RETURN(false);

    changes|= field_changes;
  }

  /* Check if changes are compatible with current handler. */
  if (table->file->check_if_incompatible_data(create_info, changes))
    DBUG_RETURN(false);

  /* Go through keys and check if they are compatible. */
  KEY *table_key;
  KEY *table_key_end= table->key_info + table->s->keys;
  KEY *new_key;
  KEY *new_key_end= key_info_buffer + key_count;

  /* Step through all keys of the first table and search matching keys. */
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    /* Search a key with the same name. */
    for (new_key= key_info_buffer; new_key < new_key_end; new_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (new_key >= new_key_end)
      DBUG_RETURN(false);

    /* Check that the key types are compatible. */
    if ((table_key->algorithm != new_key->algorithm) ||
	((table_key->flags & HA_KEYFLAG_MASK) !=
         (new_key->flags & HA_KEYFLAG_MASK)) ||
        (table_key->user_defined_key_parts != new_key->user_defined_key_parts))
      DBUG_RETURN(false);

    /* Check that the key parts remain compatible. */
    KEY_PART_INFO *table_part;
    KEY_PART_INFO *table_part_end= table_key->key_part +
      table_key->user_defined_key_parts;
    KEY_PART_INFO *new_part;
    for (table_part= table_key->key_part, new_part= new_key->key_part;
         table_part < table_part_end;
         table_part++, new_part++)
    {
      /*
	Key definition is different if we are using a different field or
	if the used key part length is different. We know that the fields
        are equal. Comparing field numbers is sufficient.
      */
      if ((table_part->length != new_part->length) ||
          (table_part->fieldnr - 1 != new_part->fieldnr))
        DBUG_RETURN(false);
    }
  }

  /* Step through all keys of the second table and find matching keys. */
  for (new_key= key_info_buffer; new_key < new_key_end; new_key++)
  {
    /* Search a key with the same name. */
    for (table_key= table->key_info; table_key < table_key_end; table_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (table_key >= table_key_end)
      DBUG_RETURN(false);
  }

  *metadata_equal= true; // Tables are compatible
  DBUG_RETURN(false);
}


/**
   Report a zero date warning if no default value is supplied
   for the DATE/DATETIME 'NOT NULL' field and 'NO_ZERO_DATE'
   sql_mode is enabled.

   @param thd                Thread handle.
   @param datetime_field     DATE/DATETIME column definition.
*/
static void push_zero_date_warning(THD *thd, Create_field *datetime_field)
{
  uint f_length= 0;
  enum enum_mysql_timestamp_type t_type= MYSQL_TIMESTAMP_DATE;

  switch (datetime_field->sql_type)
  {
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
    f_length= MAX_DATE_WIDTH; // "0000-00-00";
    t_type= MYSQL_TIMESTAMP_DATE;
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATETIME2:
    f_length= MAX_DATETIME_WIDTH; // "0000-00-00 00:00:00";
    t_type= MYSQL_TIMESTAMP_DATETIME;
    break;
  default:
    DBUG_ASSERT(false);  // Should not get here.
  }
  make_truncated_value_warning(thd, Sql_condition::SL_WARNING,
                               ErrConvString(my_zero_datetime6, f_length),
                               t_type, datetime_field->field_name);
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
                             Alter_info::enum_enable_or_disable keys_onoff)
{
  int error= 0;
  DBUG_ENTER("alter_table_manage_keys");
  DBUG_PRINT("enter", ("table=%p were_disabled=%d on_off=%d",
             table, indexes_were_disabled, keys_onoff));

  switch (keys_onoff) {
  case Alter_info::ENABLE:
    error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
    break;
  case Alter_info::LEAVE_AS_IS:
    if (!indexes_were_disabled)
      break;
    /* fall-through: disabled indexes */
  case Alter_info::DISABLE:
    error= table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if (error == HA_ERR_WRONG_COMMAND)
  {
    push_warning_printf(current_thd, Sql_condition::SL_NOTE,
                        ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                        table->s->table_name.str);
    error= 0;
  } else if (error)
    table->file->print_error(error, MYF(0));

  DBUG_RETURN(error);
}


/**
  Check if the pending ALTER TABLE operations support the in-place
  algorithm based on restrictions in the SQL layer or given the
  nature of the operations themselves. If in-place isn't supported,
  it won't be necessary to check with the storage engine.

  @param table        The original TABLE.
  @param create_info  Information from the parsing phase about new
                      table properties.
  @param alter_info   Data related to detected changes.
  @param alter_ctx    Runtime context for ALTER TABLE.

  @return false       In-place is possible, check with storage engine.
  @return true        Incompatible operations, must use table copy.
*/

static bool is_inplace_alter_impossible(TABLE *table,
                                        HA_CREATE_INFO *create_info,
                                        const Alter_info *alter_info,
                                        const Alter_table_ctx *alter_ctx)
{
  DBUG_ENTER("is_inplace_alter_impossible");

  /* At the moment we can't handle altering temporary tables without a copy. */
  if (table->s->tmp_table)
    DBUG_RETURN(true);

  /*
    For the ALTER TABLE tbl_name ORDER BY ... we always use copy
    algorithm. In theory, this operation can be done in-place by some
    engine, but since a) no current engine does this and b) our current
    API lacks infrastructure for passing information about table ordering
    to storage engine we simply always do copy now.

    ENABLE/DISABLE KEYS is a MyISAM/Heap specific operation that is
    not supported for in-place in combination with other operations.
    Alone, it will be done by simple_rename_or_index_change().

    Stored generated columns are evaluated in server, thus can't be added/changed
    inplace.
  */
  if (alter_info->flags & (Alter_info::ALTER_ORDER |
                           Alter_info::ALTER_KEYS_ONOFF))
    DBUG_RETURN(true);

  /*
    If the table engine is changed explicitly (using ENGINE clause)
    or implicitly (e.g. when non-partitioned table becomes
    partitioned) a regular alter table (copy) needs to be
    performed.
  */
  if (create_info->db_type != table->s->db_type())
  {
    /*
      If we are altering/recreating a table using the generic partitioning
      engine ha_partition, but the real engine supports partitioning
      natively, do not disallow INPLACE, since it will be handled in
      ha_partition/real engine and allow the engine to be upgraded to native
      partitioning!
    */
    if (!is_ha_partition_handlerton(table->s->db_type()) ||
        !create_info->db_type->partition_flags ||
        table->part_info->default_engine_type != create_info->db_type ||
        (create_info->used_fields & HA_CREATE_USED_ENGINE))
    {
      DBUG_RETURN(true);
    }
  }

  /*
    There was a bug prior to mysql-4.0.25. Number of null fields was
    calculated incorrectly. As a result frm and data files gets out of
    sync after fast alter table. There is no way to determine by which
    mysql version (in 4.0 and 4.1 branches) table was created, thus we
    disable fast alter table for all tables created by mysql versions
    prior to 5.0 branch.
    See BUG#6236.
  */
  if (!table->s->mysql_version)
    DBUG_RETURN(true);

  /*
    If default value is changed and the table includes or will include
    generated columns that depend on the DEFAULT function, we cannot
    do the operation inplace as indexes or value of stored generated
    columns might become invalid.
  */
  if ((alter_info->flags &
       (Alter_info::ALTER_CHANGE_COLUMN_DEFAULT |
        Alter_info::ALTER_CHANGE_COLUMN)) &&
       table->has_gcol())
  {
    for (Field **vfield= table->vfield; *vfield; vfield++)
    {
      if ((*vfield)->gcol_info->expr_item->walk(
           &Item::check_gcol_depend_default_processor,
           Item::WALK_POSTFIX, NULL))
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}


/**
  Perform in-place alter table.

  @param thd                Thread handle.
  @param table_list         TABLE_LIST for the table to change.
  @param table              The original TABLE.
  @param altered_table      TABLE object for new version of the table.
  @param ha_alter_info      Structure describing ALTER TABLE to be carried
                            out and serving as a storage place for data
                            used during different phases.
  @param inplace_supported  Enum describing the locking requirements.
  @param target_mdl_request Metadata request/lock on the target table name.
  @param alter_ctx          ALTER TABLE runtime context.

  @retval   true              Error
  @retval   false             Success

  @note
    If mysql_alter_table does not need to copy the table, it is
    either an alter table where the storage engine does not
    need to know about the change, only the frm will change,
    or the storage engine supports performing the alter table
    operation directly, in-place without mysql having to copy
    the table.

  @note This function frees the TABLE object associated with the new version of
        the table and removes the .FRM file for it in case of both success and
        failure.
*/

static bool mysql_inplace_alter_table(THD *thd,
                                      TABLE_LIST *table_list,
                                      TABLE *table,
                                      TABLE *altered_table,
                                      Alter_inplace_info *ha_alter_info,
                                      enum_alter_inplace_result inplace_supported,
                                      MDL_request *target_mdl_request,
                                      Alter_table_ctx *alter_ctx)
{
  Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);
  MDL_ticket *mdl_ticket= table->mdl_ticket;
  HA_CREATE_INFO *create_info= ha_alter_info->create_info;
  Alter_info *alter_info= ha_alter_info->alter_info;
  bool reopen_tables= false;

  DBUG_ENTER("mysql_inplace_alter_table");

  /*
    Upgrade to EXCLUSIVE lock if:
    - This is requested by the storage engine
    - Or the storage engine needs exclusive lock for just the prepare
      phase
    - Or requested by the user

    Note that we handle situation when storage engine needs exclusive
    lock for prepare phase under LOCK TABLES in the same way as when
    exclusive lock is required for duration of the whole statement.
  */
  if (inplace_supported == HA_ALTER_INPLACE_EXCLUSIVE_LOCK ||
      ((inplace_supported == HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE ||
        inplace_supported == HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE) &&
       (thd->locked_tables_mode == LTM_LOCK_TABLES ||
        thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES)) ||
       alter_info->requested_lock == Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE)
  {
    if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      goto cleanup;
    /*
      Get rid of all TABLE instances belonging to this thread
      except one to be used for in-place ALTER TABLE.

      This is mostly needed to satisfy InnoDB assumptions/asserts.
    */
    close_all_tables_for_name(thd, table->s, alter_ctx->is_table_renamed(),
                              table);
    /*
      If we are under LOCK TABLES we will need to reopen tables which we
      just have closed in case of error.
    */
    reopen_tables= true;
  }
  else if (inplace_supported == HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE ||
           inplace_supported == HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE)
  {
    /*
      Storage engine has requested exclusive lock only for prepare phase
      and we are not under LOCK TABLES.
      Don't mark TABLE_SHARE as old in this case, as this won't allow opening
      of table by other threads during main phase of in-place ALTER TABLE.
    */
    if (thd->mdl_context.upgrade_shared_lock(table->mdl_ticket, MDL_EXCLUSIVE,
                                             thd->variables.lock_wait_timeout))
      goto cleanup;

    tdc_remove_table(thd, TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE,
                     table->s->db.str, table->s->table_name.str,
                     false);
  }

  /*
    Upgrade to SHARED_NO_WRITE lock if:
    - The storage engine needs writes blocked for the whole duration
    - Or this is requested by the user
    Note that under LOCK TABLES, we will already have SHARED_NO_READ_WRITE.
  */
  if ((inplace_supported == HA_ALTER_INPLACE_SHARED_LOCK ||
       alter_info->requested_lock == Alter_info::ALTER_TABLE_LOCK_SHARED) &&
      thd->mdl_context.upgrade_shared_lock(table->mdl_ticket,
                                           MDL_SHARED_NO_WRITE,
                                           thd->variables.lock_wait_timeout))
  {
    goto cleanup;
  }

  // It's now safe to take the table level lock.
  if (lock_tables(thd, table_list, alter_ctx->tables_opened, 0))
    goto cleanup;

  if (alter_ctx->error_if_not_empty)
  {
    bool has_records= true;
    DBUG_ASSERT(table->mdl_ticket->get_type() == MDL_EXCLUSIVE);
    if (table_list->table->file->ha_table_flags() & HA_HAS_RECORDS)
    {
      ha_rows tmp= 0;
      if (!table_list->table->file->ha_records(&tmp) && tmp == 0)
        has_records= false;
    }
    else if(table_list->table->contains_records(thd, &has_records))
    {
      my_error(ER_INVALID_USE_OF_NULL, MYF(0));
      goto cleanup;
    }

    if (has_records)
    {
      if (alter_ctx->error_if_not_empty &
          Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT)
      {
        my_error(ER_INVALID_USE_OF_NULL, MYF(0));
      }
      else if ((alter_ctx->error_if_not_empty &
                Alter_table_ctx::DATETIME_WITHOUT_DEFAULT) &&
               (thd->variables.sql_mode & MODE_NO_ZERO_DATE))
      {
        /*
          Report a warning if the NO ZERO DATE MODE is enabled. The
          warning will be promoted to an error if strict mode is
          also enabled.
        */
        push_zero_date_warning(thd, alter_ctx->datetime_field);
      }

      if (thd->is_error())
        goto cleanup;
    }

    // Empty table, so don't allow inserts during inplace operation.
    if (inplace_supported == HA_ALTER_INPLACE_NO_LOCK ||
        inplace_supported == HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE)
      inplace_supported= HA_ALTER_INPLACE_SHARED_LOCK;
  }

  DEBUG_SYNC(thd, "alter_table_inplace_after_lock_upgrade");
  THD_STAGE_INFO(thd, stage_alter_inplace_prepare);

  switch (inplace_supported) {
  case HA_ALTER_ERROR:
  case HA_ALTER_INPLACE_NOT_SUPPORTED:
    DBUG_ASSERT(0);
    // fall through
  case HA_ALTER_INPLACE_NO_LOCK:
  case HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE:
    switch (alter_info->requested_lock) {
    case Alter_info::ALTER_TABLE_LOCK_DEFAULT:
    case Alter_info::ALTER_TABLE_LOCK_NONE:
      ha_alter_info->online= true;
      break;
    case Alter_info::ALTER_TABLE_LOCK_SHARED:
    case Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE:
      break;
    }
    break;
  case HA_ALTER_INPLACE_EXCLUSIVE_LOCK:
  case HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE:
  case HA_ALTER_INPLACE_SHARED_LOCK:
    break;
  }

  if (table->file->ha_prepare_inplace_alter_table(altered_table,
                                                  ha_alter_info))
  {
    goto rollback;
  }

  /*
    Downgrade the lock if storage engine has told us that exclusive lock was
    necessary only for prepare phase (unless we are not under LOCK TABLES) and
    user has not explicitly requested exclusive lock.
  */
  if ((inplace_supported == HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE ||
       inplace_supported == HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE) &&
      !(thd->locked_tables_mode == LTM_LOCK_TABLES ||
        thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES) &&
      (alter_info->requested_lock != Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE))
  {
    /* If storage engine or user requested shared lock downgrade to SNW. */
    if (inplace_supported == HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE ||
        alter_info->requested_lock == Alter_info::ALTER_TABLE_LOCK_SHARED)
      table->mdl_ticket->downgrade_lock(MDL_SHARED_NO_WRITE);
    else
    {
      DBUG_ASSERT(inplace_supported == HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE);
      table->mdl_ticket->downgrade_lock(MDL_SHARED_UPGRADABLE);
    }
  }

  DEBUG_SYNC(thd, "alter_table_inplace_after_lock_downgrade");
  THD_STAGE_INFO(thd, stage_alter_inplace);

  if (table->file->ha_inplace_alter_table(altered_table,
                                          ha_alter_info))
  {
    goto rollback;
  }

  // Upgrade to EXCLUSIVE before commit.
  if (wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_RENAME))
    goto rollback;

  /*
    If we are killed after this point, we should ignore and continue.
    We have mostly completed the operation at this point, there should
    be no long waits left.
  */

  DBUG_EXECUTE_IF("alter_table_rollback_new_index", {
      table->file->ha_commit_inplace_alter_table(altered_table,
                                                 ha_alter_info,
                                                 false);
      my_error(ER_UNKNOWN_ERROR, MYF(0));
      goto cleanup;
    });

  DEBUG_SYNC(thd, "alter_table_inplace_before_commit");
  THD_STAGE_INFO(thd, stage_alter_inplace_commit);

  /*
    Acquire SRO locks on parent tables to prevent concurrent DML on them to
    perform cascading actions. These actions require acquring InnoDB locks,
    which might otherwise create deadlock with locks acquired by
    ha_innobase::commit_inplace_alter_table(). This deadlock can be
    be resolved by aborting expensive ALTER TABLE statement, which
    we would like to avoid.

    Note that we ignore FOREIGN_KEY_CHECKS=0 setting completely here since
    we need to avoid deadlock even if user is ready to sacrifice some
    consistency and set FOREIGN_KEY_CHECKS=0.

    It is possible that acquisition of locks on parent tables will result
    in MDL deadlocks. But since deadlocks involving two or more DDL
    statements should be rare, it is unlikely that our ALTER TABLE will
    be aborted due to such deadlock.
  */
  if (lock_fk_dependent_tables(thd, table))
    goto rollback;

  if (table->file->ha_commit_inplace_alter_table(altered_table,
                                                 ha_alter_info,
                                                 true))
  {
    goto rollback;
  }

  close_all_tables_for_name(thd, table->s, alter_ctx->is_table_renamed(), NULL);
  table_list->table= table= NULL;
  close_temporary_table(thd, altered_table, true, false);

  /*
    Replace the old .FRM with the new .FRM, but keep the old name for now.
    Rename to the new name (if needed) will be handled separately below.
  */
  if (mysql_rename_table(create_info->db_type, alter_ctx->new_db,
                         alter_ctx->tmp_name, alter_ctx->db, alter_ctx->alias,
                         FN_FROM_IS_TMP | NO_HA_TABLE))
  {
    // Since changes were done in-place, we can't revert them.
    (void) quick_rm_table(thd, create_info->db_type,
                          alter_ctx->new_db, alter_ctx->tmp_name,
                          FN_IS_TMP | NO_HA_TABLE);
    DBUG_RETURN(true);
  }

  table_list->mdl_request.ticket= mdl_ticket;
  if (open_table(thd, table_list, &ot_ctx))
    DBUG_RETURN(true);

  /*
    Tell the handler that the changed frm is on disk and table
    has been re-opened
  */
  table_list->table->file->ha_notify_table_changed();

  /*
    We might be going to reopen table down on the road, so we have to
    restore state of the TABLE object which we used for obtaining of
    handler object to make it usable for later reopening.
  */
  close_thread_table(thd, &thd->open_tables);
  table_list->table= NULL;

  // Rename altered table if requested.
  if (alter_ctx->is_table_renamed())
  {
    // Remove TABLE and TABLE_SHARE for old name from TDC.
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL,
                     alter_ctx->db, alter_ctx->table_name, false);

    if (mysql_rename_table(create_info->db_type, alter_ctx->db,
                           alter_ctx->table_name,
                           alter_ctx->new_db, alter_ctx->new_alias, 0))
    {
      /*
        If the rename fails we will still have a working table
        with the old name, but with other changes applied.
      */
      DBUG_RETURN(true);
    }
    if (change_trigger_table_name(thd,
                                  alter_ctx->db,
                                  alter_ctx->alias,
                                  alter_ctx->table_name,
                                  alter_ctx->new_db,
                                  alter_ctx->new_alias))
    {
      /*
        If the rename of trigger files fails, try to rename the table
        back so we at least have matching table and trigger files.
      */
      (void) mysql_rename_table(create_info->db_type,
                                alter_ctx->new_db, alter_ctx->new_alias,
                                alter_ctx->db, alter_ctx->alias, NO_FK_CHECKS);
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);

 rollback:
  table->file->ha_commit_inplace_alter_table(altered_table,
                                             ha_alter_info,
                                             false);
 cleanup:
  if (reopen_tables)
  {
    /* Close the only table instance which is still around. */
    close_all_tables_for_name(thd, table->s, alter_ctx->is_table_renamed(), NULL);
    if (thd->locked_tables_list.reopen_tables(thd))
      thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);
    /* QQ; do something about metadata locks ? */
  }
  close_temporary_table(thd, altered_table, true, false);
  // Delete temporary .frm/.par
  (void) quick_rm_table(thd, create_info->db_type, alter_ctx->new_db,
                        alter_ctx->tmp_name, FN_IS_TMP | NO_HA_TABLE);
  DBUG_RETURN(true);
}

/**
  maximum possible length for certain blob types.

  @param[in]      type        Blob type (e.g. MYSQL_TYPE_TINY_BLOB)

  @return
    length
*/

static uint
blob_length_by_type(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_TINY_BLOB:
    return 255;
  case MYSQL_TYPE_BLOB:
    return 65535;
  case MYSQL_TYPE_MEDIUM_BLOB:
    return 16777215;
  case MYSQL_TYPE_LONG_BLOB:
    return 4294967295U;
  default:
    DBUG_ASSERT(0); // we should never go here
    return 0;
  }
}


/**
  Convert the old temporal data types to the new temporal
  type format for ADD/CHANGE COLUMN, ADD INDEXES and ALTER
  FORCE ALTER operation.

  @param thd                Thread context.
  @param alter_info         Alter info parameters.

  @retval true              Error.
  @retval false             Either the old temporal data types
                            are not present or they are present
                            and have been successfully upgraded.
*/

static bool
upgrade_old_temporal_types(THD *thd, Alter_info *alter_info)
{
  bool old_temporal_type_present= false;

  DBUG_ENTER("upgrade_old_temporal_types");

  if (!((alter_info->flags & Alter_info::ALTER_ADD_COLUMN) ||
      (alter_info->flags & Alter_info::ALTER_ADD_INDEX) ||
      (alter_info->flags & Alter_info::ALTER_CHANGE_COLUMN) ||
      (alter_info->flags & Alter_info::ALTER_RECREATE)))
    DBUG_RETURN(false);

  /*
    Upgrade the old temporal types if any, for ADD/CHANGE COLUMN/
    ADD INDEXES and FORCE ALTER operation.
  */
  Create_field *def;
  List_iterator<Create_field> create_it(alter_info->create_list);

  while ((def= create_it++))
  {
    // Check if any old temporal type is present.
    if ((def->sql_type == MYSQL_TYPE_TIME) ||
        (def->sql_type == MYSQL_TYPE_DATETIME) ||
        (def->sql_type == MYSQL_TYPE_TIMESTAMP))
    {
       old_temporal_type_present= true;
       break;
    }
  }

  // Upgrade is not required since there are no old temporal types.
  if (!old_temporal_type_present)
    DBUG_RETURN(false);

  // Upgrade old temporal types to the new temporal types.
  create_it.rewind();
  while ((def= create_it++))
  {
    enum  enum_field_types sql_type;
    Item *default_value= def->def, *update_value= NULL;

    /*
       Set CURRENT_TIMESTAMP as default/update value based on
       the unireg_check value.
    */

    if ((def->sql_type == MYSQL_TYPE_DATETIME ||
         def->sql_type == MYSQL_TYPE_TIMESTAMP)
        && (def->unireg_check != Field::NONE))
    {
      Item_func_now_local *now = new (thd->mem_root) Item_func_now_local(0);
      if (!now)
        DBUG_RETURN(true);

      if (def->unireg_check == Field::TIMESTAMP_DN_FIELD)
        default_value= now;
      else if (def->unireg_check == Field::TIMESTAMP_UN_FIELD)
        update_value= now;
      else if (def->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
      {
        update_value= now;
        default_value= now;
      }
    }

    switch (def->sql_type)
    {
    case MYSQL_TYPE_TIME:
        sql_type= MYSQL_TYPE_TIME2;
        break;
    case MYSQL_TYPE_DATETIME:
        sql_type= MYSQL_TYPE_DATETIME2;
        break;
    case MYSQL_TYPE_TIMESTAMP:
        sql_type= MYSQL_TYPE_TIMESTAMP2;
        break;
    default:
      continue;
    }

    DBUG_ASSERT(!def->gcol_info ||
                (def->gcol_info  &&
                 (def->sql_type != MYSQL_TYPE_DATETIME
                 || def->sql_type != MYSQL_TYPE_TIMESTAMP)));
    // Replace the old temporal field with the new temporal field.
    Create_field *temporal_field= NULL;
    if (!(temporal_field= new (thd->mem_root) Create_field()) ||
        temporal_field->init(thd, def->field_name, sql_type, NULL, NULL,
                             (def->flags & NOT_NULL_FLAG), default_value,
                             update_value, &def->comment, def->change, NULL,
                             NULL, 0, NULL))
      DBUG_RETURN(true);

    temporal_field->field= def->field;
    create_it.replace(temporal_field);
  }

  // Report a NOTE informing about the upgrade.
  push_warning(thd, Sql_condition::SL_NOTE,
               ER_OLD_TEMPORALS_UPGRADED, ER(ER_OLD_TEMPORALS_UPGRADED));
  DBUG_RETURN(false);
}


/**
  Prepare column and key definitions for CREATE TABLE in ALTER TABLE.

  This function transforms parse output of ALTER TABLE - lists of
  columns and keys to add, drop or modify into, essentially,
  CREATE TABLE definition - a list of columns and keys of the new
  table. While doing so, it also performs some (bug not all)
  semantic checks.

  This function is invoked when we know that we're going to
  perform ALTER TABLE via a temporary table -- i.e. in-place ALTER TABLE
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
  @param[in,out]  alter_ctx   Runtime context for ALTER TABLE.

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

bool
mysql_prepare_alter_table(THD *thd, TABLE *table,
                          HA_CREATE_INFO *create_info,
                          Alter_info *alter_info,
                          Alter_table_ctx *alter_ctx)
{
  /* New column definitions are added here */
  List<Create_field> new_create_list;
  /* New key definitions are added here */
  List<Key> new_key_list;
  // DROP instructions for foreign keys and virtual generated columns
  List<Alter_drop> new_drop_list;
  /*
    Alter_info::alter_rename_key_list is also used by fill_alter_inplace_info()
    call. So this function should not modify original list but rather work with
    its copy.
  */
  List<Alter_rename_key> rename_key_list(alter_info->alter_rename_key_list,
                                         thd->mem_root);
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
  bool rc= true;

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

  if (!(used_fields & HA_CREATE_USED_STATS_SAMPLE_PAGES))
    create_info->stats_sample_pages= table->s->stats_sample_pages;

  if (!(used_fields & HA_CREATE_USED_STATS_AUTO_RECALC))
    create_info->stats_auto_recalc= table->s->stats_auto_recalc;

  if (!(used_fields & HA_CREATE_USED_TABLESPACE))
    create_info->tablespace= table->s->tablespace;

  if (create_info->storage_media == HA_SM_DEFAULT)
    create_info->storage_media= table->s->default_storage_media;

  /* Creation of federated table with LIKE clause needs connection string */
  if (!(used_fields & HA_CREATE_USED_CONNECTION))
    create_info->connect_string= table->s->connect_string;

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
        /*
          If a generated column is dependent on this column, this column
          cannot be dropped.
        */
        if (table->vfield &&
            table->is_field_used_by_generated_columns(field->field_index))
        {
          my_error(ER_DEPENDENT_BY_GENERATED_COLUMN, MYF(0), field->field_name);
          goto err;
        }

        /*
          Mark the drop_column operation is on virtual GC so that a non-rebuild
          on table can be done.
        */
        if (field->is_virtual_gcol())
          new_drop_list.push_back(drop);
	break; // Column was found.
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
      if (field->stored_in_db != def->stored_in_db)
      {
        my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN,
                 MYF(0),
                 "Changing the STORED status");
        goto err;
      }
      /*
        Add column being updated to the list of new columns.
        Note that columns with AFTER clauses are added to the end
        of the list for now. Their positions will be corrected later.
      */
      new_create_list.push_back(def);
      if (!def->after)
      {
        /*
          If this ALTER TABLE doesn't have an AFTER clause for the modified
          column then remove this column from the list of columns to be
          processed. So later we can iterate over the columns remaining
          in this list and process modified columns with AFTER clause or
          add new columns.
        */
        def_it.remove();
      }
      /*
        If the new column type is GEOMETRY (or a subtype) NOT NULL,
        and the old column type is nullable and not GEOMETRY (or a
        subtype), existing NULL values will be converted into empty
        strings in non-strict mode. Empty strings are illegal values
        in GEOMETRY columns.
      */
      if (def->sql_type == MYSQL_TYPE_GEOMETRY &&
          (def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)) &&
          field->type() != MYSQL_TYPE_GEOMETRY &&
          field->maybe_null() &&
          !thd->is_strict_mode())
      {
        alter_ctx->error_if_not_empty|=
          Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT;
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
	if (def->flags & BLOB_FLAG)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), field->field_name);
          goto err;
	}

	if ((def->def=alter->def))              // Use new default
        {
          def->flags&= ~NO_DEFAULT_VALUE_FLAG;
          /*
            The defaults are explicitly altered for the TIMESTAMP/DATETIME
            field, through SET DEFAULT. Hence, set the unireg check
            appropriately.
          */
          if (real_type_with_now_as_default(def->sql_type))
          {
            if (def->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
              def->unireg_check= Field::TIMESTAMP_UN_FIELD;
            else if (def->unireg_check == Field::TIMESTAMP_DN_FIELD)
              def->unireg_check= Field::NONE;
          }
        }
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
      New columns of type DATE/DATETIME/GEOMETRIC with NOT NULL constraint
      added as part of ALTER operation will generate zero date for DATE/
      DATETIME types and empty string for GEOMETRIC types when the table
      is not empty. Hence certain additional checks needs to be performed
      as described below. This cannot be caught by SE(For INPLACE ALTER)
      since it checks for only NULL value. Zero date and empty string
      does not violate the NOT NULL value constraint.
    */
    if (!def->change)
    {
      /*
        Check that the DATE/DATETIME not null field we are going to add is
        either has a default value or the '0000-00-00' is allowed by the
        set sql mode.
        If the '0000-00-00' value isn't allowed then raise the error_if_not_empty
        flag to allow ALTER TABLE only if the table to be altered is empty.
      */
      if ((def->sql_type == MYSQL_TYPE_DATE ||
           def->sql_type == MYSQL_TYPE_NEWDATE ||
           def->sql_type == MYSQL_TYPE_DATETIME ||
           def->sql_type == MYSQL_TYPE_DATETIME2) &&
          !alter_ctx->datetime_field &&
          !(~def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)))
      {
        alter_ctx->datetime_field= def;
        alter_ctx->error_if_not_empty|=
          Alter_table_ctx::DATETIME_WITHOUT_DEFAULT;
      }

      /*
        New GEOMETRY (and subtypes) columns can't be NOT NULL. To add a
        GEOMETRY NOT NULL column, first create a GEOMETRY NULL column,
        UPDATE the table to set a different value than NULL, and then do
        a ALTER TABLE MODIFY COLUMN to set NOT NULL.

        This restriction can be lifted once MySQL supports default
        values (i.e., functions) for geometry columns. The new
        restriction would then be for added GEOMETRY NOT NULL columns to
        always have a provided default value.
      */
      if (def->sql_type == MYSQL_TYPE_GEOMETRY &&
          (def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)))
      {
        alter_ctx->error_if_not_empty|= Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT;
      }
    }

    if (!def->after)
      new_create_list.push_back(def);
    else
    {
      Create_field *find;
      if (def->change)
      {
        find_it.rewind();
        /*
          For columns being modified with AFTER clause we should first remove
          these columns from the list and then add them back at their correct
          positions.
        */
        while ((find=find_it++))
        {
          /*
            Create_fields representing changed columns are added directly
            from Alter_info::create_list to new_create_list. We can therefore
            safely use pointer equality rather than name matching here.
            This prevents removing the wrong column in case of column rename.
          */
          if (find == def)
          {
            find_it.remove();
            break;
          }
        }
      }
      if (def->after == first_keyword)
        new_create_list.push_front(def);
      else
      {
        find_it.rewind();
        while ((find=find_it++))
        {
          if (!my_strcasecmp(system_charset_info, def->after, find->field_name))
            break;
        }
        if (!find)
        {
          my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table->s->table_name.str);
          goto err;
        }
        find_it.after(def);			// Put column after this
      }
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
    const char *key_name= key_info->name;
    bool index_column_dropped= false;
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
    for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
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
      {
        /*
           We are dropping a column associated with an index.
        */
        index_column_dropped= true;
	continue;				// Field is removed
      }
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
          
          In case of TEXTs we check the data type maximum length *in bytes*
          to key part length measured *in characters* (i.e. key_part_length
          devided to mbmaxlen). This is because it's OK to have:
          CREATE TABLE t1 (a tinytext, key(a(254)) character set utf8);
          In case of this example:
          - data type maximum length is 255.
          - key_part_length is 1016 (=254*4, where 4 is mbmaxlen)
         */
        if (!Field::type_can_have_key_part(cfield->field->type()) ||
            !Field::type_can_have_key_part(cfield->sql_type) ||
            /* spatial keys can't have sub-key length */
            (key_info->flags & HA_SPATIAL) ||
            (cfield->field->field_length == key_part_length &&
             !f_is_blob(key_part->key_type)) ||
            (cfield->length && (((cfield->sql_type >= MYSQL_TYPE_TINY_BLOB &&
                                  cfield->sql_type <= MYSQL_TYPE_BLOB) ? 
                                blob_length_by_type(cfield->sql_type) :
                                cfield->length) <
	     key_part_length / key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new Key_part_spec(cfield->field_name,
                                            strlen(cfield->field_name),
					    key_part_length));
    }
    if (key_parts.elements)
    {
      KEY_CREATE_INFO key_create_info;
      Key *key;
      keytype key_type;
      memset(&key_create_info, 0, sizeof(key_create_info));

      /* If this index is to stay in the table check if it has to be renamed. */
      List_iterator<Alter_rename_key> rename_key_it(rename_key_list);
      Alter_rename_key *rename_key;

      while ((rename_key= rename_key_it++))
      {
        if (! my_strcasecmp(system_charset_info, key_name,
                            rename_key->old_name))
        {
          if (! my_strcasecmp(system_charset_info, key_name,
                              primary_key_name))
          {
            my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), rename_key->old_name);
            goto err;
          }
          else if (! my_strcasecmp(system_charset_info, rename_key->new_name,
                                   primary_key_name))
          {
            my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), rename_key->new_name);
            goto err;
          }

          key_name= rename_key->new_name;
          rename_key_it.remove();
          /*
            If the user has explicitly renamed the key, we should no longer
            treat it as generated. Otherwise this key might be automatically
            dropped by mysql_prepare_create_table() and this will confuse
            code in fill_alter_inplace_info().
          */
          key_info->flags &= ~HA_GENERATED_KEY;
          break;
        }
      }

      key_create_info.algorithm= key_info->algorithm;
      if (key_info->flags & HA_USES_BLOCK_SIZE)
        key_create_info.block_size= key_info->block_size;
      if (key_info->flags & HA_USES_PARSER)
        key_create_info.parser_name= *plugin_name(key_info->parser);
      if (key_info->flags & HA_USES_COMMENT)
        key_create_info.comment= key_info->comment;

      if (key_info->flags & HA_SPATIAL)
        key_type= KEYTYPE_SPATIAL;
      else if (key_info->flags & HA_NOSAME)
      {
        if (! my_strcasecmp(system_charset_info, key_name, primary_key_name))
          key_type= KEYTYPE_PRIMARY;
        else
          key_type= KEYTYPE_UNIQUE;
      }
      else if (key_info->flags & HA_FULLTEXT)
        key_type= KEYTYPE_FULLTEXT;
      else
        key_type= KEYTYPE_MULTIPLE;
      
      if (index_column_dropped)
      {
        /*
           We have dropped a column associated with an index,
           this warrants a check for duplicate indexes
        */
        key_create_info.check_for_duplicate_indexes= true;
      }

      key= new Key(key_type, key_name, strlen(key_name),
                   &key_create_info,
                   MY_TEST(key_info->flags & HA_GENERATED_KEY),
                   key_parts);
      new_key_list.push_back(key);
    }
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type == KEYTYPE_FOREIGN &&
          ((Foreign_key *)key)->validate(new_create_list))
        goto err;
      new_key_list.push_back(key);
      if (key->name.str &&
	  !my_strcasecmp(system_charset_info, key->name.str, primary_key_name))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
        goto err;
      }
    }
  }

  if (alter_info->drop_list.elements)
  {
    // Now this contains only DROP for foreign keys and not-found objects
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++)) {
      switch (drop->type) {
      case Alter_drop::KEY:
      case Alter_drop::COLUMN:
        my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
                 alter_info->drop_list.head()->name);
        goto err;
      case Alter_drop::FOREIGN_KEY:
        break;
      default:
        DBUG_ASSERT(false);
        break;
      }
    }
    // new_drop_list has DROP for virtual generated columns; add foreign keys:
    new_drop_list.concat(&alter_info->drop_list);
  }
  if (rename_key_list.elements)
  {
    my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), rename_key_list.head()->old_name,
             table->s->table_name.str);
    goto err;
  }

  if (!create_info->comment.str)
  {
    create_info->comment.str= table->s->comment.str;
    create_info->comment.length= table->s->comment.length;
  }

  if (!create_info->compress.str)
  {
    create_info->compress.str= table->s->compress.str;
    create_info->compress.length= table->s->compress.length;
  }

  if (!create_info->encrypt_type.str)
  {
    create_info->encrypt_type.str= table->s->encrypt_type.str;
    create_info->encrypt_type.length= table->s->encrypt_type.length;
  }

  /* Do not pass the update_create_info through to each partition. */
  if (table->file->ht->db_type == DB_TYPE_PARTITION_DB)
	  create_info->data_file_name = (char*) -1;

  table->file->update_create_info(create_info);
  if ((create_info->table_options &
       (HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS)) ||
      (used_fields & HA_CREATE_USED_PACK_KEYS))
    db_create_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
  if ((create_info->table_options &
       (HA_OPTION_STATS_PERSISTENT | HA_OPTION_NO_STATS_PERSISTENT)) ||
      (used_fields & HA_CREATE_USED_STATS_PERSISTENT))
    db_create_options&= ~(HA_OPTION_STATS_PERSISTENT | HA_OPTION_NO_STATS_PERSISTENT);
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

  rc= false;
  alter_info->create_list.swap(new_create_list);
  alter_info->key_list.swap(new_key_list);
  alter_info->drop_list.swap(new_drop_list);
err:
  DBUG_RETURN(rc);
}


/**
  Get Create_field object for newly created table by its name
  in the old version of table.

  @param alter_info  Alter_info describing newly created table.
  @param old_name    Name of field in old table.

  @returns Pointer to Create_field object, NULL - if field is
           not present in new version of table.
*/

static Create_field *get_field_by_old_name(Alter_info *alter_info,
                                           const char *old_name)
{
  List_iterator_fast<Create_field> new_field_it(alter_info->create_list);
  Create_field *new_field;

  while ((new_field= new_field_it++))
  {
    if (new_field->field &&
        (my_strcasecmp(system_charset_info,
                       new_field->field->field_name,
                       old_name) == 0))
      break;
  }
  return new_field;
}


/** Type of change to foreign key column, */

enum fk_column_change_type
{
  FK_COLUMN_NO_CHANGE, FK_COLUMN_DATA_CHANGE,
  FK_COLUMN_RENAMED, FK_COLUMN_DROPPED
};


/**
  Check that ALTER TABLE's changes on columns of a foreign key are allowed.

  @param[in]   thd              Thread context.
  @param[in]   alter_info       Alter_info describing changes to be done
                                by ALTER TABLE.
  @param[in]   fk_columns       List of columns of the foreign key to check.
  @param[out]  bad_column_name  Name of field on which ALTER TABLE tries to
                                do prohibited operation.

  @note This function takes into account value of @@foreign_key_checks
        setting.

  @retval FK_COLUMN_NO_CHANGE    No significant changes are to be done on
                                 foreign key columns.
  @retval FK_COLUMN_DATA_CHANGE  ALTER TABLE might result in value
                                 change in foreign key column (and
                                 foreign_key_checks is on).
  @retval FK_COLUMN_RENAMED      Foreign key column is renamed.
  @retval FK_COLUMN_DROPPED      Foreign key column is dropped.
*/

static enum fk_column_change_type
fk_check_column_changes(THD *thd, Alter_info *alter_info,
                        List<LEX_STRING> &fk_columns,
                        const char **bad_column_name)
{
  List_iterator_fast<LEX_STRING> column_it(fk_columns);
  LEX_STRING *column;

  *bad_column_name= NULL;

  while ((column= column_it++))
  {
    Create_field *new_field= get_field_by_old_name(alter_info, column->str);

    if (new_field)
    {
      Field *old_field= new_field->field;

      if (my_strcasecmp(system_charset_info, old_field->field_name,
                        new_field->field_name))
      {
        /*
          Copy algorithm doesn't support proper renaming of columns in
          the foreign key yet. At the moment we lack API which will tell
          SE that foreign keys should be updated to use new name of column
          like it happens in case of in-place algorithm.
        */
        *bad_column_name= column->str;
        return FK_COLUMN_RENAMED;
      }

      if ((old_field->is_equal(new_field) == IS_EQUAL_NO) ||
          ((new_field->flags & NOT_NULL_FLAG) &&
           !(old_field->flags & NOT_NULL_FLAG)))
      {
        if (!(thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
        {
          /*
            Column in a FK has changed significantly. Unless
            foreign_key_checks are off we prohibit this since this
            means values in this column might be changed by ALTER
            and thus referential integrity might be broken,
          */
          *bad_column_name= column->str;
          return FK_COLUMN_DATA_CHANGE;
        }
      }
      DBUG_ASSERT(old_field->is_gcol() == new_field->is_gcol() &&
                  old_field->is_virtual_gcol() == new_field->is_virtual_gcol());
      DBUG_ASSERT(!old_field->is_gcol() ||
                  old_field->gcol_expr_is_equal(new_field));
    }
    else
    {
      /*
        Column in FK was dropped. Most likely this will break
        integrity constraints of InnoDB data-dictionary (and thus
        InnoDB will emit an error), so we prohibit this right away
        even if foreign_key_checks are off.
        This also includes a rare case when another field replaces
        field being dropped since it is easy to break referential
        integrity in this case.
      */
      *bad_column_name= column->str;
      return FK_COLUMN_DROPPED;
    }
  }

  return FK_COLUMN_NO_CHANGE;
}


/**
  Check if ALTER TABLE we are about to execute using COPY algorithm
  is not supported as it might break referential integrity.

  @note If foreign_key_checks is disabled (=0), we allow to break
        referential integrity. But we still disallow some operations
        like dropping or renaming columns in foreign key since they
        are likely to break consistency of InnoDB data-dictionary
        and thus will end-up in error anyway.

  @param[in]  thd          Thread context.
  @param[in]  table        Table to be altered.
  @param[in]  alter_info   Lists of fields, keys to be changed, added
                           or dropped.

  @retval false  Success.
  @retval true   Error, ALTER - tries to do change which is not compatible
                 with foreign key definitions on the table.
*/

static bool fk_check_copy_alter_table(THD *thd, TABLE *table,
                                      Alter_info *alter_info)
{
  List <FOREIGN_KEY_INFO> fk_parent_key_list;
  List <FOREIGN_KEY_INFO> fk_child_key_list;
  FOREIGN_KEY_INFO *f_key;

  DBUG_ENTER("fk_check_copy_alter_table");

  table->file->get_parent_foreign_key_list(thd, &fk_parent_key_list);

  /* OOM when building list. */
  if (thd->is_error())
    DBUG_RETURN(true);

  /*
    Remove from the list all foreign keys in which table participates as
    parent which are to be dropped by this ALTER TABLE. This is possible
    when a foreign key has the same table as child and parent.
  */
  List_iterator<FOREIGN_KEY_INFO> fk_parent_key_it(fk_parent_key_list);

  while ((f_key= fk_parent_key_it++))
  {
    Alter_drop *drop;
    List_iterator_fast<Alter_drop> drop_it(alter_info->drop_list);

    while ((drop= drop_it++))
    {
      /*
        InnoDB treats foreign key names in case-insensitive fashion.
        So we do it here too. For database and table name type of
        comparison used depends on lower-case-table-names setting.
        For l_c_t_n = 0 we use case-sensitive comparison, for
        l_c_t_n > 0 modes case-insensitive comparison is used.
      */
      if ((drop->type == Alter_drop::FOREIGN_KEY) &&
          (my_strcasecmp(system_charset_info, f_key->foreign_id->str,
                         drop->name) == 0) &&
          (my_strcasecmp(table_alias_charset, f_key->foreign_db->str,
                         table->s->db.str) == 0) &&
          (my_strcasecmp(table_alias_charset, f_key->foreign_table->str,
                         table->s->table_name.str) == 0))
        fk_parent_key_it.remove();
    }
  }

  fk_parent_key_it.rewind();
  while ((f_key= fk_parent_key_it++))
  {
    enum fk_column_change_type changes;
    const char *bad_column_name;

    changes= fk_check_column_changes(thd, alter_info,
                                     f_key->referenced_fields,
                                     &bad_column_name);

    switch(changes)
    {
    case FK_COLUMN_NO_CHANGE:
      /* No significant changes. We can proceed with ALTER! */
      break;
    case FK_COLUMN_DATA_CHANGE:
    {
      char buff[NAME_LEN*2+2];
      strxnmov(buff, sizeof(buff)-1, f_key->foreign_db->str, ".",
               f_key->foreign_table->str, NullS);
      my_error(ER_FK_COLUMN_CANNOT_CHANGE_CHILD, MYF(0), bad_column_name,
               f_key->foreign_id->str, buff);
      DBUG_RETURN(true);
    }
    case FK_COLUMN_RENAMED:
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=COPY",
               ER(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME),
               "ALGORITHM=INPLACE");
      DBUG_RETURN(true);
    case FK_COLUMN_DROPPED:
    {
      char buff[NAME_LEN*2+2];
      strxnmov(buff, sizeof(buff)-1, f_key->foreign_db->str, ".",
               f_key->foreign_table->str, NullS);
      my_error(ER_FK_COLUMN_CANNOT_DROP_CHILD, MYF(0), bad_column_name,
               f_key->foreign_id->str, buff);
      DBUG_RETURN(true);
    }
    default:
      DBUG_ASSERT(0);
    }
  }

  table->file->get_foreign_key_list(thd, &fk_child_key_list);

  /* OOM when building list. */
  if (thd->is_error())
    DBUG_RETURN(true);

  /*
    Remove from the list all foreign keys which are to be dropped
    by this ALTER TABLE.
  */
  List_iterator<FOREIGN_KEY_INFO> fk_key_it(fk_child_key_list);

  while ((f_key= fk_key_it++))
  {
    Alter_drop *drop;
    List_iterator_fast<Alter_drop> drop_it(alter_info->drop_list);

    while ((drop= drop_it++))
    {
      /* Names of foreign keys in InnoDB are case-insensitive. */
      if ((drop->type == Alter_drop::FOREIGN_KEY) &&
          (my_strcasecmp(system_charset_info, f_key->foreign_id->str,
                         drop->name) == 0))
        fk_key_it.remove();
    }
  }

  fk_key_it.rewind();
  while ((f_key= fk_key_it++))
  {
    enum fk_column_change_type changes;
    const char *bad_column_name;

    changes= fk_check_column_changes(thd, alter_info,
                                     f_key->foreign_fields,
                                     &bad_column_name);

    switch(changes)
    {
    case FK_COLUMN_NO_CHANGE:
      /* No significant changes. We can proceed with ALTER! */
      break;
    case FK_COLUMN_DATA_CHANGE:
      my_error(ER_FK_COLUMN_CANNOT_CHANGE, MYF(0), bad_column_name,
               f_key->foreign_id->str);
      DBUG_RETURN(true);
    case FK_COLUMN_RENAMED:
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=COPY",
               ER(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME),
               "ALGORITHM=INPLACE");
      DBUG_RETURN(true);
    case FK_COLUMN_DROPPED:
      my_error(ER_FK_COLUMN_CANNOT_DROP, MYF(0), bad_column_name,
               f_key->foreign_id->str);
      DBUG_RETURN(true);
    default:
      DBUG_ASSERT(0);
    }
  }

  DBUG_RETURN(false);
}


/**
  Rename table and/or turn indexes on/off without touching .FRM

  @param thd            Thread handler
  @param table_list     TABLE_LIST for the table to change
  @param keys_onoff     ENABLE or DISABLE KEYS?
  @param alter_ctx      ALTER TABLE runtime context.

  @return Operation status
    @retval false           Success
    @retval true            Failure
*/

static bool
simple_rename_or_index_change(THD *thd, TABLE_LIST *table_list,
                              Alter_info::enum_enable_or_disable keys_onoff,
                              Alter_table_ctx *alter_ctx)
{
  TABLE *table= table_list->table;
  MDL_ticket *mdl_ticket= table->mdl_ticket;
  int error= 0;
  DBUG_ENTER("simple_rename_or_index_change");

  if (keys_onoff != Alter_info::LEAVE_AS_IS)
  {
    if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      DBUG_RETURN(true);

    // It's now safe to take the table level lock.
    if (lock_tables(thd, table_list, alter_ctx->tables_opened, 0))
      DBUG_RETURN(true);

    if (keys_onoff == Alter_info::ENABLE)
    {
      DEBUG_SYNC(thd,"alter_table_enable_indexes");
      DBUG_EXECUTE_IF("sleep_alter_enable_indexes", my_sleep(6000000););
      error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
    }
    else if (keys_onoff == Alter_info::DISABLE)
      error=table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);

    if (error == HA_ERR_WRONG_COMMAND)
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                          table->alias);
      error= 0;
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
  }

  if (!error && alter_ctx->is_table_renamed())
  {
    THD_STAGE_INFO(thd, stage_rename);
    handlerton *old_db_type= table->s->db_type();
    /*
      Then do a 'simple' rename of the table. First we need to close all
      instances of 'source' table.
      Note that if wait_while_table_is_used() returns error here (i.e. if
      this thread was killed) then it must be that previous step of
      simple rename did nothing and therefore we can safely return
      without additional clean-up.
    */
    if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      DBUG_RETURN(true);
    close_all_tables_for_name(thd, table->s, true, NULL);

    if (mysql_rename_table(old_db_type, alter_ctx->db, alter_ctx->table_name,
                           alter_ctx->new_db, alter_ctx->new_alias, 0))
      error= -1;
    else if (change_trigger_table_name(thd,
                                       alter_ctx->db,
                                       alter_ctx->alias,
                                       alter_ctx->table_name,
                                       alter_ctx->new_db,
                                       alter_ctx->new_alias))
    {
      (void) mysql_rename_table(old_db_type,
                                alter_ctx->new_db, alter_ctx->new_alias,
                                alter_ctx->db, alter_ctx->table_name, 
                                NO_FK_CHECKS);
      error= -1;
    }
  }

  if (!error)
  {
    error= write_bin_log(thd, true, thd->query().str, thd->query().length);
    if (!error)
      my_ok(thd);
  }
  table_list->table= NULL;                    // For query cache
  query_cache.invalidate(thd, table_list, FALSE);

  if ((thd->locked_tables_mode == LTM_LOCK_TABLES ||
       thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES))
  {
    /*
      Under LOCK TABLES we should adjust meta-data locks before finishing
      statement. Otherwise we can rely on them being released
      along with the implicit commit.
    */
    if (alter_ctx->is_table_renamed())
      thd->mdl_context.release_all_locks_for_name(mdl_ticket);
    else
      mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);
  }
  DBUG_RETURN(error != 0);
}


/**
  Auxiliary class implementing RAII principle for getting permission for/
  notification about finished ALTER TABLE from interested storage engines.

  @see handlerton::notify_alter_table for details.
*/

class Alter_table_hton_notification_guard
{
public:
  Alter_table_hton_notification_guard(THD *thd, const MDL_key *key)
    : m_hton_notified(false), m_thd(thd), m_key(key)
  {
  }

  bool notify()
  {
    if (!ha_notify_alter_table(m_thd, &m_key, HA_NOTIFY_PRE_EVENT))
    {
      m_hton_notified= true;
      return false;
    }
    my_error(ER_LOCK_REFUSED_BY_ENGINE, MYF(0));
    return true;
  }

  ~Alter_table_hton_notification_guard()
  {
    if (m_hton_notified)
      (void) ha_notify_alter_table(m_thd, &m_key, HA_NOTIFY_POST_EVENT);
  }
private:
  bool m_hton_notified;
  THD *m_thd;
  const MDL_key m_key;
};


/**
  Alter table

  @param thd              Thread handle
  @param new_db           If there is a RENAME clause
  @param new_name         If there is a RENAME clause
  @param create_info      Information from the parsing phase about new
                          table properties.
  @param table_list       The table to change.
  @param alter_info       Lists of fields, keys to be changed, added
                          or dropped.

  @retval   true          Error
  @retval   false         Success

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

  Altering a table can be done in two ways. The table can be modified
  directly using an in-place algorithm, or the changes can be done using
  an intermediate temporary table (copy). In-place is the preferred
  algorithm as it avoids copying table data. The storage engine
  selects which algorithm to use in check_if_supported_inplace_alter()
  based on information about the table changes from fill_alter_inplace_info().
*/

bool mysql_alter_table(THD *thd, const char *new_db, const char *new_name,
                       HA_CREATE_INFO *create_info,
                       TABLE_LIST *table_list,
                       Alter_info *alter_info)
{
  class Silence_deprecation_warnings: public Internal_error_handler
  {
  private:
    THD *m_thd;
  public:
    Silence_deprecation_warnings(THD *thd): m_thd(thd)
    { m_thd->push_internal_handler(this); }
    bool handle_condition(THD *thd,
                          uint sql_errno,
                          const char* sqlstate,
                          Sql_condition::enum_severity_level *level,
                          const char* msg)
    {
      if (sql_errno == ER_WARN_DEPRECATED_SYNTAX)
        return true;

      return false;
    }
    void pop()
    {
      if (m_thd)
        m_thd->pop_internal_handler();
      m_thd= NULL;
    }
    ~Silence_deprecation_warnings()
    { pop(); }
  };

  DBUG_ENTER("mysql_alter_table");

  Silence_deprecation_warnings deprecation_silencer(thd);
  bool is_partitioned= false;

  /*
    Check if we attempt to alter mysql.slow_log or
    mysql.general_log table and return an error if
    it is the case.
    TODO: this design is obsolete and will be removed.
  */
  enum_log_table_type table_kind=
    query_logger.check_if_log_table(table_list, false);

  if (table_kind != QUERY_LOG_NONE)
  {
    /* Disable alter of enabled query log tables */
    if (query_logger.is_log_table_enabled(table_kind))
    {
      my_error(ER_BAD_LOG_STATEMENT, MYF(0), "ALTER");
      DBUG_RETURN(true);
    }

    /* Disable alter of log tables to unsupported engine */
    if ((create_info->used_fields & HA_CREATE_USED_ENGINE) &&
        (!create_info->db_type || /* unknown engine */
         !(create_info->db_type->flags & HTON_SUPPORT_LOG_TABLES)))
    {
      my_error(ER_UNSUPORTED_LOG_ENGINE, MYF(0));
      DBUG_RETURN(true);
    }

    if (alter_info->flags & Alter_info::ALTER_PARTITION)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "PARTITION", "log table");
      DBUG_RETURN(true);
    }
  }

  if (alter_info->with_validation != Alter_info::ALTER_VALIDATION_DEFAULT &&
      !(alter_info->flags & 
        (Alter_info::ALTER_ADD_COLUMN | Alter_info::ALTER_CHANGE_COLUMN)))
  {
    my_error(ER_WRONG_USAGE, MYF(0), "ALTER","WITH VALIDATION");
    DBUG_RETURN(true);
  }

  THD_STAGE_INFO(thd, stage_init);

  /*
    Assign target tablespace name to enable locking in lock_table_names().
    Reject invalid names.
  */
  if (create_info->tablespace)
  {
    if (check_tablespace_name(create_info->tablespace) != IDENT_NAME_OK)
      DBUG_RETURN(true);

    if (!thd->make_lex_string(&table_list->target_tablespace_name,
                              create_info->tablespace,
                              strlen(create_info->tablespace), false))
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      DBUG_RETURN(true);
    }
  }

  // Reject invalid tablespace names specified for partitions.
  if (check_partition_tablespace_names(thd->lex->part_info))
    DBUG_RETURN(true);

  /*
    Assign the partition info, so that the locks on tablespaces
    assigned for any new partitions added would be acuired during
    open_table.
  */
  thd->work_part_info= thd->lex->part_info;

  /*
    Code below can handle only base tables so ensure that we won't open a view.
    Note that RENAME TABLE the only ALTER clause which is supported for views
    has been already processed.
  */
  table_list->required_type= FRMTYPE_TABLE;

  /*
    If we are about to ALTER non-temporary table we need to get permission
    from/notify interested storage engines.
  */
  Alter_table_hton_notification_guard notification_guard(thd,
                                        &table_list->mdl_request.key);

  if (!is_temporary_table(table_list) && notification_guard.notify())
    DBUG_RETURN(true);

  Alter_table_prelocking_strategy alter_prelocking_strategy;

  DEBUG_SYNC(thd, "alter_table_before_open_tables");
  uint tables_opened;
  bool error= open_tables(thd, &table_list, &tables_opened, 0,
                          &alter_prelocking_strategy);

  DEBUG_SYNC(thd, "alter_opened_table");

  if (error)
    DBUG_RETURN(true);

  /*
    Check if ALTER TABLE ... ENGINE is disallowed by the desired storage
    engine.
  */
  if (table_list->table->s->db_type() != create_info->db_type &&
      (alter_info->flags & Alter_info::ALTER_OPTIONS) &&
      (create_info->used_fields & HA_CREATE_USED_ENGINE) &&
       ha_is_storage_engine_disabled(create_info->db_type))
  {
    /*
      If NO_ENGINE_SUBSTITUTION is disabled, then report a warning and do not
      alter the table.
    */
    if (is_engine_substitution_allowed(thd))
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_UNKNOWN_STORAGE_ENGINE,
                          ER(ER_UNKNOWN_STORAGE_ENGINE),
                          ha_resolve_storage_engine_name(create_info->db_type));
      create_info->db_type= table_list->table->s->db_type();
    }
    else
    {
      my_error(ER_DISABLED_STORAGE_ENGINE, MYF(0),
               ha_resolve_storage_engine_name(create_info->db_type));
      DBUG_RETURN(true);
    }
  }

  TABLE *table= table_list->table;
  table->use_all_columns();
  MDL_ticket *mdl_ticket= table->mdl_ticket;

  /*
    Check if the source table is non-natively partitioned. This will be
    used for pushing a deprecation warning in cases like adding/dropping
    partitions, table rename, and ALTER INPLACE. For ALTER COPY, we need
    to check the destination table.
  */
  is_partitioned= table->s->db_type() &&
          is_ha_partition_handlerton(table->s->db_type());

  /*
    Prohibit changing of the UNION list of a non-temporary MERGE table
    under LOCK tables. It would be quite difficult to reuse a shrinked
    set of tables from the old table or to open a new TABLE object for
    an extended list and verify that they belong to locked tables.
  */
  if ((thd->locked_tables_mode == LTM_LOCK_TABLES ||
       thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES) &&
      (create_info->used_fields & HA_CREATE_USED_UNION) &&
      (table->s->tmp_table == NO_TMP_TABLE))
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(true);
  }

  Alter_table_ctx alter_ctx(thd, table_list, tables_opened, new_db, new_name);

  /*
    Add old and new (if any) databases to the list of accessed databases
    for this statement. Needed for MTS.
  */
  thd->add_to_binlog_accessed_dbs(alter_ctx.db);
  if (alter_ctx.is_database_changed())
    thd->add_to_binlog_accessed_dbs(alter_ctx.new_db);

  MDL_request target_mdl_request;

  /* Check that we are not trying to rename to an existing table */
  if (alter_ctx.is_table_renamed())
  {
    if (table->s->tmp_table != NO_TMP_TABLE)
    {
      if (find_temporary_table(thd, alter_ctx.new_db, alter_ctx.new_name))
      {
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alter_ctx.new_alias);
        DBUG_RETURN(true);
      }
    }
    else
    {
      MDL_request_list mdl_requests;
      MDL_request target_db_mdl_request;

      MDL_REQUEST_INIT(&target_mdl_request,
                       MDL_key::TABLE,
                       alter_ctx.new_db, alter_ctx.new_name,
                       MDL_EXCLUSIVE, MDL_TRANSACTION);
      mdl_requests.push_front(&target_mdl_request);

      /*
        If we are moving the table to a different database, we also
        need IX lock on the database name so that the target database
        is protected by MDL while the table is moved.
      */
      if (alter_ctx.is_database_changed())
      {
        MDL_REQUEST_INIT(&target_db_mdl_request,
                         MDL_key::SCHEMA, alter_ctx.new_db, "",
                         MDL_INTENTION_EXCLUSIVE,
                         MDL_TRANSACTION);
        mdl_requests.push_front(&target_db_mdl_request);
      }

      /*
        Global intention exclusive lock must have been already acquired when
        table to be altered was open, so there is no need to do it here.
      */
      DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::GLOBAL,
                                     "", "", MDL_INTENTION_EXCLUSIVE));

      if (thd->mdl_context.acquire_locks(&mdl_requests,
                                         thd->variables.lock_wait_timeout))
        DBUG_RETURN(true);

      DEBUG_SYNC(thd, "locked_table_name");
      /*
        Table maybe does not exist, but we got an exclusive lock
        on the name, now we can safely try to find out for sure.
      */
      if (!access(alter_ctx.get_new_filename(), F_OK))
      {
        /* Table will be closed in do_command() */
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alter_ctx.new_alias);
        DBUG_RETURN(true);
      }
    }
  }

  if (!create_info->db_type)
  {
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
      create_info->db_type= table->s->db_type();
  }

  if (check_engine(thd, alter_ctx.new_db, alter_ctx.new_name, create_info))
    DBUG_RETURN(true);

  if (create_info->db_type != table->s->db_type() &&
      !table->file->can_switch_engines())
  {
    my_error(ER_ROW_IS_REFERENCED, MYF(0));
    DBUG_RETURN(true);
  }

  /*
   If foreign key is added then check permission to access parent table.

   In function "check_fk_parent_table_access", create_info->db_type is used
   to identify whether engine supports FK constraint or not. Since
   create_info->db_type is set here, check to parent table access is delayed
   till this point for the alter operation.
  */
  if ((alter_info->flags & Alter_info::ADD_FOREIGN_KEY) &&
      check_fk_parent_table_access(thd, alter_ctx.new_db,
                                   create_info, alter_info))
    DBUG_RETURN(true);

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
             ha_resolve_storage_engine_name(table->s->db_type()),
             ha_resolve_storage_engine_name(create_info->db_type)));
  if (ha_check_storage_engine_flag(table->s->db_type(), HTON_ALTER_NOT_SUPPORTED) ||
      ha_check_storage_engine_flag(create_info->db_type, HTON_ALTER_NOT_SUPPORTED))
  {
    DBUG_PRINT("info", ("doesn't support alter"));
    my_error(ER_ILLEGAL_HA, MYF(0), table_list->table_name);
    DBUG_RETURN(true);
  }

  THD_STAGE_INFO(thd, stage_setup);
  if (!(alter_info->flags & ~(Alter_info::ALTER_RENAME |
                              Alter_info::ALTER_KEYS_ONOFF)) &&
      alter_info->requested_algorithm !=
      Alter_info::ALTER_TABLE_ALGORITHM_COPY &&
      !table->s->tmp_table) // no need to touch frm
  {
    // This requires X-lock, no other lock levels supported.
    if (alter_info->requested_lock != Alter_info::ALTER_TABLE_LOCK_DEFAULT &&
        alter_info->requested_lock != Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
               "LOCK=NONE/SHARED", "LOCK=EXCLUSIVE");
      DBUG_RETURN(true);
    }
    deprecation_silencer.pop();
    if (is_partitioned)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX,
                          ER_THD(thd,
                                 ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                          alter_ctx.new_db, alter_ctx.new_alias);
    DBUG_RETURN(simple_rename_or_index_change(thd, table_list,
                                              alter_info->keys_onoff,
                                              &alter_ctx));
  }

  /* We have to do full alter table. */

  bool partition_changed= false;
  partition_info *new_part_info= NULL;
  {
    if (prep_alter_part_table(thd, table, alter_info, create_info,
                              &alter_ctx, &partition_changed,
                              &new_part_info))
    {
      DBUG_RETURN(true);
    }
    if (partition_changed &&
        (!table->file->ht->partition_flags ||
         (table->file->ht->partition_flags() & HA_CANNOT_PARTITION_FK)) &&
        !table->file->can_switch_engines())
    {
      /*
        Partitioning was changed (added/changed/removed) and the current
        handler does not support partitioning and FK relationship exists
        for the table.

        Since the current handler does not support native partitioning, it will
        be altered to use ha_partition which does not support foreign keys.
      */
      my_error(ER_FOREIGN_KEY_ON_PARTITIONED, MYF(0));
      DBUG_RETURN(true);
    }
  }

  if (mysql_prepare_alter_table(thd, table, create_info, alter_info,
                                &alter_ctx))
  {
    DBUG_RETURN(true);
  }

  set_table_default_charset(thd, create_info, const_cast<char*>(alter_ctx.db));

  if (new_part_info)
  {
    /*
      ALGORITHM and LOCK clauses are generally not allowed by the
      parser for operations related to partitioning.
      The exceptions are ALTER_PARTITION, ALTER_UPGRADE_PARTITIONING
      and ALTER_REMOVE_PARTITIONING.
      The two first should be meta-data only changes and allowed with
      INPLACE.
      For consistency, we report ER_ALTER_OPERATION_NOT_SUPPORTED for other
      combinations.
    */
    if (alter_info->requested_lock !=
        Alter_info::ALTER_TABLE_LOCK_DEFAULT)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "LOCK=NONE/SHARED/EXCLUSIVE",
               ER(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION),
               "LOCK=DEFAULT");
      DBUG_RETURN(true);
    }
    else if (alter_info->requested_algorithm !=
             Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT &&
             !((alter_info->flags == Alter_info::ALTER_PARTITION ||
                alter_info->flags == Alter_info::ALTER_UPGRADE_PARTITIONING) &&
               alter_info->requested_algorithm ==
                 Alter_info::ALTER_TABLE_ALGORITHM_INPLACE))
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=COPY/INPLACE",
               ER(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION),
               "ALGORITHM=DEFAULT");
      DBUG_RETURN(true);
    }

    /*
      Upgrade from MDL_SHARED_UPGRADABLE to MDL_SHARED_NO_WRITE.
      Afterwards it's safe to take the table level lock.
    */
    if (thd->mdl_context.upgrade_shared_lock(mdl_ticket, MDL_SHARED_NO_WRITE,
                                             thd->variables.lock_wait_timeout)
        || lock_tables(thd, table_list, alter_ctx.tables_opened, 0))
    {
      DBUG_RETURN(true);
    }

    char* table_name= const_cast<char*>(alter_ctx.table_name);
    deprecation_silencer.pop();
    if (is_partitioned)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX,
                          ER_THD(thd,
                                 ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                          alter_ctx.new_db, alter_ctx.new_alias);
    // In-place execution of ALTER TABLE for partitioning.
    DBUG_RETURN(fast_alter_partition_table(thd, table, alter_info,
                                           create_info, table_list,
                                           const_cast<char*>(alter_ctx.db),
                                           table_name,
                                           new_part_info));
  }

  /*
    Use copy algorithm if:
    - old_alter_table system variable is set without in-place requested using
      the ALGORITHM clause.
    - Or if in-place is impossible for given operation.
    - Changes to partitioning which were not handled by
      fast_alter_partition_table() needs to be handled using table copying
      algorithm unless the engine supports auto-partitioning as such engines
      can do some changes using in-place API.
  */
  if ((thd->variables.old_alter_table &&
       alter_info->requested_algorithm !=
       Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
      || is_inplace_alter_impossible(table, create_info, alter_info, &alter_ctx)
      || (partition_changed &&
          !(table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION))
     )
  {
    if (alter_info->requested_algorithm ==
        Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
               "ALGORITHM=INPLACE", "ALGORITHM=COPY");
      DBUG_RETURN(true);
    }
    alter_info->requested_algorithm= Alter_info::ALTER_TABLE_ALGORITHM_COPY;
  }

  /*
    If 'avoid_temporal_upgrade' mode is not enabled, then the
    pre MySQL 5.6.4 old temporal types if present is upgraded to the
    current format.
  */

  mysql_mutex_lock(&LOCK_global_system_variables);
  bool check_temporal_upgrade= !avoid_temporal_upgrade;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  if (check_temporal_upgrade)
  {
    if (upgrade_old_temporal_types(thd, alter_info))
      DBUG_RETURN(true);
  }

  /*
    ALTER TABLE ... ENGINE to the same engine is a common way to
    request table rebuild. Set ALTER_RECREATE flag to force table
    rebuild.
  */
  if (create_info->db_type == table->s->db_type() &&
      create_info->used_fields & HA_CREATE_USED_ENGINE)
    alter_info->flags|= Alter_info::ALTER_RECREATE;

  /*
    If the old table had partitions and we are doing ALTER TABLE ...
    engine= <new_engine>, the new table must preserve the original
    partitioning. This means that the new engine is still the
    partitioning engine, not the engine specified in the parser.
    This is discovered in prep_alter_part_table, which in such case
    updates create_info->db_type.
    It's therefore important that the assignment below is done
    after prep_alter_part_table.
  */
  handlerton *new_db_type= create_info->db_type;
  handlerton *old_db_type= table->s->db_type();
  TABLE *new_table= NULL;
  ha_rows copied=0,deleted=0;

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
  char index_file[FN_REFLEN], data_file[FN_REFLEN];

  if (!alter_ctx.is_database_changed())
  {
    if (create_info->index_file_name)
    {
      /* Fix index_file_name to have 'tmp_name' as basename */
      my_stpcpy(index_file, alter_ctx.tmp_name);
      create_info->index_file_name=fn_same(index_file,
                                           create_info->index_file_name,
                                           1);
    }
    if (create_info->data_file_name)
    {
      /* Fix data_file_name to have 'tmp_name' as basename */
      my_stpcpy(data_file, alter_ctx.tmp_name);
      create_info->data_file_name=fn_same(data_file,
                                          create_info->data_file_name,
                                          1);
    }
  }
  else
  {
    /* Ignore symlink if db is changed. */
    create_info->data_file_name=create_info->index_file_name=0;
  }

  DEBUG_SYNC(thd, "alter_table_before_create_table_no_lock");
  DBUG_EXECUTE_IF("sleep_before_create_table_no_lock",
                  my_sleep(100000););
  /*
    Promote first timestamp column, when explicit_defaults_for_timestamp
    is not set
  */
  if (!thd->variables.explicit_defaults_for_timestamp)
    promote_first_timestamp_column(&alter_info->create_list);

  /*
    Create .FRM for new version of table with a temporary name.
    We don't log the statement, it will be logged later.

    Keep information about keys in newly created table as it
    will be used later to construct Alter_inplace_info object
    and by fill_alter_inplace_info() call.
  */
  KEY *key_info;
  uint key_count;
  /*
    Remember if the new definition has new VARCHAR column;
    create_info->varchar will be reset in create_table_impl()/
    mysql_prepare_create_table().
  */
  bool varchar= create_info->varchar;

  tmp_disable_binlog(thd);
  error= create_table_impl(thd, alter_ctx.new_db, alter_ctx.tmp_name,
                           alter_ctx.table_name,
                           alter_ctx.get_tmp_path(),
                           create_info, alter_info,
                           true, 0, true, NULL,
                           &key_info, &key_count);
  reenable_binlog(thd);

  if (error)
    DBUG_RETURN(true);

  /*
    We want warnings/errors about data truncation emitted when new
    version of table is created in COPY algorithm or when values of
    virtual columns are evaluated in INPLACE algorithm.
  */
  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields= 0L;

  /* Remember that we have not created table in storage engine yet. */
  bool no_ha_table= true;

  if (alter_info->requested_algorithm != Alter_info::ALTER_TABLE_ALGORITHM_COPY)
  {
    Alter_inplace_info ha_alter_info(create_info, alter_info,
                                     key_info, key_count,
                                     thd->work_part_info
                                     );
    TABLE *altered_table= NULL;
    bool use_inplace= true;

    /* Fill the Alter_inplace_info structure. */
    if (fill_alter_inplace_info(thd, table, varchar, &ha_alter_info))
      goto err_new_table_cleanup;

    DBUG_EXECUTE_IF("innodb_index_drop_count_zero",
                    {
                      if (ha_alter_info.index_drop_count)
                      {
                        my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
                                 "Index rebuild", "Without rebuild");
                        DBUG_RETURN(true);
                      }
                    };);

   DBUG_EXECUTE_IF("innodb_index_drop_count_one",
                    {
                      if (ha_alter_info.index_drop_count != 1)
                      {
                        my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
                                 "Index change", "Index rebuild");
                        DBUG_RETURN(true);
                      }
                    };);

    // We assume that the table is non-temporary.
    DBUG_ASSERT(!table->s->tmp_table);

    if (!(altered_table= open_table_uncached(thd, alter_ctx.get_tmp_path(),
                                             alter_ctx.new_db,
                                             alter_ctx.tmp_name,
                                             true, false)))
      goto err_new_table_cleanup;

    /* Set markers for fields in TABLE object for altered table. */
    update_altered_table(ha_alter_info, altered_table);

    /*
      Mark all columns in 'altered_table' as used to allow usage
      of its record[0] buffer and Field objects during in-place
      ALTER TABLE.
    */
    altered_table->column_bitmaps_set_no_signal(&altered_table->s->all_set,
                                                &altered_table->s->all_set);

    set_column_defaults(altered_table, alter_info->create_list);

    if (ha_alter_info.handler_flags == 0)
    {
      /*
        No-op ALTER, no need to call handler API functions.

        If this code path is entered for an ALTER statement that
        should not be a real no-op, new handler flags should be added
        and fill_alter_inplace_info() adjusted.

        Note that we can end up here if an ALTER statement has clauses
        that cancel each other out (e.g. ADD/DROP identically index).

        Also note that we ignore the LOCK clause here.
      */
      close_temporary_table(thd, altered_table, true, false);
      (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                            alter_ctx.tmp_name, FN_IS_TMP | NO_HA_TABLE);
      goto end_inplace;
    }

    // Ask storage engine whether to use copy or in-place
    enum_alter_inplace_result inplace_supported=
      table->file->check_if_supported_inplace_alter(altered_table,
                                                    &ha_alter_info);

    switch (inplace_supported) {
    case HA_ALTER_INPLACE_EXCLUSIVE_LOCK:
      // If SHARED lock and no particular algorithm was requested, use COPY.
      if (alter_info->requested_lock ==
          Alter_info::ALTER_TABLE_LOCK_SHARED &&
          alter_info->requested_algorithm ==
          Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT)
      {
        use_inplace= false;
      }
      // Otherwise, if weaker lock was requested, report errror.
      else if (alter_info->requested_lock ==
               Alter_info::ALTER_TABLE_LOCK_NONE ||
               alter_info->requested_lock ==
               Alter_info::ALTER_TABLE_LOCK_SHARED)
      {
        ha_alter_info.report_unsupported_error("LOCK=NONE/SHARED",
                                               "LOCK=EXCLUSIVE");
        close_temporary_table(thd, altered_table, true, false);
        goto err_new_table_cleanup;
      }
      break;
    case HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE:
    case HA_ALTER_INPLACE_SHARED_LOCK:
      // If weaker lock was requested, report errror.
      if (alter_info->requested_lock ==
          Alter_info::ALTER_TABLE_LOCK_NONE)
      {
        ha_alter_info.report_unsupported_error("LOCK=NONE", "LOCK=SHARED");
        close_temporary_table(thd, altered_table, true, false);
        goto err_new_table_cleanup;
      }
      break;
    case HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE:
    case HA_ALTER_INPLACE_NO_LOCK:
      break;
    case HA_ALTER_INPLACE_NOT_SUPPORTED:
      // If INPLACE was requested, report error.
      if (alter_info->requested_algorithm ==
          Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
      {
        ha_alter_info.report_unsupported_error("ALGORITHM=INPLACE",
                                               "ALGORITHM=COPY");
        close_temporary_table(thd, altered_table, true, false);
        goto err_new_table_cleanup;
      }
      // COPY with LOCK=NONE is not supported, no point in trying.
      if (alter_info->requested_lock ==
          Alter_info::ALTER_TABLE_LOCK_NONE)
      {
        ha_alter_info.report_unsupported_error("LOCK=NONE", "LOCK=SHARED");
        close_temporary_table(thd, altered_table, true, false);
        goto err_new_table_cleanup;
      }
      // Otherwise use COPY
      use_inplace= false;
      break;
    case HA_ALTER_ERROR:
    default:
      close_temporary_table(thd, altered_table, true, false);
      goto err_new_table_cleanup;
    }

    if (use_inplace)
    {
      if (mysql_inplace_alter_table(thd, table_list, table,
                                    altered_table,
                                    &ha_alter_info,
                                    inplace_supported, &target_mdl_request,
                                    &alter_ctx))
      {
        thd->count_cuted_fields= CHECK_FIELD_IGNORE;
        DBUG_RETURN(true);
      }

      goto end_inplace;
    }
    else
    {
      close_temporary_table(thd, altered_table, true, false);
    }
  }

  /* ALTER TABLE using copy algorithm. */

  /* Check if ALTER TABLE is compatible with foreign key definitions. */
  if (fk_check_copy_alter_table(thd, table, alter_info))
    goto err_new_table_cleanup;

  if (!table->s->tmp_table)
  {
    // COPY algorithm doesn't work with concurrent writes.
    if (alter_info->requested_lock == Alter_info::ALTER_TABLE_LOCK_NONE)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "LOCK=NONE",
               ER(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COPY),
               "LOCK=SHARED");
      goto err_new_table_cleanup;
    }

    // If EXCLUSIVE lock is requested, upgrade already.
    if (alter_info->requested_lock == Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE &&
        wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      goto err_new_table_cleanup;

    /*
      Otherwise upgrade to SHARED_NO_WRITE.
      Note that under LOCK TABLES, we will already have SHARED_NO_READ_WRITE.
    */
    if (alter_info->requested_lock != Alter_info::ALTER_TABLE_LOCK_EXCLUSIVE &&
        thd->mdl_context.upgrade_shared_lock(mdl_ticket, MDL_SHARED_NO_WRITE,
                                             thd->variables.lock_wait_timeout))
      goto err_new_table_cleanup;

    DEBUG_SYNC(thd, "alter_table_copy_after_lock_upgrade");
  }

  // It's now safe to take the table level lock.
  if (lock_tables(thd, table_list, alter_ctx.tables_opened, 0))
    goto err_new_table_cleanup;

  {
    if (ha_create_table(thd, alter_ctx.get_tmp_path(),
                        alter_ctx.new_db, alter_ctx.tmp_name,
                        create_info, false))
      goto err_new_table_cleanup;

    /* Mark that we have created table in storage engine. */
    no_ha_table= false;

    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    {
      if (!open_table_uncached(thd, alter_ctx.get_tmp_path(),
                               alter_ctx.new_db, alter_ctx.tmp_name,
                               true, true))
        goto err_new_table_cleanup;
      /* in case of alter temp table send the tracker in OK packet */
      if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
    }
  }


  /* Open the table since we need to copy the data. */
  if (table->s->tmp_table != NO_TMP_TABLE)
  {
    TABLE_LIST tbl;
    tbl.init_one_table(alter_ctx.new_db, strlen(alter_ctx.new_db),
                       alter_ctx.tmp_name, strlen(alter_ctx.tmp_name),
                       alter_ctx.tmp_name, TL_READ_NO_INSERT);
    /* Table is in thd->temporary_tables */
    (void) open_temporary_table(thd, &tbl);
    new_table= tbl.table;
  }
  else
  {
    /* table is a normal table: Create temporary table in same directory */
    /* Open our intermediate table. */
    new_table= open_table_uncached(thd, alter_ctx.get_tmp_path(),
                                   alter_ctx.new_db, alter_ctx.tmp_name,
                                   true, true);
  }
  if (!new_table)
    goto err_new_table_cleanup;
  /*
    Note: In case of MERGE table, we do not attach children. We do not
    copy data for MERGE tables. Only the children have data.
  */

  /*
    We do not copy data for MERGE tables. Only the children have data.
    MERGE tables have HA_NO_COPY_ON_ALTER set.
  */
  if (!(new_table->file->ha_table_flags() & HA_NO_COPY_ON_ALTER))
  {
    new_table->next_number_field=new_table->found_next_number_field;
    THD_STAGE_INFO(thd, stage_copy_to_tmp_table);
    DBUG_EXECUTE_IF("abort_copy_table", {
        my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
        goto err_new_table_cleanup;
      });
   
    /*
      Acquire SRO locks on parent tables to prevent concurrent DML on them to
      perform cascading actions. Since InnoDB releases locks on table being
      altered periodically these actions might be able to succeed and
      can create orphan rows in our table otherwise.

      Note that we ignore FOREIGN_KEY_CHECKS=0 setting here because, unlike
      for DML operations it is hard to predict what kind of inconsistencies
      ignoring foreign keys will create (ignoring foreign keys in this case
      is similar to forcing other connections to ignore them).

      It is possible that acquisition of locks on parent tables will result
      in MDL deadlocks. But since deadlocks involving two or more DDL
      statements should be rare, it is unlikely that our ALTER TABLE will
      be aborted due to such deadlock.
    */
    if (lock_fk_dependent_tables(thd, table))
      goto err_new_table_cleanup;

    if (copy_data_between_tables(thd->m_stage_progress_psi,
                                 table, new_table,
                                 alter_info->create_list,
                                 &copied, &deleted,
                                 alter_info->keys_onoff,
                                 &alter_ctx))
      goto err_new_table_cleanup;
  }
  else
  {
    /* Should be MERGE only */
    DBUG_ASSERT(new_table->file->ht->db_type == DB_TYPE_MRG_MYISAM);
    if (!table->s->tmp_table &&
        wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      goto err_new_table_cleanup;
    THD_STAGE_INFO(thd, stage_manage_keys);
    DEBUG_SYNC(thd, "alter_table_manage_keys");
    alter_table_manage_keys(table, table->file->indexes_are_disabled(),
                            alter_info->keys_onoff);
    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      goto err_new_table_cleanup;
  }

  if (table->s->tmp_table != NO_TMP_TABLE)
  {
    /* Close lock if this is a transactional table */
    if (thd->lock)
    {
      if (thd->locked_tables_mode != LTM_LOCK_TABLES &&
          thd->locked_tables_mode != LTM_PRELOCKED_UNDER_LOCK_TABLES)
      {
        mysql_unlock_tables(thd, thd->lock);
        thd->lock= NULL;
      }
      else
      {
        /*
          If LOCK TABLES list is not empty and contains this table,
          unlock the table and remove the table from this list.
        */
        mysql_lock_remove(thd, thd->lock, table);
      }
    }
    /* Remove link to old table and rename the new one */
    close_temporary_table(thd, table, true, true);
    /* Should pass the 'new_name' as we store table name in the cache */
    if (rename_temporary_table(thd, new_table,
                               alter_ctx.new_db, alter_ctx.new_name))
      goto err_new_table_cleanup;
    /* We don't replicate alter table statement on temporary tables */
    if (!thd->is_current_stmt_binlog_format_row() &&
        write_bin_log(thd, true, thd->query().str, thd->query().length))
    {
      thd->count_cuted_fields= CHECK_FIELD_IGNORE;
      DBUG_RETURN(true);
    }
    goto end_temporary;
  }

  /*
    At this point, we must check whether the destination table is
    non-natively partitioned.
  */
  is_partitioned= new_table->s->db_type() &&
          is_ha_partition_handlerton(new_table->s->db_type());
  /*
    Close the intermediate table that will be the new table, but do
    not delete it! Even altough MERGE tables do not have their children
    attached here it is safe to call close_temporary_table().
  */
  close_temporary_table(thd, new_table, true, false);
  new_table= NULL;

  DEBUG_SYNC(thd, "alter_table_before_rename_result_table");

  /*
    Data is copied. Now we:
    1) Wait until all other threads will stop using old version of table
       by upgrading shared metadata lock to exclusive one.
    2) Close instances of table open by this thread and replace them
       with placeholders to simplify reopen process.
    3) Rename the old table to a temp name, rename the new one to the
       old name.
    4) If we are under LOCK TABLES and don't do ALTER TABLE ... RENAME
       we reopen new version of table.
    5) Write statement to the binary log.
    6) If we are under LOCK TABLES and do ALTER TABLE ... RENAME we
       remove placeholders and release metadata locks.
    7) If we are not not under LOCK TABLES we rely on the caller
      (mysql_execute_command()) to release metadata locks.
  */

  THD_STAGE_INFO(thd, stage_rename_result_table);

  if (wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_RENAME))
    goto err_new_table_cleanup;

  close_all_tables_for_name(thd, table->s, alter_ctx.is_table_renamed(), NULL);
  table_list->table= table= NULL;                  /* Safety */

  /*
    Rename the old table to temporary name to have a backup in case
    anything goes wrong while renaming the new table.
  */
  char backup_name[32];
  DBUG_ASSERT(sizeof(my_thread_id) == 4);
  my_snprintf(backup_name, sizeof(backup_name), "%s2-%lx-%lx", tmp_file_prefix,
              current_pid, thd->thread_id());
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, backup_name);
  if (mysql_rename_table(old_db_type, alter_ctx.db, alter_ctx.table_name,
                         alter_ctx.db, backup_name, FN_TO_IS_TMP))
  {
    // Rename to temporary name failed, delete the new table, abort ALTER.
    (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                          alter_ctx.tmp_name, FN_IS_TMP);
    goto err_with_mdl;
  }

  // Rename the new table to the correct name.
  if (mysql_rename_table(new_db_type, alter_ctx.new_db, alter_ctx.tmp_name,
                         alter_ctx.new_db, alter_ctx.new_alias,
                         FN_FROM_IS_TMP))
  {
    // Rename failed, delete the temporary table.
    (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                          alter_ctx.tmp_name, FN_IS_TMP);
    
    // Restore the backup of the original table to the old name.
    (void) mysql_rename_table(old_db_type, alter_ctx.db, backup_name,
                              alter_ctx.db, alter_ctx.alias, 
                              FN_FROM_IS_TMP | NO_FK_CHECKS);
    
    goto err_with_mdl;
  }

  // Check if we renamed the table and if so update trigger files.
  if (alter_ctx.is_table_renamed() &&
      change_trigger_table_name(thd,
                                alter_ctx.db,
                                alter_ctx.alias,
                                alter_ctx.table_name,
                                alter_ctx.new_db,
                                alter_ctx.new_alias))
  {
    // Rename succeeded, delete the new table.
    (void) quick_rm_table(thd, new_db_type,
                          alter_ctx.new_db, alter_ctx.new_alias, 0);
    // Restore the backup of the original table to the old name.
    (void) mysql_rename_table(old_db_type, alter_ctx.db, backup_name,
                              alter_ctx.db, alter_ctx.alias, 
                              FN_FROM_IS_TMP | NO_FK_CHECKS);
    goto err_with_mdl;
  }

  // ALTER TABLE succeeded, delete the backup of the old table.
  if (quick_rm_table(thd, old_db_type, alter_ctx.db, backup_name, FN_IS_TMP))
  {
    /*
      The fact that deletion of the backup failed is not critical
      error, but still worth reporting as it might indicate serious
      problem with server.
    */
    goto err_with_mdl;
  }

end_inplace:
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  if (thd->locked_tables_list.reopen_tables(thd))
    goto err_with_mdl;

  deprecation_silencer.pop();
  if (is_partitioned)
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_DEPRECATED_SYNTAX,
                        ER_THD(thd,
                               ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                        alter_ctx.new_db, alter_ctx.new_alias);

  THD_STAGE_INFO(thd, stage_end);

  DBUG_EXECUTE_IF("sleep_alter_before_main_binlog", my_sleep(6000000););
  DEBUG_SYNC(thd, "alter_table_before_main_binlog");

  ha_binlog_log_query(thd, create_info->db_type, LOGCOM_ALTER_TABLE,
                      thd->query().str, thd->query().length,
                      alter_ctx.db, alter_ctx.table_name);

  DBUG_ASSERT(!(mysql_bin_log.is_open() &&
                thd->is_current_stmt_binlog_format_row() &&
                (create_info->options & HA_LEX_CREATE_TMP_TABLE)));
  if (write_bin_log(thd, true, thd->query().str, thd->query().length))
    DBUG_RETURN(true);

  if (ha_check_storage_engine_flag(old_db_type, HTON_FLUSH_AFTER_RENAME))
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown. But we do not need to attach MERGE children.
    */
    TABLE *t_table;
    t_table= open_table_uncached(thd, alter_ctx.get_new_path(),
                                 alter_ctx.new_db, alter_ctx.new_name,
                                 false, true);
    if (t_table)
      intern_close_table(t_table);
    else
      sql_print_warning("Could not open table %s.%s after rename\n",
                        alter_ctx.new_db, alter_ctx.table_name);
    ha_flush_logs(old_db_type);
  }
  table_list->table= NULL;			// For query cache
  query_cache.invalidate(thd, table_list, FALSE);

  if (thd->locked_tables_mode == LTM_LOCK_TABLES ||
      thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES)
  {
    if (alter_ctx.is_table_renamed())
      thd->mdl_context.release_all_locks_for_name(mdl_ticket);
    else
      mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);
  }

end_temporary:
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  my_snprintf(alter_ctx.tmp_name, sizeof(alter_ctx.tmp_name),
              ER(ER_INSERT_INFO),
	      (long) (copied + deleted), (long) deleted,
	      (long) thd->get_stmt_da()->current_statement_cond_count());
  my_ok(thd, copied + deleted, 0L, alter_ctx.tmp_name);
  DBUG_RETURN(false);

err_new_table_cleanup:
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  if (new_table)
  {
    /* close_temporary_table() frees the new_table pointer. */
    close_temporary_table(thd, new_table, true, true);
  }
  else
    (void) quick_rm_table(thd, new_db_type,
                          alter_ctx.new_db, alter_ctx.tmp_name,
                          (FN_IS_TMP | (no_ha_table ? NO_HA_TABLE : 0)));

  if (alter_ctx.error_if_not_empty & Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT)
  {
    my_error(ER_INVALID_USE_OF_NULL, MYF(0));
  }

  /*
    No default value was provided for a DATE/DATETIME field, the
    current sql_mode doesn't allow the '0000-00-00' value and
    the table to be altered isn't empty.
    Report error here.
  */
  if ((alter_ctx.error_if_not_empty &
       Alter_table_ctx::DATETIME_WITHOUT_DEFAULT) &&
      (thd->variables.sql_mode & MODE_NO_ZERO_DATE) &&
      thd->get_stmt_da()->current_row_for_condition())
    push_zero_date_warning(thd, alter_ctx.datetime_field);
  DBUG_RETURN(true);

err_with_mdl:
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  /*
    An error happened while we were holding exclusive name metadata lock
    on table being altered. To be safe under LOCK TABLES we should
    remove all references to the altered table from the list of locked
    tables and release the exclusive metadata lock.
  */
  thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);
  thd->mdl_context.release_all_locks_for_name(mdl_ticket);
  DBUG_RETURN(true);
}
/* mysql_alter_table */



/**
  Prepare the transaction for the alter table's copy phase.
*/

bool mysql_trans_prepare_alter_copy_data(THD *thd)
{
  DBUG_ENTER("mysql_prepare_alter_copy_data");
  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.
    
    This needs to be done before external_lock.
  */
  if (ha_enable_transaction(thd, FALSE))
    DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}


/**
  Commit the copy phase of the alter table.
*/

bool mysql_trans_commit_alter_copy_data(THD *thd)
{
  bool error= FALSE;
  DBUG_ENTER("mysql_commit_alter_copy_data");

  if (ha_enable_transaction(thd, TRUE))
    DBUG_RETURN(TRUE);

  DEBUG_SYNC(thd, "commit_alter_copy_table");
  
  /*
    Ensure that the new table is saved properly to disk before installing
    the new .frm.
    And that InnoDB's internal latches are released, to avoid deadlock
    when waiting on other instances of the table before rename (Bug#54747).
  */
  if (trans_commit_stmt(thd))
    error= TRUE;
  if (trans_commit_implicit(thd))
    error= TRUE;

  DBUG_RETURN(error);
}


static int
copy_data_between_tables(PSI_stage_progress *psi,
                         TABLE *from,TABLE *to,
			 List<Create_field> &create,
			 ha_rows *copied,
			 ha_rows *deleted,
                         Alter_info::enum_enable_or_disable keys_onoff,
                         Alter_table_ctx *alter_ctx)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  THD *thd= current_thd;
  READ_RECORD info;
  TABLE_LIST   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows, found_rows, returned_rows;
  bool auto_increment_field_copied= 0;
  sql_mode_t save_sql_mode;
  QEP_TAB_standalone qep_tab_st;
  QEP_TAB &qep_tab= qep_tab_st.as_QEP_TAB();
  DBUG_ENTER("copy_data_between_tables");

  if (mysql_trans_prepare_alter_copy_data(thd))
    DBUG_RETURN(-1);
  
  if (!(copy= new Copy_field[to->s->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  if (to->file->ha_external_lock(thd, F_WRLCK))
    DBUG_RETURN(-1);

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(to, from->file->indexes_are_disabled(), keys_onoff);

  from->file->info(HA_STATUS_VARIABLE);
  to->file->ha_start_bulk_insert(from->file->stats.records);

  mysql_stage_set_work_estimated(psi, from->file->stats.records);

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

  SELECT_LEX *const select_lex= thd->lex->select_lex;
  ORDER *const order= select_lex->order_list.first;

  if (order)
  {
    if (to->s->primary_key != MAX_KEY && to->file->primary_key_is_clustered())
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      my_snprintf(warn_buff, sizeof(warn_buff), 
                  "ORDER BY ignored as there is a user-defined clustered index"
                  " in the table '%-.192s'", from->s->table_name.str);
      push_warning(thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                   warn_buff);
    }
    else
    {
      from->sort.io_cache=(IO_CACHE*) my_malloc(key_memory_TABLE_sort_io_cache,
                                                sizeof(IO_CACHE),
                                                MYF(MY_FAE | MY_ZEROFILL));
      tables.table= from;
      tables.alias= tables.table_name= from->s->table_name.str;
      tables.db= from->s->db.str;
      error= 1;

      Column_privilege_tracker column_privilege(thd, SELECT_ACL);

      if (select_lex->setup_ref_array(thd))
        goto err;            /* purecov: inspected */
      if (setup_order(thd, select_lex->ref_pointer_array,
                      &tables, fields, all_fields, order))
        goto err;
      qep_tab.set_table(from);
      Filesort fsort(&qep_tab, order, HA_POS_ERROR);
      if (filesort(thd, &fsort, true,
                   &examined_rows, &found_rows, &returned_rows))
        goto err;

      from->sort.found_records= returned_rows;
    }
  };

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();
  if (init_read_record(&info, thd, from, NULL, 1, 1, FALSE))
  {
    error= 1;
    goto err;
  }
  thd->get_stmt_da()->reset_current_row_for_condition();

  set_column_defaults(to, create);

  while (!(error=info.read_record(&info)))
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      error= 1;
      break;
    }
    /*
      Return error if source table isn't empty.

      For a DATE/DATETIME field, return error only if strict mode
      and No ZERO DATE mode is enabled.
    */
    if ((alter_ctx->error_if_not_empty &
         Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT) ||
        ((alter_ctx->error_if_not_empty &
          Alter_table_ctx::DATETIME_WITHOUT_DEFAULT) &&
         (thd->variables.sql_mode & MODE_NO_ZERO_DATE) &&
         thd->is_strict_mode()))
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
      copy_ptr->invoke_do_copy(copy_ptr);
    }
    if ((to->vfield && update_generated_write_fields(to->write_set, to)) ||
      thd->is_error())
    {
      error= 1;
      break;
    }

    error=to->file->ha_write_row(to->record[0]);
    to->auto_increment_field_not_null= FALSE;
    if (error)
    {
      if (!to->file->is_ignorable_error(error))
      {
        /* Not a duplicate key error. */
	to->file->print_error(error, MYF(0));
	break;
      }
      else
      {
        /* Report duplicate key error. */
        uint key_nr= to->file->get_dup_key(error);
        if ((int) key_nr >= 0)
        {
          const char *err_msg= ER(ER_DUP_ENTRY_WITH_KEY_NAME);
          if (key_nr == 0 &&
              (to->key_info[0].key_part[0].field->flags &
               AUTO_INCREMENT_FLAG))
            err_msg= ER(ER_DUP_ENTRY_AUTOINCREMENT_CASE);
          print_keydup_error(to, key_nr == MAX_KEY ? NULL :
                             &to->key_info[key_nr],
                             err_msg, MYF(0));
        }
        else
          to->file->print_error(error, MYF(0));
        break;
      }
    }
    else
    {
      DEBUG_SYNC(thd, "copy_data_between_tables_before");
      found_count++;
      mysql_stage_set_work_completed(psi, found_count);
    }
    thd->get_stmt_da()->inc_current_row_for_condition();
  }
  end_read_record(&info);
  free_io_cache(from);
  delete [] copy;				// This is never 0

  if (to->file->ha_end_bulk_insert() && error <= 0)
  {
    to->file->print_error(my_errno(),MYF(0));
    error= 1;
  }

  if (mysql_trans_commit_alter_copy_data(thd))
    error= 1;

 err:
  thd->variables.sql_mode= save_sql_mode;
  free_io_cache(from);
  *copied= found_count;
  *deleted=delete_count;
  to->file->ha_release_auto_increment();
  if (to->file->ha_external_lock(thd,F_UNLCK))
    error=1;
  if (error < 0 && to->file->extra(HA_EXTRA_PREPARE_FOR_RENAME))
    error= 1;
  DBUG_RETURN(error > 0 ? -1 : 0);
}


/*
  Recreates tables by calling mysql_alter_table().

  SYNOPSIS
    mysql_recreate_table()
    thd			Thread handler
    tables		Tables to recreate
    table_copy          Recreate the table by using
                        ALTER TABLE COPY algorithm

 RETURN
    Like mysql_alter_table().
*/
bool mysql_recreate_table(THD *thd, TABLE_LIST *table_list, bool table_copy)
{
  HA_CREATE_INFO create_info;
  Alter_info alter_info;

  DBUG_ENTER("mysql_recreate_table");
  DBUG_ASSERT(!table_list->next_global);
  /* Set lock type which is appropriate for ALTER TABLE. */
  table_list->lock_type= TL_READ_NO_INSERT;
  /* Same applies to MDL request. */
  table_list->mdl_request.set_type(MDL_SHARED_NO_WRITE);

  create_info.row_type=ROW_TYPE_NOT_USED;
  create_info.default_table_charset=default_charset_info;
  /* Force alter table to recreate table */
  alter_info.flags= (Alter_info::ALTER_CHANGE_COLUMN |
                     Alter_info::ALTER_RECREATE);

  if (table_copy)
    alter_info.requested_algorithm= Alter_info::ALTER_TABLE_ALGORITHM_COPY;

  const bool ret= mysql_alter_table(thd, NullS, NullS, &create_info,
                                    table_list, &alter_info);
  DBUG_RETURN(ret);
}


bool mysql_checksum_table(THD *thd, TABLE_LIST *tables,
                          HA_CHECK_OPT *check_opt)
{
  TABLE_LIST *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->get_protocol();
  DBUG_ENTER("mysql_checksum_table");

  /*
    CHECKSUM TABLE returns results and rollbacks statement transaction,
    so it should not be used in stored function or trigger.
  */
  DBUG_ASSERT(! thd->in_sub_stmt);

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_int(NAME_STRING("Checksum"),
                                          (longlong) 1,
                                          MY_INT64_NUM_DECIMAL_DIGITS));
  item->maybe_null= 1;
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  /*
    Close all temporary tables which were pre-open to simplify
    privilege checking. Clear all references to closed tables.
  */
  close_thread_tables(thd);
  for (table= tables; table; table= table->next_local)
    table->table= NULL;

  /* Open one table after the other to keep lock time as short as possible. */
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    TABLE *t;
    TABLE_LIST *save_next_global;

    strxmov(table_name, table->db ,".", table->table_name, NullS);

    /* Remember old 'next' pointer and break the list.  */
    save_next_global= table->next_global;
    table->next_global= NULL;
    table->lock_type= TL_READ;
    /* Allow to open real tables only. */
    table->required_type= FRMTYPE_TABLE;

    if (open_temporary_tables(thd, table) ||
        open_and_lock_tables(thd, table, 0))
    {
      t= NULL;
    }
    else
      t= table->table;

    table->next_global= save_next_global;

    protocol->start_row();
    protocol->store(table_name, system_charset_info);

    if (!t)
    {
      /* Table didn't exist */
      protocol->store_null();
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
              protocol->abort_row();
              goto err;
            }
	    ha_checksum row_crc= 0;
            int error= t->file->ha_rnd_next(t->record[0]);
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

	      row_crc= checksum_crc32(row_crc, t->record[0], t->s->null_bytes);
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
                  row_crc= checksum_crc32(row_crc, (uchar*) tmp.ptr(),
                           tmp.length());
                  break;
                }
                default:
                  row_crc= checksum_crc32(row_crc, f->ptr, f->pack_length());
                  break;
	      }
	    }

	    crc+= row_crc;
	  }
	  protocol->store((ulonglong)crc);
          t->file->ha_rnd_end();
	}
      }
      trans_rollback_stmt(thd);
      close_thread_tables(thd);
    }

    if (thd->transaction_rollback_request)
    {
      /*
        If transaction rollback was requested we honor it. To do this we
        abort statement and return error as not only CHECKSUM TABLE is
        rolled back but the whole transaction in which it was used.
      */
      protocol->abort_row();
      goto err;
    }

    /* Hide errors from client. Return NULL for problematic tables instead. */
    thd->clear_error();

    if (protocol->end_row())
      goto err;
  }

  my_eof(thd);
  DBUG_RETURN(FALSE);

err:
  DBUG_RETURN(TRUE);
}

/**
  @brief Check if the table can be created in the specified storage engine.

  Checks if the storage engine is enabled and supports the given table
  type (e.g. normal, temporary, system). May do engine substitution
  if the requested engine is disabled.

  @param thd          Thread descriptor.
  @param db_name      Database name.
  @param table_name   Name of table to be created.
  @param create_info  Create info from parser, including engine.

  @retval true  Engine not available/supported, error has been reported.
  @retval false Engine available/supported.
*/
static bool check_engine(THD *thd, const char *db_name,
                         const char *table_name, HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("check_engine");
  handlerton **new_engine= &create_info->db_type;
  handlerton *req_engine= *new_engine;
  bool no_substitution=
        MY_TEST(!is_engine_substitution_allowed(thd));
  if (!(*new_engine= ha_checktype(thd, ha_legacy_type(req_engine),
                                  no_substitution, 1)))
    DBUG_RETURN(true);

  if (req_engine && req_engine != *new_engine)
  {
    push_warning_printf(thd, Sql_condition::SL_NOTE,
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
      DBUG_RETURN(true);
    }
    *new_engine= myisam_hton;
  }

  /*
    Check, if the given table name is system table, and if the storage engine
    does supports it.
  */
  if ((create_info->used_fields & HA_CREATE_USED_ENGINE) &&
      !ha_is_valid_system_or_user_table(*new_engine, db_name, table_name))
  {
    my_error(ER_UNSUPPORTED_ENGINE, MYF(0),
             ha_resolve_storage_engine_name(*new_engine), db_name, table_name);
    *new_engine= NULL;
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}
