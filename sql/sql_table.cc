/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <memory>

#include "auth_acls.h"
#include "auth_common.h"              // check_fk_parent_table_access
#include "binlog.h"                   // mysql_bin_log
#include "binlog_event.h"
#include "dd/cache/dictionary_client.h"   // dd::cache::Dictionary_client
#include "dd/dd.h"                        // dd::get_dictionary
#include "dd/dd_schema.h"                 // dd::schema_exists
#include "dd/dd_table.h"                  // dd::drop_table, dd::update_keys...
#include "dd/dd_trigger.h"                // dd::table_has_triggers
#include "dd/dictionary.h"                // dd::Dictionary
#include "dd/string_type.h"
#include "dd/types/abstract_table.h"
#include "dd/types/column.h"
#include "dd/types/foreign_key.h"         // dd::Foreign_key
#include "dd/types/foreign_key_element.h" // dd::Foreign_key_element
#include "dd/types/index.h"               // dd::Index
#include "dd/types/table.h"               // dd::Table
#include "dd_sql_view.h"              // update_referencing_views_metadata
#include "dd_table_share.h"           // open_table_def
#include "debug_sync.h"               // DEBUG_SYNC
#include "derror.h"                   // ER_THD
#include "error_handler.h"            // Drop_table_error_handler
#include "field.h"
#include "filesort.h"                 // Filesort
#include "handler.h"
#include "item.h"
#include "item_timefunc.h"            // Item_func_now_local
#include "key.h"                      // KEY
#include "key_spec.h"                 // Key_part_spec
#include "lex_string.h"
#include "lock.h"                     // mysql_lock_remove, lock_tablespace_names
#include "log.h"                      // sql_print_error
#include "log_event.h"                // Query_log_event
#include "m_ctype.h"
#include "m_string.h"                 // my_stpncpy
#include "mdl.h"
#include "mem_root_array.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_check_opt.h"             // T_EXTEND
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "my_time.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/mysql_table.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/service_my_snprintf.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/thread_type.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld.h"                   // lower_case_table_names
#include "mysqld_error.h"             // ER_*
#include "partition_element.h"
#include "partition_info.h"           // partition_info
#include "partitioning/partition_handler.h" // Partition_handler
#include "prealloced_array.h"
#include "protocol.h"
#include "psi_memory_key.h"           // key_memory_gdl
#include "query_options.h"
#include "records.h"                  // READ_RECORD
#include "rpl_gtid.h"
#include "rpl_rli.h"                  // rli_slave etc
#include "session_tracker.h"
#include "sql_alter.h"
#include "sql_base.h"                 // lock_table_names
#include "sql_cache.h"                // query_cache
#include "sql_class.h"                // THD
#include "sql_const.h"
#include "sql_db.h"                   // get_default_db_collation
#include "sql_error.h"
#include "sql_executor.h"             // QEP_TAB_standalone
#include "sql_lex.h"
#include "sql_list.h"
#include "sql_parse.h"                // test_if_data_home_dir
#include "sql_partition.h"            // ALTER_PARTITION_PARAM_TYPE
#include "sql_plugin.h"
#include "sql_plugin_ref.h"
#include "sql_resolver.h"             // setup_order
#include "sql_show.h"
#include "sql_sort.h"
#include "sql_string.h"
#include "sql_tablespace.h"           // validate_tablespace_name
#include "sql_time.h"                 // make_truncated_value_warning
#include "sql_trigger.h"              // change_trigger_table_name
#include "strfunc.h"                  // find_type2
#include "system_variables.h"
#include "table.h"
#include "table_trigger_dispatcher.h"
#include "template_utils.h"
#include "thr_lock.h"
#include "thr_malloc.h"
#include "thr_mutex.h"
#include "transaction.h"              // trans_commit_stmt
#include "transaction_info.h"
#include "trigger.h"
#include "typelib.h"
#include "xa.h"

using std::max;
using std::min;
using binary_log::checksum_crc32;

#define ER_THD_OR_DEFAULT(thd,X) ((thd) ? ER_THD(thd, X) : ER_DEFAULT(X))

const char *primary_key_name="PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static const char *make_unique_key_name(const char *field_name,
                                        KEY *start, KEY *end);
static int copy_data_between_tables(THD *thd,
                                    PSI_stage_progress *psi,
                                    TABLE *from,TABLE *to,
                                    List<Create_field> &create,
				    ha_rows *copied,ha_rows *deleted,
                                    Alter_info::enum_enable_or_disable keys_onoff,
                                    Alter_table_ctx *alter_ctx);

static bool prepare_blob_field(THD *thd, Create_field *sql_field);
static bool check_engine(THD *thd, const char *db_name,
                         const char *table_name,
                         HA_CREATE_INFO *create_info);

static bool prepare_set_field(THD *thd, Create_field *sql_field);
static bool prepare_enum_field(THD *thd, Create_field *sql_field);

static uint blob_length_by_type(enum_field_types type);
static const Create_field *get_field_by_index(Alter_info *alter_info, uint idx);

/**
  RAII class to control the atomic DDL commit on slave.
  A slave context flag responsible to mark the DDL as committed is
  raised and kept for the entirety of DDL commit block.
  While DDL commits the slave info table won't take part
  in its transaction.
*/
class Disable_slave_info_update_guard
{
  Relay_log_info *m_rli;
  bool m_flag;

public:
  Disable_slave_info_update_guard(THD *thd)
    : m_rli(thd->rli_slave), m_flag(false)
  {
    if (!thd->slave_thread)
    {
      DBUG_ASSERT(!m_rli);

      return;
    }

    DBUG_ASSERT(m_rli->current_event);

    m_flag= static_cast<Query_log_event*>(thd->rli_slave->current_event)->
      has_ddl_committed;
    static_cast<Query_log_event*>(m_rli->current_event)->has_ddl_committed= true;
  };

  ~Disable_slave_info_update_guard()
  {
    if (m_rli)
    {
      static_cast<Query_log_event*>(m_rli->current_event)->
        has_ddl_committed= m_flag;
    }
  }
};

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
  filename_to_tablename cannot be used on partitions, due to the @#P@# part.
  There can be up to 6 '#', @#P@# for partition, @#SP@# for subpartition
  and @#TMP@# or @#REN@# for temporary or renamed partitions.
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

void parse_filename(const char *filename, size_t filename_length,
                    const char ** schema_name, size_t *schema_name_length,
                    const char ** table_name, size_t *table_name_length,
                    const char ** partition_name, size_t *partition_name_length,
                    const char ** subpartition_name, size_t *subpartition_name_length)
{
  const char *parse_ptr;
  size_t parse_length;
  const char *id_ptr= NULL;
  size_t id_length= 0;
  const char *ptr= NULL;

  parse_ptr= filename;
  parse_length= filename_length;

  while ((ptr= strchr(parse_ptr, '/')))
  {
    id_ptr= parse_ptr;
    id_length= (ptr - parse_ptr);

    parse_ptr += (id_length + 1);
    parse_length -= (id_length + 1);
  }

  *schema_name= id_ptr;
  *schema_name_length= id_length;

  ptr= strchr(parse_ptr, '#');

  if (ptr != NULL)
  {
    id_ptr= parse_ptr;
    id_length= (ptr - parse_ptr);

    parse_ptr += (id_length);
    parse_length -= (id_length);
  }
  else
  {
    id_ptr= parse_ptr;
    id_length= parse_length;

    parse_ptr= NULL;
    parse_length= 0;
  }

  *table_name= id_ptr;
  *table_name_length= id_length;

  if ((parse_length >= 4) && (native_strncasecmp(parse_ptr, "#TMP", 4) == 0))
  {
    parse_ptr += 4;
    parse_length -= 4;
  }

  if ((parse_length >= 4) && (native_strncasecmp(parse_ptr, "#REN", 4) == 0))
  {
    parse_ptr += 4;
    parse_length -= 4;
  }

  if ((parse_length >= 3) && (native_strncasecmp(parse_ptr, "#P#", 3) == 0))
  {
    parse_ptr += 3;
    parse_length -= 3;

    ptr= strchr(parse_ptr, '#');

    if (ptr != NULL)
    {
      id_ptr= parse_ptr;
      id_length= (ptr - parse_ptr);

      parse_ptr += (id_length);
      parse_length -= (id_length);
    }
    else
    {
      id_ptr= parse_ptr;
      id_length= parse_length;

      parse_ptr= NULL;
      parse_length= 0;
    }
  }
  else
  {
    id_ptr= NULL;
    id_length= 0;
  }

  *partition_name= id_ptr;
  *partition_name_length= id_length;

  if ((parse_length >= 4) && (native_strncasecmp(parse_ptr, "#SP#", 4) == 0))
  {
    parse_ptr += 4;
    parse_length -= 4;

    id_ptr= parse_ptr;
    id_length= parse_length;

    parse_ptr= NULL;
    parse_length= 0;
  }
  else
  {
    id_ptr= NULL;
    id_length= 0;
  }

  *subpartition_name= id_ptr;
  *subpartition_name_length= id_length;
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
  Create path to a temporary table, like mysql_tmpdir/@#sql1234_12_1
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
                                               O_RDWR, MYF(0))) >= 0)
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
  @param read_entry

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
  @param[out] ddl_log_entry       Information from entry

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
                                                 O_RDWR | O_TRUNC,
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
  Convert file path to db + tablename.

  Basically reverse of build_table_name()/tablename_to_filename().

  @param[in]  path       db/tablename in file name safe coding
  @param[out] db         db in system charset (size >= NAME_LEN)
  @param[out] table_name table name in system charset (size >= NAME_LEN)

  @return false if success, else true.
*/

static bool path_to_db_and_table_name(const char *path,
                                      char *db,
                                      char *table_name)
{
  char buf[FN_REFLEN + 1];
  size_t len= strlen(path);
  uint errors;
  memcpy(buf, path, len + 1);
  char *ptr= strrchr(buf, FN_LIBCHAR);
  DBUG_ASSERT(ptr);
  *ptr= '\0';
  ptr++;
  if (!strncmp(ptr, tmp_file_prefix, tmp_file_prefix_length))
  {
    if (strlen(ptr) >= NAME_LEN)
      return true;
    /* Generated "#sql..." with no non-safe file name characters. */
    strcpy(table_name, ptr);
  }
  else
  {
    /* Convert to system charset. */
    if (!strconvert(&my_charset_filename,
                    ptr,
                    system_charset_info,
                    table_name,
                    NAME_LEN,
                    &errors) ||
        errors)
    {
      return true;
    }
  }
  ptr= strrchr(buf, FN_LIBCHAR);
  DBUG_ASSERT(ptr);
  ptr++;
  /* Always convert db. */
  if (!strconvert(&my_charset_filename,
                  ptr,
                  system_charset_info,
                  db,
                  NAME_LEN,
                  &errors) ||
      errors)
  {
    return true;
  }
  return false;
}

/**
  Execute one action in a ddl log entry

  @param thd
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
    file= get_new_handler((TABLE_SHARE*)0, false, &mem_root, hton);
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
          strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
          (void) mysql_file_delete(key_file_partition_ddl_log, to_path, MYF(0));
          // TODO: This should be handled by the new DD (check in mysql_update_dd)
          char db[NAME_LEN + 1];
          char table_name[NAME_LEN + 1];
          if (!path_to_db_and_table_name(ddl_log_entry->name, db, table_name) &&
              !dd::get_dictionary()->is_dd_table_name(db, table_name))
          {
            // Check if table exists
            bool table_exists= false;
            if (dd::table_exists<dd::Table>(thd->dd_client(),
                                            db,
                                            table_name,
                                            &table_exists))
              break;

            if (table_exists)
            {
              if (ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION)
              {
                char from_db[NAME_LEN + 1];
                char from_table_name[NAME_LEN + 1];

                if (path_to_db_and_table_name(ddl_log_entry->from_name,
                                              from_db,
                                              from_table_name))
                  break;

                table_exists= false;

                if (dd::table_exists<dd::Table>(thd->dd_client(),
                                                from_db,
                                                from_table_name,
                                                &table_exists))
                  break;

                /*
                  If there are both temporary table and original table
                  and we are handling ALTER TABLE ... DROP PARTITION then
                  move triggers from original partitioned table to the
                  temporary table. Moved triggers will be later restored
                  for the original table while calling dd::rename_table
                  in the next alternative DDL_LOG_RENAME_ACTION.
                */
                if (table_exists &&
                    dd::move_triggers(thd, db, table_name,
                                      from_db, from_table_name,
                                      true))
                  break;
              }
              else
              {
                //  Remove table from DD
                if (dd::drop_table(thd, db, table_name, true))
                  break;
              }
            }
          }
        }
        else
        {
          if ((error= file->ha_delete_table(ddl_log_entry->name, NULL)))
          {
            if (error != ENOENT && error != HA_ERR_NO_SUCH_TABLE)
              break;
          }
        }
        if ((deactivate_ddl_log_entry_no_lock(ddl_log_entry->entry_pos)))
          break;
        (void) sync_ddl_log_no_lock();
        if (ddl_log_entry->action_type == DDL_LOG_DELETE_ACTION)
        {
          error= FALSE;
          break;
        }
      }
      DBUG_ASSERT(ddl_log_entry->action_type == DDL_LOG_REPLACE_ACTION);
    }
    /*
      Fall through and perform the rename action of the replace
      action. We have already indicated the success of the delete
      action in the log entry by stepping up the phase.
    */
    case DDL_LOG_RENAME_ACTION:
    {
      if (frm_action)
      {
        // TODO: This should be handled by the new DD (check in mysql_update_dd)
        char db[NAME_LEN + 1];
        char table_name[NAME_LEN + 1];
        char from_db[NAME_LEN + 1];
        char from_table_name[NAME_LEN + 1];
        if (!path_to_db_and_table_name(ddl_log_entry->name, db, table_name) &&
            !path_to_db_and_table_name(ddl_log_entry->from_name,
                                       from_db,
                                       from_table_name) &&
            !dd::get_dictionary()->is_dd_table_name(db, table_name))
        {
          // Check if table exists
          bool table_exists= false;
          if (dd::table_exists<dd::Table>(thd->dd_client(),
                                          from_db,
                                          from_table_name,
                                          &table_exists))
            break;

          if (table_exists)
          {
            // Rename table from DD
            if (dd::rename_table(thd,
                                 from_db,
                                 from_table_name,
                                 db,
                                 table_name,
                                 false,
                                 true))
              break;
          }
        }
        DBUG_ASSERT(strcmp("partition", ddl_log_entry->handler_name));
        strxmov(to_path, ddl_log_entry->name, par_ext, NullS);
        strxmov(from_path, ddl_log_entry->from_name, par_ext, NullS);
        (void) mysql_file_rename(key_file_partition_ddl_log, from_path, to_path, MYF(0));
      }
      else
      {
        if (file->ha_rename_table(ddl_log_entry->from_name,
                                  ddl_log_entry->name,
                                  NULL, NULL))
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
                                       ddl_log_entry->tmp_name,
                                       NULL, NULL);
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
                                       ddl_log_entry->from_name,
                                       NULL, NULL);
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
                                       ddl_log_entry->name,
                                       NULL, NULL);
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
  @param write_header

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

  @param thd
  @param first_entry           Reference to first action in entry

  @return Operation status
    @retval TRUE               Error
    @retval FALSE              Success
*/

static bool execute_ddl_log_entry_no_lock(THD *thd, uint first_entry)
{
  DDL_LOG_ENTRY ddl_log_entry;
  uint read_entry= first_entry;
  bool error= false;
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
      // TODO: More verbose error message, at least the content of the entry.
      sql_print_error("Failed to execute action for entry = %u from ddl log",
                      read_entry);
      error= true;
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
  @param[out] active_entry     Entry information written into   

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
      We haven't synched the log entries yet, we synch them now before
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
  Release a log memory entry.
  @param log_entry                Log memory entry to release
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

  @param thd
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
  // Mark this as a system thread to skip metadata lock checks in the DD.
  thd->system_thread= SYSTEM_THREAD_DDL_LOG_RECOVERY;

  thd->set_query(recover_query_string, strlen(recover_query_string));

  /*
    Prevent InnoDB from automatically committing InnoDB transaction
    each time data-dictionary tables are closed after being updated.
  */
  thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;

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
  Create a dd::Table-object specifying the temporary table
  definition, but do not put it into the Data Dictionary. The created
  dd::Table-instance is returned via tmp_table_def out-parameter.
  The temporary table is also created in the storage engine, depending
  on the 'no_ha_table' argument.

  @param thd           Thread handler
  @param path          Name of file (including database)
  @param db            Data base name
  @param table_name    Table name
  @param create_info   create info parameters
  @param create_fields Fields to create
  @param keys          number of keys to create
  @param key_info      Keys to create
  @param keys_onoff    Enable or disable keys.
  @param file          Handler to use
  @param no_ha_table   Indicates that only definitions needs to be created
                       and not a table in the storage engine.
  @param[out] binlog_to_trx_cache
                       Which binlog cache should be used?
                       If true => trx cache
                       If false => stmt cache
  @param[out] tmp_table_def  Placeholder for data-dictionary object for
                             temporary table which was created. It will
                             contain nullptr if no_ha_table was false.

  @retval false  ok
  @retval true   error
*/

static bool rea_create_tmp_table(THD *thd, const char *path,
                                 const char *db, const char *table_name,
                                 HA_CREATE_INFO *create_info,
                                 List<Create_field> &create_fields,
                                 uint keys, KEY *key_info,
                                 Alter_info::enum_enable_or_disable keys_onoff,
                                 handler *file, bool no_ha_table,
                                 bool *binlog_to_trx_cache,
                                 dd::Table **tmp_table_def)
{
  DBUG_ENTER("rea_create_tmp_table");

  *tmp_table_def= NULL;

  std::unique_ptr<dd::Table> tmp_table_ptr=
    dd::create_tmp_table(thd, db, table_name, create_info, create_fields,
                         key_info, keys, keys_onoff, file);
  if (!tmp_table_ptr)
    DBUG_RETURN(true);

  if (no_ha_table)
  {
    *tmp_table_def= tmp_table_ptr.release();
    DBUG_RETURN(false);
  }

  // Create the table in the storage engine.
  if (ha_create_table(thd, path, db, table_name, create_info,
                      false, false, tmp_table_ptr.get()))
  {
    DBUG_RETURN(true);
  }

  /*
    Open a table (skipping table cache) and add it into
    THD::temporary_tables list.
  */
  TABLE *table= open_table_uncached(thd, path, db, table_name, true, true,
                                    tmp_table_ptr.get());

  if (!table)
  {
    (void) rm_temporary_table(thd, create_info->db_type, path,
                              tmp_table_ptr.get());
    DBUG_RETURN(true);
  }

  // Transfer ownership of dd::Table object to TABLE_SHARE.
  table->s->tmp_table_def= tmp_table_ptr.release();

  thd->thread_specific_used= TRUE;

  if (binlog_to_trx_cache != NULL)
    *binlog_to_trx_cache= table->file->has_transactions();
  DBUG_RETURN(false);
}


/**
  Create table definition in the Data Dictionary. The table is also
  created in the storage engine, depending on the 'no_ha_table' argument.

  @param thd           Thread handler
  @param path          Name of file (including database)
  @param db            Data base name
  @param table_name    Table name
  @param create_info   create info parameters
  @param create_fields Fields to create
  @param keys          number of keys to create
  @param key_info      Keys to create
  @param keys_onoff    Enable or disable keys.
  @param fk_keys       Number of foreign keys to create
  @param fk_key_info   Foreign keys to create
  @param file          Handler to use
  @param no_ha_table   Indicates that only definitions needs to be created
                       and not a table in the storage engine.
  @param part_info     Reference to partitioning data structure.
  @param[out] binlog_to_trx_cache
                       Which binlog cache should be used?
                       If true => trx cache
                       If false => stmt cache
  @param[out] post_ddl_ht    Set to handlerton for table's SE, if this SE
                             supports atomic DDL, so caller can call SE
                             post DDL hook after committing transaction.

  @note For engines supporting atomic DDL the caller must rollback
        both statement and transaction on failure. This must be done
        before any further accesses to DD. @sa dd::create_table().

  @retval false  ok
  @retval true   error
*/

static bool rea_create_base_table(THD *thd, const char *path,
                                  const char *db, const char *table_name,
                                  HA_CREATE_INFO *create_info,
                                  List<Create_field> &create_fields,
                                  uint keys, KEY *key_info,
                                  Alter_info::enum_enable_or_disable keys_onoff,
                                  uint fk_keys, FOREIGN_KEY *fk_key_info,
                                  handler *file, bool no_ha_table,
                                  partition_info *part_info,
                                  bool *binlog_to_trx_cache,
                                  handlerton **post_ddl_ht)
{
  DBUG_ENTER("rea_create_base_table");

  if (dd::create_table(thd, db, table_name,
                       create_info,
                       create_fields,
                       key_info,
                       keys,
                       keys_onoff,
                       fk_key_info,
                       fk_keys,
                       file,
                       !(create_info->db_type->flags &
                         HTON_SUPPORTS_ATOMIC_DDL)))
    DBUG_RETURN(true);

  if (no_ha_table)
  {
    if (part_info)
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

      init_tmp_table_share(thd, &share, db, 0, table_name, path, nullptr);

      bool result= open_table_def(thd, &share, false, nullptr) ||
        open_table_from_share(thd, &share, "", 0, (uint) READ_ALL,
                              0, &table, true, nullptr);

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
        /*
          If changes were committed remove table from DD.
          We ignore the errors returned from there functions
          as we anyway report error.
        */
        if (!(create_info->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
          (void) dd::drop_table(thd, db, table_name, true);

        DBUG_RETURN(true);
      }
    }

    DBUG_RETURN(false);
  }

  // Create the table in the storage engine.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  dd::Table *table_def= nullptr;

  if (thd->dd_client()->acquire_for_modification(db, table_name,
                                                 &table_def))
    goto err;

  // Table just has been created in DD.
  DBUG_ASSERT(table_def);

  if ((create_info->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      create_info->db_type->post_ddl)
    *post_ddl_ht= create_info->db_type;

  if (ha_create_table(thd, path, db, table_name, create_info,
                      false, false, table_def))
  {
    goto err;
  }

  /*
    If the SE supports atomic DDL, we can use the trx binlog cache.
    Otherwise we must use the statement cache.
  */
  if (binlog_to_trx_cache != NULL)
    *binlog_to_trx_cache=
      (create_info->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL);

  DBUG_RETURN(false);

err:
  /*
    Remove table from data-dictionary if it was added and rollback
    won't do this automatically.
  */
  if (!(create_info->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
  {
    /*
      Creation of Dictionary tables may fail inside SE if there is
      already an entry for the table with the same name as dictionary
      table in InnoDB dictionary. This might happen during upgrade from
      5.7 or debug scenarios in current server.

      In this case, DD entry and DD cache will not be cleared here.
      Dictionary system will try to delete all dictionary tables.
    */
    if (dd_upgrade_flag &&
        dd::get_dictionary()->is_dd_table_name(db, table_name))
    {
      DBUG_RETURN(true);
    }

    /*
      We ignore error from dd_drop_table() as we anyway
      return 'true' failure below.
    */
    (void) dd::drop_table(thd, db, table_name, true);
  }

  DBUG_RETURN(true);
}


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
  my_snprintf(tmp_name, sizeof(tmp_name), "%s_p_%lx_%lx",
              tmp_file_prefix, current_pid,
              lpt->thd->thread_id());
  return build_table_filename(buff, bufflen, lpt->db, tmp_name, "", FN_IS_TMP);
}


/*
  SYNOPSIS
    mysql_update_dd()
    lpt                    Struct carrying many parameters needed for this
                           method
    flags                  Flags as defined below
      WFRM_PACK_FRM             If set we should pack the frm file and delete
                                the frm file

  RETURN VALUES
    TRUE                   Error
    FALSE                  Success

  DESCRIPTION
    A support method that creates a new version of the table defintion in
    the data dictionary and a new par file. In this process it regenerates
    the partition data.
*/

bool mysql_update_dd(ALTER_PARTITION_PARAM_TYPE *lpt, uint flags)
{
  /*
    Prepare table to prepare for writing a new frm file where the
    partitions in add/drop state have temporarily changed their state
    We set tmp_table to avoid get errors on naming of primary key index.
  */
  int error= 0;
  char shadow_path[FN_REFLEN+1];
  char *shadow_name;
  char *part_syntax_buf;
  uint syntax_len;
  DBUG_ENTER("mysql_update_dd");
  partition_info *old_part_info= lpt->table->part_info;
  FOREIGN_KEY *not_used1= NULL;
  uint not_used2= 0;

  /*
    Build shadow frm file name
  */
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1, lpt);
  shadow_name= strrchr(shadow_path, FN_LIBCHAR);
  shadow_name++;
  DBUG_ASSERT(shadow_name);
  if (flags & WSDI_WRITE_SHADOW)
  {
    if (mysql_prepare_create_table(lpt->thd, lpt->db,
                                   lpt->table_name,
                                   lpt->create_info,
                                   lpt->alter_info,
                                   lpt->table->file,
                                   &lpt->key_info_buffer,
                                   &lpt->key_count,
                                   &not_used1,
                                   &not_used2,
                                   NULL,
                                   0,
                                   /*select_field_count*/ 0))
    {
      DBUG_RETURN(TRUE);
    }
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
                                                   &lpt->alter_info->create_list,
                                                   NULL);
        lpt->thd->variables.sql_mode= sql_mode_backup;
        if (part_syntax_buf == NULL)
        {
          DBUG_RETURN(TRUE);
        }
        part_info->part_info_string= part_syntax_buf;
        part_info->part_info_len= syntax_len;
        Partition_handler *part_handler;
        part_handler= lpt->table->file->get_partition_handler();
        part_handler->set_part_info(part_info, false);
      }
    }

    // Add table details into new DD
    if (!dd::get_dictionary()->is_dd_table_name(lpt->db, shadow_name) &&
        !dd::get_dictionary()->is_dd_table_name(lpt->db, lpt->table_name))
    {
      Alter_info::enum_enable_or_disable keys_onoff=
        ((lpt->alter_info->keys_onoff == Alter_info::LEAVE_AS_IS &&
          lpt->table->file->indexes_are_disabled()) ? Alter_info::DISABLE :
         lpt->alter_info->keys_onoff);

      if (dd::create_table(lpt->thd,
                           lpt->db,
                           shadow_name,
                           lpt->create_info,
                           lpt->alter_info->create_list,
                           lpt->key_info_buffer,
                           lpt->key_count,
                           keys_onoff,
                           not_used1,
                           not_used2,
                           lpt->table->file,
                           true))
      {
        DBUG_RETURN(true);
      }
    }
  }

  if (old_part_info)
  {
    Partition_handler *part_handler= lpt->table->file->get_partition_handler();
    part_handler->set_part_info(old_part_info, false);
  }
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


bool lock_trigger_names(THD *thd,
                        TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table;
      table= table->next_global)
  {
    List<LEX_CSTRING> trigger_names;

    if (table->open_type == OT_TEMPORARY_ONLY ||
        (table->open_type == OT_TEMPORARY_OR_BASE &&
         is_temporary_table(table)))
      continue;

    if (dd::load_trigger_names(thd,
                               thd->mem_root,
                               table->db,
                               table->table_name,
                               &trigger_names))
      return true;

    List_iterator_fast<LEX_CSTRING> it(trigger_names);
    LEX_CSTRING *trigger_name;

    while ((trigger_name= it++))
    {
      if (acquire_exclusive_mdl_for_trigger(thd, table->db,
                                            trigger_name->str))
        return true;
    }
  }

  return false;
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

bool mysql_rm_table(THD *thd,TABLE_LIST *tables, bool if_exists,
                    bool drop_temporary)
{
  bool error;
  Drop_table_error_handler err_handler;
  TABLE_LIST *table;
  uint have_non_tmp_table= 0;

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
  }

  if (!drop_temporary)
  {
    if (!thd->locked_tables_mode)
    {
      if (lock_table_names(thd, tables, NULL,
                           thd->variables.lock_wait_timeout, 0) ||
          lock_trigger_names(thd, tables))
        DBUG_RETURN(true);

      for (table= tables; table; table= table->next_local)
      {
        if (is_temporary_table(table))
          continue;

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

          if (wait_while_table_is_used(thd, table->table, HA_EXTRA_FORCE_REOPEN))
            DBUG_RETURN(true);

          /* Here we are sure that a non-tmp table exists */
          have_non_tmp_table= 1;
        }
    }
  }

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  std::set<handlerton*> post_ddl_htons;
  Prealloced_array<TABLE_LIST*, 1> dropped_atomic(PSI_INSTRUMENT_ME);
  bool not_used;

  /* mark for close and remove all cached entries */
  thd->push_internal_handler(&err_handler);
  error= mysql_rm_table_no_locks(thd, tables, if_exists, drop_temporary,
                                 false, &not_used, &post_ddl_htons,
                                 &dropped_atomic);
  thd->pop_internal_handler();

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
      if (thd->lock && thd->lock->table_count == 0 &&
          have_non_tmp_table > 0)
      {
        thd->mdl_context.release_statement_locks();
        thd->locked_tables_list.unlock_locked_tables(thd);
      }
      else
      {
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
    }
  }

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
  Runtime context for DROP TABLES statement.
*/

class Drop_tables_ctx
{
public:
  Drop_tables_ctx(bool if_exists_arg, bool drop_temporary_arg,
                  bool drop_database_arg)
    : if_exists(if_exists_arg),
      drop_temporary(drop_temporary_arg),
      drop_database(drop_database_arg),
      base_atomic_tables(PSI_INSTRUMENT_ME),
      base_non_atomic_tables(PSI_INSTRUMENT_ME),
      tmp_trans_tables(PSI_INSTRUMENT_ME),
      tmp_non_trans_tables(PSI_INSTRUMENT_ME),
      nonexistent_tables(PSI_INSTRUMENT_ME),
      views(PSI_INSTRUMENT_ME),
      dropped_non_atomic(PSI_INSTRUMENT_ME),
      gtid_and_table_groups_state(NO_GTID_MANY_TABLE_GROUPS)
  {
    /* DROP DATABASE implies if_exists and absence of drop_temporary. */
    DBUG_ASSERT(!drop_database || (if_exists && !drop_temporary));
  }

  /* Parameters of DROP TABLES statement. */
  const bool if_exists;
  const bool drop_temporary;
  const bool drop_database;

  /* Different table groups of tables to be dropped. */
  Prealloced_array<TABLE_LIST*, 1> base_atomic_tables;
  Prealloced_array<TABLE_LIST*, 1> base_non_atomic_tables;
  Prealloced_array<TABLE_LIST*, 1> tmp_trans_tables;
  Prealloced_array<TABLE_LIST*, 1> tmp_non_trans_tables;
  Prealloced_array<TABLE_LIST*, 1> nonexistent_tables;
  Prealloced_array<TABLE_LIST*, 1> views;

  /* Methods which simplify checking state of the above groups. */
  bool has_base_atomic_tables() const
  {
    return base_atomic_tables.size() != 0;
  }

