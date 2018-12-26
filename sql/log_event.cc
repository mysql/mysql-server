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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "log_event.h"

#include "base64.h"            // base64_encode
#include "binary_log_funcs.h"  // my_timestamp_binary_length

#ifndef MYSQL_CLIENT
#include "debug_sync.h"        // debug_sync_set_action
#include "my_dir.h"            // my_dir
#include "log.h"               // Log_throttle
#include "rpl_mts_submode.h"   // Mts_submode
#include "rpl_rli.h"           // Relay_log_info
#include "rpl_rli_pdb.h"       // Slave_job_group
#include "rpl_slave.h"         // use_slave_mask
#include "sql_base.h"          // close_thread_tables
#include "sql_cache.h"         // QUERY_CACHE_FLAGS_SIZE
#include "sql_db.h"            // load_db_opt_by_name
#include "sql_load.h"          // mysql_load
#include "sql_locale.h"        // my_locale_by_number
#include "sql_parse.h"         // mysql_test_parse_for_slave
#include "sql_show.h"          // append_identifier
#include "transaction.h"       // trans_rollback_stmt
#include "tztime.h"            // Time_zone
#include "rpl_msr.h"           // channel_map
#include "binary_log.h"        // binary_log

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <mysql/psi/mysql_statement.h>
#include "transaction_info.h"
#include "sql_class.h"
#include "mysql/psi/mysql_transaction.h"
#include "sql_plugin.h" // plugin_foreach
#define window_size Log_throttle::LOG_THROTTLE_WINDOW_SIZE
Error_log_throttle
slave_ignored_err_throttle(window_size,
                           sql_print_information,
                           "Error log throttle: %lu time(s) Error_code: 1237"
                           " \"Slave SQL thread ignored the query because of"
                           " replicate-*-table rules\" got suppressed.");
#endif /* MYSQL_CLIENT */

#include <base64.h>
#include <my_bitmap.h>
#include <map>
#include "rpl_utility.h"
/* This is necessary for the List manipuation */
#include "sql_list.h"                           /* I_List */
#include "hash.h"
#include "sql_digest.h"
#include "rpl_gtid.h"
#include "xa_aux.h"

PSI_memory_key key_memory_log_event;
PSI_memory_key key_memory_Incident_log_event_message;
PSI_memory_key key_memory_Rows_query_log_event_rows_query;

using std::min;
using std::max;

/**
  BINLOG_CHECKSUM variable.
*/
const char *binlog_checksum_type_names[]= {
  "NONE",
  "CRC32",
  NullS
};

unsigned int binlog_checksum_type_length[]= {
  sizeof("NONE") - 1,
  sizeof("CRC32") - 1,
  0
};

TYPELIB binlog_checksum_typelib=
{
  array_elements(binlog_checksum_type_names) - 1, "",
  binlog_checksum_type_names,
  binlog_checksum_type_length
};


#define log_cs	&my_charset_latin1

/*
  Size of buffer for printing a double in format %.<PREC>g

  optional '-' + optional zero + '.'  + PREC digits + 'e' + sign +
  exponent digits + '\0'
*/
#define FMT_G_BUFSIZE(PREC) (3 + (PREC) + 5 + 1)

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
static int rows_event_stmt_cleanup(Relay_log_info const *rli, THD* thd);

static const char *HA_ERR(int i)
{
  /* 
    This function should only be called in case of an error
    was detected 
   */
  DBUG_ASSERT(i != 0);
  switch (i) {
  case HA_ERR_KEY_NOT_FOUND: return "HA_ERR_KEY_NOT_FOUND";
  case HA_ERR_FOUND_DUPP_KEY: return "HA_ERR_FOUND_DUPP_KEY";
  case HA_ERR_RECORD_CHANGED: return "HA_ERR_RECORD_CHANGED";
  case HA_ERR_WRONG_INDEX: return "HA_ERR_WRONG_INDEX";
  case HA_ERR_CRASHED: return "HA_ERR_CRASHED";
  case HA_ERR_WRONG_IN_RECORD: return "HA_ERR_WRONG_IN_RECORD";
  case HA_ERR_OUT_OF_MEM: return "HA_ERR_OUT_OF_MEM";
  case HA_ERR_NOT_A_TABLE: return "HA_ERR_NOT_A_TABLE";
  case HA_ERR_WRONG_COMMAND: return "HA_ERR_WRONG_COMMAND";
  case HA_ERR_OLD_FILE: return "HA_ERR_OLD_FILE";
  case HA_ERR_NO_ACTIVE_RECORD: return "HA_ERR_NO_ACTIVE_RECORD";
  case HA_ERR_RECORD_DELETED: return "HA_ERR_RECORD_DELETED";
  case HA_ERR_RECORD_FILE_FULL: return "HA_ERR_RECORD_FILE_FULL";
  case HA_ERR_INDEX_FILE_FULL: return "HA_ERR_INDEX_FILE_FULL";
  case HA_ERR_END_OF_FILE: return "HA_ERR_END_OF_FILE";
  case HA_ERR_UNSUPPORTED: return "HA_ERR_UNSUPPORTED";
  case HA_ERR_TOO_BIG_ROW: return "HA_ERR_TOO_BIG_ROW";
  case HA_WRONG_CREATE_OPTION: return "HA_WRONG_CREATE_OPTION";
  case HA_ERR_FOUND_DUPP_UNIQUE: return "HA_ERR_FOUND_DUPP_UNIQUE";
  case HA_ERR_UNKNOWN_CHARSET: return "HA_ERR_UNKNOWN_CHARSET";
  case HA_ERR_WRONG_MRG_TABLE_DEF: return "HA_ERR_WRONG_MRG_TABLE_DEF";
  case HA_ERR_CRASHED_ON_REPAIR: return "HA_ERR_CRASHED_ON_REPAIR";
  case HA_ERR_CRASHED_ON_USAGE: return "HA_ERR_CRASHED_ON_USAGE";
  case HA_ERR_LOCK_WAIT_TIMEOUT: return "HA_ERR_LOCK_WAIT_TIMEOUT";
  case HA_ERR_LOCK_TABLE_FULL: return "HA_ERR_LOCK_TABLE_FULL";
  case HA_ERR_READ_ONLY_TRANSACTION: return "HA_ERR_READ_ONLY_TRANSACTION";
  case HA_ERR_LOCK_DEADLOCK: return "HA_ERR_LOCK_DEADLOCK";
  case HA_ERR_CANNOT_ADD_FOREIGN: return "HA_ERR_CANNOT_ADD_FOREIGN";
  case HA_ERR_NO_REFERENCED_ROW: return "HA_ERR_NO_REFERENCED_ROW";
  case HA_ERR_ROW_IS_REFERENCED: return "HA_ERR_ROW_IS_REFERENCED";
  case HA_ERR_NO_SAVEPOINT: return "HA_ERR_NO_SAVEPOINT";
  case HA_ERR_NON_UNIQUE_BLOCK_SIZE: return "HA_ERR_NON_UNIQUE_BLOCK_SIZE";
  case HA_ERR_NO_SUCH_TABLE: return "HA_ERR_NO_SUCH_TABLE";
  case HA_ERR_TABLE_EXIST: return "HA_ERR_TABLE_EXIST";
  case HA_ERR_NO_CONNECTION: return "HA_ERR_NO_CONNECTION";
  case HA_ERR_NULL_IN_SPATIAL: return "HA_ERR_NULL_IN_SPATIAL";
  case HA_ERR_TABLE_DEF_CHANGED: return "HA_ERR_TABLE_DEF_CHANGED";
  case HA_ERR_NO_PARTITION_FOUND: return "HA_ERR_NO_PARTITION_FOUND";
  case HA_ERR_RBR_LOGGING_FAILED: return "HA_ERR_RBR_LOGGING_FAILED";
  case HA_ERR_DROP_INDEX_FK: return "HA_ERR_DROP_INDEX_FK";
  case HA_ERR_FOREIGN_DUPLICATE_KEY: return "HA_ERR_FOREIGN_DUPLICATE_KEY";
  case HA_ERR_TABLE_NEEDS_UPGRADE: return "HA_ERR_TABLE_NEEDS_UPGRADE";
  case HA_ERR_TABLE_READONLY: return "HA_ERR_TABLE_READONLY";
  case HA_ERR_AUTOINC_READ_FAILED: return "HA_ERR_AUTOINC_READ_FAILED";
  case HA_ERR_AUTOINC_ERANGE: return "HA_ERR_AUTOINC_ERANGE";
  case HA_ERR_GENERIC: return "HA_ERR_GENERIC";
  case HA_ERR_RECORD_IS_THE_SAME: return "HA_ERR_RECORD_IS_THE_SAME";
  case HA_ERR_LOGGING_IMPOSSIBLE: return "HA_ERR_LOGGING_IMPOSSIBLE";
  case HA_ERR_CORRUPT_EVENT: return "HA_ERR_CORRUPT_EVENT";
  case HA_ERR_ROWS_EVENT_APPLY : return "HA_ERR_ROWS_EVENT_APPLY";
  case HA_ERR_FK_DEPTH_EXCEEDED : return "HA_ERR_FK_DEPTH_EXCEEDED";
  case HA_ERR_INNODB_READ_ONLY: return "HA_ERR_INNODB_READ_ONLY";
  case HA_ERR_COMPUTE_FAILED: return "HA_ERR_COMPUTE_FAILED";
  }
  return "No Error!";
}

/**
   Error reporting facility for Rows_log_event::do_apply_event

   @param level     error, warning or info
   @param ha_error  HA_ERR_ code
   @param rli       pointer to the active Relay_log_info instance
   @param thd       pointer to the slave thread's thd
   @param table     pointer to the event's table object
   @param type      the type of the event
   @param log_name  the master binlog file name
   @param pos       the master binlog file pos (the next after the event)

*/
static void inline slave_rows_error_report(enum loglevel level, int ha_error,
                                           Relay_log_info const *rli, THD *thd,
                                           TABLE *table, const char * type,
                                           const char *log_name, ulong pos)
{
  const char *handler_error= (ha_error ? HA_ERR(ha_error) : NULL);
  bool is_group_replication_applier_channel=
    channel_map.is_group_replication_channel_name((const_cast<Relay_log_info *>(rli))->get_channel(), true);
  char buff[MAX_SLAVE_ERRMSG], *slider;
  const char *buff_end= buff + sizeof(buff);
  size_t len;
  Diagnostics_area::Sql_condition_iterator it=
    thd->get_stmt_da()->sql_conditions();
  const Sql_condition *err;
  buff[0]= 0;

  for (err= it++, slider= buff; err && slider < buff_end - 1;
       slider += len, err= it++)
  {
    len= my_snprintf(slider, buff_end - slider,
                     " %s, Error_code: %d;", err->message_text(),
                     err->mysql_errno());
  }
  if (is_group_replication_applier_channel)
  {
    if (ha_error != 0)
    {
      rli->report(level, thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                  ER_UNKNOWN_ERROR, "Could not execute %s event on table %s.%s;"
                  "%s handler error %s",
                  type, table->s->db.str, table->s->table_name.str,
                  buff, handler_error == NULL ? "<unknown>" : handler_error);
    }
    else
    {
      rli->report(level, thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                  ER_UNKNOWN_ERROR, "Could not execute %s event on table %s.%s;"
                  "%s", type, table->s->db.str, table->s->table_name.str,
                  buff);
    }
  }
  else
  {
    if (ha_error != 0)
    {
      rli->report(level, thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                  ER_UNKNOWN_ERROR, "Could not execute %s event on table %s.%s;"
                  "%s handler error %s; "
                  "the event's master log %s, end_log_pos %lu",
                  type, table->s->db.str, table->s->table_name.str,
                  buff, handler_error == NULL ? "<unknown>" : handler_error,
                  log_name, pos);
    }
    else
    {
      rli->report(level, thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                  ER_UNKNOWN_ERROR, "Could not execute %s event on table %s.%s;"
                  "%s the event's master log %s, end_log_pos %lu",
                  type, table->s->db.str, table->s->table_name.str,
                  buff, log_name, pos);
    }
  }
}

static void set_thd_db(THD *thd, const char *db, size_t db_len)
{
  char lcase_db_buf[NAME_LEN +1]; 
  LEX_CSTRING new_db;
  new_db.length= db_len;
  if (lower_case_table_names)
  {
    my_stpcpy(lcase_db_buf, db); 
    my_casedn_str(system_charset_info, lcase_db_buf);
    new_db.str= lcase_db_buf;
  }
  else 
    new_db.str= (char*) db;

  new_db.str= (char*) rpl_filter->get_rewrite_db(new_db.str,
                                                 &new_db.length);
  thd->set_db(new_db);
}

#endif


/*
  pretty_print_str()
*/

#ifdef MYSQL_CLIENT
static void pretty_print_str(IO_CACHE* cache, const char* str, size_t len)
{
  const char* end = str + len;
  my_b_printf(cache, "\'");
  while (str < end)
  {
    char c;
    switch ((c=*str++)) {
    case '\n': my_b_printf(cache, "\\n"); break;
    case '\r': my_b_printf(cache, "\\r"); break;
    case '\\': my_b_printf(cache, "\\\\"); break;
    case '\b': my_b_printf(cache, "\\b"); break;
    case '\t': my_b_printf(cache, "\\t"); break;
    case '\'': my_b_printf(cache, "\\'"); break;
    case 0   : my_b_printf(cache, "\\0"); break;
    default:
      my_b_printf(cache, "%c", c);
      break;
    }
  }
  my_b_printf(cache, "\'");
}
#endif /* MYSQL_CLIENT */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

static void clear_all_errors(THD *thd, Relay_log_info *rli)
{
  thd->is_slave_error = 0;
  thd->clear_error();
  rli->clear_error();
  if (rli->workers_array_initialized)
  {
    for(size_t i= 0; i < rli->get_worker_count(); i++)
    {
      rli->get_worker(i)->clear_error();
    }
  }
}

inline int idempotent_error_code(int err_code)
{
  int ret= 0;

  switch (err_code)
  {
    case 0:
      ret= 1;
    break;
    /*
      The following list of "idempotent" errors
      means that an error from the list might happen
      because of idempotent (more than once)
      applying of a binlog file.
      Notice, that binlog has a  ddl operation its
      second applying may cause

      case HA_ERR_TABLE_DEF_CHANGED:
      case HA_ERR_CANNOT_ADD_FOREIGN:

      which are not included into to the list.

      Note that HA_ERR_RECORD_DELETED is not in the list since
      do_exec_row() should not return that error code.
    */
    case HA_ERR_RECORD_CHANGED:
    case HA_ERR_KEY_NOT_FOUND:
    case HA_ERR_END_OF_FILE:
    case HA_ERR_FOUND_DUPP_KEY:
    case HA_ERR_FOUND_DUPP_UNIQUE:
    case HA_ERR_FOREIGN_DUPLICATE_KEY:
    case HA_ERR_NO_REFERENCED_ROW:
    case HA_ERR_ROW_IS_REFERENCED:
      ret= 1;
    break;
    default:
      ret= 0;
    break;
  }
  return (ret);
}

/**
  Ignore error code specified on command line.
*/

int ignored_error_code(int err_code)
{
  return ((err_code == ER_SLAVE_IGNORED_TABLE) ||
          (use_slave_mask && bitmap_is_set(&slave_error_mask, err_code)));
}

/*
  This function converts an engine's error to a server error.
   
  If the thread does not have an error already reported, it tries to 
  define it by calling the engine's method print_error. However, if a 
  mapping is not found, it uses the ER_UNKNOWN_ERROR and prints out a 
  warning message.
*/ 
int convert_handler_error(int error, THD* thd, TABLE *table)
{
  uint actual_error= (thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                           0);

  if (actual_error == 0)
  {
    table->file->print_error(error, MYF(0));
    actual_error= (thd->is_error() ? thd->get_stmt_da()->mysql_errno() :
                        ER_UNKNOWN_ERROR);
    if (actual_error == ER_UNKNOWN_ERROR)
      sql_print_warning("Unknown error detected %d in handler", error);
  }

  return (actual_error);
}

inline bool concurrency_error_code(int error)
{
  switch (error)
  {
  case ER_LOCK_WAIT_TIMEOUT:
  case ER_LOCK_DEADLOCK:
  case ER_XA_RBDEADLOCK:
    return TRUE;
  default: 
    return (FALSE);
  }
}

inline bool unexpected_error_code(int unexpected_error)
{
  switch (unexpected_error) 
  {
  case ER_NET_READ_ERROR:
  case ER_NET_ERROR_ON_WRITE:
  case ER_QUERY_INTERRUPTED:
  case ER_SERVER_SHUTDOWN:
  case ER_NEW_ABORTING_CONNECTION:
    return(TRUE);
  default:
    return(FALSE);
  }
}

/*
  pretty_print_str()
*/

static char *pretty_print_str(char *packet, const char *str, size_t len)
{
  const char *end= str + len;
  char *pos= packet;
  *pos++= '\'';
  while (str < end)
  {
    char c;
    switch ((c=*str++)) {
    case '\n': *pos++= '\\'; *pos++= 'n'; break;
    case '\r': *pos++= '\\'; *pos++= 'r'; break;
    case '\\': *pos++= '\\'; *pos++= '\\'; break;
    case '\b': *pos++= '\\'; *pos++= 'b'; break;
    case '\t': *pos++= '\\'; *pos++= 't'; break;
    case '\'': *pos++= '\\'; *pos++= '\''; break;
    case 0   : *pos++= '\\'; *pos++= '0'; break;
    default:
      *pos++= c;
      break;
    }
  }
  *pos++= '\'';
  return pos;
}
#endif /* !MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
  Creates a temporary name for load data infile:.

  @param buf		      Store new filename here
  @param file_id	      File_id (part of file name)
  @param event_server_id     Event_id (part of file name)
  @param ext		      Extension for file name

  @return
    Pointer to start of extension
*/

static char *slave_load_file_stem(char *buf, uint file_id,
                                  int event_server_id, const char *ext)
{
  char *res;
  fn_format(buf,PREFIX_SQL_LOAD,slave_load_tmpdir, "", MY_UNPACK_FILENAME);
  to_unix_path(buf);

  buf= strend(buf);
  int appended_length= sprintf(buf, "%s-%d-", server_uuid, event_server_id);
  buf+= appended_length;
  res= int10_to_str(file_id, buf, 10);
  my_stpcpy(res, ext);                             // Add extension last
  return res;                                   // Pointer to extension
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
  Delete all temporary files used for SQL_LOAD.
*/

static void cleanup_load_tmpdir()
{
  MY_DIR *dirp;
  FILEINFO *file;
  uint i;
  char fname[FN_REFLEN], prefbuf[TEMP_FILE_MAX_LEN], *p;

  if (!(dirp=my_dir(slave_load_tmpdir,MYF(0))))
    return;

  /* 
     When we are deleting temporary files, we should only remove
     the files associated with the server id of our server.
     We don't use event_server_id here because since we've disabled
     direct binlogging of Create_file/Append_file/Exec_load events
     we cannot meet Start_log event in the middle of events from one 
     LOAD DATA.
  */
  p= strmake(prefbuf, STRING_WITH_LEN(PREFIX_SQL_LOAD));
  sprintf(p,"%s-",server_uuid);

  for (i=0 ; i < dirp->number_off_files; i++)
  {
    file=dirp->dir_entry+i;
    if (is_prefix(file->name, prefbuf))
    {
      fn_format(fname,file->name,slave_load_tmpdir,"",MY_UNPACK_FILENAME);
      mysql_file_delete(key_file_misc, fname, MYF(0));
    }
  }

  my_dirend(dirp);
}
#endif


/*
  Stores string to IO_CACHE file.

  Writes str to file in the following format:
   1. Stores length using only one byte (255 maximum value);
   2. Stores complete str.
*/

static bool write_str_at_most_255_bytes(IO_CACHE *file, const char *str,
                                        uint length)
{
  uchar tmp[1];
  tmp[0]= (uchar) length;
  return (my_b_safe_write(file, tmp, sizeof(tmp)) ||
	  my_b_safe_write(file, (uchar*) str, length));
}

/**
  Transforms a string into "" or its expression in 0x... form.
*/

char *str_to_hex(char *to, const char *from, size_t len)
{
  if (len)
  {
    *to++= '0';
    *to++= 'x';
    to= octet2hex(to, from, len);
  }
  else
    to= my_stpcpy(to, "\"\"");
  return to;                               // pointer to end 0 of 'to'
}

#ifndef MYSQL_CLIENT

/**
  Append a version of the 'from' string suitable for use in a query to
  the 'to' string.  To generate a correct escaping, the character set
  information in 'csinfo' is used.
*/

int
append_query_string(THD *thd, const CHARSET_INFO *csinfo,
                    String const *from, String *to)
{
  char *beg, *ptr;
  size_t const orig_len= to->length();
  if (to->reserve(orig_len + from->length()*2+3))
    return 1;

  beg= to->c_ptr_quick() + to->length();
  ptr= beg;
  if (csinfo->escape_with_backslash_is_dangerous)
    ptr= str_to_hex(ptr, from->ptr(), from->length());
  else
  {
    *ptr++= '\'';
    if (!(thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES))
    {
      ptr+= escape_string_for_mysql(csinfo, ptr, 0,
                                    from->ptr(), from->length());
    }
    else
    {
      const char *frm_str= from->ptr();

      for (; frm_str < (from->ptr() + from->length()); frm_str++)
      {
        /* Using '' way to represent "'" */
        if (*frm_str == '\'')
          *ptr++= *frm_str;

        *ptr++= *frm_str;
      }
    }

    *ptr++= '\'';
  }
  to->length(orig_len + ptr - beg);
  return 0;
}
#endif


/**
  Prints a "session_var=value" string. Used by mysqlbinlog to print some SET
  commands just before it prints a query.
*/

#ifdef MYSQL_CLIENT

static void print_set_option(IO_CACHE* file, uint32 bits_changed,
                             uint32 option, uint32 flags, const char* name,
                             bool* need_comma)
{
  if (bits_changed & option)
  {
    if (*need_comma)
      my_b_printf(file,", ");
    my_b_printf(file,"%s=%d", name, MY_TEST(flags & option));
    *need_comma= 1;
  }
}
#endif
/**************************************************************************
	Log_event methods (= the parent class of all events)
**************************************************************************/

/**
  @return
  returns the human readable name of the event's type
*/

const char* Log_event::get_type_str(Log_event_type type)
{
  switch(type) {
  case binary_log::START_EVENT_V3:  return "Start_v3";
  case binary_log::STOP_EVENT:   return "Stop";
  case binary_log::QUERY_EVENT:  return "Query";
  case binary_log::ROTATE_EVENT: return "Rotate";
  case binary_log::INTVAR_EVENT: return "Intvar";
  case binary_log::LOAD_EVENT:   return "Load";
  case binary_log::NEW_LOAD_EVENT:   return "New_load";
  case binary_log::CREATE_FILE_EVENT: return "Create_file";
  case binary_log::APPEND_BLOCK_EVENT: return "Append_block";
  case binary_log::DELETE_FILE_EVENT: return "Delete_file";
  case binary_log::EXEC_LOAD_EVENT: return "Exec_load";
  case binary_log::RAND_EVENT: return "RAND";
  case binary_log::XID_EVENT: return "Xid";
  case binary_log::USER_VAR_EVENT: return "User var";
  case binary_log::FORMAT_DESCRIPTION_EVENT: return "Format_desc";
  case binary_log::TABLE_MAP_EVENT: return "Table_map";
  case binary_log::PRE_GA_WRITE_ROWS_EVENT: return "Write_rows_event_old";
  case binary_log::PRE_GA_UPDATE_ROWS_EVENT: return "Update_rows_event_old";
  case binary_log::PRE_GA_DELETE_ROWS_EVENT: return "Delete_rows_event_old";
  case binary_log::WRITE_ROWS_EVENT_V1: return "Write_rows_v1";
  case binary_log::UPDATE_ROWS_EVENT_V1: return "Update_rows_v1";
  case binary_log::DELETE_ROWS_EVENT_V1: return "Delete_rows_v1";
  case binary_log::BEGIN_LOAD_QUERY_EVENT: return "Begin_load_query";
  case binary_log::EXECUTE_LOAD_QUERY_EVENT: return "Execute_load_query";
  case binary_log::INCIDENT_EVENT: return "Incident";
  case binary_log::IGNORABLE_LOG_EVENT: return "Ignorable";
  case binary_log::ROWS_QUERY_LOG_EVENT: return "Rows_query";
  case binary_log::WRITE_ROWS_EVENT: return "Write_rows";
  case binary_log::UPDATE_ROWS_EVENT: return "Update_rows";
  case binary_log::DELETE_ROWS_EVENT: return "Delete_rows";
  case binary_log::GTID_LOG_EVENT: return "Gtid";
  case binary_log::ANONYMOUS_GTID_LOG_EVENT: return "Anonymous_Gtid";
  case binary_log::PREVIOUS_GTIDS_LOG_EVENT: return "Previous_gtids";
  case binary_log::HEARTBEAT_LOG_EVENT: return "Heartbeat";
  case binary_log::TRANSACTION_CONTEXT_EVENT: return "Transaction_context";
  case binary_log::VIEW_CHANGE_EVENT: return "View_change";
  case binary_log::XA_PREPARE_LOG_EVENT: return "XA_prepare";
  default: return "Unknown";                            /* impossible */
  }
}

const char* Log_event::get_type_str()
{
  return get_type_str(get_type_code());
}


/*
  Log_event::Log_event()
*/

#ifndef MYSQL_CLIENT
Log_event::Log_event(THD* thd_arg, uint16 flags_arg,
                     enum_event_cache_type cache_type_arg,
                     enum_event_logging_type logging_type_arg,
                     Log_event_header *header, Log_event_footer *footer)
  : is_valid_param(false), temp_buf(0), exec_time(0),
    event_cache_type(cache_type_arg), event_logging_type(logging_type_arg),
    crc(0), common_header(header), common_footer(footer), thd(thd_arg)
{
  server_id= thd->server_id;
  common_header->unmasked_server_id= server_id;
  common_header->when= thd->start_time;
  common_header->log_pos= 0;
  common_header->flags= flags_arg;
}

/**
  This minimal constructor is for when you are not even sure that there
  is a valid THD. For example in the server when we are shutting down or
  flushing logs after receiving a SIGHUP (then we must write a Rotate to
  the binlog but we have no THD, so we need this minimal constructor).
*/

Log_event::Log_event(Log_event_header* header, Log_event_footer *footer,
                     enum_event_cache_type cache_type_arg,
                     enum_event_logging_type logging_type_arg)
  : is_valid_param(false), temp_buf(0), exec_time(0), event_cache_type(cache_type_arg),
   event_logging_type(logging_type_arg), crc(0), common_header(header),
   common_footer(footer), thd(0)
{
  server_id=	::server_id;
  common_header->unmasked_server_id= server_id;
}
#endif /* !MYSQL_CLIENT */


/*
  Log_event::Log_event()
*/

Log_event::Log_event(Log_event_header *header,
                     Log_event_footer *footer)
  : is_valid_param(false), temp_buf(0), exec_time(0),
    event_cache_type(EVENT_INVALID_CACHE),
    event_logging_type(EVENT_INVALID_LOGGING),
    crc(0), common_header(header), common_footer(footer)
{
#ifndef MYSQL_CLIENT
  thd= 0;
#endif
  /*
     Mask out any irrelevant parts of the server_id
  */
#ifdef HAVE_REPLICATION
  server_id = common_header->unmasked_server_id & opt_server_id_mask;
#else
  server_id = common_header->unmasked_server_id;
#endif
}

/*
  This method is not on header file to avoid using key_memory_log_event
  outside log_event.cc, allowing header file to be included on plugins.
*/
void* Log_event::operator new(size_t size)
{
  return my_malloc(key_memory_log_event, size, MYF(MY_WME|MY_FAE));
}

#ifndef MYSQL_CLIENT
#ifdef HAVE_REPLICATION
inline int Log_event::do_apply_event_worker(Slave_worker *w)
{
  DBUG_EXECUTE_IF("crash_in_a_worker",
                  {
                    /* we will crash a worker after waiting for
                    2 seconds to make sure that other transactions are
                    scheduled and completed */
                    if (w->id == 2)
                    {
                      DBUG_SET("-d,crash_in_a_worker");
                      my_sleep(2000000);
                      DBUG_SUICIDE();
                    }
                  });
  return do_apply_event(w);
}

int Log_event::do_update_pos(Relay_log_info *rli)
{
  int error= 0;
  DBUG_ASSERT(!rli->belongs_to_client());
  /*
    rli is null when (as far as I (Guilhem) know) the caller is
    Load_log_event::do_apply_event *and* that one is called from
    Execute_load_log_event::do_apply_event.  In this case, we don't
    do anything here ; Execute_load_log_event::do_apply_event will
    call Log_event::do_apply_event again later with the proper rli.
    Strictly speaking, if we were sure that rli is null only in the
    case discussed above, 'if (rli)' is useless here.  But as we are
    not 100% sure, keep it for now.

    Matz: I don't think we will need this check with this refactoring.
  */

  DBUG_ASSERT(!is_mts_worker(rli->info_thd));

  if (rli)
    error= rli->stmt_done(common_header->log_pos);
  return error;
}


Log_event::enum_skip_reason
Log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    The logic for slave_skip_counter is as follows:

    - Events that are skipped because they have the same server_id as
      the slave do not decrease slave_skip_counter.

    - Other events (that pass the server_id test) will decrease
      slave_skip_counter.

    - Except in one case: if slave_skip_counter==1, it will only
      decrease to 0 if we are at a so-called group boundary. Here, a
      group is defined as the range of events that represent a single
      transaction in the relay log: see comment for is_in_group in
      rpl_rli.h for a definition.

    The difficult part to implement is the logic to avoid decreasing
    the counter to 0.  Given that groups have the form described in
    is_in_group in rpl_rli.h, we implement the logic as follows:

    - Gtid, Rand, User_var, Int_var will never decrease the counter to
      0.

    - BEGIN will set thd->variables.option_bits & OPTION_BEGIN and
      COMMIT/Xid will clear it.  This happens regardless of whether
      the BEGIN/COMMIT/Xid is skipped itself.

    - Other events will decrease the counter unless OPTION_BEGIN is
      set.
  */
  DBUG_PRINT("info", ("ev->server_id=%lu, ::server_id=%lu,"
                      " rli->replicate_same_server_id=%d,"
                      " rli->slave_skip_counter=%d",
                      (ulong) server_id, (ulong) ::server_id,
                      rli->replicate_same_server_id,
                      rli->slave_skip_counter));
  if ((server_id == ::server_id && !rli->replicate_same_server_id) ||
      (rli->slave_skip_counter == 1 && rli->is_in_group()))
    return EVENT_SKIP_IGNORE;
  else if (rli->slave_skip_counter > 0)
    return EVENT_SKIP_COUNT;
  else
    return EVENT_SKIP_NOT;
}


/*
  Log_event::pack_info()
*/

int Log_event::pack_info(Protocol *protocol)
{
  protocol->store("", &my_charset_bin);
  return 0;
}


/**
  Only called by SHOW BINLOG EVENTS
*/
int Log_event::net_send(Protocol *protocol, const char* log_name, my_off_t pos)
{
  const char *p= strrchr(log_name, FN_LIBCHAR);
  const char *event_type;
  if (p)
    log_name = p + 1;

  protocol->start_row();
  protocol->store(log_name, &my_charset_bin);
  protocol->store((ulonglong) pos);
  event_type = get_type_str();
  protocol->store(event_type, strlen(event_type), &my_charset_bin);
  protocol->store((uint32) server_id);
  protocol->store((ulonglong) common_header->log_pos);
  if (pack_info(protocol))
    return 1;
  return protocol->end_row();
}
#endif /* HAVE_REPLICATION */


/**
  init_show_field_list() prepares the column names and types for the
  output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
  EVENTS.
*/

void Log_event::init_show_field_list(List<Item>* field_list)
{
  field_list->push_back(new Item_empty_string("Log_name", 20));
  field_list->push_back(new Item_return_int("Pos", MY_INT32_NUM_DECIMAL_DIGITS,
					    MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Event_type", 20));
  field_list->push_back(new Item_return_int("Server_id", 10,
					    MYSQL_TYPE_LONG));
  field_list->push_back(new Item_return_int("End_log_pos",
                                            MY_INT32_NUM_DECIMAL_DIGITS,
					    MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Info", 20));
}

/**
   A decider of whether to trigger checksum computation or not.
   To be invoked in Log_event::write() stack.
   The decision is positive 

    S,M) if it's been marked for checksumming with @c checksum_alg
    
    M) otherwise, if @@global.binlog_checksum is not NONE and the event is 
       directly written to the binlog file.
       The to-be-cached event decides at @c write_cache() time.

   Otherwise the decision is negative.

   @note   A side effect of the method is altering Log_event::checksum_alg
           it the latter was undefined at calling.

   @return true (positive) or false (negative)
*/
my_bool Log_event::need_checksum()
{
  DBUG_ENTER("Log_event::need_checksum");
  my_bool ret= FALSE;
  /* 
     few callers of Log_event::write 
     (incl FD::write, FD constructing code on the slave side, Rotate relay log
     and Stop event) 
     provides their checksum alg preference through Log_event::checksum_alg.
  */
  if (common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
    ret= (common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF);
  else if (binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF &&
           event_cache_type == Log_event::EVENT_NO_CACHE)
    ret= (binlog_checksum_options != 0);
  else
    ret= FALSE;

  /*
    FD calls the methods before data_written has been calculated.
    The following invariant claims if the current is not the first
    call (and therefore data_written is not zero) then `ret' must be
    TRUE. It may not be null because FD is always checksummed.
  */

  DBUG_ASSERT(get_type_code() != binary_log::FORMAT_DESCRIPTION_EVENT || ret ||
              common_header->data_written == 0);

  if (common_footer->checksum_alg == binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
    common_footer->checksum_alg= ret ? // calculated value stored
      static_cast<enum_binlog_checksum_alg>(binlog_checksum_options) :
      binary_log::BINLOG_CHECKSUM_ALG_OFF;

  DBUG_ASSERT(!ret ||
              ((common_footer->checksum_alg ==
                static_cast<enum_binlog_checksum_alg>(binlog_checksum_options) ||
               /*
                  Stop event closes the relay-log and its checksum alg
                  preference is set by the caller can be different
                  from the server's binlog_checksum_options.
               */
               get_type_code() == binary_log::STOP_EVENT ||
               /*
                  Rotate:s can be checksummed regardless of the server's
                  binlog_checksum_options. That applies to both
                  the local RL's Rotate and the master's Rotate
                  which IO thread instantiates via queue_binlog_ver_3_event.
               */
               get_type_code() == binary_log::ROTATE_EVENT ||
               /*
                  The previous event has its checksum option defined
                  according to the format description event.
               */
               get_type_code() == binary_log::PREVIOUS_GTIDS_LOG_EVENT ||
               /* FD is always checksummed */
               get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT) &&
               common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF));

  DBUG_ASSERT(common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
  DBUG_ASSERT(((get_type_code() != binary_log::ROTATE_EVENT &&
                get_type_code() != binary_log::STOP_EVENT) ||
                get_type_code() != binary_log::FORMAT_DESCRIPTION_EVENT) ||
              event_cache_type == Log_event::EVENT_NO_CACHE);

  DBUG_RETURN(ret);
}

bool Log_event::wrapper_my_b_safe_write(IO_CACHE* file, const uchar* buf, size_t size)
{
  DBUG_EXECUTE_IF("simulate_temp_file_write_error",
                  {
                    file->write_pos=file->write_end;
                    DBUG_SET("+d,simulate_file_write_error");
                  });
  if (need_checksum() && size != 0)
    crc= checksum_crc32(crc, buf, size);
  bool ret = my_b_safe_write(file, buf, size);
  DBUG_EXECUTE_IF("simulate_temp_file_write_error",
                  {
                    DBUG_SET("-d,simulate_file_write_error");
                  });
  return ret;
}

bool Log_event::write_footer(IO_CACHE* file) 
{
  /*
     footer contains the checksum-algorithm descriptor 
     followed by the checksum value
  */
  if (need_checksum())
  {
    uchar buf[BINLOG_CHECKSUM_LEN];
    int4store(buf, crc);
    return (my_b_safe_write(file, (uchar*) buf, sizeof(buf)));
  }
  return 0;
}


uint32 Log_event::write_header_to_memory(uchar *buf)
{
  // Query start time
  ulong timestamp= (ulong) get_time();

#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("inc_event_time_by_1_hour",1,0)  &&
      DBUG_EVALUATE_IF("dec_event_time_by_1_hour",1,0))
  {
    /**
      This assertion guarantees that these debug flags are not
      used at the same time (they would cancel each other).
    */
    DBUG_ASSERT(0);
  }
  else
  {
    DBUG_EXECUTE_IF("inc_event_time_by_1_hour", timestamp= timestamp + 3600;);
    DBUG_EXECUTE_IF("dec_event_time_by_1_hour", timestamp= timestamp - 3600;);
  }
#endif

  /*
    Header will be of size LOG_EVENT_HEADER_LEN for all events, except for
    FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT, where it will be
    LOG_EVENT_MINIMAL_HEADER_LEN (remember these 2 have a frozen header,
    because we read them before knowing the format).
  */

  int4store(buf, timestamp);
  buf[EVENT_TYPE_OFFSET]= get_type_code();
  int4store(buf + SERVER_ID_OFFSET, server_id);
  int4store(buf + EVENT_LEN_OFFSET,
            static_cast<uint32>(common_header->data_written));
  int4store(buf + LOG_POS_OFFSET,
            static_cast<uint32>(common_header->log_pos));
  int2store(buf + FLAGS_OFFSET, common_header->flags);

  return LOG_EVENT_HEADER_LEN;
}


bool Log_event::write_header(IO_CACHE* file, size_t event_data_length)
{
  uchar header[LOG_EVENT_HEADER_LEN];
  bool ret;
  DBUG_ENTER("Log_event::write_header");

  /* Store number of bytes that will be written by this event */
  common_header->data_written= event_data_length + sizeof(header);

  if (need_checksum())
  {
    crc= checksum_crc32(0L, NULL, 0);
    common_header->data_written += BINLOG_CHECKSUM_LEN;
  }

  /*
    log_pos != 0 if this is relay-log event. In this case we should not
    change the position
  */

  if (is_artificial_event())
  {
    /*
      Artificial events are automatically generated and do not exist
      in master's binary log, so log_pos should be set to 0.
    */
    common_header->log_pos= 0;
  }
  else  if (!common_header->log_pos)
  {
    /*
      Calculate position of end of event

      Note that with a SEQ_READ_APPEND cache, my_b_tell() does not
      work well.  So this will give slightly wrong positions for the
      Format_desc/Rotate/Stop events which the slave writes to its
      relay log. For example, the initial Format_desc will have
      end_log_pos=91 instead of 95. Because after writing the first 4
      bytes of the relay log, my_b_tell() still reports 0. Because
      my_b_append() does not update the counter which my_b_tell()
      later uses (one should probably use my_b_append_tell() to work
      around this).  To get right positions even when writing to the
      relay log, we use the (new) my_b_safe_tell().

      Note that this raises a question on the correctness of all these
      DBUG_ASSERT(my_b_tell()=rli->event_relay_log_pos).

      If in a transaction, the log_pos which we calculate below is not
      very good (because then my_b_safe_tell() returns start position
      of the BEGIN, so it's like the statement was at the BEGIN's
      place), but it's not a very serious problem (as the slave, when
      it is in a transaction, does not take those end_log_pos into
      account (as it calls inc_event_relay_log_pos()). To be fixed
      later, so that it looks less strange. But not bug.
    */

    common_header->log_pos= my_b_safe_tell(file) + common_header->data_written;
  }

  write_header_to_memory(header);

  ret= my_b_safe_write(file, header, LOG_EVENT_HEADER_LEN);

  /*
    Update the checksum.

    In case this is a Format_description_log_event, we need to clear
    the LOG_EVENT_BINLOG_IN_USE_F flag before computing the checksum,
    since the flag will be cleared when the binlog is closed.  On
    verification, the flag is dropped before computing the checksum
    too.
  */
  if (need_checksum() &&
      (common_header->flags & LOG_EVENT_BINLOG_IN_USE_F) != 0)
  {
    common_header->flags &= ~LOG_EVENT_BINLOG_IN_USE_F;
    int2store(header + FLAGS_OFFSET, common_header->flags);
  }
  crc= my_checksum(crc, header, LOG_EVENT_HEADER_LEN);

  DBUG_RETURN( ret);
}


/**
  This needn't be format-tolerant, because we only read
  LOG_EVENT_MINIMAL_HEADER_LEN (we just want to read the event's length).

  The caller should allocate the packet buffer before calling this function.
*/

int Log_event::read_log_event(IO_CACHE* file, String* packet,
                              mysql_mutex_t* log_lock,
                              enum_binlog_checksum_alg checksum_alg_arg,
                              const char *log_file_name_arg,
                              bool* is_binlog_active,
                              char *event_header)
{

  ulong data_len;
  int result=0;
  char local_buf[LOG_EVENT_MINIMAL_HEADER_LEN];
  char *buf= event_header != NULL ? event_header : local_buf;
  uchar ev_offset= packet->length();
  DBUG_ENTER("Log_event::read_log_event(IO_CACHE *, String *, mysql_mutex_t, uint8)");

  if (log_lock)
    mysql_mutex_lock(log_lock);

  if (log_file_name_arg)
    *is_binlog_active= mysql_bin_log.is_active(log_file_name_arg);

  /* If the event header wasn't passed, we need to read it. */
  if (buf == local_buf)
  {
    if (my_b_read(file, (uchar*) buf, LOG_EVENT_MINIMAL_HEADER_LEN))
    {
      /*
        If the read hits eof, we must report it as eof so the caller
        will know it can go into cond_wait to be woken up on the next
        update to the log.
      */
      DBUG_PRINT("error",("my_b_read failed. file->error: %d", file->error));
      if (!file->error)
        result= LOG_READ_EOF;
      else
        result= (file->error > 0 ? LOG_READ_TRUNC : LOG_READ_IO);
      goto end;
    }
  }
  else
    DBUG_PRINT("info",("Skipped reading the event header. Using the provided one."));

  data_len= uint4korr(buf + EVENT_LEN_OFFSET);
  if (data_len < LOG_EVENT_MINIMAL_HEADER_LEN ||
      data_len > max(current_thd->variables.max_allowed_packet,
                     opt_binlog_rows_event_max_size + MAX_LOG_EVENT_HEADER))
  {
    DBUG_PRINT("error",("data_len is out of bounds. data_len: %lu", data_len));
    result= ((data_len < LOG_EVENT_MINIMAL_HEADER_LEN) ? LOG_READ_BOGUS :
	     LOG_READ_TOO_LARGE);
    goto end;
  }

  /*
    If the event header wasn't passed, the caller doesn't know the event size
    yet, so the packet size may not have enough space to load the entire
    event. We need to adjust the packet size here since the call to my_b_read()
    below expects the buffer to be allocated.
  */
  if (buf == local_buf)
  {
    ulong new_alloc_len= packet->length() + data_len;
    if (new_alloc_len > packet->alloced_length() &&
        packet->mem_realloc(new_alloc_len))
    {
      /* Failed to allocate packet */
      result= LOG_READ_MEM;
      goto end;
    }
  }

  /* Check packet buffer size and append the log event header to it */
  if (packet->alloced_length() - packet->length() < data_len ||
      packet->append(buf, LOG_EVENT_MINIMAL_HEADER_LEN))
  {
    DBUG_PRINT("info", ("first packet->append failed (out of memory)"));
    /* Failed to allocate packet */
    result= LOG_READ_MEM;
    goto end;
  }
  data_len-= LOG_EVENT_MINIMAL_HEADER_LEN;
  if (data_len)
  {
    /*
      Append rest of event, read directly from file into packet.

      We are avoiding to call packet->append(IO_CACHE, size_t) at this point
      because the String::append logic will call String::mem_realloc() that
      might resize the buffer (changing its pointer) in order to reserve a
      space for a trailing '\0' that we don't need.
    */
    char *event_data_buffer= const_cast<char*>(packet->ptr() +
                                               packet->length());
    result= my_b_read(file,
                      reinterpret_cast<uchar*>(event_data_buffer),
                      data_len);
    if (result)
    {
      /*
        Fatal error occured when appending rest of the event
        to packet, possible failures:
	1. EOF occured when reading from file, it's really an error
           as data_len is >=0 there's supposed to be more bytes available.
           file->error will have been set to number of bytes left to read
        2. Read was interrupted, file->error would normally be set to -1
        3. Failed to allocate memory for packet, my_errno
           will be ENOMEM(file->error shuold be 0, but since the
           memory allocation occurs before the call to read it might
           be uninitialized)
      */
      DBUG_PRINT("info", ("second packet->append failed (out of memory)"));
      result= (my_errno() == ENOMEM ? LOG_READ_MEM :
               (file->error >= 0 ? LOG_READ_TRUNC: LOG_READ_IO));
      goto end;
    }
    else
    {
      packet->length(packet->length() + data_len);
      /*
        Corrupt the event for Dump thread.
        We also need to exclude Previous_gtids_log_event and Gtid_log_event
        events from injected corruption to allow dump thread to move forward
        on binary log until the missing transactions from slave when
        MASTER_AUTO_POSITION= 1.
      */
      DBUG_EXECUTE_IF("corrupt_read_log_event",
	uchar *debug_event_buf_c = (uchar*) packet->ptr() + ev_offset;
        if (debug_event_buf_c[EVENT_TYPE_OFFSET] != binary_log::FORMAT_DESCRIPTION_EVENT &&
            debug_event_buf_c[EVENT_TYPE_OFFSET] != binary_log::PREVIOUS_GTIDS_LOG_EVENT &&
            debug_event_buf_c[EVENT_TYPE_OFFSET] != binary_log::GTID_LOG_EVENT)
        {
          int debug_cor_pos = rand() % (data_len + LOG_EVENT_MINIMAL_HEADER_LEN -
                              BINLOG_CHECKSUM_LEN);
          debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
          DBUG_PRINT("info", ("Corrupt the event at Log_event::read_log_event: byte on position %d", debug_cor_pos));
	}
      );
      /*
        CRC verification of the Dump thread
      */
      binary_log_debug::debug_checksum_test=
        DBUG_EVALUATE_IF("simulate_checksum_test_failure", true, false);

      if (opt_master_verify_checksum &&
        Log_event_footer::event_checksum_test((uchar*)packet->ptr() + ev_offset,
                                              data_len + LOG_EVENT_MINIMAL_HEADER_LEN,
                                              checksum_alg_arg))
      {
        DBUG_PRINT("info", ("checksum test failed"));
        result= LOG_READ_CHECKSUM_FAILURE;
        goto end;
      }
    }
  }

end:
  if (log_lock)
    mysql_mutex_unlock(log_lock);
  DBUG_PRINT("info", ("read_log_event returns %d", result));
  DBUG_RETURN(result);
}
#endif /* !MYSQL_CLIENT */

#ifndef MYSQL_CLIENT
#define UNLOCK_MUTEX if (log_lock) mysql_mutex_unlock(log_lock);
#define LOCK_MUTEX if (log_lock) mysql_mutex_lock(log_lock);
#else
#define UNLOCK_MUTEX
#define LOCK_MUTEX
#endif

#ifndef MYSQL_CLIENT
/**
  @note
    Allocates memory;  The caller is responsible for clean-up.
*/
Log_event* Log_event::read_log_event(IO_CACHE* file,
                                     mysql_mutex_t* log_lock,
                                     const Format_description_log_event
                                     *description_event,
                                     my_bool crc_check)
#else
Log_event* Log_event::read_log_event(IO_CACHE* file,
                                     const Format_description_log_event
                                     *description_event,
                                     my_bool crc_check,
                                     read_log_event_filter_function f)
#endif
{
  DBUG_ENTER("Log_event::read_log_event(IO_CACHE *[, mysql_mutex_t *], Format_description_log_event *, my_bool)");
  DBUG_ASSERT(description_event != 0);
  char head[LOG_EVENT_MINIMAL_HEADER_LEN];
  /*
    First we only want to read at most LOG_EVENT_MINIMAL_HEADER_LEN, just to
    check the event for sanity and to know its length; no need to really parse
    it. We say "at most" because this could be a 3.23 master, which has header
    of 13 bytes, whereas LOG_EVENT_MINIMAL_HEADER_LEN is 19 bytes (it's
    "minimal" over the set {MySQL >=4.0}).
  */
  uint header_size= min<uint>(description_event->common_header_len,
                              LOG_EVENT_MINIMAL_HEADER_LEN);

  LOCK_MUTEX;
  DBUG_PRINT("info", ("my_b_tell: %lu", (ulong) my_b_tell(file)));
  if (my_b_read(file, (uchar *) head, header_size))
  {
    DBUG_PRINT("info", ("Log_event::read_log_event(IO_CACHE*,Format_desc*) "
                        "failed in my_b_read((IO_CACHE*)%p, (uchar*)%p, %u)",
                        file, head, header_size));
    UNLOCK_MUTEX;
    /*
      No error here; it could be that we are at the file's end. However
      if the next my_b_read() fails (below), it will be an error as we
      were able to read the first bytes.
    */
    DBUG_RETURN(0);
  }
  ulong data_len = uint4korr(head + EVENT_LEN_OFFSET);
  char *buf= 0;
  const char *error= 0;
  Log_event *res=  0;
#if !defined(MYSQL_SERVER) && !defined(EMBEDDED_LIBRARY)
  ulong log_max_allowed_packet;
  mysql_get_option(NULL, MYSQL_OPT_MAX_ALLOWED_PACKET,
                   &log_max_allowed_packet);
#else
  THD *thd=current_thd;
  uint log_max_allowed_packet= thd ? slave_max_allowed_packet : ~0U;
#endif

  ulong const max_size=
    max<ulong>(log_max_allowed_packet,
               opt_binlog_rows_event_max_size + MAX_LOG_EVENT_HEADER);
  if (data_len > max_size)
  {
    error = "Event too big";
    goto err;
  }

  if (data_len < header_size)
  {
    error = "Event too small";
    goto err;
  }

  // some events use the extra byte to null-terminate strings
  if (!(buf = (char*) my_malloc(key_memory_log_event,
                                data_len+1, MYF(MY_WME))))
  {
    error = "Out of memory";
    goto err;
  }
  buf[data_len] = 0;
  memcpy(buf, head, header_size);
  if (my_b_read(file, (uchar*) buf + header_size, data_len - header_size))
  {
    error = "read error";
    goto err;
  }

#if defined(MYSQL_CLIENT)
  if (f && f(&buf, &data_len, description_event))
  {
    error = "Error applying filter while reading event";
    goto err;
  }
#endif
  if ((res= read_log_event(buf, data_len, &error, description_event, crc_check)))
    res->register_temp_buf(buf);

err:
  UNLOCK_MUTEX;
  if (!res)
  {
    DBUG_ASSERT(error != 0);
    sql_print_error("Error in Log_event::read_log_event(): "
                    "'%s', data_len: %lu, event_type: %d",
		    error,data_len,head[EVENT_TYPE_OFFSET]);
    my_free(buf);
    /*
      The SQL slave thread will check if file->error<0 to know
      if there was an I/O error. Even if there is no "low-level" I/O errors
      with 'file', any of the high-level above errors is worrying
      enough to stop the SQL thread now ; as we are skipping the current event,
      going on with reading and successfully executing other events can
      only corrupt the slave's databases. So stop.
      The file->error is also checked to record the position of
      the last valid event when master server recovers.
    */
    file->error= -1;
  }
  DBUG_RETURN(res);
}


/**
  Binlog format tolerance is in (buf, event_len, description_event)
  constructors.
*/

Log_event* Log_event::read_log_event(const char* buf, uint event_len,
				     const char **error,
                                     const Format_description_log_event *description_event,
                                     my_bool crc_check)
{
  Log_event* ev= NULL;
  enum_binlog_checksum_alg  alg;
  DBUG_ENTER("Log_event::read_log_event(char *, uint, char **, Format_description_log_event *, my_bool)");
  DBUG_ASSERT(description_event != 0);
  DBUG_PRINT("info", ("binlog_version: %d", description_event->binlog_version));
  DBUG_DUMP("data", (unsigned char*) buf, event_len);

  /* Check the integrity */
  if (event_len < EVENT_LEN_OFFSET ||
      event_len != uint4korr(buf+EVENT_LEN_OFFSET))
  {
    DBUG_PRINT("error", ("event_len=%u EVENT_LEN_OFFSET=%d "
                         "buf[EVENT_TYPE_OFFSET]=%d ENUM_END_EVENT=%d "
                         "uint4korr(buf+EVENT_LEN_OFFSET)=%d",
                         event_len, EVENT_LEN_OFFSET,
                         buf[EVENT_TYPE_OFFSET], binary_log::ENUM_END_EVENT,
                         uint4korr(buf+EVENT_LEN_OFFSET)));
    *error="Sanity check failed";		// Needed to free buffer
    DBUG_RETURN(NULL); // general sanity check - will fail on a partial read
  }

  uint event_type= buf[EVENT_TYPE_OFFSET];
  // all following START events in the current file are without checksum
  if (event_type == binary_log::START_EVENT_V3)
    (const_cast< Format_description_log_event *>(description_event))->
            common_footer->checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_OFF;
  // Sanity check for Format description event
  if (event_type == binary_log::FORMAT_DESCRIPTION_EVENT)
  {
    if (event_len < LOG_EVENT_MINIMAL_HEADER_LEN +
        ST_COMMON_HEADER_LEN_OFFSET)
    {
      *error= "Found invalid Format description event in binary log";
      DBUG_RETURN(0);
    }
    uint tmp_header_len= buf[LOG_EVENT_MINIMAL_HEADER_LEN + ST_COMMON_HEADER_LEN_OFFSET];
    if (event_len < tmp_header_len + ST_SERVER_VER_OFFSET + ST_SERVER_VER_LEN)
    {
      *error= "Found invalid Format description event in binary log";
      DBUG_RETURN(0);
    }
  }
  /*
    CRC verification by SQL and Show-Binlog-Events master side.
    The caller has to provide @description_event->checksum_alg to
    be the last seen FD's (A) descriptor.
    If event is FD the descriptor is in it.
    Notice, FD of the binlog can be only in one instance and therefore
    Show-Binlog-Events executing master side thread needs just to know
    the only FD's (A) value -  whereas RL can contain more.
    In the RL case, the alg is kept in FD_e (@description_event) which is reset 
    to the newer read-out event after its execution with possibly new alg descriptor.
    Therefore in a typical sequence of RL:
    {FD_s^0, FD_m, E_m^1} E_m^1 
    will be verified with (A) of FD_m.

    See legends definition on MYSQL_BIN_LOG::relay_log_checksum_alg docs
    lines (log.h).

    Notice, a pre-checksum FD version forces alg := BINLOG_CHECKSUM_ALG_UNDEF.
  */
  alg= (event_type != binary_log::FORMAT_DESCRIPTION_EVENT) ?
       description_event->common_footer->checksum_alg :
       Log_event_footer::get_checksum_alg(buf, event_len);
  // Emulate the corruption during reading an event
  DBUG_EXECUTE_IF("corrupt_read_log_event_char",
    if (event_type != binary_log::FORMAT_DESCRIPTION_EVENT)
    {
      char *debug_event_buf_c = (char *)buf;
      int debug_cor_pos = rand() % (event_len - BINLOG_CHECKSUM_LEN);
      debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
      DBUG_PRINT("info", ("Corrupt the event at Log_event::read_log_event(char*,...): byte on position %d", debug_cor_pos));
      DBUG_SET("");
    }
  );

#ifndef DBUG_OFF
  binary_log_debug::debug_checksum_test=
    DBUG_EVALUATE_IF("simulate_checksum_test_failure", true, false);
#endif
  if (crc_check &&
      Log_event_footer::event_checksum_test((uchar *) buf, event_len, alg) &&
      /* Skip the crc check when simulating an unknown ignorable log event. */
      !DBUG_EVALUATE_IF("simulate_unknown_ignorable_log_event", 1, 0))
  {
    *error= "Event crc check failed! Most likely there is event corruption.";
#ifdef MYSQL_CLIENT
    if (force_opt)
    {
      ev= new Unknown_log_event(buf, description_event);
      DBUG_RETURN(ev);
    }
#endif
    DBUG_RETURN(NULL);
  }

  if (event_type > description_event->number_of_event_types &&
      event_type != binary_log::FORMAT_DESCRIPTION_EVENT &&
      /*
        Skip the event type check when simulating an
        unknown ignorable log event.
      */
      !DBUG_EVALUATE_IF("simulate_unknown_ignorable_log_event", 1, 0))
  {
    /*
      It is unsafe to use the description_event if its post_header_len
      array does not include the event type.
    */
    DBUG_PRINT("error", ("event type %d found, but the current "
                         "Format_description_log_event supports only %d event "
                         "types", event_type,
                         description_event->number_of_event_types));
    ev= NULL;
  }
  else
  {
    /*
      In some previuos versions (see comment in
      Format_description_log_event::Format_description_log_event(char*,...)),
      event types were assigned different id numbers than in the
      present version. In order to replicate from such versions to the
      present version, we must map those event type id's to our event
      type id's.  The mapping is done with the event_type_permutation
      array, which was set up when the Format_description_log_event
      was read.
    */
    if (description_event->event_type_permutation)
    {
      uint new_event_type;
      if (event_type >= EVENT_TYPE_PERMUTATION_NUM)
        /* Safe guard for read out of bounds of event_type_permutation. */
        new_event_type= binary_log::UNKNOWN_EVENT;
      else
        new_event_type= description_event->event_type_permutation[event_type];

      DBUG_PRINT("info", ("converting event type %d to %d (%s)",
                 event_type, new_event_type,
                 get_type_str((Log_event_type)new_event_type)));
      event_type= new_event_type;
    }

    if (alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF &&
        (event_type == binary_log::FORMAT_DESCRIPTION_EVENT ||
         alg != binary_log::BINLOG_CHECKSUM_ALG_OFF))
      event_len= event_len - BINLOG_CHECKSUM_LEN;

    switch(event_type) {
    case binary_log::QUERY_EVENT:
#ifndef DBUG_OFF
      binary_log_debug::debug_query_mts_corrupt_db_names=
        DBUG_EVALUATE_IF("query_log_event_mts_corrupt_db_names", true, false);
#endif
      ev  = new Query_log_event(buf, event_len, description_event,
                                binary_log::QUERY_EVENT);
      break;
    case binary_log::LOAD_EVENT:
    case binary_log::NEW_LOAD_EVENT:
#ifndef DBUG_OFF
      binary_log_debug::debug_simulate_invalid_address=
        DBUG_EVALUATE_IF("simulate_invalid_address", true, false);
#endif
      ev = new Load_log_event(buf, event_len, description_event);
      break;
    case binary_log::ROTATE_EVENT:
      ev = new Rotate_log_event(buf, event_len, description_event);
      break;
    case binary_log::CREATE_FILE_EVENT:
#ifndef DBUG_OFF
      binary_log_debug::debug_simulate_invalid_address=
        DBUG_EVALUATE_IF("simulate_invalid_address", true, false);
#endif
      ev = new Create_file_log_event(buf, event_len, description_event);
      break;
    case binary_log::APPEND_BLOCK_EVENT:
      ev = new Append_block_log_event(buf, event_len, description_event);
      break;
    case binary_log::DELETE_FILE_EVENT:
      ev = new Delete_file_log_event(buf, event_len, description_event);
      break;
    case binary_log::EXEC_LOAD_EVENT:
      ev = new Execute_load_log_event(buf, event_len, description_event);
      break;
    case binary_log::START_EVENT_V3: /* this is sent only by MySQL <=4.x */
      ev = new Start_log_event_v3(buf, event_len, description_event);
      break;
    case binary_log::STOP_EVENT:
      ev = new Stop_log_event(buf, description_event);
      break;
    case binary_log::INTVAR_EVENT:
      ev = new Intvar_log_event(buf, description_event);
      break;
    case binary_log::XID_EVENT:
      ev = new Xid_log_event(buf, description_event);
      break;
    case binary_log::RAND_EVENT:
      ev = new Rand_log_event(buf, description_event);
      break;
    case binary_log::USER_VAR_EVENT:
      ev = new User_var_log_event(buf, event_len, description_event);
      break;
    case binary_log::FORMAT_DESCRIPTION_EVENT:
      ev = new Format_description_log_event(buf, event_len, description_event);
      break;
#if defined(HAVE_REPLICATION)
    case binary_log::PRE_GA_WRITE_ROWS_EVENT:
      ev = new Write_rows_log_event_old(buf, event_len, description_event);
      break;
    case binary_log::PRE_GA_UPDATE_ROWS_EVENT:
      ev = new Update_rows_log_event_old(buf, event_len, description_event);
      break;
    case binary_log::PRE_GA_DELETE_ROWS_EVENT:
      ev = new Delete_rows_log_event_old(buf, event_len, description_event);
      break;
    case binary_log::WRITE_ROWS_EVENT_V1:
      if (!(description_event->post_header_len.empty()))
        ev = new Write_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::UPDATE_ROWS_EVENT_V1:
      if (!(description_event->post_header_len.empty()))
        ev = new Update_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::DELETE_ROWS_EVENT_V1:
      if (!(description_event->post_header_len.empty()))
        ev = new Delete_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::TABLE_MAP_EVENT:
      if (!(description_event->post_header_len.empty()))
        ev = new Table_map_log_event(buf, event_len, description_event);
      break;
#endif
    case binary_log::BEGIN_LOAD_QUERY_EVENT:
      ev = new Begin_load_query_log_event(buf, event_len, description_event);
      break;
    case binary_log::EXECUTE_LOAD_QUERY_EVENT:
      ev= new Execute_load_query_log_event(buf, event_len, description_event);
      break;
    case binary_log::INCIDENT_EVENT:
      ev = new Incident_log_event(buf, event_len, description_event);
      break;
    case binary_log::ROWS_QUERY_LOG_EVENT:
      ev= new Rows_query_log_event(buf, event_len, description_event);
      break;
    case binary_log::GTID_LOG_EVENT:
    case binary_log::ANONYMOUS_GTID_LOG_EVENT:
      ev= new Gtid_log_event(buf, event_len, description_event);
      break;
    case binary_log::PREVIOUS_GTIDS_LOG_EVENT:
      ev= new Previous_gtids_log_event(buf, event_len, description_event);
      break;
#if defined(HAVE_REPLICATION)
    case binary_log::WRITE_ROWS_EVENT:
      ev = new Write_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::UPDATE_ROWS_EVENT:
      ev = new Update_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::DELETE_ROWS_EVENT:
      ev = new Delete_rows_log_event(buf, event_len, description_event);
      break;
    case binary_log::TRANSACTION_CONTEXT_EVENT:
      ev = new Transaction_context_log_event(buf, event_len, description_event);
      break;
    case binary_log::VIEW_CHANGE_EVENT:
      ev = new View_change_log_event(buf, event_len, description_event);
      break;
#endif
    case binary_log::XA_PREPARE_LOG_EVENT:
      ev= new XA_prepare_log_event(buf, description_event);
      break;
    default:
      /*
        Create an object of Ignorable_log_event for unrecognized sub-class.
        So that SLAVE SQL THREAD will only update the position and continue.
      */
      if (uint2korr(buf + FLAGS_OFFSET) & LOG_EVENT_IGNORABLE_F)
      {
        ev= new Ignorable_log_event(buf, description_event);
      }
      else
      {
        DBUG_PRINT("error",("Unknown event code: %d",
                            (int) buf[EVENT_TYPE_OFFSET]));
        ev= NULL;
      }
      break;
    }
  }

  if (ev)
  {
    ev->common_footer->checksum_alg= alg;
    if (ev->common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF &&
        ev->common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
      ev->crc= uint4korr(buf + (event_len));
  }

  DBUG_PRINT("read_event", ("%s(type_code: %d; event_len: %d)",
                            ev ? ev->get_type_str() : "<unknown>",
                            buf[EVENT_TYPE_OFFSET],
                            event_len));
  /*
    is_valid is used for small event-specific sanity tests which are
    important; for example there are some my_malloc() in constructors
    (e.g. Query_log_event::Query_log_event(char*...)); when these
    my_malloc() fail we can't return an error out of the constructor
    (because constructor is "void") ; so instead we leave the pointer we
    wanted to allocate (e.g. 'query') to 0 and we test it and set the
    value of is_valid to true or false based on the test.
    Same for Format_description_log_event, member 'post_header_len'.

    SLAVE_EVENT is never used, so it should not be read ever.
  */
  if (!ev || !ev->is_valid() || (event_type == binary_log::SLAVE_EVENT))
  {
    DBUG_PRINT("error",("Found invalid event in binary log"));
    delete ev;
#ifdef MYSQL_CLIENT
    if (!force_opt) /* then mysqlbinlog dies */
    {
      *error= "Found invalid event in binary log";
      DBUG_RETURN(0);
    }
    ev= new Unknown_log_event(buf, description_event);
#else
    *error= "Found invalid event in binary log";
    DBUG_RETURN(0);
#endif
  }
  DBUG_RETURN(ev);  
}

#ifdef MYSQL_CLIENT

/*
  Log_event::print_header()
*/

void Log_event::print_header(IO_CACHE* file,
                             PRINT_EVENT_INFO* print_event_info,
                             bool is_more MY_ATTRIBUTE((unused)))
{
  char llbuff[22];
  my_off_t hexdump_from= print_event_info->hexdump_from;
  DBUG_ENTER("Log_event::print_header");

  my_b_printf(file, "#");
  print_timestamp(file, NULL);
  my_b_printf(file, " server id %lu  end_log_pos %s ", (ulong) server_id,
              llstr(common_header->log_pos,llbuff));

  /* print the checksum */

  if (common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF &&
      common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
  {
    char checksum_buf[BINLOG_CHECKSUM_LEN * 2 + 4]; // to fit to "0x%lx "
    size_t const bytes_written=
      my_snprintf(checksum_buf, sizeof(checksum_buf), "0x%08lx ", (ulong) crc);
    my_b_printf(file, "%s ", get_type(&binlog_checksum_typelib,
                                      common_footer->checksum_alg));
    my_b_printf(file, checksum_buf, bytes_written);
  }

  /* mysqlbinlog --hexdump */
  if (print_event_info->hexdump_from)
  {
    my_b_printf(file, "\n");
    uchar *ptr= (uchar*)temp_buf;
    my_off_t size=
      uint4korr(ptr + EVENT_LEN_OFFSET) - LOG_EVENT_MINIMAL_HEADER_LEN;
    my_off_t i;

    /* Header len * 4 >= header len * (2 chars + space + extra space) */
    char *h, hex_string[49]= {0};
    char *c, char_string[16+1]= {0};

    /* Pretty-print event common header if header is exactly 19 bytes */
    if (print_event_info->common_header_len == LOG_EVENT_MINIMAL_HEADER_LEN)
    {
      char emit_buf[256];               // Enough for storing one line
      my_b_printf(file, "# Position  Timestamp   Type   Master ID        "
                  "Size      Master Pos    Flags \n");
      size_t const bytes_written=
        my_snprintf(emit_buf, sizeof(emit_buf),
                    "# %8.8lx %02x %02x %02x %02x   %02x   "
                    "%02x %02x %02x %02x   %02x %02x %02x %02x   "
                    "%02x %02x %02x %02x   %02x %02x\n",
                    (unsigned long) hexdump_from,
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
                    ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13],
                    ptr[14], ptr[15], ptr[16], ptr[17], ptr[18]);
      DBUG_ASSERT(static_cast<size_t>(bytes_written) < sizeof(emit_buf));
      my_b_write(file, (uchar*) emit_buf, bytes_written);
      ptr += LOG_EVENT_MINIMAL_HEADER_LEN;
      hexdump_from += LOG_EVENT_MINIMAL_HEADER_LEN;
    }

    /* Rest of event (without common header) */
    for (i= 0, c= char_string, h=hex_string;
	 i < size;
	 i++, ptr++)
    {
      my_snprintf(h, 4, (i % 16 <= 7) ? "%02x " : " %02x", *ptr);
      h += 3;

      *c++= my_isalnum(&my_charset_bin, *ptr) ? *ptr : '.';

      if (i % 16 == 15)
      {
        /*
          my_b_printf() does not support full printf() formats, so we
          have to do it this way.

          TODO: Rewrite my_b_printf() to support full printf() syntax.
         */
        char emit_buf[256];
        size_t const bytes_written=
          my_snprintf(emit_buf, sizeof(emit_buf),
                      "# %8.8lx %-48.48s |%16s|\n",
                      (unsigned long) (hexdump_from + (i & 0xfffffff0)),
                      hex_string, char_string);
        DBUG_ASSERT(static_cast<size_t>(bytes_written) < sizeof(emit_buf));
	my_b_write(file, (uchar*) emit_buf, bytes_written);
	hex_string[0]= 0;
	char_string[0]= 0;
	c= char_string;
	h= hex_string;
      }
    }
    *c= '\0';
    DBUG_ASSERT(hex_string[48] == 0);
    
    if (hex_string[0])
    {
      char emit_buf[256];
      // Right-pad hex_string with spaces, up to 48 characters.
      memset(h, ' ', (sizeof(hex_string) -1) - (h - hex_string));
      size_t const bytes_written=
        my_snprintf(emit_buf, sizeof(emit_buf),
                    "# %8.8lx %-48.48s |%s|\n",
                    (unsigned long) (hexdump_from + (i & 0xfffffff0)),
                    hex_string, char_string);
      DBUG_ASSERT(static_cast<size_t>(bytes_written) < sizeof(emit_buf));
      my_b_write(file, (uchar*) emit_buf, bytes_written);
    }
    /*
      need a # to prefix the rest of printouts for example those of
      Rows_log_event::print_helper().
    */
    my_b_write(file, reinterpret_cast<const uchar*>("# "), 2);
  }
  DBUG_VOID_RETURN;
}


/**
  Prints a quoted string to io cache.
  Control characters are displayed as hex sequence, e.g. \x00
  
  @param[in] file              IO cache
  @param[in] prt               Pointer to string
  @param[in] length            String length
*/

static void
my_b_write_quoted(IO_CACHE *file, const uchar *ptr, uint length)
{
  const uchar *s;
  my_b_printf(file, "'");
  for (s= ptr; length > 0 ; s++, length--)
  {
    if (*s > 0x1F && *s != '\'' && *s != '\\')
      my_b_write(file, s, 1);
    else
    {
      uchar hex[10];
      size_t len= my_snprintf((char*) hex, sizeof(hex), "%s%02x", "\\x", *s);
      my_b_write(file, hex, len);
    }
  }
  my_b_printf(file, "'");
}

/**
  Prints a bit string to io cache in format  b'1010'.
  
  @param[in] file              IO cache
  @param[in] ptr               Pointer to string
  @param[in] nbits             Number of bits
*/
static void
my_b_write_bit(IO_CACHE *file, const uchar *ptr, uint nbits)
{
  uint bitnum, nbits8= ((nbits + 7) / 8) * 8, skip_bits= nbits8 - nbits;
  my_b_printf(file, "b'");
  for (bitnum= skip_bits ; bitnum < nbits8; bitnum++)
  {
    int is_set= (ptr[(bitnum) / 8] >> (7 - bitnum % 8))  & 0x01;
    my_b_write(file, (const uchar*) (is_set ? "1" : "0"), 1);
  }
  my_b_printf(file, "'");
}


/**
  Prints a packed string to io cache.
  The string consists of length packed to 1 or 2 bytes,
  followed by string data itself.
  
  @param[in] file              IO cache
  @param[in] ptr               Pointer to string
  @param[in] length            String size
  
  @retval   - number of bytes scanned.
*/
static size_t
my_b_write_quoted_with_length(IO_CACHE *file, const uchar *ptr, uint length)
{
  if (length < 256)
  {
    length= *ptr;
    my_b_write_quoted(file, ptr + 1, length);
    return length + 1;
  }
  else
  {
    length= uint2korr(ptr);
    my_b_write_quoted(file, ptr + 2, length);
    return length + 2;
  }
}


/**
  Prints a 32-bit number in both signed and unsigned representation
  
  @param[in] file              IO cache
  @param[in] sl                Signed number
  @param[in] ul                Unsigned number
*/
static void
my_b_write_sint32_and_uint32(IO_CACHE *file, int32 si, uint32 ui)
{
  my_b_printf(file, "%d", si);
  if (si < 0)
    my_b_printf(file, " (%u)", ui);
}


/**
  Print a packed value of the given SQL type into IO cache
  
  @param[in] file              IO cache
  @param[in] ptr               Pointer to string
  @param[in] type              Column type
  @param[in] meta              Column meta information
  @param[out] typestr          SQL type string buffer (for verbose output)
  @param[out] typestr_length   Size of typestr
  
  @retval   - number of bytes scanned from ptr.
*/
static size_t
log_event_print_value(IO_CACHE *file, const uchar *ptr,
                      uint type, uint meta,
                      char *typestr, size_t typestr_length)
{
  uint32 length= 0;

  if (type == MYSQL_TYPE_STRING)
  {
    if (meta >= 256)
    {
      uint byte0= meta >> 8;
      uint byte1= meta & 0xFF;
      
      if ((byte0 & 0x30) != 0x30)
      {
        /* a long CHAR() field: see #37426 */
        length= byte1 | (((byte0 & 0x30) ^ 0x30) << 4);
        type= byte0 | 0x30;
      }
      else
        length = meta & 0xFF;
    }
    else
      length= meta;
  }

  switch (type) {
  case MYSQL_TYPE_LONG:
    {
      my_snprintf(typestr, typestr_length, "INT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      int32 si= sint4korr(ptr);
      uint32 ui= uint4korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      return 4;
    }

  case MYSQL_TYPE_TINY:
    {
      my_snprintf(typestr, typestr_length, "TINYINT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      my_b_write_sint32_and_uint32(file, (int) (signed char) *ptr,
                                  (uint) (unsigned char) *ptr);
      return 1;
    }

  case MYSQL_TYPE_SHORT:
    {
      my_snprintf(typestr, typestr_length, "SHORTINT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      int32 si= (int32) sint2korr(ptr);
      uint32 ui= (uint32) uint2korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      return 2;
    }
  
  case MYSQL_TYPE_INT24:
    {
      my_snprintf(typestr, typestr_length, "MEDIUMINT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      int32 si= sint3korr(ptr);
      uint32 ui= uint3korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      return 3;
    }

  case MYSQL_TYPE_LONGLONG:
    {
      my_snprintf(typestr, typestr_length, "LONGINT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      char tmp[64];
      longlong si= sint8korr(ptr);
      longlong10_to_str(si, tmp, -10);
      my_b_printf(file, "%s", tmp);
      if (si < 0)
      {
        ulonglong ui= uint8korr(ptr);
        longlong10_to_str((longlong) ui, tmp, 10);
        my_b_printf(file, " (%s)", tmp);        
      }
      return 8;
    }

  case MYSQL_TYPE_NEWDECIMAL:
    {
      uint precision= meta >> 8;
      uint decimals= meta & 0xFF;
      my_snprintf(typestr, typestr_length, "DECIMAL(%d,%d)",
                  precision, decimals);
      if(!ptr)
        return my_b_printf(file, "NULL");
      uint bin_size= my_decimal_get_binary_size(precision, decimals);
      my_decimal dec;
      binary2my_decimal(E_DEC_FATAL_ERROR, (uchar*) ptr, &dec,
                        precision, decimals);
      int len= DECIMAL_MAX_STR_LENGTH;
      char buff[DECIMAL_MAX_STR_LENGTH + 1];
      decimal2string(&dec,buff,&len, 0, 0, 0);
      my_b_printf(file, "%s", buff);
      return bin_size;
    }

  case MYSQL_TYPE_FLOAT:
    {
      my_snprintf(typestr, typestr_length, "FLOAT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      float fl;
      float4get(&fl, ptr);
      char tmp[320];
      sprintf(tmp, "%-20g", (double) fl);
      my_b_printf(file, "%s", tmp); /* my_snprintf doesn't support %-20g */
      return 4;
    }

  case MYSQL_TYPE_DOUBLE:
    {
      strcpy(typestr, "DOUBLE");
      if(!ptr)
        return my_b_printf(file, "NULL");
      double dbl;
      float8get(&dbl, ptr);
      char tmp[320];
      sprintf(tmp, "%-.20g", dbl); /* my_snprintf doesn't support %-20g */
      my_b_printf(file, "%s", tmp);
      return 8;
    }
  
  case MYSQL_TYPE_BIT:
    {
      /* Meta-data: bit_len, bytes_in_rec, 2 bytes */
      uint nbits= ((meta >> 8) * 8) + (meta & 0xFF);
      my_snprintf(typestr, typestr_length, "BIT(%d)", nbits);
      if(!ptr)
        return my_b_printf(file, "NULL");
      length= (nbits + 7) / 8;
      my_b_write_bit(file, ptr, nbits);
      return length;
    }

  case MYSQL_TYPE_TIMESTAMP:
    {
      my_snprintf(typestr, typestr_length, "TIMESTAMP");
      if(!ptr)
        return my_b_printf(file, "NULL");
      uint32 i32= uint4korr(ptr);
      my_b_printf(file, "%d", i32);
      return 4;
    }

  case MYSQL_TYPE_TIMESTAMP2:
    {
      my_snprintf(typestr, typestr_length, "TIMESTAMP(%d)", meta);
      if(!ptr)
        return my_b_printf(file, "NULL");
      char buf[MAX_DATE_STRING_REP_LENGTH];
      struct timeval tm;
      my_timestamp_from_binary(&tm, ptr, meta);
      int buflen= my_timeval_to_str(&tm, buf, meta);
      my_b_write(file, buf, buflen);
      return my_timestamp_binary_length(meta);
    }

  case MYSQL_TYPE_DATETIME:
    {
      my_snprintf(typestr, typestr_length, "DATETIME");
      if(!ptr)
        return my_b_printf(file, "NULL");
      size_t d, t;
      uint64 i64= uint8korr(ptr); /* YYYYMMDDhhmmss */
      d= static_cast<size_t>(i64 / 1000000);
      t= i64 % 1000000;
      my_b_printf(file, "%04d-%02d-%02d %02d:%02d:%02d",
                  static_cast<int>(d / 10000),
                  static_cast<int>(d % 10000) / 100,
                  static_cast<int>(d % 100),
                  static_cast<int>(t / 10000),
                  static_cast<int>(t % 10000) / 100,
                  static_cast<int>(t % 100));
      return 8;
    }

  case MYSQL_TYPE_DATETIME2:
    {
      my_snprintf(typestr, typestr_length, "DATETIME(%d)", meta);
      if(!ptr)
        return my_b_printf(file, "NULL");
      char buf[MAX_DATE_STRING_REP_LENGTH];
      MYSQL_TIME ltime;
      longlong packed= my_datetime_packed_from_binary(ptr, meta);
      TIME_from_longlong_datetime_packed(&ltime, packed);
      int buflen= my_datetime_to_str(&ltime, buf, meta);
      my_b_write_quoted(file, (uchar *) buf, buflen);
      return my_datetime_binary_length(meta);
    }

  case MYSQL_TYPE_TIME:
    {
      my_snprintf(typestr, typestr_length, "TIME");
      if(!ptr)
        return my_b_printf(file, "NULL");
      uint32 i32= uint3korr(ptr);
      my_b_printf(file, "'%02d:%02d:%02d'",
                  i32 / 10000, (i32 % 10000) / 100, i32 % 100);
      return 3;
    }

  case MYSQL_TYPE_TIME2:
    {
      my_snprintf(typestr, typestr_length, "TIME(%d)", meta);
      if(!ptr)
        return my_b_printf(file, "NULL");
      char buf[MAX_DATE_STRING_REP_LENGTH];
      MYSQL_TIME ltime;
      longlong packed= my_time_packed_from_binary(ptr, meta);
      TIME_from_longlong_time_packed(&ltime, packed);
      int buflen= my_time_to_str(&ltime, buf, meta);
      my_b_write_quoted(file, (uchar *) buf, buflen);
      return my_time_binary_length(meta);
    }

  case MYSQL_TYPE_NEWDATE:
    {
      my_snprintf(typestr, typestr_length, "DATE");
      if(!ptr)
        return my_b_printf(file, "NULL");
      uint32 tmp= uint3korr(ptr);
      int part;
      char buf[11];
      char *pos= &buf[10];  // start from '\0' to the beginning

      /* Copied from field.cc */
      *pos--=0;					// End NULL
      part=(int) (tmp & 31);
      *pos--= (char) ('0'+part%10);
      *pos--= (char) ('0'+part/10);
      *pos--= ':';
      part=(int) (tmp >> 5 & 15);
      *pos--= (char) ('0'+part%10);
      *pos--= (char) ('0'+part/10);
      *pos--= ':';
      part=(int) (tmp >> 9);
      *pos--= (char) ('0'+part%10); part/=10;
      *pos--= (char) ('0'+part%10); part/=10;
      *pos--= (char) ('0'+part%10); part/=10;
      *pos=   (char) ('0'+part);
      my_b_printf(file , "'%s'", buf);
      return 3;
    }

  case MYSQL_TYPE_YEAR:
    {
      my_snprintf(typestr, typestr_length, "YEAR");
      if(!ptr)
        return my_b_printf(file, "NULL");
      uint32 i32= *ptr;
      my_b_printf(file, "%04d", i32+ 1900);
      return 1;
    }
  
  case MYSQL_TYPE_ENUM:
    switch (meta & 0xFF) {
    case 1:
      my_snprintf(typestr, typestr_length, "ENUM(1 byte)");
      if(!ptr)
        return my_b_printf(file, "NULL");
      my_b_printf(file, "%d", (int) *ptr);
      return 1;
    case 2:
      {
        my_snprintf(typestr, typestr_length, "ENUM(2 bytes)");
        if(!ptr)
          return my_b_printf(file, "NULL");
        int32 i32= uint2korr(ptr);
        my_b_printf(file, "%d", i32);
        return 2;
      }
    default:
      my_b_printf(file, "!! Unknown ENUM packlen=%d", meta & 0xFF); 
      return 0;
    }
    break;
    
  case MYSQL_TYPE_SET:
    my_snprintf(typestr, typestr_length, "SET(%d bytes)", meta & 0xFF);
    if(!ptr)
      return my_b_printf(file, "NULL");
    my_b_write_bit(file, ptr , (meta & 0xFF) * 8);
    return meta & 0xFF;
  
  case MYSQL_TYPE_BLOB:
    switch (meta) {
    case 1:
      my_snprintf(typestr, typestr_length, "TINYBLOB/TINYTEXT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      length= *ptr;
      my_b_write_quoted(file, ptr + 1, length);
      return length + 1;
    case 2:
      my_snprintf(typestr, typestr_length, "BLOB/TEXT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      length= uint2korr(ptr);
      my_b_write_quoted(file, ptr + 2, length);
      return length + 2;
    case 3:
      my_snprintf(typestr, typestr_length, "MEDIUMBLOB/MEDIUMTEXT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      length= uint3korr(ptr);
      my_b_write_quoted(file, ptr + 3, length);
      return length + 3;
    case 4:
      my_snprintf(typestr, typestr_length, "LONGBLOB/LONGTEXT");
      if(!ptr)
        return my_b_printf(file, "NULL");
      length= uint4korr(ptr);
      my_b_write_quoted(file, ptr + 4, length);
      return length + 4;
    default:
      my_b_printf(file, "!! Unknown BLOB packlen=%d", length);
      return 0;
    }

  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_VAR_STRING:
    length= meta;
    my_snprintf(typestr, typestr_length, "VARSTRING(%d)", length);
    if(!ptr) 
      return my_b_printf(file, "NULL");
    return my_b_write_quoted_with_length(file, ptr, length);

  case MYSQL_TYPE_STRING:
    my_snprintf(typestr, typestr_length, "STRING(%d)", length);
    if(!ptr)
      return my_b_printf(file, "NULL");
    return my_b_write_quoted_with_length(file, ptr, length);

  case MYSQL_TYPE_JSON:
    my_snprintf(typestr, typestr_length, "JSON");
    if (!ptr)
      return my_b_printf(file, "NULL");
    length= uint2korr(ptr);
    my_b_write_quoted(file, ptr + meta, length);
    return length + meta;

  default:
    {
      char tmp[5];
      my_snprintf(tmp, sizeof(tmp), "%04x", meta);
      my_b_printf(file,
                  "!! Don't know how to handle column type=%d meta=%d (%s)",
                  type, meta, tmp);
    }
    break;
  }
  *typestr= 0;
  return 0;
}


/**
  Print a packed row into IO cache
  
  @param[in] file              IO cache
  @param[in] td                Table definition
  @param[in] print_event_into  Print parameters
  @param[in] cols_bitmap       Column bitmaps.
  @param[in] value             Pointer to packed row
  @param[in] prefix            Row's SQL clause ("SET", "WHERE", etc)
  
  @retval   - number of bytes scanned.
*/


size_t
Rows_log_event::print_verbose_one_row(IO_CACHE *file, table_def *td,
                                      PRINT_EVENT_INFO *print_event_info,
                                      MY_BITMAP *cols_bitmap,
                                      const uchar *value, const uchar *prefix)
{
  const uchar *value0= value;
  const uchar *null_bits= value;
  uint null_bit_index= 0;
  char typestr[64]= "";

  /*
    Skip metadata bytes which gives the information about nullabity of master
    columns. Master writes one bit for each affected column.
   */
  value+= (bitmap_bits_set(cols_bitmap) + 7) / 8;
  
  my_b_printf(file, "%s", prefix);
  
  for (size_t i= 0; i < td->size(); i ++)
  {
    int is_null= (null_bits[null_bit_index / 8] 
                  >> (null_bit_index % 8))  & 0x01;

    if (bitmap_is_set(cols_bitmap, i) == 0)
      continue;
    
    my_b_printf(file, "###   @%d=", static_cast<int>(i + 1));
    if (!is_null)
    {
      size_t fsize= td->calc_field_size((uint)i, (uchar*) value);
      if (value + fsize > m_rows_end)
      {
        my_b_printf(file, "***Corrupted replication event was detected."
                    " Not printing the value***\n");
        value+= fsize;
        return 0;
      }
    }
    size_t size= log_event_print_value(file,is_null? NULL: value,
                                         td->type(i), td->field_metadata(i),
                                         typestr, sizeof(typestr));
    if (!size)
      return 0;

    if(!is_null)
      value+= size;

    if (print_event_info->verbose > 1)
    {
      my_b_printf(file, " /* ");

      my_b_printf(file, "%s ", typestr);
      
      my_b_printf(file, "meta=%d nullable=%d is_null=%d ",
                  td->field_metadata(i),
                  td->maybe_null(i), is_null);
      my_b_printf(file, "*/");
    }
    
    my_b_printf(file, "\n");
    
    null_bit_index++;
  }
  return value - value0;
}


/**
  Print a row event into IO cache in human readable form (in SQL format)
  
  @param[in] file              IO cache
  @param[in] print_event_into  Print parameters
*/
void Rows_log_event::print_verbose(IO_CACHE *file,
                                   PRINT_EVENT_INFO *print_event_info)
{
  // Quoted length of the identifier can be twice the original length
  char quoted_db[1 + NAME_LEN * 2 + 2];
  char quoted_table[1 + NAME_LEN * 2 + 2];
  size_t quoted_db_len, quoted_table_len;
  Table_map_log_event *map;
  table_def *td;
  const char *sql_command, *sql_clause1, *sql_clause2;
  Log_event_type general_type_code= get_general_type_code();
  
  if (m_extra_row_data)
  {
    uint8 extra_data_len= m_extra_row_data[EXTRA_ROW_INFO_LEN_OFFSET];
    uint8 extra_payload_len= extra_data_len - EXTRA_ROW_INFO_HDR_BYTES;
    assert(extra_data_len >= EXTRA_ROW_INFO_HDR_BYTES);

    my_b_printf(file, "### Extra row data format: %u, len: %u :",
                m_extra_row_data[EXTRA_ROW_INFO_FORMAT_OFFSET],
                extra_payload_len);
    if (extra_payload_len)
    {
      /*
         Buffer for hex view of string, including '0x' prefix,
         2 hex chars / byte and trailing 0
      */
      const int buff_len= 2 + (256 * 2) + 1;
      char buff[buff_len];
      str_to_hex(buff, (const char*) &m_extra_row_data[EXTRA_ROW_INFO_HDR_BYTES],
                 extra_payload_len);
      my_b_printf(file, "%s", buff);
    }
    my_b_printf(file, "\n");
  }

  switch (general_type_code) {
  case binary_log::WRITE_ROWS_EVENT:
    sql_command= "INSERT INTO";
    sql_clause1= "### SET\n";
    sql_clause2= NULL;
    break;
  case binary_log::DELETE_ROWS_EVENT:
    sql_command= "DELETE FROM";
    sql_clause1= "### WHERE\n";
    sql_clause2= NULL;
    break;
  case binary_log::UPDATE_ROWS_EVENT:
    sql_command= "UPDATE";
    sql_clause1= "### WHERE\n";
    sql_clause2= "### SET\n";
    break;
  default:
    sql_command= sql_clause1= sql_clause2= NULL;
    DBUG_ASSERT(0); /* Not possible */
  }
  
  if (!(map= print_event_info->m_table_map.get_table(m_table_id)) ||
      !(td= map->create_table_def()))
  {
    char llbuff[22];
    my_b_printf(file, "### Row event for unknown table #%s",
                llstr(m_table_id, llbuff));
    return;
  }

  /* If the write rows event contained no values for the AI */
  if (((general_type_code == binary_log::WRITE_ROWS_EVENT) &&
      (m_rows_buf==m_rows_end)))
  {
    my_b_printf(file, "### INSERT INTO `%s`.`%s` VALUES ()\n", 
                      map->get_db_name(), map->get_table_name());
    goto end;
  }

  for (const uchar *value= m_rows_buf; value < m_rows_end; )
  {
    size_t length;
#ifdef MYSQL_SERVER
    quoted_db_len= my_strmov_quoted_identifier(this->thd, (char *) quoted_db,
                                        map->get_db_name(), 0);
    quoted_table_len= my_strmov_quoted_identifier(this->thd,
                                                  (char *) quoted_table,
                                                  map->get_table_name(), 0);
#else
    quoted_db_len= my_strmov_quoted_identifier((char *) quoted_db,
                                               map->get_db_name());
    quoted_table_len= my_strmov_quoted_identifier((char *) quoted_table,
                                          map->get_table_name());
#endif
    quoted_db[quoted_db_len]= '\0';
    quoted_table[quoted_table_len]= '\0';
    my_b_printf(file, "### %s %s.%s\n",
                      sql_command,
                      quoted_db, quoted_table);
    /* Print the first image */
    if (!(length= print_verbose_one_row(file, td, print_event_info,
                                  &m_cols, value,
                                  (const uchar*) sql_clause1)))
      goto end;
    value+= length;

    /* Print the second image (for UPDATE only) */
    if (sql_clause2)
    {
      if (!(length= print_verbose_one_row(file, td, print_event_info,
                                      &m_cols_ai, value,
                                      (const uchar*) sql_clause2)))
        goto end;
      value+= length;
    }
  }

end:
  delete td;
}

#ifdef MYSQL_CLIENT
void free_table_map_log_event(Table_map_log_event *event)
{
  delete event;
}
#endif

void Log_event::print_base64(IO_CACHE* file,
                             PRINT_EVENT_INFO* print_event_info,
                             bool more)
{
  const uchar *ptr= (const uchar *)temp_buf;
  uint32 size= uint4korr(ptr + EVENT_LEN_OFFSET);
  DBUG_ENTER("Log_event::print_base64");

  uint64 const tmp_str_sz= base64_needed_encoded_length((uint64) size);
  char *const tmp_str= (char *) my_malloc(key_memory_log_event,
                                          tmp_str_sz, MYF(MY_WME));
  if (!tmp_str) {
    fprintf(stderr, "\nError: Out of memory. "
            "Could not print correct binlog event.\n");
    DBUG_VOID_RETURN;
  }

  if (base64_encode(ptr, (size_t) size, tmp_str))
  {
    DBUG_ASSERT(0);
  }

  if (print_event_info->base64_output_mode != BASE64_OUTPUT_DECODE_ROWS)
  {
    if (my_b_tell(file) == 0)
      my_b_printf(file, "\nBINLOG '\n");

    my_b_printf(file, "%s\n", tmp_str);

    if (!more)
      my_b_printf(file, "'%s\n", print_event_info->delimiter);
  }
  
  if (print_event_info->verbose)
  {
    Rows_log_event *ev= NULL;
    Log_event_type et= (Log_event_type) ptr[EVENT_TYPE_OFFSET];

    if (common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF &&
        common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF)
      size-= BINLOG_CHECKSUM_LEN; // checksum is displayed through the header

    const Format_description_event fd_evt=
          Format_description_event(glob_description_event->binlog_version,
                                   server_version);
    switch(et)
    {
    case binary_log::TABLE_MAP_EVENT:
    {
      Table_map_log_event *map;
      map= new Table_map_log_event((const char*) ptr, size,
                                   &fd_evt);
      print_event_info->m_table_map.set_table(map->get_table_id(), map);
      break;
    }
    case binary_log::WRITE_ROWS_EVENT:
    case binary_log::WRITE_ROWS_EVENT_V1:
    {
      ev= new Write_rows_log_event((const char*) ptr, size,
                                   &fd_evt);
      break;
    }
    case binary_log::DELETE_ROWS_EVENT:
    case binary_log::DELETE_ROWS_EVENT_V1:
    {
      ev= new Delete_rows_log_event((const char*) ptr, size,
                                    &fd_evt);
      break;
    }
    case binary_log::UPDATE_ROWS_EVENT:
    case binary_log::UPDATE_ROWS_EVENT_V1:
    {
      ev= new Update_rows_log_event((const char*) ptr, size,
                                    &fd_evt);
      break;
    }
    default:
      break;
    }
    
    if (ev)
    {
      ev->print_verbose(&print_event_info->footer_cache, print_event_info);
      delete ev;
    }
  }
    
  my_free(tmp_str);
  DBUG_VOID_RETURN;
}


/*
  Log_event::print_timestamp()
*/

void Log_event::print_timestamp(IO_CACHE* file, time_t *ts)
{
  struct tm *res;
  /*
    In some Windows versions timeval.tv_sec is defined as "long",
    not as "time_t" and can be of a different size.
    Let's use a temporary time_t variable to execute localtime()
    with a correct argument type.
  */
  time_t ts_tmp= ts ? *ts : (ulong)common_header->when.tv_sec;
  DBUG_ENTER("Log_event::print_timestamp");
  struct tm tm_tmp;
  localtime_r(&ts_tmp, (res= &tm_tmp));
  my_b_printf(file,"%02d%02d%02d %2d:%02d:%02d",
              res->tm_year % 100,
              res->tm_mon+1,
              res->tm_mday,
              res->tm_hour,
              res->tm_min,
              res->tm_sec);
  DBUG_VOID_RETURN;
}

#endif /* MYSQL_CLIENT */


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
inline Log_event::enum_skip_reason
Log_event::continue_group(Relay_log_info *rli)
{
  if (rli->slave_skip_counter == 1)
    return Log_event::EVENT_SKIP_IGNORE;
  return Log_event::do_shall_skip(rli);
}

/**
   @param end_group_sets_max_dbs  when true the group terminal event 
                          can carry partition info, see a note below.
   @return true  in cases the current event
                 carries partition data,
           false otherwise

   @note Some events combination may force to adjust partition info.
         In particular BEGIN, BEGIN_LOAD_QUERY_EVENT, COMMIT
         where none of the events holds partitioning data
         causes the sequential applying of the group through
         assigning OVER_MAX_DBS_IN_EVENT_MTS to mts_accessed_dbs
         of the group terminator (e.g COMMIT query) event.
*/
bool Log_event::contains_partition_info(bool end_group_sets_max_dbs)
{
  bool res;

  switch (get_type_code()) {
  case binary_log::TABLE_MAP_EVENT:
  case binary_log::EXECUTE_LOAD_QUERY_EVENT:
    res= true;

    break;

  case binary_log::QUERY_EVENT:
  {
    Query_log_event *qev= static_cast<Query_log_event*>(this);
    if ((ends_group() && end_group_sets_max_dbs) ||
        (qev->is_query_prefix_match(STRING_WITH_LEN("XA COMMIT")) ||
         qev->is_query_prefix_match(STRING_WITH_LEN("XA ROLLBACK"))))
    {
      res= true;
      qev->mts_accessed_dbs= OVER_MAX_DBS_IN_EVENT_MTS;
    }
    else
      res= (!ends_group() && !starts_group()) ? true : false;
    break;
  }
  default:
    res= false;
  }

  return res;
}
/*
  SYNOPSIS
    This function assigns a parent ID to the job group being scheduled in parallel.
    It also checks if we can schedule the new event in parallel with the previous ones
    being executed.

  @param        ev log event that has to be scheduled next.
  @param       rli Pointer to coordinato's relay log info.
  @return      true if error
               false otherwise
 */
bool schedule_next_event(Log_event* ev, Relay_log_info* rli)
{
  int error;
  // Check if we can schedule this event
  error= rli->current_mts_submode->schedule_next_event(rli, ev);
  switch (error)
  {
  case ER_MTS_CANT_PARALLEL:
    char llbuff[22];
    llstr(rli->get_event_relay_log_pos(), llbuff);
    my_error(ER_MTS_CANT_PARALLEL, MYF(0),
    ev->get_type_str(), rli->get_event_relay_log_name(), llbuff,
             "The master event is logically timestamped incorrectly.");
    return true;
  case ER_MTS_INCONSISTENT_DATA:
    /* Don't have to do anything. */
    return true;
  case -1:
    /* Unable to schedule: wait_for_last_committed_trx has failed */
    return true;
  default:
    return false;
  }
  /* Keep compiler happy */
  return false;
}


/**
   The method maps the event to a Worker and return a pointer to it.
   Sending the event to the Worker is done by the caller.

   Irrespective of the type of Group marking (DB partioned or BGC) the
   following holds true:

   - recognize the beginning of a group to allocate the group descriptor
     and queue it;
   - associate an event with a Worker (which also handles possible conflicts
     detection and waiting for their termination);
   - finalize the group assignement when the group closing event is met.

   When parallelization mode is BGC-based the partitioning info in the event
   is simply ignored. Thereby association with a Worker does not require
   Assigned Partition Hash of the partitioned method.
   This method is not interested in all the taxonomy of the event group
   property, what we care about is the boundaries of the group.

   As a part of the group, an event belongs to one of the following types:

   B - beginning of a group of events (BEGIN query_log_event)
   g - mini-group representative event containing the partition info
      (any Table_map, a Query_log_event)
   p - a mini-group internal event that *p*receeding its g-parent
      (int_, rand_, user_ var:s)
   r - a mini-group internal "regular" event that follows its g-parent
      (Delete, Update, Write -rows)
   T - terminator of the group (XID, COMMIT, ROLLBACK, auto-commit query)

   Only the first g-event computes the assigned Worker which once
   is determined remains to be for the rest of the group.
   That is the g-event solely carries partitioning info.
   For B-event the assigned Worker is NULL to indicate Coordinator
   has not yet decided. The same applies to p-event.

   Notice, these is a special group consisting of optionally multiple p-events
   terminating with a g-event.
   Such case is caused by old master binlog and a few corner-cases of
   the current master version (todo: to fix).

   In case of the event accesses more than OVER_MAX_DBS the method
   has to ensure sure previously assigned groups to all other workers are
   done.


   @note The function updates GAQ queue directly, updates APH hash
         plus relocates some temporary tables from Coordinator's list into
         involved entries of APH through @c map_db_to_worker.
         There's few memory allocations commented where to be freed.

   @return a pointer to the Worker struct or NULL.
*/

Slave_worker *Log_event::get_slave_worker(Relay_log_info *rli)
{
  Slave_job_group group= Slave_job_group(), *ptr_group= NULL;
  bool is_s_event;
  Slave_worker *ret_worker= NULL;
  char llbuff[22];
  Slave_committed_queue *gaq= rli->gaq;
  DBUG_ENTER("Log_event::get_slave_worker");

  /* checking partioning properties and perform corresponding actions */

  // Beginning of a group designated explicitly with BEGIN or GTID
  if ((is_s_event= starts_group()) || is_gtid_event(this) ||
      // or DDL:s or autocommit queries possibly associated with own p-events
      (!rli->curr_group_seen_begin && !rli->curr_group_seen_gtid &&
       /*
         the following is a special case of B-free still multi-event group like
         { p_1,p_2,...,p_k, g }.
         In that case either GAQ is empty (the very first group is being
         assigned) or the last assigned group index points at one of
         mapped-to-a-worker.
       */
       (gaq->empty() ||
        gaq->get_job_group(rli->gaq->assigned_group_index)->
        worker_id != MTS_WORKER_UNDEF)))
  {
    if (!rli->curr_group_seen_gtid && !rli->curr_group_seen_begin)
    {
      rli->mts_groups_assigned++;

      rli->curr_group_isolated= FALSE;
      group.reset(common_header->log_pos, rli->mts_groups_assigned);
      // the last occupied GAQ's array index
      gaq->assigned_group_index= gaq->en_queue(&group);
      DBUG_PRINT("info",("gaq_idx= %ld  gaq->size=%ld",
                         gaq->assigned_group_index,
                         gaq->size));
      DBUG_ASSERT(gaq->assigned_group_index != MTS_WORKER_UNDEF);
      DBUG_ASSERT(gaq->assigned_group_index < gaq->size);
      DBUG_ASSERT(gaq->get_job_group(rli->gaq->assigned_group_index)->
                  group_relay_log_name == NULL);
      DBUG_ASSERT(rli->last_assigned_worker == NULL ||
                  !is_mts_db_partitioned(rli));

      if (is_s_event || is_gtid_event(this))
      {
        Slave_job_item job_item= {this, rli->get_event_relay_log_number(),
                                  rli->get_event_start_pos()};
        // B-event is appended to the Deferred Array associated with GCAP
        rli->curr_group_da.push_back(job_item);

        DBUG_ASSERT(rli->curr_group_da.size() == 1);

        if (starts_group())
        {
          // mark the current group as started with explicit B-event
          rli->mts_end_group_sets_max_dbs= true;
          rli->curr_group_seen_begin= true;
        }

        if (is_gtid_event(this))
          // mark the current group as started with explicit Gtid-event
          rli->curr_group_seen_gtid= true;
        if (schedule_next_event(this, rli))
        {
          rli->abort_slave= 1;
          DBUG_RETURN(NULL);
        }
        DBUG_RETURN(ret_worker);
      }
    }
    else
    {
      /*
       The block is a result of not making GTID event as group starter.
       TODO: Make GITD event as B-event that is starts_group() to
       return true.
      */
      Slave_job_item job_item= {this, rli->get_event_relay_log_number(),
                                rli->get_event_relay_log_pos()};

      // B-event is appended to the Deferred Array associated with GCAP
      rli->curr_group_da.push_back(job_item);
      rli->curr_group_seen_begin= true;
      rli->mts_end_group_sets_max_dbs= true;
      if (!rli->curr_group_seen_gtid && schedule_next_event(this, rli))
      {
        rli->abort_slave= 1;
        DBUG_RETURN(NULL);
      }

      DBUG_ASSERT(rli->curr_group_da.size() == 2);
      DBUG_ASSERT(starts_group());
      DBUG_RETURN (ret_worker);
    }
    if (schedule_next_event(this, rli))
    {
      rli->abort_slave= 1;
      DBUG_RETURN(NULL);
    }
  }

  ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
  if (!is_mts_db_partitioned(rli))
  {
    /* Get least occupied worker */
    ret_worker=
      rli->current_mts_submode->get_least_occupied_worker(rli, &rli->workers,
                                                          this);
    if (ret_worker == NULL)
    {
      /* get_least_occupied_worker may return NULL if the thread is killed */
      Slave_job_item job_item= {this, rli->get_event_relay_log_number(),
                                rli->get_event_start_pos()};
      rli->curr_group_da.push_back(job_item);

      DBUG_ASSERT(thd->killed);
      DBUG_RETURN(NULL);
    }
    ptr_group->worker_id= ret_worker->id;
  }
  else if (contains_partition_info(rli->mts_end_group_sets_max_dbs))
  {
    int i= 0;
    Mts_db_names mts_dbs;

    get_mts_dbs(&mts_dbs);
    /*
      Bug 12982188 - MTS: SBR ABORTS WITH ERROR 1742 ON LOAD DATA
      Logging on master can create a group with no events holding
      the partition info.
      The following assert proves there's the only reason
      for such group.
    */
#ifndef DBUG_OFF
    {
      bool empty_group_with_gtids= rli->curr_group_seen_begin &&
                                   rli->curr_group_seen_gtid &&
                                   ends_group();

      bool begin_load_query_event=
        ((rli->curr_group_da.size() == 3 && rli->curr_group_seen_gtid) ||
         (rli->curr_group_da.size() == 2 && !rli->curr_group_seen_gtid)) &&
        (rli->curr_group_da.back().data->
         get_type_code() == binary_log::BEGIN_LOAD_QUERY_EVENT);

      bool delete_file_event=
        ((rli->curr_group_da.size() == 4 && rli->curr_group_seen_gtid) ||
         (rli->curr_group_da.size() == 3 && !rli->curr_group_seen_gtid)) &&
        (rli->curr_group_da.back().data->
         get_type_code() == binary_log::DELETE_FILE_EVENT);

      DBUG_ASSERT((!ends_group() ||
                   (get_type_code() == binary_log::QUERY_EVENT &&
                    static_cast<Query_log_event*>(this)->
                    is_query_prefix_match(STRING_WITH_LEN("XA ROLLBACK")))) ||
                  empty_group_with_gtids ||
                  (rli->mts_end_group_sets_max_dbs &&
                   (begin_load_query_event || delete_file_event)));
    }
#endif

    // partioning info is found which drops the flag
    rli->mts_end_group_sets_max_dbs= false;
    ret_worker= rli->last_assigned_worker;
    if (mts_dbs.num == OVER_MAX_DBS_IN_EVENT_MTS)
    {
      // Worker with id 0 to handle serial execution
      if (!ret_worker)
        ret_worker= rli->workers.at(0);
      // No need to know a possible error out of synchronization call.
      (void) rli->current_mts_submode->
        wait_for_workers_to_finish(rli, ret_worker);
      /*
        this marking is transferred further into T-event of the current group.
      */
      rli->curr_group_isolated= TRUE;
    }

    /* One run of the loop in the case of over-max-db:s */
    for (i= 0; i < ((mts_dbs.num != OVER_MAX_DBS_IN_EVENT_MTS) ? mts_dbs.num : 1);
         i++)
    {
      /*
        The over max db:s case handled through passing to map_db_to_worker
        such "all" db as encoded as  the "" empty string.
        Note, the empty string is allocated in a large buffer
        to satisfy hashcmp() implementation.
      */
      const char all_db[NAME_LEN]= {0};
      if (!(ret_worker=
            map_db_to_worker(mts_dbs.num == OVER_MAX_DBS_IN_EVENT_MTS ?
                             all_db : mts_dbs.name[i], rli,
                             &mts_assigned_partitions[i],
                             /*
                               todo: optimize it. Although pure
                               rows- event load in insensetive to the flag value
                             */
                             TRUE,
                             ret_worker)))
      {
        llstr(rli->get_event_relay_log_pos(), llbuff);
        my_error(ER_MTS_CANT_PARALLEL, MYF(0),
                 get_type_str(), rli->get_event_relay_log_name(), llbuff,
                 "could not distribute the event to a Worker");
        DBUG_RETURN(ret_worker);
      }
      // all temporary tables are transferred from Coordinator in over-max case
      DBUG_ASSERT(mts_dbs.num != OVER_MAX_DBS_IN_EVENT_MTS || !thd->temporary_tables);
      DBUG_ASSERT(!strcmp(mts_assigned_partitions[i]->db,
                          mts_dbs.num != OVER_MAX_DBS_IN_EVENT_MTS ?
                          mts_dbs.name[i] : all_db));
      DBUG_ASSERT(ret_worker == mts_assigned_partitions[i]->worker);
      DBUG_ASSERT(mts_assigned_partitions[i]->usage >= 0);
    }

    if ((ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index))->
        worker_id == MTS_WORKER_UNDEF)
    {
      ptr_group->worker_id= ret_worker->id;

      DBUG_ASSERT(ptr_group->group_relay_log_name == NULL);
    }

    DBUG_ASSERT(i == mts_dbs.num || mts_dbs.num == OVER_MAX_DBS_IN_EVENT_MTS);
  }
  else
  {
    // a mini-group internal "regular" event
    if (rli->last_assigned_worker)
    {
      ret_worker= rli->last_assigned_worker;

      DBUG_ASSERT(rli->curr_group_assigned_parts.size() > 0 ||
                  ret_worker->id == 0);
    }
    else // int_, rand_, user_ var:s, load-data events
    {

      if (!(get_type_code() == binary_log::INTVAR_EVENT ||
            get_type_code() == binary_log::RAND_EVENT ||
            get_type_code() == binary_log::USER_VAR_EVENT ||
            get_type_code() == binary_log::BEGIN_LOAD_QUERY_EVENT ||
            get_type_code() == binary_log::APPEND_BLOCK_EVENT ||
            get_type_code() == binary_log::DELETE_FILE_EVENT ||
            is_ignorable_event()))
      {
        DBUG_ASSERT(!ret_worker);

        llstr(rli->get_event_relay_log_pos(), llbuff);
        my_error(ER_MTS_CANT_PARALLEL, MYF(0),
                 get_type_str(), rli->get_event_relay_log_name(), llbuff,
                 "the event is a part of a group that is unsupported in "
                 "the parallel execution mode");

        DBUG_RETURN(ret_worker);
      }
      /*
        In the logical clock scheduler any internal gets scheduled directly.
        That is Int_var, @User_var and Rand bypass the deferred array.
        Their association with relay-log physical coordinates is provided
        by the same mechanism that applies to a regular event.
      */
      Slave_job_item job_item= {this, rli->get_event_relay_log_number(),
                                rli->get_event_start_pos()};
      rli->curr_group_da.push_back(job_item);

      DBUG_ASSERT(!ret_worker);
      DBUG_RETURN (ret_worker);
    }
  }

  DBUG_ASSERT(ret_worker);
  // T-event: Commit, Xid, a DDL query or dml query of B-less group.4

  /*
    Preparing event physical coordinates info for Worker before any
    event got scheduled so when Worker error-stopped at the first
    event it would be aware of where exactly in the event stream.
  */
  if (!ret_worker->master_log_change_notified)
  {
    if (!ptr_group)
      ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
    ptr_group->group_master_log_name=
      my_strdup(key_memory_log_event, rli->get_group_master_log_name(), MYF(MY_WME));
    ret_worker->master_log_change_notified= true;

    DBUG_ASSERT(!ptr_group->notified);
#ifndef DBUG_OFF
    ptr_group->notified= true;
#endif
  }

  /* Notify the worker about new FD */
  if (!ret_worker->fd_change_notified)
  {
    if (!ptr_group)
      ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
    /*
      Increment the usage counter on behalf of Worker.
      This avoids inadvertent FD deletion in a race case where Coordinator
      would install a next new FD before Worker has noticed the previous one.
    */
    rli->get_rli_description_event()->usage_counter.atomic_add(1);
    ptr_group->new_fd_event= rli->get_rli_description_event();
    ret_worker->fd_change_notified= true;
  }

  if (ends_group() ||
      (!rli->curr_group_seen_begin &&
       (get_type_code() == binary_log::QUERY_EVENT ||
        /*
          When applying an old binary log without Gtid_log_event and
          Anonymous_gtid_log_event, the logic of multi-threaded slave
          still need to require that an event (for example, Query_log_event,
          User_var_log_event, Intvar_log_event, and Rand_log_event) that
          appeared outside of BEGIN...COMMIT was treated as a transaction
          of its own. This was just a technicality in the code and did not
          cause a problem, since the event and the following Query_log_event
          would both be assigned to dedicated worker 0.
        */
        !rli->curr_group_seen_gtid)))
  {
    rli->mts_group_status= Relay_log_info::MTS_END_GROUP;
    if (rli->curr_group_isolated)
      set_mts_isolate_group();
    if (!ptr_group)
      ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);

    DBUG_ASSERT(ret_worker != NULL);

    /*
      The following two blocks are executed if the worker has not been
      notified about new relay-log or a new checkpoints.
      Relay-log string is freed by Coordinator, Worker deallocates
      strings in the checkpoint block.
      However if the worker exits earlier reclaiming for both happens anyway at
      GAQ delete.
    */
    if (!ret_worker->relay_log_change_notified)
    {
      /*
        Prior this event, C rotated the relay log to drop each
        Worker's notified flag. Now group terminating event initiates
        the new relay-log (where the current event is from) name
        delivery to Worker that will receive it in commit_positions().
      */
      DBUG_ASSERT(ptr_group->group_relay_log_name == NULL);

      ptr_group->group_relay_log_name= (char *)
        my_malloc(key_memory_log_event,
                  strlen(rli->
                         get_group_relay_log_name()) + 1, MYF(MY_WME));
      strcpy(ptr_group->group_relay_log_name,
             rli->get_event_relay_log_name());

      DBUG_ASSERT(ptr_group->group_relay_log_name != NULL);

      ret_worker->relay_log_change_notified= TRUE;
    }

    if (!ret_worker->checkpoint_notified)
    {
      if (!ptr_group)
        ptr_group= gaq->get_job_group(rli->gaq->assigned_group_index);
      ptr_group->checkpoint_log_name=
        my_strdup(key_memory_log_event, rli->get_group_master_log_name(), MYF(MY_WME));
      ptr_group->checkpoint_log_pos= rli->get_group_master_log_pos();
      ptr_group->checkpoint_relay_log_name=
        my_strdup(key_memory_log_event, rli->get_group_relay_log_name(), MYF(MY_WME));
      ptr_group->checkpoint_relay_log_pos= rli->get_group_relay_log_pos();
      ptr_group->shifted= ret_worker->bitmap_shifted;
      ret_worker->bitmap_shifted= 0;
      ret_worker->checkpoint_notified= TRUE;
    }
    ptr_group->checkpoint_seqno= rli->checkpoint_seqno;
    ptr_group->ts= common_header->when.tv_sec + (time_t) exec_time; // Seconds_behind_master related
    rli->checkpoint_seqno++;
    /*
      Coordinator should not use the main memroot however its not
      reset elsewhere either, so let's do it safe way.
      The main mem root is also reset by the SQL thread in at the end
      of applying which Coordinator does not do in this case.
      That concludes the memroot reset can't harm anything in SQL thread roles
      after Coordinator has finished its current scheduling.
    */
    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));

#ifndef DBUG_OFF
    w_rr++;
#endif

  }

  DBUG_RETURN (ret_worker);
}


int Log_event::apply_gtid_event(Relay_log_info *rli)
{
  DBUG_ENTER("LOG_EVENT:apply_gtid_event");

  int error= 0;
  if (rli->curr_group_da.size() < 1)
    DBUG_RETURN(1);

  Log_event* ev= rli->curr_group_da[0].data;
  DBUG_ASSERT(ev->get_type_code() == binary_log::GTID_LOG_EVENT ||
              ev->get_type_code() ==
              binary_log::ANONYMOUS_GTID_LOG_EVENT);

  error= ev->do_apply_event(rli);
  /* Clean up */
  delete ev;
  rli->curr_group_da.clear();
  rli->curr_group_seen_gtid= false;
  /*
    Removes the job from the (G)lobal (A)ssigned (Q)ueue after
    applying it.
  */
  DBUG_ASSERT(rli->gaq->len > 0);
  Slave_job_group g= Slave_job_group();
  rli->gaq->de_tail(&g);
  /*
    The rli->mts_groups_assigned is increased when adding the slave job
    generated for the gtid into the (G)lobal (A)ssigned (Q)ueue. So we
    decrease it here.
  */
  rli->mts_groups_assigned--;

  DBUG_RETURN(error);
}


/**
   Scheduling event to execute in parallel or execute it directly.
   In MTS case the event gets associated with either Coordinator or a
   Worker.  A special case of the association is NULL when the Worker
   can't be decided yet.  In the single threaded sequential mode the
   event maps to SQL thread rli.

   @note in case of MTS failure Coordinator destroys all gathered
         deferred events.

   @return 0 as success, otherwise a failure.
*/
int Log_event::apply_event(Relay_log_info *rli)
{
  DBUG_ENTER("LOG_EVENT:apply_event");
  DBUG_PRINT("info", ("event_type=%s", get_type_str()));
  bool parallel= FALSE;
  enum enum_mts_event_exec_mode actual_exec_mode= EVENT_EXEC_PARALLEL;
  THD *rli_thd= rli->info_thd;

  worker= rli;

  if (rli->is_mts_recovery())
  {
    bool skip=
      bitmap_is_set(&rli->recovery_groups, rli->mts_recovery_index) &&
      (get_mts_execution_mode(::server_id,
                              rli->mts_group_status ==
                              Relay_log_info::MTS_IN_GROUP,
                              rli->current_mts_submode->get_type() ==
                              MTS_PARALLEL_TYPE_DB_NAME)
       == EVENT_EXEC_PARALLEL);
    if (skip)
    {
      DBUG_RETURN(0);
    }
    else
    {
      DBUG_RETURN(do_apply_event(rli));
    }
  }

  if (!(parallel= rli->is_parallel_exec()) ||
      ((actual_exec_mode=
        get_mts_execution_mode(::server_id,
                               rli->mts_group_status ==
                               Relay_log_info::MTS_IN_GROUP,
                               rli->current_mts_submode->get_type() ==
                               MTS_PARALLEL_TYPE_DB_NAME))
       != EVENT_EXEC_PARALLEL))
  {
    if (parallel)
    {
      /*
         There are two classes of events that Coordinator executes
         itself. One e.g the master Rotate requires all Workers to finish up
         their assignments. The other async class, e.g the slave Rotate,
         can't have this such synchronization because Worker might be waiting
         for terminal events to finish.
      */

      if (actual_exec_mode != EVENT_EXEC_ASYNC)
      {
        /*
          this  event does not split the current group but is indeed
          a separator beetwen two master's binlog therefore requiring
          Workers to sync.
        */
        if (rli->curr_group_da.size() > 0 &&
            is_mts_db_partitioned(rli) &&
            get_type_code() != binary_log::INCIDENT_EVENT)
        {
          char llbuff[22];
          /*
             Possible reason is a old version binlog sequential event
             wrappped with BEGIN/COMMIT or preceeded by User|Int|Random- var.
             MTS has to stop to suggest restart in the permanent sequential mode.
          */
          llstr(rli->get_event_relay_log_pos(), llbuff);
          my_error(ER_MTS_CANT_PARALLEL, MYF(0),
                   get_type_str(), rli->get_event_relay_log_name(), llbuff,
                   "possible malformed group of events from an old master");

          /* Coordinator cant continue, it marks MTS group status accordingly */
          rli->mts_group_status= Relay_log_info::MTS_KILLED_GROUP;

          goto err;
        }

        if (get_type_code() == binary_log::INCIDENT_EVENT &&
            rli->curr_group_da.size() > 0 &&
            rli->current_mts_submode->get_type() ==
            MTS_PARALLEL_TYPE_LOGICAL_CLOCK)
        {
#ifndef DBUG_OFF
          DBUG_ASSERT(rli->curr_group_da.size() == 1);
          Log_event* ev= rli->curr_group_da[0].data;
          DBUG_ASSERT(ev->get_type_code() == binary_log::GTID_LOG_EVENT ||
                      ev->get_type_code() ==
                      binary_log::ANONYMOUS_GTID_LOG_EVENT);
#endif
          /*
            With MTS logical clock mode, when coordinator is applying an
            incident event, it must withdraw delegated_job increased by
            the incident's GTID before waiting for workers to finish.
            So that it can exit from mts_checkpoint_routine.
          */
          ((Mts_submode_logical_clock*)rli->current_mts_submode)->
            withdraw_delegated_job();
        }
        /*
          Marking sure the event will be executed in sequential mode.
        */
        if (rli->current_mts_submode->wait_for_workers_to_finish(rli) == -1)
        {
          // handle synchronization error
          rli->report(WARNING_LEVEL, 0,
                      "Slave worker thread has failed to apply an event. As a "
                      "consequence, the coordinator thread is stopping "
                      "execution.");
          DBUG_RETURN(-1);
        }
        /*
          Given not in-group mark the event handler can invoke checkpoint
          update routine in the following course.
        */
        DBUG_ASSERT(rli->mts_group_status == Relay_log_info::MTS_NOT_IN_GROUP
                    || !is_mts_db_partitioned(rli));

        if (get_type_code() == binary_log::INCIDENT_EVENT &&
            rli->curr_group_da.size() > 0)
        {
          DBUG_ASSERT(rli->curr_group_da.size() == 1);
          /*
            When MTS is enabled, the incident event must be applied by the
            coordinator. So the coordinator applies its GTID right before
            applying the incident event..
          */
          int error= apply_gtid_event(rli);
          if (error)
            DBUG_RETURN(-1);
        }

#ifndef DBUG_OFF
        /* all Workers are idle as done through wait_for_workers_to_finish */
        for (uint k= 0; k < rli->curr_group_da.size(); k++)
        {
          DBUG_ASSERT(!(rli->workers[k]->usage_partition));
          DBUG_ASSERT(!(rli->workers[k]->jobs.len));
        }
#endif
      }
      else
      {
        DBUG_ASSERT(actual_exec_mode == EVENT_EXEC_ASYNC);
      }
    }
    DBUG_RETURN(do_apply_event(rli));
  }

  DBUG_ASSERT(actual_exec_mode == EVENT_EXEC_PARALLEL);
  DBUG_ASSERT(!(rli->curr_group_seen_begin && ends_group()) ||
              /*
                This is an empty group being processed due to gtids.
              */
              (rli->curr_group_seen_begin && rli->curr_group_seen_gtid
              && ends_group()) || is_mts_db_partitioned(rli) ||
              rli->last_assigned_worker ||
              /*
                Begin_load_query can be logged w/o db info and within
                Begin/Commit. That's a pattern forcing sequential
                applying of LOAD-DATA.
              */
              (rli->curr_group_da.back().data->
               get_type_code() == binary_log::BEGIN_LOAD_QUERY_EVENT) ||
              /*
                Delete_file can also be logged w/o db info and within
                Begin/Commit. That's a pattern forcing sequential
                applying of LOAD-DATA.
              */
              (rli->curr_group_da.back().data->
               get_type_code() == binary_log::DELETE_FILE_EVENT));

  worker= NULL;
  rli->mts_group_status= Relay_log_info::MTS_IN_GROUP;

  worker= (Relay_log_info*)
    (rli->last_assigned_worker= get_slave_worker(rli));

#ifndef DBUG_OFF
  if (rli->last_assigned_worker)
    DBUG_PRINT("mts", ("Assigning job to worker %lu",
               rli->last_assigned_worker->id));
#endif

err:
  if (rli_thd->is_error() || (!worker && rli->abort_slave))
  {
    DBUG_ASSERT(!worker);

    /*
      Destroy all deferred buffered events but the current prior to exit.
      The current one will be deleted as an event never destined/assigned
      to any Worker in Coordinator's regular execution path.
    */
    for (uint k= 0; k < rli->curr_group_da.size(); k++)
    {
      Log_event *ev_buf= rli->curr_group_da[k].data;
      if (this != ev_buf)
        delete ev_buf;
    }
    rli->curr_group_da.clear();
  }
  else
  {
    DBUG_ASSERT(worker || rli->curr_group_assigned_parts.size() == 0);
  }

  DBUG_RETURN((!(rli_thd->is_error() || (!worker && rli->abort_slave)) ||
               DBUG_EVALUATE_IF("fault_injection_get_slave_worker", 1, 0)) ?
              0 : -1);
}

#endif

/**************************************************************************
	Query_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
  This (which is used only for SHOW BINLOG EVENTS) could be updated to
  print SET @@session_var=. But this is not urgent, as SHOW BINLOG EVENTS is
  only an information, it does not produce suitable queries to replay (for
  example it does not print LOAD DATA INFILE).
  @todo
    show the catalog ??
*/

int Query_log_event::pack_info(Protocol *protocol)
{
  // TODO: show the catalog ??
  String str_buf;
  // Add use `DB` to the string if required
  if (!(common_header->flags & LOG_EVENT_SUPPRESS_USE_F)
      && db && db_len)
  {
    str_buf.append("use ");
    append_identifier(this->thd, &str_buf, db, db_len);
    str_buf.append("; ");
  }
  // Add the query to the string
  if (query && q_len)
    str_buf.append(query);
 // persist the buffer in protocol
  protocol->store(str_buf.ptr(), str_buf.length(), &my_charset_bin);
  return 0;
}
#endif

#ifndef MYSQL_CLIENT

/**
  Utility function for the next method (Query_log_event::write()) .
*/
static void write_str_with_code_and_len(uchar **dst, const char *src,
                                        size_t len, uint code)
{
  /*
    only 1 byte to store the length of catalog, so it should not
    surpass 255
  */
  DBUG_ASSERT(len <= 255);
  DBUG_ASSERT(src);
  *((*dst)++)= code;
  *((*dst)++)= (uchar) len;
  memmove(*dst, src, len);
  (*dst)+= len;
}


/**
  Query_log_event::write().

  @note
    In this event we have to modify the header to have the correct
    EVENT_LEN_OFFSET as we don't yet know how many status variables we
    will print!
*/

bool Query_log_event::write(IO_CACHE* file)
{
  uchar buf[Binary_log_event::QUERY_HEADER_LEN + MAX_SIZE_LOG_EVENT_STATUS];
  uchar *start, *start_of_status;
  size_t event_length;

  if (!query)
    return 1;                                   // Something wrong with event

  /*
    We want to store the thread id:
    (- as an information for the user when he reads the binlog)
    - if the query uses temporary table: for the slave SQL thread to know to
    which master connection the temp table belongs.
    Now imagine we (write()) are called by the slave SQL thread (we are
    logging a query executed by this thread; the slave runs with
    --log-slave-updates). Then this query will be logged with
    thread_id=the_thread_id_of_the_SQL_thread. Imagine that 2 temp tables of
    the same name were created simultaneously on the master (in the masters
    binlog you have
    CREATE TEMPORARY TABLE t; (thread 1)
    CREATE TEMPORARY TABLE t; (thread 2)
    ...)
    then in the slave's binlog there will be
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    which is bad (same thread id!).

    To avoid this, we log the thread's thread id EXCEPT for the SQL
    slave thread for which we log the original (master's) thread id.
    Now this moves the bug: what happens if the thread id on the
    master was 10 and when the slave replicates the query, a
    connection number 10 is opened by a normal client on the slave,
    and updates a temp table of the same name? We get a problem
    again. To avoid this, in the handling of temp tables (sql_base.cc)
    we use thread_id AND server_id.  TODO when this is merged into
    4.1: in 4.1, slave_proxy_id has been renamed to pseudo_thread_id
    and is a session variable: that's to make mysqlbinlog work with
    temp tables. We probably need to introduce

    SET PSEUDO_SERVER_ID
    for mysqlbinlog in 4.1. mysqlbinlog would print:
    SET PSEUDO_SERVER_ID=
    SET PSEUDO_THREAD_ID=
    for each query using temp tables.
  */
  int4store(buf + Q_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + Q_EXEC_TIME_OFFSET, exec_time);
  buf[Q_DB_LEN_OFFSET] = (char) db_len;
  int2store(buf + Q_ERR_CODE_OFFSET, error_code);

  /*
    You MUST always write status vars in increasing order of code. This
    guarantees that a slightly older slave will be able to parse those he
    knows.
  */
  start_of_status= start= buf+Binary_log_event::QUERY_HEADER_LEN;
  if (flags2_inited)
  {
    *start++= Q_FLAGS2_CODE;
    int4store(start, flags2);
    start+= 4;
  }
  if (sql_mode_inited)
  {
    *start++= Q_SQL_MODE_CODE;
    int8store(start, sql_mode);
    start+= 8;
  }
  if (catalog_len) // i.e. this var is inited (false for 4.0 events)
  {
    write_str_with_code_and_len(&start,
                                catalog, catalog_len, Q_CATALOG_NZ_CODE);
    /*
      In 5.0.x where x<4 masters we used to store the end zero here. This was
      a waste of one byte so we don't do it in x>=4 masters. We change code to
      Q_CATALOG_NZ_CODE, because re-using the old code would make x<4 slaves
      of this x>=4 master segfault (expecting a zero when there is
      none). Remaining compatibility problems are: the older slave will not
      find the catalog; but it is will not crash, and it's not an issue
      that it does not find the catalog as catalogs were not used in these
      older MySQL versions (we store it in binlog and read it from relay log
      but do nothing useful with it). What is an issue is that the older slave
      will stop processing the Q_* blocks (and jumps to the db/query) as soon
      as it sees unknown Q_CATALOG_NZ_CODE; so it will not be able to read
      Q_AUTO_INCREMENT*, Q_CHARSET and so replication will fail silently in
      various ways. Documented that you should not mix alpha/beta versions if
      they are not exactly the same version, with example of 5.0.3->5.0.2 and
      5.0.4->5.0.3. If replication is from older to new, the new will
      recognize Q_CATALOG_CODE and have no problem.
    */
  }
  if (auto_increment_increment != 1 || auto_increment_offset != 1)
  {
    *start++= Q_AUTO_INCREMENT;
    int2store(start, static_cast<uint16>(auto_increment_increment));
    int2store(start+2, static_cast<uint16>(auto_increment_offset));
    start+= 4;
  }
  if (charset_inited)
  {
    *start++= Q_CHARSET_CODE;
    memcpy(start, charset, 6);
    start+= 6;
  }
  if (time_zone_len)
  {
    /* In the TZ sys table, column Name is of length 64 so this should be ok */
    DBUG_ASSERT(time_zone_len <= MAX_TIME_ZONE_NAME_LENGTH);
    write_str_with_code_and_len(&start,
                                time_zone_str, time_zone_len, Q_TIME_ZONE_CODE);
  }
  if (lc_time_names_number)
  {
    DBUG_ASSERT(lc_time_names_number <= 0xFF);
    *start++= Q_LC_TIME_NAMES_CODE;
    int2store(start, lc_time_names_number);
    start+= 2;
  }
  if (charset_database_number)
  {
    DBUG_ASSERT(charset_database_number <= 0xFF);
    *start++= Q_CHARSET_DATABASE_CODE;
    int2store(start, charset_database_number);
    start+= 2;
  }
  if (table_map_for_update)
  {
    *start++= Q_TABLE_MAP_FOR_UPDATE_CODE;
    int8store(start, table_map_for_update);
    start+= 8;
  }
  if (master_data_written != 0)
  {
    /*
      Q_MASTER_DATA_WRITTEN_CODE only exists in relay logs where the master
      has binlog_version<4 and the slave has binlog_version=4. See comment
      for master_data_written in log_event.h for details.
    */
    *start++= Q_MASTER_DATA_WRITTEN_CODE;
    int4store(start, static_cast<uint32>(master_data_written));
    start+= 4;
  }

  if (thd && thd->need_binlog_invoker())
  {
    LEX_CSTRING invoker_user;
    LEX_CSTRING invoker_host;
    memset(&invoker_user, 0, sizeof(invoker_user));
    memset(&invoker_host, 0, sizeof(invoker_host));

    if (thd->slave_thread && thd->has_invoker())
    {
      /* user will be null, if master is older than this patch */
      invoker_user= thd->get_invoker_user();
      invoker_host= thd->get_invoker_host();
    }
    else
    {
      Security_context *ctx= thd->security_context();
      LEX_CSTRING priv_user= ctx->priv_user();
      LEX_CSTRING priv_host= ctx->priv_host();

      invoker_user.length= priv_user.length;
      invoker_user.str= (char *) priv_user.str;
      if (priv_host.str[0] != '\0')
      {
        invoker_host.str= (char *) priv_host.str;
        invoker_host.length= priv_host.length;
      }
    }

    *start++= Q_INVOKER;

    /*
      Store user length and user. The max length of use is 16, so 1 byte is
      enough to store the user's length.
     */
    *start++= (uchar)invoker_user.length;
    memcpy(start, invoker_user.str, invoker_user.length);
    start+= invoker_user.length;

    /*
      Store host length and host. The max length of host is 60, so 1 byte is
      enough to store the host's length.
     */
    *start++= (uchar)invoker_host.length;
    memcpy(start, invoker_host.str, invoker_host.length);
    start+= invoker_host.length;
  }

  if (thd && thd->get_binlog_accessed_db_names() != NULL)
  {
    uchar dbs;
    *start++= Q_UPDATED_DB_NAMES;

    compile_time_assert(MAX_DBS_IN_EVENT_MTS <= OVER_MAX_DBS_IN_EVENT_MTS);

    /* 
       In case of the number of db:s exceeds MAX_DBS_IN_EVENT_MTS
       no db:s is written and event will require the sequential applying on slave.
    */
    dbs=
      (thd->get_binlog_accessed_db_names()->elements <= MAX_DBS_IN_EVENT_MTS) ?
      thd->get_binlog_accessed_db_names()->elements : OVER_MAX_DBS_IN_EVENT_MTS;

    DBUG_ASSERT(dbs != 0);

    if (dbs <= MAX_DBS_IN_EVENT_MTS)
    {
      List_iterator_fast<char> it(*thd->get_binlog_accessed_db_names());
      char *db_name= it++;
      /* 
         the single "" db in the acccessed db list corresponds to the same as
         exceeds MAX_DBS_IN_EVENT_MTS case, so dbs is set to the over-max.
      */
      if (dbs == 1 && !strcmp(db_name, ""))
        dbs= OVER_MAX_DBS_IN_EVENT_MTS;
      *start++= dbs;
      if (dbs != OVER_MAX_DBS_IN_EVENT_MTS)
        do
        {
          strcpy((char*) start, db_name);
          start += strlen(db_name) + 1;
        } while ((db_name= it++));
    }
    else
    {
      *start++= dbs;
    }
  }

  if (thd && thd->query_start_usec_used)
  {
    *start++= Q_MICROSECONDS;
    get_time();
    int3store(start, common_header->when.tv_usec);
    start+= 3;
  }

  if (thd && thd->binlog_need_explicit_defaults_ts == true)
  {
    *start++= Q_EXPLICIT_DEFAULTS_FOR_TIMESTAMP;
    *start++= thd->variables.explicit_defaults_for_timestamp;
  }
  /*
    NOTE: When adding new status vars, please don't forget to update
    the MAX_SIZE_LOG_EVENT_STATUS in log_event.h

    Here there could be code like
    if (command-line-option-which-says-"log_this_variable" && inited)
    {
    *start++= Q_THIS_VARIABLE_CODE;
    int4store(start, this_variable);
    start+= 4;
    }
  */

  /* Store length of status variables */
  status_vars_len= (uint) (start-start_of_status);
  DBUG_ASSERT(status_vars_len <= MAX_SIZE_LOG_EVENT_STATUS);
  int2store(buf + Q_STATUS_VARS_LEN_OFFSET, status_vars_len);

  /*
    Calculate length of whole event
    The "1" below is the \0 in the db's length
  */
  event_length= (uint) (start-buf) + get_post_header_size_for_derived() + db_len + 1 + q_len;

  return (write_header(file, event_length) ||
          wrapper_my_b_safe_write(file, (uchar*) buf, Binary_log_event::QUERY_HEADER_LEN) ||
          write_post_header_for_derived(file) ||
          wrapper_my_b_safe_write(file, start_of_status,
                          (uint) (start-start_of_status)) ||
          wrapper_my_b_safe_write(file, db ? (uchar*) db : (uchar*)"", db_len + 1) ||
          wrapper_my_b_safe_write(file, (uchar*) query, q_len) ||
	  write_footer(file)) ? 1 : 0;
}


/**
  The simplest constructor that could possibly work.  This is used for
  creating static objects that have a special meaning and are invisible
  to the log.
*/
Query_log_event::Query_log_event()
  : binary_log::Query_event(),
    Log_event(header(), footer()),
    data_buf(NULL)
{}

/**
  Creates a Query Log Event.

  @param thd_arg      Thread handle
  @param query_arg    Array of char representing the query
  @param query_length Size of the 'query_arg' array
  @param using_trans  Indicates that there are transactional changes.
  @param immediate    After being written to the binary log, the event
                      must be flushed immediately. This indirectly implies
                      the stmt-cache.
  @param suppress_use Suppress the generation of 'USE' statements
  @param errcode      The error code of the query
  @param ignore       Ignore user's statement, i.e. lex information, while
                      deciding which cache must be used.
*/
Query_log_event::Query_log_event(THD* thd_arg, const char* query_arg,
				 size_t query_length, bool using_trans,
				 bool immediate, bool suppress_use,
                                 int errcode, bool ignore_cmd_internals)

: binary_log::Query_event(query_arg,
                          thd_arg->catalog().str,
                          thd_arg->db().str,
                          query_length,
                          thd_arg->thread_id(),
                          thd_arg->variables.sql_mode,
                          thd_arg->variables.auto_increment_increment,
                          thd_arg->variables.auto_increment_offset,
                          thd_arg->variables.lc_time_names->number,
                          (ulonglong)thd_arg->table_map_for_update,
                          errcode,
                          thd_arg->db().str ? strlen(thd_arg->db().str) : 0,
                          thd_arg->catalog().str ? strlen(thd_arg->catalog().str) : 0),
  Log_event(thd_arg,
            (thd_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F :
             0) |
            (suppress_use ? LOG_EVENT_SUPPRESS_USE_F : 0),
            using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                          Log_event::EVENT_STMT_CACHE,
            Log_event::EVENT_NORMAL_LOGGING,
            header(), footer()),
  data_buf(0)
{
  DBUG_EXECUTE_IF("debug_lock_before_query_log_event",
                  DBUG_SYNC_POINT("debug_lock.before_query_log_event", 10););

  /* save the original thread id; we already know the server id */
  slave_proxy_id= thd_arg->variables.pseudo_thread_id;
  if (query != 0)
    is_valid_param= true;
  /*
  exec_time calculation has changed to use the same method that is used
  to fill out "thd_arg->start_time"
  */

  struct timeval end_time;
  ulonglong micro_end_time= my_micro_time();
  my_micro_time_to_timeval(micro_end_time, &end_time);

  exec_time= end_time.tv_sec - thd_arg->start_time.tv_sec;

  /**
    @todo this means that if we have no catalog, then it is replicated
    as an existing catalog of length zero. is that safe? /sven
  */
  catalog_len = (catalog) ? strlen(catalog) : 0;
  /* status_vars_len is set just before writing the event */
  db_len = (db) ? strlen(db) : 0;
  if (thd_arg->variables.collation_database != thd_arg->db_charset)
    charset_database_number= thd_arg->variables.collation_database->number;
  
  /*
    We only replicate over the bits of flags2 that we need: the rest
    are masked out by "& OPTIONS_WRITTEN_TO_BINLOG".

    We also force AUTOCOMMIT=1.  Rationale (cf. BUG#29288): After
    fixing BUG#26395, we always write BEGIN and COMMIT around all
    transactions (even single statements in autocommit mode).  This is
    so that replication from non-transactional to transactional table
    and error recovery from XA to non-XA table should work as
    expected.  The BEGIN/COMMIT are added in log.cc. However, there is
    one exception: MyISAM bypasses log.cc and writes directly to the
    binlog.  So if autocommit is off, master has MyISAM, and slave has
    a transactional engine, then the slave will just see one long
    never-ending transaction.  The only way to bypass explicit
    BEGIN/COMMIT in the binlog is by using a non-transactional table.
    So setting AUTOCOMMIT=1 will make this work as expected.

    Note: explicitly replicate AUTOCOMMIT=1 from master. We do not
    assume AUTOCOMMIT=1 on slave; the slave still reads the state of
    the autocommit flag as written by the master to the binlog. This
    behavior may change after WL#4162 has been implemented.
  */
  flags2= (uint32) (thd_arg->variables.option_bits &
                    (OPTIONS_WRITTEN_TO_BIN_LOG & ~OPTION_NOT_AUTOCOMMIT));
  DBUG_ASSERT(thd_arg->variables.character_set_client->number < 256*256);
  DBUG_ASSERT(thd_arg->variables.collation_connection->number < 256*256);
  DBUG_ASSERT(thd_arg->variables.collation_server->number < 256*256);
  DBUG_ASSERT(thd_arg->variables.character_set_client->mbminlen == 1);
  int2store(charset, thd_arg->variables.character_set_client->number);
  int2store(charset+2, thd_arg->variables.collation_connection->number);
  int2store(charset+4, thd_arg->variables.collation_server->number);
  if (thd_arg->time_zone_used)
  {
    /*
      Note that our event becomes dependent on the Time_zone object
      representing the time zone. Fortunately such objects are never deleted
      or changed during mysqld's lifetime.
    */
    time_zone_len= thd_arg->variables.time_zone->get_name()->length();
    time_zone_str= thd_arg->variables.time_zone->get_name()->ptr();
  }
  else
    time_zone_len= 0;

  /*
    In what follows, we define in which cache, trx-cache or stmt-cache,
    this Query Log Event will be written to.

    If ignore_cmd_internals is defined, we rely on the is_trans flag to
    choose the cache and this is done in the base class Log_event. False
    means that the stmt-cache will be used and upon statement commit/rollback
    the cache will be flushed to disk. True means that the trx-cache will
    be used and upon transaction commit/rollback the cache will be flushed
    to disk.

    If set immediate cache is defined, for convenience, we automatically
    use the stmt-cache. This mean that the statement will be written
    to the stmt-cache and immediately flushed to disk without waiting
    for a commit/rollback notification.

    For example, the cluster/ndb captures a request to execute a DDL
    statement and synchronously propagate it to all available MySQL
    servers. Unfortunately, the current protocol assumes that the
    generated events are immediately written to diks and does not check
    for commit/rollback.

    Upon dropping a connection, DDLs (i.e. DROP TEMPORARY TABLE) are
    generated and in this case the statements have the immediate flag
    set because there is no commit/rollback.

    If the immediate flag is not set, the decision on the cache is based
    on the current statement and the flag is_trans, which indicates if
    a transactional engine was updated. 

    Statements are classifed as row producers (i.e. can_generate_row_events())
    or non-row producers. Non-row producers, DDL in general, are treated
    as the immediate flag was set and for convenience are written to the
    stmt-cache and immediately flushed to disk. 

    Row producers are handled in general according to the is_trans flag.
    False means that the stmt-cache will be used and upon statement
    commit/rollback the cache will be flushed to disk. True means that the
    trx-cache will be used and upon transaction commit/rollback the cache
    will be flushed to disk.

    Unfortunately, there are exceptions to this non-row and row producer
    rules:

      . The SAVEPOINT, ROLLBACK TO SAVEPOINT, RELEASE SAVEPOINT does not
        have the flag is_trans set because there is no updated engine but
        must be written to the trx-cache.

      . SET If auto-commit is on, it must not go through a cache.

      . CREATE TABLE is classfied as non-row producer but CREATE TEMPORARY
        must be handled as row producer.

      . DROP TABLE is classfied as non-row producer but DROP TEMPORARY
        must be handled as row producer.

    Finally, some statements that does not have the flag is_trans set may
    be written to the trx-cache based on the following criteria:

      . updated both a transactional and a non-transactional engine (i.e.
        stmt_has_updated_trans_table()).

      . accessed both a transactional and a non-transactional engine and
        is classified as unsafe (i.e. is_mixed_stmt_unsafe()).

      . is executed within a transaction and previously a transactional
        engine was updated and the flag binlog_direct_non_trans_update
        is set.
  */
  if (ignore_cmd_internals)
    return;

  /*
    TRUE defines that the trx-cache must be used.
  */
  bool cmd_can_generate_row_events= FALSE;
  /*
    TRUE defines that the trx-cache must be used.
  */
  bool cmd_must_go_to_trx_cache= FALSE;
   
  LEX *lex= thd->lex;
  if (!immediate)
  {
    switch (lex->sql_command)
    {
      case SQLCOM_DROP_TABLE:
        cmd_can_generate_row_events= lex->drop_temporary &&
                                     thd->in_multi_stmt_transaction_mode();
      break;
      case SQLCOM_CREATE_TABLE:
        cmd_must_go_to_trx_cache= lex->select_lex->item_list.elements &&
                                  thd->is_current_stmt_binlog_format_row();
        cmd_can_generate_row_events= 
          ((lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
            thd->in_multi_stmt_transaction_mode()) || cmd_must_go_to_trx_cache;
        break;
      case SQLCOM_SET_OPTION:
        if (lex->autocommit)
          cmd_can_generate_row_events= cmd_must_go_to_trx_cache= FALSE;
        else
          cmd_can_generate_row_events= TRUE;
        break;
      case SQLCOM_RELEASE_SAVEPOINT:
      case SQLCOM_ROLLBACK_TO_SAVEPOINT:
      case SQLCOM_SAVEPOINT:
      case SQLCOM_XA_PREPARE:
        cmd_can_generate_row_events= cmd_must_go_to_trx_cache= TRUE;
        break;
      default:
        cmd_can_generate_row_events=
          sqlcom_can_generate_row_events(thd->lex->sql_command);
        break;
    }
  }
  
  if (cmd_can_generate_row_events)
  {
    cmd_must_go_to_trx_cache= cmd_must_go_to_trx_cache || using_trans;
    if (cmd_must_go_to_trx_cache ||
        stmt_has_updated_trans_table(thd->get_transaction()->ha_trx_info(Transaction_ctx::STMT)) ||
        thd->lex->is_mixed_stmt_unsafe(thd->in_multi_stmt_transaction_mode(),
                                       thd->variables.binlog_direct_non_trans_update,
                                       trans_has_updated_trans_table(thd),
                                       thd->tx_isolation) ||
        (!thd->variables.binlog_direct_non_trans_update && trans_has_updated_trans_table(thd)))
    {
      event_logging_type= Log_event::EVENT_NORMAL_LOGGING; 
      event_cache_type= Log_event::EVENT_TRANSACTIONAL_CACHE;
    }
    else
    {
      event_logging_type= Log_event::EVENT_NORMAL_LOGGING; 
      event_cache_type= Log_event::EVENT_STMT_CACHE;
    }
  }
  else
  {
    event_logging_type= Log_event::EVENT_IMMEDIATE_LOGGING;
    event_cache_type= Log_event::EVENT_STMT_CACHE;
  }

  DBUG_ASSERT(event_cache_type != Log_event::EVENT_INVALID_CACHE);
  DBUG_ASSERT(event_logging_type != Log_event::EVENT_INVALID_LOGGING);
  DBUG_PRINT("info",("Query_log_event has flags2: %lu  sql_mode: %llu",
                     (ulong) flags2, (ulonglong) sql_mode));
}
#endif /* MYSQL_CLIENT */


/**
  This is used by the SQL slave thread to prepare the event before execution.
*/
Query_log_event::Query_log_event(const char* buf, uint event_len,
                                 const Format_description_event
                                 *description_event,
                                 Log_event_type event_type)
  :binary_log::Query_event(buf, event_len, description_event, event_type),
   Log_event(header(), footer())
{
  DBUG_ENTER("Query_log_event::Query_log_event(char*,...)");
  slave_proxy_id= thread_id;
  exec_time= query_exec_time;

  ulong buf_len= catalog_len + 1 +
                  time_zone_len + 1 +
                  user_len + 1 +
                  host_len + 1 +
                  data_len + 1;
#if !defined(MYSQL_CLIENT)
  buf_len+= sizeof(size_t)/*for db_len */ + db_len + 1 + QUERY_CACHE_FLAGS_SIZE;
#endif

  if (!(data_buf = (Log_event_header::Byte*) my_malloc(key_memory_log_event,
                                                       buf_len, MYF(MY_WME))))
    DBUG_VOID_RETURN;
  /*
    The data buffer is used by the slave SQL thread while applying
    the event. The catalog, time_zone)str, user, host, db, query
    are pointers to this data_buf. The function call below, points these
    const pointers to the data buffer.
  */
  if (!(fill_data_buf(data_buf, buf_len)))
    DBUG_VOID_RETURN;

  if (query != 0 && q_len > 0)
    is_valid_param= true;

  /**
    The buffer contains the following:
    +--------+-----------+------+------+---------+----+-------+
    | catlog | time_zone | user | host | db name | \0 | Query |
    +--------+-----------+------+------+---------+----+-------+

    To support the query cache we append the following buffer to the above
    +-------+----------------------------------------+-------+
    |db len | uninitiatlized space of size of db len | FLAGS |
    +-------+----------------------------------------+-------+

    The area of buffer starting from Query field all the way to the end belongs
    to the Query buffer and its structure is described in alloc_query() in
    sql_parse.cc

    We append the db length at the end of the buffer. This will be used by
    Query_cache::send_result_to_client() in case the query cache is On.
   */
#if !defined(MYSQL_CLIENT)
  size_t db_length= db_len;
  memcpy(data_buf + query_data_written, &db_length, sizeof(size_t));
#endif
  DBUG_VOID_RETURN;
}


#ifdef MYSQL_CLIENT
/**
  Query_log_event::print().

  @todo
    print the catalog ??
*/
void Query_log_event::print_query_header(IO_CACHE* file,
					 PRINT_EVENT_INFO* print_event_info)
{
  // TODO: print the catalog ??
  char buff[48], *end;  // Enough for "SET TIMESTAMP=1305535348.123456"
  char quoted_id[1+ 2*FN_REFLEN+ 2];
  size_t quoted_len= 0;
  bool different_db= true;
  uint32 tmp;

  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info, FALSE);
    my_b_printf(file, "\t%s\tthread_id=%lu\texec_time=%lu\terror_code=%d\n",
                get_type_str(), (ulong) thread_id, (ulong) exec_time,
                error_code);
  }

  if ((common_header->flags & LOG_EVENT_SUPPRESS_USE_F))
  {
    if (!is_trans_keyword())
      print_event_info->db[0]= '\0';
  }
  else if (db)
  {
#ifdef MYSQL_SERVER
    quoted_len= my_strmov_quoted_identifier(this->thd, (char*)quoted_id, db, 0);
#else
    quoted_len= my_strmov_quoted_identifier((char*)quoted_id, db);
#endif
    quoted_id[quoted_len]= '\0';
    different_db= memcmp(print_event_info->db, db, db_len + 1);
    if (different_db)
      memcpy(print_event_info->db, db, db_len + 1);
    if (db[0] && different_db) 
      my_b_printf(file, "use %s%s\n", quoted_id, print_event_info->delimiter);
  }

  end= int10_to_str((long)common_header->when.tv_sec,
                    my_stpcpy(buff,"SET TIMESTAMP="),10);
  if (common_header->when.tv_usec)
    end+= sprintf(end, ".%06d", (int) common_header->when.tv_usec);
  end= my_stpcpy(end, print_event_info->delimiter);
  *end++='\n';
  DBUG_ASSERT(end < buff + sizeof(buff));
  my_b_write(file, (uchar*) buff, (uint) (end-buff));
  if ((!print_event_info->thread_id_printed ||
       ((common_header->flags & LOG_EVENT_THREAD_SPECIFIC_F) &&
        thread_id != print_event_info->thread_id)))
  {
    // If --short-form, print deterministic value instead of pseudo_thread_id.
    my_b_printf(file,"SET @@session.pseudo_thread_id=%lu%s\n",
                short_form ? 999999999 : (ulong)thread_id,
                print_event_info->delimiter);
    print_event_info->thread_id= thread_id;
    print_event_info->thread_id_printed= 1;
  }

  /*
    If flags2_inited==0, this is an event from 3.23 or 4.0; nothing to
    print (remember we don't produce mixed relay logs so there cannot be
    5.0 events before that one so there is nothing to reset).
  */
  if (likely(flags2_inited)) /* likely as this will mainly read 5.0 logs */
  {
    /* tmp is a bitmask of bits which have changed. */
    if (likely(print_event_info->flags2_inited)) 
      /* All bits which have changed */
      tmp= (print_event_info->flags2) ^ flags2;
    else /* that's the first Query event we read */
    {
      print_event_info->flags2_inited= 1;
      tmp= ~((uint32)0); /* all bits have changed */
    }

    if (unlikely(tmp)) /* some bits have changed */
    {
      bool need_comma= 0;
      my_b_printf(file, "SET ");
      print_set_option(file, tmp, OPTION_NO_FOREIGN_KEY_CHECKS, ~flags2,
                       "@@session.foreign_key_checks", &need_comma);
      print_set_option(file, tmp, OPTION_AUTO_IS_NULL, flags2,
                       "@@session.sql_auto_is_null", &need_comma);
      print_set_option(file, tmp, OPTION_RELAXED_UNIQUE_CHECKS, ~flags2,
                       "@@session.unique_checks", &need_comma);
      print_set_option(file, tmp, OPTION_NOT_AUTOCOMMIT, ~flags2,
                       "@@session.autocommit", &need_comma);
      my_b_printf(file,"%s\n", print_event_info->delimiter);
      print_event_info->flags2= flags2;
    }
  }

  /*
    Now the session variables;
    it's more efficient to pass SQL_MODE as a number instead of a
    comma-separated list.
    FOREIGN_KEY_CHECKS, SQL_AUTO_IS_NULL, UNIQUE_CHECKS are session-only
    variables (they have no global version; they're not listed in
    sql_class.h), The tests below work for pure binlogs or pure relay
    logs. Won't work for mixed relay logs but we don't create mixed
    relay logs (that is, there is no relay log with a format change
    except within the 3 first events, which mysqlbinlog handles
    gracefully). So this code should always be good.
  */

  if (likely(sql_mode_inited) &&
      (unlikely(print_event_info->sql_mode != sql_mode ||
                !print_event_info->sql_mode_inited)))
  {
    my_b_printf(file,"SET @@session.sql_mode=%lu%s\n",
                (ulong)sql_mode, print_event_info->delimiter);
    print_event_info->sql_mode= sql_mode;
    print_event_info->sql_mode_inited= 1;
  }
  if (print_event_info->auto_increment_increment != auto_increment_increment ||
      print_event_info->auto_increment_offset != auto_increment_offset)
  {
    my_b_printf(file,"SET @@session.auto_increment_increment=%u, @@session.auto_increment_offset=%u%s\n",
                auto_increment_increment,auto_increment_offset,
                print_event_info->delimiter);
    print_event_info->auto_increment_increment= auto_increment_increment;
    print_event_info->auto_increment_offset=    auto_increment_offset;
  }

  /* TODO: print the catalog when we feature SET CATALOG */

  if (likely(charset_inited) &&
      (unlikely(!print_event_info->charset_inited ||
                memcmp(print_event_info->charset, charset, 6))))
  {
    char *charset_p= charset; // Avoid type-punning warning.
    CHARSET_INFO *cs_info= get_charset(uint2korr(charset_p), MYF(MY_WME));
    if (cs_info)
    {
      /* for mysql client */
      my_b_printf(file, "/*!\\C %s */%s\n",
                  cs_info->csname, print_event_info->delimiter);
    }
    my_b_printf(file,"SET "
                "@@session.character_set_client=%d,"
                "@@session.collation_connection=%d,"
                "@@session.collation_server=%d"
                "%s\n",
                uint2korr(charset_p),
                uint2korr(charset+2),
                uint2korr(charset+4),
                print_event_info->delimiter);
    memcpy(print_event_info->charset, charset, 6);
    print_event_info->charset_inited= 1;
  }
  if (time_zone_len)
  {
    if (memcmp(print_event_info->time_zone_str,
               time_zone_str, time_zone_len+1))
    {
      my_b_printf(file,"SET @@session.time_zone='%s'%s\n",
                  time_zone_str, print_event_info->delimiter);
      memcpy(print_event_info->time_zone_str, time_zone_str, time_zone_len+1);
    }
  }
  if (lc_time_names_number != print_event_info->lc_time_names_number)
  {
    my_b_printf(file, "SET @@session.lc_time_names=%d%s\n",
                lc_time_names_number, print_event_info->delimiter);
    print_event_info->lc_time_names_number= lc_time_names_number;
  }
  if (charset_database_number != print_event_info->charset_database_number)
  {
    if (charset_database_number)
      my_b_printf(file, "SET @@session.collation_database=%d%s\n",
                  charset_database_number, print_event_info->delimiter);
    else
      my_b_printf(file, "SET @@session.collation_database=DEFAULT%s\n",
                  print_event_info->delimiter);
    print_event_info->charset_database_number= charset_database_number;
  }
  if (explicit_defaults_ts != TERNARY_UNSET)
    my_b_printf(file, "SET @@session.explicit_defaults_for_timestamp=%d%s\n",
                explicit_defaults_ts == TERNARY_OFF? 0 : 1,
                print_event_info->delimiter);
}


void Query_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;

  /**
    reduce the size of io cache so that the write function is called
    for every call to my_b_write().
   */
  DBUG_EXECUTE_IF ("simulate_file_write_error",
                   {head->write_pos= head->write_end- 500;});
  print_query_header(head, print_event_info);
  my_b_write(head, (uchar*) query, q_len);
  my_b_printf(head, "\n%s\n", print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
   Associating slave Worker thread to a subset of temporary tables.

   @param thd_arg THD instance pointer
   @param rli     Relay_log_info of the worker
*/
void Query_log_event::attach_temp_tables_worker(THD *thd_arg,
                                                const Relay_log_info* rli)
{
  rli->current_mts_submode->attach_temp_tables(thd_arg, rli, this);
}

/**
   Dissociating slave Worker thread from its thd->temporary_tables
   to possibly update the involved entries of db-to-worker hash
   with new values of temporary_tables.

   @param thd_arg THD instance pointer
   @param rli     relay log info of the worker thread
*/
void Query_log_event::detach_temp_tables_worker(THD *thd_arg,
                                                const Relay_log_info *rli)
{
  rli->current_mts_submode->detach_temp_tables(thd_arg, rli, this);
}

/*
  Query_log_event::do_apply_event()
*/
int Query_log_event::do_apply_event(Relay_log_info const *rli)
{
  return do_apply_event(rli, query, q_len);
}

/*
  is_silent_error

  Return true if the thread has an error which should be
  handled silently
*/
  
static bool is_silent_error(THD* thd)
{
  DBUG_ENTER("is_silent_error");
  Diagnostics_area::Sql_condition_iterator it=
    thd->get_stmt_da()->sql_conditions();
  const Sql_condition *err;
  while ((err= it++))
  {
    DBUG_PRINT("info", ("has condition %d %s", err->mysql_errno(),
                        err->message_text()));
    switch (err->mysql_errno())
    {
    case ER_SLAVE_SILENT_RETRY_TRANSACTION:
    {
      DBUG_RETURN(true);
    }
    default:
      break;
    }
  }
  DBUG_RETURN(false);
}

/**
  @todo
  Compare the values of "affected rows" around here. Something
  like:
  @code
     if ((uint32) affected_in_event != (uint32) affected_on_slave)
     {
     sql_print_error("Slave: did not get the expected number of affected \
     rows running query from master - expected %d, got %d (this numbers \
     should have matched modulo 4294967296).", 0, ...);
     thd->query_error = 1;
     }
  @endcode
  We may also want an option to tell the slave to ignore "affected"
  mismatch. This mismatch could be implemented with a new ER_ code, and
  to ignore it you would use --slave-skip-errors...
*/
int Query_log_event::do_apply_event(Relay_log_info const *rli,
                                      const char *query_arg, size_t q_len_arg)
{
  DBUG_ENTER("Query_log_event::do_apply_event");
  int expected_error,actual_error= 0;
  HA_CREATE_INFO db_options;

  DBUG_PRINT("info", ("query=%s, q_len_arg=%lu",
                      query, static_cast<unsigned long>(q_len_arg)));

  /*
    Colleagues: please never free(thd->catalog) in MySQL. This would
    lead to bugs as here thd->catalog is a part of an alloced block,
    not an entire alloced block (see
    Query_log_event::do_apply_event()). Same for thd->db().str.  Thank
    you.
  */

  if (catalog_len)
  {
    LEX_CSTRING catalog_lex_cstr= { catalog, catalog_len};
    thd->set_catalog(catalog_lex_cstr);
  }
  else
    thd->set_catalog(EMPTY_CSTR);

  size_t valid_len;
  bool len_error;
  bool is_invalid_db_name= validate_string(system_charset_info, db, db_len,
                                           &valid_len, &len_error);

  DBUG_PRINT("debug",("is_invalid_db_name= %s, valid_len=%zu, len_error=%s",
                      is_invalid_db_name ? "true" : "false",
                      valid_len,
                      len_error ? "true" : "false"));

  if (is_invalid_db_name || len_error)
  {
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                "Invalid database name in Query event.");
    thd->is_slave_error= true;
    goto end;
  }

  set_thd_db(thd, db, db_len);

  /*
    Setting the character set and collation of the current database thd->db.
   */
  load_db_opt_by_name(thd, thd->db().str, &db_options);
  if (db_options.default_table_charset)
    thd->db_charset= db_options.default_table_charset;
  thd->variables.auto_increment_increment= auto_increment_increment;
  thd->variables.auto_increment_offset=    auto_increment_offset;
  if (explicit_defaults_ts != TERNARY_UNSET)
    thd->variables.explicit_defaults_for_timestamp=
      explicit_defaults_ts == TERNARY_OFF? 0 : 1;

  /*
    todo: such cleanup should not be specific to Query event and therefore
          is preferable at a common with other event pre-execution point
  */
  clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
  if (strcmp("COMMIT", query) == 0 && rli->tables_to_lock != NULL)
  {
    /*
      Cleaning-up the last statement context:
      the terminal event of the current statement flagged with
      STMT_END_F got filtered out in ndb circular replication.
    */
    int error;
    char llbuff[22];
    if ((error= rows_event_stmt_cleanup(const_cast<Relay_log_info*>(rli), thd)))
    {
      const_cast<Relay_log_info*>(rli)->report(ERROR_LEVEL, error,
                  "Error in cleaning up after an event preceding the commit; "
                  "the group log file/position: %s %s",
                  const_cast<Relay_log_info*>(rli)->get_group_master_log_name(),
                  llstr(const_cast<Relay_log_info*>(rli)->get_group_master_log_pos(),
                        llbuff));
    }
    /*
      Executing a part of rli->stmt_done() logics that does not deal
      with group position change. The part is redundant now but is 
      future-change-proof addon, e.g if COMMIT handling will start checking
      invariants like IN_STMT flag must be off at committing the transaction.
    */
    const_cast<Relay_log_info*>(rli)->inc_event_relay_log_pos();
    const_cast<Relay_log_info*>(rli)->clear_flag(Relay_log_info::IN_STMT);
  }
  else
  {
    const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
  }

  {
    thd->set_time(&(common_header->when));
    thd->set_query(query_arg, q_len_arg);
    thd->set_query_id(next_query_id());
    thd->variables.pseudo_thread_id= thread_id;		// for temp tables
    attach_temp_tables_worker(thd, rli);
    DBUG_PRINT("query",("%s", thd->query().str));

    if (ignored_error_code((expected_error= error_code)) ||
	!unexpected_error_code(expected_error))
    {
      if (flags2_inited)
        /*
          all bits of thd->variables.option_bits which are 1 in OPTIONS_WRITTEN_TO_BIN_LOG
          must take their value from flags2.
        */
        thd->variables.option_bits= flags2|(thd->variables.option_bits & ~OPTIONS_WRITTEN_TO_BIN_LOG);
      /*
        else, we are in a 3.23/4.0 binlog; we previously received a
        Rotate_log_event which reset thd->variables.option_bits and sql_mode etc, so
        nothing to do.
      */
      /*
        We do not replicate MODE_NO_DIR_IN_CREATE. That is, if the master is a
        slave which runs with SQL_MODE=MODE_NO_DIR_IN_CREATE, this should not
        force us to ignore the dir too. Imagine you are a ring of machines, and
        one has a disk problem so that you temporarily need
        MODE_NO_DIR_IN_CREATE on this machine; you don't want it to propagate
        elsewhere (you don't want all slaves to start ignoring the dirs).
      */
      if (sql_mode_inited)
        thd->variables.sql_mode=
          (sql_mode_t) ((thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE) |
                       (sql_mode & ~(ulonglong) MODE_NO_DIR_IN_CREATE));
      if (charset_inited)
      {
        if (rli->cached_charset_compare(charset))
        {
          char *charset_p= charset; // Avoid type-punning warning.
          /* Verify that we support the charsets found in the event. */
          if (!(thd->variables.character_set_client=
                get_charset(uint2korr(charset_p), MYF(MY_WME))) ||
              !(thd->variables.collation_connection=
                get_charset(uint2korr(charset+2), MYF(MY_WME))) ||
              !(thd->variables.collation_server=
                get_charset(uint2korr(charset+4), MYF(MY_WME))))
          {
            /*
              We updated the thd->variables with nonsensical values (0). Let's
              set them to something safe (i.e. which avoids crash), and we'll
              stop with EE_UNKNOWN_CHARSET in compare_errors (unless set to
              ignore this error).
            */
            set_slave_thread_default_charset(thd, rli);
            goto compare_errors;
          }
          thd->update_charset(); // for the charset change to take effect
          /*
            We cannot ask for parsing a statement using a character set
            without state_maps (parser internal data).
          */
          if (!thd->variables.character_set_client->state_maps)
          {
            rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                        ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                        "character_set cannot be parsed");
            thd->is_slave_error= true;
            goto end;
          }
          /*
            Reset thd->query_string.cs to the newly set value.
            Note, there is a small flaw here. For a very short time frame
            if the new charset is different from the old charset and
            if another thread executes "SHOW PROCESSLIST" after
            the above thd->set_query() and before this thd->set_query(),
            and if the current query has some non-ASCII characters,
            the another thread may see some '?' marks in the PROCESSLIST
            result. This should be acceptable now. This is a reminder
            to fix this if any refactoring happens here sometime.
          */
          thd->set_query(query_arg, q_len_arg);
        }
      }
      if (time_zone_len)
      {
        String tmp(time_zone_str, time_zone_len, &my_charset_bin);
        if (!(thd->variables.time_zone= my_tz_find(thd, &tmp)))
        {
          my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), tmp.c_ptr());
          thd->variables.time_zone= global_system_variables.time_zone;
          goto compare_errors;
        }
      }
      if (lc_time_names_number)
      {
        if (!(thd->variables.lc_time_names=
              my_locale_by_number(lc_time_names_number)))
        {
          my_printf_error(ER_UNKNOWN_ERROR,
                      "Unknown locale: '%d'", MYF(0), lc_time_names_number);
          thd->variables.lc_time_names= &my_locale_en_US;
          goto compare_errors;
        }
      }
      else
        thd->variables.lc_time_names= &my_locale_en_US;
      if (charset_database_number)
      {
        CHARSET_INFO *cs;
        if (!(cs= get_charset(charset_database_number, MYF(0))))
        {
          char buf[20];
          int10_to_str((int) charset_database_number, buf, -10);
          my_error(ER_UNKNOWN_COLLATION, MYF(0), buf);
          goto compare_errors;
        }
        thd->variables.collation_database= cs;
      }
      else
        thd->variables.collation_database= thd->db_charset;

      thd->table_map_for_update= (table_map)table_map_for_update;

      LEX_STRING user_lex= LEX_STRING();
      LEX_STRING host_lex= LEX_STRING();
      if (user)
      {
        user_lex.str= const_cast<char*>(user);
        user_lex.length= strlen(user);
      }
      if (host)
      {
        host_lex.str= const_cast<char*>(host);
        host_lex.length= strlen(host);
      }
      thd->set_invoker(&user_lex, &host_lex);
      /*
        Flag if we need to rollback the statement transaction on
        slave if it by chance succeeds.
        If we expected a non-zero error code and get nothing and,
        it is a concurrency issue or ignorable issue, effects
        of the statement should be rolled back.
      */
      if (expected_error &&
          (ignored_error_code(expected_error) ||
           concurrency_error_code(expected_error)))
      {
        thd->variables.option_bits|= OPTION_MASTER_SQL_ERROR;
      }
      /* Execute the query (note that we bypass dispatch_command()) */
      Parser_state parser_state;
      if (!parser_state.init(thd, thd->query().str, thd->query().length))
      {
        DBUG_ASSERT(thd->m_digest == NULL);
        thd->m_digest= & thd->m_digest_state;
        DBUG_ASSERT(thd->m_statement_psi == NULL);
        thd->m_statement_psi= MYSQL_START_STATEMENT(&thd->m_statement_state,
                                                    stmt_info_rpl.m_key,
                                                    thd->db().str,
                                                    thd->db().length,
                                                    thd->charset(), NULL);
        THD_STAGE_INFO(thd, stage_starting);

        if (thd->m_digest != NULL)
          thd->m_digest->reset(thd->m_token_array, max_digest_length);

        mysql_parse(thd, &parser_state);

        /*
          Transaction isolation level of pure row based replicated transactions
          can be optimized to ISO_READ_COMMITTED by the applier when applying
          the Gtid_log_event.

          If we are applying a statement other than transaction control ones
          after having optimized the transactions isolation level, we must warn
          about the non-standard situation we have found.
        */
        if (is_sbr_logging_format() &&
            thd->variables.tx_isolation > ISO_READ_COMMITTED &&
            thd->tx_isolation == ISO_READ_COMMITTED)
        {
          String message;
          message.append("The isolation level for the current transaction "
                         "was changed to READ_COMMITTED based on the "
                         "assumption that it had only row events and was "
                         "not mixed with statements. "
                         "However, an unexpected statement was found in "
                         "the middle of the transaction."
                         "Query: '");
          message.append(thd->query().str);
          message.append("'");
          rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                      ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                      message.c_ptr());
          thd->is_slave_error= true;
          goto end;
        }

        /* Finalize server status flags after executing a statement. */
        thd->update_server_status();
        log_slow_statement(thd);
      }

      thd->variables.option_bits&= ~OPTION_MASTER_SQL_ERROR;

      /*
        Resetting the enable_slow_log thd variable.

        We need to reset it back to the opt_log_slow_slave_statements
        value after the statement execution (and slow logging
        is done). It might have changed if the statement was an
        admin statement (in which case, down in mysql_parse execution
        thd->enable_slow_log is set to the value of
        opt_log_slow_admin_statements).
      */
      thd->enable_slow_log= opt_log_slow_slave_statements;
    }
    else
    {
      /*
        The query got a really bad error on the master (thread killed etc),
        which could be inconsistent. Parse it to test the table names: if the
        replicate-*-do|ignore-table rules say "this query must be ignored" then
        we exit gracefully; otherwise we warn about the bad error and tell DBA
        to check/fix it.
      */
      if (mysql_test_parse_for_slave(thd))
        clear_all_errors(thd, const_cast<Relay_log_info*>(rli)); /* Can ignore query */
      else
      {
        rli->report(ERROR_LEVEL, ER_ERROR_ON_MASTER, ER(ER_ERROR_ON_MASTER),
                    expected_error, thd->query().str);
        thd->is_slave_error= 1;
      }
      goto end;
    }

    /* If the query was not ignored, it is printed to the general log */
    if (!thd->is_error() ||
        thd->get_stmt_da()->mysql_errno() != ER_SLAVE_IGNORED_TABLE)
    {
      /* log the rewritten query if the query was rewritten 
         and the option to log raw was not set.
        
         There is an assumption here. We assume that query log
         events can never have multi-statement queries, thus the
         parsed statement is the same as the raw one.
       */
      if (opt_general_log_raw || thd->rewritten_query.length() == 0)
        query_logger.general_log_write(thd, COM_QUERY, thd->query().str,
                                       thd->query().length);
      else
        query_logger.general_log_write(thd, COM_QUERY,
                                       thd->rewritten_query.c_ptr_safe(),
                                       thd->rewritten_query.length());
    }

compare_errors:
    /* Parser errors shall be ignored when (GTID) skipping statements */
    if (thd->is_error() &&
        thd->get_stmt_da()->mysql_errno() == ER_PARSE_ERROR &&
        gtid_pre_statement_checks(thd) == GTID_STATEMENT_SKIP)
    {
      thd->get_stmt_da()->reset_diagnostics_area();
    }
    /*
      In the slave thread, we may sometimes execute some DROP / * 40005
      TEMPORARY * / TABLE that come from parts of binlogs (likely if we
      use RESET SLAVE or CHANGE MASTER TO), while the temporary table
      has already been dropped. To ignore such irrelevant "table does
      not exist errors", we silently clear the error if TEMPORARY was used.
    */
    if (thd->lex->sql_command == SQLCOM_DROP_TABLE &&
        thd->lex->drop_temporary &&
        thd->is_error() &&
        thd->get_stmt_da()->mysql_errno() == ER_BAD_TABLE_ERROR &&
        !expected_error)
      thd->get_stmt_da()->reset_diagnostics_area();
    /*
      If we expected a non-zero error code, and we don't get the same error
      code, and it should be ignored or is related to a concurrency issue.
    */
    actual_error= thd->is_error() ? thd->get_stmt_da()->mysql_errno() : 0;
    DBUG_PRINT("info",("expected_error: %d  sql_errno: %d",
                       expected_error, actual_error));

    /*
      If a statement with expected error is received on slave and if the
      statement is not filtered on the slave, only then compare the expected
      error with the actual error that happened on slave.
    */
    if ((expected_error && rpl_filter->db_ok(thd->db().str) &&
         expected_error != actual_error &&
         !concurrency_error_code(expected_error)) &&
        !ignored_error_code(actual_error) &&
        !ignored_error_code(expected_error))
    {
      if (!ignored_error_code(ER_INCONSISTENT_ERROR))
      {
        rli->report(ERROR_LEVEL, ER_INCONSISTENT_ERROR,
                    ER(ER_INCONSISTENT_ERROR),
                    ER_THD(thd, expected_error), expected_error,
                    (actual_error ?
                     thd->get_stmt_da()->message_text() :
                     "no error"),
                    actual_error, print_slave_db_safe(db), query_arg);
        thd->is_slave_error= 1;
      }
      else
      {
        rli->report(INFORMATION_LEVEL, actual_error,
                    "The actual error and expected error on slave are"
                    " different that will result in ER_INCONSISTENT_ERROR but"
                    " that is passed as an argument to slave_skip_errors so no"
                    " error is thrown. "
                    "The expected error was %s with, Error_code: %d. "
                    "The actual error is %s with ",
                    ER(expected_error), expected_error,
                    thd->get_stmt_da()->message_text());
        clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      }
    }
    /*
      If we get the same error code as expected and it is not a concurrency
      issue, or should be ignored.
    */
    else if ((expected_error == actual_error &&
              !concurrency_error_code(expected_error)) ||
             ignored_error_code(actual_error))
    {
      DBUG_PRINT("info",("error ignored"));
      if (actual_error && ignored_error_code(actual_error))
      {
        if (actual_error == ER_SLAVE_IGNORED_TABLE)
        {
          if (!slave_ignored_err_throttle.log())
            rli->report(INFORMATION_LEVEL, actual_error,
                        "Could not execute %s event. Detailed error: %s;"
                        " Error log throttle is enabled. This error will not be"
                        " displayed for next %lu secs. It will be suppressed",
                        get_type_str(), thd->get_stmt_da()->message_text(),
                        (window_size / 1000000));
        }
        else
          rli->report(INFORMATION_LEVEL, actual_error,
                      "Could not execute %s event. Detailed error: %s;",
                      get_type_str(), thd->get_stmt_da()->message_text());
      }
      clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      thd->killed= THD::NOT_KILLED;
    }
    /*
      Other cases: mostly we expected no error and get one.
    */
    else if (thd->is_slave_error || thd->is_fatal_error)
    {
      if (!is_silent_error(thd))
      {
        rli->report(ERROR_LEVEL, actual_error,
                    "Error '%s' on query. Default database: '%s'. Query: '%s'",
                    (actual_error ?
                     thd->get_stmt_da()->message_text() :
                     "unexpected success or fatal error"),
                    print_slave_db_safe(thd->db().str), query_arg);
      }
      thd->is_slave_error= 1;
    }

    /*
      TODO: compare the values of "affected rows" around here. Something
      like:
      if ((uint32) affected_in_event != (uint32) affected_on_slave)
      {
      sql_print_error("Slave: did not get the expected number of affected \
      rows running query from master - expected %d, got %d (this numbers \
      should have matched modulo 4294967296).", 0, ...);
      thd->is_slave_error = 1;
      }
      We may also want an option to tell the slave to ignore "affected"
      mismatch. This mismatch could be implemented with a new ER_ code, and
      to ignore it you would use --slave-skip-errors...

      To do the comparison we need to know the value of "affected" which the
      above mysql_parse() computed. And we need to know the value of
      "affected" in the master's binlog. Both will be implemented later. The
      important thing is that we now have the format ready to log the values
      of "affected" in the binlog. So we can release 5.0.0 before effectively
      logging "affected" and effectively comparing it.
    */
  } /* End of if (db_ok(... */

  {
    /**
      The following failure injecion works in cooperation with tests
      setting @@global.debug= 'd,stop_slave_middle_group'.
      The sql thread receives the killed status and will proceed
      to shutdown trying to finish incomplete events group.
    */

    // TODO: address the middle-group killing in MTS case

    DBUG_EXECUTE_IF("stop_slave_middle_group",
                    if (strcmp("COMMIT", query) != 0 &&
                        strcmp("BEGIN", query) != 0)
                    {
                      if (thd->get_transaction()->cannot_safely_rollback(
                          Transaction_ctx::SESSION))
                        const_cast<Relay_log_info*>(rli)->abort_slave= 1;
                    };);
  }

end:

  if (thd->temporary_tables)
    detach_temp_tables_worker(thd, rli);
  /*
    Probably we have set thd->query, thd->db, thd->catalog to point to places
    in the data_buf of this event. Now the event is going to be deleted
    probably, so data_buf will be freed, so the thd->... listed above will be
    pointers to freed memory.
    So we must set them to 0, so that those bad pointers values are not later
    used. Note that "cleanup" queries like automatic DROP TEMPORARY TABLE
    don't suffer from these assignments to 0 as DROP TEMPORARY
    TABLE uses the db.table syntax.
  */
  thd->set_catalog(NULL_CSTR);
  thd->set_db(NULL_CSTR);                 /* will free the current database */
  thd->reset_query();
  thd->lex->sql_command= SQLCOM_END;
  DBUG_PRINT("info", ("end: query= 0"));

  /* Mark the statement completed. */
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  thd->m_statement_psi= NULL;
  thd->m_digest= NULL;

  /*
    As a disk space optimization, future masters will not log an event for
    LAST_INSERT_ID() if that function returned 0 (and thus they will be able
    to replace the THD::stmt_depends_on_first_successful_insert_id_in_prev_stmt
    variable by (THD->first_successful_insert_id_in_prev_stmt > 0) ; with the
    resetting below we are ready to support that.
  */
  thd->first_successful_insert_id_in_prev_stmt_for_binlog= 0;
  thd->first_successful_insert_id_in_prev_stmt= 0;
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
  DBUG_RETURN(thd->is_slave_error);
}

int Query_log_event::do_update_pos(Relay_log_info *rli)
{
  int ret= Log_event::do_update_pos(rli);

  DBUG_EXECUTE_IF("crash_after_commit_and_update_pos",
       if (!strcmp("COMMIT", query))
       {
         sql_print_information("Crashing crash_after_commit_and_update_pos.");
         rli->flush_info(true);
         ha_flush_logs(0); 
         DBUG_SUICIDE();
       }
  );
  
  return ret;
}


Log_event::enum_skip_reason
Query_log_event::do_shall_skip(Relay_log_info *rli)
{
  DBUG_ENTER("Query_log_event::do_shall_skip");
  DBUG_PRINT("debug", ("query: %s; q_len: %d", query, static_cast<int>(q_len)));
  DBUG_ASSERT(query && q_len > 0);

  if (rli->slave_skip_counter > 0)
  {
    if (strcmp("BEGIN", query) == 0)
    {
      thd->variables.option_bits|= OPTION_BEGIN;
      DBUG_RETURN(Log_event::continue_group(rli));
    }

    if (strcmp("COMMIT", query) == 0 || strcmp("ROLLBACK", query) == 0)
    {
      thd->variables.option_bits&= ~OPTION_BEGIN;
      DBUG_RETURN(Log_event::EVENT_SKIP_COUNT);
    }
  }
  Log_event::enum_skip_reason ret= Log_event::do_shall_skip(rli);
  DBUG_RETURN(ret);
}

#endif

/**
   Return the query string pointer (and its size) from a Query log event
   using only the event buffer (we don't instantiate a Query_log_event
   object for this).

   @param buf               Pointer to the event buffer.
   @param length            The size of the event buffer.
   @param description_event The description event of the master which logged
                            the event.
   @param[out] query        The pointer to receive the query pointer.

   @return                  The size of the query.
*/
size_t Query_log_event::get_query(const char *buf, size_t length,
                                  const Format_description_log_event *fd_event,
                                  char** query)
{
  DBUG_ASSERT((Log_event_type)buf[EVENT_TYPE_OFFSET] ==
              binary_log::QUERY_EVENT);

  char db_len;                                  /* size of db name */
  uint status_vars_len= 0;                      /* size of status_vars */
  size_t qlen;                                  /* size of the query */
  int checksum_size= 0;                         /* size of trailing checksum */
  const char *end_of_query;

  uint common_header_len= fd_event->common_header_len;
  uint query_header_len= fd_event->post_header_len[binary_log::QUERY_EVENT-1];

  /* Error if the event content is too small */
  if (length < (common_header_len + query_header_len))
    goto err;

  /* Skip the header */
  buf+= common_header_len;

  /* Check if there are status variables in the event */
  if ((query_header_len - QUERY_HEADER_MINIMAL_LEN) > 0)
  {
    status_vars_len= uint2korr(buf + Q_STATUS_VARS_LEN_OFFSET);
  }

  /* Check if the event has trailing checksum */
  if (fd_event->common_footer->checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_OFF)
    checksum_size= 4;

  db_len= (uchar)buf[Q_DB_LEN_OFFSET];

  /* Error if the event content is too small */
  if (length < (common_header_len + query_header_len +
                db_len + 1 + status_vars_len + checksum_size))
    goto err;

  *query= (char *)buf + query_header_len + db_len + 1 + status_vars_len;

  /* Calculate the query length */
  end_of_query= buf + (length - common_header_len) - /* we skipped the header */
                checksum_size;
  qlen= end_of_query - *query;
  return qlen;

err:
  *query= NULL;
  return 0;
}


/**************************************************************************
	Start_log_event_v3 methods
**************************************************************************/
#ifndef MYSQL_CLIENT
Start_log_event_v3::Start_log_event_v3()
  : binary_log::Start_event_v3(),
    Log_event(header(), footer(), Log_event::EVENT_INVALID_CACHE,
              Log_event::EVENT_INVALID_LOGGING)
{
  is_valid_param= true;
}
#endif

/*
  Start_log_event_v3::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Start_log_event_v3::pack_info(Protocol *protocol)
{
  char buf[12 + ST_SERVER_VER_LEN + 14 + 22], *pos;
  pos= my_stpcpy(buf, "Server ver: ");
  pos= my_stpcpy(pos, server_version);
  pos= my_stpcpy(pos, ", Binlog ver: ");
  pos= int10_to_str(binlog_version, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
  return 0;
}
#endif


/*
  Start_log_event_v3::print()
*/

#ifdef MYSQL_CLIENT
void Start_log_event_v3::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  DBUG_ENTER("Start_log_event_v3::print");

  IO_CACHE *const head= &print_event_info->head_cache;

  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tStart: binlog v %d, server v %s created ",
                binlog_version, server_version);
    print_timestamp(head, NULL);
    if (created)
      my_b_printf(head," at startup");
    my_b_printf(head, "\n");
    if (common_header->flags & LOG_EVENT_BINLOG_IN_USE_F)
      my_b_printf(head, "# Warning: this binlog is either in use or was not "
                  "closed properly.\n");
  }

  if (is_relay_log_event())
  {
    my_b_printf(head, "# This Format_description_event appears in a relay log "
                "and was generated by the slave thread.\n");
    DBUG_VOID_RETURN;
  }

  if (!is_artificial_event() && created)
  {
#ifdef WHEN_WE_HAVE_THE_RESET_CONNECTION_SQL_COMMAND
    /*
      This is for mysqlbinlog: like in replication, we want to delete the stale
      tmp files left by an unclean shutdown of mysqld (temporary tables)
      and rollback unfinished transaction.
      Probably this can be done with RESET CONNECTION (syntax to be defined).
    */
    my_b_printf(head,"RESET CONNECTION%s\n", print_event_info->delimiter);
#else
    my_b_printf(head,"ROLLBACK%s\n", print_event_info->delimiter);
#endif
  }
  if (temp_buf &&
      print_event_info->base64_output_mode != BASE64_OUTPUT_NEVER &&
      !print_event_info->short_form)
  {
    if (print_event_info->base64_output_mode != BASE64_OUTPUT_DECODE_ROWS)
      my_b_printf(head, "BINLOG '\n");
    print_base64(head, print_event_info, FALSE);
    print_event_info->printed_fd_event= TRUE;

    /*
      If --skip-gtids is given, the server when it replays the output
      should generate a new GTID if gtid_mode=ON.  However, when the
      server reads the base64-encoded Format_description_log_event, it
      will cleverly detect that this is a binlog to be replayed, and
      act a little bit like the replication thread, in the following
      sense: if the thread does not see any 'SET GTID_NEXT' statement,
      it will assume the binlog was created by an old server and try
      to preserve transactions as anonymous.  This is the opposite of
      what we want when passing the --skip-gtids flag, so therefore we
      output the following statement.

      The behavior where the client preserves transactions following a
      Format_description_log_event as anonymous was introduced in
      5.6.16.
    */
    if (print_event_info->skip_gtids)
      my_b_printf(head, "/*!50616 SET @@SESSION.GTID_NEXT='AUTOMATIC'*/%s\n",
                  print_event_info->delimiter);
  }
  DBUG_VOID_RETURN;
}
#endif /* MYSQL_CLIENT */

/*
  Start_log_event_v3::Start_log_event_v3()
*/

Start_log_event_v3::Start_log_event_v3(const char* buf, uint event_len,
                                       const Format_description_event
                                       *description_event)
: binary_log::Start_event_v3(buf, event_len, description_event),
  Log_event(header(), footer())
{
  is_valid_param= server_version[0] != 0;
  if (event_len < (uint)description_event->common_header_len +
      ST_COMMON_HEADER_LEN_OFFSET)
  {
    server_version[0]= 0;
    return;
  }
  buf+= description_event->common_header_len;
  binlog_version= uint2korr(buf+ST_BINLOG_VER_OFFSET);
  memcpy(server_version, buf+ST_SERVER_VER_OFFSET,
	 ST_SERVER_VER_LEN);
  // prevent overrun if log is corrupted on disk
  server_version[ST_SERVER_VER_LEN-1]= 0;
  created= uint4korr(buf+ST_CREATED_OFFSET);
  dont_set_created= 1;
}


/*
  Start_log_event_v3::write()
*/

#ifndef MYSQL_CLIENT
bool Start_log_event_v3::write(IO_CACHE* file)
{
  char buff[Binary_log_event::START_V3_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= get_time();
  int4store(buff + ST_CREATED_OFFSET, static_cast<uint32>(created));
  return (write_header(file, sizeof(buff)) ||
          wrapper_my_b_safe_write(file, (uchar*) buff, sizeof(buff)) ||
	  write_footer(file));
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
  Start_log_event_v3::do_apply_event() .
  The master started

    IMPLEMENTATION
    - To handle the case where the master died without having time to write
    DROP TEMPORARY TABLE, DO RELEASE_LOCK (prepared statements' deletion is
    TODO), we clean up all temporary tables that we got, if we are sure we
    can (see below).

  @todo
    - Remove all active user locks.
    Guilhem 2003-06: this is true but not urgent: the worst it can cause is
    the use of a bit of memory for a user lock which will not be used
    anymore. If the user lock is later used, the old one will be released. In
    other words, no deadlock problem.
*/

int Start_log_event_v3::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Start_log_event_v3::do_apply_event");
  int error= 0;
  switch (binlog_version)
  {
  case 3:
  case 4:
    /*
      This can either be 4.x (then a Start_log_event_v3 is only at master
      startup so we are sure the master has restarted and cleared his temp
      tables; the event always has 'created'>0) or 5.0 (then we have to test
      'created').
    */
    if (created)
    {
      error= close_temporary_tables(thd);
      cleanup_load_tmpdir();
    }
    else
    {
      /*
        Set all temporary tables thread references to the current thread
        as they may point to the "old" SQL slave thread in case of its
        restart.
      */
      TABLE *table;
      for (table= thd->temporary_tables; table; table= table->next)
        table->in_use= thd;
    }
    break;

    /*
       Now the older formats; in that case load_tmpdir is cleaned up by the I/O
       thread.
    */
  case 1:
    if (strncmp(rli->get_rli_description_event()->server_version,
                "3.23.57",7) >= 0 && created)
    {
      /*
        Can distinguish, based on the value of 'created': this event was
        generated at master startup.
      */
      error= close_temporary_tables(thd);
    }
    /*
      Otherwise, can't distinguish a Start_log_event generated at
      master startup and one generated by master FLUSH LOGS, so cannot
      be sure temp tables have to be dropped. So do nothing.
    */
    break;
  default:
    /*
      This case is not expected. It can be either an event corruption or an
      unsupported binary log version.
    */
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                "Binlog version not supported");
    DBUG_RETURN(1);
  }
  DBUG_RETURN(error);
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */

/***************************************************************************
       Format_description_log_event methods
****************************************************************************/

/**
  Format_description_log_event 1st ctor.

    Ctor. Can be used to create the event to write to the binary log (when the
    server starts or when FLUSH LOGS), or to create artificial events to parse
    binlogs from MySQL 3.23 or 4.x.
    When in a client, only the 2nd use is possible.

  @param binlog_version         the binlog version for which we want to build
                                an event. Can be 1 (=MySQL 3.23), 3 (=4.0.x
                                x>=2 and 4.1) or 4 (MySQL 5.0). Note that the
                                old 4.0 (binlog version 2) is not supported;
                                it should not be used for replication with
                                5.0.
  @param server_ver             a string containing the server version.
*/

Format_description_log_event::
Format_description_log_event(uint8_t binlog_ver, const char* server_ver)
: binary_log::Start_event_v3(binary_log::FORMAT_DESCRIPTION_EVENT),
  Format_description_event(binlog_ver,  (binlog_ver <= 3 || server_ver != 0) ?
                           server_ver : ::server_version)
{
  is_valid_param= header_is_valid() && version_is_valid();
  common_header->type_code= binary_log::FORMAT_DESCRIPTION_EVENT;
  /*
   We here have the possibility to simulate a master before we changed
   the table map id to be stored in 6 bytes: when it was stored in 4
   bytes (=> post_header_len was 6). This is used to test backward
   compatibility.
   This code can be removed after a few months (today is Dec 21st 2005),
   when we know that the 4-byte masters are not deployed anymore (check
   with Tomas Ulin first!), and the accompanying test (rpl_row_4_bytes)
   too.
  */
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  post_header_len[binary_log::TABLE_MAP_EVENT-1]=
                  post_header_len[binary_log::WRITE_ROWS_EVENT_V1-1]=
                  post_header_len[binary_log::UPDATE_ROWS_EVENT_V1-1]=
                  post_header_len[binary_log::DELETE_ROWS_EVENT_V1-1]= 6;);
}


/**
  The problem with this constructor is that the fixed header may have a
  length different from this version, but we don't know this length as we
  have not read the Format_description_log_event which says it, yet. This
  length is in the post-header of the event, but we don't know where the
  post-header starts.

  So this type of event HAS to:
  - either have the header's length at the beginning (in the header, at a
  fixed position which will never be changed), not in the post-header. That
  would make the header be "shifted" compared to other events.
  - or have a header of size LOG_EVENT_MINIMAL_HEADER_LEN (19), in all future
  versions, so that we know for sure.

  I (Guilhem) chose the 2nd solution. Rotate has the same constraint (because
  it is sent before Format_description_log_event).
*/

Format_description_log_event::
Format_description_log_event(const char* buf, uint event_len,
                             const Format_description_event
                             *description_event)
  : binary_log::Start_event_v3(buf, event_len, description_event),
    Format_description_event(buf, event_len, description_event),
    Start_log_event_v3(buf, event_len, description_event)
{
  is_valid_param= header_is_valid() && version_is_valid();
  common_header->type_code= binary_log::FORMAT_DESCRIPTION_EVENT;

  /*
   We here have the possibility to simulate a master of before we changed
   the table map id to be stored in 6 bytes: when it was stored in 4
   bytes (=> post_header_len was 6). This is used to test backward
   compatibility.
 */
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  post_header_len[binary_log::TABLE_MAP_EVENT-1]=
                  post_header_len[binary_log::WRITE_ROWS_EVENT_V1-1]=
                  post_header_len[binary_log::UPDATE_ROWS_EVENT_V1-1]=
                  post_header_len[binary_log::DELETE_ROWS_EVENT_V1-1]= 6;);
}

#ifndef MYSQL_CLIENT
bool Format_description_log_event::write(IO_CACHE* file)
{
  bool ret;
  bool no_checksum;
  /*
    We don't call Start_log_event_v3::write() because this would make 2
    my_b_safe_write().
  */
  uchar buff[Binary_log_event::FORMAT_DESCRIPTION_HEADER_LEN +
             BINLOG_CHECKSUM_ALG_DESC_LEN];
  size_t rec_size= sizeof(buff);
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy((char*) buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= get_time();
  int4store(buff + ST_CREATED_OFFSET, static_cast<uint32>(created));
  buff[ST_COMMON_HEADER_LEN_OFFSET]= LOG_EVENT_HEADER_LEN;
  memcpy((char*) buff+ST_COMMON_HEADER_LEN_OFFSET + 1,  &post_header_len.front(),
         Binary_log_event::LOG_EVENT_TYPES);
  /*
    if checksum is requested
    record the checksum-algorithm descriptor next to
    post_header_len vector which will be followed by the checksum value.
    Master is supposed to trigger checksum computing by binlog_checksum_options,
    slave does it via marking the event according to
    FD_queue checksum_alg value.
  */
  compile_time_assert(sizeof(BINLOG_CHECKSUM_ALG_DESC_LEN == 1));
#ifndef DBUG_OFF
  common_header->data_written= 0; // to prepare for need_checksum assert
#endif
  buff[Binary_log_event::FORMAT_DESCRIPTION_HEADER_LEN]= need_checksum() ?
    (uint8) common_footer->checksum_alg :
     (uint8) binary_log::BINLOG_CHECKSUM_ALG_OFF;
  /*
     FD of checksum-aware server is always checksum-equipped, (V) is in,
     regardless of @@global.binlog_checksum policy.
     Thereby a combination of (A) == 0, (V) != 0 means
     it's the checksum-aware server's FD event that heads checksum-free binlog
     file.
     Here 0 stands for checksumming OFF to evaluate (V) as 0 is that case.
     A combination of (A) != 0, (V) != 0 denotes FD of the checksum-aware server
     heading the checksummed binlog.
     (A), (V) presence in FD of the checksum-aware server makes the event
     1 + 4 bytes bigger comparing to the former FD.
  */

  if ((no_checksum= (common_footer->checksum_alg ==
                     binary_log::BINLOG_CHECKSUM_ALG_OFF)))
  {
    // Forcing (V) room to fill anyway
    common_footer->checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_CRC32;
  }
  ret= (write_header(file, rec_size) ||
        wrapper_my_b_safe_write(file, buff, rec_size) ||
        write_footer(file));
  if (no_checksum)
    common_footer->checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_OFF;
  return ret;
}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Format_description_log_event::do_apply_event(Relay_log_info const *rli)
{
  int ret= 0;
  DBUG_ENTER("Format_description_log_event::do_apply_event");

  /*
    As a transaction NEVER spans on 2 or more binlogs:
    if we have an active transaction at this point, the master died
    while writing the transaction to the binary log, i.e. while
    flushing the binlog cache to the binlog. XA guarantees that master has
    rolled back. So we roll back.
    Note: this event could be sent by the master to inform us of the
    format of its binlog; in other words maybe it is not at its
    original place when it comes to us; we'll know this by checking
    log_pos ("artificial" events have log_pos == 0).
  */
  if (!thd->rli_fake && !is_artificial_event() && created &&
      thd->get_transaction()->is_active(Transaction_ctx::SESSION))
  {
    /* This is not an error (XA is safe), just an information */
    rli->report(INFORMATION_LEVEL, 0,
                "Rolling back unfinished transaction (no COMMIT "
                "or ROLLBACK in relay log). A probable cause is that "
                "the master died while writing the transaction to "
                "its binary log, thus rolled back too."); 
    const_cast<Relay_log_info*>(rli)->cleanup_context(thd, 1);
  }

  /*
    If this event comes from ourselves, there is no cleaning task to
    perform, we don't call Start_log_event_v3::do_apply_event()
    (this was just to update the log's description event).
  */
  if (server_id != (uint32) ::server_id)
  {
    /*
      If the event was not requested by the slave i.e. the master sent
      it while the slave asked for a position >4, the event will make
      rli->group_master_log_pos advance. Say that the slave asked for
      position 1000, and the Format_desc event's end is 96. Then in
      the beginning of replication rli->group_master_log_pos will be
      0, then 96, then jump to first really asked event (which is
      >96). So this is ok.
    */
    ret= Start_log_event_v3::do_apply_event(rli);
  }

  if (!ret)
  {
    /* Save the information describing this binlog */
    const_cast<Relay_log_info *>(rli)->set_rli_description_event(this);
  }

  DBUG_RETURN(ret);
}

int Format_description_log_event::do_update_pos(Relay_log_info *rli)
{
  if (server_id == (uint32) ::server_id)
  {
    /*
      We only increase the relay log position if we are skipping
      events and do not touch any group_* variables, nor flush the
      relay log info.  If there is a crash, we will have to re-skip
      the events again, but that is a minor issue.

      If we do not skip stepping the group log position (and the
      server id was changed when restarting the server), it might well
      be that we start executing at a position that is invalid, e.g.,
      at a Rows_log_event or a Query_log_event preceeded by a
      Intvar_log_event instead of starting at a Table_map_log_event or
      the Intvar_log_event respectively.
     */
    rli->inc_event_relay_log_pos();
    return 0;
  }
  else
  {
    return Log_event::do_update_pos(rli);
  }
}

Log_event::enum_skip_reason
Format_description_log_event::do_shall_skip(Relay_log_info *rli)
{
  return Log_event::EVENT_SKIP_NOT;
}

#endif



  /**************************************************************************
        Load_log_event methods
   General note about Load_log_event: the binlogging of LOAD DATA INFILE is
   going to be changed in 5.0 (or maybe in 5.1; not decided yet).
   However, the 5.0 slave could still have to read such events (from a 4.x
   master), convert them (which just means maybe expand the header, when 5.0
   servers have a UID in events) (remember that whatever is after the header
   will be like in 4.x, as this event's format is not modified in 5.0 as we
   will use new types of events to log the new LOAD DATA INFILE features).
   To be able to read/convert, we just need to not assume that the common
   header is of length LOG_EVENT_HEADER_LEN (we must use the description
   event).
   Note that I (Guilhem) manually tested replication of a big LOAD DATA INFILE
   between 3.23 and 5.0, and between 4.0 and 5.0, and it works fine (and the
   positions displayed in SHOW SLAVE STATUS then are fine too).
  **************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
uint Load_log_event::get_query_buffer_length()
{
  return
    //the DB name may double if we escape the quote character
    5 + 2*db_len + 3 +
    18 + fname_len*4 + 2 +                    // "LOAD DATA INFILE 'file''"
    11 +                                    // "CONCURRENT "
    7 +					    // LOCAL
    9 +                                     // " REPLACE or IGNORE "
    13 + table_name_len*2 +                 // "INTO TABLE `table`"
    21 + sql_ex.data_info.field_term_len*4 + 2 +
                                            // " FIELDS TERMINATED BY 'str'"
    23 + sql_ex.data_info.enclosed_len*4 + 2 +
                                            // " OPTIONALLY ENCLOSED BY 'str'"
    12 + sql_ex.data_info.escaped_len*4 + 2 +         // " ESCAPED BY 'str'"
    21 + sql_ex.data_info.line_term_len*4 + 2 +
                                            // " LINES TERMINATED BY 'str'"
    19 + sql_ex.data_info.line_start_len*4 + 2 +
                                            // " LINES STARTING BY 'str'"
    15 + 22 +                               // " IGNORE xxx  LINES"
    3 + (num_fields-1)*2 + field_block_len; // " (field1, field2, ...)"
}


void Load_log_event::print_query(bool need_db, const char *cs, char *buf,
                                 char **end, char **fn_start, char **fn_end)
{
  char quoted_id[1 + NAME_LEN * 2 + 2];//quoted  length
  size_t  quoted_id_len= 0;
  char *pos= buf;

  if (need_db && db && db_len)
  {
    pos= my_stpcpy(pos, "use ");
#ifdef MYSQL_SERVER
    quoted_id_len= my_strmov_quoted_identifier(this->thd, (char *) quoted_id,
                                               db, 0);
#else
    quoted_id_len= my_strmov_quoted_identifier((char *) quoted_id, db);
#endif
    quoted_id[quoted_id_len]= '\0';
    pos= my_stpcpy(pos, quoted_id);
    pos= my_stpcpy(pos, "; ");
  }

  pos= my_stpcpy(pos, "LOAD DATA ");

  if (is_concurrent)
    pos= my_stpcpy(pos, "CONCURRENT ");

  if (fn_start)
    *fn_start= pos;

  if (check_fname_outside_temp_buf())
    pos= my_stpcpy(pos, "LOCAL ");
  pos= my_stpcpy(pos, "INFILE ");
  pos= pretty_print_str(pos, fname, fname_len);
  pos= my_stpcpy(pos, " ");

  if (sql_ex.data_info.opt_flags & REPLACE_FLAG)
    pos= my_stpcpy(pos, "REPLACE ");
  else if (sql_ex.data_info.opt_flags & IGNORE_FLAG)
    pos= my_stpcpy(pos, "IGNORE ");

  pos= my_stpcpy(pos ,"INTO");

  if (fn_end)
    *fn_end= pos;

  pos= my_stpcpy(pos ," TABLE ");
  memcpy(pos, table_name, table_name_len);
  pos+= table_name_len;

  if (cs != NULL)
  {
    pos= my_stpcpy(pos ," CHARACTER SET ");
    pos= my_stpcpy(pos ,  cs);
  }

  /* We have to create all optional fields as the default is not empty */
  pos= my_stpcpy(pos, " FIELDS TERMINATED BY ");
  pos= pretty_print_str(pos, sql_ex.data_info.field_term,
                        sql_ex.data_info.field_term_len);
  if (sql_ex.data_info.opt_flags & OPT_ENCLOSED_FLAG)
    pos= my_stpcpy(pos, " OPTIONALLY ");
  pos= my_stpcpy(pos, " ENCLOSED BY ");
  pos= pretty_print_str(pos, sql_ex.data_info.enclosed,
                        sql_ex.data_info.enclosed_len);

  pos= my_stpcpy(pos, " ESCAPED BY ");
  pos= pretty_print_str(pos, sql_ex.data_info.escaped,
                        sql_ex.data_info.escaped_len);

  pos= my_stpcpy(pos, " LINES TERMINATED BY ");
  pos= pretty_print_str(pos, sql_ex.data_info.line_term,
                        sql_ex.data_info.line_term_len);
  if (sql_ex.data_info.line_start_len)
  {
    pos= my_stpcpy(pos, " STARTING BY ");
    pos= pretty_print_str(pos, sql_ex.data_info.line_start,
                          sql_ex.data_info.line_start_len);
  }

  if ((long) skip_lines > 0)
  {
    pos= my_stpcpy(pos, " IGNORE ");
    pos= longlong10_to_str((longlong) skip_lines, pos, 10);
    pos= my_stpcpy(pos," LINES ");    
  }

  if (num_fields)
  {
    uint i;
    const char *field= fields;
    pos= my_stpcpy(pos, " (");
    for (i = 0; i < num_fields; i++)
    {
      if (i)
      {
        *pos++= ' ';
        *pos++= ',';
      }
      quoted_id_len= my_strmov_quoted_identifier(this->thd, quoted_id, field,
                                                 0);
      memcpy(pos, quoted_id, quoted_id_len-1);
    }
    *pos++= ')';
  }

  *end= pos;
}


int Load_log_event::pack_info(Protocol *protocol)
{
  char *buf, *end;

  if (!(buf= (char*) my_malloc(key_memory_log_event,
                               get_query_buffer_length(), MYF(MY_WME))))
    return 1;
  print_query(TRUE, NULL, buf, &end, 0, 0);
  protocol->store(buf, end-buf, &my_charset_bin);
  my_free(buf);
  return 0;
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


#ifndef MYSQL_CLIENT

/*
  Load_log_event::write_data_header()
*/

bool Load_log_event::write_data_header(IO_CACHE* file)
{
  char buf[Binary_log_event::LOAD_HEADER_LEN];
  int4store(buf + L_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  return my_b_safe_write(file, (uchar*)buf, Binary_log_event::LOAD_HEADER_LEN) != 0;
}


/*
  Load_log_event::write_data_body()
*/

bool Load_log_event::write_data_body(IO_CACHE* file)
{
  if (sql_ex.write_data(file))
    return 1;
  if (num_fields && fields && field_lens)
  {
    if (my_b_safe_write(file, (uchar*)field_lens, num_fields) ||
	my_b_safe_write(file, (uchar*)fields, field_block_len))
      return 1;
  }
  return (my_b_safe_write(file, (uchar*)table_name, table_name_len + 1) ||
	  my_b_safe_write(file, (uchar*)db, db_len + 1) ||
	  my_b_safe_write(file, (uchar*)fname, fname_len));
}


/*
  Load_log_event::Load_log_event()
*/

Load_log_event::Load_log_event(THD *thd_arg, sql_exchange *ex,
			       const char *db_arg, const char *table_name_arg,
			       List<Item> &fields_arg,
                               bool is_concurrent_arg,
			       enum enum_duplicates handle_dup,
			       bool ignore, bool using_trans)
  : binary_log::Load_event(),
   Log_event(thd_arg,
             thd_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F : 0,
             using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                           Log_event::EVENT_STMT_CACHE,
             Log_event::EVENT_NORMAL_LOGGING,
             header(), footer())
{
  thread_id= thd_arg->thread_id();
  slave_proxy_id= thd_arg->variables.pseudo_thread_id;
  table_name= table_name_arg ? table_name_arg : "";
  db= db_arg;
  fname= ex->file_name;
  local_fname= FALSE;
  is_concurrent= is_concurrent_arg;

  /*
  exec_time calculation has changed to use the same method that is used
  to fill out "thd_arg->start_time"
  */

  struct timeval end_time;
  ulonglong micro_end_time= my_micro_time();
  my_micro_time_to_timeval(micro_end_time, &end_time);

  exec_time= end_time.tv_sec - thd_arg->start_time.tv_sec;

  /* db can never be a zero pointer in 4.0 */
  db_len = strlen(db);
  table_name_len =  strlen(table_name);
  fname_len = (fname) ?  strlen(fname) : 0;
  sql_ex.data_info.field_term = ex->field.field_term->ptr();
  sql_ex.data_info.field_term_len = (uint8) ex->field.field_term->length();
  sql_ex.data_info.enclosed = ex->field.enclosed->ptr();
  sql_ex.data_info.enclosed_len = (uint8) ex->field.enclosed->length();
  sql_ex.data_info.line_term = ex->line.line_term->ptr();
  sql_ex.data_info.line_term_len = (uint8) ex->line.line_term->length();
  sql_ex.data_info.line_start = ex->line.line_start->ptr();
  sql_ex.data_info.line_start_len = (uint8) ex->line.line_start->length();
  sql_ex.data_info.escaped = (char*) ex->field.escaped->ptr();
  sql_ex.data_info.escaped_len = (uint8) ex->field.escaped->length();
  sql_ex.data_info.opt_flags = 0;
  sql_ex.data_info.cached_new_format = -1;

  if (ex->dumpfile)
    sql_ex.data_info.opt_flags|= DUMPFILE_FLAG;
  if (ex->field.opt_enclosed)
    sql_ex.data_info.opt_flags|= OPT_ENCLOSED_FLAG;

  sql_ex.data_info.empty_flags= 0;

  switch (handle_dup) {
  case DUP_REPLACE:
    sql_ex.data_info.opt_flags|= REPLACE_FLAG;
    break;
  case DUP_UPDATE:				// Impossible here
  case DUP_ERROR:
    break;
  }
  if (ignore)
    sql_ex.data_info.opt_flags|= IGNORE_FLAG;

  if (!ex->field.field_term->length())
    sql_ex.data_info.empty_flags |= FIELD_TERM_EMPTY;
  if (!ex->field.enclosed->length())
    sql_ex.data_info.empty_flags |= ENCLOSED_EMPTY;
  if (!ex->line.line_term->length())
    sql_ex.data_info.empty_flags |= LINE_TERM_EMPTY;
  if (!ex->line.line_start->length())
    sql_ex.data_info.empty_flags |= LINE_START_EMPTY;
  if (!ex->field.escaped->length())
    sql_ex.data_info.empty_flags |= ESCAPED_EMPTY;

  skip_lines = ex->skip_lines;

  List_iterator<Item> li(fields_arg);
  field_lens_buf.length(0);
  fields_buf.length(0);
  Item* item;
  while ((item = li++))
  {
    num_fields++;
    uchar len= (uchar) item->item_name.length();
    field_block_len += len + 1;
    fields_buf.append(item->item_name.ptr(), len + 1);
    field_lens_buf.append((char*)&len, 1);
  }

  field_lens = (const uchar*)field_lens_buf.ptr();
  fields = fields_buf.ptr();
  if (table_name != 0)
    is_valid_param= true;

  if (sql_ex.data_info.new_format())
    common_header->type_code= binary_log::NEW_LOAD_EVENT;
  else
    common_header->type_code= binary_log::LOAD_EVENT;
}
#endif /* !MYSQL_CLIENT */


/**
  @note
    The caller must do buf[event_len] = 0 before he starts using the
    constructed event.
*/
Load_log_event::Load_log_event(const char *buf, uint event_len,
                               const Format_description_event *description_event)
: binary_log::Load_event(buf, event_len, description_event),
  Log_event(header(), footer())
{
  DBUG_ENTER("Load_log_event");
  if (table_name != 0)
    is_valid_param= true;
  thread_id= slave_proxy_id;
  if (event_len)
  {
    /**
      We need to set exec_time here, which is ued to calcutate seconds behind
      master on the slave.
    */
    exec_time= load_exec_time;
    /*
      I (Guilhem) manually tested replication of LOAD DATA INFILE for 3.23->5.0,
      4.0->5.0 and 5.0->5.0 and it works.
    */
    sql_ex.data_info= sql_ex_data;
  }
  if (sql_ex.data_info.new_format())
    common_header->type_code= binary_log::NEW_LOAD_EVENT;
  else
    common_header->type_code= binary_log::LOAD_EVENT;
  DBUG_VOID_RETURN;
}


/*
  Load_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Load_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}


void Load_log_event::print(FILE* file_arg, PRINT_EVENT_INFO* print_event_info,
			   bool commented)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  size_t id_len= 0;
  char str_buf[1 + 2*FN_REFLEN + 2];

  DBUG_ENTER("Load_log_event::print");
  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tQuery\tthread_id=%u\texec_time=%ld\n",
                thread_id, exec_time);
  }

  bool different_db= 1;
  if (db)
  {
    /*
      If the database is different from the one of the previous statement, we
      need to print the "use" command, and we update the last_db.
      But if commented, the "use" is going to be commented so we should not
      update the last_db.
    */
    if ((different_db= memcmp(print_event_info->db, db, db_len + 1)) &&
        !commented)
      memcpy(print_event_info->db, db, db_len + 1);
  }
  
  if (db && db[0] && different_db)
  {
#ifdef MYSQL_SERVER
    id_len= my_strmov_quoted_identifier(this->thd, str_buf, db, 0);
#else
    id_len= my_strmov_quoted_identifier(str_buf, db);
#endif
    str_buf[id_len]= '\0';
    my_b_printf(head, "%suse %s%s\n",
                commented ? "# " : "", str_buf, print_event_info->delimiter);
  }
  if (common_header->flags & LOG_EVENT_THREAD_SPECIFIC_F)
    my_b_printf(head,"%sSET @@session.pseudo_thread_id=%lu%s\n",
            commented ? "# " : "", (ulong)thread_id,
            print_event_info->delimiter);
  my_b_printf(head, "%sLOAD DATA ",
              commented ? "# " : "");
  if (check_fname_outside_temp_buf())
    my_b_printf(head, "LOCAL ");
  my_b_printf(head, "INFILE '%-*s' ", static_cast<int>(fname_len), fname);

  if (sql_ex.data_info.opt_flags & REPLACE_FLAG)
    my_b_printf(head,"REPLACE ");
  else if (sql_ex.data_info.opt_flags & IGNORE_FLAG)
    my_b_printf(head,"IGNORE ");

#ifdef MYSQL_SERVER
    id_len= my_strmov_quoted_identifier(this->thd, str_buf, table_name, 0);
#else
    id_len= my_strmov_quoted_identifier(str_buf, table_name);
#endif
  str_buf[id_len]= '\0';
  my_b_printf(head, "INTO TABLE %s", str_buf);

  my_b_printf(head, " FIELDS TERMINATED BY ");
  pretty_print_str(head, sql_ex.data_info.field_term,
                   sql_ex.data_info.field_term_len);

  if (sql_ex.data_info.opt_flags & OPT_ENCLOSED_FLAG)
    my_b_printf(head," OPTIONALLY ");
  my_b_printf(head, " ENCLOSED BY ");
  pretty_print_str(head, sql_ex.data_info.enclosed,
                   sql_ex.data_info.enclosed_len);

  my_b_printf(head, " ESCAPED BY ");
  pretty_print_str(head, sql_ex.data_info.escaped,
                   sql_ex.data_info.escaped_len);

  my_b_printf(head," LINES TERMINATED BY ");
  pretty_print_str(head, sql_ex.data_info.line_term,
                   sql_ex.data_info.line_term_len);


  if (sql_ex.data_info.line_start)
  {
    my_b_printf(head," STARTING BY ");
    pretty_print_str(head, sql_ex.data_info.line_start,
                     sql_ex.data_info.line_start_len);
  }
  if ((long) skip_lines > 0)
    my_b_printf(head, " IGNORE %ld LINES", (long) skip_lines);

  if (num_fields)
  {
    uint i;
    const char* field = fields;
    my_b_printf(head, " (");
    for (i = 0; i < num_fields; i++)
    {
      if (i)
        my_b_printf(head, ",");
      id_len= my_strmov_quoted_identifier((char *) str_buf, field);
      str_buf[id_len]= '\0';
      my_b_printf(head, "%s", str_buf);

      field += field_lens[i]  + 1;
    }
    my_b_printf(head, ")");
  }

  my_b_printf(head, "%s\n", print_event_info->delimiter);
  DBUG_VOID_RETURN;
}
#endif /* MYSQL_CLIENT */

#ifndef MYSQL_CLIENT

/**
  Load_log_event::set_fields()

  @note
    This function can not use the member variable 
    for the database, since LOAD DATA INFILE on the slave
    can be for a different database than the current one.
    This is the reason for the affected_db argument to this method.
*/

void Load_log_event::set_fields(const char* affected_db, 
				List<Item> &field_list,
                                Name_resolution_context *context)
{
  uint i;
  const char* field = fields;
  for (i= 0; i < num_fields; i++)
  {
    field_list.push_back(new Item_field(context,
                                        affected_db, table_name, field));
    field+= field_lens[i]  + 1;
  }
}
#endif /* !MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
/**
  Does the data loading job when executing a LOAD DATA on the slave.

  @param net
  @param rli
  @param use_rli_only_for_errors     If set to 1, rli is provided to
                                     Load_log_event::exec_event only for this
                                     function to have rli->get_rpl_log_name and
                                     rli->last_slave_error, both being used by
                                     error reports.  If set to 0, rli is provided
                                     for full use, i.e. for error reports and
                                     position advancing.

  @todo
    fix this; this can be done by testing rules in
    Create_file_log_event::exec_event() and then discarding Append_block and
    al.
  @todo
    this is a bug - this needs to be moved to the I/O thread

  @retval
    0           Success
  @retval
    1           Failure
*/

int Load_log_event::do_apply_event(NET* net, Relay_log_info const *rli,
                                   bool use_rli_only_for_errors)
{
  DBUG_ASSERT(thd->query().str == NULL);
  thd->reset_query();                    // Should not be needed
  set_thd_db(thd, db, db_len);
  thd->is_slave_error= 0;
  clear_all_errors(thd, const_cast<Relay_log_info*>(rli));

  /* see Query_log_event::do_apply_event() and BUG#13360 */
  DBUG_ASSERT(!rli->m_table_map.count());
  /*
    Usually lex_start() is called by mysql_parse(), but we need it here
    as the present method does not call mysql_parse().
  */
  lex_start(thd);
  thd->lex->local_file= local_fname;
  mysql_reset_thd_for_next_command(thd);

  /*
    It is possible that the thread does not hold anonymous GTID
    ownership here, e.g. in case this is the first event of a relay
    log.
  */
  gtid_reacquire_ownership_if_anonymous(thd);

   /*
    We test replicate_*_db rules. Note that we have already prepared
    the file to load, even if we are going to ignore and delete it
    now. So it is possible that we did a lot of disk writes for
    nothing. In other words, a big LOAD DATA INFILE on the master will
    still consume a lot of space on the slave (space in the relay log
    + space of temp files: twice the space of the file to load...)
    even if it will finally be ignored.  TODO: fix this; this can be
    done by testing rules in Create_file_log_event::do_apply_event()
    and then discarding Append_block and al. Another way is do the
    filtering in the I/O thread (more efficient: no disk writes at
    all).
  */
  if (rpl_filter->db_ok(thd->db().str))
  {
    thd->set_time(&(common_header->when));
    thd->set_query_id(next_query_id());
    DBUG_ASSERT(!thd->get_stmt_da()->is_set());

    TABLE_LIST tables;
    char table_buf[NAME_LEN + 1];
    my_stpcpy(table_buf, table_name);
    if (lower_case_table_names)
      my_casedn_str(system_charset_info, table_buf);
    tables.init_one_table(thd->strmake(thd->db().str, thd->db().length),
                          thd->db().length,
                          table_buf, strlen(table_buf),
                          table_buf, TL_WRITE);
    tables.updating= 1;

    // the table will be opened in mysql_load    
    if (rpl_filter->is_on() && !rpl_filter->tables_ok(thd->db().str, &tables))
    {
      // TODO: this is a bug - this needs to be moved to the I/O thread
      if (net)
        skip_load_data_infile(net);
    }
    else
    {
      char llbuff[22];
      char *end;
      enum enum_duplicates handle_dup;
      char *load_data_query;

      /*
        Forge LOAD DATA INFILE query which will be used in SHOW PROCESS LIST
        and written to slave's binlog if binlogging is on.
      */
      if (!(load_data_query= (char *)thd->alloc(get_query_buffer_length() + 1)))
      {
        /*
          This will set thd->fatal_error in case of OOM. So we surely will notice
          that something is wrong.
        */
        goto error;
      }

      print_query(FALSE, NULL, load_data_query, &end, NULL, NULL);
      *end= 0;
      thd->set_query(load_data_query, static_cast<size_t>(end - load_data_query));

      if (sql_ex.data_info.opt_flags & REPLACE_FLAG)
        handle_dup= DUP_REPLACE;
      else if (sql_ex.data_info.opt_flags & IGNORE_FLAG)
      {
        thd->lex->set_ignore(true);
        handle_dup= DUP_ERROR;
      }
      else
      {
        /*
          When replication is running fine, if it was DUP_ERROR on the
          master then we could choose IGNORE here, because if DUP_ERROR
          suceeded on master, and data is identical on the master and slave,
          then there should be no uniqueness errors on slave, so IGNORE is
          the same as DUP_ERROR. But in the unlikely case of uniqueness errors
          (because the data on the master and slave happen to be different
          (user error or bug), we want LOAD DATA to print an error message on
          the slave to discover the problem.

          If reading from net (a 3.23 master), mysql_load() will change this
          to IGNORE.
        */
        handle_dup= DUP_ERROR;
      }
      /*
        We need to set thd->lex->sql_command and thd->lex->duplicates
        since InnoDB tests these variables to decide if this is a LOAD
        DATA ... REPLACE INTO ... statement even though mysql_parse()
        is not called.  This is not needed in 5.0 since there the LOAD
        DATA ... statement is replicated using mysql_parse(), which
        sets the thd->lex fields correctly.
      */
      thd->lex->sql_command= SQLCOM_LOAD;
      thd->lex->duplicates= handle_dup;

      sql_exchange ex((char*)fname, sql_ex.data_info.opt_flags & DUMPFILE_FLAG);
      String field_term(sql_ex.data_info.field_term,
                        sql_ex.data_info.field_term_len,log_cs);
      String enclosed(sql_ex.data_info.enclosed,
                      sql_ex.data_info.enclosed_len,log_cs);
      String line_term(sql_ex.data_info.line_term,
                       sql_ex.data_info.line_term_len,log_cs);
      String line_start(sql_ex.data_info.line_start,
                        sql_ex.data_info.line_start_len,log_cs);
      String escaped(sql_ex.data_info.escaped,
                     sql_ex.data_info.escaped_len, log_cs);
      const String empty_str("", 0, log_cs);
      ex.field.field_term= &field_term;
      ex.field.enclosed= &enclosed;
      ex.line.line_term= &line_term;
      ex.line.line_start= &line_start;
      ex.field.escaped= &escaped;

      ex.field.opt_enclosed= (sql_ex.data_info.opt_flags & OPT_ENCLOSED_FLAG);
      if (sql_ex.data_info.empty_flags & FIELD_TERM_EMPTY)
        ex.field.field_term= &empty_str;

      ex.skip_lines= skip_lines;
      List<Item> field_list;
      thd->lex->select_lex->context.resolve_in_table_list_only(&tables);
      set_fields(tables.db, field_list, &thd->lex->select_lex->context);
      thd->variables.pseudo_thread_id= thread_id;
      if (net)
      {
        // mysql_load will use thd->net to read the file
        thd->get_protocol_classic()->set_vio(net->vio);
        // Make sure the client does not get confused about the packet sequence
        thd->get_protocol_classic()->set_pkt_nr(net->pkt_nr);
      }
      /*
        It is safe to use tmp_list twice because we are not going to
        update it inside mysql_load().
      */
      List<Item> tmp_list;
      /*
        Prepare column privilege check for LOAD statement.
        This is necessary because the replication code for LOAD bypasses
        regular privilege checking, which is done by check_one_table_access()
        in regular code path.
        We can assign INSERT privileges to the table since the slave thread
        operates with all privileges.
      */
      tables.set_privileges(INSERT_ACL);
      tables.set_want_privilege(INSERT_ACL);

      if (open_temporary_tables(thd, &tables) ||
          mysql_load(thd, &ex, &tables, field_list, tmp_list, tmp_list,
                     handle_dup, net != 0))
        thd->is_slave_error= 1;
      if (thd->cuted_fields)
      {
        /* log_pos is the position of the LOAD event in the master log */
        sql_print_warning("Slave: load data infile on table '%s' at "
                          "log position %s in log '%s' produced %ld "
                          "warning(s). Default database: '%s'",
                          (char*) table_name,
                          llstr(common_header->log_pos,llbuff),
                          const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                          (ulong) thd->cuted_fields,
                          print_slave_db_safe(thd->db().str));
      }
      if (net)
      {
        net->pkt_nr= thd->get_protocol_classic()->get_pkt_nr();
      }
    }
  }
  else
  {
    /*
      We will just ask the master to send us /dev/null if we do not
      want to load the data.
      TODO: this a bug - needs to be done in I/O thread
    */
    if (net)
      skip_load_data_infile(net);
  }

error:
  thd->get_protocol_classic()->set_vio(NULL);
  const char *remember_db= thd->db().str;
  thd->set_catalog(NULL_CSTR);
  thd->set_db(NULL_CSTR);                   /* will free the current database */
  thd->reset_query();
  thd->get_stmt_da()->set_overwrite_status(true);
  thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
  thd->get_stmt_da()->set_overwrite_status(false);
  close_thread_tables(thd);
  /*
    - If transaction rollback was requested due to deadlock
      perform it and release metadata locks.
    - If inside a multi-statement transaction,
    defer the release of metadata locks until the current
    transaction is either committed or rolled back. This prevents
    other statements from modifying the table for the entire
    duration of this transaction.  This provides commit ordering
    and guarantees serializability across multiple transactions.
    - If in autocommit mode, or outside a transactional context,
    automatically release metadata locks of the current statement.
  */
  if (thd->transaction_rollback_request)
  {
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  }
  else if (! thd->in_multi_stmt_transaction_mode())
    thd->mdl_context.release_transactional_locks();
  else
    thd->mdl_context.release_statement_locks();

  DBUG_EXECUTE_IF("LOAD_DATA_INFILE_has_fatal_error",
                  thd->is_slave_error= 0; thd->is_fatal_error= 1;);

  if (thd->is_slave_error)
  {
    /* this err/sql_errno code is copy-paste from net_send_error() */
    const char *err;
    int sql_errno;
    if (thd->is_error())
    {
      err= thd->get_stmt_da()->message_text();
      sql_errno= thd->get_stmt_da()->mysql_errno();
    }
    else
    {
      sql_errno=ER_UNKNOWN_ERROR;
      err=ER(sql_errno);       
    }
    rli->report(ERROR_LEVEL, sql_errno,"\
Error '%s' running LOAD DATA INFILE on table '%s'. Default database: '%s'",
                    err, (char*)table_name, print_slave_db_safe(remember_db));
    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    return 1;
  }
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));

  if (thd->is_fatal_error)
  {
    char buf[256];
    my_snprintf(buf, sizeof(buf),
                "Running LOAD DATA INFILE on table '%-.64s'."
                " Default database: '%-.64s'",
                (char*)table_name,
                print_slave_db_safe(remember_db));

    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER(ER_SLAVE_FATAL_ERROR), buf);
    return 1;
  }

  return ( use_rli_only_for_errors ? 0 : Log_event::do_apply_event(rli) ); 
}
#endif


/**************************************************************************
  Rotate_log_event methods
**************************************************************************/

/*
  Rotate_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rotate_log_event::pack_info(Protocol *protocol)
{
  char buf1[256], buf[22];
  String tmp(buf1, sizeof(buf1), log_cs);
  tmp.length(0);
  tmp.append(new_log_ident, ident_len);
  tmp.append(STRING_WITH_LEN(";pos="));
  tmp.append(llstr(pos,buf));
  protocol->store(tmp.ptr(), tmp.length(), &my_charset_bin);
  return 0;
}
#endif


/*
  Rotate_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Rotate_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char buf[22];
  IO_CACHE *const head= &print_event_info->head_cache;

  if (print_event_info->short_form)
    return;
  print_header(head, print_event_info, FALSE);
  my_b_printf(head, "\tRotate to ");
  if (new_log_ident)
    my_b_write(head, (uchar*) new_log_ident, (uint)ident_len);
  my_b_printf(head, "  pos: %s\n", llstr(pos, buf));
}
#endif /* MYSQL_CLIENT */



/*
  Rotate_log_event::Rotate_log_event() (2 constructors)
*/


#ifndef MYSQL_CLIENT
Rotate_log_event::Rotate_log_event(const char* new_log_ident_arg,
                                   size_t ident_len_arg, ulonglong pos_arg,
                                   uint flags_arg)
: binary_log::Rotate_event(new_log_ident_arg, ident_len_arg, flags_arg, pos_arg),
  Log_event(header(), footer(),
            Log_event::EVENT_NO_CACHE, Log_event::EVENT_IMMEDIATE_LOGGING)
{
#ifndef DBUG_OFF
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(...,flags)");
#endif
  new_log_ident= new_log_ident_arg;
  pos= pos_arg;
  ident_len= ident_len_arg ?
             ident_len_arg : (uint) strlen(new_log_ident_arg);
  flags= flags_arg;

#ifndef DBUG_OFF
  char buff[22];
  DBUG_PRINT("enter",("new_log_ident: %s  pos: %s  flags: %lu", new_log_ident_arg,
                      llstr(pos_arg, buff), (ulong) flags));
#endif
  if (flags & DUP_NAME)
    new_log_ident= my_strndup(key_memory_log_event,
                              new_log_ident_arg, ident_len, MYF(MY_WME));
  if (new_log_ident != 0)
    is_valid_param= true;
  if (flags & RELAY_LOG)
    set_relay_log_event();
  DBUG_VOID_RETURN;
}
#endif


Rotate_log_event::Rotate_log_event(const char* buf, uint event_len,
                                   const Format_description_event* description_event)
: binary_log::Rotate_event(buf, event_len, description_event),
  Log_event(header(), footer())
{
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(char*,...)");

  if (new_log_ident != 0)
    is_valid_param= true;
  DBUG_PRINT("debug", ("new_log_ident: '%s'", new_log_ident));
  DBUG_VOID_RETURN;
}


/*
  Rotate_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Rotate_log_event::write(IO_CACHE* file)
{
  char buf[Binary_log_event::ROTATE_HEADER_LEN];
  int8store(buf + R_POS_OFFSET, pos);
  return (write_header(file, Binary_log_event::ROTATE_HEADER_LEN + ident_len) || 
          wrapper_my_b_safe_write(file, (uchar*) buf, Binary_log_event::ROTATE_HEADER_LEN) ||
          wrapper_my_b_safe_write(file, (uchar*) new_log_ident,
                                     (uint) ident_len) ||
          write_footer(file));
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/*
  Got a rotate log event from the master.

  This is mainly used so that we can later figure out the logname and
  position for the master.

  We can't rotate the slave's BINlog as this will cause infinitive rotations
  in a A -> B -> A setup.
  The NOTES below is a wrong comment which will disappear when 4.1 is merged.

  This must only be called from the Slave SQL thread, since it calls
  flush_relay_log_info().

  @retval
    0	ok
*/
int Rotate_log_event::do_update_pos(Relay_log_info *rli)
{
  int error= 0;
  DBUG_ENTER("Rotate_log_event::do_update_pos");
#ifndef DBUG_OFF
  char buf[32];
#endif

  DBUG_PRINT("info", ("server_id=%lu; ::server_id=%lu",
                      (ulong) this->server_id, (ulong) ::server_id));
  DBUG_PRINT("info", ("new_log_ident: %s", this->new_log_ident));
  DBUG_PRINT("info", ("pos: %s", llstr(this->pos, buf)));

  /*
    If we are in a transaction or in a group: the only normal case is
    when the I/O thread was copying a big transaction, then it was
    stopped and restarted: we have this in the relay log:

    BEGIN
    ...
    ROTATE (a fake one)
    ...
    COMMIT or ROLLBACK

    In that case, we don't want to touch the coordinates which
    correspond to the beginning of the transaction.  Starting from
    5.0.0, there also are some rotates from the slave itself, in the
    relay log, which shall not change the group positions.
  */

  /*
    The way we check if SQL thread is currently in a group is different
    for STS and MTS.
  */
  bool in_group = rli->is_parallel_exec() ?
    (rli->mts_group_status == Relay_log_info::MTS_IN_GROUP) :
    rli->is_in_group();

  if ((server_id != ::server_id || rli->replicate_same_server_id) &&
      !is_relay_log_event() &&
      !in_group)
  {
    if (!is_mts_db_partitioned(rli) && server_id != ::server_id)
    {
      // force the coordinator to start a new binlog segment.
      static_cast<Mts_submode_logical_clock*>
        (rli->current_mts_submode)->start_new_group();
    }
    if (rli->is_parallel_exec())
    {
      /*
        Rotate events are special events that are handled as a
        synchronization point. For that reason, the checkpoint
        routine is being called here.
      */
      if ((error= mts_checkpoint_routine(rli, 0, false,
                                         true/*need_data_lock=true*/)))
        goto err;
    }

    mysql_mutex_lock(&rli->data_lock);
    DBUG_PRINT("info", ("old group_master_log_name: '%s'  "
                        "old group_master_log_pos: %lu",
                        rli->get_group_master_log_name(),
                        (ulong) rli->get_group_master_log_pos()));

    memcpy((void *)rli->get_group_master_log_name(),
           new_log_ident, ident_len + 1);
    rli->notify_group_master_log_name_update();
    if ((error= rli->inc_group_relay_log_pos(pos,
                                             false/*need_data_lock=false*/)))
    {
      mysql_mutex_unlock(&rli->data_lock);
      goto err;
    }

    DBUG_PRINT("info", ("new group_master_log_name: '%s'  "
                        "new group_master_log_pos: %lu",
                        rli->get_group_master_log_name(),
                        (ulong) rli->get_group_master_log_pos()));
    mysql_mutex_unlock(&rli->data_lock);
    if (rli->is_parallel_exec())
    {
      bool real_event= server_id && !is_artificial_event();
      rli->reset_notified_checkpoint(0,
                                     real_event ?
                                     common_header->when.tv_sec +
                                     (time_t) exec_time : 0,
                                     true/*need_data_lock=true*/,
                                     real_event? true : false);
    }

    /*
      Reset thd->variables.option_bits and sql_mode etc, because this could be the signal of
      a master's downgrade from 5.0 to 4.0.
      However, no need to reset rli_description_event: indeed, if the next
      master is 5.0 (even 5.0.1) we will soon get a Format_desc; if the next
      master is 4.0 then the events are in the slave's format (conversion).
    */
    set_slave_thread_options(thd);
    set_slave_thread_default_charset(thd, rli);
    thd->variables.sql_mode= global_system_variables.sql_mode;
    thd->variables.auto_increment_increment=
      thd->variables.auto_increment_offset= 1;
    /*
      Rotate_log_events are generated on Slaves with server_id=0
      for all the ignored events, so that the positions in the repository
      is updated properly even for ignored events.

      This kind of Rotate_log_event is generated when

        1) the event is generated on the same host and reached due
           to circular replication (server_id == ::server_id)

        2) the event is from the host which is listed in ignore_server_ids

        3) IO thread is receiving HEARTBEAT event from the master

        4) IO thread is receiving PREVIOUS_GTID_LOG_EVENT from the master.

      We have to free thd's mem_root here after we update the positions
      in the repository table. Otherwise, imagine a situation where
      Slave is keep getting ignored events only and no other (non-ignored)
      events from the Master, Slave never executes free_root (that generally
      happens from Query_log_event::do_apply_event or
      Rows_log_event::do_apply_event when they find end of the group event).
    */
    if (server_id == 0)
      free_root(thd->mem_root, MYF(MY_KEEP_PREALLOC));
  }
  else
    rli->inc_event_relay_log_pos();

err:
  DBUG_RETURN(error);
}


Log_event::enum_skip_reason
Rotate_log_event::do_shall_skip(Relay_log_info *rli)
{
  enum_skip_reason reason= Log_event::do_shall_skip(rli);

  switch (reason) {
  case Log_event::EVENT_SKIP_NOT:
  case Log_event::EVENT_SKIP_COUNT:
    return Log_event::EVENT_SKIP_NOT;

  case Log_event::EVENT_SKIP_IGNORE:
    return Log_event::EVENT_SKIP_IGNORE;
  }
  DBUG_ASSERT(0);
  return Log_event::EVENT_SKIP_NOT;             // To keep compiler happy
}

#endif


/**************************************************************************
	Intvar_log_event methods
**************************************************************************/

/*
  Intvar_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Intvar_log_event::pack_info(Protocol *protocol)
{
  char buf[256], *pos;
  pos= strmake(buf, (get_var_type_string()).c_str(), sizeof(buf)-23);
  *pos++= '=';
  pos= longlong10_to_str(val, pos, -10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
  return 0;
}
#endif


/*
  Intvar_log_event::Intvar_log_event()
*/
Intvar_log_event::Intvar_log_event(const char* buf,
                                   const Format_description_event*
                                   description_event)
: binary_log::Intvar_event(buf, description_event),
  Log_event(header(), footer())
{
  is_valid_param= true;
}

/*
  Intvar_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Intvar_log_event::write(IO_CACHE* file)
{
  uchar buf[9];
  buf[I_TYPE_OFFSET]= (uchar) type;
  int8store(buf + I_VAL_OFFSET, val);
  return (write_header(file, sizeof(buf)) ||
          wrapper_my_b_safe_write(file, buf, sizeof(buf)) ||
          write_footer(file));
}
#endif


/*
  Intvar_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Intvar_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char llbuff[22];
  const char *msg= NULL;
  IO_CACHE *const head= &print_event_info->head_cache;

  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tIntvar\n");
  }

  my_b_printf(head, "SET ");
  switch (type) {
  case LAST_INSERT_ID_EVENT:
    msg="LAST_INSERT_ID";
    break;
  case INSERT_ID_EVENT:
    msg="INSERT_ID";
    break;
  case INVALID_INT_EVENT:
  default: // cannot happen
    msg="INVALID_INT";
    break;
  }
  my_b_printf(head, "%s=%s%s\n",
              msg, llstr(val,llbuff), print_event_info->delimiter);
}
#endif


#if defined(HAVE_REPLICATION)&& !defined(MYSQL_CLIENT)

/*
  Intvar_log_event::do_apply_event()
*/

int Intvar_log_event::do_apply_event(Relay_log_info const *rli)
{
  /*
    We are now in a statement until the associated query log event has
    been processed.
   */
  const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

  if (rli->deferred_events_collecting)
    return rli->deferred_events->add(this);

  switch (type) {
  case LAST_INSERT_ID_EVENT:
    thd->first_successful_insert_id_in_prev_stmt= val;
    thd->substitute_null_with_insert_id= TRUE;
    break;
  case INSERT_ID_EVENT:
    thd->force_one_auto_inc_interval(val);
    break;
  }
  return 0;
}

int Intvar_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}


Log_event::enum_skip_reason
Intvar_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    It is a common error to set the slave skip counter to 1 instead of
    2 when recovering from an insert which used a auto increment,
    rand, or user var.  Therefore, if the slave skip counter is 1, we
    just say that this event should be skipped by ignoring it, meaning
    that we do not change the value of the slave skip counter since it
    will be decreased by the following insert event.
  */
  return continue_group(rli);
}

#endif


/**************************************************************************
  Rand_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rand_log_event::pack_info(Protocol *protocol)
{
  char buf1[256], *pos;
  pos= my_stpcpy(buf1,"rand_seed1=");
  pos= int10_to_str((long) seed1, pos, 10);
  pos= my_stpcpy(pos, ",rand_seed2=");
  pos= int10_to_str((long) seed2, pos, 10);
  protocol->store(buf1, (uint) (pos-buf1), &my_charset_bin);
  return 0;
}
#endif


Rand_log_event::Rand_log_event(const char* buf,
                               const Format_description_event* description_event)
  : binary_log::Rand_event(buf, description_event),
    Log_event(header(), footer())
{
  is_valid_param= true;
}


#ifndef MYSQL_CLIENT
bool Rand_log_event::write(IO_CACHE* file)
{
  uchar buf[16];
  int8store(buf + RAND_SEED1_OFFSET, seed1);
  int8store(buf + RAND_SEED2_OFFSET, seed2);
  return (write_header(file, sizeof(buf)) ||
          wrapper_my_b_safe_write(file, buf, sizeof(buf)) ||
	  write_footer(file));
}
#endif


#ifdef MYSQL_CLIENT
void Rand_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;

  char llbuff[22],llbuff2[22];
  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tRand\n");
  }
  my_b_printf(head, "SET @@RAND_SEED1=%s, @@RAND_SEED2=%s%s\n",
              llstr(seed1, llbuff),llstr(seed2, llbuff2),
              print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rand_log_event::do_apply_event(Relay_log_info const *rli)
{
  /*
    We are now in a statement until the associated query log event has
    been processed.
   */
  const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

  if (rli->deferred_events_collecting)
    return rli->deferred_events->add(this);

  thd->rand.seed1= (ulong) seed1;
  thd->rand.seed2= (ulong) seed2;
  return 0;
}

int Rand_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}


Log_event::enum_skip_reason
Rand_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    It is a common error to set the slave skip counter to 1 instead of
    2 when recovering from an insert which used a auto increment,
    rand, or user var.  Therefore, if the slave skip counter is 1, we
    just say that this event should be skipped by ignoring it, meaning
    that we do not change the value of the slave skip counter since it
    will be decreased by the following insert event.
  */
  return continue_group(rli);
}

/**
   Exec deferred Int-, Rand- and User- var events prefixing
   a Query-log-event event.

   @param thd THD handle

   @return false on success, true if a failure in an event applying occurred.
*/
bool slave_execute_deferred_events(THD *thd)
{
  bool res= false;
  Relay_log_info *rli= thd->rli_slave;

  DBUG_ASSERT(rli && (!rli->deferred_events_collecting || rli->deferred_events));

  if (!rli->deferred_events_collecting || rli->deferred_events->is_empty())
    return res;

  res= rli->deferred_events->execute(rli);
  rli->deferred_events->rewind();
  return res;
}

#endif /* !MYSQL_CLIENT */


/**************************************************************************
  Xid_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Xid_log_event::pack_info(Protocol *protocol)
{
  char buf[128], *pos;
  pos= my_stpcpy(buf, "COMMIT /* xid=");
  pos= longlong10_to_str(xid, pos, 10);
  pos= my_stpcpy(pos, " */");
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
  return 0;
}
#endif

Xid_log_event::
Xid_log_event(const char* buf,
              const Format_description_event *description_event)
  : binary_log::Xid_event(buf, description_event),
    Xid_apply_log_event(buf, description_event, header(), footer())
{
  is_valid_param= true;
}

#ifndef MYSQL_CLIENT
bool Xid_log_event::write(IO_CACHE* file)
{
  DBUG_EXECUTE_IF("do_not_write_xid", return 0;);
  return (write_header(file, sizeof(xid)) ||
	  wrapper_my_b_safe_write(file, (uchar*) &xid, sizeof(xid)) ||
	  write_footer(file));
}
#endif


#ifdef MYSQL_CLIENT
void Xid_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;

  if (!print_event_info->short_form)
  {
    char buf[64];
    longlong10_to_str(xid, buf, 10);

    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tXid = %s\n", buf);
  }
  my_b_printf(head, "COMMIT%s\n", print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
/**
   The methods combines few commit actions to make it useable
   as in the single- so multi- threaded case.

   @param  thd_arg a pointer to THD handle
   @return false  as success and
           true   as an error 
*/

bool Xid_log_event::do_commit(THD *thd_arg)
{
  DBUG_EXECUTE_IF("dbug.reached_commit",
                  {DBUG_SET("+d,dbug.enabled_commit");});
  bool error= trans_commit(thd_arg); /* Automatically rolls back on error. */
  DBUG_EXECUTE_IF("crash_after_apply", 
                  sql_print_information("Crashing crash_after_apply.");
                  DBUG_SUICIDE(););
  thd_arg->mdl_context.release_transactional_locks();

  error |= mysql_bin_log.gtid_end_transaction(thd_arg);

  /*
    The parser executing a SQLCOM_COMMIT or SQLCOM_ROLLBACK will reset the
    tx isolation level and access mode when the statement is finishing a
    transaction.

    For replicated workload, when dealing with pure transactional workloads,
    there will be no QUERY(COMMIT) finishing a transaction, but a
    Xid_log_event instead.

    So, if the slave applier changed the current transaction isolation level,
    it needs to be restored to the session default value once the current
    transaction has been committed.
  */
  trans_reset_one_shot_chistics(thd);

  /*
    Increment the global status commit count variable
  */
  if (!error)
    thd_arg->status_var.com_stat[SQLCOM_COMMIT]++;

  return error;
}

/**
   Worker commits Xid transaction and in case of its transactional
   info table marks the current group as done in the Coordnator's
   Group Assigned Queue.

   @return zero as success or non-zero as an error
*/
int Xid_apply_log_event::do_apply_event_worker(Slave_worker *w)
{
  int error= 0;
  bool skipped_commit_pos= true;

  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  Slave_committed_queue *coordinator_gaq= w->c_rli->gaq;

  /* For a slave Xid_log_event is COMMIT */
  query_logger.general_log_print(thd, COM_QUERY,
                                 "COMMIT /* implicit, from Xid_log_event */");

  DBUG_PRINT("mts", ("do_apply group master %s %llu  group relay %s %llu event %s %llu.",
                     w->get_group_master_log_name(),
                     w->get_group_master_log_pos(),
                     w->get_group_relay_log_name(),
                     w->get_group_relay_log_pos(),
                     w->get_event_relay_log_name(),
                     w->get_event_relay_log_pos()));

  DBUG_EXECUTE_IF("crash_before_update_pos",
                  sql_print_information("Crashing crash_before_update_pos.");
                  DBUG_SUICIDE(););

  ulong gaq_idx= mts_group_idx;
  Slave_job_group *ptr_group= coordinator_gaq->get_job_group(gaq_idx);

  if (!thd->get_transaction()->xid_state()->check_in_xa(false) &&
      w->is_transactional())
  {
    /*
      Regular (not XA) transaction updates the transactional info table
      along with the main transaction. Otherwise, the local flag turned
      and given its value the info table is updated after do_commit.
      todo: the flag won't be need upon the full xa crash-safety bug76233
            gets fixed.
    */
    skipped_commit_pos= false;
    if ((error= w->commit_positions(this, ptr_group,
                                    w->is_transactional())))
      goto err;
  }

  DBUG_PRINT("mts", ("do_apply group master %s %llu  group relay %s %llu event %s %llu.",
                     w->get_group_master_log_name(),
                     w->get_group_master_log_pos(),
                     w->get_group_relay_log_name(),
                     w->get_group_relay_log_pos(),
                     w->get_event_relay_log_name(),
                     w->get_event_relay_log_pos()));

  DBUG_EXECUTE_IF("crash_after_update_pos_before_apply",
                  sql_print_information("Crashing crash_after_update_pos_before_apply.");
                  DBUG_SUICIDE(););

  error= do_commit(thd);
  if (error)
  {
    if (!skipped_commit_pos)
      w->rollback_positions(ptr_group);
  }
  else if (skipped_commit_pos)
    error= w->commit_positions(this, ptr_group,
                               w->is_transactional());
err:
  return error;
}

int Xid_apply_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Xid_log_event::do_apply_event");
  int error= 0;
  char saved_group_master_log_name[FN_REFLEN];
  char saved_group_relay_log_name[FN_REFLEN];
  volatile my_off_t saved_group_master_log_pos;
  volatile my_off_t saved_group_relay_log_pos;

  char new_group_master_log_name[FN_REFLEN];
  char new_group_relay_log_name[FN_REFLEN];
  volatile my_off_t new_group_master_log_pos;
  volatile my_off_t new_group_relay_log_pos;

  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  /*
    Anonymous GTID ownership may be released here if the last
    statement before XID updated a non-transactional table and was
    written to the binary log as a separate transaction (either
    because binlog_format=row or because
    binlog_direct_non_transactional_updates=1).  So we need to
    re-acquire anonymous ownership.
  */
  gtid_reacquire_ownership_if_anonymous(thd);
  Relay_log_info *rli_ptr= const_cast<Relay_log_info *>(rli);

  /* For a slave Xid_log_event is COMMIT */
  query_logger.general_log_print(thd, COM_QUERY,
                                 "COMMIT /* implicit, from Xid_log_event */");

  mysql_mutex_lock(&rli_ptr->data_lock);

  /*
    Save the rli positions. We need them to temporarily reset the positions
    just before the commit.
   */
  strmake(saved_group_master_log_name, rli_ptr->get_group_master_log_name(),
          FN_REFLEN - 1);
  saved_group_master_log_pos= rli_ptr->get_group_master_log_pos();
  strmake(saved_group_relay_log_name, rli_ptr->get_group_relay_log_name(),
          FN_REFLEN - 1);
  saved_group_relay_log_pos= rli_ptr->get_group_relay_log_pos();

  DBUG_PRINT("info", ("do_apply group master %s %llu  group relay %s %llu event %s %llu\n",
    rli_ptr->get_group_master_log_name(),
    rli_ptr->get_group_master_log_pos(),
    rli_ptr->get_group_relay_log_name(),
    rli_ptr->get_group_relay_log_pos(),
    rli_ptr->get_event_relay_log_name(),
    rli_ptr->get_event_relay_log_pos()));

  DBUG_EXECUTE_IF("crash_before_update_pos",
                  sql_print_information("Crashing crash_before_update_pos.");
                  DBUG_SUICIDE(););

  /*
    We need to update the positions in here to make it transactional.  
  */
  rli_ptr->inc_event_relay_log_pos();
  rli_ptr->set_group_relay_log_pos(rli_ptr->get_event_relay_log_pos());
  rli_ptr->set_group_relay_log_name(rli_ptr->get_event_relay_log_name());

  rli_ptr->notify_group_relay_log_name_update();

  if (common_header->log_pos) // 3.23 binlogs don't have log_posx
    rli_ptr->set_group_master_log_pos(common_header->log_pos);

  /*
    rli repository being transactional means replication is crash safe.
    Positions are written into transactional tables ahead of commit and the
    changes are made permanent during commit.
    XA transactional does not actually commit so has to defer its flush_info().
   */
  if (!thd->get_transaction()->xid_state()->check_in_xa(false) &&
      rli_ptr->is_transactional())
  {
    if ((error= rli_ptr->flush_info(true)))
      goto err;
  }

  DBUG_PRINT("info", ("do_apply group master %s %llu  group relay %s %llu event %s %llu\n",
                      rli_ptr->get_group_master_log_name(),
                      rli_ptr->get_group_master_log_pos(),
                      rli_ptr->get_group_relay_log_name(),
                      rli_ptr->get_group_relay_log_pos(),
                      rli_ptr->get_event_relay_log_name(),
                      rli_ptr->get_event_relay_log_pos()));

  DBUG_EXECUTE_IF("crash_after_update_pos_before_apply",
                  sql_print_information("Crashing crash_after_update_pos_before_apply.");
                  DBUG_SUICIDE(););

  /**
    Commit operation expects the global transaction state variable 'xa_state'to
    be set to 'XA_NOTR'. In order to simulate commit failure we set
    the 'xa_state' to 'XA_IDLE' so that the commit reports 'ER_XAER_RMFAIL'
    error.
   */
  DBUG_EXECUTE_IF("simulate_commit_failure",
                  {
                    thd->get_transaction()->xid_state()->set_state(
                        XID_STATE::XA_IDLE);
                  });

  /*
    Save the new rli positions. These positions will be set back to group*
    positions on successful completion of the commit operation.
   */
  strmake(new_group_master_log_name, rli_ptr->get_group_master_log_name(),
          FN_REFLEN - 1);
  new_group_master_log_pos= rli_ptr->get_group_master_log_pos();
  strmake(new_group_relay_log_name, rli_ptr->get_group_relay_log_name(),
          FN_REFLEN - 1);
  new_group_relay_log_pos= rli_ptr->get_group_relay_log_pos();
  /*
    Rollback positions in memory just before commit. Position values will be
    reset to their new values only on successful commit operation.
   */
  rli_ptr->set_group_master_log_name(saved_group_master_log_name);
  rli_ptr->notify_group_master_log_name_update();
  rli_ptr->set_group_master_log_pos(saved_group_master_log_pos);
  rli_ptr->set_group_relay_log_name(saved_group_relay_log_name);
  rli_ptr->notify_group_relay_log_name_update();
  rli_ptr->set_group_relay_log_pos(saved_group_relay_log_pos);

  DBUG_PRINT("info", ("Rolling back to group master %s %llu  group relay %s"
                      " %llu\n", rli_ptr->get_group_master_log_name(),
                      rli_ptr->get_group_master_log_pos(),
                      rli_ptr->get_group_relay_log_name(),
                      rli_ptr->get_group_relay_log_pos()));
  mysql_mutex_unlock(&rli_ptr->data_lock);
  error= do_commit(thd);
  mysql_mutex_lock(&rli_ptr->data_lock);
  if (error)
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Xid_log_event: Commit could not be completed, '%s'",
                thd->get_stmt_da()->message_text());
  }
  else
  {
    DBUG_EXECUTE_IF("crash_after_commit_before_update_pos",
                    sql_print_information("Crashing "
                                          "crash_after_commit_before_update_pos.");
                    DBUG_SUICIDE(););
    /* Update positions on successful commit */
    rli_ptr->set_group_master_log_name(new_group_master_log_name);
    rli_ptr->notify_group_master_log_name_update();
    rli_ptr->set_group_master_log_pos(new_group_master_log_pos);
    rli_ptr->set_group_relay_log_name(new_group_relay_log_name);
    rli_ptr->notify_group_relay_log_name_update();
    rli_ptr->set_group_relay_log_pos(new_group_relay_log_pos);

    DBUG_PRINT("info", ("Updating positions on succesful commit to group master"
                        " %s %llu  group relay %s %llu\n",
                        rli_ptr->get_group_master_log_name(),
                        rli_ptr->get_group_master_log_pos(),
                        rli_ptr->get_group_relay_log_name(),
                        rli_ptr->get_group_relay_log_pos()));

    /*
      For transactional repository the positions are flushed ahead of commit.
      Where as for non transactional rli repository the positions are flushed
      only on succesful commit.
     */
    if (!rli_ptr->is_transactional())
      rli_ptr->flush_info(false);
  }
err:
  mysql_cond_broadcast(&rli_ptr->data_cond);
  mysql_mutex_unlock(&rli_ptr->data_lock);

  DBUG_RETURN(error);
}

Log_event::enum_skip_reason
Xid_apply_log_event::do_shall_skip(Relay_log_info *rli)
{
  DBUG_ENTER("Xid_log_event::do_shall_skip");
  if (rli->slave_skip_counter > 0) {
    thd->variables.option_bits&= ~OPTION_BEGIN;
    DBUG_RETURN(Log_event::EVENT_SKIP_COUNT);
  }
  DBUG_RETURN(Log_event::do_shall_skip(rli));
}
#endif /* !MYSQL_CLIENT */

/**************************************************************************
  XA_prepare_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int XA_prepare_log_event::pack_info(Protocol *protocol)
{
  char buf[ser_buf_size];
  char query[sizeof("XA COMMIT ONE PHASE") + 1 + sizeof(buf)];

  /* RHS of the following assert is unknown to client sources */
  compile_time_assert(ser_buf_size == XID::ser_buf_size);
  serialize_xid(buf, my_xid.formatID, my_xid.gtrid_length,
                my_xid.bqual_length, my_xid.data);
  sprintf(query,
          (one_phase ? "XA COMMIT %s ONE PHASE" :  "XA PREPARE %s"),
          buf);

  protocol->store(query, strlen(query), &my_charset_bin);
  return 0;
}
#endif


#ifndef MYSQL_CLIENT
bool XA_prepare_log_event::write(IO_CACHE* file)
{
  uint8 one_byte= one_phase;
  uchar buf_f[4];
  uchar buf_g[4];
  uchar buf_b[4];
  int4store(buf_f, static_cast<XID*>(xid)->get_format_id());
  int4store(buf_g, static_cast<XID*>(xid)->get_gtrid_length());
  int4store(buf_b, static_cast<XID*>(xid)->get_bqual_length());

  DBUG_ASSERT(xid_bufs_size == sizeof(buf_f) + sizeof(buf_g) + sizeof(buf_b));

  return write_header(file, sizeof(one_byte) + xid_bufs_size +
                      static_cast<XID*>(xid)->get_gtrid_length() +
                      static_cast<XID*>(xid)->get_bqual_length())
    ||
    wrapper_my_b_safe_write(file, &one_byte, sizeof(one_byte)) ||
    wrapper_my_b_safe_write(file, buf_f, sizeof(buf_f)) ||
    wrapper_my_b_safe_write(file, buf_g, sizeof(buf_g)) ||
    wrapper_my_b_safe_write(file, buf_b, sizeof(buf_b)) ||
    wrapper_my_b_safe_write(file, (uchar*) static_cast<XID*>(xid)->get_data(),
                            static_cast<XID*>(xid)->get_gtrid_length() +
                            static_cast<XID*>(xid)->get_bqual_length()) ||
    write_footer(file);
}
#endif


#ifdef MYSQL_CLIENT
void XA_prepare_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  char buf[ser_buf_size];

  print_header(head, print_event_info, FALSE);
  serialize_xid(buf, my_xid.formatID, my_xid.gtrid_length,
                        my_xid.bqual_length, my_xid.data);
  my_b_printf(head, "\tXA PREPARE %s\n", buf);
  my_b_printf(head, one_phase ? "XA COMMIT %s ONE PHASE\n%s\n" : "XA PREPARE %s\n%s\n",
              buf, print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/**
  Differs from Xid_log_event::do_commit in that it carries out
  XA prepare (not the commit).
  It also can commit on one phase when the event's member @c one_phase
  set to true.

  @param  thd    a pointer to THD handle
  @return false  as success and
          true   as an error
*/

bool XA_prepare_log_event::do_commit(THD *thd)
{
  bool error= false;
  xid_t xid;

  enum_gtid_statement_status state= gtid_pre_statement_checks(thd);
  if (state == GTID_STATEMENT_EXECUTE)
  {
    if (gtid_pre_statement_post_implicit_commit_checks(thd))
      state= GTID_STATEMENT_CANCEL;
  }
  if (state == GTID_STATEMENT_CANCEL)
  {
    uint error= thd->get_stmt_da()->mysql_errno();
    DBUG_ASSERT(error != 0);
    thd->rli_slave->report(ERROR_LEVEL, error,
                           "Error executing XA PREPARE event: '%s'",
                           thd->get_stmt_da()->message_text());
    thd->is_slave_error= 1;
    return true;
  }
  else if (state == GTID_STATEMENT_SKIP)
    return false;

  xid.set(my_xid.formatID,
          my_xid.data, my_xid.gtrid_length,
          my_xid.data + my_xid.gtrid_length, my_xid.bqual_length);
  if (!one_phase)
  {
    /*
      This is XA-prepare branch.
    */
    thd->lex->sql_command= SQLCOM_XA_PREPARE;
    thd->lex->m_sql_cmd= new Sql_cmd_xa_prepare(&xid);
    error= thd->lex->m_sql_cmd->execute(thd);
  }
  else
  {
    thd->lex->sql_command= SQLCOM_XA_COMMIT;
    thd->lex->m_sql_cmd= new Sql_cmd_xa_commit(&xid, XA_ONE_PHASE);
    error= thd->lex->m_sql_cmd->execute(thd);
  }
  error|= mysql_bin_log.gtid_end_transaction(thd);

  return error;
}
#endif


/**************************************************************************
  User_var_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int User_var_log_event::pack_info(Protocol* protocol)
{
  char *buf= 0;
  char quoted_id[1 + FN_REFLEN * 2 + 2];// quoted identifier
  size_t id_len= my_strmov_quoted_identifier(this->thd, quoted_id, name, name_len);
  quoted_id[id_len]= '\0';
  size_t val_offset= 2 + id_len;
  size_t event_len= val_offset;

  if (is_null)
  {
    if (!(buf= (char*) my_malloc(key_memory_log_event,
                                 val_offset + 5, MYF(MY_WME))))
      return 1;
    my_stpcpy(buf + val_offset, "NULL");
    event_len= val_offset + 4;
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      double real_val;
      float8get(&real_val, val);
      if (!(buf= (char*) my_malloc(key_memory_log_event,
                                   val_offset + MY_GCVT_MAX_FIELD_WIDTH + 1,
                                   MYF(MY_WME))))
        return 1;
      event_len+= my_gcvt(real_val, MY_GCVT_ARG_DOUBLE, MY_GCVT_MAX_FIELD_WIDTH,
                          buf + val_offset, NULL);
      break;
    case INT_RESULT:
      if (!(buf= (char*) my_malloc(key_memory_log_event,
                                   val_offset + 22, MYF(MY_WME))))
        return 1;
      event_len= longlong10_to_str(uint8korr(val), buf + val_offset, 
                                   ((flags & User_var_log_event::UNSIGNED_F) ? 
                                    10 : -10))-buf;
      break;
    case DECIMAL_RESULT:
    {
      if (!(buf= (char*) my_malloc(key_memory_log_event,
                                   val_offset + DECIMAL_MAX_STR_LENGTH + 1,
                                   MYF(MY_WME))))
        return 1;
      String str(buf+val_offset, DECIMAL_MAX_STR_LENGTH + 1, &my_charset_bin);
      my_decimal dec;
      binary2my_decimal(E_DEC_FATAL_ERROR, (uchar*) (val+2), &dec, val[0],
                        val[1]);
      my_decimal2string(E_DEC_FATAL_ERROR, &dec, 0, 0, 0, &str);
      event_len= str.length() + val_offset;
      break;
    } 
    case STRING_RESULT:
      /* 15 is for 'COLLATE' and other chars */
      buf= (char*) my_malloc(key_memory_log_event,
                             event_len+val_len*2+1+2*MY_CS_NAME_SIZE+15,
                             MYF(MY_WME));
      CHARSET_INFO *cs;
      if (!buf)
        return 1;
      if (!(cs= get_charset(charset_number, MYF(0))))
      {
        my_stpcpy(buf+val_offset, "???");
        event_len+= 3;
      }
      else
      {
        char *p= strxmov(buf + val_offset, "_", cs->csname, " ", NullS);
        p= str_to_hex(p, val, val_len);
        p= strxmov(p, " COLLATE ", cs->name, NullS);
        event_len= p-buf;
      }
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return 1;
    }
  }
  buf[0]= '@';
  memcpy(buf + 1, quoted_id, id_len);
  buf[1 + id_len]= '=';
  protocol->store(buf, event_len, &my_charset_bin);
  my_free(buf);
  return 0;
}
#endif /* !MYSQL_CLIENT */


User_var_log_event::
	User_var_log_event(const char* buf, uint event_len,
			   const Format_description_event* description_event)
  : binary_log::User_var_event(buf, event_len, description_event),
    Log_event(header(), footer())
#ifndef MYSQL_CLIENT
    ,deferred(false), query_id(0)
#endif
{
  if (name != 0)
    is_valid_param= true;
}


#ifndef MYSQL_CLIENT
bool User_var_log_event::write(IO_CACHE* file)
{
  char buf[UV_NAME_LEN_SIZE];
  char buf1[UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE + 
	    UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE];
  uchar buf2[MY_MAX(8, DECIMAL_MAX_FIELD_SIZE + 2)], *pos= buf2;
  uint unsigned_len= 0;
  uint buf1_length;
  ulong event_length;

  int4store(buf, name_len);
  
  if ((buf1[0]= is_null))
  {
    buf1_length= 1;
    val_len= 0;                                 // Length of 'pos'
  }    
  else
  {
    buf1[1]= type;
    int4store(buf1 + 2, charset_number);

    switch (type) {
    case REAL_RESULT:
      float8store(buf2, *(double*) val);
      break;
    case INT_RESULT:
      int8store(buf2, *(longlong*) val);
      unsigned_len= 1;
      break;
    case DECIMAL_RESULT:
    {
      my_decimal *dec= (my_decimal *)val;
      dec->sanity_check();
      buf2[0]= (char)(dec->intg + dec->frac);
      buf2[1]= (char)dec->frac;
      decimal2bin(dec, buf2+2, buf2[0], buf2[1]);
      val_len= decimal_bin_size(buf2[0], buf2[1]) + 2;
      break;
    }
    case STRING_RESULT:
      pos= (uchar*) val;
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return 0;
    }
    int4store(buf1 + 2 + UV_CHARSET_NUMBER_SIZE, val_len);
    buf1_length= 10;
  }

  /* Length of the whole event */
  event_length= sizeof(buf)+ name_len + buf1_length + val_len + unsigned_len;

  return (write_header(file, event_length) ||
          wrapper_my_b_safe_write(file, (uchar*) buf, sizeof(buf))   ||
	  wrapper_my_b_safe_write(file, (uchar*) name, name_len)     ||
	  wrapper_my_b_safe_write(file, (uchar*) buf1, buf1_length) ||
	  wrapper_my_b_safe_write(file, pos, val_len) ||
          wrapper_my_b_safe_write(file, &flags, unsigned_len) ||
	  write_footer(file));
}
#endif


/*
  User_var_log_event::print()
*/

#ifdef MYSQL_CLIENT
void User_var_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  char quoted_id[1 + NAME_LEN * 2 + 2];// quoted length of the identifier
  char name_id[NAME_LEN];
  size_t quoted_len= 0;

  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tUser_var\n");
  }
  my_stpcpy(name_id, name);
  name_id[name_len]= '\0';
  my_b_printf(head, "SET @");
  quoted_len= my_strmov_quoted_identifier((char *) quoted_id,
                                          (const char *) name_id);
  quoted_id[quoted_len]= '\0';
  my_b_write(head, (uchar*) quoted_id, quoted_len);

  if (is_null)
  {
    my_b_printf(head, ":=NULL%s\n", print_event_info->delimiter);
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      double real_val;
      char real_buf[FMT_G_BUFSIZE(14)];
      float8get(&real_val, val);
      sprintf(real_buf, "%.14g", real_val);
      my_b_printf(head, ":=%s%s\n", real_buf, print_event_info->delimiter);
      break;
    case INT_RESULT:
      char int_buf[22];
      longlong10_to_str(uint8korr(val), int_buf, 
                        ((flags & User_var_log_event::UNSIGNED_F) ? 10 : -10));
      my_b_printf(head, ":=%s%s\n", int_buf, print_event_info->delimiter);
      break;
    case DECIMAL_RESULT:
    {
      char str_buf[200];
      int str_len= sizeof(str_buf) - 1;
      int precision= (int)val[0];
      int scale= (int)val[1];
      decimal_digit_t dec_buf[10];
      decimal_t dec;
      dec.len= 10;
      dec.buf= dec_buf;

      bin2decimal((uchar*) val+2, &dec, precision, scale);
      decimal2string(&dec, str_buf, &str_len, 0, 0, 0);
      str_buf[str_len]= 0;
      my_b_printf(head, ":=%s%s\n", str_buf, print_event_info->delimiter);
      break;
    }
    case STRING_RESULT:
    {
      /*
        Let's express the string in hex. That's the most robust way. If we
        print it in character form instead, we need to escape it with
        character_set_client which we don't know (we will know it in 5.0, but
        in 4.1 we don't know it easily when we are printing
        User_var_log_event). Explanation why we would need to bother with
        character_set_client (quoting Bar):
        > Note, the parser doesn't switch to another unescaping mode after
        > it has met a character set introducer.
        > For example, if an SJIS client says something like:
        > SET @a= _ucs2 \0a\0b'
        > the string constant is still unescaped according to SJIS, not
        > according to UCS2.
      */
      char *hex_str;
      CHARSET_INFO *cs;

      hex_str= (char *)my_malloc(key_memory_log_event,
                                 2*val_len+1+2,MYF(MY_WME)); // 2 hex digits / byte
      if (!hex_str)
        return;
      str_to_hex(hex_str, val, val_len);
      /*
        For proper behaviour when mysqlbinlog|mysql, we need to explicitely
        specify the variable's collation. It will however cause problems when
        people want to mysqlbinlog|mysql into another server not supporting the
        character set. But there's not much to do about this and it's unlikely.
      */
      if (!(cs= get_charset(charset_number, MYF(0))))
        /*
          Generate an unusable command (=> syntax error) is probably the best
          thing we can do here.
        */
        my_b_printf(head, ":=???%s\n", print_event_info->delimiter);
      else
        my_b_printf(head, ":=_%s %s COLLATE `%s`%s\n",
                    cs->csname, hex_str, cs->name,
                    print_event_info->delimiter);
      my_free(hex_str);
    }
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return;
    }
  }
}
#endif


/*
  User_var_log_event::do_apply_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int User_var_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("User_var_log_event::do_apply_event");
  Item *it= 0;
  CHARSET_INFO *charset;
  query_id_t sav_query_id= 0; /* memorize orig id when deferred applying */

  if (rli->deferred_events_collecting)
  {
    set_deferred(current_thd->query_id);
    int ret= rli->deferred_events->add(this);
    DBUG_RETURN(ret);
  }
  else if (is_deferred())
  {
    sav_query_id= current_thd->query_id;
    current_thd->query_id= query_id; /* recreating original time context */
  }

  if (!(charset= get_charset(charset_number, MYF(MY_WME))))
  {
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                "Invalid character set for User var event");
    DBUG_RETURN(1);
  }
  double real_val;
  longlong int_val;

  /*
    We are now in a statement until the associated query log event has
    been processed.
   */
  const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

  if (is_null)
  {
    it= new Item_null();
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      if (val_len != 8)
      {
        rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                    ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                    "Invalid variable length at User var event");
        DBUG_RETURN(1);
      }
      float8get(&real_val, val);
      it= new Item_float(real_val, 0);
      val= (char*) &real_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case INT_RESULT:
      if (val_len != 8)
      {
        rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                    ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                    "Invalid variable length at User var event");
        DBUG_RETURN(1);
      }
      int_val= (longlong) uint8korr(val);
      it= new Item_int(int_val);
      val= (char*) &int_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case DECIMAL_RESULT:
    {
      if (val_len < 3)
      {
        rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                    ER_THD(thd, ER_SLAVE_FATAL_ERROR),
                    "Invalid variable length at User var event");
        DBUG_RETURN(1);
      }
      Item_decimal *dec= new Item_decimal((uchar*) val+2, val[0], val[1]);
      it= dec;
      val= (char *)dec->val_decimal(NULL);
      val_len= sizeof(my_decimal);
      break;
    }
    case STRING_RESULT:
      it= new Item_string(val, val_len, charset);
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      DBUG_RETURN(0);
    }
  }
  Item_func_set_user_var *e=
    new Item_func_set_user_var(Name_string(name, name_len, false), it, false);
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)

    Fix_fields() can fail, in which case a call of update_hash() might
    crash the server, so if fix fields fails, we just return with an
    error.
  */
  if (e->fix_fields(thd, 0))
    DBUG_RETURN(1);

  /*
    A variable can just be considered as a table with
    a single record and with a single column. Thus, like
    a column value, it could always have IMPLICIT derivation.
   */
  e->update_hash(val, val_len, (Item_result)type, charset, DERIVATION_IMPLICIT,
                 (flags & binary_log::User_var_event::UNSIGNED_F));
  if (!is_deferred())
    free_root(thd->mem_root, 0);
  else
    current_thd->query_id= sav_query_id; /* restore current query's context */

  DBUG_RETURN(0);
}

int User_var_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}

Log_event::enum_skip_reason
User_var_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    It is a common error to set the slave skip counter to 1 instead
    of 2 when recovering from an insert which used a auto increment,
    rand, or user var.  Therefore, if the slave skip counter is 1, we
    just say that this event should be skipped by ignoring it, meaning
    that we do not change the value of the slave skip counter since it
    will be decreased by the following insert event.
  */
  return continue_group(rli);
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
  Unknown_log_event methods
**************************************************************************/

#ifdef HAVE_REPLICATION
#ifdef MYSQL_CLIENT
void Unknown_log_event::print(FILE* file_arg, PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache, "\n# %s", "Unknown event\n");
}
#endif  

/**************************************************************************
	Stop_log_event methods
**************************************************************************/

/*
  Stop_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Stop_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;

  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache, "\tStop\n");
}
#endif /* MYSQL_CLIENT */


#ifndef MYSQL_CLIENT
/*
  The master stopped.  We used to clean up all temporary tables but
  this is useless as, as the master has shut down properly, it has
  written all DROP TEMPORARY TABLE (prepared statements' deletion is
  TODO only when we binlog prep stmts).  We used to clean up
  slave_load_tmpdir, but this is useless as it has been cleared at the
  end of LOAD DATA INFILE.  So we have nothing to do here.  The place
  were we must do this cleaning is in
  Start_log_event_v3::do_apply_event(), not here. Because if we come
  here, the master was sane.

  This must only be called from the Slave SQL thread, since it calls
  flush_relay_log_info().
*/
int Stop_log_event::do_update_pos(Relay_log_info *rli)
{
  int error_inc= 0;
  int error_flush= 0;
  /*
    We do not want to update master_log pos because we get a rotate event
    before stop, so by now group_master_log_name is set to the next log.
    If we updated it, we will have incorrect master coordinates and this
    could give false triggers in MASTER_POS_WAIT() that we have reached
    the target position when in fact we have not.
    The group position is always unchanged in MTS mode because the event
    is never executed so can't be scheduled to a Worker.
  */
  if ((thd->variables.option_bits & OPTION_BEGIN) || rli->is_parallel_exec())
    rli->inc_event_relay_log_pos();
  else
  {
    error_inc= rli->inc_group_relay_log_pos(0, true/*need_data_lock=true*/);
    error_flush= rli->flush_info(TRUE);
  }
  return (error_inc || error_flush);
}

#endif /* !MYSQL_CLIENT */
#endif /* HAVE_REPLICATION */


/**************************************************************************
	Create_file_log_event methods
**************************************************************************/

#ifndef MYSQL_CLIENT
/*
  Create_file_log_event::write_data_body()
*/

bool Create_file_log_event::write_data_body(IO_CACHE* file)
{
  bool res;
  if ((res= Load_log_event::write_data_body(file)) || fake_base)
    return res;
  return (my_b_safe_write(file, (uchar*) "", 1) ||
          my_b_safe_write(file, block, block_len));
}


/*
  Create_file_log_event::write_data_header()
*/

bool Create_file_log_event::write_data_header(IO_CACHE* file)
{
  bool res;
  uchar buf[Binary_log_event::CREATE_FILE_HEADER_LEN];
  if ((res= Load_log_event::write_data_header(file)) || fake_base)
    return res;
  int4store(buf + CF_FILE_ID_OFFSET, file_id);
  return my_b_safe_write(file, buf, Binary_log_event::CREATE_FILE_HEADER_LEN) != 0;
}


/*
  Create_file_log_event::write_base()
*/

bool Create_file_log_event::write_base(IO_CACHE* file)
{
  bool res;
  fake_base= 1;                                 // pretend we are Load event
  common_header->type_code= Load_log_event::get_type_code();
  DBUG_EXECUTE_IF("simulate_cache_write_failure",
                  {
                  res= TRUE;
                  my_error(ER_UNKNOWN_ERROR, MYF(0));
                  return res;
                  });
  res= write(file);
  fake_base= 0;
  common_header->type_code= binary_log::CREATE_FILE_EVENT;
  return res;
}

#endif /* !MYSQL_CLIENT */

/*
  Create_file_log_event ctor
*/

Create_file_log_event::
Create_file_log_event(const char* buf, uint len,
                      const Format_description_event* description_event)
 : binary_log::Load_event(buf, 0, description_event),
  Load_log_event(buf,0,description_event),
  binary_log::Create_file_event(buf, len, description_event)
{
  DBUG_ENTER("Create_file_log_event::Create_file_log_event(char*,...)");
  /**
    We need to set exec_time here, which is ued to calcutate seconds behind
    master on the slave.
  */
  exec_time= load_exec_time;
  sql_ex.data_info= sql_ex_data;
  if (inited_from_old || block != 0)
    is_valid_param= true;
  if (fake_base)
    common_header->type_code= Load_log_event::get_type_code();
  else
    common_header->type_code= binary_log::CREATE_FILE_EVENT;
  DBUG_VOID_RETURN;
}


/*
  Create_file_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Create_file_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info,
				  bool enable_local)
{
  if (print_event_info->short_form)
  {
    if (enable_local && check_fname_outside_temp_buf())
      Load_log_event::print(file, print_event_info);
    return;
  }

  if (enable_local)
  {
    Load_log_event::print(file, print_event_info,
			  !check_fname_outside_temp_buf());
    /**
      reduce the size of io cache so that the write function is called
      for every call to my_b_printf().
     */
    DBUG_EXECUTE_IF ("simulate_create_event_write_error",
                     {(&print_event_info->head_cache)->write_pos=
                     (&print_event_info->head_cache)->write_end;
                     DBUG_SET("+d,simulate_file_write_error");});
    /* 
       That one is for "file_id: etc" below: in mysqlbinlog we want the #, in
       SHOW BINLOG EVENTS we don't.
    */
    my_b_printf(&print_event_info->head_cache, "#");
  }

  my_b_printf(&print_event_info->head_cache,
              " file_id: %d  block_len: %d\n", file_id, block_len);
}


void Create_file_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}
#endif /* MYSQL_CLIENT */


/*
  Create_file_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Create_file_log_event::pack_info(Protocol *protocol)
{
  char buf[NAME_LEN*2 + 30 + 21*2], *pos;
  pos= my_stpcpy(buf, "db=");
  memcpy(pos, db, db_len);
  pos= my_stpcpy(pos + db_len, ";table=");
  memcpy(pos, table_name, table_name_len);
  pos= my_stpcpy(pos + table_name_len, ";file_id=");
  pos= int10_to_str((long) file_id, pos, 10);
  pos= my_stpcpy(pos, ";block_len=");
  pos= int10_to_str((long) block_len, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
  return 0;
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**
  Create_file_log_event::do_apply_event()
  Constructor for Create_file_log_event to intantiate an event
  from the relay log on the slave.

  @retval
    0           Success
  @retval
    1           Failure
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Create_file_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname_buf[FN_REFLEN+TEMP_FILE_MAX_LEN];
  char *ext;
  int fd = -1;
  IO_CACHE file;
  int error = 1;

  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  THD_STAGE_INFO(thd, stage_making_temp_file_create_before_load_data);
  memset(&file, 0, sizeof(file));
  ext= slave_load_file_stem(fname_buf, file_id, server_id, ".info");
  /* old copy may exist already */
  mysql_file_delete(key_file_log_event_info, fname_buf, MYF(0));
  /**
    To simulate file creation failure, convert the file name to a
    directory by appending a "/" to the file name.
   */
  DBUG_EXECUTE_IF("simulate_file_create_error_create_log_event",
                  {
                  strcat(fname_buf,"/");
                  });
  if ((fd= mysql_file_create(key_file_log_event_info,
                             fname_buf, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, WRITE_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Create_file event: could not open file '%s', '%s'",
                fname_buf, thd->get_stmt_da()->message_text());
    goto err;
  }
  
  // a trick to avoid allocating another buffer
  fname= fname_buf;
  fname_len= (uint) (my_stpcpy(ext, ".data") - fname);
  if (write_base(&file))
  {
    my_stpcpy(ext, ".info"); // to have it right in the error message
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Create_file event: could not write to file '%s', '%s'",
                fname_buf, thd->get_stmt_da()->message_text());
    goto err;
  }
  end_io_cache(&file);
  mysql_file_close(fd, MYF(0));
  
  // fname_buf now already has .data, not .info, because we did our trick
  /* old copy may exist already */
  mysql_file_delete(key_file_log_event_data, fname_buf, MYF(0));
  DBUG_EXECUTE_IF("simulate_file_create_error_create_log_event_2",
                  {
                  strcat(fname_buf, "/");
                  });
  if ((fd= mysql_file_create(key_file_log_event_data,
                             fname_buf, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Create_file event: could not open file '%s', '%s'",
                fname_buf, thd->get_stmt_da()->message_text());
    goto err;
  }
  /**
    To simulate file write failure,close the file before the write operation.
    Write will fail with an error reporting file is UNOPENED. 
   */
  DBUG_EXECUTE_IF("simulate_file_write_error_create_log_event",
                  {
                  mysql_file_close(fd, MYF(0));
                  });
  if (mysql_file_write(fd, block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Create_file event: write to '%s' failed, '%s'",
                fname_buf, thd->get_stmt_da()->message_text());
    goto err;
  }
  error=0;					// Everything is ok

err:
  if (error)
  {
    end_io_cache(&file);
    /*
      Error occured. Delete .info and .data files if they are created.
    */
    my_stpcpy(ext,".info");
    mysql_file_delete(key_file_log_event_info, fname_buf, MYF(0));
    my_stpcpy(ext,".data");
    mysql_file_delete(key_file_log_event_data, fname_buf, MYF(0));
  }
  if (fd >= 0)
    mysql_file_close(fd, MYF(0));
  return error != 0;
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Append_block_log_event methods
**************************************************************************/

/*
  Append_block_log_event ctor
*/

#ifndef MYSQL_CLIENT  
Append_block_log_event::Append_block_log_event(THD *thd_arg,
                                               const char *db_arg,
					       uchar *block_arg,
					       uint block_len_arg,
					       bool using_trans)
  : binary_log::Append_block_event(db_arg, block_arg, block_len_arg, thd_arg->file_id),
    Log_event(thd_arg, 0,
              using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                            Log_event::EVENT_STMT_CACHE,
              Log_event::EVENT_NORMAL_LOGGING,
              header(), footer())
{
  if (block != 0)
    is_valid_param= true;
}
#endif


/*
  Append_block_log_event ctor
*/

Append_block_log_event::Append_block_log_event(const char* buf, uint len,
                                               const Format_description_event*
                                               description_event)
  : binary_log::Append_block_event(buf, len, description_event),
    Log_event(header(), footer())
{

  DBUG_ENTER("Append_block_log_event::Append_block_log_event(char*,...)");
  if (block != 0)
    is_valid_param= true;
  DBUG_VOID_RETURN;
}


/*
  Append_block_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Append_block_log_event::write(IO_CACHE* file)
{
  uchar buf[Binary_log_event::APPEND_BLOCK_HEADER_LEN];
  int4store(buf + AB_FILE_ID_OFFSET, file_id);
  return (write_header(file, Binary_log_event::APPEND_BLOCK_HEADER_LEN +
                       block_len) ||
          wrapper_my_b_safe_write(file, buf,
                                  Binary_log_event::APPEND_BLOCK_HEADER_LEN) ||
	  wrapper_my_b_safe_write(file, block, block_len) ||
	  write_footer(file));
}
#endif


/*
  Append_block_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Append_block_log_event::print(FILE* file,
				   PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache,
              "\n#%s: file_id: %d  block_len: %d\n",
              get_type_str(), file_id, block_len);
}
#endif /* MYSQL_CLIENT */


/*
  Append_block_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Append_block_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  size_t length;
  length= my_snprintf(buf, sizeof(buf), ";file_id=%u;block_len=%u",
                      file_id, block_len);
  protocol->store(buf, length, &my_charset_bin);
  return 0;
}


/*
  Append_block_log_event::get_create_or_append()
*/

int Append_block_log_event::get_create_or_append() const
{
  return 0; /* append to the file, fail if not exists */
}

/*
  Append_block_log_event::do_apply_event()
*/

int Append_block_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+TEMP_FILE_MAX_LEN];
  int fd;
  int error = 1;
  DBUG_ENTER("Append_block_log_event::do_apply_event");

  THD_STAGE_INFO(thd, stage_making_temp_file_append_before_load_data);
  slave_load_file_stem(fname, file_id, server_id, ".data");
  if (get_create_or_append())
  {
    /*
      Usually lex_start() is called by mysql_parse(), but we need it here
      as the present method does not call mysql_parse().
    */
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);
    /* old copy may exist already */
    mysql_file_delete(key_file_log_event_data, fname, MYF(0));
    DBUG_EXECUTE_IF("simulate_file_create_error_Append_block_event",
                    {
                    strcat(fname, "/");
                    });
    if ((fd= mysql_file_create(key_file_log_event_data,
                               fname, CREATE_MODE,
                               O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                               MYF(MY_WME))) < 0)
    {
      rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                  "Error in %s event: could not create file '%s', '%s'",
                  get_type_str(), fname, thd->get_stmt_da()->message_text());
      goto err;
    }
  }
  else if ((fd= mysql_file_open(key_file_log_event_data,
                                fname,
                                O_WRONLY | O_APPEND | O_BINARY | O_NOFOLLOW,
                                MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in %s event: could not open file '%s', '%s'",
                get_type_str(), fname, thd->get_stmt_da()->message_text());
    goto err;
  }
  DBUG_EXECUTE_IF("remove_slave_load_file_before_write",
                  {
                    my_delete_allow_opened(fname, MYF(0));
                  });

  DBUG_EXECUTE_IF("simulate_file_write_error_Append_block_event",
                  {
                    mysql_file_close(fd, MYF(0));
                  });
  if (mysql_file_write(fd, block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in %s event: write to '%s' failed, '%s'",
                get_type_str(), fname, thd->get_stmt_da()->message_text());
    goto err;
  }
  error=0;

err:
  if (fd >= 0)
    mysql_file_close(fd, MYF(0));
  DBUG_RETURN(error);
}
#endif


/**************************************************************************
	Delete_file_log_event methods
**************************************************************************/

/*
  Delete_file_log_event ctor
*/

#ifndef MYSQL_CLIENT
Delete_file_log_event::Delete_file_log_event(THD *thd_arg, const char* db_arg,
					     bool using_trans)
  : binary_log::Delete_file_event(thd_arg->file_id, db_arg),
    Log_event(thd_arg, 0,
              using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                            Log_event::EVENT_STMT_CACHE,
              Log_event::EVENT_NORMAL_LOGGING,
              header(), footer())
{
  if (file_id != 0)
    is_valid_param= true;
}
#endif

/*
  Delete_file_log_event ctor
*/

Delete_file_log_event::Delete_file_log_event(const char* buf, uint len,
                                             const Format_description_event*
                                             description_event)
  : binary_log::Delete_file_event(buf, len, description_event),
    Log_event(header(), footer())
{
  if (file_id != 0)
    is_valid_param= true;
}


/*
  Delete_file_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Delete_file_log_event::write(IO_CACHE* file)
{
 uchar buf[Binary_log_event::DELETE_FILE_HEADER_LEN];
 int4store(buf + DF_FILE_ID_OFFSET, file_id);
 return (write_header(file, sizeof(buf)) ||
         wrapper_my_b_safe_write(file, buf, sizeof(buf)) ||
	 write_footer(file));
}
#endif


/*
  Delete_file_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Delete_file_log_event::print(FILE* file,
				  PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache,
              "\n#Delete_file: file_id=%u\n", file_id);
}
#endif /* MYSQL_CLIENT */

/*
  Delete_file_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Delete_file_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  size_t length;
  length= my_snprintf(buf, sizeof(buf), ";file_id=%u", (uint) file_id);
  protocol->store(buf, length, &my_charset_bin);
  return 0;
}
#endif

/*
  Delete_file_log_event::do_apply_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Delete_file_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+TEMP_FILE_MAX_LEN];
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  char *ext= slave_load_file_stem(fname, file_id, server_id, ".data");
  mysql_file_delete(key_file_log_event_data, fname, MYF(MY_WME));
  my_stpcpy(ext, ".info");
  mysql_file_delete(key_file_log_event_info, fname, MYF(MY_WME));
  return 0;
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Execute_load_log_event methods
**************************************************************************/

/*
  Execute_load_log_event ctor
*/

#ifndef MYSQL_CLIENT  
Execute_load_log_event::Execute_load_log_event(THD *thd_arg,
                                               const char* db_arg,
					       bool using_trans)
: binary_log::Execute_load_event(thd_arg->file_id, db_arg),
  Log_event(thd_arg, 0,
             using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                           Log_event::EVENT_STMT_CACHE,
             Log_event::EVENT_NORMAL_LOGGING,
             header(), footer())
{
  if (file_id != 0)
    is_valid_param= true;
}
#endif
  

/*
  Execute_load_log_event ctor
*/

Execute_load_log_event::Execute_load_log_event(const char* buf, uint len,
                                               const Format_description_event*
                                               description_event)
: binary_log::Execute_load_event(buf, len, description_event),
  Log_event(header(), footer())
{
  if (file_id != 0)
    is_valid_param= true;
}


/*
  Execute_load_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Execute_load_log_event::write(IO_CACHE* file)
{
  uchar buf[Binary_log_event::EXEC_LOAD_HEADER_LEN];
  int4store(buf + EL_FILE_ID_OFFSET, file_id);
  return (write_header(file, sizeof(buf)) || 
          wrapper_my_b_safe_write(file, buf, sizeof(buf)) ||
	  write_footer(file));
}
#endif


/*
  Execute_load_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Execute_load_log_event::print(FILE* file,
				   PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache, "\n#Exec_load: file_id=%d\n",
              file_id);
}
#endif

/*
  Execute_load_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Execute_load_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  size_t length;
  length= my_snprintf(buf, sizeof(buf), ";file_id=%u", (uint) file_id);
  protocol->store(buf, length, &my_charset_bin);
  return 0;
}


/*
  Execute_load_log_event::do_apply_event()
*/

int Execute_load_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+TEMP_FILE_MAX_LEN];
  char *ext;
  int fd;
  int error= 1;
  IO_CACHE file;
  Load_log_event *lev= 0;

  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  ext= slave_load_file_stem(fname, file_id, server_id, ".info");
  /**
    To simulate file open failure, convert the file name to a
    directory by appending a "/" to the file name. File open
    will fail with an error reporting it is not a directory.
   */
  DBUG_EXECUTE_IF("simulate_file_open_error_exec_event",
                  {
                  strcat(fname,"/");
                  });
  if ((fd= mysql_file_open(key_file_log_event_info,
                           fname, O_RDONLY | O_BINARY | O_NOFOLLOW,
                           MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, READ_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                "Error in Exec_load event: could not open file, '%s'",
                thd->get_stmt_da()->message_text());
    goto err;
  }
  if (!(lev= (Load_log_event*)
        Log_event::read_log_event(&file,
                                  (mysql_mutex_t*) 0,
                                  rli->get_rli_description_event(),
                                  opt_slave_sql_verify_checksum)) ||
      lev->get_type_code() != binary_log::NEW_LOAD_EVENT)
  {
    rli->report(ERROR_LEVEL, ER_FILE_CORRUPT, ER(ER_FILE_CORRUPT),
                fname);
    goto err;
  }
  lev->thd = thd;
  /*
    lev->do_apply_event should use rli only for errors.
    lev->do_apply_event is the place where the table is loaded (it
    calls mysql_load()).
  */
  if (lev->do_apply_event(0,rli,1))
  {
    /*
      We want to indicate the name of the file that could not be loaded
      (SQL_LOADxxx).
      But as we are here we are sure the error is in rli->last_slave_error and
      rli->last_slave_errno (example of error: duplicate entry for key), so we
      don't want to overwrite it with the filename.
      What we want instead is add the filename to the current error message.
    */
    char *tmp= my_strdup(key_memory_log_event,
                         rli->last_error().message, MYF(MY_WME));
    if (tmp)
    {
      rli->report(ERROR_LEVEL, rli->last_error().number,
                  "%s. Failed executing load from '%s'", tmp, fname);
      my_free(tmp);
    }
    goto err;
  }
  /*
    We have an open file descriptor to the .info file; we need to close it
    or Windows will refuse to delete the file in mysql_file_delete().
  */
  if (fd >= 0)
  {
    mysql_file_close(fd, MYF(0));
    end_io_cache(&file);
    fd= -1;
  }
  error = 0;

err:
  DBUG_EXECUTE_IF("simulate_file_open_error_exec_event",
                  {
                     my_stpcpy(ext, ".info");
                  });
  mysql_file_delete(key_file_log_event_info, fname, MYF(MY_WME));
  my_stpcpy(ext, ".data");
  mysql_file_delete(key_file_log_event_data, fname, MYF(MY_WME));
  delete lev;
  if (fd >= 0)
  {
    mysql_file_close(fd, MYF(0));
    end_io_cache(&file);
  }
  return error;
}

#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Begin_load_query_log_event methods
**************************************************************************/

#ifndef MYSQL_CLIENT
Begin_load_query_log_event::
Begin_load_query_log_event(THD* thd_arg, const char* db_arg, uchar* block_arg,
                           uint block_len_arg, bool using_trans)
 : binary_log::Append_block_event(db_arg, block_arg, block_len_arg,
                                  thd_arg->file_id),
   Append_block_log_event(thd_arg, db_arg, block_arg, block_len_arg,
                          using_trans),
   binary_log::Begin_load_query_event()
{
  common_header->type_code= binary_log::BEGIN_LOAD_QUERY_EVENT;
  file_id= thd_arg->file_id= mysql_bin_log.next_file_id();
}
#endif


Begin_load_query_log_event::
Begin_load_query_log_event(const char* buf, uint len,
                           const Format_description_event* desc_event)
  : binary_log::Append_block_event(buf, len, desc_event),
    Append_block_log_event(buf, len, desc_event),
    binary_log::Begin_load_query_event(buf, len, desc_event)
{
}


#if defined( HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Begin_load_query_log_event::get_create_or_append() const
{
  return 1; /* create the file */
}
#endif /* defined( HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
Log_event::enum_skip_reason
Begin_load_query_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1, then we should not start executing
    on the next event.
  */
  return continue_group(rli);
}
#endif


/**************************************************************************
	Execute_load_query_log_event methods
**************************************************************************/


#ifndef MYSQL_CLIENT
Execute_load_query_log_event::
Execute_load_query_log_event(THD *thd_arg, const char* query_arg,
                             ulong query_length_arg, uint fn_pos_start_arg,
                             uint fn_pos_end_arg,
                             binary_log::enum_load_dup_handling dup_handling_arg,
                             bool using_trans, bool immediate, bool suppress_use,
                             int errcode)
: binary_log::Query_event(query_arg, thd_arg->catalog().str, thd_arg->db().str,
                          query_length_arg,
                          thd_arg->thread_id(), thd_arg->variables.sql_mode,
                          thd_arg->variables.auto_increment_increment,
                          thd_arg->variables.auto_increment_offset,
                          thd_arg->variables.lc_time_names->number,
                          (ulonglong)thd_arg->table_map_for_update,
                          errcode,
                          thd_arg->db().str ? strlen(thd_arg->db().str) : 0,
                          thd_arg->catalog().str ? strlen(thd_arg->catalog().str) : 0),
  Query_log_event(thd_arg, query_arg, query_length_arg, using_trans, immediate,
                  suppress_use, errcode),
  binary_log::Execute_load_query_event(thd_arg->file_id, fn_pos_start_arg,
                           fn_pos_end_arg, dup_handling_arg)
{
  if (Query_log_event::is_valid() && file_id != 0)
    is_valid_param= true;
  common_header->type_code= binary_log::EXECUTE_LOAD_QUERY_EVENT;
}
#endif /* !MYSQL_CLIENT */


Execute_load_query_log_event::
Execute_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_event* desc_event)
  : binary_log::Query_event(buf, event_len, desc_event,
                            binary_log::EXECUTE_LOAD_QUERY_EVENT),
    Query_log_event(buf, event_len, desc_event,
                    binary_log::EXECUTE_LOAD_QUERY_EVENT),
    binary_log::Execute_load_query_event(buf, event_len, desc_event)
{
  if (!Query_log_event::is_valid())
  {
    //clear all the variables set in execute_load_query_event
    file_id= 0; fn_pos_start= 0; fn_pos_end= 0;
    dup_handling= binary_log::LOAD_DUP_ERROR;
  }
  if (Query_log_event::is_valid() && file_id != 0)
    is_valid_param= true;
}


ulong Execute_load_query_log_event::get_post_header_size_for_derived()
{
  return Binary_log_event::EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN;
}


#ifndef MYSQL_CLIENT
bool
Execute_load_query_log_event::write_post_header_for_derived(IO_CACHE* file)
{
  uchar buf[Binary_log_event::EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN];
  int4store(buf, file_id);
  int4store(buf + 4, fn_pos_start);
  int4store(buf + 4 + 4, fn_pos_end);
  *(buf + 4 + 4 + 4)= (uchar) dup_handling;
  return wrapper_my_b_safe_write(file, buf, Binary_log_event::EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN);
}
#endif


#ifdef MYSQL_CLIENT
void Execute_load_query_log_event::print(FILE* file,
                                         PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}

/**
  Prints the query as LOAD DATA LOCAL and with rewritten filename.
*/
void Execute_load_query_log_event::print(FILE* file,
                                         PRINT_EVENT_INFO* print_event_info,
                                         const char *local_fname)
{
  IO_CACHE *const head= &print_event_info->head_cache;

  print_query_header(head, print_event_info);
  /**
    reduce the size of io cache so that the write function is called
    for every call to my_b_printf().
   */
  DBUG_EXECUTE_IF ("simulate_execute_event_write_error",
                   {head->write_pos= head->write_end;
                   DBUG_SET("+d,simulate_file_write_error");});

  if (local_fname)
  {
    my_b_write(head, (uchar*) query, fn_pos_start);
    my_b_printf(head, " LOCAL INFILE ");
    pretty_print_str(head, local_fname, strlen(local_fname));

    if (dup_handling == binary_log::LOAD_DUP_REPLACE)
      my_b_printf(head, " REPLACE");
    my_b_printf(head, " INTO");
    my_b_write(head, (uchar*) query + fn_pos_end, q_len-fn_pos_end);
    my_b_printf(head, "\n%s\n", print_event_info->delimiter);
  }
  else
  {
    my_b_write(head, (uchar*) query, q_len);
    my_b_printf(head, "\n%s\n", print_event_info->delimiter);
  }

  if (!print_event_info->short_form)
    my_b_printf(head, "# file_id: %d \n", file_id);
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Execute_load_query_log_event::pack_info(Protocol *protocol)
{
  char *buf, *pos;
  if (!(buf= (char*) my_malloc(key_memory_log_event,
                               9 + (db_len * 2) + 2 + q_len + 10 + 21,
                               MYF(MY_WME))))
    return 1;
  pos= buf;
  if (db && db_len)
  {
    /*
      Statically allocates room to store '\0' and an identifier
      that may have NAME_LEN * 2 due to quoting and there are
      two quoting characters that wrap them.
    */
    char quoted_db[1 + NAME_LEN * 2 + 2];// quoted length of the identifier
    size_t size= 0;
    size= my_strmov_quoted_identifier(this->thd, quoted_db, db, 0);
    pos= my_stpcpy(buf, "use ");
    memcpy(pos, quoted_db, size);
    pos= my_stpcpy(pos + size, "; ");
  }
  if (query && q_len)
  {
    memcpy(pos, query, q_len);
    pos+= q_len;
  }
  pos= my_stpcpy(pos, " ;file_id=");
  pos= int10_to_str((long) file_id, pos, 10);
  protocol->store(buf, pos-buf, &my_charset_bin);
  my_free(buf);
  return 0;
}


int
Execute_load_query_log_event::do_apply_event(Relay_log_info const *rli)
{
  char *p;
  char *buf;
  char *fname;
  char *fname_end;
  int error;

  buf= (char*) my_malloc(key_memory_log_event,
                         q_len + 1 - (fn_pos_end - fn_pos_start) +
                         (FN_REFLEN + TEMP_FILE_MAX_LEN) + 10 + 8 + 5, MYF(MY_WME));

  DBUG_EXECUTE_IF("LOAD_DATA_INFILE_has_fatal_error", my_free(buf); buf= NULL;);

  /* Replace filename and LOCAL keyword in query before executing it */
  if (buf == NULL)
  {
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER(ER_SLAVE_FATAL_ERROR), "Not enough memory");
    return 1;
  }

  p= buf;
  memcpy(p, query, fn_pos_start);
  p+= fn_pos_start;
  fname= (p= strmake(p, STRING_WITH_LEN(" INFILE \'")));
  p= slave_load_file_stem(p, file_id, server_id, ".data");
  fname_end= p= strend(p);                      // Safer than p=p+5
  *(p++)='\'';
  switch (dup_handling) {
  case binary_log::LOAD_DUP_IGNORE:
    p= strmake(p, STRING_WITH_LEN(" IGNORE"));
    break;
  case binary_log::LOAD_DUP_REPLACE:
    p= strmake(p, STRING_WITH_LEN(" REPLACE"));
    break;
  default:
    /* Ordinary load data */
    break;
  }
  p= strmake(p, STRING_WITH_LEN(" INTO "));
  p= strmake(p, query+fn_pos_end, q_len-fn_pos_end);

  error= Query_log_event::do_apply_event(rli, buf, p-buf);

  /* Forging file name for deletion in same buffer */
  *fname_end= 0;

  /*
    If there was an error the slave is going to stop, leave the
    file so that we can re-execute this event at START SLAVE.
  */
  if (!error)
    mysql_file_delete(key_file_log_event_data, fname, MYF(MY_WME));

  my_free(buf);
  return error;
}
#endif


/**************************************************************************
	sql_ex_info methods
**************************************************************************/

/*
  sql_ex_info::write_data()
*/

bool sql_ex_info::write_data(IO_CACHE* file)
{
  if (data_info.new_format())
  {
    return (write_str_at_most_255_bytes(file, data_info.field_term,
                                        (uint) data_info.field_term_len) ||
	    write_str_at_most_255_bytes(file, data_info.enclosed,
                                        (uint) data_info.enclosed_len) ||
	    write_str_at_most_255_bytes(file, data_info.line_term,
                                        (uint) data_info.line_term_len) ||
	    write_str_at_most_255_bytes(file, data_info.line_start,
                                        (uint) data_info.line_start_len) ||
	    write_str_at_most_255_bytes(file, data_info.escaped,
                                        (uint) data_info.escaped_len) ||
	    my_b_safe_write(file,(uchar*) &(data_info.opt_flags), 1));
  }
  else
  {
    /**
      @todo This is sensitive to field padding. We should write a
      char[7], not an old_sql_ex. /sven
    */
    binary_log::old_sql_ex old_ex;
    old_ex.field_term= *(data_info.field_term);
    old_ex.enclosed=   *(data_info.enclosed);
    old_ex.line_term=  *(data_info.line_term);
    old_ex.line_start= *(data_info.line_start);
    old_ex.escaped=    *(data_info.escaped);
    old_ex.opt_flags=  data_info.opt_flags;
    old_ex.empty_flags= data_info.empty_flags;
    return my_b_safe_write(file, (uchar*) &old_ex, sizeof(old_ex)) != 0;
  }
}


/**
  sql_ex_info::init()
  This method initializes the members of strcuture variable sql_ex_info,
  defined in a Load_log_event. The structure, initializes the sub struct
  data_info, with the subclause characters in a LOAD_DATA_INFILE query.

*/
const char *sql_ex_info::init(const char *buf, const char *buf_end,
                              bool use_new_format)
{
  return data_info.init(buf, buf_end, use_new_format);
}
#ifndef DBUG_OFF
#ifndef MYSQL_CLIENT
static uchar dbug_extra_row_data_val= 0;

/**
   set_extra_data

   Called during self-test to generate various
   self-consistent binlog row event extra
   thread data structures which can be checked
   when reading the binlog.

   @param arr  Buffer to use
*/
const uchar* set_extra_data(uchar* arr)
{
  uchar val= (dbug_extra_row_data_val++) %
    (EXTRA_ROW_INFO_MAX_PAYLOAD + 1); /* 0 .. MAX_PAYLOAD + 1 */
  arr[EXTRA_ROW_INFO_LEN_OFFSET]= val + EXTRA_ROW_INFO_HDR_BYTES;
  arr[EXTRA_ROW_INFO_FORMAT_OFFSET]= val;
  for (uchar i=0; i<val; i++)
    arr[EXTRA_ROW_INFO_HDR_BYTES+i]= val;

  return arr;
}

#endif // #ifndef MYSQL_CLIENT

/**
   check_extra_data

   Called during self-test to check that
   binlog row event extra data is self-
   consistent as defined by the set_extra_data
   function above.

   Will assert(false) if not.

   @param extra_row_data
*/
void check_extra_data(uchar* extra_row_data)
{
  assert(extra_row_data);
  uint16 len= extra_row_data[EXTRA_ROW_INFO_LEN_OFFSET];
  uint8 val= len - EXTRA_ROW_INFO_HDR_BYTES;
  assert(extra_row_data[EXTRA_ROW_INFO_FORMAT_OFFSET] == val);
  for (uint16 i= 0; i < val; i++)
  {
    assert(extra_row_data[EXTRA_ROW_INFO_HDR_BYTES + i] == val);
  }
}

#endif  // #ifndef DBUG_OFF

/**************************************************************************
	Rows_log_event member functions
**************************************************************************/

#ifndef MYSQL_CLIENT
Rows_log_event::Rows_log_event(THD *thd_arg, TABLE *tbl_arg, const Table_id& tid,
                               MY_BITMAP const *cols, bool using_trans,
                               Log_event_type event_type,
                               const uchar* extra_row_info)
 : binary_log::Rows_event(event_type),
   Log_event(thd_arg, 0,
             using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                           Log_event::EVENT_STMT_CACHE,
             Log_event::EVENT_NORMAL_LOGGING,
             header(), footer())
#ifdef HAVE_REPLICATION
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL), m_key_info(NULL),
    m_distinct_keys(Key_compare(&m_key_info)), m_distinct_key_spare_buf(NULL)
#endif
{
  common_header->type_code= m_type;
  m_row_count= 0;
  m_table_id= tid;
  m_width= tbl_arg ? tbl_arg->s->fields : 1;
  m_rows_buf= 0; m_rows_cur= 0; m_rows_end= 0; m_flags= 0;
  m_type= event_type; m_extra_row_data=0;

  DBUG_ASSERT(tbl_arg && tbl_arg->s && tid.is_valid());

  if (thd_arg->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS)
      set_flags(NO_FOREIGN_KEY_CHECKS_F);
  if (thd_arg->variables.option_bits & OPTION_RELAXED_UNIQUE_CHECKS)
      set_flags(RELAXED_UNIQUE_CHECKS_F);
#ifndef DBUG_OFF
  uchar extra_data[255];
  DBUG_EXECUTE_IF("extra_row_data_set",
                  /* Set extra row data to a known value */
                  extra_row_info = set_extra_data(extra_data););
#endif
  if (extra_row_info)
  {
    /* Copy Extra data from thd into new event */
    uint8 extra_data_len= extra_row_info[EXTRA_ROW_INFO_LEN_OFFSET];
    assert(extra_data_len >= EXTRA_ROW_INFO_HDR_BYTES);

    m_extra_row_data= (uchar*) my_malloc(key_memory_log_event,
                                         extra_data_len, MYF(MY_WME));

    if (likely(m_extra_row_data != NULL))
    {
      memcpy(m_extra_row_data, extra_row_info,
             extra_data_len);
    }
  }

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
  /*
   -Check that malloc() succeeded in allocating memory for the rows
    buffer and the COLS vector.
   -Checking that an Update_rows_log_event
    is valid is done while setting the Update_rows_log_event::is_valid
  */
  if (m_rows_buf && m_cols.bitmap)
    is_valid_param= true;
}
#endif

Rows_log_event::Rows_log_event(const char *buf, uint event_len,
                               const Format_description_event
                               *description_event)
: binary_log::Rows_event(buf, event_len, description_event),
  Log_event(header(), footer()),
  m_row_count(0),
#ifndef MYSQL_CLIENT
  m_table(NULL),
#endif
  m_rows_buf(0), m_rows_cur(0), m_rows_end(0)
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL), m_key_info(NULL),
    m_distinct_keys(Key_compare(&m_key_info)), m_distinct_key_spare_buf(NULL)
#endif
{
  DBUG_ENTER("Rows_log_event::Rows_log_event(const char*,...)");

  DBUG_ASSERT(header()->type_code == m_type);


  if (m_extra_row_data)
    DBUG_EXECUTE_IF("extra_row_data_check",
                    /* Check extra data has expected value */
                    check_extra_data(m_extra_row_data););


  /*
     m_cols and m_cols_ai are of the type MY_BITMAP, which are members of
     class Rows_log_event, and are used while applying the row events on
     the slave.
     The bitmap integer is initialized by copying the contents of the
     vector column_before_image for m_cols.bitamp, and vector
     column_after_image for m_cols_ai.bitmap. m_cols_ai is only initialized
     for UPDATE_ROWS_EVENTS, else it is equal to the before image.
  */
  memset(&m_cols, 0, sizeof(m_cols));
  /* if bitmap_init fails, is_valid will be set to false */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf) * 8 ? m_bitbuf : NULL,
                          m_width,
                          false)))
  {
    if (!columns_before_image.empty())
    {
      memcpy(m_cols.bitmap, &columns_before_image[0], (m_width + 7) / 8);
      create_last_word_mask(&m_cols);
      DBUG_DUMP("m_cols", (uchar*) m_cols.bitmap, no_bytes_in_map(&m_cols));
    } //end if columns_before_image.empty()
    else
    m_cols.bitmap= NULL;
  }
  else
  {
    // Needed because bitmap_init() does not set it to null on failure
    m_cols.bitmap= NULL;
    DBUG_VOID_RETURN;
  }
  m_cols_ai.bitmap= m_cols.bitmap; //See explanation below while setting is_valid.

  if ((m_type == binary_log::UPDATE_ROWS_EVENT) ||
      (m_type == binary_log::UPDATE_ROWS_EVENT_V1))
  {
    /* if bitmap_init fails, is_valid will be set to false*/
    if (likely(!bitmap_init(&m_cols_ai,
                            m_width <= sizeof(m_bitbuf_ai) * 8 ?
                                        m_bitbuf_ai : NULL,
                            m_width,
                            false)))
    {
      if (!columns_after_image.empty())
      {
        memcpy(m_cols_ai.bitmap, &columns_after_image[0], (m_width + 7) / 8);
        create_last_word_mask(&m_cols_ai);
       DBUG_DUMP("m_cols_ai", (uchar*) m_cols_ai.bitmap,
                  no_bytes_in_map(&m_cols_ai));
      }
      else
        m_cols_ai.bitmap= NULL;
    }
    else
    {
      // Needed because bitmap_init() does not set it to null on failure
      m_cols_ai.bitmap= 0;
      DBUG_VOID_RETURN;
    }
  }


  /*
    m_rows_buf, m_cur_row and m_rows_end are pointers to the vector rows.
    m_rows_buf is the pointer to the first byte of first row in the event.
    m_curr_row points to current row being applied on the slave. Initially,
    this points to the same element as m_rows_buf in the vector.
    m_rows_end points to the last byte in the last row in the event.

    These pointers are used while applying the events on to the slave, and
    are not required for decoding.
  */
  if (likely(!row.empty()))
  {
    m_rows_buf= &row[0];
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
    m_curr_row= m_rows_buf;
#endif
    m_rows_end= m_rows_buf + row.size() - 1;
    m_rows_cur= m_rows_end;
  }
  /*
    -Check that malloc() succeeded in allocating memory for the row
     buffer and the COLS vector.
    -Checking that an Update_rows_log_event
     is valid is done while setting the Update_rows_log_event::is_valid
  */
  if (m_rows_buf && m_cols.bitmap)
    is_valid_param= true;
  DBUG_VOID_RETURN;
}

Rows_log_event::~Rows_log_event()
{
  if (m_cols.bitmap)
  {
    if (m_cols.bitmap == m_bitbuf) // no my_malloc happened
      m_cols.bitmap= 0; // so no my_free in bitmap_free
    bitmap_free(&m_cols); // To pair with bitmap_init().
  }
}
size_t Rows_log_event::get_data_size()
{
  int const general_type_code= get_general_type_code();

  uchar buf[sizeof(m_width) + 1];
  uchar *end= net_store_length(buf, m_width);

  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  return 6 + no_bytes_in_map(&m_cols) + (end - buf) +
                  (general_type_code == binary_log::UPDATE_ROWS_EVENT ?
                                        no_bytes_in_map(&m_cols_ai) : 0) +
                  (m_rows_cur - m_rows_buf););

  int data_size= 0;
  bool is_v2_event= common_header->type_code > binary_log::DELETE_ROWS_EVENT_V1;
  if (is_v2_event)
  {
    data_size= Binary_log_event::ROWS_HEADER_LEN_V2 +
      (m_extra_row_data ?
       ROWS_V_TAG_LEN + m_extra_row_data[EXTRA_ROW_INFO_LEN_OFFSET]:
       0);
  }
  else
  {
    data_size= Binary_log_event::ROWS_HEADER_LEN_V1;
  }
  data_size+= no_bytes_in_map(&m_cols);
  data_size+= (uint) (end - buf);

  if (general_type_code == binary_log::UPDATE_ROWS_EVENT)
    data_size+= no_bytes_in_map(&m_cols_ai);

  data_size+= (uint) (m_rows_cur - m_rows_buf);
  return data_size; 
}


#ifndef MYSQL_CLIENT
int Rows_log_event::do_add_row_data(uchar *row_data, size_t length)
{
  /*
    When the table has a primary key, we would probably want, by default, to
    log only the primary key value instead of the entire "before image". This
    would save binlog space. TODO
  */
  DBUG_ENTER("Rows_log_event::do_add_row_data");
  DBUG_PRINT("enter", ("row_data: 0x%lx  length: %lu", (ulong) row_data,
                       (ulong) length));

  /*
    If length is zero, there is nothing to write, so we just
    return. Note that this is not an optimization, since calling
    realloc() with size 0 means free().
   */
  if (length == 0)
  {
    m_row_count++;
    DBUG_RETURN(0);
  }

  DBUG_DUMP("row_data", row_data, min<size_t>(length, 32));

  DBUG_ASSERT(m_rows_buf <= m_rows_cur);
  DBUG_ASSERT(!m_rows_buf || (m_rows_end && m_rows_buf < m_rows_end));
  DBUG_ASSERT(m_rows_cur <= m_rows_end);

  /* The cast will always work since m_rows_cur <= m_rows_end */
  if (static_cast<size_t>(m_rows_end - m_rows_cur) <= length)
  {
    size_t const block_size= 1024;
    ulong cur_size= m_rows_cur - m_rows_buf;
    DBUG_EXECUTE_IF("simulate_too_big_row_case1",
                     cur_size= UINT_MAX32 - (block_size * 10);
                     length= UINT_MAX32 - (block_size * 10););
    DBUG_EXECUTE_IF("simulate_too_big_row_case2",
                     cur_size= UINT_MAX32 - (block_size * 10);
                     length= block_size * 10;);
    DBUG_EXECUTE_IF("simulate_too_big_row_case3",
                     cur_size= block_size * 10;
                     length= UINT_MAX32 - (block_size * 10););
    DBUG_EXECUTE_IF("simulate_too_big_row_case4",
                     cur_size= UINT_MAX32 - (block_size * 10);
                     length= (block_size * 10) - block_size + 1;);
    ulong remaining_space= UINT_MAX32 - cur_size;
    /* Check that the new data fits within remaining space and we can add
       block_size without wrapping.
     */
    if (length > remaining_space ||
        ((length + block_size) > remaining_space))
    {
      sql_print_error("The row data is greater than 4GB, which is too big to "
                      "write to the binary log.");
      DBUG_RETURN(ER_BINLOG_ROW_LOGGING_FAILED);
    }
    const size_t new_alloc= 
        block_size * ((cur_size + length + block_size - 1) / block_size);
    if (new_alloc)
      row.resize(new_alloc);

    /* If the memory moved, we need to move the pointers */
    if (new_alloc && &row[0] != m_rows_buf)
    {
      m_rows_buf= &row[0];
      if (m_rows_buf && m_cols.bitmap)
        is_valid_param= true;
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

/**
  Checks if any of the columns in the given table is
  signaled in the bitmap.

  For each column in the given table checks if it is
  signaled in the bitmap. This is most useful when deciding
  whether a before image (BI) can be used or not for
  searching a row. If no column is signaled, then the
  image cannot be used for searching a record (regardless
  of using position(), index scan or table scan). Here is
  an example:

  MASTER> SET @@binlog_row_image='MINIMAL';
  MASTER> CREATE TABLE t1 (a int, b int, c int, primary key(c));
  SLAVE>  CREATE TABLE t1 (a int, b int);
  MASTER> INSERT INTO t1 VALUES (1,2,3);
  MASTER> UPDATE t1 SET a=2 WHERE b=2;

  For the update statement only the PK (column c) is
  logged in the before image (BI). As such, given that
  the slave has no column c, it will not be able to
  find the row, because BI has no values for the columns
  the slave knows about (column a and b).

  @param table   the table reference on the slave.
  @param cols the bitmap signaling columns available in
                 the BI.

  @return TRUE if BI contains usable colums for searching,
          FALSE otherwise.
*/
static
my_bool is_any_column_signaled_for_table(TABLE *table, MY_BITMAP *cols)
{
  DBUG_ENTER("is_any_column_signaled_for_table");

  for (Field **ptr= table->field ;
       *ptr && ((*ptr)->field_index < cols->n_bits);
       ptr++)
  {
    if (bitmap_is_set(cols, (*ptr)->field_index))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN (FALSE);
}

/**
  Checks if the fields in the given key are signaled in
  the bitmap.

  Validates whether the before image is usable for the
  given key. It can be the case that the before image
  does not contain values for the key (eg, master was
  using 'minimal' option for image logging and slave has
  different index structure on the table). Here is an
  example:

  MASTER> SET @@binlog_row_image='MINIMAL';
  MASTER> CREATE TABLE t1 (a int, b int, c int, primary key(c));
  SLAVE> CREATE TABLE t1 (a int, b int, c int, key(a,c));
  MASTER> INSERT INTO t1 VALUES (1,2,3);
  MASTER> UPDATE t1 SET a=2 WHERE b=2;

  When finding the row on the slave, one cannot use the
  index (a,c) to search for the row, because there is only
  data in the before image for column c. This function
  checks the fields needed for a given key and searches
  the bitmap to see if all the fields required are
  signaled.

  @param keyinfo  reference to key.
  @param cols     the bitmap signaling which columns
                  have available data.

  @return TRUE if all fields are signaled in the bitmap
          for the given key, FALSE otherwise.
*/
static
my_bool are_all_columns_signaled_for_key(KEY *keyinfo, MY_BITMAP *cols)
{
  DBUG_ENTER("are_all_columns_signaled_for_key");

  for (uint i=0 ; i < keyinfo->user_defined_key_parts ;i++)
  {
    uint fieldnr= keyinfo->key_part[i].fieldnr - 1;
    if (fieldnr >= cols->n_bits ||
        !bitmap_is_set(cols, fieldnr))
      DBUG_RETURN(FALSE);
  }

  DBUG_RETURN(TRUE);
}

/**
  Searches the table for a given key that can be used
  according to the existing values, ie, columns set
  in the bitmap.

  The caller can specify which type of key to find by
  setting the following flags in the key_type parameter:

    - PRI_KEY_FLAG
      Returns the primary key.

    - UNIQUE_KEY_FLAG
      Returns a unique key (flagged with HA_NOSAME)

    - MULTIPLE_KEY_FLAG
      Returns a key that is not unique (flagged with HA_NOSAME
      and without HA_NULL_PART_KEY) nor PK.

  The above flags can be used together, in which case, the
  search is conducted in the above listed order. Eg, the
  following flag:

    (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)

  means that a primary key is returned if it is suitable. If
  not then the unique keys are searched. If no unique key is
  suitable, then the keys are searched. Finally, if no key
  is suitable, MAX_KEY is returned.

  @param table    reference to the table.
  @param bi_cols  a bitmap that filters out columns that should
                  not be considered while searching the key.
                  Columns that should be considered are set.
  @param key_type the type of key to search for.

  @return MAX_KEY if no key, according to the key_type specified
          is suitable. Returns the key otherwise.

*/
static
uint
search_key_in_table(TABLE *table, MY_BITMAP *bi_cols, uint key_type)
{
  DBUG_ENTER("search_key_in_table");

  KEY *keyinfo;
  uint res= MAX_KEY;
  uint key;

  if (key_type & PRI_KEY_FLAG &&
      (table->s->primary_key < MAX_KEY))
  {
    DBUG_PRINT("debug", ("Searching for PK"));
    keyinfo= table->s->key_info + table->s->primary_key;
    if (are_all_columns_signaled_for_key(keyinfo, bi_cols))
      DBUG_RETURN(table->s->primary_key);
  }

  if (key_type & UNIQUE_KEY_FLAG)
  {
    DBUG_PRINT("debug", ("Searching for UK"));
    for (key=0,keyinfo= table->key_info ;
         (key < table->s->keys) && (res == MAX_KEY);
         key++,keyinfo++)
    {
      /*
        - Unique keys cannot be disabled, thence we skip the check.
        - Skip unique keys with nullable parts
        - Skip primary keys
      */
      if (!((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME) ||
          (key == table->s->primary_key))
        continue;
      res= are_all_columns_signaled_for_key(keyinfo, bi_cols) ?
           key : MAX_KEY;

      if (res < MAX_KEY)
        DBUG_RETURN(res);
    }
    DBUG_PRINT("debug", ("UK has NULLABLE parts or not all columns signaled."));
  }

  if (key_type & MULTIPLE_KEY_FLAG && table->s->keys)
  {
    DBUG_PRINT("debug", ("Searching for K."));
    for (key=0,keyinfo= table->key_info ;
         (key < table->s->keys) && (res == MAX_KEY);
         key++,keyinfo++)
    {
      /*
        - Skip innactive keys
        - Skip unique keys without nullable parts
        - Skip indices that do not support ha_index_next() e.g. full-text
        - Skip primary keys
      */
      if (!(table->s->keys_in_use.is_set(key)) ||
          ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME) ||
          !(table->file->index_flags(key, 0, true) & HA_READ_NEXT) ||
          (key == table->s->primary_key))
        continue;

      res= are_all_columns_signaled_for_key(keyinfo, bi_cols) ?
           key : MAX_KEY;

      if (res < MAX_KEY)
        DBUG_RETURN(res);
    }
    DBUG_PRINT("debug", ("Not all columns signaled for K."));
  }

  DBUG_RETURN(res);
}

void
Rows_log_event::decide_row_lookup_algorithm_and_key()
{

  DBUG_ENTER("decide_row_lookup_algorithm_and_key");

  /*
    Decision table:
    - I  --> Index scan / search
    - T  --> Table scan
    - Hi --> Hash over index
    - Ht --> Hash over the entire table

    |--------------+-----------+------+------+------|
    | Index\Option | I , T , H | I, T | I, H | T, H |
    |--------------+-----------+------+------+------|
    | PK / UK      | I         | I    | I    | Hi   |
    | K            | Hi        | I    | Hi   | Hi   |
    | No Index     | Ht        | T    | Ht   | Ht   |
    |--------------+-----------+------+------+------|

  */

  TABLE *table= this->m_table;
  uint event_type= this->get_general_type_code();
  MY_BITMAP *cols= &this->m_cols;
  this->m_rows_lookup_algorithm= ROW_LOOKUP_NOT_NEEDED;
  this->m_key_index= MAX_KEY;
  this->m_key_info= NULL;

  if (event_type == binary_log::WRITE_ROWS_EVENT)  // row lookup not needed
    DBUG_VOID_RETURN;

  if (!(slave_rows_search_algorithms_options & SLAVE_ROWS_INDEX_SCAN))
    goto TABLE_OR_INDEX_HASH_SCAN;

  /* PK or UK => use LOOKUP_INDEX_SCAN */
  this->m_key_index= search_key_in_table(table, cols, (PRI_KEY_FLAG | UNIQUE_KEY_FLAG));
  if (this->m_key_index != MAX_KEY)
  {
    DBUG_PRINT("info", ("decide_row_lookup_algorithm_and_key: decided - INDEX_SCAN"));
    this->m_rows_lookup_algorithm= ROW_LOOKUP_INDEX_SCAN;
    goto end;
  }

TABLE_OR_INDEX_HASH_SCAN:

  /*
     NOTE: Engines like Blackhole cannot use HASH_SCAN, because
           they do not syncronize reads .
   */
  if (!(slave_rows_search_algorithms_options & SLAVE_ROWS_HASH_SCAN) ||
      (table->file->ha_table_flags() & HA_READ_OUT_OF_SYNC))
    goto TABLE_OR_INDEX_FULL_SCAN;

  /* search for a key to see if we can narrow the lookup domain further. */
  this->m_key_index= search_key_in_table(table, cols, (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG));
  this->m_rows_lookup_algorithm= ROW_LOOKUP_HASH_SCAN;
  if (m_key_index < MAX_KEY)
    m_distinct_key_spare_buf= (uchar*) thd->alloc(table->key_info[m_key_index].key_length);
  DBUG_PRINT("info", ("decide_row_lookup_algorithm_and_key: decided - HASH_SCAN"));
  goto end;

TABLE_OR_INDEX_FULL_SCAN:

  this->m_key_index= MAX_KEY;

  /* If we can use an index, try to narrow the scan a bit further. */
  if (slave_rows_search_algorithms_options & SLAVE_ROWS_INDEX_SCAN)
    this->m_key_index= search_key_in_table(table, cols, (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG));

  if (this->m_key_index != MAX_KEY)
  {
    DBUG_PRINT("info", ("decide_row_lookup_algorithm_and_key: decided - INDEX_SCAN"));
    this->m_rows_lookup_algorithm= ROW_LOOKUP_INDEX_SCAN;
  }
  else
  {
    DBUG_PRINT("info", ("decide_row_lookup_algorithm_and_key: decided - TABLE_SCAN"));
    this->m_rows_lookup_algorithm= ROW_LOOKUP_TABLE_SCAN;
  }

end:

  /* m_key_index is ready, set m_key_info now. */
  m_key_info= m_table->key_info + m_key_index;
  /*
    m_key_info will influence key comparison code in HASH_SCAN mode,
    so the m_distinct_keys set should still be empty.
  */
  DBUG_ASSERT(m_distinct_keys.empty());

#ifndef DBUG_OFF
  const char* s= ((m_rows_lookup_algorithm == Rows_log_event::ROW_LOOKUP_TABLE_SCAN) ? "TABLE_SCAN" :
                  ((m_rows_lookup_algorithm == Rows_log_event::ROW_LOOKUP_HASH_SCAN) ? "HASH_SCAN" :
                   "INDEX_SCAN"));

  // only for testing purposes
  slave_rows_last_search_algorithm_used= m_rows_lookup_algorithm;
  DBUG_PRINT("debug", ("Row lookup method: %s", s));
#endif

  DBUG_VOID_RETURN;
}

/*
  Encapsulates the  operations to be done before applying
  row events for update and delete.

  @ret value error code
             0 success
*/
int
Rows_log_event::row_operations_scan_and_key_setup()
{
  int error= 0;
  DBUG_ENTER("Row_log_event::row_operations_scan_and_key_setup");

  /*
     Prepare memory structures for search operations. If
     search is performed:

     1. using hash search => initialize the hash
     2. using key => decide on key to use and allocate mem structures
     3. using table scan => do nothing
   */
  decide_row_lookup_algorithm_and_key();

  switch (m_rows_lookup_algorithm)
  {
  case ROW_LOOKUP_HASH_SCAN:
    {
      if (m_hash.init())
        error= HA_ERR_OUT_OF_MEM;
      goto err;
    }
  case ROW_LOOKUP_INDEX_SCAN:
    {
      DBUG_ASSERT (m_key_index < MAX_KEY);
      // Allocate buffer for key searches
      m_key= (uchar*)my_malloc(key_memory_log_event,
                               m_key_info->key_length, MYF(MY_WME));
      if (!m_key)
        error= HA_ERR_OUT_OF_MEM;
      goto err;
    }
  case ROW_LOOKUP_TABLE_SCAN:
  default: break;
  }
err:
  DBUG_RETURN(error);
}

/*
  Encapsulates the  operations to be done after applying
  row events for update and delete.

  @ret value error code
             0 success
*/

int
Rows_log_event::row_operations_scan_and_key_teardown(int error)
{
  DBUG_ENTER("Rows_log_event::row_operations_scan_and_key_teardown");

  DBUG_ASSERT(!m_table->file->inited);
  switch (m_rows_lookup_algorithm)
  {
  case ROW_LOOKUP_HASH_SCAN:
    {
      m_hash.deinit(); // we don't need the hash anymore.
      goto err;
    }

  case ROW_LOOKUP_INDEX_SCAN:
    {
      if (m_table->s->keys > 0)
      {
        my_free(m_key); // Free for multi_malloc
        m_key= NULL;
        m_key_index= MAX_KEY;
        m_key_info= NULL;
      }
     goto err;
    }

  case ROW_LOOKUP_TABLE_SCAN:
  default: break;
  }

err:
  m_rows_lookup_algorithm= ROW_LOOKUP_UNDEFINED;
  DBUG_RETURN(error);
}

/*
  Compares table->record[0] and table->record[1]

  Returns TRUE if different.
*/
static bool record_compare(TABLE *table, MY_BITMAP *cols)
{
  DBUG_ENTER("record_compare");

  /*
    Need to set the X bit and the filler bits in both records since
    there are engines that do not set it correctly.

    In addition, since MyISAM checks that one hasn't tampered with the
    record, it is necessary to restore the old bytes into the record
    after doing the comparison.

    TODO[record format ndb]: Remove it once NDB returns correct
    records. Check that the other engines also return correct records.
   */

  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_DUMP("record[1]", table->record[1], table->s->reclength);

  bool result= false;
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

  /**
    Compare full record only if:
    - there are no blob fields (otherwise we would also need
      to compare blobs contents as well);
    - there are no varchar fields (otherwise we would also need
      to compare varchar contents as well);
    - there are no null fields, otherwise NULLed fields
      contents (i.e., the don't care bytes) may show arbitrary
      values, depending on how each engine handles internally.
    - if all the bitmap is set (both are full rows)
    */
  if ((table->s->blob_fields +
       table->s->varchar_fields +
       table->s->null_fields) == 0 &&
      bitmap_is_set_all(cols))
  {
    result= cmp_record(table,record[1]);
  }

  /*
    Fallback to field-by-field comparison:
    1. start by checking if the field is signaled:
    2. if it is, first compare the null bit if the field is nullable
    3. then compare the contents of the field, if it is not
       set to null
   */
  else
  {
    for (Field **ptr=table->field ;
         *ptr && ((*ptr)->field_index < cols->n_bits) && !result;
         ptr++)
    {
      Field *field= *ptr;
      if (bitmap_is_set(cols, field->field_index) && !field->is_virtual_gcol())
      {
        /* compare null bit */
        if (field->is_null() != field->is_null_in_record(table->record[1]))
          result= true;

        /* compare content, only if fields are not set to NULL */
        else if (!field->is_null())
          result= field->cmp_binary_offset(table->s->rec_buff_length);
      }
    }
  }

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

      if (table->s->last_null_bit_pos)
        table->record[i][table->s->null_bytes - 1]= saved_filler[i];
    }
  }

  DBUG_RETURN(result);
}

void Rows_log_event::do_post_row_operations(Relay_log_info const *rli, int error)
{

  /*
    If m_curr_row_end  was not set during event execution (e.g., because
    of errors) we can't proceed to the next row. If the error is transient
    (i.e., error==0 at this point) we must call unpack_current_row() to set
    m_curr_row_end.
  */

  DBUG_PRINT("info", ("curr_row: 0x%lu; curr_row_end: 0x%lu; rows_end: 0x%lu",
                      (ulong) m_curr_row, (ulong) m_curr_row_end, (ulong) m_rows_end));

  if (!m_curr_row_end && !error)
  {
    const uchar *previous_m_curr_row= m_curr_row;
    error= unpack_current_row(rli, &m_cols);

    if (!error && previous_m_curr_row == m_curr_row)
    {
      error= 1;
    }
  }

  // at this moment m_curr_row_end should be set
  DBUG_ASSERT(error || m_curr_row_end != NULL);
  DBUG_ASSERT(error || m_curr_row <= m_curr_row_end);
  DBUG_ASSERT(error || m_curr_row_end <= m_rows_end);

  m_curr_row= m_curr_row_end;

  if (error == 0 && !m_table->file->has_transactions())
  {
    thd->get_transaction()->set_unsafe_rollback_flags(Transaction_ctx::SESSION,
                                                      TRUE);
    thd->get_transaction()->set_unsafe_rollback_flags(Transaction_ctx::STMT,
                                                      TRUE);
  }
}

int Rows_log_event::handle_idempotent_and_ignored_errors(Relay_log_info const *rli, int *err)
{
  int error= *err;
  if (error)
  {
    int actual_error= convert_handler_error(error, thd, m_table);
    bool idempotent_error= (idempotent_error_code(error) &&
                           (rbr_exec_mode == RBR_EXEC_MODE_IDEMPOTENT));
    bool ignored_error= (idempotent_error == 0 ?
                         ignored_error_code(actual_error) : 0);

    if (idempotent_error || ignored_error)
    {
      loglevel ll;
      if (idempotent_error)
        ll= WARNING_LEVEL;
      else
        ll= INFORMATION_LEVEL;
      slave_rows_error_report(ll, error, rli, thd, m_table,
                              get_type_str(),
                              const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                              (ulong) common_header->log_pos);
      thd->get_stmt_da()->reset_condition_info(thd);
      clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      *err= 0;
      if (idempotent_error == 0)
        return ignored_error;
    }
  }

  return *err;
}

int Rows_log_event::do_apply_row(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_apply_row");

  int error= 0;

  /* in_use can have been set to NULL in close_tables_for_reopen */
  THD* old_thd= m_table->in_use;
  if (!m_table->in_use)
    m_table->in_use= thd;

  error= do_exec_row(rli);

  if(error)
  {
    DBUG_PRINT("info", ("error: %s", HA_ERR(error)));
    DBUG_ASSERT(error != HA_ERR_RECORD_DELETED);
  }

  m_table->in_use = old_thd;

  DBUG_RETURN(error);
}

/**
   Does the cleanup
     -  closes the index if opened by open_record_scan
     -  closes the table if opened for scanning.
*/
int
Rows_log_event::close_record_scan()
{
  DBUG_ENTER("Rows_log_event::close_record_scan");
  int error= 0;

  // if there is something to actually close
  if (m_key_index < MAX_KEY)
  {
    if (m_table->file->inited)
      error= m_table->file->ha_index_end();
  }
  else if (m_table->file->inited)
    error= m_table->file->ha_rnd_end();

  DBUG_RETURN(error);
}

/**
  Fetches next row. If it is a HASH_SCAN over an index, it populates
  table->record[0] with the next row corresponding to the index. If
  the indexes are in non-contigous ranges it fetches record corresponding
  to the key value in the next range.

  @parms: bool first_read : signifying if this is the first time we are reading a row
          over an index.
  @return_value: -  error code when there are no more reeords to be fetched or some other
                    error occured,
                 -  0 otherwise.
*/
int
Rows_log_event::next_record_scan(bool first_read)
{
  DBUG_ENTER("Rows_log_event::next_record_scan");
  DBUG_ASSERT(m_table->file->inited);
  TABLE *table= m_table;
  int error= 0;

  if (m_key_index >= MAX_KEY)
    error= table->file->ha_rnd_next(table->record[0]);
  else
  {
    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    if (table->s->null_bytes > 0)
      table->record[0][table->s->null_bytes - 1]|=
        256U - (1U << table->s->last_null_bit_pos);

    if (!first_read)
    {
      /*
        if we fail to fetch next record corresponding to a key value, we
        move to the next key value. If we are out of key values as well an error
        will be returned.
       */
      error= table->file->ha_index_next_same(table->record[0], m_key,
                                             m_key_info->key_length);
      if(m_rows_lookup_algorithm == ROW_LOOKUP_HASH_SCAN)
      {
        /*
          if we are out of rows for this particular key value, we reposition the
          marker according to the next key value that we have in the list.
         */
        if (error)
        {
          if (m_itr != m_distinct_keys.end())
          {
            m_key= *m_itr;
            m_itr++;
            first_read= true;
          }
          else
            error= HA_ERR_KEY_NOT_FOUND;
        }
      }
    }

    if (first_read)
      if ((error= table->file->ha_index_read_map(table->record[0], m_key,
                                                 HA_WHOLE_KEY,
                                                 HA_READ_KEY_EXACT)))
      {
        DBUG_PRINT("info",("no record matching the key found in the table"));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
      }
  }

  DBUG_RETURN(error);
}

/**
  Initializes scanning of rows. Opens an index and initializes an iterator
  over a list of distinct keys (m_distinct_keys) if it is a HASH_SCAN
  over an index or the table if its a HASH_SCAN over the table.
*/
int
Rows_log_event::open_record_scan()
{
  int error= 0;
  TABLE *table= m_table;
  DBUG_ENTER("Rows_log_event::open_record_scan");

  if (m_key_index < MAX_KEY )
  {
    if(m_rows_lookup_algorithm == ROW_LOOKUP_HASH_SCAN)
    {
      /* initialize the iterator over the list of distinct keys that we have */
      m_itr= m_distinct_keys.begin();

      /* get the first element from the list of keys and increment the
         iterator
       */
      m_key= *m_itr;
      m_itr++;
    }
    else {
      /* this is an INDEX_SCAN we need to store the key in m_key */
      DBUG_ASSERT((m_rows_lookup_algorithm == ROW_LOOKUP_INDEX_SCAN) && m_key);
      key_copy(m_key, m_table->record[0], m_key_info, 0);
    }

    /*
      Save copy of the record in table->record[1]. It might be needed
      later if linear search is used to find exact match.
     */
    store_record(table,record[1]);

    DBUG_PRINT("info",("locating record using a key (index_read)"));

    /* The m_key_index'th key is active and usable: search the table using the index */
    if (!table->file->inited && (error= table->file->ha_index_init(m_key_index, FALSE)))
    {
      DBUG_PRINT("info",("ha_index_init returns error %d",error));
      goto end;
    }

    DBUG_DUMP("key data", m_key, m_key_info->key_length);
  }
  else
  {
    if ((error= table->file->ha_rnd_init(1)))
    {
      DBUG_PRINT("info",("error initializing table scan"
          " (ha_rnd_init returns %d)",error));
      table->file->print_error(error, MYF(0));
    }
  }

end:
  DBUG_RETURN(error);
}

/**
  Populates the m_distinct_keys with unique keys to be modified
  during HASH_SCAN over keys.
  @return_value -0 success
                -Err_code
*/
int
Rows_log_event::add_key_to_distinct_keyset()
{
  int error= 0;
  DBUG_ENTER("Rows_log_event::add_key_to_distinct_keyset");
  DBUG_ASSERT(m_key_index < MAX_KEY);
  key_copy(m_distinct_key_spare_buf, m_table->record[0], m_key_info, 0);
  std::pair<std::set<uchar *, Key_compare>::iterator,bool> ret=
    m_distinct_keys.insert(m_distinct_key_spare_buf);
  if (ret.second)
  {
    /* Insert is successful, so allocate a new buffer for next key */
    m_distinct_key_spare_buf= (uchar*) thd->alloc(m_key_info->key_length);
    if (!m_distinct_key_spare_buf)
    {
      error= HA_ERR_OUT_OF_MEM;
      goto err;
    }
  }

err:
  DBUG_RETURN(error);
}


int Rows_log_event::do_index_scan_and_update(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_index_scan_and_update");
  DBUG_ASSERT(m_table && m_table->in_use != NULL);

  int error= 0;
  const uchar *saved_m_curr_row= m_curr_row;

  /*
    rpl_row_tabledefs.test specifies that
    if the extra field on the slave does not have a default value
    and this is okay with Delete or Update events.
    Todo: fix wl3228 hld that requires defaults for all types of events
  */

  prepare_record(m_table, &m_cols, FALSE);
  if ((error= unpack_current_row(rli, &m_cols)))
    goto end;

  /*
    Trying to do an index scan without a usable key
    This is a valid state because we allow the user
    to set Slave_rows_search_algorithm= 'INDEX_SCAN'.

    Therefore on tables with no indexes we will end
    up here.
   */
  if (m_key_index >= MAX_KEY)
  {
    error= HA_ERR_END_OF_FILE;
    goto end;
  }

#ifndef DBUG_OFF
  DBUG_PRINT("info",("looking for the following record"));
  DBUG_DUMP("record[0]", m_table->record[0], m_table->s->reclength);
#endif

  if (m_key_index != m_table->s->primary_key)
    /* we dont have a PK, or PK is not usable */
    goto INDEX_SCAN;

  if ((m_table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL))
  {
    /*
      Read removal is possible since the engine supports write without
      previous read using full primary key
    */
    DBUG_PRINT("info", ("using read before write removal"));
    DBUG_ASSERT(m_key_index == m_table->s->primary_key);

    /*
      Tell the handler to ignore if key exists or not, since it's
      not yet known if the key does exist(when using rbwr)
    */
    m_table->file->extra(HA_EXTRA_IGNORE_NO_KEY);

    goto end;
  }

  if ((m_table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION))
  {
    /*
      Use a more efficient method to fetch the record given by
      table->record[0] if the engine allows it.  We first compute a
      row reference using the position() member function (it will be
      stored in table->file->ref) and then use rnd_pos() to position
      the "cursor" (i.e., record[0] in this case) at the correct row.

      TODO: Check that the correct record has been fetched by
      comparing it with the original record. Take into account that the
      record on the master and slave can be of different
      length. Something along these lines should work:

      ADD>>>  store_record(table,record[1]);
              int error= table->file->rnd_pos(table->record[0], table->file->ref);
      ADD>>>  DBUG_ASSERT(memcmp(table->record[1], table->record[0],
                                 table->s->reclength) == 0);

    */

    DBUG_PRINT("info",("locating record using primary key (position)"));
    if (m_table->file->inited && (error= m_table->file->ha_index_end()))
      goto end;

    error= m_table->file->rnd_pos_by_record(m_table->record[0]);
    if (error)
    {
      DBUG_PRINT("info",("rnd_pos returns error %d",error));
      if (error == HA_ERR_RECORD_DELETED)
        error= HA_ERR_KEY_NOT_FOUND;
    }

    goto end;
  }

  // We can't use position() - try other methods.

INDEX_SCAN:

  /* Use the m_key_index'th key */

  if ((error= open_record_scan()))
    goto end;

  error= next_record_scan(true);
  if (error)
  {
    DBUG_PRINT("info",("no record matching the key found in the table"));
    if (error == HA_ERR_RECORD_DELETED)
      error= HA_ERR_KEY_NOT_FOUND;
    goto end;
  }


  DBUG_PRINT("info",("found first matching record"));
  DBUG_DUMP("record[0]", m_table->record[0], m_table->s->reclength);
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
  if (m_key_info->flags & HA_NOSAME || m_key_index == m_table->s->primary_key)
  {
    /* Unique does not have non nullable part */
    if (!(m_key_info->flags & (HA_NULL_PART_KEY)))
      goto end;  // record found
    else
    {
      /*
        Unique has nullable part. We need to check if there is any field in the
        BI image that is null and part of UNNI.
      */
      bool null_found= FALSE;
      for (uint i=0; i < m_key_info->user_defined_key_parts && !null_found; i++)
      {
        uint fieldnr= m_key_info->key_part[i].fieldnr - 1;
        Field **f= m_table->field+fieldnr;
        null_found= (*f)->is_null();
      }

      if (!null_found)
        goto end;           // record found

      /* else fall through to index scan */
    }
  }

  /*
    In case key is not unique, we still have to iterate over records found
    and find the one which is identical to the row given. A copy of the
    record we are looking for is stored in record[1].
   */
  DBUG_PRINT("info",("non-unique index, scanning it to find matching record"));

  while (record_compare(m_table, &m_cols))
  {
    while((error= next_record_scan(false)))
    {
      /* We just skip records that has already been deleted */
      if (error == HA_ERR_RECORD_DELETED)
        continue;
      DBUG_PRINT("info",("no record matching the given row found"));
      goto end;
    }
  }

end:

  DBUG_ASSERT(error != HA_ERR_RECORD_DELETED);

  if (error && error != HA_ERR_RECORD_DELETED)
    m_table->file->print_error(error, MYF(0));
  else
    error= do_apply_row(rli);

  if (!error)
    error= close_record_scan();  
  else
    /* 
      we are already with errors. Keep the error code and 
      try to close the scan anyway.
    */
    (void) close_record_scan(); 

  if ((get_general_type_code() == binary_log::UPDATE_ROWS_EVENT) &&
      (saved_m_curr_row == m_curr_row))
  {
    /* we need to unpack the AI so that positions get updated */
    m_curr_row= m_curr_row_end;
    unpack_current_row(rli, &m_cols_ai);
  }
  m_table->default_column_bitmaps();
  DBUG_RETURN(error);

}

int Rows_log_event::do_hash_row(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_hash_row");
  DBUG_ASSERT(m_table && m_table->in_use != NULL);
  int error= 0;

  /* create an empty entry to add to the hash table */
  HASH_ROW_ENTRY* entry= m_hash.make_entry();
  if (entry == NULL)
  {
    error= 1;
    goto end;
  }
  /* Prepare the record, unpack and save positions. */
  entry->positions->bi_start= m_curr_row;        // save the bi start pos
  prepare_record(m_table, &m_cols, false);
  if ((error= unpack_current_row(rli, &m_cols)))
    goto end;
  entry->positions->bi_ends= m_curr_row_end;    // save the bi end pos

  /*
    Now that m_table->record[0] is filled in, we can add the entry
    to the hash table. Note that the put operation calculates the
    key based on record[0] contents (including BLOB fields).
   */
  m_hash.put(m_table, &m_cols, entry);

  if (m_key_index < MAX_KEY)
    add_key_to_distinct_keyset();

  /*
    We need to unpack the AI to advance the positions, so we
    know when we have reached m_rows_end and that we do not
    unpack the AI in the next iteration as if it was a BI.
  */
  if (get_general_type_code() == binary_log::UPDATE_ROWS_EVENT)
  {
    /* Save a copy of the BI. */
    store_record(m_table, record[1]);

     /*
      This is the situation after hashing the BI:

      ===|=== before image ====|=== after image ===|===
         ^                     ^
         m_curr_row            m_curr_row_end
    */

    /* Set the position to the start of the record to be unpacked. */
    m_curr_row= m_curr_row_end;

    /* We shouldn't need this, but lets not leave loose ends */
    prepare_record(m_table, &m_cols, false);
    error= unpack_current_row(rli, &m_cols_ai);

    /*
      This is the situation after unpacking the AI:

      ===|=== before image ====|=== after image ===|===
                               ^                   ^
                               m_curr_row          m_curr_row_end
    */

    /* Restore back the copy of the BI. */
    restore_record(m_table, record[1]);
  }

end:
  DBUG_RETURN(error);
}

int Rows_log_event::do_scan_and_update(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_scan_and_update");
  DBUG_ASSERT(m_table && m_table->in_use != NULL);
  DBUG_ASSERT(m_hash.is_empty() == false);
  TABLE *table= m_table;
  int error= 0;
  const uchar *saved_last_m_curr_row= NULL;
  const uchar *saved_last_m_curr_row_end= NULL;
  /* create an empty entry to add to the hash table */
  HASH_ROW_ENTRY* entry= NULL;
  int idempotent_errors= 0;
  int i= 0;

  saved_last_m_curr_row=m_curr_row;
  saved_last_m_curr_row_end=m_curr_row_end;

  DBUG_PRINT("info",("Hash was populated with %d records!", m_hash.size()));

  /* open table or index depending on whether we have set m_key_index or not. */
  if ((error= open_record_scan()))
    goto err;

  /*
     Scan the table only once and compare against entries in hash.
     When a match is found, apply the changes.
   */
  do
  {
    /* get the next record from the table */
    error= next_record_scan(i == 0);
    i++;

    if(error)
      DBUG_PRINT("info", ("error: %s", HA_ERR(error)));
    switch (error) {
      case 0:
      {
        entry= m_hash.get(table, &m_cols);
        store_record(table, record[1]);

        /**
           If there are collisions we need to be sure that this is
           indeed the record we want.  Loop through all records for
           the given key and explicitly compare them against the
           record we got from the storage engine.
         */
        while(entry)
        {
          m_curr_row= entry->positions->bi_start;
          m_curr_row_end= entry->positions->bi_ends;

          prepare_record(table, &m_cols, false);
          if ((error= unpack_current_row(rli, &m_cols)))
            goto close_table;

          if (record_compare(table, &m_cols))
            m_hash.next(&entry);
          else
            break;   // we found a match
        }

        /**
           We found the entry we needed, just apply the changes.
         */
        if (entry)
        {
          // just to be safe, copy the record from the SE to table->record[0]
          restore_record(table, record[1]);

          /**
             At this point, both table->record[0] and
             table->record[1] have the SE row that matched the one
             in the hash table.

             Thence if this is a DELETE we wouldn't need to mess
             around with positions anymore, but since this can be an
             update, we need to provide positions so that AI is
             unpacked correctly to table->record[0] in UPDATE
             implementation of do_exec_row().
          */
          m_curr_row= entry->positions->bi_start;
          m_curr_row_end= entry->positions->bi_ends;

          /* we don't need this entry anymore, just delete it */
          if ((error= m_hash.del(entry)))
            goto err;

          if ((error= do_apply_row(rli)))
          {
            if (handle_idempotent_and_ignored_errors(rli, &error))
              goto close_table;

            do_post_row_operations(rli, error);
          }
        }
      }
      break;

      case HA_ERR_RECORD_DELETED:
        // get next
        continue;

      case HA_ERR_KEY_NOT_FOUND:
        /* If the slave exec mode is idempotent or the error is
            skipped error, then don't break */
        if (handle_idempotent_and_ignored_errors(rli, &error))
          goto close_table;
        idempotent_errors++;
        continue;

      case HA_ERR_END_OF_FILE:
      default:
        // exception (hash is not empty and we have reached EOF or
        // other error happened)
        goto close_table;
    }
  }
  /**
    if the rbr_exec_mode is set to Idempotent, we cannot expect the hash to
    be empty. In such cases we count the number of idempotent errors and check
    if it is equal to or greater than the number of rows left in the hash.
   */
  while (((idempotent_errors < m_hash.size()) && !m_hash.is_empty()) &&
         (!error || (error == HA_ERR_RECORD_DELETED)));

close_table:
  if (error == HA_ERR_RECORD_DELETED)
    error= 0;

  if (error)
  {
    table->file->print_error(error, MYF(0));
    DBUG_PRINT("info", ("Failed to get next record"
                        " (ha_rnd_next returns %d)",error));
    /*
      we are already with errors. Keep the error code and
      try to close the scan anyway.
    */
    (void) close_record_scan();
  }
  else
    error= close_record_scan();

  DBUG_ASSERT((m_hash.is_empty() && !error) ||
              (!m_hash.is_empty() &&
               ((error) || (idempotent_errors >= m_hash.size()))));

err:

  if ((m_hash.is_empty() && !error) || (idempotent_errors >= m_hash.size()))
  {
    /**
       Reset the last positions, because the positions are lost while
       handling entries in the hash.
     */
    m_curr_row= saved_last_m_curr_row;
    m_curr_row_end= saved_last_m_curr_row_end;
  }

  DBUG_RETURN(error);
}

int Rows_log_event::do_hash_scan_and_update(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_hash_scan_and_update");
  DBUG_ASSERT(m_table && m_table->in_use != NULL);

  // HASHING PART

  /* unpack the BI (and AI, if it exists) and add it to the hash map. */
  if (int error= this->do_hash_row(rli))
    DBUG_RETURN(error);

  /* We have not yet hashed all rows in the buffer. Do not proceed to the SCAN part. */
  if (m_curr_row_end < m_rows_end)
    DBUG_RETURN (0);

  DBUG_PRINT("info",("Hash was populated with %d records!", m_hash.size()));
  DBUG_ASSERT(m_curr_row_end == m_rows_end);

  // SCANNING & UPDATE PART

  DBUG_RETURN(this->do_scan_and_update(rli));
}

int Rows_log_event::do_table_scan_and_update(Relay_log_info const *rli)
{
  int error= 0;
  const uchar* saved_m_curr_row= m_curr_row;
  TABLE* table= m_table;

  DBUG_ENTER("Rows_log_event::do_table_scan_and_update");
  DBUG_ASSERT(m_curr_row != m_rows_end);
  DBUG_PRINT("info",("locating record using table scan (ha_rnd_next)"));

  saved_m_curr_row= m_curr_row;

  /** unpack the before image */
  prepare_record(table, &m_cols, FALSE);
  if (!(error= unpack_current_row(rli, &m_cols)))
  {
    /** save a copy so that we can compare against it later */
    store_record(m_table, record[1]);

    int restart_count= 0; // Number of times scanning has restarted from top

    if ((error= m_table->file->ha_rnd_init(1)))
    {
      DBUG_PRINT("info",("error initializing table scan"
                         " (ha_rnd_init returns %d)",error));
      goto end;
    }

    /* Continue until we find the right record or have made a full loop */
    do
    {
  restart_ha_rnd_next:
      error= m_table->file->ha_rnd_next(m_table->record[0]);
      if (error)
        DBUG_PRINT("info", ("error: %s", HA_ERR(error)));
      switch (error) {
      case HA_ERR_END_OF_FILE:
        // restart scan from top
        if (++restart_count < 2)
        {
          if ((error= m_table->file->ha_rnd_init(1)))
            goto end;
          goto restart_ha_rnd_next;
        }
        break;

      case HA_ERR_RECORD_DELETED:
        // fetch next
        goto restart_ha_rnd_next;
      case 0:
        // we're good, check if record matches
        break;

      default:
        // exception
        goto end;
      }
    }
    while (restart_count < 2 && record_compare(m_table, &m_cols));
  }

end:

  DBUG_ASSERT(error != HA_ERR_RECORD_DELETED);

  /* either we report error or apply the changes */
  if (error && error != HA_ERR_RECORD_DELETED)
  {
    DBUG_PRINT("info", ("Failed to get next record"
                        " (ha_rnd_next returns %d)",error));
    m_table->file->print_error(error, MYF(0));
  }
  else
    error= do_apply_row(rli);


  if (!error)
    error= close_record_scan();  
  else
    /* 
      we are already with errors. Keep the error code and 
      try to close the scan anyway.
    */
    (void) close_record_scan(); 

  if ((get_general_type_code() == binary_log::UPDATE_ROWS_EVENT) &&
      (saved_m_curr_row == m_curr_row)) // we need to unpack the AI
  {
    m_curr_row= m_curr_row_end;
    unpack_current_row(rli, &m_cols);
  }

  table->default_column_bitmaps();
  DBUG_RETURN(error);
}

int Rows_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_apply_event(Relay_log_info*)");
  TABLE *table= NULL;
  int error= 0;

  /*
    'thd' has been set by exec_relay_log_event(), just before calling
    do_apply_event(). We still check here to prevent future coding
    errors.
  */
  DBUG_ASSERT(rli->info_thd == thd);

  /*
    If there is no locks taken, this is the first binrow event seen
    after the table map events.  We should then lock all the tables
    used in the transaction and proceed with execution of the actual
    event.
  */
  if (!thd->lock)
  {
    /*
      Lock_tables() reads the contents of thd->lex, so they must be
      initialized.

      We also call the mysql_reset_thd_for_next_command(), since this
      is the logical start of the next "statement". Note that this
      call might reset the value of current_stmt_binlog_format, so
      we need to do any changes to that value after this function.
    */
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    enum_gtid_statement_status state= gtid_pre_statement_checks(thd);
    if (state == GTID_STATEMENT_EXECUTE)
    {
      if (gtid_pre_statement_post_implicit_commit_checks(thd))
        state= GTID_STATEMENT_CANCEL;
    }

    if (state == GTID_STATEMENT_CANCEL)
    {
      uint error= thd->get_stmt_da()->mysql_errno();
      DBUG_ASSERT(error != 0);
      rli->report(ERROR_LEVEL, error,
                  "Error executing row event: '%s'",
                  thd->get_stmt_da()->message_text());
      thd->is_slave_error= 1;
      DBUG_RETURN(-1);
    }
    else if (state == GTID_STATEMENT_SKIP)
      goto end;

    /*
      The current statement is just about to begin and 
      has not yet modified anything. Note, all.modified is reset
      by mysql_reset_thd_for_next_command.
    */
    thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::STMT);
    /*
      This is a row injection, so we flag the "statement" as
      such. Note that this code is called both when the slave does row
      injections and when the BINLOG statement is used to do row
      injections.
    */
    thd->lex->set_stmt_row_injection();

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

    thd->binlog_row_event_extra_data = m_extra_row_data;

    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(thd->variables.option_bits) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    if (open_and_lock_tables(thd, rli->tables_to_lock, 0))
    {
      uint actual_error= thd->get_stmt_da()->mysql_errno();
      if (thd->is_slave_error || thd->is_fatal_error)
      {
        if (ignored_error_code(actual_error))
        {
          if (log_warnings > 1)
            rli->report(WARNING_LEVEL, actual_error,
                        "Error executing row event: '%s'",
                        (actual_error ? thd->get_stmt_da()->message_text() :
                         "unexpected success or fatal error"));
          thd->get_stmt_da()->reset_condition_info(thd);
          clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
          error= 0;
          goto end;
        }
        else
        {
          rli->report(ERROR_LEVEL, actual_error,
                      "Error executing row event: '%s'",
                      (actual_error ? thd->get_stmt_da()->message_text() :
                       "unexpected success or fatal error"));
          thd->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
          DBUG_RETURN(actual_error);
        }
      }
    }

    /*
      When the open and locking succeeded, we check all tables to
      ensure that they still have the correct type.
    */

    {
      DBUG_PRINT("debug", ("Checking compability of tables to lock - tables_to_lock: %p",
                           rli->tables_to_lock));

      /**
        When using RBR and MyISAM MERGE tables the base tables that make
        up the MERGE table can be appended to the list of tables to lock.
  
        Thus, we just check compatibility for those that tables that have
        a correspondent table map event (ie, those that are actually going
        to be accessed while applying the event). That's why the loop stops
        at rli->tables_to_lock_count .

        NOTE: The base tables are added here are removed when 
              close_thread_tables is called.
       */
      TABLE_LIST *table_list_ptr= rli->tables_to_lock;
      for (uint i=0 ; table_list_ptr && (i < rli->tables_to_lock_count);
           table_list_ptr= table_list_ptr->next_global, i++)
      {
        /*
          Below if condition takes care of skipping base tables that
          make up the MERGE table (which are added by open_tables()
          call). They are added next to the merge table in the list.
          For eg: If RPL_TABLE_LIST is t3->t1->t2 (where t1 and t2
          are base tables for merge table 't3'), open_tables will modify
          the list by adding t1 and t2 again immediately after t3 in the
          list (*not at the end of the list*). New table_to_lock list will
          look like t3->t1'->t2'->t1->t2 (where t1' and t2' are TABLE_LIST
          objects added by open_tables() call). There is no flag(or logic) in
          open_tables() that can skip adding these base tables to the list.
          So the logic here should take care of skipping them.

          tables_to_lock_count logic will take care of skipping base tables
          that are added at the end of the list.
          For eg: If RPL_TABLE_LIST is t1->t2->t3, open_tables will modify
          the list into t1->t2->t3->t1'->t2'. t1' and t2' will be skipped
          because tables_to_lock_count logic in this for loop.
        */
        if (table_list_ptr->parent_l)
          continue;
        /*
          We can use a down cast here since we know that every table added
          to the tables_to_lock is a RPL_TABLE_LIST (or child table which is
          skipped above).
        */
        RPL_TABLE_LIST *ptr= static_cast<RPL_TABLE_LIST*>(table_list_ptr);
        DBUG_ASSERT(ptr->m_tabledef_valid);
        TABLE *conv_table;
        if (!ptr->m_tabledef.compatible_with(thd, const_cast<Relay_log_info*>(rli),
                                             ptr->table, &conv_table))
        {
          DBUG_PRINT("debug", ("Table: %s.%s is not compatible with master",
                               ptr->table->s->db.str,
                               ptr->table->s->table_name.str));
          if (thd->is_slave_error)
          {
            const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
            DBUG_RETURN(ERR_BAD_TABLE_DEF);
          }
          else
          {
            thd->get_stmt_da()->reset_condition_info(thd);
            clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
            error= 0;
            goto end;
          }
        }
        DBUG_PRINT("debug", ("Table: %s.%s is compatible with master"
                             " - conv_table: %p",
                             ptr->table->s->db.str,
                             ptr->table->s->table_name.str, conv_table));
        ptr->m_conv_table= conv_table;
      }
    }

    /*
      ... and then we add all the tables to the table map and but keep
      them in the tables to lock list.

      We also invalidate the query cache for all the tables, since
      they will now be changed.

      TODO [/Matz]: Maybe the query cache should not be invalidated
      here? It might be that a table is not changed, even though it
      was locked for the statement.  We do know that each
      Rows_log_event contain at least one row, so after processing one
      Rows_log_event, we can invalidate the query cache for the
      associated table.
     */
    TABLE_LIST *ptr= rli->tables_to_lock;
    for (uint i=0 ;  ptr && (i < rli->tables_to_lock_count); ptr= ptr->next_global, i++)
    {
      /*
        Please see comment in above 'for' loop to know the reason
        for this if condition
      */
      if (ptr->parent_l)
        continue;
      const_cast<Relay_log_info*>(rli)->m_table_map.set_table(ptr->table_id, ptr->table);
    }

    query_cache.invalidate_locked_for_write(rli->tables_to_lock);
  }

  table=
    m_table= const_cast<Relay_log_info*>(rli)->m_table_map.get_table(m_table_id);

  DBUG_PRINT("debug", ("m_table: 0x%lx, m_table_id: %llu", (ulong) m_table,
                       m_table_id.id()));

  /*
    A row event comprising of a P_S table
    - should not be replicated (i.e executed) by the slave SQL thread.
    - should not be executed by the client in the  form BINLOG '...' stmts.
  */
  if (table && table->s->table_category == TABLE_CATEGORY_PERFORMANCE)
    table= NULL;

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
    thd->set_time(&(common_header->when));

    thd->binlog_row_event_extra_data = m_extra_row_data;

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

    DBUG_PRINT_BITSET("debug", "Setting table's read_set from: %s", &m_cols);

    bitmap_set_all(table->read_set);
    if (get_general_type_code() == binary_log::DELETE_ROWS_EVENT ||
        get_general_type_code() == binary_log::UPDATE_ROWS_EVENT)
        bitmap_intersect(table->read_set,&m_cols);

    /*
      Call mark_generated_columns() to set read_set/write_set bits of the
      virtual generated columns as required in order to get these computed.
      This is needed since all columns need to have a value in the before
      image for the record when doing the update (some storage engines will
      use this for maintaining of secondary indexes). This call is required
      even for DELETE events to set write_set bit in order to satisfy
      ASSERTs in Field_*::store functions.

      binlog_prepare_row_image() function, which will be called from
      binlogging functions (binlog_update_row() and binlog_delete_row())
      will take care of removing these spurious fields required during
      execution but not needed for binlogging. In case of inserts, there
      are no spurious fields (all generated columns are required to be written
      into the binlog).
    */

    if (m_table->vfield)
      m_table->mark_generated_columns(get_general_type_code() == binary_log::UPDATE_ROWS_EVENT);

    bitmap_set_all(table->write_set);

    /* WRITE ROWS EVENTS store the bitmap in m_cols instead of m_cols_ai */
    MY_BITMAP *after_image= ((get_general_type_code() == binary_log::UPDATE_ROWS_EVENT) ?
                             &m_cols_ai : &m_cols);
    bitmap_intersect(table->write_set, after_image);

    if (thd->slave_thread) // set the mode for slave
      this->rbr_exec_mode= slave_exec_mode_options;
    else //set the mode for user thread
      this->rbr_exec_mode= thd->variables.rbr_exec_mode_options;

    // Do event specific preparations
    error= do_before_row_operations(rli);

    /*
      Bug#56662 Assertion failed: next_insert_id == 0, file handler.cc
      Don't allow generation of auto_increment value when processing
      rows event by setting 'MODE_NO_AUTO_VALUE_ON_ZERO'. The exception
      to this rule happens when the auto_inc column exists on some
      extra columns on the slave. In that case, do not force
      MODE_NO_AUTO_VALUE_ON_ZERO.
    */
    sql_mode_t saved_sql_mode= thd->variables.sql_mode;
    if (!is_auto_inc_in_extra_columns())
      thd->variables.sql_mode= MODE_NO_AUTO_VALUE_ON_ZERO;

    // row processing loop

    /*
      set the initial time of this ROWS statement if it was not done
      before in some other ROWS event.
     */
    const_cast<Relay_log_info*>(rli)->set_row_stmt_start_timestamp();

    const uchar *saved_m_curr_row= m_curr_row;

    int (Rows_log_event::*do_apply_row_ptr)(Relay_log_info const *)= NULL;

    /**
       Skip update rows events that don't have data for this slave's
       table.
     */
    if ((get_general_type_code() == binary_log::UPDATE_ROWS_EVENT) &&
        !is_any_column_signaled_for_table(table, &m_cols_ai))
      goto AFTER_MAIN_EXEC_ROW_LOOP;

    /**
       If there are no columns marked in the read_set for this table,
       that means that we cannot lookup any row using the available BI
       in the binarr log. Thence, we immediatly raise an error:
       HA_ERR_END_OF_FILE.
     */

    if ((m_rows_lookup_algorithm != ROW_LOOKUP_NOT_NEEDED) &&
        !is_any_column_signaled_for_table(table, &m_cols))
    {
      error= HA_ERR_END_OF_FILE;
      goto AFTER_MAIN_EXEC_ROW_LOOP;
    }
    switch (m_rows_lookup_algorithm)
    {
      case ROW_LOOKUP_HASH_SCAN:
        do_apply_row_ptr= &Rows_log_event::do_hash_scan_and_update;
        break;

      case ROW_LOOKUP_INDEX_SCAN:
        do_apply_row_ptr= &Rows_log_event::do_index_scan_and_update;
        break;

      case ROW_LOOKUP_TABLE_SCAN:
        do_apply_row_ptr= &Rows_log_event::do_table_scan_and_update;
        break;

      case ROW_LOOKUP_NOT_NEEDED:
        DBUG_ASSERT(get_general_type_code() == binary_log::WRITE_ROWS_EVENT);

        /* No need to scan for rows, just apply it */
        do_apply_row_ptr= &Rows_log_event::do_apply_row;
        break;

      default:
        DBUG_ASSERT(0);
        error= 1;
        goto AFTER_MAIN_EXEC_ROW_LOOP;
        break;
    }

    do {

      error= (this->*do_apply_row_ptr)(rli);

      if (handle_idempotent_and_ignored_errors(rli, &error))
        break;

      /* this advances m_curr_row */
      do_post_row_operations(rli, error);

    } while (!error && (m_curr_row != m_rows_end));

AFTER_MAIN_EXEC_ROW_LOOP:

    if (saved_m_curr_row != m_curr_row && !table->file->has_transactions())
    {
      /*
        Usually, the trans_commit_stmt() propagates unsafe_rollback_flags
        from statement to transaction level. However, we cannot rely on
        this when row format is in use as several events can be processed
        before calling this function. This happens because it is called
        only when the latest event generated by a statement is processed.

        There are however upper level functions that execute per event
        and check transaction's status. So if the unsafe_rollback_flags
        are not propagated here, this can lead to errors.

        For example, a transaction that updates non-transactional tables
        may be stopped in the middle thus leading to inconsistencies
        after a restart.
      */
      thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);
      thd->get_transaction()->merge_unsafe_rollback_flags();
    }

    /*
      Restore the sql_mode after the rows event is processed.
    */
    thd->variables.sql_mode= saved_sql_mode;

    {/*
         The following failure injecion works in cooperation with tests
         setting @@global.debug= 'd,stop_slave_middle_group'.
         The sql thread receives the killed status and will proceed
         to shutdown trying to finish incomplete events group.
     */
      DBUG_EXECUTE_IF("stop_slave_middle_group",
                      if (thd->get_transaction()->cannot_safely_rollback(
                          Transaction_ctx::SESSION))
                        const_cast<Relay_log_info*>(rli)->abort_slave= 1;);
    }

    if ((error= do_after_row_operations(rli, error)) &&
        ignored_error_code(convert_handler_error(error, thd, table)))
    {
      slave_rows_error_report(INFORMATION_LEVEL, error, rli, thd, table,
                              get_type_str(),
                              const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                              (ulong) common_header->log_pos);
      thd->get_stmt_da()->reset_condition_info(thd);
      clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      error= 0;
    }
  } // if (table)

  if (error)
  {
    slave_rows_error_report(ERROR_LEVEL, error, rli, thd, table,
                            get_type_str(),
                            const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                            (ulong) common_header->log_pos);
    /*
      @todo We should probably not call
      reset_current_stmt_binlog_format_row() from here.

      Note: this applies to log_event_old.cc too.
      /Sven
    */
    thd->reset_current_stmt_binlog_format_row();
    thd->is_slave_error= 1;
    DBUG_RETURN(error);
  }

end:
  if (get_flags(STMT_END_F))
  {
    if((error= rows_event_stmt_cleanup(rli, thd)))
    {
      if (table)
        slave_rows_error_report(ERROR_LEVEL,
                                thd->is_error() ? 0 : error,
                                rli, thd, table,
                                get_type_str(),
                                const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                                (ulong) common_header->log_pos);
      else
      {
        rli->report(ERROR_LEVEL,
                    thd->is_error() ? thd->get_stmt_da()->mysql_errno() : error,
                    "Error in cleaning up after an event of type:%s; %s; the group"
                    " log file/position: %s %lu", get_type_str(),
                    thd->is_error() ? thd->get_stmt_da()->message_text() :
                    "unexpected error",
                    const_cast<Relay_log_info*>(rli)->get_rpl_log_name(),
                    (ulong) common_header->log_pos);
      }
    }
   /* We are at end of the statement (STMT_END_F flag), lets clean
     the memory which was used from thd's mem_root now.
     This needs to be done only if we are here in SQL thread context.
     In other flow ( in case of a regular thread which can happen
     when the thread is applying BINLOG'...' row event) we should
     *not* try to free the memory here. It will be done latter
     in dispatch_command() after command execution is completed.
    */
   if (thd->slave_thread)
     free_root(thd->mem_root, MYF(MY_KEEP_PREALLOC));
  }
  DBUG_RETURN(error);
}

Log_event::enum_skip_reason
Rows_log_event::do_shall_skip(Relay_log_info *rli)
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

/**
   The function is called at Rows_log_event statement commit time,
   normally from Rows_log_event::do_update_pos() and possibly from
   Query_log_event::do_apply_event() of the COMMIT.
   The function commits the last statement for engines, binlog and
   releases resources have been allocated for the statement.

   @retval  0         Ok.
   @retval  non-zero  Error at the commit.
 */

static int rows_event_stmt_cleanup(Relay_log_info const *rli, THD * thd)
{
  DBUG_EXECUTE_IF("simulate_rows_event_cleanup_failure",
                  {
                  my_error(ER_ERROR_DURING_COMMIT, MYF(0), 1);
                  return (1);
                  });
  int error;
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
    error= thd->binlog_flush_pending_rows_event(TRUE);

    /*
      If this event is not in a transaction, the call below will, if some
      transactional storage engines are involved, commit the statement into
      them and flush the pending event to binlog.
      If this event is in a transaction, the call will do nothing, but a
      Xid_log_event will come next which will, if some transactional engines
      are involved, commit the transaction and flush the pending event to the
      binlog.
      If there was a deadlock the transaction should have been rolled back
      already. So there should be no need to rollback the transaction.
    */
    DBUG_ASSERT(! thd->transaction_rollback_request);
    error|= (error ? trans_rollback_stmt(thd) : trans_commit_stmt(thd));

    /*
      Now what if this is not a transactional engine? we still need to
      flush the pending event to the binlog; we did it with
      thd->binlog_flush_pending_rows_event(). Note that we imitate
      what is done for real queries: a call to
      ha_autocommit_or_rollback() (sometimes only if involves a
      transactional engine), and a call to be sure to have the pending
      event flushed.
    */

    /*
      @todo We should probably not call
      reset_current_stmt_binlog_format_row() from here.

      Note: this applies to log_event_old.cc too

      Btw, the previous comment about transactional engines does not
      seem related to anything that happens here.
      /Sven
    */
    thd->reset_current_stmt_binlog_format_row();

    const_cast<Relay_log_info*>(rli)->cleanup_context(thd, 0);

    /*
      Clean sql_command value
    */
    thd->lex->sql_command= SQLCOM_END;

  }
  return error;
}

/**
   The method either increments the relay log position or
   commits the current statement and increments the master group
   possition if the event is STMT_END_F flagged and
   the statement corresponds to the autocommit query (i.e replicated
   without wrapping in BEGIN/COMMIT)

   @retval 0         Success
   @retval non-zero  Error in the statement commit
 */
int
Rows_log_event::do_update_pos(Relay_log_info *rli)
{
  DBUG_ENTER("Rows_log_event::do_update_pos");
  int error= 0;

  DBUG_PRINT("info", ("flags: %s",
                      get_flags(STMT_END_F) ? "STMT_END_F " : ""));

  /* Worker does not execute binlog update position logics */
  DBUG_ASSERT(!is_mts_worker(rli->info_thd));

  if (get_flags(STMT_END_F))
  {
    /*
      Indicate that a statement is finished.
      Step the group log position if we are not in a transaction,
      otherwise increase the event log position.
    */
    error= rli->stmt_done(common_header->log_pos);
  }
  else
  {
    rli->inc_event_relay_log_pos();
  }

  DBUG_RETURN(error);
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifndef MYSQL_CLIENT
bool Rows_log_event::write_data_header(IO_CACHE *file)
{
  uchar buf[Binary_log_event::ROWS_HEADER_LEN_V2];	// No need to init the buffer
  DBUG_ASSERT(m_table_id.is_valid());
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, (ulong) m_table_id.id());
                    int2store(buf + 4, m_flags);
                    return (wrapper_my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + ROWS_MAPID_OFFSET, m_table_id.id());
  int2store(buf + ROWS_FLAGS_OFFSET, m_flags);
  int rc = 0;
  if (likely(!log_bin_use_v1_row_events))
  {
    /*
       v2 event, with variable header portion.
       Determine length of variable header payload
    */
    uint16 vhlen= 2;
    uint16 vhpayloadlen= 0;
    uint16 extra_data_len= 0;
    if (m_extra_row_data)
    {
      extra_data_len= m_extra_row_data[EXTRA_ROW_INFO_LEN_OFFSET];
      vhpayloadlen= ROWS_V_TAG_LEN + extra_data_len;
    }

    /* Var-size header len includes len itself */
    int2store(buf + ROWS_VHLEN_OFFSET, vhlen + vhpayloadlen);
    rc= wrapper_my_b_safe_write(file, buf, Binary_log_event::ROWS_HEADER_LEN_V2);

    /* Write var-sized payload, if any */
    if ((vhpayloadlen > 0) &&
        (rc == 0))
    {
      /* Add tag and extra row info */
      uchar type_code= ROWS_V_EXTRAINFO_TAG;
      rc= wrapper_my_b_safe_write(file, &type_code, ROWS_V_TAG_LEN);
      if (rc==0)
        rc= wrapper_my_b_safe_write(file, m_extra_row_data, extra_data_len);
    }
  }
  else
  {
    rc= wrapper_my_b_safe_write(file, buf, Binary_log_event::ROWS_HEADER_LEN_V1);
  }

  return (rc != 0);
}

bool Rows_log_event::write_data_body(IO_CACHE*file)
{
  /*
     Note that this should be the number of *bits*, not the number of
     bytes.
  */
  uchar sbuf[sizeof(m_width) + 1];
  my_ptrdiff_t const data_size= m_rows_cur - m_rows_buf;
  bool res= false;
  uchar *const sbuf_end= net_store_length(sbuf, (size_t) m_width);
  DBUG_ASSERT(static_cast<size_t>(sbuf_end - sbuf) <= sizeof(sbuf));

  DBUG_DUMP("m_width", sbuf, (size_t) (sbuf_end - sbuf));
  res= res || wrapper_my_b_safe_write(file, sbuf, (size_t) (sbuf_end - sbuf));

  DBUG_DUMP("m_cols", (uchar*) m_cols.bitmap, no_bytes_in_map(&m_cols));
  res= res || wrapper_my_b_safe_write(file, (uchar*) m_cols.bitmap,
                              no_bytes_in_map(&m_cols));
  /*
    TODO[refactor write]: Remove the "down cast" here (and elsewhere).
   */
  if (get_general_type_code() == binary_log::UPDATE_ROWS_EVENT)
  {
    DBUG_DUMP("m_cols_ai", (uchar*) m_cols_ai.bitmap,
              no_bytes_in_map(&m_cols_ai));
    res= res || wrapper_my_b_safe_write(file, (uchar*) m_cols_ai.bitmap,
                                no_bytes_in_map(&m_cols_ai));
  }
  DBUG_DUMP("rows", m_rows_buf, data_size);
  res= res || wrapper_my_b_safe_write(file, m_rows_buf, (size_t) data_size);

  return res;

}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rows_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  char const *const flagstr=
    get_flags(STMT_END_F) ? " flags: STMT_END_F" : "";
  size_t bytes= my_snprintf(buf, sizeof(buf),
                            "table_id: %llu%s", m_table_id.id(), flagstr);
  protocol->store(buf, bytes, &my_charset_bin);
  return 0;
}
#endif

#ifdef MYSQL_CLIENT
void Rows_log_event::print_helper(FILE *file,
                                  PRINT_EVENT_INFO *print_event_info,
                                  char const *const name)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  IO_CACHE *const body= &print_event_info->body_cache;
  if (!print_event_info->short_form)
  {
    bool const last_stmt_event= get_flags(STMT_END_F);
    print_header(head, print_event_info, !last_stmt_event);
    my_b_printf(head, "\t%s: table id %llu%s\n",
                name, m_table_id.id(),
                last_stmt_event ? " flags: STMT_END_F" : "");
    print_base64(body, print_event_info, !last_stmt_event);
  }
}
#endif

/**************************************************************************
	Table_map_log_event member functions and support functions
**************************************************************************/

/**
  @page How replication of field metadata works.
  
  When a table map is created, the master first calls 
  Table_map_log_event::save_field_metadata() which calculates how many 
  values will be in the field metadata. Only those fields that require the 
  extra data are added. The method also loops through all of the fields in 
  the table calling the method Field::save_field_metadata() which returns the
  values for the field that will be saved in the metadata and replicated to
  the slave. Once all fields have been processed, the table map is written to
  the binlog adding the size of the field metadata and the field metadata to
  the end of the body of the table map.

  When a table map is read on the slave, the field metadata is read from the 
  table map and passed to the table_def class constructor which saves the 
  field metadata from the table map into an array based on the type of the 
  field. Field metadata values not present (those fields that do not use extra 
  data) in the table map are initialized as zero (0). The array size is the 
  same as the columns for the table on the slave.

  Additionally, values saved for field metadata on the master are saved as a 
  string of bytes (uchar) in the binlog. A field may require 1 or more bytes
  to store the information. In cases where values require multiple bytes 
  (e.g. values > 255), the endian-safe methods are used to properly encode 
  the values on the master and decode them on the slave. When the field
  metadata values are captured on the slave, they are stored in an array of
  type uint16. This allows the least number of casts to prevent casting bugs
  when the field metadata is used in comparisons of field attributes. When
  the field metadata is used for calculating addresses in pointer math, the
  type used is uint32. 
*/

#if !defined(MYSQL_CLIENT)
/**
  Save the field metadata based on the real_type of the field.
  The metadata saved depends on the type of the field. Some fields
  store a single byte for pack_length() while others store two bytes
  for field_length (max length).
  
  @retval  0  Ok.

  @todo
  We may want to consider changing the encoding of the information.
  Currently, the code attempts to minimize the number of bytes written to 
  the tablemap. There are at least two other alternatives; 1) using 
  net_store_length() to store the data allowing it to choose the number of
  bytes that are appropriate thereby making the code much easier to 
  maintain (only 1 place to change the encoding), or 2) use a fixed number
  of bytes for each field. The problem with option 1 is that net_store_length()
  will use one byte if the value < 251, but 3 bytes if it is > 250. Thus,
  for fields like CHAR which can be no larger than 255 characters, the method
  will use 3 bytes when the value is > 250. Further, every value that is
  encoded using 2 parts (e.g., pack_length, field_length) will be numerically
  > 250 therefore will use 3 bytes for eah value. The problem with option 2
  is less wasteful for space but does waste 1 byte for every field that does
  not encode 2 parts. 
*/
int Table_map_log_event::save_field_metadata()
{
  DBUG_ENTER("Table_map_log_event::save_field_metadata");
  int index= 0;
  for (unsigned int i= 0; i < m_table->s->fields ; i++)
  {
    DBUG_PRINT("debug", ("field_type: %d", m_coltype[i]));
    index+= m_table->s->field[i]->save_field_metadata(&m_field_metadata[index]);
  }
  DBUG_RETURN(index);
}
#endif /* !defined(MYSQL_CLIENT) */

/*
  Constructor used to build an event for writing to the binary log.
  Mats says tbl->s lives longer than this event so it's ok to copy pointers
  (tbl->s->db etc) and not pointer content.
 */
#if !defined(MYSQL_CLIENT)
Table_map_log_event::Table_map_log_event(THD *thd_arg, TABLE *tbl,
                                         const Table_id& tid,
                                         bool using_trans)
  : binary_log::Table_map_event(tid, tbl->s->fields, (tbl->s->db.str),
                                ((tbl->s->db.str) ? tbl->s->db.length : 0),
                                (tbl->s->table_name.str),
                                (tbl->s->table_name.length)),
    Log_event(thd_arg, 0,
              using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
                            Log_event::EVENT_STMT_CACHE,
              Log_event::EVENT_NORMAL_LOGGING,
              header(), footer())
{
  common_header->type_code= binary_log::TABLE_MAP_EVENT;
  m_table= tbl;
  m_flags= TM_BIT_LEN_EXACT_F;

  uchar cbuf[sizeof(m_colcnt) + 1];
  uchar *cbuf_end;
  DBUG_ASSERT(m_table_id.is_valid());
  /*
    In TABLE_SHARE, "db" and "table_name" are 0-terminated (see this comment in
    table.cc / alloc_table_share():
      Use the fact the key is db/0/table_name/0
    As we rely on this let's assert it.
  */
  DBUG_ASSERT((tbl->s->db.str == 0) ||
              (tbl->s->db.str[tbl->s->db.length] == 0));
  DBUG_ASSERT(tbl->s->table_name.str[tbl->s->table_name.length] == 0);


  m_data_size=  Binary_log_event::TABLE_MAP_HEADER_LEN;
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master", m_data_size= 6;);
  m_data_size+= m_dblen + 2;	// Include length and terminating \0
  m_data_size+= m_tbllen + 2;	// Include length and terminating \0
  cbuf_end= net_store_length(cbuf, (size_t) m_colcnt);
  DBUG_ASSERT(static_cast<size_t>(cbuf_end - cbuf) <= sizeof(cbuf));
  m_data_size+= (cbuf_end - cbuf) + m_colcnt;	// COLCNT and column types

  m_coltype= (uchar *)my_malloc(key_memory_log_event,
                                m_colcnt, MYF(MY_WME));

  DBUG_ASSERT(m_colcnt == m_table->s->fields);
  for (unsigned int i= 0; i < m_table->s->fields; ++i)
    m_coltype[i]= m_table->field[i]->binlog_type();


  /*
    Calculate a bitmap for the results of maybe_null() for all columns.
    The bitmap is used to determine when there is a column from the master
    that is not on the slave and is null and thus not in the row data during
    replication.
  */
  uint num_null_bytes= (m_table->s->fields + 7) / 8;
  m_data_size+= num_null_bytes;
  /*
    m_null_bits is a pointer indicating which columns can have a null value
    in a particular table.
  */
  m_null_bits= (uchar *)my_malloc(key_memory_log_event,
                                  num_null_bytes, MYF(MY_WME));

  m_field_metadata= (uchar*)my_malloc(key_memory_log_event,
                                      (m_colcnt * 2), MYF(MY_WME));
  memset(m_field_metadata, 0, (m_colcnt * 2));

  if (m_null_bits != NULL && m_field_metadata != NULL && m_coltype != NULL)
    is_valid_param= true;
  /*
    Create an array for the field metadata and store it.
  */
  m_field_metadata_size= save_field_metadata();
  DBUG_ASSERT(m_field_metadata_size <= (m_colcnt * 2));

  /*
    Now set the size of the data to the size of the field metadata array
    plus one or three bytes (see pack.c:net_store_length) for number of 
    elements in the field metadata array.
  */
  if (m_field_metadata_size < 251)
    m_data_size+= m_field_metadata_size + 1; 
  else
    m_data_size+= m_field_metadata_size + 3; 

  memset(m_null_bits, 0, num_null_bytes);
  for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
    if (m_table->field[i]->maybe_null())
      m_null_bits[(i / 8)]+= 1 << (i % 8);
  /*
    Marking event to require sequential execution in MTS
    if the query might have updated FK-referenced db.
    Unlike Query_log_event where this fact is encoded through 
    the accessed db list in the Table_map case m_flags is exploited.
  */
  uchar dbs= thd_arg->get_binlog_accessed_db_names() ?
    thd_arg->get_binlog_accessed_db_names()->elements : 0;
  if (dbs == 1)
  {
    char *db_name= thd_arg->get_binlog_accessed_db_names()->head();
    if (!strcmp(db_name, ""))
      m_flags |= TM_REFERRED_FK_DB_F;
  }
}
#endif /* !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#if defined(HAVE_REPLICATION)
Table_map_log_event::Table_map_log_event(const char *buf, uint event_len,
                                         const Format_description_event
                                         *description_event)

   : binary_log::Table_map_event(buf, event_len, description_event),
     Log_event(header(), footer())
#ifndef MYSQL_CLIENT
    ,m_table(NULL)
#endif
{
  DBUG_ENTER("Table_map_log_event::Table_map_log_event(const char*,uint,...)");
  if (m_null_bits != NULL && m_field_metadata != NULL && m_coltype != NULL)
    is_valid_param= true;
  DBUG_ASSERT(header()->type_code == binary_log::TABLE_MAP_EVENT);
  DBUG_VOID_RETURN;
}
#endif

Table_map_log_event::~Table_map_log_event()
{
  if(m_null_bits)
  {
    my_free(m_null_bits);
    m_null_bits= NULL;
  }
  if(m_field_metadata)
  {
    my_free(m_field_metadata);
    m_field_metadata= NULL;
  }
}

/*
  Return value is an error code, one of:

      -1     Failure to open table   [from open_tables()]
       0     Success
       1     No room for more tables [from set_table()]
       2     Out of memory           [from set_table()]
       3     Wrong table definition
       4     Daisy-chaining RBR with SBR not possible
 */

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

enum enum_tbl_map_status
{
  /* no duplicate identifier found */
  OK_TO_PROCESS= 0,

  /* this table map must be filtered out */
  FILTERED_OUT= 1,

  /* identifier mapping table with different properties */
  SAME_ID_MAPPING_DIFFERENT_TABLE= 2,
  
  /* a duplicate identifier was found mapping the same table */
  SAME_ID_MAPPING_SAME_TABLE= 3,

  /*
    this table must be filtered out but found an active XA transaction. XA
    transactions shouldn't be used with replication filters, until disabling
    the XA read only optimization is a supported feature.
  */
  FILTERED_WITH_XA_ACTIVE = 4
};

/*
  Checks if this table map event should be processed or not. First
  it checks the filtering rules, and then looks for duplicate identifiers
  in the existing list of rli->tables_to_lock.

  It checks that there hasn't been any corruption by verifying that there
  are no duplicate entries with different properties.

  In some cases, some binary logs could get corrupted, showing several
  tables mapped to the same table_id, 0 (see: BUG#56226). Thus we do this
  early sanity check for such cases and avoid that the server crashes 
  later.

  In some corner cases, the master logs duplicate table map events, i.e.,
  same id, same database name, same table name (see: BUG#37137). This is
  different from the above as it's the same table that is mapped again 
  to the same identifier. Thus we cannot just check for same ids and 
  assume that the event is corrupted we need to check every property. 

  NOTE: in the event that BUG#37137 ever gets fixed, this extra check 
        will still be valid because we would need to support old binary 
        logs anyway.

  @param rli The relay log info reference.
  @param table_list A list element containing the table to check against.
  @return OK_TO_PROCESS 
            if there was no identifier already in rli->tables_to_lock 
            
          FILTERED_OUT
            if the event is filtered according to the filtering rules

          SAME_ID_MAPPING_DIFFERENT_TABLE 
            if the same identifier already maps a different table in 
            rli->tables_to_lock

          SAME_ID_MAPPING_SAME_TABLE 
            if the same identifier already maps the same table in 
            rli->tables_to_lock.
*/
static enum_tbl_map_status
check_table_map(Relay_log_info const *rli, RPL_TABLE_LIST *table_list)
{
  DBUG_ENTER("check_table_map");
  enum_tbl_map_status res= OK_TO_PROCESS;

  if (rli->info_thd->slave_thread /* filtering is for slave only */ &&
      (!rpl_filter->db_ok(table_list->db) ||
       (rpl_filter->is_on() && !rpl_filter->tables_ok("", table_list))))
    if (rli->info_thd->get_transaction()->xid_state()->has_state(
            XID_STATE::XA_ACTIVE))
      res = FILTERED_WITH_XA_ACTIVE;
    else
      res = FILTERED_OUT;
  else
  {
    RPL_TABLE_LIST *ptr= static_cast<RPL_TABLE_LIST*>(rli->tables_to_lock);
    for(uint i=0 ; ptr && (i< rli->tables_to_lock_count); 
        ptr= static_cast<RPL_TABLE_LIST*>(ptr->next_local), i++)
    {
      if (ptr->table_id == table_list->table_id)
      {

        if (strcmp(ptr->db, table_list->db) || 
            strcmp(ptr->alias, table_list->table_name) || 
            ptr->lock_type != TL_WRITE) // the ::do_apply_event always sets TL_WRITE
          res= SAME_ID_MAPPING_DIFFERENT_TABLE;
        else
          res= SAME_ID_MAPPING_SAME_TABLE;

        break;
      }
    }
  }

  DBUG_PRINT("debug", ("check of table map ended up with: %u", res));

  DBUG_RETURN(res);
}

int Table_map_log_event::do_apply_event(Relay_log_info const *rli)
{
  RPL_TABLE_LIST *table_list;
  char *db_mem, *tname_mem, *ptr;
  size_t dummy_len;
  void *memory;
  DBUG_ENTER("Table_map_log_event::do_apply_event(Relay_log_info*)");
  DBUG_ASSERT(rli->info_thd == thd);

  /* Step the query id to mark what columns that are actually used. */
  thd->set_query_id(next_query_id());

  if (!(memory= my_multi_malloc(key_memory_log_event,
                                MYF(MY_WME),
                                &table_list, sizeof(RPL_TABLE_LIST),
                                &db_mem, (uint) NAME_LEN + 1,
                                &tname_mem, (uint) NAME_LEN + 1,
                                NullS)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  my_stpcpy(db_mem, m_dbnam.c_str());
  my_stpcpy(tname_mem, m_tblnam.c_str());

  if (lower_case_table_names)
  {
    my_casedn_str(system_charset_info, db_mem);
    my_casedn_str(system_charset_info, tname_mem);
  }

  /* rewrite rules changed the database */
  if (((ptr= (char*) rpl_filter->get_rewrite_db(db_mem, &dummy_len)) != db_mem))
    my_stpcpy(db_mem, ptr);

  table_list->init_one_table(db_mem, strlen(db_mem),
                             tname_mem, strlen(tname_mem),
                             tname_mem, TL_WRITE);

  table_list->table_id=
    DBUG_EVALUATE_IF("inject_tblmap_same_id_maps_diff_table", 0, m_table_id.id());
  table_list->updating= 1;
  table_list->required_type= FRMTYPE_TABLE;
  DBUG_PRINT("debug", ("table: %s is mapped to %llu", table_list->table_name,
                       table_list->table_id.id()));

  enum_tbl_map_status tblmap_status= check_table_map(rli, table_list);
  if (tblmap_status == OK_TO_PROCESS)
  {
    DBUG_ASSERT(thd->lex->query_tables != table_list);

    /*
      Use placement new to construct the table_def instance in the
      memory allocated for it inside table_list.

      The memory allocated by the table_def structure (i.e., not the
      memory allocated *for* the table_def structure) is released
      inside Relay_log_info::clear_tables_to_lock() by calling the
      table_def destructor explicitly.
    */
    new (&table_list->m_tabledef)
        table_def(m_coltype, m_colcnt,
                  m_field_metadata, m_field_metadata_size,
                  m_null_bits, m_flags);

    table_list->m_tabledef_valid= TRUE;
    table_list->m_conv_table= NULL;
    table_list->open_type= OT_BASE_ONLY;

    /*
      We record in the slave's information that the table should be
      locked by linking the table into the list of tables to lock.
    */
    table_list->next_global= table_list->next_local= rli->tables_to_lock;
    const_cast<Relay_log_info*>(rli)->tables_to_lock= table_list;
    const_cast<Relay_log_info*>(rli)->tables_to_lock_count++;
    /* 'memory' is freed in clear_tables_to_lock */
  }
  else  // FILTERED_OUT, SAME_ID_MAPPING_*
  {
    if (tblmap_status == FILTERED_WITH_XA_ACTIVE)
    {
      if (thd->slave_thread)
        rli->report(ERROR_LEVEL, ER_XA_REPLICATION_FILTERS,
                    "%s", ER_THD(thd, ER_XA_REPLICATION_FILTERS));
      else
        /*
          For the cases in which a 'BINLOG' statement is set to
          execute in a user session
         */
        my_printf_error(ER_XA_REPLICATION_FILTERS,
                        "%s", MYF(0), ER_THD(thd, ER_XA_REPLICATION_FILTERS));
    }
    /*
      If mapped already but with different properties, we raise an
      error.
      If mapped already but with same properties we skip the event.
      If filtered out we skip the event.

      In all three cases, we need to free the memory previously 
      allocated.
     */
    else if (tblmap_status == SAME_ID_MAPPING_DIFFERENT_TABLE)
    {
      /*
        Something bad has happened. We need to stop the slave as strange things
        could happen if we proceed: slave crash, wrong table being updated, ...
        As a consequence we push an error in this case.
       */

      char buf[256];

      my_snprintf(buf, sizeof(buf), 
                  "Found table map event mapping table id %llu which "
                  "was already mapped but with different settings.",
                  table_list->table_id.id());

      if (thd->slave_thread)
        rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                    ER(ER_SLAVE_FATAL_ERROR), buf);
      else
        /* 
          For the cases in which a 'BINLOG' statement is set to 
          execute in a user session 
         */
        my_printf_error(ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR), 
                        MYF(0), buf);
    } 
    
    my_free(memory);
  }

  DBUG_RETURN(tblmap_status == SAME_ID_MAPPING_DIFFERENT_TABLE);
}

Log_event::enum_skip_reason
Table_map_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1, then we should not start executing
    on the next event.
  */
  return continue_group(rli);
}

int Table_map_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifndef MYSQL_CLIENT
bool Table_map_log_event::write_data_header(IO_CACHE *file)
{
  DBUG_ASSERT(m_table_id.is_valid());
  uchar buf[Binary_log_event::TABLE_MAP_HEADER_LEN];
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, static_cast<uint32>(m_table_id.id()));
                    int2store(buf + 4, m_flags);
                    return (wrapper_my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + TM_MAPID_OFFSET, m_table_id.id());
  int2store(buf + TM_FLAGS_OFFSET, m_flags);
  return (wrapper_my_b_safe_write(file, buf, Binary_log_event::TABLE_MAP_HEADER_LEN));
}

bool Table_map_log_event::write_data_body(IO_CACHE *file)
{
  DBUG_ASSERT(!m_dbnam.empty());
  DBUG_ASSERT(!m_tblnam.empty());
  /* We use only one byte per length for storage in event: */
  DBUG_ASSERT(m_dblen <= 128);
  DBUG_ASSERT(m_tbllen <= 128);

  uchar const dbuf[]= { (uchar) m_dblen };
  uchar const tbuf[]= { (uchar) m_tbllen };

  uchar cbuf[sizeof(m_colcnt) + 1];
  uchar *const cbuf_end= net_store_length(cbuf, (size_t) m_colcnt);
  DBUG_ASSERT(static_cast<size_t>(cbuf_end - cbuf) <= sizeof(cbuf));

  /*
    Store the size of the field metadata.
  */
  uchar mbuf[sizeof(m_field_metadata_size)];
  uchar *const mbuf_end= net_store_length(mbuf, m_field_metadata_size);

  return (wrapper_my_b_safe_write(file, dbuf, sizeof(dbuf)) ||
          wrapper_my_b_safe_write(file,
                                  (const uchar*)m_dbnam.c_str(),
                                  m_dblen+1) ||
          wrapper_my_b_safe_write(file, tbuf, sizeof(tbuf)) ||
          wrapper_my_b_safe_write(file,
                                 (const uchar*)m_tblnam.c_str(),
                                  m_tbllen+1) ||
          wrapper_my_b_safe_write(file, cbuf, (size_t) (cbuf_end - cbuf)) ||
          wrapper_my_b_safe_write(file, m_coltype, m_colcnt) ||
          wrapper_my_b_safe_write(file, mbuf, (size_t) (mbuf_end - mbuf)) ||
          wrapper_my_b_safe_write(file,
                                  m_field_metadata, m_field_metadata_size),
          wrapper_my_b_safe_write(file, m_null_bits, (m_colcnt + 7) / 8));
 }
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/*
  Print some useful information for the SHOW BINARY LOG information
  field.
 */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Table_map_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  size_t bytes= my_snprintf(buf, sizeof(buf),
                            "table_id: %llu (%s.%s)",
                            m_table_id.id(), m_dbnam.c_str(), m_tblnam.c_str());
  protocol->store(buf, bytes, &my_charset_bin);
  return 0;
}
#endif


#endif


#ifdef MYSQL_CLIENT
void Table_map_log_event::print(FILE *, PRINT_EVENT_INFO *print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(&print_event_info->head_cache, print_event_info, TRUE);
    my_b_printf(&print_event_info->head_cache,
                "\tTable_map: `%s`.`%s` mapped to number %llu\n",
                m_dbnam.c_str(), m_tblnam.c_str(), m_table_id.id());
    print_base64(&print_event_info->body_cache, print_event_info, TRUE);
  }
}
#endif

/**************************************************************************
	Write_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Write_rows_log_event::Write_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                           const Table_id& tid_arg,
                                           bool is_transactional,
                                           const uchar* extra_row_info)
: binary_log::Rows_event(m_type),
  Rows_log_event(thd_arg, tbl_arg, tid_arg, tbl_arg->write_set, is_transactional,
                   log_bin_use_v1_row_events?
                   binary_log::WRITE_ROWS_EVENT_V1:
                   binary_log::WRITE_ROWS_EVENT,
                   extra_row_info)
{
  common_header->type_code= m_type;
}
#endif

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Write_rows_log_event::Write_rows_log_event(const char *buf, uint event_len,
                                           const Format_description_event
                                           *description_event)
: binary_log::Rows_event(buf, event_len, description_event),
  Rows_log_event(buf, event_len, description_event),
  binary_log::Write_rows_event(buf, event_len, description_event)
{
  DBUG_ASSERT(header()->type_code == m_type);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int
Write_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  int error= 0;

  /*
    Increment the global status insert count variable
  */
  if (get_flags(STMT_END_F))
    thd->status_var.com_stat[SQLCOM_INSERT]++;

  /*
    Let storage engines treat this event as an INSERT command.

    Set 'sql_command' as SQLCOM_INSERT after the tables are locked.
    When locking the tables, it should be SQLCOM_END.
    THD::decide_logging_format which is called from "lock tables"
    assumes that row_events will have 'sql_command' as SQLCOM_END.
  */
  thd->lex->sql_command= SQLCOM_INSERT;

  /**
     todo: to introduce a property for the event (handler?) which forces
     applying the event in the replace (idempotent) fashion.
  */
  if ((rbr_exec_mode == RBR_EXEC_MODE_IDEMPOTENT) ||
      (m_table->s->db_type()->db_type == DB_TYPE_NDBCLUSTER))
  {
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
  }

 
  /* Honor next number column if present */
  m_table->next_number_field= m_table->found_next_number_field;
  /*
   * Fixed Bug#45999, In RBR, Store engine of Slave auto-generates new
   * sequence numbers for auto_increment fields if the values of them are 0.
   * If generateing a sequence number is decided by the values of
   * table->auto_increment_field_not_null and SQL_MODE(if includes
   * MODE_NO_AUTO_VALUE_ON_ZERO) in update_auto_increment function.
   * SQL_MODE of slave sql thread is always consistency with master's.
   * In RBR, auto_increment fields never are NULL, except if the auto_inc
   * column exists only on the slave side (i.e., in an extra column
   * on the slave's table).
   */
  if (!is_auto_inc_in_extra_columns())
    m_table->auto_increment_field_not_null= TRUE;
  else
  {
    /*
      Here we have checked that there is an extra field
      on this server's table that has an auto_inc column.

      Mark that the auto_increment field is null and mark
      the read and write set bits.

      (There can only be one AUTO_INC column, it is always
       indexed and it cannot have a DEFAULT value).
    */
    m_table->auto_increment_field_not_null= FALSE;
    m_table->mark_auto_increment_column();
  }

  /**
     Sets it to ROW_LOOKUP_NOT_NEEDED.
   */
  decide_row_lookup_algorithm_and_key();
  DBUG_ASSERT(m_rows_lookup_algorithm==ROW_LOOKUP_NOT_NEEDED);

  return error;
}

int 
Write_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                              int error)
{
  int local_error= 0;

  /**
    Clear the write_set bit for auto_inc field that only
    existed on the destination table as an extra column.
   */
  if (is_auto_inc_in_extra_columns())
  {
    bitmap_clear_bit(m_table->write_set, m_table->next_number_field->field_index);
    bitmap_clear_bit( m_table->read_set, m_table->next_number_field->field_index);

    if (get_flags(STMT_END_F))
      m_table->file->ha_release_auto_increment();
  }
  m_table->next_number_field=0;
  m_table->auto_increment_field_not_null= FALSE;
  if ((rbr_exec_mode == RBR_EXEC_MODE_IDEMPOTENT) ||
      m_table->s->db_type()->db_type == DB_TYPE_NDBCLUSTER)
  {
    m_table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    m_table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    /*
      resetting the extra with 
      table->file->extra(HA_EXTRA_NO_IGNORE_NO_KEY); 
      fires bug#27077
      explanation: file->reset() performs this duty
      ultimately. Still todo: fix
    */
  }
  if ((local_error= m_table->file->ha_end_bulk_insert()))
  {
    m_table->file->print_error(local_error, MYF(0));
  }

  m_rows_lookup_algorithm= ROW_LOOKUP_UNDEFINED;

  return error? error : local_error;
}

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

/**
   Check if an error is a duplicate key error.

   This function is used to check if an error code is one of the
   duplicate key error, i.e., and error code for which it is sensible
   to do a <code>get_dup_key()</code> to retrieve the duplicate key.

   @param errcode The error code to check.

   @return <code>true</code> if the error code is such that
   <code>get_dup_key()</code> will return true, <code>false</code>
   otherwise.
 */
bool
is_duplicate_key_error(int errcode)
{
  switch (errcode)
  {
  case HA_ERR_FOUND_DUPP_KEY:
  case HA_ERR_FOUND_DUPP_UNIQUE:
    return true;
  }
  return false;
}

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
Write_rows_log_event::write_row(const Relay_log_info *const rli,
                                const bool overwrite)
{
  DBUG_ENTER("write_row");
  DBUG_ASSERT(m_table != NULL && thd != NULL);

  TABLE *table= m_table;  // pointer to event's table
  int error;
  int keynum= 0;
  char* key= NULL;

  prepare_record(table, &m_cols,
                 table->file->ht->db_type != DB_TYPE_NDBCLUSTER);

  /* unpack row into table->record[0] */
  if ((error= unpack_current_row(rli, &m_cols)))
    DBUG_RETURN(error);

  /*
    When m_curr_row == m_curr_row_end, it means a row that contains nothing,
    so all the pointers shall be pointing to the same address, or else
    we have corrupt data and shall throw the error.
  */
  DBUG_PRINT("debug",("m_rows_buf= %p, m_rows_cur= %p, m_rows_end= %p",
                      m_rows_buf, m_rows_cur, m_rows_end));
  DBUG_PRINT("debug",("m_curr_row= %p, m_curr_row_end= %p",
                      m_curr_row, m_curr_row_end));
  if (m_curr_row == m_curr_row_end &&
      !((m_rows_buf == m_rows_cur) && (m_rows_cur == m_rows_end)))
  {
    my_error(ER_SLAVE_CORRUPT_EVENT, MYF(0));
    DBUG_RETURN(ER_SLAVE_CORRUPT_EVENT);
  }

  if (m_curr_row == m_rows_buf)
  {
    /* this is the first row to be inserted, we estimate the rows with
       the size of the first row and use that value to initialize
       storage engine for bulk insertion */
    DBUG_ASSERT(!(m_curr_row > m_curr_row_end));
    ulong estimated_rows= 0;
    if (m_curr_row < m_curr_row_end)
      estimated_rows= (m_rows_end - m_curr_row) / (m_curr_row_end - m_curr_row);
    else if (m_curr_row == m_curr_row_end)
      estimated_rows= 1;

    m_table->file->ha_start_bulk_insert(estimated_rows);
  }

  /*
    Explicitly set the auto_inc to null to make sure that
    it gets an auto_generated value.
  */
  if (is_auto_inc_in_extra_columns())
    m_table->next_number_field->set_null();
  
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

  m_table->mark_columns_per_binlog_row_image();

  while ((error= table->file->ha_write_row(table->record[0])))
  {
    if (error == HA_ERR_LOCK_DEADLOCK ||
        error == HA_ERR_LOCK_WAIT_TIMEOUT ||
        (keynum= table->file->get_dup_key(error)) < 0 ||
        !overwrite)
    {
      DBUG_PRINT("info",("get_dup_key returns %d)", keynum));
      /*
        Deadlock, waiting for lock or just an error from the handler
        such as HA_ERR_FOUND_DUPP_KEY when overwrite is false.
        Retrieval of the duplicate key number may fail
        - either because the error was not "duplicate key" error
        - or because the information which key is not available
      */
      table->file->print_error(error, MYF(0));
      goto error;
    }
    /*
      key index value is either valid in the range [0-MAX_KEY) or
      has value MAX_KEY as a marker for the case when no information
      about key can be found. In the last case we have to require
      that storage engine has the flag HA_DUPLICATE_POS turned on.
      If this invariant is false then DBUG_ASSERT will crash
      the server built in debug mode. For the server that was built
      without DEBUG we have additional check for the value of key index
      in the code below in order to report about error in any case.
    */
    DBUG_ASSERT(keynum != MAX_KEY ||
                (keynum == MAX_KEY &&
                 (table->file->ha_table_flags() & HA_DUPLICATE_POS)));
    /*
       We need to retrieve the old row into record[1] to be able to
       either update or delete the offending record.  We either:

       - use ha_rnd_pos() with a row-id (available as dupp_row) to the
         offending row, if that is possible (MyISAM and Blackhole), or else

       - use ha_index_read_idx_map() with the key that is duplicated, to
         retrieve the offending row.
     */
    if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
    {
      DBUG_PRINT("info",("Locating offending record using ha_rnd_pos()"));

      if (table->file->inited && (error= table->file->ha_index_end()))
      {
        table->file->print_error(error, MYF(0));
        goto error;
      }
      if ((error= table->file->ha_rnd_init(FALSE)))
      {
        table->file->print_error(error, MYF(0));
        goto error;
      }

      error= table->file->ha_rnd_pos(table->record[1], table->file->dup_ref);

      table->file->ha_rnd_end();
      if (error)
      {
        DBUG_PRINT("info",("ha_rnd_pos() returns error %d",error));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
        table->file->print_error(error, MYF(0));
        goto error;
      }
    }
    else
    {
      DBUG_PRINT("info",("Locating offending record using index_read_idx()"));

      if (table->file->extra(HA_EXTRA_FLUSH_CACHE))
      {
        DBUG_PRINT("info",("Error when setting HA_EXTRA_FLUSH_CACHE"));
        error= my_errno();
        goto error;
      }

      if (key == NULL)
      {
        key= static_cast<char*>(my_alloca(table->s->max_unique_length));
        if (key == NULL)
        {
          DBUG_PRINT("info",("Can't allocate key buffer"));
          error= ENOMEM;
          goto error;
        }
      }

      if ((uint)keynum < MAX_KEY)
      {
        key_copy((uchar*)key, table->record[0], table->key_info + keynum,
                 0);
        error= table->file->ha_index_read_idx_map(table->record[1], keynum,
                                                  (const uchar*)key,
                                                  HA_WHOLE_KEY,
                                                  HA_READ_KEY_EXACT);
      }
      else
        /*
          For the server built in non-debug mode returns error if
          handler::get_dup_key() returned MAX_KEY as the value of key index.
        */
        error= HA_ERR_FOUND_DUPP_KEY;

      if (error)
      {
        DBUG_PRINT("info",("ha_index_read_idx_map() returns %s", HA_ERR(error)));
        if (error == HA_ERR_RECORD_DELETED)
          error= HA_ERR_KEY_NOT_FOUND;
        table->file->print_error(error, MYF(0));
        goto error;
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
      error= unpack_current_row(rli, &m_cols);
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
      
      goto error;
    }
    else
    {
      DBUG_PRINT("info",("Deleting offending row and trying to write new one again"));
      if ((error= table->file->ha_delete_row(table->record[1])))
      {
        DBUG_PRINT("info",("ha_delete_row() returns error %d",error));
        table->file->print_error(error, MYF(0));
        goto error;
      }
      /* Will retry ha_write_row() with the offending row removed. */
    }
  }

error:
  m_table->default_column_bitmaps();
  DBUG_RETURN(error);
}

#endif

int
Write_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  DBUG_ASSERT(m_table != NULL);
  int error= write_row(rli, rbr_exec_mode == RBR_EXEC_MODE_IDEMPOTENT);

  if (error && !thd->is_error())
  {
    DBUG_ASSERT(0);
    my_error(ER_UNKNOWN_ERROR, MYF(0));
  }

  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Write_rows_log_event::print(FILE *file, PRINT_EVENT_INFO* print_event_info)
{
  DBUG_EXECUTE_IF("simulate_cache_read_error",
                  {DBUG_SET("+d,simulate_my_b_fill_error");});
  Rows_log_event::print_helper(file, print_event_info, "Write_rows");
}
#endif

/**************************************************************************
	Delete_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */

#ifndef MYSQL_CLIENT
Delete_rows_log_event::Delete_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             const Table_id& tid,
                                             bool is_transactional,
                                             const uchar* extra_row_info)
: binary_log::Rows_event(m_type),
  Rows_log_event(thd_arg, tbl_arg, tid, tbl_arg->read_set, is_transactional,
                 log_bin_use_v1_row_events?
                 binary_log::DELETE_ROWS_EVENT_V1:
                 binary_log::DELETE_ROWS_EVENT,
                 extra_row_info),
    binary_log::Delete_rows_event()
{
  common_header->type_code= m_type;
}
#endif /* #if !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Delete_rows_log_event::Delete_rows_log_event(const char *buf, uint event_len,
                                             const Format_description_event
                                             *description_event)
: binary_log::Rows_event(buf, event_len, description_event),
  Rows_log_event(buf, event_len, description_event),
  binary_log::Delete_rows_event(buf, event_len, description_event)
{
  DBUG_ASSERT(header()->type_code == m_type);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int
Delete_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  int error= 0;
  DBUG_ENTER("Delete_rows_log_event::do_before_row_operations");
  /*
    Increment the global status delete count variable
   */
  if (get_flags(STMT_END_F))
    thd->status_var.com_stat[SQLCOM_DELETE]++;

  /*
    Let storage engines treat this event as a DELETE command.

    Set 'sql_command' as SQLCOM_UPDATE after the tables are locked.
    When locking the tables, it should be SQLCOM_END.
    THD::decide_logging_format which is called from "lock tables"
    assumes that row_events will have 'sql_command' as SQLCOM_END.
  */
  thd->lex->sql_command= SQLCOM_DELETE;

  error= row_operations_scan_and_key_setup();
  DBUG_RETURN(error);

}

int
Delete_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                               int error)
{
  DBUG_ENTER("Delete_rows_log_event::do_after_row_operations");
  error= row_operations_scan_and_key_teardown(error);
  DBUG_RETURN(error);
}

int Delete_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  int error;
  DBUG_ASSERT(m_table != NULL);
  /* m_table->record[0] contains the BI */
  m_table->mark_columns_per_binlog_row_image();
  error= m_table->file->ha_delete_row(m_table->record[0]);
  m_table->default_column_bitmaps();
  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Delete_rows_log_event::print(FILE *file,
                                  PRINT_EVENT_INFO* print_event_info)
{
  Rows_log_event::print_helper(file, print_event_info, "Delete_rows");
}
#endif


/**************************************************************************
	Update_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Update_rows_log_event::Update_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             const Table_id& tid,
                                             bool is_transactional,
                                             const uchar* extra_row_info)
: binary_log::Rows_event(m_type),
  Rows_log_event(thd_arg, tbl_arg, tid, tbl_arg->read_set, is_transactional,
                 log_bin_use_v1_row_events?
                 binary_log::UPDATE_ROWS_EVENT_V1:
                 binary_log::UPDATE_ROWS_EVENT,
                 extra_row_info)
{
  common_header->type_code= m_type;
  init(tbl_arg->write_set);
  if (Rows_log_event::is_valid() && m_cols_ai.bitmap)
    is_valid_param= true;
}

void Update_rows_log_event::init(MY_BITMAP const *cols)
{
  /* if bitmap_init fails, caught in is_valid() */
  if (likely(!bitmap_init(&m_cols_ai,
                          m_width <= sizeof(m_bitbuf_ai)*8 ? m_bitbuf_ai : NULL,
                          m_width,
                          false)))
  {
    /* Cols can be zero if this is a dummy binrows event */
    if (likely(cols != NULL))
    {
      memcpy(m_cols_ai.bitmap, cols->bitmap, no_bytes_in_map(cols));
      create_last_word_mask(&m_cols_ai);
    }
  }
}
#endif /* !defined(MYSQL_CLIENT) */


Update_rows_log_event::~Update_rows_log_event()
{
  if (m_cols_ai.bitmap) {
  if (m_cols_ai.bitmap == m_bitbuf_ai) // no my_malloc happened
    m_cols_ai.bitmap= 0; // so no my_free in bitmap_free
  bitmap_free(&m_cols_ai); // To pair with bitmap_init().
}
}


/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Update_rows_log_event::Update_rows_log_event(const char *buf, uint event_len,
                                             const Format_description_event
                                             *description_event)
: binary_log::Rows_event(buf, event_len, description_event),
  Rows_log_event(buf, event_len, description_event),
  binary_log::Update_rows_event(buf, event_len, description_event)
{
  if (Rows_log_event::is_valid() && m_cols_ai.bitmap)
    is_valid_param= true;
  DBUG_ASSERT(header()->type_code== m_type);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int
Update_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  int error= 0;
  DBUG_ENTER("Update_rows_log_event::do_before_row_operations");
  /*
    Increment the global status update count variable
  */
  if (get_flags(STMT_END_F))
    thd->status_var.com_stat[SQLCOM_UPDATE]++;

  /*
    Let storage engines treat this event as an UPDATE command.

    Set 'sql_command' as SQLCOM_UPDATE after the tables are locked.
    When locking the tables, it should be SQLCOM_END.
    THD::decide_logging_format which is called from "lock tables"
    assumes that row_events will have 'sql_command' as SQLCOM_END.
   */
  thd->lex->sql_command= SQLCOM_UPDATE;

  error= row_operations_scan_and_key_setup();
  DBUG_RETURN(error);

}

int
Update_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                               int error)
{
  DBUG_ENTER("Update_rows_log_event::do_after_row_operations");
  error= row_operations_scan_and_key_teardown(error);
  DBUG_RETURN(error);
}

int
Update_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  DBUG_ASSERT(m_table != NULL);
  int error= 0;

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
  /* this also updates m_curr_row_end */
  if ((error= unpack_current_row(rli, &m_cols_ai)))
    return error;

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row is in record[0].
  */
  DBUG_PRINT("info",("Updating row in table"));
  DBUG_DUMP("old record", m_table->record[1], m_table->s->reclength);
  DBUG_DUMP("new values", m_table->record[0], m_table->s->reclength);

  m_table->mark_columns_per_binlog_row_image();
  error= m_table->file->ha_update_row(m_table->record[1], m_table->record[0]);
  if (error == HA_ERR_RECORD_IS_THE_SAME)
    error= 0;
  m_table->default_column_bitmaps();

  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Update_rows_log_event::print(FILE *file,
				  PRINT_EVENT_INFO* print_event_info)
{
  Rows_log_event::print_helper(file, print_event_info, "Update_rows");
}
#endif


Incident_log_event::
Incident_log_event(const char *buf, uint event_len,
                   const Format_description_event *description_event)
   : binary_log::Incident_event(buf, event_len, description_event),
     Log_event(header(), footer())
{
  DBUG_ENTER("Incident_log_event::Incident_log_event");
  if (incident > INCIDENT_NONE && incident < INCIDENT_COUNT)
    is_valid_param= true;
  DBUG_VOID_RETURN;
}


Incident_log_event::~Incident_log_event()
{
  if (message)
    bapi_free(message);
}


const char *
Incident_log_event::description() const
{
  static const char *const description[]= {
    "NOTHING",                                  // Not used
    "LOST_EVENTS"
  };

  DBUG_PRINT("info", ("incident: %d", incident));

  return description[incident];
}


#ifndef MYSQL_CLIENT
int Incident_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  size_t bytes;
  if (message_length > 0)
    bytes= my_snprintf(buf, sizeof(buf), "#%d (%s)",
                       incident, description());
  else
    bytes= my_snprintf(buf, sizeof(buf), "#%d (%s): %s",
                       incident, description(), message);
  protocol->store(buf, bytes, &my_charset_bin);
  return 0;
}
#endif


#ifdef MYSQL_CLIENT
void
Incident_log_event::print(FILE *file,
                          PRINT_EVENT_INFO *print_event_info)
{
  if (print_event_info->short_form)
    return;

  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache,
              "\n# Incident: %s\nRELOAD DATABASE; # Shall generate syntax error\n",
              description());
}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int
Incident_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Incident_log_event::do_apply_event");

  /*
    It is not necessary to do GTID related check if the error
    'ER_SLAVE_INCIDENT' is ignored.
  */
  if (ignored_error_code(ER_SLAVE_INCIDENT))
  {
    DBUG_PRINT("info", ("Ignoring Incident"));
    mysql_bin_log.gtid_end_transaction(thd);
    DBUG_RETURN(0);
  }

  enum_gtid_statement_status state= gtid_pre_statement_checks(thd);
  if (state == GTID_STATEMENT_EXECUTE)
  {
    if (gtid_pre_statement_post_implicit_commit_checks(thd))
      state= GTID_STATEMENT_CANCEL;
  }

  if (state == GTID_STATEMENT_CANCEL)
  {
    uint error= thd->get_stmt_da()->mysql_errno();
    DBUG_ASSERT(error != 0);
    rli->report(ERROR_LEVEL, error,
                "Error executing incident event: '%s'",
                thd->get_stmt_da()->message_text());
    thd->is_slave_error= 1;
    DBUG_RETURN(-1);
  }
  else if (state == GTID_STATEMENT_SKIP)
  {
    /*
      Make slave skip the Incident event through general commands of GTID
      i.e. 'set gtid_next=<GTID>; begin; commit;'.
    */
    DBUG_RETURN(0);
  }

  rli->report(ERROR_LEVEL, ER_SLAVE_INCIDENT,
              ER(ER_SLAVE_INCIDENT),
              description(),
              message_length > 0 ? message : "<none>");
  DBUG_RETURN(1);
}
#endif

bool
Incident_log_event::write_data_header(IO_CACHE *file)
{
  DBUG_ENTER("Incident_log_event::write_data_header");
  DBUG_PRINT("enter", ("incident: %d", incident));
  uchar buf[sizeof(int16)];
  int2store(buf, (int16) incident);
#ifndef MYSQL_CLIENT
  DBUG_RETURN(wrapper_my_b_safe_write(file, buf, sizeof(buf)));
#else
   DBUG_RETURN(my_b_safe_write(file, buf, sizeof(buf)));
#endif
}

bool
Incident_log_event::write_data_body(IO_CACHE *file)
{
  uchar tmp[1];
  DBUG_ENTER("Incident_log_event::write_data_body");
  tmp[0]= (uchar) message_length;
  crc= checksum_crc32(crc, (uchar*) tmp, 1);
  if (message_length > 0)
  {
    crc= checksum_crc32(crc, (uchar*) message, message_length);
    // todo: report a bug on write_str accepts uint but treats it as uchar
  }
  DBUG_RETURN(write_str_at_most_255_bytes(file, message, (uint) message_length));
}


Ignorable_log_event::Ignorable_log_event(const char *buf,
                                         const Format_description_event *descr_event)
  : binary_log::Ignorable_event(buf, descr_event),
    Log_event(header(), footer())
{
  DBUG_ENTER("Ignorable_log_event::Ignorable_log_event");

  is_valid_param= true;
  DBUG_VOID_RETURN;
}

Ignorable_log_event::~Ignorable_log_event()
{
}

#ifndef MYSQL_CLIENT
/* Pack info for its unrecognized ignorable event */
int Ignorable_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  size_t bytes;
  bytes= my_snprintf(buf, sizeof(buf), "# Unrecognized ignorable event");
  protocol->store(buf, bytes, &my_charset_bin);
  return 0;
}
#endif

#ifdef MYSQL_CLIENT
/* Print for its unrecognized ignorable event */
void
Ignorable_log_event::print(FILE *file,
                           PRINT_EVENT_INFO *print_event_info)
{
  if (print_event_info->short_form)
    return;

  print_header(&print_event_info->head_cache, print_event_info, FALSE);
  my_b_printf(&print_event_info->head_cache, "\tIgnorable\n");
  my_b_printf(&print_event_info->head_cache,
              "# Unrecognized ignorable event\n");
}
#endif


Rows_query_log_event::Rows_query_log_event(const char *buf, uint event_len,
                                           const Format_description_event
                                           *descr_event)
  : binary_log::Ignorable_event(buf, descr_event),
    Ignorable_log_event(buf, descr_event),
    binary_log::Rows_query_event(buf, event_len, descr_event)
{
  is_valid_param= (m_rows_query != NULL);
}

#ifndef MYSQL_CLIENT
int Rows_query_log_event::pack_info(Protocol *protocol)
{
  char *buf;
  size_t bytes;
  size_t len= sizeof("# ") + strlen(m_rows_query);
  if (!(buf= (char*) my_malloc(key_memory_log_event,
                               len, MYF(MY_WME))))
    return 1;
  bytes= my_snprintf(buf, len, "# %s", m_rows_query);
  protocol->store(buf, bytes, &my_charset_bin);
  my_free(buf);
  return 0;
}
#endif

#ifdef MYSQL_CLIENT
void
Rows_query_log_event::print(FILE *file,
                            PRINT_EVENT_INFO *print_event_info)
{
  if (!print_event_info->short_form && print_event_info->verbose > 1)
  {
    IO_CACHE *const head= &print_event_info->head_cache;
    IO_CACHE *const body= &print_event_info->body_cache;
    char *token= NULL, *saveptr= NULL;
    char *rows_query_copy= NULL;
    if (!(rows_query_copy= my_strdup(key_memory_log_event,
                                     m_rows_query, MYF(MY_WME))))
      return;

    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\tRows_query\n");
    /*
      Prefix every line of a multi-line query with '#' to prevent the
      statement from being executed when binary log will be processed
      using 'mysqlbinlog --verbose --verbose'.
    */
    for (token= my_strtok_r(rows_query_copy, "\n", &saveptr); token;
         token= my_strtok_r(NULL, "\n", &saveptr))
      my_b_printf(head, "# %s\n", token);
    my_free(rows_query_copy);
    print_base64(body, print_event_info, true);
  }
}
#endif

bool
Rows_query_log_event::write_data_body(IO_CACHE *file)
{
  DBUG_ENTER("Rows_query_log_event::write_data_body");
  /*
   m_rows_query length will be stored using only one byte, but on read
   that length will be ignored and the complete query will be read.
  */
  DBUG_RETURN(write_str_at_most_255_bytes(file, m_rows_query,
              strlen(m_rows_query)));
}

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
int Rows_query_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_query_log_event::do_apply_event");
  DBUG_ASSERT(rli->info_thd == thd);
  /* Set query for writing Rows_query log event into binlog later.*/
  thd->set_query(m_rows_query, strlen(m_rows_query));

  DBUG_ASSERT(rli->rows_query_ev == NULL);

  const_cast<Relay_log_info*>(rli)->rows_query_ev= this;
  /* Tell worker not to free the event */
  worker= NULL;
  DBUG_RETURN(0);
}
#endif


const char *Gtid_log_event::SET_STRING_PREFIX= "SET @@SESSION.GTID_NEXT= '";


Gtid_log_event::Gtid_log_event(const char *buffer, uint event_len,
                               const Format_description_event *description_event)
   : binary_log::Gtid_event(buffer, event_len, description_event),
     Log_event(header(), footer())
{
  DBUG_ENTER("Gtid_log_event::Gtid_log_event(const char *,"
             " uint, const Format_description_log_event *");

#ifndef DBUG_OFF
  uint8_t const common_header_len= description_event->common_header_len;
  uint8 const post_header_len=
    buffer[EVENT_TYPE_OFFSET] == binary_log::ANONYMOUS_GTID_LOG_EVENT ?
    description_event->post_header_len[binary_log::ANONYMOUS_GTID_LOG_EVENT - 1] :
    description_event->post_header_len[binary_log::GTID_LOG_EVENT - 1];
  DBUG_PRINT("info",
             ("event_len: %u; common_header_len: %d; post_header_len: %d",
              event_len, common_header_len, post_header_len));
#endif

  is_valid_param= true;
  spec.type= get_type_code() == binary_log::ANONYMOUS_GTID_LOG_EVENT ?
             ANONYMOUS_GROUP : GTID_GROUP;
  sid.copy_from((uchar *)Uuid_parent_struct.bytes);
  spec.gtid.sidno= gtid_info_struct.rpl_gtid_sidno;
  spec.gtid.gno= gtid_info_struct.rpl_gtid_gno;
  DBUG_VOID_RETURN;
}

#ifndef MYSQL_CLIENT
Gtid_log_event::Gtid_log_event(THD* thd_arg, bool using_trans,
                               int64 last_committed_arg,
                               int64 sequence_number_arg,
                               bool may_have_sbr_stmts_arg)
: binary_log::Gtid_event(last_committed_arg, sequence_number_arg,
                         may_have_sbr_stmts_arg),
  Log_event(thd_arg, thd_arg->variables.gtid_next.type == ANONYMOUS_GROUP ?
            LOG_EVENT_IGNORABLE_F : 0,
            using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
            Log_event::EVENT_STMT_CACHE, Log_event::EVENT_NORMAL_LOGGING,
            header(), footer())
{
  DBUG_ENTER("Gtid_log_event::Gtid_log_event(THD *)");
  if (thd->owned_gtid.sidno > 0)
  {
    spec.set(thd->owned_gtid);
    sid= thd->owned_sid;
  }
  else
  {
    DBUG_ASSERT(thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS);
    spec.set_anonymous();
    spec.gtid.clear();
    sid.clear();
  }

  Log_event_type event_type= (spec.type == ANONYMOUS_GROUP ?
                              binary_log::ANONYMOUS_GTID_LOG_EVENT :
                              binary_log::GTID_LOG_EVENT);
  common_header->type_code= event_type;

#ifndef DBUG_OFF
  char buf[MAX_SET_STRING_LENGTH + 1];
  to_string(buf);
  DBUG_PRINT("info", ("%s", buf));
#endif
  is_valid_param= true;
  DBUG_VOID_RETURN;
}

Gtid_log_event::Gtid_log_event(uint32 server_id_arg, bool using_trans,
                               int64 last_committed_arg,
                               int64 sequence_number_arg,
                               bool may_have_sbr_stmts_arg,
                               const Gtid_specification spec_arg)
 : binary_log::Gtid_event(last_committed_arg, sequence_number_arg,
                          may_have_sbr_stmts_arg),
   Log_event(header(), footer(),
             using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
             Log_event::EVENT_STMT_CACHE, Log_event::EVENT_NORMAL_LOGGING)
{
  DBUG_ENTER("Gtid_log_event::Gtid_log_event(uint32, bool, int64, int64, const Gtid_specification)");
  server_id= server_id_arg;
  common_header->unmasked_server_id= server_id_arg;

  if (spec_arg.type == GTID_GROUP)
  {
    DBUG_ASSERT(spec_arg.gtid.sidno > 0 && spec_arg.gtid.gno > 0);
    spec.set(spec_arg.gtid);
    global_sid_lock->rdlock();
    sid= global_sid_map->sidno_to_sid(spec_arg.gtid.sidno);
    global_sid_lock->unlock();
  }
  else
  {
    DBUG_ASSERT(spec_arg.type == ANONYMOUS_GROUP);
    spec.set_anonymous();
    spec.gtid.clear();
    sid.clear();
    common_header->flags|= LOG_EVENT_IGNORABLE_F;
  }

  Log_event_type event_type= (spec.type == ANONYMOUS_GROUP ?
                              binary_log::ANONYMOUS_GTID_LOG_EVENT :
                              binary_log::GTID_LOG_EVENT);
  common_header->type_code= event_type;

#ifndef DBUG_OFF
  char buf[MAX_SET_STRING_LENGTH + 1];
  to_string(buf);
  DBUG_PRINT("info", ("%s", buf));
#endif
  is_valid_param= true;
  DBUG_VOID_RETURN;
}
#endif

#ifndef MYSQL_CLIENT
int Gtid_log_event::pack_info(Protocol *protocol)
{
  char buffer[MAX_SET_STRING_LENGTH + 1];
  size_t len= to_string(buffer);
  protocol->store(buffer, len, &my_charset_bin);
  return 0;
}
#endif

size_t Gtid_log_event::to_string(char *buf) const
{
  char *p= buf;
  DBUG_ASSERT(strlen(SET_STRING_PREFIX) == SET_STRING_PREFIX_LENGTH);
  strcpy(p, SET_STRING_PREFIX);
  p+= SET_STRING_PREFIX_LENGTH;
  p+= spec.to_string(&sid, p);
  *p++= '\'';
  *p= '\0';
  return p - buf;
}

#ifdef MYSQL_CLIENT
void
Gtid_log_event::print(FILE *file, PRINT_EVENT_INFO *print_event_info)
{
  char buffer[MAX_SET_STRING_LENGTH + 1];
  IO_CACHE *const head= &print_event_info->head_cache;
  if (!print_event_info->short_form)
  {
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "\t%s\tlast_committed=%llu\tsequence_number=%llu\t"
                "rbr_only=%s\n",
                get_type_code() == binary_log::GTID_LOG_EVENT ?
                "GTID" : "Anonymous_GTID",
                last_committed, sequence_number,
                may_have_sbr_stmts ? "no" : "yes");
  }

  /*
    The applier thread can always use "READ COMMITTED" isolation for
    transactions containing only RBR events (Table_map + Rows).

    This would prevent some deadlock issues because InnoDB doesn't
    acquire GAP locks in "READ COMMITTED" isolation level since
    MySQL 5.7.18.
  */
  if (!may_have_sbr_stmts)
  {
    my_b_printf(head,
                "/*!50718 SET TRANSACTION ISOLATION LEVEL "
                "READ COMMITTED*/%s\n",
                print_event_info->delimiter);
  }

  to_string(buffer);
  my_b_printf(head, "%s%s\n", buffer, print_event_info->delimiter);
}
#endif

#ifdef MYSQL_SERVER
uint32 Gtid_log_event::write_data_header_to_memory(uchar *buffer)
{
  DBUG_ENTER("Gtid_log_event::write_data_header_to_memory");
  uchar *ptr_buffer= buffer;

  /* Encode the GTID flags */
  uchar gtid_flags= 0;
  gtid_flags|= may_have_sbr_stmts ?
               binary_log::Gtid_event::FLAG_MAY_HAVE_SBR : 0;
  *ptr_buffer= gtid_flags;
  ptr_buffer+= ENCODED_FLAG_LENGTH;

#ifndef DBUG_OFF
  char buf[binary_log::Uuid::TEXT_LENGTH + 1];
  sid.to_string(buf);
  DBUG_PRINT("info", ("sid=%s sidno=%d gno=%lld",
                      buf, spec.gtid.sidno, spec.gtid.gno));
#endif

  sid.copy_to(ptr_buffer);
  ptr_buffer+= ENCODED_SID_LENGTH;

  int8store(ptr_buffer, spec.gtid.gno);
  ptr_buffer+= ENCODED_GNO_LENGTH;

  *ptr_buffer= LOGICAL_TIMESTAMP_TYPECODE;
  ptr_buffer+= LOGICAL_TIMESTAMP_TYPECODE_LENGTH;

  DBUG_ASSERT((sequence_number == 0 && last_committed == 0) ||
              (sequence_number > last_committed));
  DBUG_EXECUTE_IF("set_commit_parent_100",
                  { last_committed= max<int64>(sequence_number > 1 ? 1 : 0,
                                               sequence_number - 100); });
  DBUG_EXECUTE_IF("set_commit_parent_150",
                  { last_committed= max<int64>(sequence_number > 1 ? 1 : 0,
                                               sequence_number - 150); });
  DBUG_EXECUTE_IF("feign_commit_parent", { last_committed= sequence_number; });
  int8store(ptr_buffer, last_committed);
  int8store(ptr_buffer + 8, sequence_number);
  ptr_buffer+= LOGICAL_TIMESTAMP_LENGTH;

  DBUG_ASSERT(ptr_buffer == (buffer + POST_HEADER_LENGTH));

  DBUG_RETURN(POST_HEADER_LENGTH);
}

bool Gtid_log_event::write_data_header(IO_CACHE *file)
{
  DBUG_ENTER("Gtid_log_event::write_data_header");
  uchar buffer[POST_HEADER_LENGTH];
  write_data_header_to_memory(buffer);
  DBUG_RETURN(wrapper_my_b_safe_write(file, (uchar *) buffer,
                                      POST_HEADER_LENGTH));
}

#endif // MYSQL_SERVER

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
int Gtid_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Gtid_log_event::do_apply_event");
  DBUG_ASSERT(rli->info_thd == thd);

  /*
    In rare cases it is possible that we already own a GTID (either
    ANONYMOUS or GTID_GROUP). This can happen if a transaction was truncated
    in the middle in the relay log and then next relay log begins with a
    Gtid_log_events without closing the transaction context from the previous
    relay log. In this case the only sensible thing to do is to discard the
    truncated transaction and move on.

    Note that when the applier is "GTID skipping" a transactions it
    owns nothing, but its gtid_next->type == GTID_GROUP.
  */
  const Gtid_specification *gtid_next= &thd->variables.gtid_next;
  if (!thd->owned_gtid.is_empty() ||
      (thd->owned_gtid.is_empty() && gtid_next->type == GTID_GROUP))
  {
    /*
      Slave will execute this code if a previous Gtid_log_event was applied
      but the GTID wasn't consumed yet (the transaction was not committed,
      nor rolled back, nor skipped).
      On a client session we cannot do consecutive SET GTID_NEXT without
      a COMMIT or a ROLLBACK in the middle.
      Applying this event without rolling back the current transaction may
      lead to problems, as a "BEGIN" event following this GTID will
      implicitly commit the "partial transaction" and will consume the
      GTID. If this "partial transaction" was left in the relay log by the
      IO thread restarting in the middle of a transaction, you could have
      the partial transaction being logged with the GTID on the slave,
      causing data corruption on replication.
    */
    if (thd->server_status & SERVER_STATUS_IN_TRANS)
    {
      /* This is not an error (XA is safe), just an information */
      rli->report(INFORMATION_LEVEL, 0,
                  "Rolling back unfinished transaction (no COMMIT "
                  "or ROLLBACK in relay log). A probable cause is partial "
                  "transaction left on relay log because of restarting IO "
                  "thread with auto-positioning protocol.");
      const_cast<Relay_log_info*>(rli)->cleanup_context(thd, 1);
    }
    gtid_state->update_on_rollback(thd);
  }

  global_sid_lock->rdlock();

  // make sure that sid has been converted to sidno
  if (spec.type == GTID_GROUP)
  {
    if (get_sidno(false) < 0)
    {
      global_sid_lock->unlock();
      DBUG_RETURN(1); // out of memory
    }
  }

  // set_gtid_next releases global_sid_lock
  if (set_gtid_next(thd, spec))
    // This can happen e.g. if gtid_mode is incompatible with spec.
    DBUG_RETURN(1);

  thd->set_currently_executing_gtid_for_slave_thread();

  /*
    If the current transaction contains no changes logged with SBR
    we can assume this transaction as a pure row based replicated one.

    Based on this assumption, we can set current transaction tx_isolation to
    READ COMMITTED in order to avoid concurrent transactions to be blocked by
    InnoDB gap locks.

    The session tx_isolation will be restored:
    - When the transaction finishes with QUERY(COMMIT|ROLLBACK),
      as the MySQL server does for ordinary user sessions;
    - When applying a Xid_log_event, after committing the transaction;
    - When applying a XA_prepare_log_event, after preparing the transaction;
    - When the applier needs to abort a transaction execution.

    Notice that when a transaction is being "gtid skipped", its statements are
    not actually executed (see mysql_execute_command()). So, the call to the
    function that would restore the tx_isolation after finishing the transaction
    may not happen.
  */
  if (DBUG_EVALUATE_IF("force_trx_as_rbr_only", true,
                       !may_have_sbr_stmts &&
                       thd->tx_isolation > ISO_READ_COMMITTED &&
                       gtid_pre_statement_checks(thd) != GTID_STATEMENT_SKIP))
  {
    DBUG_ASSERT(thd->get_transaction()->is_empty(Transaction_ctx::STMT));
    DBUG_ASSERT(thd->get_transaction()->is_empty(Transaction_ctx::SESSION));
    DBUG_ASSERT(!thd->lock);
    DBUG_PRINT("info", ("setting tx_isolation to READ COMMITTED"));
    set_tx_isolation(thd, ISO_READ_COMMITTED, true/*one_shot*/);
  }

  DBUG_RETURN(0);
}

int Gtid_log_event::do_update_pos(Relay_log_info *rli)
{
  /*
    This event does not increment group positions. This means
    that if there is a failure after it has been processed,
    it will be automatically re-executed.
  */
  rli->inc_event_relay_log_pos();
  DBUG_EXECUTE_IF("crash_after_update_pos_gtid",
                  sql_print_information("Crashing crash_after_update_pos_gtid.");
                  DBUG_SUICIDE(););
  return 0;
}

Log_event::enum_skip_reason Gtid_log_event::do_shall_skip(Relay_log_info *rli)
{
  return Log_event::continue_group(rli);
}
#endif

Previous_gtids_log_event::
Previous_gtids_log_event(const char *buf, uint event_len,
                         const Format_description_event *description_event)
: binary_log::Previous_gtids_event(buf, event_len, description_event),
  Log_event(header(), footer())
{
  DBUG_ENTER("Previous_gtids_log_event::Previous_gtids_log_event");
  if (buf != NULL)
    is_valid_param= true;
  DBUG_VOID_RETURN;
}

#ifndef MYSQL_CLIENT
Previous_gtids_log_event::Previous_gtids_log_event(const Gtid_set *set)
: binary_log::Previous_gtids_event(),
  Log_event(header(), footer(),
            Log_event::EVENT_NO_CACHE,
            Log_event::EVENT_IMMEDIATE_LOGGING)
{
  DBUG_ENTER("Previous_gtids_log_event::Previous_gtids_log_event(THD *, const Gtid_set *)");
  common_header->type_code= binary_log::PREVIOUS_GTIDS_LOG_EVENT;
  common_header->flags|= LOG_EVENT_IGNORABLE_F;
  global_sid_lock->assert_some_lock();
  buf_size= set->get_encoded_length();
  uchar *buffer= (uchar *) my_malloc(key_memory_log_event,
                                     buf_size, MYF(MY_WME));
  if (buffer != NULL)
  {
    set->encode(buffer);
    register_temp_buf((char *)buffer);
  }
  buf= buffer;
  // if buf is empty, is_valid will be false
  if(buf != 0)
    is_valid_param= true;
  DBUG_VOID_RETURN;
}
#endif

#ifndef MYSQL_CLIENT
int Previous_gtids_log_event::pack_info(Protocol *protocol)
{
  size_t length= 0;
  char *str= get_str(&length, &Gtid_set::default_string_format);
  if (str == NULL)
    return 1;
  protocol->store(str, length, &my_charset_bin);
  my_free(str);
  return 0;
}
#endif

#ifdef MYSQL_CLIENT
void Previous_gtids_log_event::print(FILE *file,
                                     PRINT_EVENT_INFO *print_event_info)
{
  IO_CACHE *const head= &print_event_info->head_cache;
  char *str= get_str(NULL, &Gtid_set::commented_string_format);
  if (str != NULL)
  {
    if (!print_event_info->short_form)
    {
      print_header(head, print_event_info, FALSE);
      my_b_printf(head, "\tPrevious-GTIDs\n");
    }
    my_b_printf(head, "%s\n", str);
    my_free(str);
  }
}
#endif

int Previous_gtids_log_event::add_to_set(Gtid_set *target) const
{
  DBUG_ENTER("Previous_gtids_log_event::add_to_set(Gtid_set *)");
  size_t end_pos= 0;
  size_t add_size= DBUG_EVALUATE_IF("gtid_has_extra_data", 10, 0);
  /* Silently ignore additional unknown data at the end of the encoding */
  PROPAGATE_REPORTED_ERROR_INT(target->add_gtid_encoding(buf,
                                                         buf_size + add_size,
                                                         &end_pos));
  DBUG_ASSERT(end_pos <= buf_size);
  DBUG_RETURN(0);
}

char *Previous_gtids_log_event::get_str(
  size_t *length_p, const Gtid_set::String_format *string_format) const
{
  DBUG_ENTER("Previous_gtids_log_event::get_str(size_t *, const Gtid_set::String_format *)");
  Sid_map sid_map(NULL);
  Gtid_set set(&sid_map, NULL);
  DBUG_PRINT("info", ("temp_buf=%p buf=%p", temp_buf, buf));
  if (set.add_gtid_encoding(buf, buf_size) != RETURN_STATUS_OK)
    DBUG_RETURN(NULL);
  set.dbug_print("set");
  size_t length= set.get_string_length(string_format);
  DBUG_PRINT("info", ("string length= %lu", (ulong) length));
  char* str= (char *)my_malloc(key_memory_log_event,
                               length + 1, MYF(MY_WME));
  if (str != NULL)
  {
    set.to_string(str, false/*need_lock*/, string_format);
    if (length_p != NULL)
      *length_p= length;
  }
  DBUG_RETURN(str);
}

#ifndef MYSQL_CLIENT
bool Previous_gtids_log_event::write_data_body(IO_CACHE *file)
{
  DBUG_ENTER("Previous_gtids_log_event::write_data_body");
  DBUG_PRINT("info", ("size=%d", static_cast<int>(buf_size)));
  bool ret= wrapper_my_b_safe_write(file, buf, buf_size);
  DBUG_RETURN(ret);
}
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
int Previous_gtids_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}
#endif


/**************************************************************************
	Transaction_context_log_event methods
**************************************************************************/
#ifndef MYSQL_CLIENT
Transaction_context_log_event::
Transaction_context_log_event(const char *server_uuid_arg,
                              bool using_trans,
                              my_thread_id thread_id_arg,
                              bool is_gtid_specified_arg)
  : binary_log::Transaction_context_event(thread_id_arg,
                                          is_gtid_specified_arg),
    Log_event(header(), footer(),
              using_trans ? Log_event::EVENT_TRANSACTIONAL_CACHE :
              Log_event::EVENT_STMT_CACHE, Log_event::EVENT_NORMAL_LOGGING)
{
  DBUG_ENTER("Transaction_context_log_event::Transaction_context_log_event(THD *, const char *, ulonglong)");
  common_header->flags|= LOG_EVENT_IGNORABLE_F;
  server_uuid= NULL;
  sid_map= new Sid_map(NULL);
  snapshot_version= new Gtid_set(sid_map);

  /*
    Copy global_sid_map to a local copy to avoid the acquisition
    of the global_sid_lock for operations on top of this snapshot
    version.
    The Sid_map and Gtid_executed must be read under the protection
    of MYSQL_BIN_LOG.LOCK_commit to avoid race conditions between
    ordered commits in the storage engine and gtid_state update.
  */
  if (mysql_bin_log.get_gtid_executed(sid_map, snapshot_version))
    goto err;

  server_uuid= my_strdup(key_memory_log_event, server_uuid_arg, MYF(MY_WME));
  if (server_uuid == NULL)
    goto err;

  // These two fields are only populated on event decoding.
  // Encoding is done directly from snapshot_version field.
  encoded_snapshot_version= NULL;
  encoded_snapshot_version_length= 0;

  // Debug sync point for SQL threads.
  DBUG_EXECUTE_IF("debug.wait_after_set_snapshot_version_on_transaction_context_log_event",
                  {
                    const char act[]=
                        "now wait_for "
                        "signal.resume_after_set_snapshot_version_on_transaction_context_log_event";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  is_valid_param= true;
  DBUG_VOID_RETURN;

err:
  is_valid_param= false;
  DBUG_VOID_RETURN;
}
#endif

Transaction_context_log_event::
Transaction_context_log_event(const char *buffer, uint event_len,
                              const Format_description_event *descr_event)
  : binary_log::Transaction_context_event(buffer, event_len, descr_event),
    Log_event(header(), footer())
{
  DBUG_ENTER("Transaction_context_log_event::Transaction_context_log_event (const char *, uint, const Format_description_event*)");
  common_header->flags|= LOG_EVENT_IGNORABLE_F;

  sid_map= new Sid_map(NULL);
  snapshot_version= new Gtid_set(sid_map);

  if (server_uuid == NULL || encoded_snapshot_version == NULL)
    goto err;

  is_valid_param= true;
  DBUG_VOID_RETURN;

err:
  is_valid_param= false;
  DBUG_VOID_RETURN;
}

Transaction_context_log_event::~Transaction_context_log_event()
{
  DBUG_ENTER("Transaction_context_log_event::~Transaction_context_log_event");
  if (server_uuid)
    my_free((void*)server_uuid);
  server_uuid= NULL;
  if (encoded_snapshot_version)
    my_free((void*) encoded_snapshot_version);
  encoded_snapshot_version= NULL;
  delete snapshot_version;
  delete sid_map;
  DBUG_VOID_RETURN;
}

size_t Transaction_context_log_event::to_string(char *buf, ulong len) const
{
  DBUG_ENTER("Transaction_context_log_event::to_string");
  DBUG_RETURN(my_snprintf(buf, len,
                          "server_uuid=%s\tthread_id=%lu",
                          server_uuid, thread_id));
}

#ifndef MYSQL_CLIENT
int Transaction_context_log_event::pack_info(Protocol *protocol)
{
  DBUG_ENTER("Transaction_context_log_event::pack_info");
  char buf[256];
  size_t bytes= to_string(buf, 256);
  protocol->store(buf, bytes, &my_charset_bin);
  DBUG_RETURN(0);
}
#endif

#ifdef MYSQL_CLIENT
void Transaction_context_log_event::print(FILE *file,
                                          PRINT_EVENT_INFO *print_event_info)
{
  DBUG_ENTER("Transaction_context_log_event::print");
  char buf[256];
  IO_CACHE *const head= &print_event_info->head_cache;

  if (!print_event_info->short_form)
  {
    to_string(buf, 256);
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "Transaction_context: %s\n", buf);
  }
  DBUG_VOID_RETURN;
}
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
int Transaction_context_log_event::do_update_pos(Relay_log_info *rli)
{
  DBUG_ENTER("Transaction_context_log_event::do_update_pos");
  rli->inc_event_relay_log_pos();
  DBUG_RETURN(0);
}
#endif

size_t Transaction_context_log_event::get_data_size()
{
  DBUG_ENTER("Transaction_context_log_event::get_data_size");

  size_t size= Binary_log_event::TRANSACTION_CONTEXT_HEADER_LEN;
  size += strlen(server_uuid);
  size += get_snapshot_version_size();
  size += get_data_set_size(&write_set);
  size += get_data_set_size(&read_set);

  DBUG_RETURN(size);
}

#ifndef MYSQL_CLIENT
bool Transaction_context_log_event::write_data_header(IO_CACHE* file)
{
  DBUG_ENTER("Transaction_context_log_event::write_data_header");
  char buf[Binary_log_event::TRANSACTION_CONTEXT_HEADER_LEN];

  buf[ENCODED_SERVER_UUID_LEN_OFFSET] = (char) strlen(server_uuid);
  int4store(buf + ENCODED_THREAD_ID_OFFSET, thread_id);
  buf[ENCODED_GTID_SPECIFIED_OFFSET] = gtid_specified;
  int4store(buf + ENCODED_SNAPSHOT_VERSION_LEN_OFFSET, get_snapshot_version_size());
  int4store(buf + ENCODED_WRITE_SET_ITEMS_OFFSET, write_set.size());
  int4store(buf + ENCODED_READ_SET_ITEMS_OFFSET, read_set.size());
  DBUG_RETURN(wrapper_my_b_safe_write(file, (const uchar *) buf,
                                      Binary_log_event::TRANSACTION_CONTEXT_HEADER_LEN));
}

bool Transaction_context_log_event::write_data_body(IO_CACHE* file)
{
  DBUG_ENTER("Transaction_context_log_event::write_data_body");

  if (wrapper_my_b_safe_write(file,
                              (const uchar*) server_uuid,
                              strlen(server_uuid)) ||
      write_snapshot_version(file) ||
      write_data_set(file, &write_set) ||
      write_data_set(file, &read_set))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

bool Transaction_context_log_event::write_snapshot_version(IO_CACHE* file)
{
  DBUG_ENTER("Transaction_context_log_event::write_snapshot_version");
  bool result= false;

  uint32 len= get_snapshot_version_size();
  uchar *buffer= (uchar *) my_malloc(key_memory_log_event,
                                     len, MYF(MY_WME));
  if (buffer == NULL)
    DBUG_RETURN(true);

  snapshot_version->encode(buffer);
  if (wrapper_my_b_safe_write(file, buffer, len))
    result= true;

  my_free(buffer);
  DBUG_RETURN(result);
}

bool Transaction_context_log_event::write_data_set(IO_CACHE* file,
                                                   std::list<const char*> *set)
{
  DBUG_ENTER("Transaction_context_log_event::write_data_set");
  for (std::list<const char*>::iterator it=set->begin();
       it != set->end();
       ++it)
  {
    char buf[ENCODED_READ_WRITE_SET_ITEM_LEN];
    const char* hash= *it;
    uint16 len= strlen(hash);

    int2store(buf, len);
    if (wrapper_my_b_safe_write(file,
                                (const uchar*) buf,
                                ENCODED_READ_WRITE_SET_ITEM_LEN) ||
        wrapper_my_b_safe_write(file, (const uchar*) hash, len))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}
#endif

bool Transaction_context_log_event::read_snapshot_version()
{
  DBUG_ENTER("Transaction_context_log_event::read_snapshot_version");
  DBUG_ASSERT(snapshot_version->is_empty());

  global_sid_lock->wrlock();
  enum_return_status return_status= global_sid_map->copy(sid_map);
  global_sid_lock->unlock();
  if (return_status != RETURN_STATUS_OK)
    DBUG_RETURN(true);

  DBUG_RETURN(snapshot_version->add_gtid_encoding(encoded_snapshot_version,
                                                  encoded_snapshot_version_length)
                  != RETURN_STATUS_OK);
}

size_t Transaction_context_log_event::get_snapshot_version_size()
{
  DBUG_ENTER("Transaction_context_log_event::get_snapshot_version_size");
  size_t result= snapshot_version->get_encoded_length();
  DBUG_RETURN(result);
}

int Transaction_context_log_event::get_data_set_size(std::list<const char*> *set)
{
  DBUG_ENTER("Transaction_context_log_event::get_data_set_size");
  int size= 0;

  for (std::list<const char*>::iterator it=set->begin();
       it != set->end();
       ++it)
    size += ENCODED_READ_WRITE_SET_ITEM_LEN + strlen(*it);

  DBUG_RETURN(size);
}

void Transaction_context_log_event::add_write_set(const char *hash)
{
  DBUG_ENTER("Transaction_context_log_event::add_write_set");
  write_set.push_back(hash);
  DBUG_VOID_RETURN;
}

void Transaction_context_log_event::add_read_set(const char *hash)
{
  DBUG_ENTER("Transaction_context_log_event::add_read_set");
  read_set.push_back(hash);
  DBUG_VOID_RETURN;
}

/**************************************************************************
	View_change_log_event methods
**************************************************************************/

#ifndef MYSQL_CLIENT
View_change_log_event::View_change_log_event(char* raw_view_id)
  : binary_log::View_change_event(raw_view_id),
    Log_event(header(), footer(), Log_event::EVENT_TRANSACTIONAL_CACHE,
              Log_event::EVENT_NORMAL_LOGGING)
{
  DBUG_ENTER("View_change_log_event::View_change_log_event(char*)");
  common_header->flags|= LOG_EVENT_IGNORABLE_F;

  if (strlen(view_id) != 0)
    is_valid_param= true;

  DBUG_VOID_RETURN;
}
#endif

View_change_log_event::
View_change_log_event(const char *buffer,
                      uint event_len,
                      const Format_description_event *descr_event)
  : binary_log::View_change_event(buffer, event_len, descr_event),
    Log_event(header(), footer())
{
  DBUG_ENTER("View_change_log_event::View_change_log_event(const char *,"
             " uint, const Format_description_event*)");
  common_header->flags|= LOG_EVENT_IGNORABLE_F;

  if (strlen(view_id) != 0)
    is_valid_param= true;

  //Change the cache/logging types to allow writing to the binary log cache
  event_cache_type= EVENT_TRANSACTIONAL_CACHE;
  event_logging_type= EVENT_NORMAL_LOGGING;

  DBUG_VOID_RETURN;
}

View_change_log_event::~View_change_log_event()
{
  DBUG_ENTER("View_change_log_event::~View_change_log_event");
  certification_info.clear();
  DBUG_VOID_RETURN;
}

size_t View_change_log_event::get_data_size()
{
  DBUG_ENTER("View_change_log_event::get_data_size");

  size_t size= Binary_log_event::VIEW_CHANGE_HEADER_LEN;
  size+= get_size_data_map(&certification_info);

  DBUG_RETURN(size);
}

size_t
View_change_log_event::get_size_data_map(std::map<std::string, std::string> *map)
{
  DBUG_ENTER("View_change_log_event::get_size_data_map");
  size_t size= 0;

  std::map<std::string, std::string>::iterator iter;
  size+= (ENCODED_CERT_INFO_KEY_SIZE_LEN +
          ENCODED_CERT_INFO_VALUE_LEN) * map->size();
  for (iter= map->begin(); iter!= map->end(); iter++)
    size+= iter->first.length() + iter->second.length();

  DBUG_RETURN(size);
}

size_t View_change_log_event::to_string(char *buf, ulong len) const
{
  DBUG_ENTER("View_change_log_event::to_string");
  DBUG_RETURN(my_snprintf(buf, len, "view_id=%s", view_id));
}

#ifndef MYSQL_CLIENT
int View_change_log_event::pack_info(Protocol *protocol)
{
  DBUG_ENTER("View_change_log_event::pack_info");
  char buf[256];
  size_t bytes= to_string(buf, 256);
  protocol->store(buf, bytes, &my_charset_bin);
  DBUG_RETURN(0);
}
#endif

#ifdef MYSQL_CLIENT
void View_change_log_event::print(FILE *file,
                                  PRINT_EVENT_INFO *print_event_info)
{
  DBUG_ENTER("View_change_log_event::print");
  char buf[256];
  IO_CACHE *const head= &print_event_info->head_cache;

  if (!print_event_info->short_form)
  {
    to_string(buf, 256);
    print_header(head, print_event_info, FALSE);
    my_b_printf(head, "View_change_log_event: %s\n", buf);
  }
  DBUG_VOID_RETURN;
}
#endif

#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)

int View_change_log_event::do_apply_event(Relay_log_info const *rli)
{
  enum_gtid_statement_status state= gtid_pre_statement_checks(thd);
  if (state == GTID_STATEMENT_SKIP)
    return 0;

  if (state == GTID_STATEMENT_CANCEL ||
         (state == GTID_STATEMENT_EXECUTE &&
          gtid_pre_statement_post_implicit_commit_checks(thd)))
  {
    uint error= thd->get_stmt_da()->mysql_errno();
    DBUG_ASSERT(error != 0);
    rli->report(ERROR_LEVEL, error,
                "Error executing View Change event: '%s'",
                thd->get_stmt_da()->message_text());
    thd->is_slave_error= 1;
    return -1;
  }

  if (!opt_bin_log)
  {
    return 0;
  }

  int error= mysql_bin_log.write_event(this);
  if (error)
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, ER(ER_SLAVE_FATAL_ERROR),
                "Could not write the VIEW CHANGE event in the binary log.");

  return (error);
}

int View_change_log_event::do_update_pos(Relay_log_info *rli)
{
  DBUG_ENTER("View_change_log_event::do_update_pos");
  rli->inc_event_relay_log_pos();
  DBUG_RETURN(0);
}
#endif

#ifndef MYSQL_CLIENT
bool View_change_log_event::write_data_header(IO_CACHE* file){
  DBUG_ENTER("View_change_log_event::write_data_header");
  char buf[Binary_log_event::VIEW_CHANGE_HEADER_LEN];

  memcpy(buf, view_id, ENCODED_VIEW_ID_MAX_LEN);
  int8store(buf + ENCODED_SEQ_NUMBER_OFFSET, seq_number);
  int4store(buf + ENCODED_CERT_INFO_SIZE_OFFSET, certification_info.size());
  DBUG_RETURN(wrapper_my_b_safe_write(file,(const uchar *) buf,
                                      Binary_log_event::VIEW_CHANGE_HEADER_LEN));
}

bool View_change_log_event::write_data_body(IO_CACHE* file){
  DBUG_ENTER("Transaction_context_log_event::write_data_body");

  if (write_data_map(file, &certification_info))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

bool View_change_log_event::write_data_map(IO_CACHE* file,
                                           std::map<std::string, std::string> *map)
{
  DBUG_ENTER("View_change_log_event::write_data_set");
  bool result= false;

  std::map<std::string, std::string>::iterator iter;
  for (iter= map->begin(); iter!= map->end(); iter++)
  {
    uchar buf_key_len[ENCODED_CERT_INFO_KEY_SIZE_LEN];
    uint16 key_len= iter->first.length();
    int2store(buf_key_len, key_len);

    const char *key= iter->first.c_str();

    uchar buf_value_len[ENCODED_CERT_INFO_VALUE_LEN];
    uint32 value_len= iter->second.length();
    int4store(buf_value_len, value_len);

    const char *value= iter->second.c_str();

    if (wrapper_my_b_safe_write(file, buf_key_len,
                                ENCODED_CERT_INFO_KEY_SIZE_LEN) ||
        wrapper_my_b_safe_write(file, (const uchar*) key, key_len) ||
        wrapper_my_b_safe_write(file, buf_value_len,
                                ENCODED_CERT_INFO_VALUE_LEN) ||
        wrapper_my_b_safe_write(file, (const uchar*) value, value_len))
      DBUG_RETURN(result);
  }

  DBUG_RETURN(false);
}

#endif

/*
  Updates the certification info map.
*/
void
View_change_log_event::set_certification_info(std::map<std::string, std::string> *info)
{
  DBUG_ENTER("View_change_log_event::set_certification_database_snapshot");
  certification_info.clear();

  std::map<std::string, std::string>::iterator it;
  for(it= info->begin(); it != info->end(); ++it)
  {
    std::string key= it->first;
    std::string value= it->second;
    certification_info[key]= value;
  }

  DBUG_VOID_RETURN;
}


#ifdef MYSQL_CLIENT
/**
  The default values for these variables should be values that are
  *incorrect*, i.e., values that cannot occur in an event.  This way,
  they will always be printed for the first event.
*/
st_print_event_info::st_print_event_info()
  :flags2_inited(0), sql_mode_inited(0), sql_mode(0),
   auto_increment_increment(0),auto_increment_offset(0), charset_inited(0),
   lc_time_names_number(~0),
   charset_database_number(ILLEGAL_CHARSET_INFO_NUMBER),
   thread_id(0), thread_id_printed(false),
   base64_output_mode(BASE64_OUTPUT_UNSPEC), printed_fd_event(FALSE),
   have_unflushed_events(false), skipped_event_in_transaction(false)
{
  /*
    Currently we only use static PRINT_EVENT_INFO objects, so zeroed at
    program's startup, but these explicit memset() is for the day someone
    creates dynamic instances.
  */
  memset(db, 0, sizeof(db));
  memset(charset, 0, sizeof(charset));
  memset(time_zone_str, 0, sizeof(time_zone_str));
  delimiter[0]= ';';
  delimiter[1]= 0;
  myf const flags = MYF(MY_WME | MY_NABP);
  open_cached_file(&head_cache, NULL, NULL, 0, flags);
  open_cached_file(&body_cache, NULL, NULL, 0, flags);
  open_cached_file(&footer_cache, NULL, NULL, 0, flags);
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
Heartbeat_log_event::Heartbeat_log_event(const char* buf, uint event_len,
                                         const Format_description_event*
                                         description_event)
  : binary_log::Heartbeat_event(buf, event_len, description_event),
    Log_event(header(), footer())
{
  if ((log_ident != NULL && header()->log_pos >= BIN_LOG_HEADER_SIZE))
    is_valid_param= true;
}
#endif

#ifdef MYSQL_SERVER
/*
  This is a utility function that adds a quoted identifier into the a buffer.
  This also escapes any existance of the quote string inside the identifier.

  SYNOPSIS
    my_strmov_quoted_identifier
    thd                   thread handler
    buffer                target buffer
    identifier            the identifier to be quoted
    length                length of the identifier
*/
size_t my_strmov_quoted_identifier(THD* thd, char *buffer,
                                   const char* identifier,
                                   size_t length)
{
  int q= thd ? get_quote_char_for_identifier(thd, identifier, length) : '`';
  return my_strmov_quoted_identifier_helper(q, buffer, identifier, length);
}
#else
size_t my_strmov_quoted_identifier(char *buffer,  const char* identifier)
{
  int q= '`';
  return my_strmov_quoted_identifier_helper(q, buffer, identifier, 0);
}

#endif

size_t my_strmov_quoted_identifier_helper(int q, char *buffer,
                                          const char* identifier,
                                          size_t length)
{
  size_t written= 0;
  char quote_char;
  size_t id_length= (length) ? length : strlen(identifier);

  if (q == EOF)
  {
    (void) strncpy(buffer, identifier, id_length);
    return id_length;
  }
  quote_char= (char) q;
  *buffer++= quote_char;
  written++;
  while (id_length--)
  {
    if (*identifier == quote_char)
    {
      *buffer++= quote_char;
      written++;
    }
    *buffer++= *identifier++;
    written++;
  }
  *buffer++= quote_char;
  return ++written;
}