  bool has_base_non_atomic_tables() const
  {
    return base_non_atomic_tables.size() != 0;
  }

  bool has_tmp_trans_tables() const
  {
    return tmp_trans_tables.size() != 0;
  }

  bool has_tmp_non_trans_tables() const
  {
    return tmp_non_trans_tables.size() != 0;
  }

  bool has_any_nonexistent_tables() const
  {
    return nonexistent_tables.size() != 0;
  }
  bool has_base_nonexistent_tables() const
  {
    return !drop_temporary && nonexistent_tables.size() != 0;
  }

  bool has_tmp_nonexistent_tables() const
  {
    return drop_temporary && nonexistent_tables.size() != 0;
  }

  bool has_views() const
  {
    return views.size() != 0;
  }

  /**
    Base tables in SE which do not support atomic DDL which we managed to
    drop so far.
  */
  Prealloced_array<TABLE_LIST*, 1> dropped_non_atomic;

  bool has_dropped_non_atomic() const
  {
    return dropped_non_atomic.size() != 0;
  }

  /**
    In which situation regarding GTID mode and different types
    of tables to be dropped we are.

    TODO: consider splitting into 2 orthogonal enum/bools.
  */
  enum { NO_GTID_MANY_TABLE_GROUPS,
         NO_GTID_SINGLE_TABLE_GROUP,
         GTID_MANY_TABLE_GROUPS,
         GTID_SINGLE_TABLE_GROUP } gtid_and_table_groups_state;

  /* Methods to simplify quering the above state. */
  bool has_no_gtid_many_table_groups() const
  {
    return gtid_and_table_groups_state == NO_GTID_MANY_TABLE_GROUPS;
  }

  bool has_no_gtid_single_table_group() const
  {
    return gtid_and_table_groups_state == NO_GTID_SINGLE_TABLE_GROUP;
  }

  bool has_gtid_many_table_groups() const
  {
    return gtid_and_table_groups_state == GTID_MANY_TABLE_GROUPS;
  }

  bool has_gtid_single_table_group() const
  {
    return gtid_and_table_groups_state == GTID_SINGLE_TABLE_GROUP;
  }
};


/**
  Auxiliary function which appends to the string table identifier with proper
  quoting and schema part if necessary.
*/

static void append_table_ident(THD *thd, String *to, const TABLE_LIST *table,
                               bool force_db)
{
  //  Don't write the database name if it is the current one.
  if (thd->db().str == NULL || strcmp(table->db, thd->db().str) != 0
      || force_db)
  {
    append_identifier(thd, to, table->db, table->db_length,
                      system_charset_info, thd->charset());
    to->append(".");
  }
  append_identifier(thd, to, table->table_name, table->table_name_length,
                    system_charset_info, thd->charset());
}


/**
  Auxiliary function which appends to the string schema and table name for
  the table (without quoting).
*/

static void append_table_name(String *to, const TABLE_LIST *table)
{
  to->append(String(table->db, system_charset_info));
  to->append('.');
  to->append(String(table->table_name, system_charset_info));
}


/**
  Auxiliary class which is used to construct synthesized DROP TABLES
  statements for the binary log during execution of DROP TABLES statement.
*/

class Drop_tables_query_builder
{
public:
  Drop_tables_query_builder(THD *thd, bool temporary, bool if_exists,
                            bool is_trans, bool no_db)
    : m_bin_log_is_open(mysql_bin_log.is_open()),
      m_thd(thd), m_is_trans(is_trans), m_no_db(no_db)
  {
    if (m_bin_log_is_open)
    {
      m_built_query.set_charset(system_charset_info);
      m_built_query.append("DROP ");
      if (temporary)
        m_built_query.append("TEMPORARY ");
      m_built_query.append("TABLE ");
      if (if_exists)
        m_built_query.append("IF EXISTS ");
    }
  }

  /*
    Constructor for the most common case:
    - base tables
    - write to binlog trx cache
    - Database exists
  */
  Drop_tables_query_builder(THD *thd, bool if_exists)
    : m_bin_log_is_open(mysql_bin_log.is_open()),
      m_thd(thd), m_is_trans(true), m_no_db(false)
  {
    if (m_bin_log_is_open)
    {
      m_built_query.set_charset(system_charset_info);
      m_built_query.append("DROP TABLE ");
      if (if_exists)
        m_built_query.append("IF EXISTS ");
    }
  }

private:
  void add_table_impl(const TABLE_LIST *table)
  {
    append_table_ident(m_thd, &m_built_query, table, m_no_db);
    m_built_query.append(",");

    m_thd->add_to_binlog_accessed_dbs(table->db);
  }

public:
  void add_table(const TABLE_LIST *table)
  {
    if (m_bin_log_is_open)
      add_table_impl(table);
  }

  void add_array(const Prealloced_array<TABLE_LIST *, 1> &tables)
  {
    if (m_bin_log_is_open)
    {
      for (TABLE_LIST *table : tables)
        add_table_impl(table);
    }
  }

  bool write_bin_log()
  {
    if (m_bin_log_is_open)
    {
      /* Chop off the last comma */
      m_built_query.chop();
      m_built_query.append(" /* generated by server */");

      /*
        We can't use ::write_bin_log() here as this method is sometimes used
        in case when DROP TABLES statement is supposed to report an error.
        And ::write_bin_log() either resets error in DA or uses it for binlog
        event (which we would like to avoid too).
      */
      if (m_thd->binlog_query(THD::STMT_QUERY_TYPE,
                              m_built_query.ptr(), m_built_query.length(),
                              m_is_trans , false /* direct */,
                              m_no_db /* suppress_use */, 0 /* errcode */))
        return true;
    }
    return false;
  }

private:
  bool m_bin_log_is_open;
  THD *m_thd;
  bool m_is_trans;
  bool m_no_db;
  String m_built_query;
};


/**
  Auxiliary function which prepares for DROP TABLES execution by sorting
  tables to be dropped into groups according to their types.
*/

static bool
rm_table_sort_into_groups(THD *thd, Drop_tables_ctx *drop_ctx,
                          TABLE_LIST *tables)
{
  /*
    Sort tables into groups according to type of handling they require:

    1) Base tables and views. Further divided into the following groups:

       a) Base tables in storage engines which don't support atomic DDL.

          Their drop can't be rolled back in case of crash or error.
          So we drop each such table individually and write to binlog
          a single-table DROP TABLE statement corresponding to this
          action right after it. This increases chances of SE,
          data-dictionary and binary log being in sync if crash occurs.
          This also handles case of error/statement being killed in
          a natural way - by the time when error occurrs we already
          have logged all drops which were successfull. So we don't
          need to write the whole failed statement with error code
          to binary log.

       b) Base tables in SEs which support atomic DDL.

          Their drop can be rolled back, so we drop them in SE, remove
          from data-dictionary and write corresponding statement to the
          binary log in one atomic transaction all together.

       c) Views.

          Have to be dropped when this function is called as part of
          DROP DATABASE implementation. Dropping them requires
          data-dictionary update only, so can be done atomically
          with b).

       d) Non-existent tables.

          In the absence of IF EXISTS clause cause statement failure.
          We do this check before dropping any tables to get nice atomic
          behavior for most common failure scenario even for tables which
          don't support atomic DDL.

          When IF EXISTS clause is present notes are generated instead of
          error. We assume that non-existing tables support atomic DDL and
          write such tables to binary log together with tables from group b)
          (after all no-op can be rolled back!) to get a nice single DROP
          TABLES statement in the binlog in the default use-case. It is not
          a big problem if this assumption turns out to be false on slave.
          The statement still will be applied correctly (but crash-safeness
          will be sacrificed).

    2) Temporary tables.

       To avoid problems due to shadowing base tables should be always
       binlogged as DROP TEMPORARY TABLE.

       Their drop can't be rolled back even for transactional SEs, on the
       other hand it can't fail once first simple checks are done. So it
       makes sense to drop them after base tables.

       If the current binlog format is row, the IF EXISTS clause needs to be
       appended because one does not know if CREATE TEMPORARY was previously
       written to the binary log.

       Unlike for base tables, it is possible to drop database in which some
       connection has temporary tables open. So we can end-up in situation
       when connection's default database is no more, but still the connection
       has some temporary tables in it. It is possible to drop such tables,
       but we should be careful when binlogging such drop.
       Using "USE db_which_is_no_more;" before DROP TEMPORARY TABLES will
       break replication.

       Temporary tables are further divided into the following groups:

       a) Temporary tables in non-transactional SE
       b) Temporary tables in transactional SE

          DROP TEMPORARY TABLES does not commit an ongoing transaction. So in
          some circumstances we must binlog changes to non-transactional tables
          ahead of transaction, while changes to transactional tables should be
          binlogged as part of transaction.

       c) Non-existent temporary tables.

          Can be non-empty only if DROP TEMPORARY TABLES was used (otherwise
          all non-existent tables go to group 1.d)).

          Similarly to group 1.d) if IF EXISTS clause is absent causes
          statement failure. Otherwise note is generated for each such table.

          The non-existing temporary tables are logged together with
          transactional ones (group 2.b)), if any transactional tables exist
          or if there is only non-existing tables; otherwise are logged
          together with non-transactional ones (group 2.a)).

       This logic ensures that:
       - On master, transactional and non-transactional tables are
         written to different statements.
       - Therefore, slave will never see statements containing both
         transactional and non-transactional temporary tables.
       - Since non-existing temporary tables are logged together with
         whatever type of temporary tables that exist, the slave thus
         writes any statement as just one statement. I.e., the slave
         never splits a statement into two.  This is crucial when GTIDs
         are enabled, since otherwise the statement, which already has
         a GTID, would need two different GTIDs.
  */
  for (TABLE_LIST *table= tables; table; table= table->next_local)
  {
    /*
      Check THD::killed flag, so we can abort potentially lengthy loop.
      This can be relevant for DROP DATABASE, for example.
    */
    if (thd->killed)
      return true;

    if (table->open_type != OT_BASE_ONLY)
    {
      /* DROP DATABASE doesn't deal with temporary tables. */
      DBUG_ASSERT(!drop_ctx->drop_database);

      if (!is_temporary_table(table))
      {
        // A temporary table was not found.
        if (drop_ctx->drop_temporary)
        {
          drop_ctx->nonexistent_tables.push_back(table);
          continue;
        }
        /*
          Not DROP TEMPORARY and no matching temporary table.
          Continue with base tables.
        */
      }
      else
      {
        /*
          A temporary table was found and can be successfully dropped.

          The fact that this temporary table is used by an outer statement
          should be detected and reported as error earlier.
        */
        DBUG_ASSERT(table->table->query_id == thd->query_id);

        if (table->table->file->has_transactions())
          drop_ctx->tmp_trans_tables.push_back(table);
        else
          drop_ctx->tmp_non_trans_tables.push_back(table);
        continue;
      }
    }

    /* We should not try to drop active log tables. Callers enforce this. */
    DBUG_ASSERT(query_logger.check_if_log_table(table, true) == QUERY_LOG_NONE);

    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Abstract_table *abstract_table_def= NULL;
    if (thd->dd_client()->acquire(table->db, table->table_name,
                                  &abstract_table_def))
    {
      /* Error should have been reported by data-dictionary subsystem. */
      return true;
    }

    if (!abstract_table_def)
    {
      // If table is missing try to discover it from some storage engine.
      int result= ha_create_table_from_engine(thd, table->db,
                                              (lower_case_table_names == 2) ?
                                              table->alias :
                                              table->table_name);
      if (result > 0)
      {
        // Error during discovery, error should be reported already.
        return true;
      }
      else if (result == 0)
      {
        // Table was discovered. Re-try to retrieve its definition.
        if (thd->dd_client()->acquire(table->db, table->table_name,
                                      &abstract_table_def))
          return true;
      }
      else // result < 0
      {
        // No table was found.
      }
    }

    if (!abstract_table_def)
      drop_ctx->nonexistent_tables.push_back(table);
    else if (abstract_table_def->type() == dd::enum_table_type::BASE_TABLE)
    {
      const dd::Table *table_def=
        dynamic_cast<const dd::Table*>(abstract_table_def);

      handlerton *hton;
      if (dd::table_storage_engine(thd, table->db, table->table_name,
                                   table_def, &hton))
        return true;

      if (hton->flags & HTON_SUPPORTS_ATOMIC_DDL)
        drop_ctx->base_atomic_tables.push_back(table);
      else
        drop_ctx->base_non_atomic_tables.push_back(table);
    }
    else // View
    {
      if (! drop_ctx->drop_database)
      {
        /*
          Historically, DROP TABLES treats situation when we have a view
          instead of table to be dropped as non-existent table.
        */
        drop_ctx->nonexistent_tables.push_back(table);
      }
      else
        drop_ctx->views.push_back(table);
    }
  }

  return false;
}


/**
  Auxiliary function which evaluates in which situation DROP TABLES
  is regarding GTID and different table groups.
*/

static bool
rm_table_eval_gtid_and_table_groups_state(THD *thd, Drop_tables_ctx *drop_ctx)
{
  if (thd->variables.gtid_next.type == GTID_GROUP)
  {
    /*
      This statement has been assigned GTID.

      In this case we need to take special care about group handling
      and commits, as statement can't be logged/split into several
      statements in this case.

      Three different situations are possible in this case:
      - "normal" when we have one GTID assigned and one group
        to go as single statement to binary logs
      - "prohibited" when we have one GTID assigned and two
        kinds of temporary tables or mix of temporary and
        base tables
      - "awkward" when we have one GTID but several groups or
        several tables in non-atomic base group (1.a).
    */

    if (drop_ctx->drop_database)
    {
      /* DROP DATABASE doesn't drop any temporary tables. */
      DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
      DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());

      if (!drop_ctx->has_base_non_atomic_tables())
      {
        /*
          Normal case. This is DROP DATABASE and we don't have any tables in
          SEs which don't support atomic DDL. Remaining tables, views,
          routines and events can be dropped atomically and atomically logged
          as a single DROP DATABASE statement by the caller.
        */
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_SINGLE_TABLE_GROUP;
      }
      else
      {
        /*
          Awkward case. We have GTID assigned for DROP DATABASE and it needs
          to drop table in SE which doesn't support atomic DDL.

          Most probably we are replicating from older (pre-5.8) master or tables
          on master and slave have different SEs.
          We try to handle situation in the following way - if the whole statement
          succeeds caller will log all changes as a single DROP DATABASE under
          GTID provided. In case of failure we will emit special error saying
          that statement can't be logged correctly and manual intervention is
          required.
        */
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_MANY_TABLE_GROUPS;
      }
    }
    else
    {
      /* Only DROP DATABASE drops views. */
      DBUG_ASSERT(!drop_ctx->has_views());

      if ((drop_ctx->has_tmp_trans_tables() &&
           drop_ctx->has_tmp_non_trans_tables()) ||
          ((drop_ctx->has_base_non_atomic_tables() ||
            drop_ctx->has_base_atomic_tables() ||
            drop_ctx->has_base_nonexistent_tables()) &&
           (drop_ctx->has_tmp_trans_tables() ||
            drop_ctx->has_tmp_non_trans_tables())))
      {
        /*
          Prohibited case. We have either both kinds of temporary tables or
          mix of non-temporary and temporary tables.

          Normally, such DROP TEMPORARY TABLES or DROP TABLES statements are
          written into the binary log at least in two pieces. This is, of
          course, impossible with a single GTID assigned.

          Executing such statements with a GTID assigned is prohibited at
          least since 5.7, so should not create new problems with backward
          compatibility and cross-version replication.

          (Writing deletion of different kinds of temporary and/or base tables
           as single multi-table DROP TABLES under single GTID might be
           theoretically possible in some cases, but has its own problems).
        */
        my_error(ER_GTID_UNSAFE_BINLOG_SPLITTABLE_STATEMENT_AND_GTID_GROUP,
                 MYF(0));
        return true;
      }
      else if (drop_ctx->base_non_atomic_tables.size() == 1 &&
               !drop_ctx->has_base_atomic_tables() &&
               !drop_ctx->has_base_nonexistent_tables())
      {
        /*
          Normal case. Single base table in SE which don't support atomic DDL
          so it will be logged as a single-table DROP TABLES statement.
          Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_nonexistent_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_SINGLE_TABLE_GROUP;
      }
      else if ((drop_ctx->has_base_atomic_tables() ||
                drop_ctx->has_base_nonexistent_tables()) &&
               !drop_ctx->has_base_non_atomic_tables())
      {
        /*
          Normal case. Several base tables which can be dropped atomically.
          Can be logged as one atomic multi-table DROP TABLES statement.
          Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_SINGLE_TABLE_GROUP;
      }
      else if (drop_ctx->has_tmp_trans_tables() ||
               (!drop_ctx->has_tmp_non_trans_tables() &&
                drop_ctx->has_tmp_nonexistent_tables()))
      {
        /*
          Normal case. Some temporary transactional tables (and/or possibly
          some non-existent temporary tables) to be logged as one multi-table
          DROP TEMPORARY TABLES statement.
          Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_base_non_atomic_tables());
        DBUG_ASSERT(!drop_ctx->has_base_atomic_tables() &&
                    !drop_ctx->has_base_nonexistent_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_SINGLE_TABLE_GROUP;
      }
      else if (drop_ctx->has_tmp_non_trans_tables())
      {
        /*
          Normal case. Some temporary non-transactional tables (and possibly
          some non-existent temporary tables) to be logged as one multi-table
          DROP TEMPORARY TABLES statement.
          Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_base_non_atomic_tables());
        DBUG_ASSERT(!drop_ctx->has_base_atomic_tables() &&
                    !drop_ctx->has_base_nonexistent_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_SINGLE_TABLE_GROUP;
      }
      else
      {
        /*
          Awkward case. We have several tables from non-atomic group 1.a, or
          tables from both atomic (1.b, 1.c, 1.d) and non-atomic groups.

          Most probably we are replicating from older (pre-5.8) master or tables
          on master and slave have different SEs.
          We try to handle this situation gracefully by writing single multi-table
          DROP TABLES statement including tables from all groups under GTID provided.
          Of course this means that we are not crash-safe in this case. But we can't
          be fully crash-safe in cases when non-atomic tables are involved anyway.

          Note that temporary tables groups still should be empty in this case.
        */
        DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
        DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::GTID_MANY_TABLE_GROUPS;
      }
    }
  }
  else
  {
    /*
      This statement has no GTID assigned. We can handle any mix of
      groups in this case. However full atomicity is guaranteed only
      in certain scenarios.
    */

    if (drop_ctx->drop_database)
    {
      /* DROP DATABASE doesn't drop any temporary tables. */
      DBUG_ASSERT(!drop_ctx->has_tmp_trans_tables());
      DBUG_ASSERT(!drop_ctx->has_tmp_non_trans_tables());

      if (!drop_ctx->has_base_non_atomic_tables())
      {
        /*
          Fully atomic case. This is DROP DATABASE and we don't have any
          tables in SEs which don't support atomic DDL. Remaining tables,
          views, routines and events can be dropped atomically and atomically
          logged as a single DROP DATABASE statement by the caller.
        */
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::NO_GTID_SINGLE_TABLE_GROUP;
      }
      else
      {
        /*
          Non-atomic case. This is DROP DATABASE which needs to drop some
          tables in SE which doesn't support atomic DDL. To improve
          crash-safety we log separate DROP TABLE IF EXISTS for each such
          table dropped. Remaining tables, views, routines and events are
          dropped atomically and atomically logged as a single DROP DATABASE
          statement by the caller.
        */
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::NO_GTID_MANY_TABLE_GROUPS;
      }
    }
    else
    {
      /* Only DROP DATABASE drops views. */
      DBUG_ASSERT(!drop_ctx->has_views());

      if (drop_ctx->base_non_atomic_tables.size() == 1 &&
          !drop_ctx->has_base_atomic_tables() &&
          !drop_ctx->has_base_nonexistent_tables() &&
          !drop_ctx->has_tmp_trans_tables() &&
          !drop_ctx->has_tmp_non_trans_tables())
      {
        /*
          Simple non-atomic case. Single base table in SE which don't
          support atomic DDL so it will be logged as a single-table
          DROP TABLES statement. Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_tmp_nonexistent_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::NO_GTID_SINGLE_TABLE_GROUP;
      }
      else if ((drop_ctx->has_base_atomic_tables() ||
                drop_ctx->has_base_nonexistent_tables()) &&
               !drop_ctx->has_base_non_atomic_tables() &&
               !drop_ctx->has_tmp_trans_tables() &&
               !drop_ctx->has_tmp_non_trans_tables())
      {
        /*
          Fully atomic case. Several base tables which can be dropped
          atomically. Can be logged as one atomic multi-table DROP TABLES
          statement. Other groups are empty.
        */
        DBUG_ASSERT(!drop_ctx->has_tmp_nonexistent_tables());
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::NO_GTID_SINGLE_TABLE_GROUP;
      }
      else if (!drop_ctx->has_base_non_atomic_tables() &&
               !drop_ctx->has_base_atomic_tables() &&
               !drop_ctx->has_base_nonexistent_tables())
      {
        /* No base tables to be dropped. */
        if (drop_ctx->has_tmp_trans_tables() &&
            drop_ctx->has_tmp_non_trans_tables())
        {
          /*
            Complex case with temporary tables. We have both transactional
            and non-transactional temporary tables and no base tables at all.

            We will log separate DROP TEMPORARY TABLES statements for each of
            two groups.
          */
          drop_ctx->gtid_and_table_groups_state=
            Drop_tables_ctx::NO_GTID_MANY_TABLE_GROUPS;
        }
        else
        {
          /*
            Simple case with temporary tables. We have either only
            transactional or non-transactional temporary tables.
            Possibly some non-existent temporary tables.

            We can log our statement as a single DROP TEMPORARY TABLES
            statement.
          */
          DBUG_ASSERT((drop_ctx->has_tmp_trans_tables() &&
                       !drop_ctx->has_tmp_non_trans_tables()) ||
                      (!drop_ctx->has_tmp_trans_tables() &&
                       drop_ctx->has_tmp_non_trans_tables()) ||
                      (!drop_ctx->has_tmp_trans_tables() &&
                       !drop_ctx->has_tmp_non_trans_tables() &&
                       drop_ctx->has_tmp_nonexistent_tables()));
          drop_ctx->gtid_and_table_groups_state=
            Drop_tables_ctx::NO_GTID_SINGLE_TABLE_GROUP;
        }
      }
      else
      {
        /*
          Complex non-atomic case. We have several tables from non-atomic group 1.a,
          or tables from both atomic (1.b, 1.c, 1.d) and non-atomic groups, or
          mix of base and temporary tables.

          Our statement will be written to binary log as several DROP TABLES and
          DROP TEMPORARY TABLES statements.
        */
        drop_ctx->gtid_and_table_groups_state=
          Drop_tables_ctx::NO_GTID_MANY_TABLE_GROUPS;
      }
    }
  }

  return false;
}

/**
  Auxiliary function which drops single base table.

  @param        thd             Thread handler.
  @param        drop_ctx        DROP TABLES runtime context.
  @param        table           Table to drop.
  @param        atomic          Indicates whether table to be dropped is in SE
                                which supports atomic DDL, so changes to the
                                data-dictionary should not be committed.
  @param[in,out] post_ddl_htons Set of handlertons for tables in SEs supporting
                                atomic DDL for which post-DDL hook needs to
                                be called after statement commit or rollback.

  @sa mysql_rm_table_no_locks().

  @retval  False - ok
  @retval  True  - error
*/

static bool
drop_base_table(THD *thd, const Drop_tables_ctx &drop_ctx,
                TABLE_LIST *table, bool atomic,
                std::set<handlerton*> *post_ddl_htons)
{
  char path[FN_REFLEN + 1];

  /* Check that we have an exclusive lock on the table to be dropped. */
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                 table->db, table->table_name, MDL_EXCLUSIVE));

  /*
    Good point to check if we were killed for non-atomic tables group.
    All previous tables are dropped both in SE and data-dictionary and
    corresponding DROP TABLE statements are written to binary log.
    We didn't do anything for the current table yet.

    For atomic tables the exact place of this check should not matter.
  */
  if (thd->killed)
    return true;

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_def= nullptr;
  if (thd->dd_client()->acquire<dd::Table>(table->db, table->table_name,
                                           &table_def))
    return true;

  if (!table_def)
  {
    DBUG_ASSERT(0);
    String tbl_name;
    append_table_name(&tbl_name, table);
    my_error(ER_BAD_TABLE_ERROR, MYF(0), tbl_name.c_ptr());
    return true;
  }

  handlerton *hton;
  if (dd::table_storage_engine(thd, table->db, table->table_name,
                               table_def, &hton))
  {
    DBUG_ASSERT(0);
    return true;
  }

  if (thd->locked_tables_mode)
  {
    /*
      Under LOCK TABLES we still have table open at this point.
      Close it and remove all instances from Table/Table Definition
      cache.

      Note that we won't try to reopen tables in storage engines
      supporting atomic DDL those removal will be later rolled back
      thanks to some error. Such situations should be fairly rare.
    */
    close_all_tables_for_name(thd, table->table->s, true, NULL);
    /*
      Prepare TABLE_LIST element for Query Cache invalidation,
      also marks table as needing MDL release.
    */
    table->table= 0;
  }
  else
  {
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table->db, table->table_name,
                     false);
  }

  (void) build_table_filename(path, sizeof(path) - 1, table->db,
                              table->table_name, "",
                              table->internal_tmp_table ? FN_IS_TMP : 0);

  int error= ha_delete_table(thd, hton, path, table->db, table->table_name,
                             table_def, !drop_ctx.drop_database);

  /*
    Table was present in data-dictionary but is missing in storage engine.
    This situation can occur for SEs which don't support atomic DDL due
    to crashes. In this case we allow table removal from data-dictionary
    and reporting success if IF EXISTS clause was specified.

    Such situation should not be possible for SEs supporting atomic DDL,
    but we still play safe even in this case and allow table removal.
  */
#ifdef ASSERT_TO_BE_ENABLED_ONCE_WL7016_IS_READY
   DBUG_ASSERT(!atomic || (error != ENOENT && error != HA_ERR_NO_SUCH_TABLE));
#endif

  if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) && drop_ctx.if_exists)
  {
    error= 0;
    thd->clear_error();
  }

  if (atomic && hton->post_ddl)
    post_ddl_htons->insert(hton);

  if (error)
  {
    if (error == HA_ERR_ROW_IS_REFERENCED)
      my_error(ER_ROW_IS_REFERENCED, MYF(0));
    else if (error == HA_ERR_TOO_MANY_CONCURRENT_TRXS)
      my_error(HA_ERR_TOO_MANY_CONCURRENT_TRXS, MYF(0));
    else
    {
      String tbl_name;
      append_table_name(&tbl_name, table);
      my_error(((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) ?
                ER_ENGINE_CANT_DROP_MISSING_TABLE :
                ER_ENGINE_CANT_DROP_TABLE),
               MYF(0), tbl_name.c_ptr());
    }
    return true;
  }

  /*
    Invalidate query cache once we deleted table in SE even if we will
    fail to fully delete the table.
  */
  query_cache.invalidate_single(thd, table, false);

#ifdef HAVE_PSI_SP_INTERFACE
  if (remove_all_triggers_from_perfschema(thd, table))
    return true;
#endif
  /*
    Don't delete entry from DD in case we are dropping
    DD tables in case of upgrade.
  */
  if (!dd_upgrade_flag)
  {
    /*
      Remove table from data-dictionary and immediately commit this change
      if we are removing table in SE which does not support atomic DDL.
      This way chances of SE and data-dictionary getting out of sync in
      case of crash are reduced.

      Things will go bad if we will fail to delete table from data-dictionary
      as table is already gone in SE. But this should be really rare situation
      (OOM, out of disk space, bugs). Also user can fix it by running DROP TABLE
      IF EXISTS on the same table again.

      Don't commit the changes if table belongs to SE supporting atomic DDL.
    */
    error= dd::drop_table(thd, table->db, table->table_name,
                          table_def, !atomic) ||
           update_referencing_views_metadata(thd, table, !atomic, nullptr);
  }

  if (error)
    return true;

  return false;
}


/**
  Execute the drop of a normal or temporary table.

  @param  thd             Thread handler
  @param  tables          Tables to drop
  @param  if_exists       If set, don't give an error if table doesn't exists.
                          In this case we give an warning of level 'NOTE'
  @param  drop_temporary  Only drop temporary tables
  @param  drop_database   This is DROP DATABASE statement. Drop views
                          and handle binary logging in a special way.
  @param[out] dropped_non_atomic_flag Indicates whether we have dropped some
                                      tables in SEs which don't support atomic
                                      DDL.
  @param[out] post_ddl_htons     Set of handlertons for tables in SEs supporting
                                 atomic DDL for which post-DDL hook needs to
                                 be called after statement commit or rollback.
  @param[out] dropped_atomic     List of tables in SEs supporting atomic DDL
                                 which we have managed to drop. This parameter
                                 is a workaround used by DROP DATABASE until
                                 WL#7016 is implemented.

  @retval  False - ok
  @retval  True  - error

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

bool mysql_rm_table_no_locks(THD *thd, TABLE_LIST *tables, bool if_exists,
                             bool drop_temporary, bool drop_database,
                             bool *dropped_non_atomic_flag,
                             std::set<handlerton*> *post_ddl_htons
#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
                             , Prealloced_array<TABLE_LIST*, 1> *dropped_atomic
#endif
                             )
{
  Drop_tables_ctx drop_ctx(if_exists, drop_temporary, drop_database);

  bool default_db_doesnt_exist= false;

  DBUG_ENTER("mysql_rm_table_no_locks");

  *dropped_non_atomic_flag= false;

  if (rm_table_sort_into_groups(thd, &drop_ctx, tables))
    DBUG_RETURN(true);

  /*
    Figure out in which situation we are regarding GTID and different
    table groups.
  */
  if (rm_table_eval_gtid_and_table_groups_state(thd, &drop_ctx))
    DBUG_RETURN(true);

  if (!drop_ctx.if_exists && drop_ctx.has_any_nonexistent_tables())
  {
    /*
      No IF EXISTS clause and some non-existing tables.

      Fail before dropping any tables. This gives us nice "atomic" (succeed
      or don't drop anything) behavior for most common failure scenario even
      for tables which don't support atomic DDL.

      Do this check after getting full list of missing tables to produce
      better error message.
    */
    String wrong_tables;

    for (TABLE_LIST *table : drop_ctx.nonexistent_tables)
    {
      if (wrong_tables.length())
        wrong_tables.append(',');
      append_table_name(&wrong_tables, table);
    }

    my_error(ER_BAD_TABLE_ERROR, MYF(0), wrong_tables.c_ptr());
    DBUG_RETURN(true);
  }

  /*
    Check early if default database exists. We don't want code responsible
    for dropping temporary tables fail due to this check after some tables
    were dropped already.
  */
  if (thd->db().str != NULL)
  {
    bool exists= false;
    if (dd::schema_exists(thd, thd->db().str, &exists))
      DBUG_RETURN(true);
    default_db_doesnt_exist= !exists;
  }

  if (drop_ctx.if_exists && drop_ctx.has_any_nonexistent_tables())
  {
    for (TABLE_LIST *table : drop_ctx.nonexistent_tables)
    {
      String tbl_name;
      append_table_name(&tbl_name, table);
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_BAD_TABLE_ERROR,
                          ER_THD(thd, ER_BAD_TABLE_ERROR),
                          tbl_name.c_ptr());
    }
  }

  if (drop_ctx.has_base_non_atomic_tables())
  {
    /*
      Handle base tables in storage engines which don't support atomic DDL.

      Their drop can't be rolled back in case of crash or error. So we drop
      each such table individually and write to binlog a single-table DROP
      TABLE statement corresponding to this action right after it.
      This increases chances of SE, data-dictionary and binary log being in
      sync if crash occurs.
      This also handles case of error/statement being killed in a natural
      way - by the time when error occurrs we already have logged all drops
      which were successfull. So we don't need to write the whole failed
      statement with error code to binary log.

      Note that we process non-atomic tables before atomic ones in order to
      avoid situations when DROP TABLES for mixed set of tables will fail
      and leave changes to atomic, "transactional" tables around.
    */
    for (TABLE_LIST *table : drop_ctx.base_non_atomic_tables)
    {
      if (drop_base_table(thd, drop_ctx,
                          table, false /* non-atomic */,
                          nullptr))
        goto err_with_rollback;

      *dropped_non_atomic_flag= true;

      drop_ctx.dropped_non_atomic.push_back(table);

      if (!drop_ctx.has_gtid_many_table_groups())
      {
        /*
          We don't have GTID assigned, or we have GTID assigned and this is
          single-table DROP TABLE for this specific table.

          Write single-table DROP TABLE statement to binary log.

          Do this even if table was dropped as part of DROP DATABASE statement,
          as this descreases chance of things getting out of sync in case of
          crash.
        */
        if (drop_ctx.drop_database)
        {
          if (mysql_bin_log.is_open())
          {
            String built_query;

            built_query.set_charset(system_charset_info);
            built_query.append("DROP TABLE IF EXISTS ");

            append_identifier(thd, &built_query, table->table_name,
                              table->table_name_length, system_charset_info,
                              thd->charset());

            built_query.append(" /* generated by server */");

            thd->add_to_binlog_accessed_dbs(table->db);


            Query_log_event qinfo(thd, built_query.ptr(),
                                built_query.length(), false,
                                true, false, 0);
            qinfo.db= table->db;
            qinfo.db_len= table->db_length;

            if (mysql_bin_log.write_event(&qinfo))
              goto err_with_rollback;
          }
        }
        else
        {
          Drop_tables_query_builder built_query(thd, false /* no TEMPORARY */,
                                                drop_ctx.if_exists,
                                                false /* stmt binlog cache */,
                                                false /* db exists */);

          built_query.add_table(table);

          if (built_query.write_bin_log())
            goto err_with_rollback;
        }

        if (drop_ctx.has_no_gtid_single_table_group() ||
            drop_ctx.has_gtid_single_table_group())
        {
          /*
            This was a single-table DROP TABLE for this specific table.
            Commit change to binary log and/or mark GTID as executed instead.
            In theory, we also can update slave info atomically with binlog/
            GTID changes,
          */
          if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
            goto err_with_rollback;
        }
        else
        {
          /*
            We don't have GTID assigned and this is not single-table
            DROP TABLE. Commit change to binary log (if there was any)
            and get GTID assigned for our single-table change. Do not
            release ANONYMOUS_GROUP ownership yet as there can be more
            tables to drop and corresponding statements to write to
            binary log. Do not update slave info as there might be more
            groups.
          */
          DBUG_ASSERT(drop_ctx.has_no_gtid_many_table_groups());

          thd->is_commit_in_middle_of_statement= true;
          bool error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));
          thd->is_commit_in_middle_of_statement= false;

          if (error)
            goto err_with_rollback;
        }
      }
      else
      {
        /*
          We have GTID assigned and several tables from SEs which don't support
          atomic DDL, or tables in different groups. Postpone writing to binary
          log/marking GTID as executed until all tables are processed.

          Nothing to commit here as change to data-dictionary is already
          committed earlier.
        */
      }
    }
  }

  if (drop_ctx.has_base_atomic_tables() || drop_ctx.has_views() ||
      drop_ctx.has_base_nonexistent_tables())
  {
    /*
      Handle base tables in SEs which support atomic DDL, as well as views
      and non-existent tables.

      Drop all these objects in SE and data-dictionary in a single atomic
      transaction. Write corresponding multi-table DROP TABLE statement to
      the binary log as part of the same transaction.
    */
    DEBUG_SYNC(thd, "rm_table_no_locks_before_delete_table");
    DBUG_EXECUTE_IF("sleep_before_no_locks_delete_table",
                    my_sleep(100000););

    DBUG_EXECUTE_IF("rm_table_no_locks_abort_before_atomic_tables",
                    {
                      my_error(ER_UNKNOWN_ERROR, MYF(0));
                      goto err_with_rollback;
                    });

    for (TABLE_LIST *table : drop_ctx.base_atomic_tables)
    {
      if (drop_base_table(thd, drop_ctx,
                          table, true /* atomic */,
                          post_ddl_htons))
      {
#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
        if (thd->transaction_rollback_request)
          goto err_with_rollback;

        if (!drop_ctx.drop_database &&
            (dropped_atomic->size() != 0 ||
             (drop_ctx.has_gtid_many_table_groups() &&
              drop_ctx.has_dropped_non_atomic())))
        {
          Drop_tables_query_builder built_query(thd, false /* no TEMPORARY */,
                                                drop_ctx.if_exists,
                                                /* stmt or trx cache. */
                                                dropped_atomic->size() != 0,
                                                false /* db exists */);
          if (drop_ctx.has_gtid_many_table_groups() &&
              drop_ctx.has_dropped_non_atomic())
            built_query.add_array(drop_ctx.dropped_non_atomic);
          built_query.add_array(*dropped_atomic);
          built_query.write_bin_log();
        }

        if (drop_ctx.drop_database)
        {
          Disable_gtid_state_update_guard disabler(thd);
          (void) trans_commit_stmt(thd);
          (void) trans_commit_implicit(thd);
        }
        else
        {
          // We need to turn off updating of slave info here
          // without conflicting with GTID update.
          Disable_slave_info_update_guard disabler(thd);

          (void) trans_commit_stmt(thd);
          (void) trans_commit_implicit(thd);
        }
        DBUG_RETURN(true);
#else
        goto err_with_rollback;
#endif
      }

#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
      dropped_atomic->push_back(table);
#endif
    }

    DBUG_EXECUTE_IF("rm_table_no_locks_abort_after_atomic_tables",
                    {
                      my_error(ER_UNKNOWN_ERROR, MYF(0));
                      /* WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY */
                      (void) trans_commit_stmt(thd);
                      (void) trans_commit_implicit(thd);
                      /* WORKAROUND_ENDS */
                      goto err_with_rollback;
                    });

    for (TABLE_LIST *table : drop_ctx.views)
    {
      /* Check that we have an exclusive lock on the view to be dropped. */
      DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                     table->db, table->table_name,
                                     MDL_EXCLUSIVE));

      tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table->db, table->table_name,
                       false);

      const dd::View *view= nullptr;
      if (thd->dd_client()->acquire(table->db, table->table_name, &view))
        goto err_with_rollback;

      if (thd->dd_client()->drop(view) ||
          update_referencing_views_metadata(thd, table, false, nullptr))
        goto err_with_rollback;

      /*
        No need to log anything since we drop views here only if called by
        DROP DATABASE implementation.
      */
      DBUG_ASSERT(drop_ctx.drop_database);

      query_cache.invalidate_single(thd, table, FALSE);
    }

#ifndef DBUG_OFF
    for (TABLE_LIST *table : drop_ctx.nonexistent_tables)
    {
      /*
        Check that we have an exclusive lock on the table which we were
        supposed drop.
      */
      DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                     table->db, table->table_name,
                                     MDL_EXCLUSIVE));
    }
#endif

    DEBUG_SYNC(thd, "rm_table_no_locks_before_binlog");

    int error= 0;

    if (drop_ctx.drop_database)
    {
      /*
        This is DROP DATABASE.

        If we don't have GTID assigned removal of tables from this group will be
        logged as DROP DATABASE and committed atomically, together with removal of
        events and stored routines, by the caller.

        The same thing should happen if we have GTID assigned and tables only from
        this group.

        If we have GTID assigned and mix of tables from SEs which support atomic
        DDL and which don't support it we will still behave in similar way.
        If the whole statement succeeds removal of tables from all groups will
        be logged as single DROP DATABASE statement. In case of failure we will
        report special error, but in addition it makes sense to rollback all changes
        to tables in SEs supporting atomic DDL.

        So do nothing here in all three cases described above.
      */
#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
      Disable_gtid_state_update_guard disabler(thd);
      error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));
#endif
    }
    else if (!drop_ctx.has_gtid_many_table_groups())
    {
      /*
        We don't have GTID assigned, or we have GTID assigned and our DROP
        TABLES only drops table from this group, so we have fully atomic
        multi-table DROP TABLES statement.

        If we have not dropped any tables at all (we have only non-existing
        tables) we don't have transaction started. We can't use binlog's
        trx cache in this case as it requires active transaction with valid
        XID.
      */
      Drop_tables_query_builder built_query(thd, false /* no TEMPORARY */,
                                            drop_ctx.if_exists,
                                            /* stmt or trx cache. */
                                            drop_ctx.has_base_atomic_tables(),
                                            false /* db exists */);


      built_query.add_array(drop_ctx.base_atomic_tables);
      built_query.add_array(drop_ctx.nonexistent_tables);

      if (built_query.write_bin_log())
        goto err_with_rollback;

      if (drop_ctx.has_no_gtid_single_table_group() ||
          drop_ctx.has_gtid_single_table_group())
      {
        /*
          This is fully atomic multi-table DROP TABLES.
          Commit changes to SEs, data-dictionary and binary log/or
          and mark GTID as executed/update slave info tables atomically.
        */
        error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));
      }
      else
      {
        /*
          We don't have GTID assigned and this is not fully-atomic DROP TABLES.
          Commit changes to SE, data-dictionary and binary log and get GTID
          assigned for our changes.
          Do not release ANONYMOUS_GROUP ownership and update slave info yet
          as there can be more tables (e.g. temporary) to drop and corresponding
          statements to write to binary log.
        */
        DBUG_ASSERT(drop_ctx.has_no_gtid_many_table_groups());

        thd->is_commit_in_middle_of_statement= true;
        error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));
        thd->is_commit_in_middle_of_statement= false;
      }
    }
    else
    {
      /*
        We have GTID assigned, some tables from SEs which don't support
        atomic DDL and some from SEs which do.

        Postpone writing to binary log and marking GTID as executed until
        later stage. We also postpone committing removal of tables in SEs
        supporting atomic DDL and corresponding changes to the data-
        dictionary until the same stage. This allows to minimize change
        difference between SEs/data-dictionary and binary log in case of
        crash.

        If crash occurs binary log won't contain any traces about removal
        of tables in both SEs support and not-supporting atomic DDL.
        And only tables in SEs not supporting atomic DDL will be missing
        from SEs and the data-dictionary. Since removal of tables in SEs
        supporting atomic DDL will be rolled back during recovery.
      */
    }

    if (error)
      goto err_with_rollback;
  }

  if (!drop_ctx.drop_database && drop_ctx.has_gtid_many_table_groups())
  {
    /*
      We DROP TABLES statement with GTID assigned and either several tables
      from SEs which don't support atomic DDL, or at least one table from
      such SE and some tables from SEs which do support atomic DDL.

      We have postponed write to binlog earlier. Now it is time to do it.

      If we don't have active transaction at this point (i.e. no tables
      in SE supporting atomic DDL were dropped) we can't use binlog's trx
      cache for this. as it requires active transaction with valid XID.
      If we have active transaction (i.e. some tables in SE supporting
      atomic DDL were dropped) we have to use trx cache to ensure that
      our transaction is properly recovered in case of crash/restart.
    */
    Drop_tables_query_builder built_query(thd, false /* no TEMPORARY */,
                                          drop_ctx.if_exists,
                                          /* trx or stmt cache */
                                          drop_ctx.has_base_atomic_tables(),
                                          false /* db exists */);

    built_query.add_array(drop_ctx.base_non_atomic_tables);
    built_query.add_array(drop_ctx.base_atomic_tables);
    built_query.add_array(drop_ctx.nonexistent_tables);

    if (built_query.write_bin_log())
      goto err_with_rollback;

    /*
      Commit our changes to the binary log (if any) and mark GTID
      as executed. This also commits removal of tables in SEs
      supporting atomic DDL from SE and the data-dictionary.
      In theory, we can update slave info atomically with binlog/GTID
      changes here.
    */
    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      goto err_with_rollback;
  }

  /*
    Dropping of temporary tables cannot be rolled back. On the other hand it
    can't fail at this stage. So to get nice error handling behavior
    (either fully succeed or fail and do nothing (if there are no tables
    which don't support atomic DDL)) we process such tables after we are
    done with base tables.

    DROP TEMPORARY TABLES does not commit an ongoing transaction. So in
    some circumstances we must binlog changes to non-transactional
    ahead of transaction (so we need to tell binlog that these changes
    are non-transactional), while changes to transactional tables
    should be binlogged as part of transaction.
  */
  if (drop_ctx.has_tmp_non_trans_tables())
  {
    /*
      Handle non-transactional temporary tables.
    */

    /* DROP DATABASE doesn't deal with temporary tables. */
    DBUG_ASSERT(!drop_ctx.drop_database);

    /*
      If the current format is row, the IF EXISTS clause needs to be
      appended because one does not know if CREATE TEMPORARY was
      previously written to the binary log.

      If default database does not exist, set
      'is_drop_tmp_if_exists_with_no_defaultdb flag to 'true',
      so that the 'DROP TEMPORARY TABLE IF EXISTS' command is logged
      with a fully-qualified table name and we don't write "USE db"
      prefix.
    */
    bool log_if_exists= (thd->is_current_stmt_binlog_format_row() ||
                         drop_ctx.if_exists);
    bool is_drop_tmp_if_exists_with_no_defaultdb= log_if_exists &&
                                                  default_db_doesnt_exist;

    Drop_tables_query_builder built_query(thd, true /* DROP TEMPORARY */,
                                log_if_exists, false /* stmt cache */,
                                is_drop_tmp_if_exists_with_no_defaultdb);

    for (TABLE_LIST *table : drop_ctx.tmp_non_trans_tables)
    {
      /*
        Don't check THD::killed flag. We can't rollback deletion of
        temporary table, so aborting on KILL will make DROP TABLES
        less atomic.
        OTOH it is unlikely that we have many temporary tables to drop
        so being immune to KILL is not that horrible in most cases.
      */
      drop_temporary_table(thd, table);
    }

    built_query.add_array(drop_ctx.tmp_non_trans_tables);

    /*
      If there are no transactional temporary tables to be dropped
      add non-existent tables to this group. This ensures that on
      slave we won't split DROP TEMPORARY TABLES even if some tables
      are missing on it (which is no-no for GTID mode).
    */
    if (drop_ctx.drop_temporary && !drop_ctx.has_tmp_trans_tables())
      built_query.add_array(drop_ctx.nonexistent_tables);

    thd->get_transaction()->mark_dropped_temp_table(Transaction_ctx::STMT);
    thd->thread_specific_used= true;

    if (built_query.write_bin_log())
      goto err_with_rollback;

    if (!drop_ctx.has_gtid_single_table_group())
    {
      /*
        We don't have GTID assigned. If we are not inside of transaction
        commit transaction in binary log to get one for our statement.
      */
      if (mysql_bin_log.is_open() &&
          ! thd->in_active_multi_stmt_transaction())
      {
        /*
          The single purpose of this hack is to generate GTID for the DROP
          TEMPORARY TABLES statement we just have written.

          Some notes about it:

          *) if the binary log is closed GTIDs are not generated, so there is
             no point in the below "commit".
          *) thd->in_active_multi_stmt_transaction() is true means that there
             is an active transaction with some changes to transactional tables
             and in binlog transactional cache. Doing "commit" in such a case
             will commit these changes in SE and flush binlog's cache to disk,
             so can not be allowed.
             OTOH, when thd->in_active_multi_stmt_transaction() false and
             thd->in_multi_stmt_transaction_mode() is true there is
             transaction from user's point of view. However there were no
             changes to transactional tables to commit (all changes were only
             to non-transactional tables) and nothing in binlog transactional
             cache (all changes to non-transactional tables were written to
             binlog directly). Calling "commit" in this case won't do anything
             besides generating GTID and can be allowed.
          *) We use MYSQL_BIN_LOG::commit() and not trans_commit_implicit(),
             for example, because we don't want to end user's explicitly
             started transaction.
          *) In theory we can allow to update slave info here by not raising
             THD::is_commit_in_middle_of_statement flag if we are in
             no-GTID-single-group case. However there is little benefit from
             it as dropping of temporary tables should not fail.

          TODO: Consider if there is some better way to achieve this.
                For example, can we use trans_commit_implicit() to split
                out temporary parts from DROP TABLES statement or when
                splitting DROP TEMPORARY TABLES and there is no explicit
                user transaction. And just write two temporary parts
                to appropriate caches in case when DROP TEMPORARY is used
                inside of user's transaction?
        */
        thd->is_commit_in_middle_of_statement= true;
        bool error= mysql_bin_log.commit(thd, true);
        thd->is_commit_in_middle_of_statement= false;

        if (error)
          goto err_with_rollback;
      }
    }
    else
    {
      /*
        We have GTID assigned. Rely on commit at the end of statement or
        transaction to flush changes to binary log and mark GTID as executed.
      */
    }
  }

  if (drop_ctx.has_tmp_trans_tables() ||
      (!drop_ctx.has_tmp_non_trans_tables() &&
       drop_ctx.has_tmp_nonexistent_tables()))
  {
    /*
      Handle transactional temporary tables (and possibly non-existent
      temporary tables if they were not handled earlier).
    */

    /* DROP DATABASE doesn't deal with temporary tables. */
    DBUG_ASSERT(!drop_ctx.drop_database);

    /*
      If the current format is row, the IF EXISTS clause needs to be
      appended because one does not know if CREATE TEMPORARY was
      previously written to the binary log.

      If default database does not exist, set
      'is_drop_tmp_if_exists_with_no_defaultdb flag to 'true',
      so that the 'DROP TEMPORARY TABLE IF EXISTS' command is logged
      with a fully-qualified table name and we don't write "USE db"
      prefix.

      If we are executing DROP TABLES (without TEMPORARY clause) we
      can't use binlog's trx cache, as it requires activetransaction
      with valid XID. Luckily, trx cache is not strictly necessary in
      this case and DROP TEMPORARY TABLES where it is really needed is
      exempted from this rule.
    */
    bool log_if_exists= (thd->is_current_stmt_binlog_format_row() ||
                         drop_ctx.if_exists);
    bool is_drop_tmp_if_exists_with_no_defaultdb= log_if_exists &&
                                                  default_db_doesnt_exist;

    Drop_tables_query_builder built_query(thd, true /* DROP TEMPORARY */,
                                log_if_exists,
                                drop_ctx.drop_temporary /* trx/stmt cache */,
                                is_drop_tmp_if_exists_with_no_defaultdb);

    for (TABLE_LIST *table : drop_ctx.tmp_trans_tables)
    {
      /*
        Don't check THD::killed flag. We can't rollback deletion of
        temporary table, so aborting on KILL will make DROP TABLES
        less atomic.
        OTOH it is unlikely that we have many temporary tables to drop
        so being immune to KILL is not that horrible in most cases.
      */
      drop_temporary_table(thd, table);
    }

    built_query.add_array(drop_ctx.tmp_trans_tables);

    /*
      Add non-existent temporary tables to this group if there are some
      and they were not handled earlier.
      This ensures that on slave we won't split DROP TEMPORARY TABLES
      even if some tables are missing on it (which is no-no for GTID mode).
    */
    if (drop_ctx.drop_temporary)
      built_query.add_array(drop_ctx.nonexistent_tables);

    thd->get_transaction()->mark_dropped_temp_table(Transaction_ctx::STMT);
    thd->thread_specific_used= true;

    if (built_query.write_bin_log())
      goto err_with_rollback;

    if (!drop_ctx.has_gtid_single_table_group())
    {
      /*
        We don't have GTID assigned. If we are not inside of transaction
        commit transaction in binary log to get one for our statement.
      */
      if (mysql_bin_log.is_open() &&
          ! thd->in_active_multi_stmt_transaction())
      {
        /*
          See the rationale for the hack with "commit" above.
        */
        thd->is_commit_in_middle_of_statement= true;
        bool error= mysql_bin_log.commit(thd, true);
        thd->is_commit_in_middle_of_statement= false;

        if (error)
          goto err_with_rollback;
      }
    }
    else
    {
      /*
        We have GTID assigned. Rely on commit at the end of statement or
        transaction to flush changes to binary log and mark GTID as executed.
      */
    }
  }

  if (!drop_ctx.drop_database)
  {
    for (handlerton *hton : *post_ddl_htons)
      hton->post_ddl(thd);
  }

  DBUG_RETURN(false);

err_with_rollback:
  if (!drop_ctx.drop_database)
  {
    /*
      Be consistent with successfull case. Rollback statement
      and call post-DDL hooks within this function.

      Note that this will rollback deletion of tables in SEs
      supporting atomic DDL only. Tables in engines which
      don't support atomic DDL are completely gone at this
      point.
    */

    if (drop_ctx.has_gtid_many_table_groups() &&
        drop_ctx.has_dropped_non_atomic())
    {
      /*
        So far we have been postponing writing DROP TABLES statement for
        tables in engines not supporting atomic DDL. We are going to write
        it now and let it to consume GTID assigned. Hence rollback of
        tables deletion of in SEs supporting atomic DDL should not rollback
        GTID. Use guard class to disable this.
      */
      Disable_gtid_state_update_guard disabler(thd);
      trans_rollback_stmt(thd);
      /*
        Full rollback in case we have THD::transaction_rollback_request
        and to synchronize DD state in cache and on disk (as statement
        rollback doesn't clear DD cache of modified uncommitted objects).
      */
      trans_rollback(thd);
    }
    else
    {
      trans_rollback_stmt(thd);
      /*
        Full rollback in case we have THD::transaction_rollback_request
        and to synchronize DD state in cache and on disk (as statement
        rollback doesn't clear DD cache of modified uncommitted objects).
      */
      trans_rollback(thd);
    }

    for (handlerton *hton : *post_ddl_htons)
         hton->post_ddl(thd);

    if (drop_ctx.has_gtid_many_table_groups() &&
        drop_ctx.has_dropped_non_atomic())
    {
      /*
        We have some tables dropped in SEs which don't support atomic DDL for
        which there were no binlog events written so far. Now we are going to
        write DROP TABLES statement for them and mark GTID as executed.
        This is not totally correct since original statement is only partially
        executed, but is consistent with 5.7 behavior.

        TODO: Long-term we probably should generate new slave-based GTID for
              this event, or report special error about partial execution.

        We don't have active transaction at this point so we can't use binlog's
        trx cache for this. It requires active transaction with valid XID.

      */
      Drop_tables_query_builder built_query(thd, false /* no TEMPORARY */,
                                            drop_ctx.if_exists,
                                            false /* stmt cache*/,
                                            false /* db exists */);

      built_query.add_array(drop_ctx.dropped_non_atomic);

      (void) built_query.write_bin_log();

      // Write statement to binary log and mark GTID as executed.

      // We need to turn off updating of slave info
      // without conflicting with GTID update.
      {
        Disable_slave_info_update_guard disabler(thd);

        (void) trans_commit_stmt(thd);
        (void) trans_commit_implicit(thd);
      }
    }
  }
  DBUG_RETURN(true);
}


/**
  Quickly remove a table.

  @param thd         Thread context.
  @param base        The handlerton handle.
  @param db          The database name.
  @param table_name  The table name.
  @param flags       Flags for build_table_filename().

  @note In case when NO_DD_COMMIT flag was used, the caller must rollback
        both statement and transaction on failure. This is necessary to
        revert results of handler::ha_delete_table() call in case when
        update to the data-dictionary which follows it fails. Also this must
        be done before any further accesses to DD. @sa dd::drop_table().

  @return False in case of success, True otherwise.
*/

bool quick_rm_table(THD *thd, handlerton *base, const char *db,
                    const char *table_name, uint flags)
{
  DBUG_ENTER("quick_rm_table");

  // Build the schema qualified table name, to be submitted to the handler.
  char path[FN_REFLEN + 1];
  (void) build_table_filename(path, sizeof(path) - 1,
                              db, table_name, "", flags);


  const dd::Table *table_def= nullptr;
  if (thd->dd_client()->acquire<dd::Table>(db, table_name, &table_def))
    DBUG_RETURN(true);

  /* We try to remove non-existing tables in some scenarios. */
  if (!table_def)
    DBUG_RETURN(false);

  if (ha_delete_table(thd, base, path, db, table_name, table_def, 0))
    DBUG_RETURN(true);

  // Remove the table object from the data dictionary. If this fails, the
  // DD operation is already rolled back, and we must return with an error.
  // Note that the DD operation is done after invoking the SE. This is
  // because the DDL code will handle situations where a table is present
  // in the DD while missing from the SE, but not the opposite.
  if (!dd::get_dictionary()->is_dd_table_name(db, table_name) &&
      dd::drop_table(thd, db, table_name, table_def,
                     !(flags & NO_DD_COMMIT)))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
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

static int sort_keys(const void *a_arg, const void *b_arg)
{
  const KEY *a= pointer_cast<const KEY*>(a_arg);
  const KEY *b= pointer_cast<const KEY*>(b_arg);
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
    thd           Thread handle
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

static bool check_duplicates_in_interval(THD *thd,
                                         const char *set_or_name,
                                         const char *name, TYPELIB *typelib,
                                         const CHARSET_INFO *cs,
                                         uint *dup_val_count)
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
      ErrConvString err(*cur_value, *cur_length, cs);
      if (thd->is_strict_mode())
      {
        my_error(ER_DUPLICATED_VALUE_IN_TYPE, MYF(0),
                 name, err.ptr(), set_or_name);
        return 1;
      }
      push_warning_printf(thd,Sql_condition::SL_NOTE,
                          ER_DUPLICATED_VALUE_IN_TYPE,
                          ER_THD(thd, ER_DUPLICATED_VALUE_IN_TYPE),
                          name, err.ptr(), set_or_name);
      (*dup_val_count)++;
    }
  }
  return 0;
}


/**
  Check TYPELIB (set or enum) length of individual values.

  @param col_name   Name of column to be checked (for error msg)
  @param cs         Charset + collation pair of the interval
  @param interval   List of values for the column

  @return           true if max length is exceeded (error reported),
                    false otherwise
*/

static bool check_interval_length(const char *col_name,
                                  const CHARSET_INFO *cs,
                                  TYPELIB *interval)
{
  const char **pos;
  uint *len= interval->type_lengths;
  for (pos= interval->type_names; *pos; pos++, len++)
  {
    if (cs->cset->numchars(cs, *pos, *pos + *len) > MAX_INTERVAL_VALUE_LENGTH)
    {
      my_error(ER_TOO_LONG_SET_ENUM_VALUE, MYF(0), col_name);
      return true;
    }
  }
  return false;
}


/**
  Prepare a create_table instance for packing

  @param thd                    Thread handle
  @param [in,out] sql_field     field to prepare for packing
  @param table_flags            table flags

  @return true if error, false if ok
*/

bool prepare_pack_create_field(THD *thd, Create_field *sql_field,
                               longlong table_flags)
{
  unsigned int dup_val_count;
  DBUG_ENTER("prepare_pack_create_field");
  DBUG_ASSERT(sql_field->charset);

  sql_field->maybe_null= true;
  sql_field->is_zerofill= false;
  sql_field->is_unsigned= false;

  switch (sql_field->sql_type) {
  case MYSQL_TYPE_GEOMETRY:
    if (!(table_flags & HA_CAN_GEOMETRY))
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "GEOMETRY");
      DBUG_RETURN(true);
    }
    /* fall-through */
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_JSON:
    sql_field->length= 8;			// Unireg field length
    DBUG_ASSERT(sql_field->auto_flags == Field::NONE);
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
        my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
                 static_cast<ulong>(MAX_FIELD_CHARLENGTH));
        DBUG_RETURN(true);
      }
    }
    break;
  case MYSQL_TYPE_STRING:
    break;
  case MYSQL_TYPE_ENUM:
    DBUG_ASSERT(sql_field->auto_flags == Field::NONE);
    if (check_duplicates_in_interval(thd, "ENUM",sql_field->field_name,
                                     sql_field->interval,
                                     sql_field->charset, &dup_val_count))
      DBUG_RETURN(true);
    if (check_interval_length(sql_field->field_name,
                              sql_field->charset,
                              sql_field->interval))
      DBUG_RETURN(true);
    if (sql_field->interval->count > MAX_ENUM_VALUES)
    {
      my_error(ER_TOO_BIG_ENUM, MYF(0), sql_field->field_name);
      DBUG_RETURN(true);
    }
    break;
  case MYSQL_TYPE_SET:
    DBUG_ASSERT(sql_field->auto_flags == Field::NONE);
    if (check_duplicates_in_interval(thd, "SET", sql_field->field_name,
                                     sql_field->interval,
                                     sql_field->charset, &dup_val_count))
      DBUG_RETURN(true);
    if (check_interval_length(sql_field->field_name,
                              sql_field->charset,
                              sql_field->interval))
      DBUG_RETURN(true);
    /* Check that count of unique members is not more then 64 */
    if (sql_field->interval->count - dup_val_count > sizeof(longlong) * 8)
    {
       my_error(ER_TOO_BIG_SET, MYF(0), sql_field->field_name);
       DBUG_RETURN(true);
    }
    break;
  case MYSQL_TYPE_DATE:			// Rest of string types
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIME2:
  case MYSQL_TYPE_DATETIME2:
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_BIT:
    break;
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_TIMESTAMP2:
  case MYSQL_TYPE_NEWDECIMAL:
  default:
    if (sql_field->flags & ZEROFILL_FLAG)
      sql_field->is_zerofill= true;
    if (sql_field->flags & UNSIGNED_FLAG)
      sql_field->is_unsigned= true;
    break;
  }

  if (sql_field->flags & NOT_NULL_FLAG)
    sql_field->maybe_null= false;
  sql_field->pack_length_override= 0;

  DBUG_RETURN(false);
}


static TYPELIB *create_typelib(MEM_ROOT *mem_root, Create_field *field_def)
{
  if (!field_def->interval_list.elements)
    return NULL;

  TYPELIB *result= reinterpret_cast<TYPELIB*>(alloc_root(mem_root,
                                                         sizeof(TYPELIB)));
  if (!result)
    return NULL;

  result->count= field_def->interval_list.elements;
  result->name= "";

  // Allocate type_names and type_lengths as one block.
  size_t nbytes= (sizeof(char*) + sizeof(uint)) * (result->count + 1);
  if (!(result->type_names=
          reinterpret_cast<const char **>(alloc_root(mem_root, nbytes))))
    return NULL;

  result->type_lengths=
    reinterpret_cast<uint*>(result->type_names + result->count + 1);

  List_iterator<String> it(field_def->interval_list);
  for (uint i= 0; i < result->count; i++)
  {
    size_t dummy;
    String *tmp= it++;

    if (String::needs_conversion(tmp->length(), tmp->charset(),
                                 field_def->charset, &dummy))
    {
      uint cnv_errs;
      String conv;
      conv.copy(tmp->ptr(), tmp->length(), tmp->charset(),
                field_def->charset, &cnv_errs);

      result->type_names[i]= strmake_root(mem_root, conv.ptr(), conv.length());
      result->type_lengths[i]= conv.length();
    }
    else
    {
      result->type_names[i]= tmp->ptr();
      result->type_lengths[i]= tmp->length();
    }

    // Strip trailing spaces.
    size_t length= field_def->charset->cset->lengthsp(field_def->charset,
                                                      result->type_names[i],
                                                      result->type_lengths[i]);
    result->type_lengths[i]= length;
    (const_cast<char*>(result->type_names[i]))[length]= '\0';
  }
  result->type_names[result->count]= NULL;  // End marker (char*)
  result->type_lengths[result->count]= 0;   // End marker (uint)

  field_def->interval_list.empty(); // Don't need interval_list anymore
  return result;
}


/**
  Prepare an instance of Create_field for field creation
  (fill all necessary attributes). Only used for stored programs.

  @param[in]  thd          Thread handle
  @param[out] field_def    An instance of initialized create_field

  @return Error status.
*/

bool prepare_sp_create_field(THD *thd,
                             Create_field *field_def)
{
  if (field_def->sql_type == MYSQL_TYPE_SET)
  {
    if (prepare_set_field(thd, field_def))
      return true;
  }
  else if (field_def->sql_type == MYSQL_TYPE_ENUM)
  {
    if (prepare_enum_field(thd, field_def))
      return true;
  }
  else if (field_def->sql_type == MYSQL_TYPE_BIT)
    field_def->treat_bit_as_char= true;

  field_def->create_length_to_internal_length();
  if (prepare_blob_field(thd, field_def))
    return true;

  return prepare_pack_create_field(thd, field_def, HA_CAN_GEOMETRY);
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

const CHARSET_INFO* get_sql_field_charset(const Create_field *sql_field,
                                          const HA_CREATE_INFO *create_info)
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
        column_definition->sql_type == MYSQL_TYPE_TIMESTAMP2)       //  ms TIMESTAMP
    {
      if ((column_definition->flags & NOT_NULL_FLAG) != 0 && // NOT NULL,
          column_definition->def == NULL &&            // no constant default,
          column_definition->gcol_info == NULL &&      // not a generated column
          column_definition->auto_flags == Field::NONE) // no function default
      {
        DBUG_PRINT("info", ("First TIMESTAMP column '%s' was promoted to "
                            "DEFAULT CURRENT_TIMESTAMP ON UPDATE "
                            "CURRENT_TIMESTAMP",
                            column_definition->field_name
                            ));
        column_definition->auto_flags= Field::DEFAULT_NOW|Field::ON_UPDATE_NOW;
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
  @param key_info           Array with all keys for the table.
  @param key_count          Number of keys in the table.
  @param alter_info         Alter_info structure describing ALTER TABLE.

  @note Unlike has_index_def_changed() and similar code in
        mysql_compare_tables() this function compares KEY objects for the same
        table/created by the same mysql_prepare_create(). Hence difference in
        field number comparison. We also differentiate UNIQUE and PRIMARY keys.

  @retval false           Ok.
  @retval true            Error.
*/
static bool check_duplicate_key(THD *thd, const char *error_schema_name,
                                const char *error_table_name,
                                const KEY *key, const KEY *key_info,
                                uint key_count, Alter_info *alter_info)
{
  const KEY *k;
  const KEY *k_end= key_info + key_count;

  /* This function should not be called for PRIMARY or generated keys. */
  DBUG_ASSERT(key->name != primary_key_name &&
              !(key->flags & HA_GENERATED_KEY));

  for (k= key_info; k != k_end; k++)
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

    /*
      Treat key as not duplicate if:
      - it is generated (as it will be automagically removed if duplicate later)
      - has different type (Instead of differentiating between PRIMARY and
        UNIQUE keys we simply skip check for PRIMARY keys. The fact that we
        have only one primary key for the table is checked elsewhere.)
      - has different algorithm
      - has different number of key parts
    */
    if ((k->flags & HA_GENERATED_KEY)  ||
        ((key->flags & HA_KEYFLAG_MASK) != (k->flags & HA_KEYFLAG_MASK)) ||
        (k->name == primary_key_name) ||
        (key->algorithm != k->algorithm) ||
        (key->user_defined_key_parts != k->user_defined_key_parts))
    {
      // Keys are different.
      continue;
    }

    /*
      Keys 'key' and 'k' might be identical.
      Check that the keys have identical columns in the same order.
    */
    const KEY_PART_INFO *key_part;
    const KEY_PART_INFO *key_part_end= key->key_part +
                                       key->user_defined_key_parts;
    const KEY_PART_INFO *k_part;
    bool all_columns_are_identical= true;

    for (key_part= key->key_part, k_part= k->key_part;
         key_part < key_part_end;
         key_part++, k_part++)
    {
      /*
        Key definition is different if we are using a different field,
        if the used key part length is different or key parts has different
        direction. Note since both KEY objects come from
        mysql_prepare_create_table() we can compare field numbers directly.
      */
      if ((key_part->length != k_part->length) ||
          (key_part->fieldnr != k_part->fieldnr) ||
          (key_part->key_part_flag != k_part->key_part_flag))
      {
        all_columns_are_identical= false;
        break;
      }
    }

    // Report a warning if we have two identical keys.

    if (all_columns_are_identical)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_DUP_INDEX, ER_THD(thd, ER_DUP_INDEX),
                          key->name, error_schema_name, error_table_name);
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


/**
  Helper function which allows to detect column types for which we historically
  used key packing (optimization implemented only by MyISAM) under erroneous
  assumption that they have BLOB type.
*/
static bool is_phony_blob(enum_field_types sql_type, uint decimals)
{
  const uint FIELDFLAG_BLOB= 1024;
  const uint FIELDFLAG_DEC_SHIFT= 8;

  return (sql_type == MYSQL_TYPE_NEWDECIMAL ||
          sql_type == MYSQL_TYPE_DOUBLE ||
          sql_type == MYSQL_TYPE_DECIMAL) &&
          (((decimals << FIELDFLAG_DEC_SHIFT) & FIELDFLAG_BLOB) != 0);
}


static bool prepare_set_field(THD *thd, Create_field *sql_field)
{
  DBUG_ENTER("prepare_set_field");
  DBUG_ASSERT(sql_field->sql_type == MYSQL_TYPE_SET);

  /*
    Create typelib from interval_list, and if necessary
    convert strings from client character set to the
    column character set.
  */
  if (!sql_field->interval)
  {
    /*
      Create the typelib in runtime memory - we will free the
      occupied memory at the same time when we free this
      sql_field -- at the end of execution.
    */
    sql_field->interval= create_typelib(thd->mem_root, sql_field);
  }

  // Comma is an invalid character for SET names
  char comma_buf[4]; /* 4 bytes for utf32 */
  int comma_length=
    sql_field->charset->cset->wc_mb(sql_field->charset, ',',
                                    reinterpret_cast<uchar*>(comma_buf),
                                    reinterpret_cast<uchar*>(comma_buf) +
                                    sizeof(comma_buf));
  DBUG_ASSERT(comma_length > 0);

  for (uint i= 0; i < sql_field->interval->count; i++)
  {
    if (sql_field->charset->coll->strstr(sql_field->charset,
                                         sql_field->interval->type_names[i],
                                         sql_field->interval->type_lengths[i],
                                         comma_buf, comma_length, NULL, 0))
    {
      ErrConvString err(sql_field->interval->type_names[i],
                        sql_field->interval->type_lengths[i],
                        sql_field->charset);
      my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", err.ptr());
      DBUG_RETURN(true);
    }
  }

  if (sql_field->def != NULL)
  {
    char *not_used;
    uint not_used2;
    bool not_found= false;
    String str;
    String *def= sql_field->def->val_str(&str);
    if (def == NULL) /* SQL "NULL" maps to NULL */
    {
      if ((sql_field->flags & NOT_NULL_FLAG) != 0)
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(true);
      }

      /* else, NULL is an allowed value */
      (void) find_set(sql_field->interval, NULL, 0,
                      sql_field->charset, &not_used, &not_used2, &not_found);
    }
    else /* not NULL */
    {
      (void) find_set(sql_field->interval, def->ptr(), def->length(),
                      sql_field->charset, &not_used, &not_used2, &not_found);
    }

    if (not_found)
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
      DBUG_RETURN(true);
    }
  }

  sql_field->length= 0;
  const char **pos;
  uint *len;
  for (pos= sql_field->interval->type_names,
         len= sql_field->interval->type_lengths;
       *pos ; pos++, len++)
  {
    // SET uses tot_length
    sql_field->length+= sql_field->charset->cset->numchars(sql_field->charset,
                                                           *pos, *pos + *len);
  }
  sql_field->length+= (sql_field->interval->count - 1);
  sql_field->length= min<size_t>(sql_field->length, MAX_FIELD_WIDTH - 1);

  DBUG_RETURN(false);
}


static bool prepare_enum_field(THD *thd, Create_field *sql_field)
{
  DBUG_ENTER("prepare_enum_field");
  DBUG_ASSERT(sql_field->sql_type == MYSQL_TYPE_ENUM);

  /*
    Create typelib from interval_list, and if necessary
    convert strings from client character set to the
    column character set.
  */
  if (!sql_field->interval)
  {
    /*
      Create the typelib in runtime memory - we will free the
      occupied memory at the same time when we free this
      sql_field -- at the end of execution.
    */
    sql_field->interval= create_typelib(thd->mem_root, sql_field);
  }

  if (sql_field->def != NULL)
  {
    String str;
    String *def= sql_field->def->val_str(&str);
    if (def == NULL) /* SQL "NULL" maps to NULL */
    {
      if ((sql_field->flags & NOT_NULL_FLAG) != 0)
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(true);
      }

      /* else, the defaults yield the correct length for NULLs. */
    }
    else /* not NULL */
    {
      def->length(sql_field->charset->cset->lengthsp(sql_field->charset,
                                                     def->ptr(), def->length()));
      if (find_type2(sql_field->interval, def->ptr(),
                     def->length(), sql_field->charset) == 0) /* not found */
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(true);
      }
    }
  }

  sql_field->length= 0;
  const char **pos;
  uint *len;
  for (pos= sql_field->interval->type_names,
         len= sql_field->interval->type_lengths;
       *pos ; pos++, len++)
  {
    // ENUM uses max_length
    sql_field->length= max(sql_field->length,
                           sql_field->charset->cset->numchars(sql_field->charset,
                                                              *pos, *pos + *len));
  }
  sql_field->length= min<size_t>(sql_field->length, MAX_FIELD_WIDTH - 1);

  DBUG_RETURN(false);
}


bool prepare_create_field(THD *thd, HA_CREATE_INFO *create_info,
                          List<Create_field> *create_list,
                          int *select_field_pos, handler *file,
                          Create_field *sql_field, int field_no)
{
  DBUG_ENTER("prepare_create_field");
  DBUG_ASSERT(create_list);
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
      DBUG_RETURN(true);
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
      DBUG_RETURN(true);
    }
  }

  if (sql_field->sql_type == MYSQL_TYPE_SET)
  {
    if (prepare_set_field(thd, sql_field))
      DBUG_RETURN(true);
  }
  else if (sql_field->sql_type == MYSQL_TYPE_ENUM)
  {
    if (prepare_enum_field(thd, sql_field))
      DBUG_RETURN(true);
  }
  else if (sql_field->sql_type == MYSQL_TYPE_BIT)
  {
    if (file->ha_table_flags() & HA_CAN_BIT_FIELD)
    {
      create_info->null_bits+= sql_field->length & 7;
      sql_field->treat_bit_as_char= false;
    }
    else
      sql_field->treat_bit_as_char= true;
  }

  sql_field->create_length_to_internal_length();
  if (prepare_blob_field(thd, sql_field))
    DBUG_RETURN(true);

  if (!(sql_field->flags & NOT_NULL_FLAG))
    create_info->null_bits++;

  if (check_column_name(sql_field->field_name))
  {
    my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
    DBUG_RETURN(true);
  }

  if (validate_comment_length(thd,
                              sql_field->comment.str,
                              &sql_field->comment.length,
                              COLUMN_COMMENT_MAXLEN,
                              ER_TOO_LONG_FIELD_COMMENT,
                              sql_field->field_name))
      DBUG_RETURN(true);

  /* Check if we have used the same field name before */
  Create_field *dup_field;
  List_iterator<Create_field> it(*create_list);
  for (int dup_no= 0; (dup_field= it++) != sql_field; dup_no++)
  {
    if (my_strcasecmp(system_charset_info,
                      sql_field->field_name,
                      dup_field->field_name) == 0)
    {
      /*
        If this was a CREATE ... SELECT statement, accept a field
        redefinition if we are changing a field in the SELECT part
      */
      if (field_no < (*select_field_pos) || dup_no >= (*select_field_pos))
      {
        my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
        DBUG_RETURN(true);
      }
      else
      {
        /* Field redefined */

        /*
          If we are replacing a BIT field, revert the increment
          of null_bits that was done above.
        */
        if (sql_field->sql_type == MYSQL_TYPE_BIT &&
            file->ha_table_flags() & HA_CAN_BIT_FIELD)
          create_info->null_bits-= sql_field->length & 7;

        sql_field->def=         dup_field->def;
        sql_field->sql_type=    dup_field->sql_type;

        /*
          If we are replacing a field with a BIT field, we need
          to initialize treat_bit_as_char. Note that we do not need to
          increment null_bits here as this dup_field
          has already been processed.
        */
        if (sql_field->sql_type == MYSQL_TYPE_BIT)
        {
          sql_field->treat_bit_as_char=
            !(file->ha_table_flags() & HA_CAN_BIT_FIELD);
        }

        sql_field->charset=             (dup_field->charset ?
                                         dup_field->charset :
                                         create_info->default_table_charset);
        sql_field->length=              dup_field->char_length;
        sql_field->pack_length=         dup_field->pack_length;
        sql_field->key_length=          dup_field->key_length;
        sql_field->decimals=            dup_field->decimals;
        sql_field->auto_flags=          dup_field->auto_flags;
        /*
           We're making one field from two, the result field will have
           dup_field->flags as flags. If we've incremented null_bits
           because of sql_field->flags, decrement it back.
        */
        if (!(sql_field->flags & NOT_NULL_FLAG))
          create_info->null_bits--;
        sql_field->flags=               dup_field->flags;
        sql_field->create_length_to_internal_length();
        sql_field->interval=            dup_field->interval;
        sql_field->gcol_info=           dup_field->gcol_info;
        sql_field->stored_in_db=        dup_field->stored_in_db;
        it.remove();                    // Remove first (create) definition
        (*select_field_pos)--;
        break;
      }
    }
  }

  /* Don't pack rows in old tables if the user has requested this */
  if ((sql_field->flags & BLOB_FLAG) ||
      (sql_field->sql_type == MYSQL_TYPE_VARCHAR &&
       create_info->row_type != ROW_TYPE_FIXED))
    create_info->table_options|= HA_OPTION_PACK_RECORD;

  if (prepare_pack_create_field(thd, sql_field, file->ha_table_flags()))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


static void calculate_field_offsets(List<Create_field> *create_list)
{
  DBUG_ASSERT(create_list);
  List_iterator<Create_field> it(*create_list);
  size_t record_offset= 0;
  bool has_vgc= false;
  Create_field *sql_field;
  while ((sql_field= it++))
  {
    sql_field->offset= record_offset;
    /*
      For now skip fields that are not physically stored in the database
      (generated fields) and update their offset later (see the next loop).
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
    while ((sql_field= it++))
    {
      if (!sql_field->stored_in_db)
      {
        sql_field->offset= record_offset;
        record_offset+= sql_field->pack_length;
      }
    }
  }
}


/**
   Count keys and key segments. Note that FKs are ignored.
   Also mark redundant keys to be ignored.

   @param[in,out] key_list  List of keys to count and possibly mark as ignored.
   @param[out] key_count    Returned number of keys counted (excluding FK).
   @param[out] key_parts    Returned number of key segments (excluding FK).
   @param[out] fk_key_count Returned number of foreign keys.
   @param[in,out] redundant_keys  Array where keys to be ignored will be marked.
   @param[in]  is_ha_has_desc_index Whether storage supports desc indexes
*/

static bool count_keys(const Prealloced_array<const Key_spec*, 1> &key_list,
                       uint *key_count, uint *key_parts,
                       uint *fk_key_count,
                       Mem_root_array<bool> *redundant_keys,
                       bool is_ha_has_desc_index)
{
  *key_count= 0;
  *key_parts= 0;

  for (size_t key_counter= 0; key_counter < key_list.size(); key_counter++)
  {
    const Key_spec *key= key_list[key_counter];

    for (size_t key2_counter= 0;
         key2_counter < key_list.size() && key_list[key2_counter] != key;
         key2_counter++)
    {
      const Key_spec *key2= key_list[key2_counter];
      /*
        foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
        'generated', and a generated key is a prefix of the other key.
        Then we do not need the generated shorter key.

        KEYTYPE_SPATIAL and KEYTYPE_FULLTEXT cannot be used as
        supporting keys for foreign key constraints even if the
        generated key is prefix of such a key.
      */
      if ((key2->type != KEYTYPE_FOREIGN &&
           key->type != KEYTYPE_FOREIGN &&
           key2->type != KEYTYPE_SPATIAL &&
           key2->type != KEYTYPE_FULLTEXT &&
           !redundant_keys->at(key2_counter) &&
           !foreign_key_prefix(key, key2)))
      {
        /* TODO: issue warning message */
        /* mark that the generated key should be ignored */
        if (!key2->generated ||
            (key->generated &&
             key->columns.size() < key2->columns.size()))
          (*redundant_keys)[key_counter]= true;
        else
        {
          (*redundant_keys)[key2_counter]= true;
          (*key_parts)-= key2->columns.size();
          (*key_count)--;
        }
        break;
      }
    }

    if (!redundant_keys->at(key_counter))
    {
      if (key->type == KEYTYPE_FOREIGN)
        (*fk_key_count)++;
      else
      {
        (*key_count)++;
        (*key_parts)+= key->columns.size();
        for (uint i= 0; i < key->columns.size(); i++)
        {
          const Key_part_spec *kp= key->columns[i];
          if (!kp->is_ascending && !is_ha_has_desc_index)
          {
            my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "descending indexes");
            return true;
          }
        }
      }
    }
  }
  return false;
}


static bool prepare_key_column(THD *thd, HA_CREATE_INFO *create_info,
                               List<Create_field> *create_list,
                               const Key_spec *key,
                               const Key_part_spec *column,
                               const size_t column_nr, KEY *key_info,
                               KEY_PART_INFO *key_part_info,
                               const handler *file, int *auto_increment,
                               const CHARSET_INFO **ft_key_charset)
{
  DBUG_ENTER("prepare_key_column");

  /*
    Find the matching table column.
  */
  uint field= 0;
  Create_field *sql_field;
  DBUG_ASSERT(create_list);
  List_iterator<Create_field> it(*create_list);
  while ((sql_field= it++) && my_strcasecmp(system_charset_info,
                                            column->field_name.str,
                                            sql_field->field_name))
    field++;
  if (!sql_field)
  {
    my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
    DBUG_RETURN(true);
  }

  /*
    Virtual generated column checks.
  */
  if (sql_field->is_virtual_gcol())
  {
    const char *errmsg= NULL;
    if (key->type == KEYTYPE_FULLTEXT)
      errmsg= "Fulltext index on virtual generated column";
    else if (key->type == KEYTYPE_SPATIAL)
      errmsg= "Spatial index on virtual generated column";
    else if (key->type == KEYTYPE_PRIMARY)
      errmsg= "Defining a virtual generated column as primary key";
    if (errmsg)
    {
      my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN, MYF(0), errmsg);
      DBUG_RETURN(true);
    }
    /* Check if the storage engine supports indexes on virtual columns. */
    if (!(file->ha_table_flags() & HA_CAN_INDEX_VIRTUAL_GENERATED_COLUMN))
    {
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               ha_resolve_storage_engine_name(file->ht),
               "Index on virtual generated column");
      DBUG_RETURN(true);
    }
    key_info->flags|= HA_VIRTUAL_GEN_KEY;
  }

  // JSON columns cannot be used as keys.
  if (sql_field->sql_type == MYSQL_TYPE_JSON)
  {
    my_error(ER_JSON_USED_AS_KEY, MYF(0), column->field_name.str);
    DBUG_RETURN(true);
  }

  if (sql_field->auto_flags & Field::NEXT_NUMBER)
  {
    if (column_nr == 0 || (file->ha_table_flags() & HA_AUTO_PART_KEY))
      (*auto_increment)--;			// Field is used
  }

  /*
    Check for duplicate columns.
  */
  for (const Key_part_spec *dup_column : key->columns)
  {
    if (dup_column == column)
      break;
    if (!my_strcasecmp(system_charset_info,
                       column->field_name.str, dup_column->field_name.str))
    {
      my_error(ER_DUP_FIELDNAME, MYF(0), column->field_name.str);
      DBUG_RETURN(true);
    }
  }

  uint column_length;
  if (key->type == KEYTYPE_FULLTEXT)
  {
    if ((sql_field->sql_type != MYSQL_TYPE_STRING &&
         sql_field->sql_type != MYSQL_TYPE_VARCHAR &&
         !is_blob(sql_field->sql_type)) ||
        sql_field->charset == &my_charset_bin ||
        sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
        (*ft_key_charset && sql_field->charset != *ft_key_charset))
    {
      my_error(ER_BAD_FT_COLUMN, MYF(0), column->field_name.str);
      DBUG_RETURN(true);
    }
    *ft_key_charset= sql_field->charset;
    /*
      for fulltext keys keyseg length is 1 for blobs (it's ignored in ft
      code anyway, and 0 (set to column width later) for char's. it has
      to be correct col width for char's, as char data are not prefixed
      with length (unlike blobs, where ft code takes data length from a
      data prefix, ignoring column->length).
    */
    column_length= MY_TEST(is_blob(sql_field->sql_type));
  }
  else
  {
    column_length= column->length * sql_field->charset->mbmaxlen;

    if (key->type == KEYTYPE_SPATIAL)
    {
      if (column_length)
      {
        my_error(ER_WRONG_SUB_KEY, MYF(0));
        DBUG_RETURN(true);
      }
      if (sql_field->sql_type != MYSQL_TYPE_GEOMETRY)
      {
        my_error(ER_SPATIAL_MUST_HAVE_GEOM_COL, MYF(0));
        DBUG_RETURN(true);
      }
      /*
        4 is: (Xmin,Xmax,Ymin,Ymax), this is for 2D case
        Lately we'll extend this code to support more dimensions
      */
      column_length= 4*sizeof(double);
    }

    if (is_blob(sql_field->sql_type) ||
        (sql_field->sql_type == MYSQL_TYPE_GEOMETRY &&
         key->type != KEYTYPE_SPATIAL))
    {
      if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
      {
        my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name.str);
        DBUG_RETURN(true);
      }
      if (sql_field->sql_type == MYSQL_TYPE_GEOMETRY &&
          sql_field->geom_type == Field::GEOM_POINT)
        column_length= MAX_LEN_GEOM_POINT_FIELD;
      if (!column_length)
      {
        my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name.str);
        DBUG_RETURN(true);
      }
    }

    if (key->type == KEYTYPE_PRIMARY)
    {
      /*
        Set NO_DEFAULT_VALUE_FLAG for the PRIMARY KEY column if default
        values is not explicitly provided for the column in CREATE TABLE
        statement and it is not an AUTO_INCREMENT field.

        Default values for TIMESTAMP/DATETIME needs special handling as:

        a) If default is explicitly specified (lets say this as case 1) :
             DEFAULT CURRENT_TIMESTAMP
             DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
           MySQL does not set sql_field->def flag , but sets
           Field::DEFAULT_NOW in Create_info::auto_flags.
           This flags are also set during timestamp column promotion (case2)

           When explicit_defaults_for_timestamp is not set, the behavior
           expected in both case1 and case2 is to retain the defaults even
           when the column participates in PRIMARY KEY. When
           explicit_defaults_for_timestamp is set, the promotion logic
           is disabled and the above mentioned flag is not used implicitly.

        b) If explicit_defaults_for_timestamp variable is not set:
           Default value assigned due to first timestamp column promotion is
           retained.
           Default constant value assigned due to implicit promotion of second
           timestamp column is removed.
      */
      if (!sql_field->def &&
          !(sql_field->flags & AUTO_INCREMENT_FLAG) &&
          !(real_type_with_now_as_default(sql_field->sql_type) &&
            (sql_field->auto_flags & Field::DEFAULT_NOW)))
      {
        sql_field->flags|= NO_DEFAULT_VALUE_FLAG;
      }
      /*
        Emitting error when field is a part of primary key and is
        explicitly requested to be NULL by the user.
      */
      if ((sql_field->flags & EXPLICIT_NULL_FLAG))
      {
        my_error(ER_PRIMARY_CANT_HAVE_NULL, MYF(0));
        DBUG_RETURN(true);
      }
    }

    if (!(sql_field->flags & NOT_NULL_FLAG))
    {
      if (key->type == KEYTYPE_PRIMARY)
      {
        /* Implicitly set primary key fields to NOT NULL for ISO conf. */
        sql_field->flags|= NOT_NULL_FLAG;
        sql_field->maybe_null= false;
        create_info->null_bits--;
      }
      else
      {
        key_info->flags|= HA_NULL_PART_KEY;
        if (!(file->ha_table_flags() & HA_NULL_IN_KEY))
        {
          my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name.str);
          DBUG_RETURN(true);
        }
        if (key->type == KEYTYPE_SPATIAL)
        {
          my_error(ER_SPATIAL_CANT_HAVE_NULL, MYF(0));
          DBUG_RETURN(true);
        }
      }
    }
  } // key->type != KEYTYPE_FULLTEXT

  key_part_info->fieldnr= field;
  key_part_info->offset=  static_cast<uint16>(sql_field->offset);
  key_part_info->key_part_flag|= column->is_ascending ? 0 : HA_REVERSE_SORT;

  size_t key_part_length= sql_field->key_length;

  if (column_length)
  {
    if (is_blob(sql_field->sql_type))
    {
      key_part_length= column_length;
      /*
        There is a possibility that the given prefix length is less
        than the engine max key part length, but still greater
        than the BLOB field max size. We handle this case
        using the max_field_size variable below.
      */
      size_t max_field_size= blob_length_by_type(sql_field->sql_type);
      if (key_part_length > max_field_size ||
          key_part_length > file->max_key_length() ||
          key_part_length > file->max_key_part_length())
      {
        // Given prefix length is too large, adjust it.
        key_part_length= min(file->max_key_length(), file->max_key_part_length());
        if (max_field_size)
          key_part_length= min(key_part_length, max_field_size);
        if (key->type == KEYTYPE_MULTIPLE)
        {
          /* not a critical problem */
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_TOO_LONG_KEY, ER_THD(thd, ER_TOO_LONG_KEY),
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
          DBUG_RETURN(true);
        }
      }
    } // is_blob
    // Catch invalid use of partial keys
    else if (sql_field->sql_type != MYSQL_TYPE_GEOMETRY &&
             // is the key partial?
             column_length != key_part_length &&
             // is prefix length bigger than field length?
             (column_length > key_part_length ||
              // can the field have a partial key?
              !Field::type_can_have_key_part (sql_field->sql_type) ||
              // does the storage engine allow prefixed search?
              ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
               // and is this a 'unique' key?
               (key_info->flags & HA_NOSAME))))
    {
      my_error(ER_WRONG_SUB_KEY, MYF(0));
      DBUG_RETURN(TRUE);
    }
    else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
      key_part_length= column_length;
  } // column_length
  else if (key_part_length == 0)
  {
    my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name.str);
    DBUG_RETURN(true);
  }
  if (key_part_length > file->max_key_part_length() &&
      key->type != KEYTYPE_FULLTEXT)
  {
    key_part_length= file->max_key_part_length();
    if (key->type == KEYTYPE_MULTIPLE)
    {
      /* not a critical problem */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_TOO_LONG_KEY, ER_THD(thd, ER_TOO_LONG_KEY),
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
      DBUG_RETURN(true);
    }
  }
  key_part_info->length= static_cast<uint16>(key_part_length);

  /*
    Use packed keys for long strings on the first column

    Due to incorrect usage of sql_field->pack_flag & FIELDFLAG_BLOB check
    we have used packing for some columns which are not strings or BLOBs
    (see also is_phony_blob()). Since changing this would mean breaking
    binary compatibility for MyISAM tables with indexes on such columns
    we mimic this buggy behavior here.
  */
  if (!((create_info->table_options & HA_OPTION_NO_PACK_KEYS)) &&
      (key_part_length >= KEY_DEFAULT_PACK_LENGTH &&
       (sql_field->sql_type == MYSQL_TYPE_STRING ||
        sql_field->sql_type == MYSQL_TYPE_VARCHAR ||
        is_blob(sql_field->sql_type) ||
        is_phony_blob(sql_field->sql_type, sql_field->decimals))))
  {
    if ((column_nr == 0 &&
         (is_blob(sql_field->sql_type) ||
          is_phony_blob(sql_field->sql_type, sql_field->decimals))) ||
        sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      key_info->flags|= HA_BINARY_PACK_KEY;
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
    key_part_info->key_part_flag|= HA_PART_KEY_SEG;
  }

  key_info->key_length+= key_part_length;
  DBUG_RETURN(false);
}


const char* find_fk_supporting_index(Alter_info *alter_info,
                                     const KEY *key_info_buffer,
                                     const uint key_count,
                                     const FOREIGN_KEY *fk)
{
  for (const KEY *key= key_info_buffer;
       key < key_info_buffer + key_count; key++)
  {
    // The index may have more elements, but must start with the same
    // elements as the FK.
    if (fk->key_parts > key->actual_key_parts)
      continue;

    if ((key->flags & HA_FULLTEXT) ||
        (key->flags & HA_SPATIAL))
      continue;

    if (key->flags & HA_KEY_HAS_PART_KEY_SEG)
      continue;  // Prefix indexes cannot be supporting indexes.

    bool match= true;
    for (uint i= 0; i < fk->key_parts && match; i++)
    {
      const Create_field *col= get_field_by_index(alter_info,
                                                  key->key_part[i].fieldnr);
      if (col->is_virtual_gcol())
        match= false;
      else if (my_strcasecmp(system_charset_info,
                             col->field_name,
                             fk->key_part[i].str) != 0)
        match= false;
    }
    if (match)
      return key->name;
  }
  return nullptr;
}


/**
  Restore the foreign key name for foreign keys which have been
  temporarily renamed during ALTER TABLE. This is needed to avoid
  problems with duplicate foreign key names while we have two
  definitions of the same table.

  @param thd            Thread handle.
  @param schema_name    Database name.
  @param table_name     Table name.
  @param fk_keyinfo     FK keyinfo containing orignal FK names.
  @param fk_keys        Number of FKs.
  @param commit_dd_changes  Indicates whether change should be committed.

  @note In case when commit_dd_changes is false, the caller must rollback
        both statement and transaction on failure, before any further
        accesses to DD. This is because such a failure might be caused by
        a deadlock, which requires rollback before any other operations on
        SE (including reads using attachable transactions) can be done.
        If case when commit_dd_changes is true this function will handle
        transaction rollback itself.

  @retval false on success
  @retval true on failure
 */

static bool restore_foreign_key_names(THD *thd,
                                      const dd::String_type &schema_name,
                                      const dd::String_type &table_name,
                                      const FOREIGN_KEY *fk_keyinfo,
                                      uint fk_keys,
                                      bool commit_dd_changes)
{
  DBUG_ENTER("restore_foreign_key_names");

  dd::Table *table_def= nullptr;
  if (thd->dd_client()->acquire_for_modification(schema_name, table_name,
                                                 &table_def))
    DBUG_RETURN(true);

  // Restore the original name for pre-existing foreign keys
  // that had their name temporarily altered during ALTER TABLE
  // to avoid violating the unique constraint on FK name.
  bool updated= false;
  for (dd::Foreign_key *fk : *table_def->foreign_keys())
  {
    for (const FOREIGN_KEY *fk_key= fk_keyinfo;
         fk_key != fk_keyinfo + fk_keys;
         ++fk_key)
    {
      if (fk_key->orig_name != nullptr &&
          fk->name().length() == strlen(fk_key->name) &&
          (strncmp(fk_key->name, fk->name().c_str(), fk->name().length()) == 0))
      {
        fk->set_name(fk_key->orig_name);
        updated= true;
        break;
      }
    }
  }

  if (!updated)
    DBUG_RETURN(false);

  Disable_gtid_state_update_guard disabler(thd);

  if (thd->dd_client()->update(table_def))
  {
    if (commit_dd_changes)
    {
      trans_rollback_stmt(thd);
      // Full rollback in case we have THD::transaction_rollback_request.
      trans_rollback(thd);
    }
    DBUG_RETURN(true);
  }

  if (commit_dd_changes)

  {
    if (trans_commit_stmt(thd) || trans_commit(thd))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/**
  Generate a foreign key name. Foreign key names have to be unique
  for a given schema. This function is used when the user has not
  specified neither constraint name nor foreign key name.

  For now, we have to replicate the name generated by InnoDB.
  As long as InnoDB also stores FK metadata, we have to agree on
  name in order for DROP FOREIGN KEY to work properly.

  The format is (table_name)_ibfk_(counter). The counter is 1-based
  and per table. The number chosen for the counter is 1 higher than
  the highest number currently in use.

  @todo Implement new naming scheme (or move responsibility of
        naming to the SE layer).

  @param table_name          Table name.
  @param fk_info_buffer      Array of FKs.
  @param fk_number           Index to FK to be added.

  @retval  Generated name
*/

static const char* generate_fk_name(const char *table_name,
                                    FOREIGN_KEY **fk_info_buffer,
                                    uint fk_number)
{
  // InnoDB name generation (for now).
  // Find the highest used key number.
  uint key_number= 0;
  for (uint i= 0; i < fk_number; i++)
  {
    const char *s;
    if ((*fk_info_buffer)[i].orig_name != nullptr)
    {
      // The FK already existed, check the orignal name
      s= strstr((*fk_info_buffer)[i].orig_name,
                dd::FOREIGN_KEY_NAME_SUBSTR);
    }
    else
    {
      // New FK, check the newly generated name
      s= strstr((*fk_info_buffer)[i].name,
                dd::FOREIGN_KEY_NAME_SUBSTR);
    }
    if (s && strlen(s) > 6)
    {
      char *e= NULL;
      uint nr= my_strtoull(s + 6, &e, 10);
      if (!*e && nr > key_number)
        key_number= nr;
    }
  }
  std::string name(table_name);
  name.append(dd::FOREIGN_KEY_NAME_SUBSTR);
  name.append(std::to_string(key_number + 1));
  return sql_strdup(name.c_str());
}


/**
  Prepare FOREIGN_KEY struct with info about a foreign key.

  @param thd                 Thread handle.
  @param create_info         Create info from parser.
  @param alter_info          Alter_info structure describing ALTER TABLE.
  @param table_name          Table name.
  @param key_info_buffer     Array of indexes.
  @param key_count           Number of indexes.
  @param fk_info_buffer      Array of FKs (pre-existing and new).
  @param fk_number           Index to the FK to be prepared.
  @param fk_key              Parser info about new FK to prepare.
  @param[out] fk_info        Struct to populate.

  @retval true if error (error reported), false otherwise.
*/

static bool prepare_foreign_key(THD *thd,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                const char *table_name,
                                KEY *key_info_buffer,
                                uint key_count,
                                FOREIGN_KEY **fk_info_buffer,
                                uint fk_number,
                                const Foreign_key_spec *fk_key,
                                FOREIGN_KEY *fk_info)
{
  DBUG_ENTER("prepare_foreign_key");

  // FKs are not supported for temporary tables.
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    my_error(ER_CANNOT_ADD_FOREIGN, MYF(0), table_name);
    DBUG_RETURN(true);
  }

  // Validate checks (among other things) that index prefixes are
  // not used and that generated columns are not used with
  // SET NULL and ON UPDATE CASCASE. Since this cannot change once
  // the FK has been made, it is enough to check it for new FKs.
  if (fk_key->validate(thd, table_name, alter_info->create_list))
    DBUG_RETURN(true);

  if (fk_key->name.str)
    fk_info->name= fk_key->name.str;
  else
  {
    fk_info->name= generate_fk_name(table_name,
                                    fk_info_buffer, fk_number);
  }
  // New FKs doesn't have an original FK name.
  fk_info->orig_name= nullptr;

  if (check_string_char_length(to_lex_cstring(fk_info->name),
                               "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), fk_info->name);
    DBUG_RETURN(true);
  }

  fk_info->key_parts= fk_key->columns.size();

  if (fk_key->ref_db.str)
  {
    fk_info->ref_db= fk_key->ref_db;
    if (lower_case_table_names == 1) // Store lowercase if LCTN = 1
    {
      char buff[NAME_LEN + 1];
      my_stpncpy(buff, fk_info->ref_db.str, NAME_LEN);
      my_casedn_str(system_charset_info, buff);
      fk_info->ref_db.str= sql_strdup(buff);
      fk_info->ref_db.length= strlen(fk_info->ref_db.str);
    }
  }
  else
    fk_info->ref_db= thd->db(); // No schema given, use current schema

  fk_info->ref_table= fk_key->ref_table;
  if (lower_case_table_names == 1) // Store lowercase if LCTN = 1
  {
    char buff[NAME_LEN + 1];
    my_stpncpy(buff, fk_info->ref_table.str, NAME_LEN);
    my_casedn_str(system_charset_info, buff);
    fk_info->ref_table.str= sql_strdup(buff);
    fk_info->ref_table.length= strlen(fk_info->ref_table.str);
  }

  fk_info->delete_opt= fk_key->delete_opt;
  fk_info->update_opt= fk_key->update_opt;
  fk_info->match_opt= fk_key->match_opt;

  fk_info->key_part=
    reinterpret_cast<LEX_CSTRING*>(thd->mem_calloc(sizeof(LEX_CSTRING) *
                                                   fk_key->columns.size()));
  fk_info->fk_key_part=
    reinterpret_cast<LEX_CSTRING*>(thd->mem_calloc(sizeof(LEX_CSTRING) *
                                                   fk_key->columns.size()));
  for (size_t column_nr= 0;
       column_nr < fk_key->ref_columns.size();
       column_nr++)
  {
    const Key_part_spec *col= fk_key->columns[column_nr];
    fk_info->key_part[column_nr]= col->field_name;
    const Key_part_spec *fk_col= fk_key->ref_columns[column_nr];

    if (check_column_name(fk_col->field_name.str))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), fk_col->field_name.str);
      DBUG_RETURN(true);
    }

    // Always store column names in lower case.
    char buff[NAME_LEN + 1];
    my_stpncpy(buff, fk_col->field_name.str, NAME_LEN);
    my_casedn_str(system_charset_info, buff);
    fk_info->fk_key_part[column_nr].str= sql_strdup(buff);
    fk_info->fk_key_part[column_nr].length= strlen(buff);
  }

  const char* supporting_index=
    find_fk_supporting_index(alter_info, key_info_buffer, key_count, fk_info);
  if (supporting_index == nullptr)
  {
    my_error(ER_CANNOT_ADD_FOREIGN, MYF(0));
    DBUG_RETURN(true);
  }
  // TODO: For now we use the index in the child table rather than the parent.
  fk_info->unique_index_name= supporting_index;

  DBUG_RETURN(false);
}


static bool prepare_key(THD *thd, HA_CREATE_INFO *create_info,
                        List<Create_field> *create_list, const Key_spec *key,
                        KEY **key_info_buffer, KEY *key_info,
                        KEY_PART_INFO **key_part_info,
                        Mem_root_array<const KEY *> &keys_to_check,
                        uint key_number, const handler *file,
                        int *auto_increment)
{
  DBUG_ENTER("prepare_key");
  DBUG_ASSERT(create_list);

  /*
    General checks.
  */

  if (key->columns.size() > file->max_key_parts() && key->type != KEYTYPE_SPATIAL)
  {
    my_error(ER_TOO_MANY_KEY_PARTS,MYF(0), file->max_key_parts());
    DBUG_RETURN(true);
  }

  if (check_string_char_length(key->name, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), key->name.str);
    DBUG_RETURN(true);
  }

  if (key->name.str && (key->type != KEYTYPE_PRIMARY) &&
      !my_strcasecmp(system_charset_info, key->name.str, primary_key_name))
  {
    my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
    DBUG_RETURN(true);
  }

  /* Create the key name based on the first column (if not given) */
  if (key->type == KEYTYPE_PRIMARY)
    key_info->name= primary_key_name;
  else if (key->name.str)
    key_info->name= key->name.str;
  else
  {
    const Key_part_spec *first_col= key->columns[0];
    List_iterator<Create_field> it(*create_list);
    Create_field *sql_field;
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         first_col->field_name.str,
                         sql_field->field_name))
      ;
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), first_col->field_name.str);
      DBUG_RETURN(true);
    }
    key_info->name= make_unique_key_name(sql_field->field_name,
                                         *key_info_buffer, key_info);
  }
  if (key->type != KEYTYPE_PRIMARY &&
      check_if_keyname_exists(key_info->name, *key_info_buffer, key_info))
  {
    my_error(ER_DUP_KEYNAME, MYF(0), key_info->name);
    DBUG_RETURN(true);
  }

  if (!key_info->name || check_column_name(key_info->name))
  {
    my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
    DBUG_RETURN(true);
  }

  key_info->comment.length= key->key_create_info.comment.length;
  key_info->comment.str= key->key_create_info.comment.str;
  if (validate_comment_length(thd, key_info->comment.str,
                              &key_info->comment.length,
                              INDEX_COMMENT_MAXLEN,
                              ER_TOO_LONG_INDEX_COMMENT,
                              key_info->name))
    DBUG_RETURN(true);
  if (key_info->comment.length > 0)
    key_info->flags|= HA_USES_COMMENT;

  switch (key->type) {
  case KEYTYPE_MULTIPLE:
    key_info->flags= 0;
    break;
  case KEYTYPE_FULLTEXT:
    if (!(file->ha_table_flags() & HA_CAN_FULLTEXT))
    {
      my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
      DBUG_RETURN(true);
    }
    key_info->flags= HA_FULLTEXT;
    if (key->key_create_info.parser_name.str)
    {
      key_info->parser_name= key->key_create_info.parser_name;
      key_info->flags|= HA_USES_PARSER;
    }
    else
      key_info->parser_name= NULL_CSTR;
    break;
  case KEYTYPE_SPATIAL:
    if (!(file->ha_table_flags() & HA_CAN_RTREEKEYS))
    {
      my_error(ER_TABLE_CANT_HANDLE_SPKEYS, MYF(0));
      DBUG_RETURN(true);
    }
    if (key->columns.size() != 1)
    {
      my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), 1);
      DBUG_RETURN(true);
    }
    key_info->flags= HA_SPATIAL;
    break;
  case KEYTYPE_PRIMARY:
  case KEYTYPE_UNIQUE:
    key_info->flags= HA_NOSAME;
    break;
  default:
    DBUG_ASSERT(false);
    DBUG_RETURN(true);
  }
  if (key->generated)
    key_info->flags|= HA_GENERATED_KEY;

  key_info->algorithm=              key->key_create_info.algorithm;
  key_info->user_defined_key_parts= key->columns.size();
  key_info->actual_key_parts=       key_info->user_defined_key_parts;
  key_info->key_part=               *key_part_info;
  key_info->usable_key_parts=       key_number;
  key_info->is_algorithm_explicit=  false;
  key_info->is_visible= key->key_create_info.is_visible;

  /*
    Make SPATIAL to be RTREE by default
    SPATIAL only on BLOB or at least BINARY, this
    actually should be replaced by special GEOM type
    in near future when new frm file is ready
    checking for proper key parts number:
  */

  if (key_info->flags & HA_SPATIAL)
  {
    DBUG_ASSERT(!key->key_create_info.is_algorithm_explicit);
    key_info->algorithm= HA_KEY_ALG_RTREE;
  }
  else if (key_info->flags & HA_FULLTEXT)
  {
    DBUG_ASSERT(!key->key_create_info.is_algorithm_explicit);
    key_info->algorithm= HA_KEY_ALG_FULLTEXT;
  }
  else
  {
    if (key->key_create_info.is_algorithm_explicit)
    {
      if (key->key_create_info.algorithm == HA_KEY_ALG_RTREE)
      {
        if ((key_info->user_defined_key_parts & 1) == 1)
        {
          my_error(ER_TOO_MANY_KEY_PARTS, MYF(0), 1);
          DBUG_RETURN(true);
        }
        /* TODO: To be deleted */
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
        DBUG_RETURN(true);
      }
      else
      {
        /*
          If key algorithm was specified explicitly check if it is
          supported by SE.
        */
        if (file->is_index_algorithm_supported(key->key_create_info.algorithm))
        {
          key_info->is_algorithm_explicit= true;
          key_info->algorithm= key->key_create_info.algorithm;
        }
        else
        {
          /*
            If explicit algorithm is not supported by SE, replace it with
            default one. Don't mark key algorithm as explicitly specified
            in this case.
          */
          key_info->algorithm= file->get_default_index_algorithm();

          push_warning_printf(thd, Sql_condition::SL_NOTE,
                              ER_UNSUPPORTED_INDEX_ALGORITHM,
                              ER_THD(thd, ER_UNSUPPORTED_INDEX_ALGORITHM),
                              ((key->key_create_info.algorithm ==
                                HA_KEY_ALG_HASH) ? "HASH" : "BTREE"));
        }
      }
    }
    else
    {
      /*
        If key algorithm was not explicitly specified used default one for
        this SE. Interesting side-effect of this is that ALTER TABLE will
        cause index rebuild if SE default changes.
        Assert that caller doesn't use any non-default algorithm in this
        case as such setting is ignored anyway.
      */
      DBUG_ASSERT(key->key_create_info.algorithm == HA_KEY_ALG_SE_SPECIFIC);
      key_info->algorithm= file->get_default_index_algorithm();
    }
  }

  /*
    Take block size from key part or table part
    TODO: Add warning if block size changes. We can't do it here, as
    this may depend on the size of the key
  */
  key_info->block_size= (key->key_create_info.block_size ?
                         key->key_create_info.block_size :
                         create_info->key_block_size);

  if (key_info->block_size)
    key_info->flags|= HA_USES_BLOCK_SIZE;

  const CHARSET_INFO *ft_key_charset= NULL;  // for FULLTEXT
  key_info->key_length= 0;
  for (size_t column_nr= 0 ;
       column_nr < key->columns.size() ;
       column_nr++, (*key_part_info)++)
  {
    if (prepare_key_column(thd, create_info, create_list, key,
                           key->columns[column_nr],
                           column_nr, key_info, *key_part_info, file,
                           auto_increment, &ft_key_charset))
      DBUG_RETURN(true);
  }
  key_info->actual_flags= key_info->flags;

  if (key_info->key_length > file->max_key_length() &&
      key->type != KEYTYPE_FULLTEXT)
  {
    my_error(ER_TOO_LONG_KEY, MYF(0), file->max_key_length());
    if (thd->is_error()) // May be silenced - see Bug#20629014
      DBUG_RETURN(true);
  }

  /*
    We only check for duplicate indexes if it is requested and the key is
    not auto-generated and non-PRIMARY.

    Check is requested if the key was explicitly created or altered
    (Index is altered/column associated with it is dropped) by the user
    (unless it's a foreign key).

    The fact that we have only one PRIMARY key for the table is checked
    elsewhere.

    At this point we simply add qualifying keys to the list, so we can
    perform check later when we properly construct KEY objects for all
    keys.
  */
  if (key->check_for_duplicate_indexes &&
      !key->generated && key->type != KEYTYPE_PRIMARY)
  {
    if (keys_to_check.push_back(key_info))
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


/**
  Primary/unique key check. Checks that:

  - If the storage engine requires it, that there is an index that is
    candidate for promotion.

  - If such a promotion occurs, checks that the candidate index is not
    declared invisible.

  @param file The storage engine handler.
  @param key_info_buffer All indexes in the table.
  @param key_count Number of indexes.

  @retval false OK.
  @retval true An error occured and my_error() was called.
*/

static bool check_promoted_index(const handler *file,
                                 const KEY *key_info_buffer,
                                 uint key_count)
{
  bool has_unique_key= false;
  const KEY *end= key_info_buffer + key_count;
  for (const KEY *k= key_info_buffer; k < end && !has_unique_key; ++k)
    if (!(k->flags & HA_NULL_PART_KEY) && (k->flags & HA_NOSAME))
    {
      has_unique_key= true;
      if (!k->is_visible)
      {
        my_error(ER_PK_INDEX_CANT_BE_INVISIBLE, MYF(0));
        return true;
      }
    }
  if (!has_unique_key && (file->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_error(ER_REQUIRES_PRIMARY_KEY, MYF(0));
    return true;
  }
  return false;
}


// Prepares the table and key structures for table creation.
bool mysql_prepare_create_table(THD *thd,
                                const char* error_schema_name,
                                const char* error_table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                handler *file, KEY **key_info_buffer,
                                uint *key_count,
                                FOREIGN_KEY **fk_key_info_buffer,
                                uint *fk_key_count,
                                FOREIGN_KEY *existing_fks,
                                uint existing_fks_count,
                                int select_field_count)
{
  DBUG_ENTER("mysql_prepare_create_table");

  /*
    Validation of table properties.
  */
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

  LEX_STRING *compress= &create_info->compress;
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
    DBUG_RETURN(true);
  }

  LEX_STRING *encrypt_type= &create_info->encrypt_type;
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

  if (validate_comment_length(thd,
                              create_info->comment.str,
                              &create_info->comment.length,
                              TABLE_COMMENT_MAXLEN,
                              ER_TOO_LONG_TABLE_COMMENT,
                              error_table_name))
  {
    DBUG_RETURN(true);
  }

  if (alter_info->create_list.elements > MAX_FIELDS)
  {
    my_error(ER_TOO_MANY_FIELDS, MYF(0));
    DBUG_RETURN(true);
  }

  /*
    Checks which previously were done during .FRM creation.

    TODO: Check if the old .FRM limitations still make sense
    with the new DD.
  */

  /* Fix this when we have new .frm files;  Current limit is 4G rows (QQ) */
  if (create_info->max_rows > UINT_MAX32)
    create_info->max_rows= UINT_MAX32;
  if (create_info->min_rows > UINT_MAX32)
    create_info->min_rows= UINT_MAX32;

  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    create_info->table_options|= HA_OPTION_PACK_RECORD;

  /*
    Prepare fields.
  */
  int select_field_pos= alter_info->create_list.elements - select_field_count;
  create_info->null_bits= 0;
  Create_field *sql_field;
  List_iterator<Create_field> it(alter_info->create_list);
  for (int field_no= 0; (sql_field=it++) ; field_no++)
  {
    if (prepare_create_field(thd, create_info, &alter_info->create_list,
                             &select_field_pos, file, sql_field, field_no))
      DBUG_RETURN(true);
  }
  calculate_field_offsets(&alter_info->create_list);

  /*
    Auto increment and blob checks.
  */
  int auto_increment= 0;
  int blob_columns= 0;
  it.rewind();
  while ((sql_field=it++))
  {
    if (sql_field->auto_flags & Field::NEXT_NUMBER)
      auto_increment++;
    switch (sql_field->sql_type)
    {
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_JSON:
      blob_columns++;
      break;
    default:
      break;
    }
  }
  if (auto_increment > 1)
  {
    my_error(ER_WRONG_AUTO_KEY, MYF(0));
    DBUG_RETURN(true);
  }
  if (auto_increment && (file->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_error(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT, MYF(0));
    DBUG_RETURN(true);
  }
  if (blob_columns && (file->ha_table_flags() & HA_NO_BLOBS))
  {
    my_error(ER_TABLE_CANT_HANDLE_BLOB, MYF(0));
    DBUG_RETURN(true);
  }
  /*
   CREATE TABLE[with auto_increment column] SELECT is unsafe as the rows
   inserted in the created table depends on the order of the rows fetched
   from the select tables. This order may differ on master and slave. We
   therefore mark it as unsafe.
  */
  if (select_field_count > 0 && auto_increment)
    thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_SELECT_AUTOINC);

  /*
    Count keys and key segments.
    Also mark redundant keys to be ignored.
  */
  uint key_parts;
  Mem_root_array<bool> redundant_keys(thd->mem_root,
                                      alter_info->key_list.size(), false);
  if (count_keys(alter_info->key_list, key_count, &key_parts,
                 fk_key_count, &redundant_keys,
                 (file->ha_table_flags() & HA_DESCENDING_INDEX)))
    DBUG_RETURN(true);
  if (*key_count > file->max_keys())
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0), file->max_keys());
    DBUG_RETURN(true);
  }

  /*
    Make KEY objects for the keys in the new table.
  */
  KEY *key_info;
  (*key_info_buffer)= key_info= (KEY*) sql_calloc(sizeof(KEY) * (*key_count));
  KEY_PART_INFO *key_part_info=
    (KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO) * key_parts);

  if (!*key_info_buffer || !key_part_info)
    DBUG_RETURN(true);				// Out of memory

  Mem_root_array<const KEY *> keys_to_check(thd->mem_root);
  if (keys_to_check.reserve(*key_count))
    DBUG_RETURN(true);				// Out of memory

  uint key_number= 0;
  bool primary_key= false;

  // First prepare non-foreign keys so that they are ready when
  // we prepare foreign keys.
  for (size_t i= 0; i < alter_info->key_list.size(); i++)
  {
    if (redundant_keys[i])
      continue; // Skip redundant keys

    const Key_spec *key= alter_info->key_list[i];

    if (key->type == KEYTYPE_PRIMARY)
    {
      if (primary_key)
      {
        my_error(ER_MULTIPLE_PRI_KEY, MYF(0));
        DBUG_RETURN(true);
      }
      primary_key= true;
    }

    if (key->type != KEYTYPE_FOREIGN)
    {
      if (prepare_key(thd, create_info, &alter_info->create_list,
                      key, key_info_buffer,
                      key_info, &key_part_info, keys_to_check,
                      key_number, file, &auto_increment))
        DBUG_RETURN(true);
      key_info++;
      key_number++;
    }
  }

  // Normal keys are done, now prepare foreign keys.
  (*fk_key_count)+= existing_fks_count;
  FOREIGN_KEY *fk_key_info;
  (*fk_key_info_buffer)= fk_key_info=
    (FOREIGN_KEY*) sql_calloc(sizeof(FOREIGN_KEY) * (*fk_key_count));

  if (!fk_key_info)
    DBUG_RETURN(true);				// Out of memory

  // Copy pre-existing foreign leys.
  if (existing_fks_count > 0)
    memcpy(*fk_key_info_buffer, existing_fks,
           existing_fks_count * sizeof(FOREIGN_KEY));
  uint fk_number= existing_fks_count;
  fk_key_info+= existing_fks_count;

  // Check that pre-existing foreign keys still have valid supporting indexes.
  // This is done here after any index changes have been reflected in
  // key_info_buffer.
  for (FOREIGN_KEY *fk= *fk_key_info_buffer;
       fk < (*fk_key_info_buffer) + existing_fks_count; fk++)
  {
    const char *new_supporting_index=
      find_fk_supporting_index(alter_info, *key_info_buffer, *key_count, fk);
    if (new_supporting_index == nullptr)
    {
      // No supporting index anymore, report the index used
      // before as the reason.
      my_error(ER_DROP_INDEX_FK, MYF(0), fk->unique_index_name);
      DBUG_RETURN(true);
    }
    // TODO: For now we use the index in the child table rather than the parent.
    fk->unique_index_name= new_supporting_index;
  }

  // Prepare new foreign keys.
  for (size_t i= 0; i < alter_info->key_list.size(); i++)
  {
    if (redundant_keys[i])
      continue; // Skip redundant keys

    const Key_spec *key= alter_info->key_list[i];

    if (key->type == KEYTYPE_FOREIGN)
    {
      if (prepare_foreign_key(thd, create_info, alter_info,
                              error_table_name,
                              *key_info_buffer, *key_count,
                              fk_key_info_buffer, fk_number,
                              down_cast<const Foreign_key_spec*>(key),
                              fk_key_info))
        DBUG_RETURN(true);
      fk_key_info++;
      fk_number++;
    }
  }

  /*
    At this point all KEY objects are for indexes are fully constructed.
    So we can check for duplicate indexes for keys for which it was requested.
  */
  const KEY **dup_check_key;
  for (dup_check_key= keys_to_check.begin();
       dup_check_key != keys_to_check.end();
       dup_check_key++)
  {
    if (check_duplicate_key(thd, error_schema_name, error_table_name,
                            *dup_check_key, *key_info_buffer, *key_count,
                            alter_info))
      DBUG_RETURN(true);
  }

  if (!primary_key && check_promoted_index(file, *key_info_buffer, *key_count))
    DBUG_RETURN(true);

  /*
    Any auto increment columns not found during prepare_key?
  */
  if (auto_increment > 0)
  {
    my_error(ER_WRONG_AUTO_KEY, MYF(0));
    DBUG_RETURN(true);
  }

  /* Sort keys in optimized order */
  my_qsort(reinterpret_cast<uchar*>(*key_info_buffer),
           *key_count, sizeof(KEY), sort_keys);

  /*
    Check if  STRICT SQL mode is active and server is not started with
    --explicit-defaults-for-timestamp. Below check was added to prevent implicit
    default 0 value of timestamp. When explicit-defaults-for-timestamp server
    option is removed, whole set of check can be removed.

    Note that this check must be after KEYs have been created as this
    can cause the NOT_NULL_FLAG to be set.
  */
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE &&
      !thd->variables.explicit_defaults_for_timestamp)
  {
    it.rewind();
    while ((sql_field= it++))
    {
      if (!sql_field->def &&
          !sql_field->gcol_info &&
          is_timestamp_type(sql_field->sql_type) &&
          (sql_field->flags & NOT_NULL_FLAG) &&
          !(sql_field->auto_flags & Field::DEFAULT_NOW))
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
        DBUG_RETURN(true);
      }
    }
  }

  /* If fixed row records, we need one bit to check for deleted rows */
  if (!(create_info->table_options & HA_OPTION_PACK_RECORD))
    create_info->null_bits++;
  ulong data_offset= (create_info->null_bits + 7) / 8;
  size_t reclength= data_offset;
  it.rewind();
  while ((sql_field= it++))
  {
    size_t length= sql_field->pack_length;
    if (sql_field->offset + data_offset + length > reclength)
      reclength= sql_field->offset + data_offset + length;
  }
  if (reclength > file->max_record_length())
  {
    my_error(ER_TOO_BIG_ROWSIZE, MYF(0),
             static_cast<long>(file->max_record_length()));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
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
    length= my_snprintf(warn_buff, sizeof(warn_buff), ER_THD(thd, err_code),
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

static bool set_table_default_charset(THD *thd,
                                      HA_CREATE_INFO *create_info,
                                      const char *db)
{
  /*
    If the table character set was not given explicitly,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (!create_info->default_table_charset &&
      get_default_db_collation(thd, db, &create_info->default_table_charset))
      return true;

  if (create_info->default_table_charset == NULL)
    create_info->default_table_charset= thd->collation();

  return false;
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
    my_snprintf(warn_buff, sizeof(warn_buff),
                ER_THD(thd, ER_AUTO_CONVERT), sql_field->field_name,
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
  @param      keys_onoff     Enable or disable keys.
  @param[out] fk_key_info    Array of FOREIGN_KEY objects describing foreign
                             keys in table which was created.
  @param[out] fk_key_count   Number of foreign keys in table which was created.
  @param[in] existing_fk_info Array of FOREIGN_KEY objects for foreign keys
                             which already existed in the table
                             (in case of ALTER TABLE).
  @param[in] existing_fk_count Number of pre-existing foreign keys.
  @param[out] tmp_table_def  Data-dictionary object for temporary table
                             which was created, but was not open because
                             of "no_ha_table" flag. NULL otherwise (if
                             table was open or is non-temporary).
  @param[out] post_ddl_ht    Set to handlerton for table's SE, if this SE
                             supports atomic DDL, so caller can call SE
                             post DDL hook after committing transaction.

  If one creates a temporary table, this is automatically opened

  Note that this function assumes that caller already have taken
  exclusive metadata lock on table being created or used some other
  way to ensure that concurrent operations won't intervene.
  mysql_create_table() is a wrapper that can be used for this.

  @note On failure, for engines supporting atomic DDL, the caller must
        rollback statement and transaction before doing anything else.

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
                       uint *key_count,
                       Alter_info::enum_enable_or_disable keys_onoff,
                       FOREIGN_KEY **fk_key_info,
                       uint *fk_key_count,
                       FOREIGN_KEY *existing_fk_info,
                       uint existing_fk_count,
                       dd::Table **tmp_table_def,
                       handlerton **post_ddl_ht)
{
  const char	*alias;
  handler	*file;
  bool		error= TRUE;
  DBUG_ENTER("create_table_impl");
  DBUG_PRINT("enter", ("db: '%s'  table: '%s'  tmp: %d",
                       db, table_name, internal_tmp_table));
  *tmp_table_def= NULL;

  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_error(ER_TABLE_MUST_HAVE_COLUMNS, MYF(0));
    DBUG_RETURN(TRUE);
  }

  // Check if new table creation is disallowed by the storage engine.
  if (!internal_tmp_table &&
      ha_is_storage_engine_disabled(create_info->db_type))
  {
    my_error(ER_DISABLED_STORAGE_ENGINE, MYF(0),
              ha_resolve_storage_engine_name(create_info->db_type));
    DBUG_RETURN(true);
  }

  if (check_engine(thd, db, table_name, create_info))
    DBUG_RETURN(TRUE);

  if (set_table_default_charset(thd, create_info, db))
    DBUG_RETURN(TRUE);

  alias= table_case_name(create_info, table_name);

  partition_info *part_info= thd->work_part_info;

  if (!(file= get_new_handler((TABLE_SHARE*) 0,
                              (part_info ||
                               (create_info->db_type->partition_flags &&
                                (create_info->db_type->partition_flags() &
                                 HA_USE_AUTO_PARTITION))),
                              thd->mem_root, create_info->db_type)))
  {
    mem_alloc_error(sizeof(handler));
    DBUG_RETURN(TRUE);
  }

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
                                                 &alter_info->create_list,
                                                 NULL);
      thd->variables.sql_mode= sql_mode_backup;
      if (part_syntax_buf == NULL)
      {
        goto err;
      }
    }
    part_info->part_info_string= part_syntax_buf;
    part_info->part_info_len= syntax_len;
    if (!engine_type->partition_flags)
    {
      /*
        The handler assigned to the table cannot handle partitioning.
      */
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "native partitioning");
      goto err;
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
      if (!(file= get_new_handler((TABLE_SHARE*) 0, true, thd->mem_root,
                                  engine_type)))
      {
        mem_alloc_error(sizeof(handler));
        DBUG_RETURN(TRUE);
      }
      create_info->db_type= engine_type;
    }
  }

  if (mysql_prepare_create_table(thd, db, error_table_name,
                                 create_info, alter_info,
                                 file,
                                 key_info, key_count,
                                 fk_key_info, fk_key_count,
                                 existing_fk_info, existing_fk_count,
                                 select_field_count))
    goto err;

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      find_temporary_table(thd, db, table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_TABLE_EXISTS_ERROR,
                          ER_THD(thd, ER_TABLE_EXISTS_ERROR),
                          alias);
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    goto err;
  }

  if (!internal_tmp_table &&
      !(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      !dd::get_dictionary()->is_dd_table_name(db, table_name))
  {
    // TODO: Check if it safe to remove this block of code
    bool exists;
    if (dd::table_exists<dd::Abstract_table>(thd->dd_client(), db, table_name,
                                             &exists))
    {
      error= 1;
      goto err;
    }

    if (exists)
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto err;
    }
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
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      !dd::get_dictionary()->is_dd_table_name(db, table_name))
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
                          WARN_OPTION_IGNORED,
                          ER_THD(thd, WARN_OPTION_IGNORED),
                          "DATA DIRECTORY");
    if (create_info->index_file_name)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          WARN_OPTION_IGNORED,
                          ER_THD(thd, WARN_OPTION_IGNORED),
                          "INDEX DIRECTORY");
    create_info->data_file_name= create_info->index_file_name= 0;
  }

  if (thd->variables.keep_files_on_create)
    create_info->options|= HA_CREATE_KEEP_FILES;

  /*
    Create table definitions.
    If "no_ha_table" is false also create table in storage engine.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (rea_create_tmp_table(thd, path, db, table_name,
                             create_info, alter_info->create_list,
                             *key_count, *key_info, keys_onoff,
                             file, no_ha_table, is_trans,
                             tmp_table_def))
      goto err;
  }
  else
  {
    if (rea_create_base_table(thd, path, db, table_name,
                              create_info, alter_info->create_list,
                              *key_count, *key_info, keys_onoff, *fk_key_count,
                              *fk_key_info, file, no_ha_table, part_info,
                              is_trans, post_ddl_ht))
      goto err;
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
                      ER_TABLE_EXISTS_ERROR,
                      ER_THD(thd, ER_TABLE_EXISTS_ERROR),
                      alias);
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
                                bool *is_trans,
                                handlerton **post_ddl_ht)
{
  KEY *not_used_1;
  uint not_used_2;
  FOREIGN_KEY *not_used_3= NULL;
  uint not_used_4= 0;
  dd::Table *not_used_5;
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

  /*
    Don't create the DD tables in the DDSE unless installing the DD.

    In upgrade scenario, to check the existence of version table,
    we try to open it. This requires dd::Table object for this table.
    To create this object we run CREATE TABLE statement for the table
    but avoid creation of table inside SE. If version table is not
    found, we create it inside SE on later steps.
  */

  bool is_innodb_stats_table= (!strcmp(db, "mysql")) &&
    ((!(strcmp(table_name, "innodb_table_stats")) ||
     !(strcmp(table_name, "innodb_index_stats"))));

  bool no_ha_table= false;
  if ((!opt_initialize || dd_upgrade_skip_se ||
      (dd_upgrade_flag && is_innodb_stats_table)) &&
      dd::get_dictionary()->is_dd_table_name(db, table_name))
    no_ha_table= true;

  return create_table_impl(thd, db, table_name, table_name,
                           path, create_info, alter_info,
                           false, select_field_count, no_ha_table, is_trans,
                           &not_used_1, &not_used_2, Alter_info::ENABLE,
                           &not_used_3, &not_used_4,
                           NULL, 0,
                           &not_used_5, post_ddl_ht);
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
  handlerton *post_ddl_ht= nullptr;
  DBUG_ENTER("mysql_create_table");

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /*
    Open or obtain "X" MDL lock on the table being created.
    To check the existence of table, lock of type "S" is obtained on the table
    and then it is upgraded to "X" if table does not exists.
  */
  if (open_tables(thd, &thd->lex->query_tables, &not_used, 0))
  {
    result= true;
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
                                     alter_info, 0, &is_trans,
                                     &post_ddl_ht);

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

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    // Update view metadata.
    if (!result)
    {
      Uncommitted_tables_guard uncommitted_tables(thd);

      if (! create_table->table && !create_table->is_view())
        uncommitted_tables.add_table(create_table);

      result= update_referencing_views_metadata(thd, create_table, !is_trans,
                                                &uncommitted_tables);
    }

    /*
      Unless we are executing CREATE TEMPORARY TABLE we need to commit
      changes to the data-dictionary, SE and binary log and possibly run
      handlerton's post-DDL hook.
    */
    if (!result)
      result= trans_commit_stmt(thd) || trans_commit_implicit(thd);

    if (result)
    {
      trans_rollback_stmt(thd);
      /*
        Full rollback in case we have THD::transaction_rollback_request
        and to synchronize DD state in cache and on disk (as statement
        rollback doesn't clear DD cache of modified uncommitted objects).
      */
      trans_rollback(thd);
    }

    /*
      In case of CREATE TABLE post-DDL hook is mostly relevant for case
      when statement is rolled back. In such cases it is responsibility
      of this hook to cleanup files which might be left after failed
      table creation attempt.
    */
    if (post_ddl_ht)
      post_ddl_ht->post_ddl(thd);
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
      return true;
  return false;
}


static const char *
make_unique_key_name(const char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      my_strcasecmp(system_charset_info,field_name,primary_key_name))
    return field_name;			// Use fieldname
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
  return "not_specified";		// Should never happen
}


/****************************************************************************
** Alter a table definition
****************************************************************************/


/**
  Rename a table.

  @param thd       Thread handle
  @param base      The handlerton handle.
  @param old_db    The old database name.
  @param old_name  The old table name.
  @param new_db    The new database name.
  @param new_name  The new table name.
  @param flags     flags
                   FN_FROM_IS_TMP old_name is temporary.
                   FN_TO_IS_TMP   new_name is temporary.
                   NO_HA_TABLE    Don't rename table in engine.
                   NO_FK_CHECKS   Don't check FK constraints during rename.
                   NO_DD_COMMIT   Don't commit transaction after updating
                                  data-dictionary.

  @note Use of NO_DD_COMMIT flag only allowed for SEs supporting atomic DDL.

  @note In case when NO_DD_COMMIT flag was used, the caller must rollback
        both statement and transaction on failure. This is necessary to
        revert results of handler::ha_rename_table() call in case when
        update to the data-dictionary which follows it fails. Also this must
        be done before any further accesses to DD. @sa dd::rename_table().

  @return false    OK
  @return true     Error
*/

bool
mysql_rename_table(THD *thd, handlerton *base, const char *old_db,
                   const char *old_name, const char *new_db,
                   const char *new_name, uint flags)
{
  DBUG_ENTER("mysql_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_db, old_name, new_db, new_name));

  /*
    Only SEs which support atomic DDL are allowed not to commit
    changes to the data-dictionary.
  */
  DBUG_ASSERT(!(flags & NO_DD_COMMIT) ||
              (base->flags & HTON_SUPPORTS_ATOMIC_DDL));
  /*
    Check if the new_db database exists. The problem is that some
    SE's may not verify if new_db database exists and they might
    succeed renaming the table. Moreover, even the InnoDB engine
    succeeds renaming the table without verifying if the new_db
    database exists when innodb_file_per_table=0.
  */

  // We must make sure the schema is released and unlocked in the right order.
  dd::Schema_MDL_locker from_mdl_locker(thd);
  dd::Schema_MDL_locker to_mdl_locker(thd);
  // Check if destination schemas exist.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *from_sch= NULL;
  const dd::Schema *to_sch= NULL;
  const dd::Table *from_table_def= NULL;
  dd::Table *to_table_def= NULL;

  if (from_mdl_locker.ensure_locked(old_db) ||
      to_mdl_locker.ensure_locked(new_db) ||
      thd->dd_client()->acquire<dd::Schema>(old_db, &from_sch) ||
      thd->dd_client()->acquire<dd::Schema>(new_db, &to_sch))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  // We did not find old_db, so stop here.
  if (!from_sch)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), old_db);
    DBUG_RETURN(true);
  }

  // We did not find new_db, so stop here.
  if (!to_sch)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), new_db);
    DBUG_RETURN(true);
  }

  // Check if we hit FN_REFLEN bytes along with file extension.
  char from[FN_REFLEN + 1];
  char to[FN_REFLEN + 1];
  size_t length;
  bool was_truncated;
  build_table_filename(from, sizeof(from) - 1, old_db, old_name, "",
                       flags & FN_FROM_IS_TMP);
  length= build_table_filename(to, sizeof(to) - 1, new_db, new_name, "",
                               flags & FN_TO_IS_TMP, &was_truncated);
  if (was_truncated || length + reg_ext_length > FN_REFLEN)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(to)-1, to);
    DBUG_RETURN(true);
  }

  // Get original dd::Table object (also emits error if it doesn't exist).
  if (thd->dd_client()->acquire<dd::Table>(old_db, old_name,
                                          &from_table_def) ||
      thd->dd_client()->acquire_for_modification<dd::Table>(old_db,
                          old_name, &to_table_def))
    DBUG_RETURN(true);

  // Set schema id, table name and hidden attribute.
  to_table_def->set_schema_id(to_sch->id());
  to_table_def->set_name(new_name);
  to_table_def->set_hidden(flags & FN_TO_IS_TMP);

  // Get the handler for the table, and issue an error if we cannot load it.
  handler *file= (base == NULL ? 0 :
         get_new_handler((TABLE_SHARE*)0,
                         from_table_def->partition_type()!=dd::Table::PT_NONE,
                         thd->mem_root, base));
  if (!file)
  {
    my_error(ER_STORAGE_ENGINE_NOT_LOADED, MYF(0), old_db, old_name);
    DBUG_RETURN(true);
  }

  /*
    If lower_case_table_names == 2 (case-preserving but case-insensitive
    file system) and the storage is not HA_FILE_BASED, we need to provide
    a lowercase file name.
  */
  char lc_from[FN_REFLEN + 1];
  char lc_to[FN_REFLEN + 1];
  char *from_base= from;
  char *to_base= to;
  if (lower_case_table_names == 2 && !(file->ha_table_flags() & HA_FILE_BASED))
  {
    char tmp_name[NAME_LEN+1];
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

  /*
    Temporarily disable foreign key checks, if requested, while the
    handler is involved.
  */
  ulonglong save_bits= thd->variables.option_bits;
  if (flags & NO_FK_CHECKS)
    thd->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;

  int error= 0;
  if (!(flags & NO_HA_TABLE))
    error= file->ha_rename_table(from_base, to_base, from_table_def,
                                 to_table_def);

  thd->variables.option_bits= save_bits;

  if (error != 0)
  {
    if (!(flags & NO_HA_TABLE))
    {
      if (error == HA_ERR_WRONG_COMMAND)
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER TABLE");
      else
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        my_error(ER_ERROR_ON_RENAME, MYF(0), from, to,
                 error, my_strerror(errbuf, sizeof(errbuf), error));
      }
    }
    delete file;
    DBUG_RETURN(true);
  }

  /*
    Note that before WL#7743 we have renamed table in the data-dictionary
    before renaming it in storage engine. However with WL#7743 engines
    supporting atomic DDL are allowed to update dd::Table object describing
    new version of table in handler::rename_table(). Hence it should saved
    after this call.
    So to avoid extra calls to DD layer and to keep code simple the
    renaming of table in the DD was moved past rename in SE for all SEs.
    From crash-safety point of view order doesn't matter for engines
    supporting atomic DDL. And for engines which can't do atomic DDL in
    either case there are scenarios in which DD and SE get out of sync.
  */
  if (dd::rename_table(thd, old_name, to_table_def, !(flags & NO_DD_COMMIT)))
  {
    /*
      In cases when we are executing atomic DDL it is responsibility of the
      caller to revert the changes to SE by rolling back transaction.

      If storage engine supports atomic DDL but commit was requested by the
      caller the above call to dd::rename_table() will roll back transaction
      on failure and thus revert change to SE.
    */
    if (!(flags & NO_HA_TABLE)
#ifdef WORKAROUND_TO_BE_REMOVED_BY_WL7016
        && !(flags & NO_DD_COMMIT)
#endif
       )
      (void) file->ha_rename_table(to_base, from_base, to_table_def,
                                   const_cast<dd::Table*>(from_table_def));
    delete file;
    DBUG_RETURN(true);
  }
  delete file;

#ifdef HAVE_PSI_TABLE_INTERFACE
  /*
    Remove the old table share from the pfs table share array. The new table
    share will be created when the renamed table is first accessed.
  */
  bool temp_table= (bool)is_prefix(old_name, tmp_file_prefix);
  PSI_TABLE_CALL(drop_table_share)
    (temp_table, old_db, static_cast<int>(strlen(old_db)),
     old_name, static_cast<int>(strlen(old_name)));
#endif

  DBUG_RETURN(false);
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
  bool is_trans= FALSE;
  uint not_used;
  Tablespace_hash_set tablespace_set(PSI_INSTRUMENT_ME);
  handlerton *post_ddl_ht= nullptr;

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
    DBUG_RETURN(true);
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
  memset(&local_create_info, 0, sizeof(local_create_info));
  local_create_info.db_type= src_table->table->s->db_type();
  local_create_info.row_type= src_table->table->s->row_type;
  if (mysql_prepare_alter_table(thd, src_table->table, &local_create_info,
                                &local_alter_info, &local_alter_ctx))
    DBUG_RETURN(true);
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

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (mysql_create_table_no_lock(thd, table->db, table->table_name,
                                 &local_create_info, &local_alter_info,
                                 0, &is_trans, &post_ddl_ht))
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
            bool result= open_table(thd, table, &ot_ctx);

            /*
              Play safe, ensure that we won't poison TDC/TC by storing
              not-yet-committed table definition there.
            */
            tdc_remove_table(thd, TDC_RT_REMOVE_NOT_OWN, table->db,
                             table->table_name, false);

            if (result)
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
          {
            if (new_table)
            {
              DBUG_ASSERT(thd->open_tables == table->table);
              close_thread_table(thd, &thd->open_tables);
              table->table= nullptr;
            }
            goto err;
          }

          /*
            As the reference table is temporary and may not exist on slave, we must
            force the ENGINE to be present into CREATE TABLE.
          */
          create_info->used_fields|= HA_CREATE_USED_ENGINE;

          int result MY_ATTRIBUTE((unused))=
            store_create_info(thd, table, &query,
                              create_info, TRUE /* show_database */);

          DBUG_ASSERT(result == 0); // store_create_info() always return 0

          if (new_table)
          {
            DBUG_ASSERT(thd->open_tables == table->table);
            /*
              When opening the table, we ignored the locked tables
              (MYSQL_OPEN_GET_NEW_TABLE). Now we can close the table
              without risking to close some locked table.
            */
            close_thread_table(thd, &thd->open_tables);
            table->table= nullptr;
          }

          if (write_bin_log(thd, true, query.ptr(), query.length(), is_trans))
            goto err;
        }
      }
      else                                      // Case 1
        if (write_bin_log(thd, true, thd->query().str, thd->query().length,
                          is_trans))
          goto err;
    }
    /*
      Case 3 and 4 does nothing under RBR
    */
  }
  else if (write_bin_log(thd, true,
                         thd->query().str, thd->query().length, is_trans))
    goto err;

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    /*
      Update view metadata. Use nested block to ensure that TDC
      invalidation happens before commit.
    */
    {
      Uncommitted_tables_guard uncommitted_tables(thd);

      if (! table->table && !table->is_view())
        uncommitted_tables.add_table(table);

      if (update_referencing_views_metadata(thd, table, !is_trans,
                                            &uncommitted_tables))
        goto err;
    }

    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      goto err;

    if (post_ddl_ht)
      post_ddl_ht->post_ddl(thd);
  }
  DBUG_RETURN(false);

err:
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    trans_rollback_stmt(thd);
    /*
      Full rollback in case we have THD::transaction_rollback_request
      and to synchronize DD state in cache and on disk (as statement
      rollback doesn't clear DD cache of modified uncommitted objects).
    */
    trans_rollback(thd);

    if (post_ddl_ht)
      post_ddl_ht->post_ddl(thd);
  }
  DBUG_RETURN(true);
}

/* table_list should contain just one table */
bool mysql_discard_or_import_tablespace(THD *thd,
                                        TABLE_LIST *table_list)
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
    Adjust values of table-level and metadata which was set in parser
    for the case general ALTER TABLE.
  */
  table_list->mdl_request.set_type(MDL_EXCLUSIVE);
  table_list->set_lock({TL_WRITE, THR_DEFAULT});
  /* Do not open views. */
  table_list->required_type= dd::enum_table_type::BASE_TABLE;

  if (open_and_lock_tables(thd, table_list, 0, &alter_prelocking_strategy))
  {
    /* purecov: begin inspected */
    DBUG_RETURN(true);
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
        DBUG_RETURN(true);
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

  bool is_non_tmp_table= (table_list->table->s->tmp_table == NO_TMP_TABLE);
  handlerton *hton= table_list->table->s->db_type();

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  dd::Table *non_tmp_table_def= nullptr;

  if (is_non_tmp_table)
  {
    if (thd->dd_client()->acquire_for_modification<dd::Table>(table_list->db,
                            table_list->table_name, &non_tmp_table_def))
      DBUG_RETURN(true);

    /* Table was successfully opened above. */
    DBUG_ASSERT(non_tmp_table_def != nullptr);
  }

  /*
    Under LOCK TABLES we need to upgrade SNRW metadata lock to X lock
    before doing discard or import of tablespace.

    Skip this step for temporary tables as metadata locks are not
    applicable for them.

    Remember the ticket for the future downgrade.
  */
  MDL_ticket *mdl_ticket= nullptr;

  if (is_non_tmp_table &&
      (thd->locked_tables_mode == LTM_LOCK_TABLES ||
       thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES))
  {
    mdl_ticket= table_list->table->mdl_ticket;
    if (thd->mdl_context.upgrade_shared_lock(mdl_ticket, MDL_EXCLUSIVE,
                                             thd->variables.lock_wait_timeout))
      DBUG_RETURN(true);
  }

  /*
    The parser sets a flag in the thd->lex->alter_info struct to indicate
    whether this is DISCARD or IMPORT. The flag is used for two purposes:

    1. To submit the appropriate parameter to the SE to indicate which
       operation is to be performed (see the source code below).
    2. To implement a callback function (the plugin API function
       'thd_tablespace_op()') allowing the SEs supporting these
       operations to check if we are doing a DISCARD or IMPORT, in order to
       suppress errors otherwise being thrown when opening tables with a
       missing tablespace.
  */

  bool discard= (thd->lex->alter_info.flags &
                 Alter_info::ALTER_DISCARD_TABLESPACE);
  error= table_list->table->file->ha_discard_or_import_tablespace(
                                    discard,
                                    (is_non_tmp_table ?
                                     non_tmp_table_def :
                                     table_list->table->s->tmp_table_def));

  THD_STAGE_INFO(thd, stage_end);

  if (error)
  {
    table_list->table->file->print_error(error, MYF(0));
  }
  else
  {
    /*
      The 0 in the call below means 'not in a transaction', which means
      immediate invalidation; that is probably what we wish here
    */
    query_cache.invalidate(thd, table_list, FALSE);

    /*
      Storage engine supporting atomic DDL can fully rollback discard/
      import if any problem occurs. This will happen during statement
      rollback.

      In case of success we need to save dd::Table object which might
      have been updated by SE. If this step or subsequent write to binary
      log fail then statement rollback will also restore status quo ante.
    */
    if (is_non_tmp_table &&
        (hton->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
        thd->dd_client()->update<dd::Table>(non_tmp_table_def))
      error= 1;

    if (!error)
      error= write_bin_log(thd, false, thd->query().str, thd->query().length,
                           (hton->flags & HTON_SUPPORTS_ATOMIC_DDL));

    /*
      TODO: In theory since we have updated table definition in the
            data-dictionary above we need to remove its TABLE/TABLE_SHARE
            from TDC now. However this makes InnoDB to produce too many
            warnings about discarded tablespace which are not always well
            justified. So this code should be enabled after InnoDB is
            adjusted to be less verbose in these cases.
    */
#ifdef NEEDS_SUPPORT_FROM_INNODB
    if (is_non_tmp_table)
      close_all_tables_for_name(thd, table_list->table->s, false, nullptr);
    table_list->table= nullptr; // Safety.
#endif
  }

  if (!error)
    error= trans_commit_stmt(thd) || trans_commit_implicit(thd);

  if (error)
  {
    trans_rollback_stmt(thd);
    trans_rollback_implicit(thd);
  }

  if (is_non_tmp_table &&
      (hton->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      hton->post_ddl)
    hton->post_ddl(thd);

  if (thd->locked_tables_mode && thd->locked_tables_list.reopen_tables(thd))
    error= 1;

  if (mdl_ticket)
    mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);

  if (error == 0)
  {
    my_ok(thd);
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
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

static const Create_field *get_field_by_index(Alter_info *alter_info, uint idx)
{
  List_iterator_fast<Create_field> field_it(alter_info->create_list);
  uint field_idx= 0;
  const Create_field *field;

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

  @param[in,out]  ha_alter_info  Structure describing changes to be done
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
    /*
      Key definition has changed if we are using a different field or
      if the used key part length is different, or key part direction has
      changed. It makes sense to check lengths first as in case when fields
      differ it is likely that lengths differ too and checking fields is more
      expensive in general case.

    */
    if (key_part->length != new_part->length ||
        (key_part->key_part_flag & HA_REVERSE_SORT) !=
        (new_part->key_part_flag & HA_REVERSE_SORT))
      return true;

    new_field= get_field_by_index(alter_info, new_part->fieldnr);

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


static int compare_uint(const void *a, const void *b)
{
  const uint *s= pointer_cast<const uint*>(a);
  const uint *t= pointer_cast<const uint*>(b);
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
   @param[in,out]  ha_alter_info      Data structure which already contains
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
                             alter_info->key_list.size())) ||
      ! (ha_alter_info->index_rename_buffer=
          (KEY_PAIR*) thd->alloc(sizeof(KEY_PAIR) *
                                 alter_info->alter_rename_key_list.size())) ||
      !(ha_alter_info->index_altered_visibility_buffer=
        (KEY_PAIR*) thd->alloc(sizeof(KEY_PAIR) *
        alter_info->alter_index_visibility_list.size())))
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
  if (alter_info->flags & Alter_info::ALTER_REBUILD_PARTITION)
    ha_alter_info->handler_flags|= Alter_inplace_info::ALTER_REBUILD_PARTITION; /* purecov: deadcode */
  /* Check for: ALTER TABLE FORCE, ALTER TABLE ENGINE and OPTIMIZE TABLE. */
  if (alter_info->flags & Alter_info::ALTER_RECREATE)
    ha_alter_info->handler_flags|= Alter_inplace_info::RECREATE_TABLE;
  if (alter_info->with_validation == Alter_info::ALTER_WITH_VALIDATION)
    ha_alter_info->handler_flags|= Alter_inplace_info::VALIDATE_VIRTUAL_COLUMN;

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

  for (const Alter_rename_key *rename_key : alter_info->alter_rename_key_list)
  {
    table_key= find_key_ci(rename_key->old_name, table->key_info, table_key_end);
    new_key= find_key_ci(rename_key->new_name, ha_alter_info->key_info_buffer,
                         new_key_end);

    table_key->flags|= HA_KEY_RENAMED;
    new_key->flags|= HA_KEY_RENAMED;

    if (! has_index_def_changed(ha_alter_info, table_key, new_key))
    {
      /* Key was not modified in any significant way but still was renamed. */
      ha_alter_info->handler_flags|= Alter_inplace_info::RENAME_INDEX;
      ha_alter_info->add_renamed_key(table_key, new_key);

      /*
        Check for insignificant changes which do not call for index
        recreation, but still require update of .FRM.
      */
      if (table_key->is_algorithm_explicit != new_key->is_algorithm_explicit)
        ha_alter_info->handler_flags|= Alter_inplace_info::CHANGE_INDEX_OPTION;
    }
    else
    {
      /* Key was modified. */
      ha_alter_info->add_modified_key(table_key, new_key);
    }
  }

  for (const Alter_index_visibility *alter_index_visibility :
         alter_info->alter_index_visibility_list)
  {
    const char *name= alter_index_visibility->name();
    table_key= find_key_ci(name, table->key_info, table_key_end);
    new_key= find_key_ci(name, ha_alter_info->key_info_buffer, new_key_end);

    if (new_key == NULL)
    {
      my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), name, table->s->table_name.str);
      DBUG_RETURN(true);
    }

    new_key->is_visible= alter_index_visibility->is_visible();
    ha_alter_info->handler_flags|= Alter_inplace_info::RENAME_INDEX;
    ha_alter_info->add_altered_index_visibility(table_key, new_key);
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
    else
    {
      /*
        Key was not modified in significant way. Still we need to check
        for insignificant changes which do not call for index recreation,
        but still require update of .FRM.
      */
      if (table_key->is_algorithm_explicit != new_key->is_algorithm_explicit)
        ha_alter_info->handler_flags|= Alter_inplace_info::CHANGE_INDEX_OPTION;
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
           sizeof(uint), compare_uint);

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
  uint fk_key_count= 0;
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
  KEY *key_info_buffer= NULL;
  FOREIGN_KEY *fk_key_info_buffer= NULL;

  /* Create the prepared information. */
  if (mysql_prepare_create_table(thd,
                                 "", // Not used
                                 "", // Not used
                                 create_info,
                                 &tmp_alter_info,
                                 table->file, &key_info_buffer,
                                 &key_count, &fk_key_info_buffer,
                                 &fk_key_count, NULL, 0, 0))
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
    const Create_field *tmp_new_field= tmp_new_field_it++;

    /* Check that NULL behavior is the same. */
    if ((tmp_new_field->flags & NOT_NULL_FLAG) !=
	(uint) (field->flags & NOT_NULL_FLAG))
      DBUG_RETURN(false);

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
bool alter_table_manage_keys(THD *thd, TABLE *table, int indexes_were_disabled,
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
    push_warning_printf(thd, Sql_condition::SL_NOTE,
                        ER_ILLEGAL_HA, ER_THD(thd, ER_ILLEGAL_HA),
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

  @return false       In-place is possible, check with storage engine.
  @return true        Incompatible operations, must use table copy.
*/

static bool is_inplace_alter_impossible(TABLE *table,
                                        HA_CREATE_INFO *create_info,
                                        const Alter_info *alter_info)
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
    DBUG_RETURN(true);

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
  if ((alter_info->flags & Alter_info::ALTER_CHANGE_COLUMN_DEFAULT) &&
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
  handlerton *db_type= table->s->db_type();
  MDL_ticket *mdl_ticket= table->mdl_ticket;
  const Alter_info *alter_info= ha_alter_info->alter_info;
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
    // We should have upgraded from MDL_SHARED_UPGRADABLE to a lock
    // blocking writes for it to be safe to check ha_records().
    if (table->mdl_ticket->get_type() == MDL_SHARED_UPGRADABLE)
    {
      my_error(ER_INVALID_USE_OF_NULL, MYF(0));
      goto cleanup;
    }

    // Check if the handler supports ha_records()
    if (!(table_list->table->file->ha_table_flags() & HA_HAS_RECORDS))
    {
      // If ha_records() is not supported, be conservative.
      my_error(ER_INVALID_USE_OF_NULL, MYF(0));
      goto cleanup;
    }

    ha_rows tmp= 0;
    if (table_list->table->file->ha_records(&tmp) || tmp > 0)
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

  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    const dd::Table *old_table_def= nullptr;
    if (thd->dd_client()->acquire<dd::Table>(table->s->db.str,
                                             table->s->table_name.str,
                                             &old_table_def))
      goto cleanup;

    dd::Table *altered_table_def= nullptr;
    if (thd->dd_client()->acquire_for_modification(alter_ctx->new_db,
                                                   alter_ctx->tmp_name,
                                                   &altered_table_def))
      goto cleanup;

    /*
      We want warnings/errors about data truncation emitted when
      values of virtual columns are evaluated in INPLACE algorithm.
    */
    thd->check_for_truncated_fields= CHECK_FIELD_WARN;
    thd->num_truncated_fields= 0L;

    if (table->file->ha_prepare_inplace_alter_table(altered_table,
                                                    ha_alter_info,
                                                    old_table_def,
                                                    altered_table_def))
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
                                            ha_alter_info,
                                            old_table_def,
                                            altered_table_def))
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
                                                 false,
                                                 old_table_def,
                                                 altered_table_def);
      my_error(ER_UNKNOWN_ERROR, MYF(0));
      thd->check_for_truncated_fields= CHECK_FIELD_IGNORE;
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
                                                   true, old_table_def,
                                                   altered_table_def))
    {
      goto rollback;
    }

    thd->check_for_truncated_fields= CHECK_FIELD_IGNORE;

    close_all_tables_for_name(thd, table->s, alter_ctx->is_table_renamed(), NULL);
    table_list->table= table= NULL;
    reopen_tables= true;
    close_temporary_table(thd, altered_table, true, false);

    /*
      Replace table definition in the data-dictionary.

      Note that any error after this point is really awkward for storage engines
      which don't support atomic DDL. Changes to table in SE are already committed
      and can't be rolled back. Failure to update data-dictionary or binary log
      will create inconsistency between them and SE. Since we can't do much in this
      situation we simply return error and hope that old table definition is
      compatible enough with a new one.

      For engines supporting atomic DDL error is business-as-usual situation.
      Rollback of statement which happens on error should revert changes to
      table in SE as well.
    */
    altered_table_def->set_schema_id(old_table_def->schema_id());
    altered_table_def->set_name(alter_ctx->alias);
    altered_table_def->set_hidden(false);

    if (thd->dd_client()->drop(old_table_def))
      goto cleanup2;

    if (thd->dd_client()->update(altered_table_def))
      goto cleanup2;


    /*
      Transfer pre-existing triggers to the new table
      definition. Since trigger names have to be unique per schema,
      we cannot create them while both the old and the temp version of the
      table definition exist.

      Do this atomically with deletion of old table definition and replacing
      it with a new one.
    */
    if (!alter_ctx->trg_info.empty() &&
        dd::add_triggers(thd, alter_ctx->db,
                         alter_ctx->table_name,
                         &alter_ctx->trg_info,
                         false))
      goto cleanup2;

    /*
      Rename pre-existing foreign keys back to their original names.
      Since foreign key names have to be unique per schema, they cannot
      have the same name in both the old and the temp version of the
      table definition. Since we now have only one defintion, the names
      can be restored.
    */
    if (alter_ctx->fk_count > 0 &&
        restore_foreign_key_names(thd, alter_ctx->db,
                                  alter_ctx->table_name,
                                  alter_ctx->fk_info,
                                  alter_ctx->fk_count,
                                  false))
      goto cleanup2;

    if (!(db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
    {
      /*
        Persist changes to data-dictionary for storage engines which don't
        support atomic DDL. Such SEs can't rollback in-place changes if error
        or crash happens after this point, so we are better to have
        data-dictionary in sync with SE.
      */
      Disable_gtid_state_update_guard disabler(thd);

      if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
        goto cleanup2;
    }

  }

#ifdef HAVE_PSI_TABLE_INTERFACE
  PSI_TABLE_CALL(drop_table_share)
    (true, alter_ctx->new_db, static_cast<int>(strlen(alter_ctx->new_db)),
     alter_ctx->tmp_name, static_cast<int>(strlen(alter_ctx->tmp_name)));
#endif

  DBUG_EXECUTE_IF("crash_after_index_create",
                  DBUG_SET("-d,crash_after_index_create");
                  DBUG_SUICIDE(););

  /*
    Tell the SE that the changed table in the data-dictionary.
    For engines which don't support atomic DDL this needs to be
    done before trying to rename the table.
  */
  if (!(db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
  {
    Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);

    table_list->mdl_request.ticket= mdl_ticket;
    if (open_table(thd, table_list, &ot_ctx))
      goto cleanup2;

    table_list->table->file->ha_notify_table_changed(ha_alter_info);

    /*
      We might be going to reopen table down on the road, so we have to
      restore state of the TABLE object which we used for obtaining of
      handler object to make it usable for later reopening.
    */
    DBUG_ASSERT(table_list->table == thd->open_tables);
    close_thread_table(thd, &thd->open_tables);
    table_list->table= NULL;

    /*
      Remove TABLE and TABLE_SHARE for from the TDC as we might have to
      rename table later.
    */
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL,
                     alter_ctx->db, alter_ctx->table_name, false);

  }

  // Rename altered table if requested.
  if (alter_ctx->is_table_renamed())
  {
    if (mysql_rename_table(thd, db_type, alter_ctx->db, alter_ctx->table_name,
                           alter_ctx->new_db, alter_ctx->new_alias,
                           ((db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) ?
                            NO_DD_COMMIT : 0)))
    {
      /*
        If the rename fails we will still have a working table
        with the old name, but with other changes applied.
      */
      goto cleanup2;
    }
  }

  THD_STAGE_INFO(thd, stage_end);

  DBUG_EXECUTE_IF("sleep_alter_before_main_binlog", my_sleep(6000000););
  DEBUG_SYNC(thd, "alter_table_before_main_binlog");

  ha_binlog_log_query(thd, ha_alter_info->create_info->db_type,
                      LOGCOM_ALTER_TABLE,
                      thd->query().str, thd->query().length,
                      alter_ctx->db, alter_ctx->table_name);

  DBUG_ASSERT(!(mysql_bin_log.is_open() &&
                thd->is_current_stmt_binlog_format_row() &&
                (ha_alter_info->create_info->options &
                 HA_LEX_CREATE_TMP_TABLE)));

  if (write_bin_log(thd, true, thd->query().str, thd->query().length,
                    (db_type->flags & HTON_SUPPORTS_ATOMIC_DDL)))
    goto cleanup2;

  {
    Uncommitted_tables_guard uncommitted_tables(thd);

    uncommitted_tables.add_table(table_list);

    bool views_err=
      (alter_ctx->is_table_renamed() ?
       update_referencing_views_metadata(thd, table_list,
                                         alter_ctx->new_db,
                                         alter_ctx->new_name,
                                         !(db_type->flags &
                                           HTON_SUPPORTS_ATOMIC_DDL),
                                         &uncommitted_tables) :
       update_referencing_views_metadata(thd, table_list,
                                         !(db_type->flags &
                                           HTON_SUPPORTS_ATOMIC_DDL),
                                         &uncommitted_tables));

    if (alter_ctx->is_table_renamed())
      tdc_remove_table(thd, TDC_RT_REMOVE_ALL,
                       alter_ctx->new_db, alter_ctx->new_name, false);

    if (views_err)
      goto cleanup2;
  }

  DEBUG_SYNC(thd, "action_after_write_bin_log");

  if (db_type->flags & HTON_SUPPORTS_ATOMIC_DDL)
  {
    /*
      Commit ALTER TABLE. Needs to be done here and not in the callers
      (which do it anyway) to be able notify SE about changed table.
    */
    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      goto cleanup2;

    /* Call SE DDL post-commit hook. */
    if (db_type->post_ddl)
      db_type->post_ddl(thd);

    /*
      Finally we can tell SE supporting atomic DDL that the changed table
      in the data-dictionary.
    */
    TABLE_LIST table_list;
    table_list.init_one_table(alter_ctx->new_db, strlen(alter_ctx->new_db),
                              alter_ctx->new_name, strlen(alter_ctx->new_name),
                              alter_ctx->new_alias, TL_READ);
    table_list.mdl_request.ticket= alter_ctx->is_table_renamed() ?
                                   target_mdl_request->ticket : mdl_ticket;

    Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);

    if (open_table(thd, &table_list, &ot_ctx))
      DBUG_RETURN(true);

    table_list.table->file->ha_notify_table_changed(ha_alter_info);

    DBUG_ASSERT(table_list.table == thd->open_tables);
    close_thread_table(thd, &thd->open_tables);
  }

  // TODO: May move the opening of the table and the call to
  //       ha_notify_table_changed() here to make sure we don't
  //       notify the handler until all meta data is complete.

  DBUG_RETURN(false);

rollback:
  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Table *old_table_def;
    thd->dd_client()->acquire<dd::Table>(table->s->db.str,
                                         table->s->table_name.str,
                                         &old_table_def);
    dd::Table *altered_table_def;
    thd->dd_client()->acquire_for_modification<dd::Table>(alter_ctx->new_db,
                                                          alter_ctx->tmp_name,
                                                          &altered_table_def);

    table->file->ha_commit_inplace_alter_table(altered_table,
                                               ha_alter_info,
                                               false, old_table_def,
                                               altered_table_def);
    thd->check_for_truncated_fields= CHECK_FIELD_IGNORE;
  }

cleanup:
  close_temporary_table(thd, altered_table, true, false);

cleanup2:

  (void) trans_rollback_stmt(thd);
  /*
    Full rollback in case we have THD::transaction_rollback_request
    and to synchronize DD state in cache and on disk (as statement
    rollback doesn't clear DD cache of modified uncommitted objects).
  */
  (void) trans_rollback(thd);

  if ((db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      db_type->post_ddl)
    db_type->post_ddl(thd);

  /*
    Re-opening of table needs to be done after rolling back the failed
    statement/transaction and clearing THD::transaction_rollback_request
    flag.
  */
  if (reopen_tables)
  {
    /* Close the only table instance which might be still around. */
    if (table)
      close_all_tables_for_name(thd, table->s, alter_ctx->is_table_renamed(), NULL);
    if (thd->locked_tables_list.reopen_tables(thd))
      thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);
    /* QQ; do something about metadata locks ? */
  }

  if (
#ifdef WORKAROUND_NEEDS_WL7141_TREE
      !(db_type->flags & HTON_SUPPORTS_ATOMIC_DDL)
#else
      true
#endif
     )
  {
    (void) dd::drop_table(thd, alter_ctx->new_db,
                          alter_ctx->tmp_name, true);
  }

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
       the auto_flags value.
    */

    if ((def->sql_type == MYSQL_TYPE_DATETIME ||
         def->sql_type == MYSQL_TYPE_TIMESTAMP)
        && (def->auto_flags != Field::NONE))
    {
      Item_func_now_local *now = new (thd->mem_root) Item_func_now_local(0);
      if (!now)
        DBUG_RETURN(true);

      if (def->auto_flags & Field::DEFAULT_NOW)
        default_value= now;
      if (def->auto_flags & Field::ON_UPDATE_NOW)
        update_value= now;
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
                 && def->sql_type != MYSQL_TYPE_TIMESTAMP)));
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
               ER_OLD_TEMPORALS_UPGRADED,
               ER_THD(thd, ER_OLD_TEMPORALS_UPGRADED));
  DBUG_RETURN(false);
}


static fk_option to_fk_option(dd::Foreign_key::enum_rule rule)
{
  switch (rule)
  {
  case dd::Foreign_key::enum_rule::RULE_NO_ACTION:
    return FK_OPTION_NO_ACTION;
  case dd::Foreign_key::enum_rule::RULE_RESTRICT:
    return FK_OPTION_RESTRICT;
  case dd::Foreign_key::enum_rule::RULE_CASCADE:
    return FK_OPTION_CASCADE;
  case dd::Foreign_key::enum_rule::RULE_SET_NULL:
    return FK_OPTION_SET_NULL;
  case dd::Foreign_key::enum_rule::RULE_SET_DEFAULT:
    return FK_OPTION_DEFAULT;
  }
  DBUG_ASSERT(false);
  return FK_OPTION_UNDEF;
}


static fk_match_opt to_fk_match_opt(dd::Foreign_key::enum_match_option match)
{
  switch (match)
  {
  case dd::Foreign_key::enum_match_option::OPTION_NONE:
    return FK_MATCH_SIMPLE;
  case dd::Foreign_key::enum_match_option:: OPTION_PARTIAL:
    return FK_MATCH_PARTIAL;
  case dd::Foreign_key::enum_match_option:: OPTION_FULL:
    return FK_MATCH_FULL;
  }
  DBUG_ASSERT(false);
  return FK_MATCH_UNDEF;
}


static void to_lex_cstring(MEM_ROOT *mem_root,
                           LEX_CSTRING *target,
                           const dd::String_type &source)
{
  target->str= strmake_root(mem_root, source.c_str(), source.length() + 1);
  target->length= source.length();
}


/**
  Remember information about pre-existing foreign keys so
  that they can be added to the new version of the table later.
  Also check that the foreign keys are still valid.

  @param      thd              Thread handle.
  @param      src_table        The source table.
  @param      alter_info       Info about ALTER TABLE statement.
  @param      alter_ctx        Runtime context for ALTER TABLE.
  @param      new_create_list  List of new columns, used for rename check.
*/

static bool transfer_preexisting_foreign_keys(
              THD *thd,
              const dd::Table *src_table,
              Alter_info *alter_info,
              Alter_table_ctx *alter_ctx,
              List<Create_field> *new_create_list)
{
  if (src_table == nullptr)
    return false; // Could be temporary table or during upgrade.

  List_iterator<Create_field> find_it(*new_create_list);

  alter_ctx->fk_info=
    (FOREIGN_KEY*)sql_calloc(sizeof(FOREIGN_KEY) *
                             src_table->foreign_keys().size());
  for (size_t i= 0; i < src_table->foreign_keys().size(); i++)
  {
    const dd::Foreign_key *dd_fk= src_table->foreign_keys()[i];

    // Skip foreign keys that are to be dropped
    bool is_dropped= false;
    for (const Alter_drop *drop : alter_info->drop_list)
    {
      // Index names are always case insensitive
      if (drop->type == Alter_drop::FOREIGN_KEY &&
          my_strcasecmp(system_charset_info,
                        drop->name,
                        dd_fk->name().c_str()) == 0)
      {
        is_dropped= true;
        break;
      }
    }
    if (is_dropped)
      continue;

    FOREIGN_KEY *sql_fk= &alter_ctx->fk_info[alter_ctx->fk_count++];

    // Remember the orignal name so we can set a temporary name to
    // avoid name conflicts between FKs in the old table and the new table.
    sql_fk->orig_name= strmake_root(thd->mem_root,
                                    dd_fk->name().c_str(),
                                    dd_fk->name().length() + 1);
    // Make a temporary, unique name from the FK id.
    std::string name("#fk_");
    name.append(std::to_string(dd_fk->id()));
    sql_fk->name= strmake_root(thd->mem_root,
                               name.c_str(),
                               name.length() + 1);

    // Note that we do not check if this FK still has a valid supporting
    // index here. This is done later in mysql_prepare_create_table()
    // after the updated index information has been prepared.
    // But we remember the name so we can use it for error reporting
    // in case the index has been dropped.
    sql_fk->unique_index_name=
      strmake_root(thd->mem_root,
                   dd_fk->unique_constraint().name().c_str(),
                   dd_fk->unique_constraint().name().length() + 1);

    sql_fk->key_parts= dd_fk->elements().size();

    to_lex_cstring(thd->mem_root, &sql_fk->ref_db,
                   dd_fk->referenced_table_schema_name());

    to_lex_cstring(thd->mem_root, &sql_fk->ref_table,
                   dd_fk->referenced_table_name());

    sql_fk->delete_opt= to_fk_option(dd_fk->delete_rule());
    sql_fk->update_opt= to_fk_option(dd_fk->update_rule());
    sql_fk->match_opt= to_fk_match_opt(dd_fk->match_option());

    sql_fk->key_part= (LEX_CSTRING*)sql_calloc(sizeof(LEX_CSTRING) *
                                               sql_fk->key_parts);
    sql_fk->fk_key_part= (LEX_CSTRING*)sql_calloc(sizeof(LEX_CSTRING)
                                                  * sql_fk->key_parts);

    for (size_t j= 0; j < sql_fk->key_parts; j++)
    {
      const dd::Foreign_key_element *dd_fk_ele= dd_fk->elements()[j];

      // Check if the column was renamed by the same statement.
      bool renamed= false;
      if (alter_info->flags & Alter_info::ALTER_CHANGE_COLUMN)
      {
        find_it.rewind();
        const Create_field *find;
        while ((find= find_it++) && !renamed)
        {
          if (find->change && my_strcasecmp(system_charset_info,
                                            dd_fk_ele->column().name().c_str(),
                                            find->change) == 0)
          {
            // Use new name
            sql_fk->key_part[j].str= find->field_name;
            sql_fk->key_part[j].length= strlen(find->field_name);
            renamed= true;
          }
        }
      }
      if (!renamed) // Use old name
        to_lex_cstring(thd->mem_root, &sql_fk->key_part[j],
                       dd_fk_ele->column().name());

      to_lex_cstring(thd->mem_root, &sql_fk->fk_key_part[j],
                     dd_fk_ele->referenced_column_name());
    }
  }
  return false;
}


/**
  Check if any foreign keys are defined using the given column
  which is about to be dropped. Report ER_FK_COLUMN_CANNOT_DROP
  if any such foreign key exists.

  @param src_table    Table with FKs to be checked.
  @param alter_info   Info about ALTER TABLE statement.
  @param field        Column to check.

  @retval true   A foreign key is using the column, error reported.
  @retval false  No foreign keys are using the column.
*/

static bool column_used_by_foreign_key(const dd::Table *src_table,
                                       Alter_info *alter_info,
                                       Field *field)
{
  if (src_table == nullptr)
    return false; // Could be temporary table or during upgrade.

  for (const dd::Foreign_key *dd_fk : src_table->foreign_keys())
  {
    // Skip foreign keys that are to be dropped
    bool is_dropped= false;
    for (const Alter_drop *drop : alter_info->drop_list)
    {
      // Index names are always case insensitive
      if (drop->type == Alter_drop::FOREIGN_KEY &&
          my_strcasecmp(system_charset_info,
                        drop->name,
                        dd_fk->name().c_str()) == 0)
      {
        is_dropped= true;
        break;
      }
    }
    if (is_dropped)
      continue;

    for (const dd::Foreign_key_element *dd_fk_ele : dd_fk->elements())
    {
      if (my_strcasecmp(system_charset_info,
                        dd_fk_ele->column().name().c_str(),
                        field->field_name) == 0)
      {
        my_error(ER_FK_COLUMN_CANNOT_DROP, MYF(0), field->field_name,
                 dd_fk->name().c_str());
        return true;
      }
    }
  }

  return false;
}


// Prepare Create_field and Key_spec objects for ALTER and upgrade.
bool prepare_fields_and_keys(THD *thd, TABLE *table,
                             HA_CREATE_INFO *create_info,
                             Alter_info *alter_info,
                             Alter_table_ctx *alter_ctx,
                             const uint &used_fields,
                             bool upgrade_flag)
{
  /* New column definitions are added here */
  List<Create_field> new_create_list;
  /* New key definitions are added here */
  Mem_root_array<const Key_spec*> new_key_list(thd->mem_root);
  // DROP instructions for foreign keys and virtual generated columns
  Mem_root_array<const Alter_drop*> new_drop_list(thd->mem_root);

  /*
    Alter_info::alter_rename_key_list is also used by fill_alter_inplace_info()
    call. So this function should not modify original list but rather work with
    its copy.
  */
  Prealloced_array<const Alter_rename_key*, 1>
    rename_key_list(alter_info->alter_rename_key_list);

  /*
    This is how we check that all indexes to be altered are name-resolved: We
    make a copy of the list from the alter_info, and remove all the indexes
    that are found in the table. Later we check that there is nothing left in
    the list. This is obviously just a copy-paste of what is done for renamed
    indexes.
  */
  Prealloced_array<const Alter_index_visibility*, 1>
    index_visibility_list(alter_info->alter_index_visibility_list);
  List_iterator<Create_field> def_it(alter_info->create_list);
  List_iterator<Create_field> find_it(new_create_list);
  List_iterator<Create_field> field_it(new_create_list);
  List<Key_part_spec> key_parts;
  KEY *key_info=table->key_info;

  DBUG_ENTER("prepare_fields_and_keys");

  restore_record(table, s->default_values);     // Empty record for DEFAULT
  Create_field *def;

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *src_table= nullptr;
  if (!table->s->tmp_table && !upgrade_flag)
  {
    if (thd->dd_client()->acquire(table->s->db.str, table->s->table_name.str,
                                  &src_table))
    {
      DBUG_RETURN(true);
    }
    // Should not happen, we know the table exists and can be opened.
    DBUG_ASSERT(src_table != nullptr);
  }

  /*
    First collect all fields from table which isn't in drop_list
  */
  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    /* Check if field should be dropped */
    size_t i= 0;
    while (i < alter_info->drop_list.size())
    {
      const Alter_drop *drop= alter_info->drop_list[i];
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(system_charset_info,field->field_name, drop->name))
      {
	/* Reset auto_increment value if it was dropped */
	if ((field->auto_flags & Field::NEXT_NUMBER) &&
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
          DBUG_RETURN(true);
        }

        if (column_used_by_foreign_key(src_table, alter_info, field))
          DBUG_RETURN(true);

        /*
          Mark the drop_column operation is on virtual GC so that a non-rebuild
          on table can be done.
        */
        if (field->is_virtual_gcol())
          new_drop_list.push_back(drop);
	break; // Column was found.
      }
      i++;
    }
    if (i < alter_info->drop_list.size())
    {
      alter_info->drop_list.erase(i);
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
        DBUG_RETURN(true);
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
      // Change default if ALTER
      size_t i= 0;
      const Alter_column *alter= NULL;
      while (i < alter_info->alter_list.size())
      {
        alter= alter_info->alter_list[i];
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
        i++;
      }
      if (i < alter_info->alter_list.size())
      {
	if (def->flags & BLOB_FLAG)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), field->field_name);
          DBUG_RETURN(true);
	}

	if ((def->def=alter->def))              // Use new default
        {
          def->flags&= ~NO_DEFAULT_VALUE_FLAG;
          /*
            The defaults are explicitly altered for the TIMESTAMP/DATETIME
            field, through SET DEFAULT. Hence, set the auto_flags member
            appropriately.
          */
          if (real_type_with_now_as_default(def->sql_type))
          {
            DBUG_ASSERT((def->auto_flags &
                         ~(Field::DEFAULT_NOW | Field::ON_UPDATE_NOW)) == 0);
            def->auto_flags&= ~Field::DEFAULT_NOW;
          }
        }
        else
          def->flags|= NO_DEFAULT_VALUE_FLAG;

	alter_info->alter_list.erase(i);
      }
    }
  }
  def_it.rewind();
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table->s->table_name.str);
      DBUG_RETURN(true);
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

    if (!def->after)
      new_create_list.push_back(def);
    else
    {
      const Create_field *find;
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
          DBUG_RETURN(true);
        }
        find_it.after(def);			// Put column after this
      }
    }
  }
  if (alter_info->alter_list.size() > 0)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             alter_info->alter_list[0]->name, table->s->table_name.str);
    DBUG_RETURN(true);
  }
  if (!new_create_list.elements)
  {
    my_error(ER_CANT_REMOVE_ALL_FIELDS, MYF(0));
    DBUG_RETURN(true);
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */

  for (uint i=0 ; i < table->s->keys ; i++,key_info++)
  {
    const char *key_name= key_info->name;
    bool index_column_dropped= false;
    size_t drop_idx= 0;
    while (drop_idx < alter_info->drop_list.size())
    {
      const Alter_drop *drop= alter_info->drop_list[drop_idx];
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(system_charset_info,key_name, drop->name))
	break;
      drop_idx++;
    }
    if (drop_idx < alter_info->drop_list.size())
    {
      alter_info->drop_list.erase(drop_idx);
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      const char *key_part_name=key_part->field->field_name;
      const Create_field *cfield;
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
             key_part->field->type() != MYSQL_TYPE_BLOB) ||
            (cfield->length && (((cfield->sql_type >= MYSQL_TYPE_TINY_BLOB &&
                                  cfield->sql_type <= MYSQL_TYPE_BLOB) ? 
                                blob_length_by_type(cfield->sql_type) :
                                cfield->length) <
	     key_part_length / key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(
        new Key_part_spec(to_lex_cstring(cfield->field_name),
                          key_part_length,
                          key_part->key_part_flag & HA_REVERSE_SORT ?
                            ORDER_DESC : ORDER_ASC));
    }
    if (key_parts.elements)
    {
      KEY_CREATE_INFO key_create_info(key_info->is_visible);

      keytype key_type;

      /* If this index is to stay in the table check if it has to be renamed. */
      for (size_t rename_idx= 0; rename_idx < rename_key_list.size(); rename_idx++)
      {
        const Alter_rename_key *rename_key= rename_key_list[rename_idx];
        if (! my_strcasecmp(system_charset_info, key_name,
                            rename_key->old_name))
        {
          if (! my_strcasecmp(system_charset_info, key_name,
                              primary_key_name))
          {
            my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), rename_key->old_name);
            DBUG_RETURN(true);
          }
          else if (! my_strcasecmp(system_charset_info, rename_key->new_name,
                                   primary_key_name))
          {
            my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), rename_key->new_name);
            DBUG_RETURN(true);
          }

          key_name= rename_key->new_name;
          rename_key_list.erase(rename_idx);
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

      // Erase all alter operations that operate on this index.
      for (auto it= index_visibility_list.begin();
           it < index_visibility_list.end();)
        if (my_strcasecmp(system_charset_info, key_name, (*it)->name()) == 0)
          index_visibility_list.erase(it);
        else
          ++it;

      if (key_info->is_algorithm_explicit)
      {
        key_create_info.algorithm= key_info->algorithm;
        key_create_info.is_algorithm_explicit= true;
      }
      else
      {
        /*
          If key algorithm was not specified explicitly for source table
          don't specify one a new version as well, This allows to handle
          ALTER TABLEs which change SE nicely.
          OTOH this means that any ALTER TABLE will rebuild such keys when
          SE changes default algorithm for key. Code will have to be adjusted
          to handle such situation more gracefully.
        */
        DBUG_ASSERT((key_create_info.is_algorithm_explicit == false) &&
                    (key_create_info.algorithm == HA_KEY_ALG_SE_SPECIFIC));
      }

      if (key_info->flags & HA_USES_BLOCK_SIZE)
        key_create_info.block_size= key_info->block_size;
      if (key_info->flags & HA_USES_PARSER)
        key_create_info.parser_name=
          to_lex_cstring(*plugin_name(key_info->parser));
      if (key_info->flags & HA_USES_COMMENT)
        key_create_info.comment= key_info->comment;

      for (const Alter_index_visibility *alter_index_visibility :
             alter_info->alter_index_visibility_list)
      {
        const char *name= alter_index_visibility->name();
        if (my_strcasecmp(system_charset_info, key_name, name) == 0)
        {
          if (table->s->primary_key <= MAX_KEY &&
              table->key_info + table->s->primary_key == key_info)
          {
            my_error(ER_PK_INDEX_CANT_BE_INVISIBLE, MYF(0));
            DBUG_RETURN(true);
          }
          key_create_info.is_visible= alter_index_visibility->is_visible();
        }
      }

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

      /*
        If we have dropped a column associated with an index,
        this warrants a check for duplicate indexes
      */
      new_key_list.push_back(new Key_spec(thd->mem_root, key_type,
                                          to_lex_cstring(key_name),
                                          &key_create_info,
                                          MY_TEST(key_info->flags & HA_GENERATED_KEY),
                                          index_column_dropped,
                                          key_parts));
    }
  }
  {
    new_key_list.reserve(new_key_list.size() + alter_info->key_list.size());
    for (size_t i= 0; i < alter_info->key_list.size(); i++)
      new_key_list.push_back(alter_info->key_list[i]); // Add new keys
  }

  if (alter_info->drop_list.size() > 0)
  {
    // Now this contains only DROP for foreign keys and not-found objects
    for (const Alter_drop *drop : alter_info->drop_list)
    {
      switch (drop->type) {
      case Alter_drop::KEY:
      case Alter_drop::COLUMN:
        my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
                 alter_info->drop_list[0]->name);
        DBUG_RETURN(true);
      case Alter_drop::FOREIGN_KEY:
        break;
      default:
        DBUG_ASSERT(false);
        break;
      }
    }
    // new_drop_list has DROP for virtual generated columns; add foreign keys:
    new_drop_list.reserve(new_drop_list.size() + alter_info->drop_list.size());
    for (const Alter_drop *drop : alter_info->drop_list)
      new_drop_list.push_back(drop);
  }

  /*
    Remember information about pre-existing triggers so
    that they can be added to the new version of the table later.
  */
  if (src_table != nullptr && src_table->has_trigger())
    src_table->clone_triggers(&alter_ctx->trg_info);

  /*
    Copy existing foreign keys from the source table into
    Alter_table_ctx so that they can be added to the new table
    later. Also checks that these foreign keys are still valid.
  */
  if (transfer_preexisting_foreign_keys(thd, src_table, alter_info,
                                        alter_ctx, &new_create_list))
    DBUG_RETURN(true);

  if (rename_key_list.size() > 0)
  {
    my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), rename_key_list[0]->old_name,
             table->s->table_name.str);
    DBUG_RETURN(true);
  }
  if (index_visibility_list.size() > 0)
  {
    my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), index_visibility_list[0]->name(),
             table->s->table_name.str);
    DBUG_RETURN(true);
  }


  alter_info->create_list.swap(new_create_list);
  alter_info->key_list.clear();
  alter_info->key_list.resize(new_key_list.size());
  std::copy(new_key_list.begin(), new_key_list.end(), alter_info->key_list.begin());
  alter_info->drop_list.clear();
  alter_info->drop_list.resize(new_drop_list.size());
  std::copy(new_drop_list.begin(), new_drop_list.end(), alter_info->drop_list.begin());

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
  uint db_create_options= (table->s->db_create_options
                           & ~(HA_OPTION_PACK_RECORD));
  uint used_fields= create_info->used_fields;

  DBUG_ENTER("mysql_prepare_alter_table");

  // Prepare data in HA_CREATE_INFO shared by ALTER and upgrade code.
  create_info->init_create_options_from_share(table->s, used_fields);

  if (!(used_fields & HA_CREATE_USED_AUTO) && table->found_next_number_field)
  {
    /* Table has an autoincrement, copy value to new table */
    table->file->info(HA_STATUS_AUTO);
    create_info->auto_increment_value= table->file->stats.auto_increment_value;
  }

  if (prepare_fields_and_keys(thd, table, create_info,
                              alter_info, alter_ctx,
                              used_fields, false))
    DBUG_RETURN(true);

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

  DBUG_RETURN(false);
}


/**
  Get Create_field object for newly created table by its name
  in the old version of table.

  @param alter_info  Alter_info describing newly created table.
  @param old_name    Name of field in old table.

  @returns Pointer to Create_field object, NULL - if field is
           not present in new version of table.
*/

static const Create_field *get_field_by_old_name(Alter_info *alter_info,
                                                 const char *old_name)
{
  List_iterator_fast<Create_field> new_field_it(alter_info->create_list);
  const Create_field *new_field;

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
    const Create_field *new_field= get_field_by_old_name(alter_info, column->str);

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
    for (const Alter_drop *drop : alter_info->drop_list)
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
               ER_THD(thd, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME),
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
    for (const Alter_drop *drop : alter_info->drop_list)
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
               ER_THD(thd, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_RENAME),
               "ALGORITHM=INPLACE");
      DBUG_RETURN(true);
    case FK_COLUMN_DROPPED:
      // Should already have been checked in column_used_by_foreign_key().
      DBUG_ASSERT(false);
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
  handlerton *old_db_type= table->s->db_type();
  bool atomic_ddl= (old_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL);

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

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
                          ER_ILLEGAL_HA, ER_THD(thd, ER_ILLEGAL_HA),
                          table->alias);
      error= 0;
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    else
    {
      /**
        Update mysql.tables.options with keys_disabled=1/0 based on keys_onoff.
        This will used by INFORMATION_SCHEMA.STATISTICS system view to display
        keys were disabled.
       */
      if (dd::update_keys_disabled(thd, table_list->db, table_list->table_name,
                                   keys_onoff, !atomic_ddl))
      {
        // Error should have been reported by DD layer already.
        error= -1;
      }
    }
  }

  if (!error && alter_ctx->is_table_renamed())
  {
    THD_STAGE_INFO(thd, stage_rename);
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

    if (mysql_rename_table(thd, old_db_type,
                           alter_ctx->db, alter_ctx->table_name,
                           alter_ctx->new_db, alter_ctx->new_alias,
                           (atomic_ddl ? NO_DD_COMMIT: 0)))
      error= -1;
  }

  if (!error)
  {
    error= write_bin_log(thd, true, thd->query().str, thd->query().length,
                         atomic_ddl &&
                         (keys_onoff != Alter_info::LEAVE_AS_IS ||
                          alter_ctx->is_table_renamed()));

    // Update referencing views metadata.
    if (!error)
    {
      Uncommitted_tables_guard uncommitted_tables(thd);

      error= update_referencing_views_metadata(thd, table_list,
                                               alter_ctx->new_db,
                                               alter_ctx->new_alias,
                                               !atomic_ddl,
                                               &uncommitted_tables);

      if (alter_ctx->is_table_renamed())
      {
        uncommitted_tables.add_table(table_list);
        tdc_remove_table(thd, TDC_RT_REMOVE_ALL, alter_ctx->new_db,
                         alter_ctx->new_name, false);
      }
    }

    /*
      Commit changes to data-dictionary, SE and binary log if it was not done
      earlier. We need to do this before releasing/downgrading MDL.
    */
    if (!error && atomic_ddl)
      error= (trans_commit_stmt(thd) || trans_commit_implicit(thd));


    if (!error)
      my_ok(thd);
  }

  if (error)
  {
    /*
      We need rollback possible changes to data-dictionary before releasing
      or downgrading metadata lock.

      Full rollback will synchronize state of data-dictionary in
      cache and on disk. Also it is  needed in case we have
      THD::transaction_rollback_request.
    */
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  }

  if (atomic_ddl && old_db_type->post_ddl)
    old_db_type->post_ddl(thd);

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
  DBUG_ENTER("mysql_alter_table");

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
    Reject invalid name lengths. Names will be validated after the table is
    opened and the SE (needed for SE specific validation) is identified.
  */
  if (create_info->tablespace)
  {
    if (validate_tablespace_name_length(create_info->tablespace))
      DBUG_RETURN(true);

    if (!thd->make_lex_string(&table_list->target_tablespace_name,
                              create_info->tablespace,
                              strlen(create_info->tablespace), false))
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      DBUG_RETURN(true);
    }
  }

  /*
    Reject invalid tablespace name lengths specified for partitions.
    Names will be validated after the table has been opened.
  */
  if (validate_partition_tablespace_name_lengths(thd->lex->part_info))
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
  table_list->required_type= dd::enum_table_type::BASE_TABLE;

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

  // Check tablespace name validity for the relevant engine.
  {
    // If there is no target handlerton, use the current.
    const handlerton* target_handlerton= create_info->db_type;
    if (target_handlerton == nullptr)
      target_handlerton= table_list->table->file->ht;

    /*
      Reject invalid tablespace names for the relevant engine, if the ALTER
      statement changes either tablespace or engine. We do this after the table
      has been opened because we need the handlerton and tablespace information.
      No need to validate if neither engine nor tablespace is changed, then the
      validation was done when the table was created.
    */
    if (create_info->tablespace || create_info->db_type)
    {
      // If there is no target table level tablespace, use the current.
      const char* target_tablespace= create_info->tablespace;
      if (target_tablespace == nullptr)
        target_tablespace= table_list->table->s->tablespace;

      // Check the tablespace/engine combination.
      DBUG_ASSERT(target_handlerton);
      if (target_tablespace != nullptr &&
          validate_tablespace_name(false,
                                   target_tablespace,
                                   target_handlerton))
        DBUG_RETURN(true);
    }

    // Reject invalid tablespace names specified for partitions.
    if (validate_partition_tablespace_names(thd->lex->part_info,
                                            target_handlerton))
      DBUG_RETURN(true);
  }

  if (lock_trigger_names(thd, table_list))
    DBUG_RETURN(true);

  // Check if ALTER TABLE ... ENGINE is disallowed by the storage engine.
  if (table_list->table->s->db_type() != create_info->db_type &&
      (alter_info->flags & Alter_info::ALTER_OPTIONS) &&
      (create_info->used_fields & HA_CREATE_USED_ENGINE) &&
       ha_is_storage_engine_disabled(create_info->db_type))
  {
    my_error(ER_DISABLED_STORAGE_ENGINE, MYF(0),
              ha_resolve_storage_engine_name(create_info->db_type));
    DBUG_RETURN(true);
  }

  TABLE *table= table_list->table;
  table->use_all_columns();
  MDL_ticket *mdl_ticket= table->mdl_ticket;

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
    Acquire and keep schema locks until commit time, so the DD layer can
    safely assert that we have proper MDL on objects stored in the DD.
  */
  dd::Schema_MDL_locker mdl_locker_1(thd), mdl_locker_2(thd);
  /*
    This releaser allows us to keep uncommitted DD objects cached
    in the Dictionary_client until commit time.
  */
  dd::cache::Dictionary_client::Auto_releaser releaser2(thd->dd_client());
  if (mdl_locker_1.ensure_locked(alter_ctx.db) ||
      mdl_locker_2.ensure_locked(alter_ctx.new_db))
    DBUG_RETURN(true);

  /*
    Add old and new (if any) databases to the list of accessed databases
    for this statement. Needed for MTS.
  */
  thd->add_to_binlog_accessed_dbs(alter_ctx.db);
  if (alter_ctx.is_database_changed())
    thd->add_to_binlog_accessed_dbs(alter_ctx.new_db);

  // Ensure that triggers are in the same schema as their subject table.
  if (alter_ctx.is_database_changed())
  {
    bool table_has_trigger= false;
    if (table->s->tmp_table == NO_TMP_TABLE &&
        dd::table_has_triggers(thd, alter_ctx.db, alter_ctx.table_name,
                               &table_has_trigger))
      DBUG_RETURN(true);

    if (table_has_trigger)
    {
      my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
      DBUG_RETURN(true);
    }
  }

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
      bool exists;
      if (dd::table_exists<dd::Abstract_table>(thd->dd_client(),
                                               alter_ctx.new_db,
                                               alter_ctx.new_name,
                                               &exists))
        DBUG_RETURN(true);

      if (exists)
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
      check_fk_parent_table_access(thd, create_info, alter_info))
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
    DBUG_RETURN(simple_rename_or_index_change(thd, table_list,
					     alter_info->keys_onoff,
					     &alter_ctx));
  }

  /* We have to do full alter table. */

  bool partition_changed= false;
  bool fast_alter_part_table= false;
  partition_info *new_part_info= NULL;
  {
    if (prep_alter_part_table(thd, table, alter_info, create_info,
                              &alter_ctx, &partition_changed,
                              &fast_alter_part_table, &new_part_info))
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

  if (set_table_default_charset(thd, create_info, alter_ctx.db))
    DBUG_RETURN(true);

  if (fast_alter_part_table)
  {
    /*
      ALGORITHM and LOCK clauses are generally not allowed by the
      parser for operations related to partitioning.
      The exceptions are ALTER_PARTITION and ALTER_REMOVE_PARTITIONING.
      For consistency, we report ER_ALTER_OPERATION_NOT_SUPPORTED here.
    */
    if (alter_info->requested_lock !=
        Alter_info::ALTER_TABLE_LOCK_DEFAULT)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "LOCK=NONE/SHARED/EXCLUSIVE",
               ER_THD(thd, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION),
               "LOCK=DEFAULT");
      DBUG_RETURN(true);
    }
    else if (alter_info->requested_algorithm !=
             Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT)
    {
      my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
               "ALGORITHM=COPY/INPLACE",
               ER_THD(thd, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_PARTITION),
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
    // In-place execution of ALTER TABLE for partitioning.

    /*
      TODO: The below legacy code is to be removed once InnoDB supports
            changes to partitioning through normal in-place ALTER SE
            API. So we don't care about atomicity and commit/rollback
            correctness in this case.
    */
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
    - Changes to partitioning which were not handled by fast_alter_part_table()
      needs to be handled using table copying algorithm unless the engine
      supports auto-partitioning (as such engines can do some changes
      using in-place API) or engine supports partitioning changes through
      in-place API but still relies on mark-up in partition_info object.
  */
  if ((thd->variables.old_alter_table &&
       alter_info->requested_algorithm !=
       Alter_info::ALTER_TABLE_ALGORITHM_INPLACE)
      || is_inplace_alter_impossible(table, create_info, alter_info)
      || (partition_changed &&
          !(table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION) &&
          !new_part_info)
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
  FOREIGN_KEY *fk_key_info= NULL;
  uint fk_key_count= 0;

  /*
    dd::Table object describing new version of temporary table. This object will
    be created in memory later in create_table_impl() and will not be put into
    the DD Object Cache.

    We become responsible for destroying this dd::Table object until we pass its
    ownership to the TABLE_SHARE of the temporary table.
  */
  dd::Table *tmp_table_def;
  Alter_info::enum_enable_or_disable keys_onoff=
    ((alter_info->keys_onoff == Alter_info::LEAVE_AS_IS &&
      table->file->indexes_are_disabled()) ? Alter_info::DISABLE :
     alter_info->keys_onoff);

  /*
    Take the X metadata lock on temporary name used for new version of
    the table. This ensures that concurrent I_S queries won't try to open it.
  */

  MDL_request tmp_name_mdl_request;
  bool is_tmp_table= (table->s->tmp_table != NO_TMP_TABLE);

  // Avoid these tables to be visible by I_S/SHOW queries.
  create_info->m_hidden= !is_tmp_table;

  if (!is_tmp_table)
  {
    MDL_REQUEST_INIT(&tmp_name_mdl_request,
                     MDL_key::TABLE,
                     alter_ctx.new_db, alter_ctx.tmp_name,
                     MDL_EXCLUSIVE, MDL_STATEMENT);
    if (thd->mdl_context.acquire_lock(&tmp_name_mdl_request,
                                      thd->variables.lock_wait_timeout))
      DBUG_RETURN(true);
  }

  tmp_disable_binlog(thd);

  error= create_table_impl(thd, alter_ctx.new_db, alter_ctx.tmp_name,
                           alter_ctx.table_name,
                           alter_ctx.get_tmp_path(),
                           create_info, alter_info,
                           true, 0, true, NULL,
                           &key_info, &key_count, keys_onoff,
                           &fk_key_info, &fk_key_count,
                           alter_ctx.fk_info, alter_ctx.fk_count,
                           &tmp_table_def, nullptr);

  reenable_binlog(thd);

  if (error)
  {
    /*
      Play it safe, rollback possible changes to the data-dictionary,
      so failed mysql_alter_table()/mysql_recreate_table() do not
      require rollback in the caller. Also do full rollback in unlikely
      case we have THD::transaction_rollback_request.
    */
    trans_rollback_stmt(thd);
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  /*
    Atomic replacement of the table is possible only if both old and new
    storage engines support DDL atomicity.
  */
  bool atomic_replace= (new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
                       (old_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL);

  /* Remember that we have not created table in storage engine yet. */
  bool no_ha_table= true;

  /* Indicates special case when we do ALTER TABLE which is really no-op. */
  bool is_noop= false;

  if (alter_info->requested_algorithm != Alter_info::ALTER_TABLE_ALGORITHM_COPY)
  {
    Alter_inplace_info ha_alter_info(create_info, alter_info,
                                     key_info, key_count,
                                     thd->work_part_info
                                     );
    TABLE *altered_table= NULL;
    bool use_inplace= true;

    /* Fill the Alter_inplace_info structure. */
    if (fill_alter_inplace_info(thd, table, &ha_alter_info))
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
                                             true, false,
                                             nullptr)))
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

      if (!(create_info->db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
        // Delete temporary table object from data dictionary.
        (void) dd::drop_table(thd, alter_ctx.new_db,
                              alter_ctx.tmp_name, true);
      else
      {
        dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
        const dd::Table *tab_obj= nullptr;
        if (thd->dd_client()->acquire<dd::Table>(alter_ctx.new_db,
                                                 alter_ctx.tmp_name,
                                                 &tab_obj))
          goto err_new_table_cleanup;

        DBUG_ASSERT(tab_obj);

        /*
          We need to revert changes to data-dictionary but
          still commit statement in this case.
        */
        if (thd->dd_client()->drop(tab_obj))
          goto err_new_table_cleanup;
      }
      is_noop= true;
      goto end_inplace_noop;
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
               ER_THD(thd, ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COPY),
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

  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    dd::Table *table_def= tmp_table_def;
    if (!table_def)
    {
      if (thd->dd_client()->acquire_for_modification<dd::Table>(
                              alter_ctx.new_db, alter_ctx.tmp_name,
                              &table_def))
        goto err_new_table_cleanup;

      DBUG_ASSERT(table_def);
    }

    if (ha_create_table(thd, alter_ctx.get_tmp_path(),
                        alter_ctx.new_db, alter_ctx.tmp_name,
                        create_info, false, false, table_def))
      goto err_new_table_cleanup;

    /* Mark that we have created table in storage engine. */
    no_ha_table= false;

    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    {
      if (!open_table_uncached(thd, alter_ctx.get_tmp_path(),
                               alter_ctx.new_db, alter_ctx.tmp_name,
                               true, true, table_def))
        goto err_new_table_cleanup;
      /* in case of alter temp table send the tracker in OK packet */
      if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
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
      /* Transfer dd::Table ownership to temporary table's share. */
      new_table->s->tmp_table_def= tmp_table_def;
      tmp_table_def= NULL;
    }
    else
    {
      /* table is a normal table: Create temporary table in same directory */
      /* Open our intermediate table. */
      new_table= open_table_uncached(thd, alter_ctx.get_tmp_path(),
                                     alter_ctx.new_db, alter_ctx.tmp_name,
                                     true, true, table_def);
    }
    if (!new_table)
      goto err_new_table_cleanup;
    /*
      Note: In case of MERGE table, we do not attach children. We do not
      copy data for MERGE tables. Only the children have data.
    */

    // It's now safe to take the table level lock.
    if (lock_tables(thd, table_list, alter_ctx.tables_opened, 0))
      goto err_new_table_cleanup;
  }

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

    if (copy_data_between_tables(thd,
                                 thd->m_stage_progress_psi,
                                 table, new_table,
                                 alter_info->create_list,
                                 &copied, &deleted,
                                 alter_info->keys_onoff,
                                 &alter_ctx))
      goto err_new_table_cleanup;

    DEBUG_SYNC(thd, "alter_after_copy_table");
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
    alter_table_manage_keys(thd, table, table->file->indexes_are_disabled(),
                            alter_info->keys_onoff);
    DBUG_ASSERT(!(new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL));
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
    /*
      We don't replicate alter table statement on temporary tables
      in RBR mode.
    */
    if (!thd->is_current_stmt_binlog_format_row() &&
        write_bin_log(thd, true, thd->query().str, thd->query().length))
    {
      /*
        We can't revert replacement of old table version with a new one
        at this point. So, if possible, commit the statement to avoid
        new table version being emptied by statement rollback.
      */
      if (!thd->transaction_rollback_request)
      {
        (void) trans_commit_stmt(thd);
        (void) trans_commit_implicit(thd);
      }
      DBUG_RETURN(true);
    }

    // Do implicit commit for consistency with non-temporary table case/
    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      DBUG_RETURN(true);

    goto end_temporary;
  }

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

  /*
    To ensure DDL atomicity after this point support from both old and
    new engines is necessary. If either of them lacks such support let
    us commit transaction so changes to data-dictionary are more closely
    reflect situations in SEs.
  */
  if (!atomic_replace)
  {
    Disable_gtid_state_update_guard disabler(thd);

    if (trans_commit_stmt(thd) || trans_commit_implicit(thd))
      goto err_new_table_cleanup;
  }


  char backup_name[32];
  DBUG_ASSERT(sizeof(my_thread_id) == 4);
  my_snprintf(backup_name, sizeof(backup_name), "%s2-%lx-%lx", tmp_file_prefix,
              current_pid, thd->thread_id());
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, backup_name);

  close_all_tables_for_name(thd, table->s, alter_ctx.is_table_renamed(), NULL);
  table_list->table= table= NULL;                  /* Safety */

  /*
    Rename the old version to temporary name to have a backup in case
    anything goes wrong while renaming the new table.

    Take the X metadata lock on this temporary name too. This ensures that
    concurrent I_S queries won't try to open it. Assert to ensure we do not
    come here when ALTERing temporary table.
  */
  {
    DBUG_ASSERT(!is_tmp_table);
    MDL_request backup_name_mdl_request;
    MDL_REQUEST_INIT(&backup_name_mdl_request,
                     MDL_key::TABLE,
                     alter_ctx.db, backup_name,
                     MDL_EXCLUSIVE, MDL_STATEMENT);
    bool backup_exists;

    if (thd->mdl_context.acquire_lock(&backup_name_mdl_request,
                                    thd->variables.lock_wait_timeout) ||
        dd::table_exists<dd::Abstract_table>(thd->dd_client(),
                                             alter_ctx.db, backup_name,
                                             &backup_exists))

    {
      /* purecov: begin tested */
      /*
        We need to clear THD::transaction_rollback_request (which might
        be set due to MDL deadlock) before attempting to remove new version
        of table.
      */
      if (thd->transaction_rollback_request)
      {
        trans_rollback_stmt(thd);
        trans_rollback(thd);
      }

      if (!atomic_replace)
      {
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
      }
#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL7016
      else
      {
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
      }
#endif
      goto err_with_mdl;
      /* purecov: end */
    }

    if (backup_exists)
    {
      /* purecov: begin tested */
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), backup_name);

      if (!atomic_replace)
      {
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
      }
#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL7016
      else
      {
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
      }
#endif
      goto err_with_mdl;
      /* purecov: end */
    }
  }

  if (mysql_rename_table(thd, old_db_type, alter_ctx.db, alter_ctx.table_name,
                         alter_ctx.db, backup_name,
                         FN_TO_IS_TMP | (atomic_replace ? NO_DD_COMMIT : 0)))
  {
    // Rename to temporary name failed, delete the new table, abort ALTER.
    if (!atomic_replace)
    {
      /*
        In non-atomic mode situations when the SE has requested rollback
        should be handled already, by executing rollback right inside
        mysql_rename_table() call.
      */
      DBUG_ASSERT(!thd->transaction_rollback_request);
      (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                            alter_ctx.tmp_name, FN_IS_TMP);
    }
#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL7016
    else
    {
      /*
        We should not try to remove new version of the table if
        THD::transaction_rollback_request is set and we can't rollback the
        transaction before removal, as it will wipe-out information which is
        needed by InnoDB. To keep things simple we just ignore the problem
        for now and let the inconsistency to creep in.
      */
      if (! thd->transaction_rollback_request)
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
    }
#endif
    goto err_with_mdl;
  }

  // Rename the new table to the correct name.
  if (mysql_rename_table(thd, new_db_type, alter_ctx.new_db, alter_ctx.tmp_name,
                         alter_ctx.new_db, alter_ctx.new_alias,
                         (FN_FROM_IS_TMP |
                          (atomic_replace ? NO_DD_COMMIT : 0))))
  {
    // Rename failed, delete the temporary table.
    if (!atomic_replace)
    {
      /*
        In non-atomic mode situations when the SE has requested rollback
        should be handled already, by executing rollback right inside
        mysql_rename_table() call.
      */
      DBUG_ASSERT(!thd->transaction_rollback_request);

      (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                            alter_ctx.tmp_name, FN_IS_TMP);

      // Restore the backup of the original table to its original name.
      // If the operation fails, we need to retry it to avoid leaving
      // the dictionary inconsistent.
      //
      // This hack might become unnecessary once InnoDB stops acquiring
      // gap locks on DD tables (which might cause deadlocks).
      uint retries= 20;
      while (retries-- &&
             mysql_rename_table(thd, old_db_type, alter_ctx.db, backup_name,
                                alter_ctx.db, alter_ctx.alias,
                                FN_FROM_IS_TMP | NO_FK_CHECKS));
    }
#ifndef WORKAROUND_TO_BE_REMOVED_BY_WL7016
    else
    {
      /*
        We should not try to restore status quo ante if
        THD::transaction_rollback_request is set and we can't rollback the
        transaction before, as it will wipe-out information which is needed
        by InnoDB. To keep things simple we just ignore the problem for now
        and let the inconsistency to creep in.
      */
      if (! thd->transaction_rollback_request)
      {
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
        (void) mysql_rename_table(thd, old_db_type, alter_ctx.db, backup_name,
                                  alter_ctx.db, alter_ctx.alias,
                                  FN_FROM_IS_TMP | NO_FK_CHECKS);
      }
    }
#endif

    goto err_with_mdl;
  }

  // ALTER TABLE succeeded, delete the backup of the old table.
  if (quick_rm_table(thd, old_db_type, alter_ctx.db, backup_name,
                     FN_IS_TMP | (atomic_replace ? NO_DD_COMMIT : 0)))
  {
    /*
      The fact that deletion of the backup failed is not critical
      error, but still worth reporting as it might indicate serious
      problem with server.
    */
    goto err_with_mdl;
  }

  /*
    Transfer pre-existing triggers to the new table.
    Since trigger names have to be unique per schema, we cannot
    create them while both the old and the tmp version of the
    table exist.
  */
  if (!alter_ctx.trg_info.empty() &&
      dd::add_triggers(thd, alter_ctx.new_db, alter_ctx.new_alias,
                       &alter_ctx.trg_info, !atomic_replace))
    goto err_with_mdl;

  /*
    Rename pre-existing foreign keys back to their original names.
    Since foreign key names have to be unique per schema, they cannot
    have the same name in both the old and the temp version of the
    table definition. Since we now have only one defintion, the names
    can be restored.
  */
  if (alter_ctx.fk_count > 0 &&
      restore_foreign_key_names(thd,
                                alter_ctx.new_db,
                                alter_ctx.new_alias,
                                alter_ctx.fk_info,
                                alter_ctx.fk_count,
                                !atomic_replace))
    goto err_with_mdl;

end_inplace_noop:

  THD_STAGE_INFO(thd, stage_end);

  DBUG_EXECUTE_IF("sleep_alter_before_main_binlog", my_sleep(6000000););
  DEBUG_SYNC(thd, "alter_table_before_main_binlog");

  ha_binlog_log_query(thd, create_info->db_type, LOGCOM_ALTER_TABLE,
                      thd->query().str, thd->query().length,
                      alter_ctx.db, alter_ctx.table_name);

  DBUG_ASSERT(!(mysql_bin_log.is_open() &&
                thd->is_current_stmt_binlog_format_row() &&
                (create_info->options & HA_LEX_CREATE_TMP_TABLE)));
  if (write_bin_log(thd, true, thd->query().str, thd->query().length,
                    atomic_replace))
    goto err_with_mdl;

  if (!is_noop)
  {
    Uncommitted_tables_guard uncommitted_tables(thd);

    uncommitted_tables.add_table(table_list);

    if (update_referencing_views_metadata(thd, table_list,
                                          new_db, new_name,
                                          !atomic_replace,
                                          &uncommitted_tables))
      goto err_with_mdl;

    if (alter_ctx.is_table_renamed())
      tdc_remove_table(thd, TDC_RT_REMOVE_ALL,
                       alter_ctx.new_db, alter_ctx.new_name, false);
  }

  // Commit if it was not done before in order to be able to reopen tables.
  if (atomic_replace &&
      (trans_commit_stmt(thd) || trans_commit_implicit(thd)))
    goto err_with_mdl;

  if ((new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      new_db_type->post_ddl)
    new_db_type->post_ddl(thd);
  if ((old_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      old_db_type->post_ddl)
    old_db_type->post_ddl(thd);

end_inplace:

  if (thd->locked_tables_list.reopen_tables(thd))
    goto err_with_mdl;

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
  my_snprintf(alter_ctx.tmp_name, sizeof(alter_ctx.tmp_name),
              ER_THD(thd, ER_INSERT_INFO),
	      (long) (copied + deleted), (long) deleted,
	      (long) thd->get_stmt_da()->current_statement_cond_count());
  my_ok(thd, copied + deleted, 0L, alter_ctx.tmp_name);
  DBUG_RETURN(false);

err_new_table_cleanup:
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (new_table)
      close_temporary_table(thd, new_table, true, true);
    else if (!no_ha_table)
      rm_temporary_table(thd, new_db_type, alter_ctx.get_tmp_path(),
                         tmp_table_def);
    delete tmp_table_def;
  }
  else
  {
    DBUG_ASSERT(tmp_table_def == nullptr);
    /* close_temporary_table() frees the new_table pointer. */
    if (new_table)
      close_temporary_table(thd, new_table, true, false);

    if (!(new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL))
    {
      if (no_ha_table) // Only remove from DD.
        (void) dd::drop_table(thd, alter_ctx.new_db,
                              alter_ctx.tmp_name, true);
      else // Remove from both DD and SE.
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);
    }
    else
    {
#ifndef WORKAROUND_UNTIL_WL7016_IS_IMPLEMENTED
      /*
        We should not try to remove new version of the table if
        THD::transaction_rollback_request is set and we can't rollback the
        transaction before removal, as it will wipe-out information which is
        needed by InnoDB. To keep things simple we just ignore the problem
        for now and let the inconsistency to creep in.
      */
      if (! thd->transaction_rollback_request && ! no_ha_table)
        // Remove from both DD and SE.
        (void) quick_rm_table(thd, new_db_type, alter_ctx.new_db,
                              alter_ctx.tmp_name, FN_IS_TMP);

#endif
      trans_rollback_stmt(thd);
      /*
        Full rollback in case we have THD::transaction_rollback_request
        and to synchronize DD state in cache and on disk (as statement
        rollback doesn't clear DD cache of modified uncommitted objects).
      */
      trans_rollback(thd);
    }
    if ((new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
        new_db_type->post_ddl)
      new_db_type->post_ddl(thd);
  }

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
  /*
    An error happened while we were holding exclusive name metadata lock
    on table being altered. To be safe under LOCK TABLES we should
    remove all references to the altered table from the list of locked
    tables and release the exclusive metadata lock. Before releasing locks
    we need to rollback changes to the data-dictionary, storage angine
    and binary log (if they were not committed earlier) and execute
    post DDL hooks.
  */
  thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);

  trans_rollback_stmt(thd);
  /*
    Full rollback in case we have THD::transaction_rollback_request
    and to synchronize DD state in cache and on disk (as statement
    rollback doesn't clear DD cache of modified uncommitted objects).
  */
  trans_rollback(thd);
  if ((new_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      new_db_type->post_ddl)
    new_db_type->post_ddl(thd);
  if ((old_db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
      old_db_type->post_ddl)
    old_db_type->post_ddl(thd);

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
  Disable_gtid_state_update_guard disabler(thd);

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
  /*
    Ensure that ha_commit_trans() which is implicitly called by
    ha_enable_transaction() doesn't update GTID and slave info states.
  */
  Disable_gtid_state_update_guard disabler(thd);
  if (ha_enable_transaction(thd, TRUE))
    DBUG_RETURN(TRUE);

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
copy_data_between_tables(THD * thd,
                         PSI_stage_progress *psi,
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

  /*
    If target storage engine supports atomic DDL we should not commit
    and disable transaction to let SE do proper cleanup on error/crash.
    Such engines should be smart enough to disable undo/redo logging
    for target table automatically.
    Temporary tables path doesn't employ atomic DDL support so disabling
    transaction is OK. Moreover doing so allows to not interfere with
    concurrent FLUSH TABLES WITH READ LOCK.
  */
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_prepare_alter_copy_data(thd))
    DBUG_RETURN(-1);

  if (!(copy= new Copy_field[to->s->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  if (to->file->ha_external_lock(thd, F_WRLCK))
  {
    delete [] copy;
    DBUG_RETURN(-1);
  }

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(thd, to, from->file->indexes_are_disabled(),
                          keys_onoff);

  /*
    We want warnings/errors about data truncation emitted when we
    copy data to new version of table.
  */
  thd->check_for_truncated_fields= CHECK_FIELD_WARN;
  thd->num_truncated_fields= 0L;

  from->file->info(HA_STATUS_VARIABLE);
  to->file->ha_start_bulk_insert(from->file->stats.records);

  mysql_stage_set_work_estimated(psi, from->file->stats.records);

  save_sql_mode= thd->variables.sql_mode;

  List_iterator<Create_field> it(create);
  const Create_field *def;
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
      memset(&tables, 0, sizeof(tables));
      tables.table= from;
      tables.alias= tables.table_name= from->s->table_name.str;
      tables.db= from->s->db.str;
      error= 1;

      Column_privilege_tracker column_privilege(thd, SELECT_ACL);

      if (select_lex->setup_base_ref_items(thd))
        goto err;            /* purecov: inspected */
      if (setup_order(thd, select_lex->base_ref_items,
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

  to->file->extra(HA_EXTRA_BEGIN_ALTER_COPY);

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
    if (thd->is_error())
    {
      error= 1;
      break;
    }

    /*
      @todo After we evaluate what other return values from
      save_in_field() that should be treated as errors, we can remove
      to check thd->is_error() below.
    */
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
          const char *err_msg= ER_THD(thd, ER_DUP_ENTRY_WITH_KEY_NAME);
          if (key_nr == 0 &&
              (to->key_info[0].key_part[0].field->flags &
               AUTO_INCREMENT_FLAG))
            err_msg= ER_THD(thd, ER_DUP_ENTRY_AUTOINCREMENT_CASE);
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

  to->file->extra(HA_EXTRA_END_ALTER_COPY);

  DBUG_EXECUTE_IF("crash_copy_before_commit", DBUG_SUICIDE(););
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_commit_alter_copy_data(thd))
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
  thd->check_for_truncated_fields= CHECK_FIELD_IGNORE;
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
  table_list->set_lock({TL_READ_NO_INSERT, THR_DEFAULT});
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
    table->set_lock({TL_READ, THR_DEFAULT});
    /* Allow to open real tables only. */
    table->required_type= dd::enum_table_type::BASE_TABLE;

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
               to string; GEOMETRY and JSON are implemented on top of BLOB.
               BIT may store its data among NULL bits, convert as well.
             */
              switch (f->type()) {
                case MYSQL_TYPE_BLOB:
                case MYSQL_TYPE_VARCHAR:
                case MYSQL_TYPE_GEOMETRY:
                case MYSQL_TYPE_JSON:
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
        MY_TEST(thd->variables.sql_mode & MODE_NO_ENGINE_SUBSTITUTION);
  if (!(*new_engine= ha_checktype(thd, ha_legacy_type(req_engine),
                                  no_substitution, 1)))
    DBUG_RETURN(true);

  if (req_engine && req_engine != *new_engine)
  {
    push_warning_printf(thd, Sql_condition::SL_NOTE,
                       ER_WARN_USING_OTHER_HANDLER,
                       ER_THD(thd, ER_WARN_USING_OTHER_HANDLER),
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
      !ha_check_if_supported_system_table(*new_engine, db_name, table_name))
  {
    my_error(ER_UNSUPPORTED_ENGINE, MYF(0),
             ha_resolve_storage_engine_name(*new_engine), db_name, table_name);
    *new_engine= NULL;
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}
