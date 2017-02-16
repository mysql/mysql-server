/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

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


#ifdef MYSQL_CLIENT

#include "sql_priv.h"
#include "mysqld_error.h"

#else

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
#include "unireg.h"
#include "my_global.h" // REQUIRED by log_event.h > m_string.h > my_bitmap.h
#include "log_event.h"
#include "sql_base.h"                           // close_thread_tables
#include "sql_cache.h"                       // QUERY_CACHE_FLAGS_SIZE
#include "sql_locale.h" // MY_LOCALE, my_locale_by_number, my_locale_en_US
#include "key.h"        // key_copy
#include "lock.h"       // mysql_unlock_tables
#include "sql_parse.h"  // mysql_test_parse_for_slave
#include "tztime.h"     // struct Time_zone
#include "sql_load.h"   // mysql_load
#include "sql_db.h"     // load_db_opt_by_name
#include "slave.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "rpl_filter.h"
#include "rpl_record.h"
#include "transaction.h"
#include <my_dir.h>
#include "sql_show.h"    // append_identifier

#endif /* MYSQL_CLIENT */

#include <base64.h>
#include <my_bitmap.h>
#include "rpl_utility.h"


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

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")


/*
  Size of buffer for printing a double in format %.<PREC>g

  optional '-' + optional zero + '.'  + PREC digits + 'e' + sign +
  exponent digits + '\0'
*/
#define FMT_G_BUFSIZE(PREC) (3 + (PREC) + 5 + 1)

/*
  Explicit instantiation to unsigned int of template available_buffer
  function.
*/
template unsigned int available_buffer<unsigned int>(const char*,
                                                     const char*,
                                                     unsigned int);

/*
  Explicit instantiation to unsigned int of template valid_buffer_range
  function.
*/
template bool valid_buffer_range<unsigned int>(unsigned int,
                                               const char*,
                                               const char*,
                                               unsigned int);

/* 
   replication event checksum is introduced in the following "checksum-home" version.
   The checksum-aware servers extract FD's version to decide whether the FD event
   carries checksum info.

   TODO: correct the constant when it has been determined 
   (which main tree to push and when) 
*/
const uchar checksum_version_split_mysql[3]= {5, 6, 1};
const ulong checksum_version_product_mysql=
  (checksum_version_split_mysql[0] * 256 +
   checksum_version_split_mysql[1]) * 256 +
  checksum_version_split_mysql[2];
const uchar checksum_version_split_mariadb[3]= {5, 3, 0};
const ulong checksum_version_product_mariadb=
  (checksum_version_split_mariadb[0] * 256 +
   checksum_version_split_mariadb[1]) * 256 +
  checksum_version_split_mariadb[2];

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
  case HA_ERR_TO_BIG_ROW: return "HA_ERR_TO_BIG_ROW";
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
  char buff[MAX_SLAVE_ERRMSG], *slider;
  const char *buff_end= buff + sizeof(buff);
  uint len;
  List_iterator_fast<MYSQL_ERROR> it(thd->warning_info->warn_list());
  MYSQL_ERROR *err;
  buff[0]= 0;

  for (err= it++, slider= buff; err && slider < buff_end - 1;
       slider += len, err= it++)
  {
    len= my_snprintf(slider, buff_end - slider,
                     " %s, Error_code: %d;", err->get_message_text(),
                     err->get_sql_errno());
  }

  if (ha_error != 0)
    rli->report(level, thd->is_error() ? thd->stmt_da->sql_errno() : 0,
                "Could not execute %s event on table %s.%s;"
                "%s handler error %s; "
                "the event's master log %s, end_log_pos %lu",
                type, table->s->db.str, table->s->table_name.str,
                buff, handler_error == NULL ? "<unknown>" : handler_error,
                log_name, pos);
  else
    rli->report(level, thd->is_error() ? thd->stmt_da->sql_errno() : 0,
                "Could not execute %s event on table %s.%s;"
                "%s the event's master log %s, end_log_pos %lu",
                type, table->s->db.str, table->s->table_name.str,
                buff, log_name, pos);
}
#endif

/*
  Cache that will automatically be written to a dedicated file on
  destruction.

  DESCRIPTION

 */
class Write_on_release_cache
{
public:
  enum flag
  {
    FLUSH_F
  };

  typedef unsigned short flag_set;

  /*
    Constructor.

    SYNOPSIS
      Write_on_release_cache
      cache  Pointer to cache to use
      file   File to write cache to upon destruction
      flags  Flags for the cache

    DESCRIPTION

      Class used to guarantee copy of cache to file before exiting the
      current block.  On successful copy of the cache, the cache will
      be reinited as a WRITE_CACHE.

      Currently, a pointer to the cache is provided in the
      constructor, but it would be possible to create a subclass
      holding the IO_CACHE itself.
   */
  Write_on_release_cache(IO_CACHE *cache, FILE *file, flag_set flags = 0)
    : m_cache(cache), m_file(file), m_flags(flags)
  {
    reinit_io_cache(m_cache, WRITE_CACHE, 0L, FALSE, TRUE);
  }

  ~Write_on_release_cache()
  {
    copy_event_cache_to_file_and_reinit(m_cache, m_file);
    if (m_flags | FLUSH_F)
      fflush(m_file);
  }

  /*
    Return a pointer to the internal IO_CACHE.

    SYNOPSIS
      operator&()

    DESCRIPTION

      Function to return a pointer to the internal cache, so that the
      object can be treated as a IO_CACHE and used with the my_b_*
      IO_CACHE functions

    RETURN VALUE
      A pointer to the internal IO_CACHE.
   */
  IO_CACHE *operator&()
  {
    return m_cache;
  }

private:
  // Hidden, to prevent usage.
  Write_on_release_cache(Write_on_release_cache const&);

  IO_CACHE *m_cache;
  FILE *m_file;
  flag_set m_flags;
};

#ifndef DBUG_OFF
uint debug_not_change_ts_if_art_event= 1; // bug#29309 simulation
#endif

/*
  pretty_print_str()
*/

#ifdef MYSQL_CLIENT
static void pretty_print_str(IO_CACHE* cache, const char* str, int len)
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

inline int ignored_error_code(int err_code)
{
#ifdef HAVE_NDB_BINLOG
  /*
    The following error codes are hard-coded and will always be ignored.
  */
  switch (err_code)
  {
  case ER_DB_CREATE_EXISTS:
  case ER_DB_DROP_EXISTS:
    return 1;
  default:
    /* Nothing to do */
    break;
  }
#endif
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
  uint actual_error= (thd->is_error() ? thd->stmt_da->sql_errno() :
                           0);

  if (actual_error == 0)
  {
    table->file->print_error(error, MYF(0));
    actual_error= (thd->is_error() ? thd->stmt_da->sql_errno() :
                        ER_UNKNOWN_ERROR);
    if (actual_error == ER_UNKNOWN_ERROR)
      if (global_system_variables.log_warnings)
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
  case ER_CONNECTION_KILLED:
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

static void
pretty_print_str(String *packet, const char *str, int len)
{
  const char *end= str + len;
  packet->append(STRING_WITH_LEN("'"));
  while (str < end)
  {
    char c;
    switch ((c=*str++)) {
    case '\n': packet->append(STRING_WITH_LEN("\\n")); break;
    case '\r': packet->append(STRING_WITH_LEN("\\r")); break;
    case '\\': packet->append(STRING_WITH_LEN("\\\\")); break;
    case '\b': packet->append(STRING_WITH_LEN("\\b")); break;
    case '\t': packet->append(STRING_WITH_LEN("\\t")); break;
    case '\'': packet->append(STRING_WITH_LEN("\\'")); break;
    case 0   : packet->append(STRING_WITH_LEN("\\0")); break;
    default:
      packet->append(&c, 1);
      break;
    }
  }
  packet->append(STRING_WITH_LEN("'"));
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

  buf = strend(buf);
  buf = int10_to_str(::server_id, buf, 10);
  *buf++ = '-';
  buf = int10_to_str(event_server_id, buf, 10);
  *buf++ = '-';
  res= int10_to_str(file_id, buf, 10);
  strmov(res, ext);                             // Add extension last
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
  char fname[FN_REFLEN], prefbuf[31], *p;

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
  p= int10_to_str(::server_id, p, 10);
  *(p++)= '-';
  *p= 0;

  for (i=0 ; i < (uint)dirp->number_off_files; i++)
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
  write_str()
*/

static bool write_str(IO_CACHE *file, const char *str, uint length)
{
  uchar tmp[1];
  tmp[0]= (uchar) length;
  return (my_b_safe_write(file, tmp, sizeof(tmp)) ||
	  my_b_safe_write(file, (uchar*) str, length));
}


/*
  read_str()
*/

static inline int read_str(const char **buf, const char *buf_end,
                           const char **str, uint8 *len)
{
  if (*buf + ((uint) (uchar) **buf) >= buf_end)
    return 1;
  *len= (uint8) **buf;
  *str= (*buf)+1;
  (*buf)+= (uint) *len+1;
  return 0;
}


/**
  Transforms a string into "" or its expression in X'HHHH' form.
*/

char *str_to_hex(char *to, const char *from, uint len)
{
  if (len)
  {
    *to++= 'X';
    *to++= '\'';
    to= octet2hex(to, from, len);
    *to++= '\'';
    *to= '\0';
  }
  else
    to= strmov(to, "\"\"");
  return to;                               // pointer to end 0 of 'to'
}

#ifndef MYSQL_CLIENT

/**
  Append a version of the 'from' string suitable for use in a query to
  the 'to' string.  To generate a correct escaping, the character set
  information in 'csinfo' is used.
*/

int
append_query_string(THD *thd, CHARSET_INFO *csinfo,
                    String const *from, String *to)
{
  char *beg, *ptr;
  uint32 const orig_len= to->length();
  if (to->reserve(orig_len + from->length() * 2 + 4))
    return 1;

  beg= (char*) to->ptr() + to->length();
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
    my_b_printf(file,"%s=%d", name, test(flags & option));
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
  case START_EVENT_V3:  return "Start_v3";
  case STOP_EVENT:   return "Stop";
  case QUERY_EVENT:  return "Query";
  case ROTATE_EVENT: return "Rotate";
  case INTVAR_EVENT: return "Intvar";
  case LOAD_EVENT:   return "Load";
  case NEW_LOAD_EVENT:   return "New_load";
  case SLAVE_EVENT:  return "Slave";
  case CREATE_FILE_EVENT: return "Create_file";
  case APPEND_BLOCK_EVENT: return "Append_block";
  case DELETE_FILE_EVENT: return "Delete_file";
  case EXEC_LOAD_EVENT: return "Exec_load";
  case RAND_EVENT: return "RAND";
  case XID_EVENT: return "Xid";
  case USER_VAR_EVENT: return "User var";
  case FORMAT_DESCRIPTION_EVENT: return "Format_desc";
  case TABLE_MAP_EVENT: return "Table_map";
  case PRE_GA_WRITE_ROWS_EVENT: return "Write_rows_event_old";
  case PRE_GA_UPDATE_ROWS_EVENT: return "Update_rows_event_old";
  case PRE_GA_DELETE_ROWS_EVENT: return "Delete_rows_event_old";
  case WRITE_ROWS_EVENT: return "Write_rows";
  case UPDATE_ROWS_EVENT: return "Update_rows";
  case DELETE_ROWS_EVENT: return "Delete_rows";
  case BEGIN_LOAD_QUERY_EVENT: return "Begin_load_query";
  case EXECUTE_LOAD_QUERY_EVENT: return "Execute_load_query";
  case INCIDENT_EVENT: return "Incident";
  case ANNOTATE_ROWS_EVENT: return "Annotate_rows";
  default: return "Unknown";				/* impossible */
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
Log_event::Log_event(THD* thd_arg, uint16 flags_arg, bool using_trans)
  :log_pos(0), temp_buf(0), exec_time(0),
   crc(0), thd(thd_arg),
   checksum_alg(BINLOG_CHECKSUM_ALG_UNDEF)
{
  server_id=	thd->server_id;
  when=         thd->start_time;
  when_sec_part=thd->start_time_sec_part;

  if (using_trans)
    cache_type= Log_event::EVENT_TRANSACTIONAL_CACHE;
  else
    cache_type= Log_event::EVENT_STMT_CACHE;
  flags= flags_arg |
    (thd->variables.option_bits & OPTION_SKIP_REPLICATION ?
     LOG_EVENT_SKIP_REPLICATION_F : 0);
}

/**
  This minimal constructor is for when you are not even sure that there
  is a valid THD. For example in the server when we are shutting down or
  flushing logs after receiving a SIGHUP (then we must write a Rotate to
  the binlog but we have no THD, so we need this minimal constructor).
*/

Log_event::Log_event()
  :temp_buf(0), exec_time(0), flags(0),
   cache_type(Log_event::EVENT_INVALID_CACHE), crc(0),
   thd(0), checksum_alg(BINLOG_CHECKSUM_ALG_UNDEF)
{
  server_id=	::server_id;
  /*
    We can't call my_time() here as this would cause a call before
    my_init() is called
  */
  when=         0;
  when_sec_part=0;
  log_pos=	0;
}
#endif /* !MYSQL_CLIENT */


/*
  Log_event::Log_event()
*/

Log_event::Log_event(const char* buf,
                     const Format_description_log_event* description_event)
  :temp_buf(0), cache_type(Log_event::EVENT_INVALID_CACHE),
    crc(0), checksum_alg(BINLOG_CHECKSUM_ALG_UNDEF)
{
#ifndef MYSQL_CLIENT
  thd = 0;
#endif
  when = uint4korr(buf);
  when_sec_part= 0;
  server_id = uint4korr(buf + SERVER_ID_OFFSET);
  data_written= uint4korr(buf + EVENT_LEN_OFFSET);
  if (description_event->binlog_version==1)
  {
    log_pos= 0;
    flags= 0;
    return;
  }
  /* 4.0 or newer */
  log_pos= uint4korr(buf + LOG_POS_OFFSET);
  /*
    If the log is 4.0 (so here it can only be a 4.0 relay log read by
    the SQL thread or a 4.0 master binlog read by the I/O thread),
    log_pos is the beginning of the event: we transform it into the end
    of the event, which is more useful.
    But how do you know that the log is 4.0: you know it if
    description_event is version 3 *and* you are not reading a
    Format_desc (remember that mysqlbinlog starts by assuming that 5.0
    logs are in 4.0 format, until it finds a Format_desc).
  */
  if (description_event->binlog_version==3 &&
      (uchar)buf[EVENT_TYPE_OFFSET]<FORMAT_DESCRIPTION_EVENT && log_pos)
  {
      /*
        If log_pos=0, don't change it. log_pos==0 is a marker to mean
        "don't change rli->group_master_log_pos" (see
        inc_group_relay_log_pos()). As it is unreal log_pos, adding the
        event len's is nonsense. For example, a fake Rotate event should
        not have its log_pos (which is 0) changed or it will modify
        Exec_master_log_pos in SHOW SLAVE STATUS, displaying a nonsense
        value of (a non-zero offset which does not exist in the master's
        binlog, so which will cause problems if the user uses this value
        in CHANGE MASTER).
      */
    log_pos+= data_written; /* purecov: inspected */
  }
  DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));

  flags= uint2korr(buf + FLAGS_OFFSET);
  if (((uchar)buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT) ||
      ((uchar)buf[EVENT_TYPE_OFFSET] == ROTATE_EVENT))
  {
    /*
      These events always have a header which stops here (i.e. their
      header is FROZEN).
    */
    /*
      Initialization to zero of all other Log_event members as they're
      not specified. Currently there are no such members; in the future
      there will be an event UID (but Format_description and Rotate
      don't need this UID, as they are not propagated through
      --log-slave-updates (remember the UID is used to not play a query
      twice when you have two masters which are slaves of a 3rd master).
      Then we are done.
    */
    return;
  }
  /* otherwise, go on with reading the header from buf (nothing now) */
}

#ifndef MYSQL_CLIENT
#ifdef HAVE_REPLICATION

int Log_event::do_update_pos(Relay_log_info *rli)
{
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
  if (rli)
  {
    /*
      bug#29309 simulation: resetting the flag to force
      wrong behaviour of artificial event to update
      rli->last_master_timestamp for only one time -
      the first FLUSH LOGS in the test.
    */
    DBUG_EXECUTE_IF("let_first_flush_log_change_timestamp",
                    if (debug_not_change_ts_if_art_event == 1
                        && is_artificial_event())
                      debug_not_change_ts_if_art_event= 0; );
    rli->stmt_done(log_pos, is_artificial_event() &&
                   IF_DBUG(debug_not_change_ts_if_art_event > 0, 1) ?
                     0 : when);
    DBUG_EXECUTE_IF("let_first_flush_log_change_timestamp",
                    if (debug_not_change_ts_if_art_event == 0)
                      debug_not_change_ts_if_art_event= 2; );
  }
  return 0;                                   // Cannot fail currently
}


Log_event::enum_skip_reason
Log_event::do_shall_skip(Relay_log_info *rli)
{
  DBUG_PRINT("info", ("ev->server_id=%lu, ::server_id=%lu,"
                      " rli->replicate_same_server_id=%d,"
                      " rli->slave_skip_counter=%d",
                      (ulong) server_id, (ulong) ::server_id,
                      rli->replicate_same_server_id,
                      rli->slave_skip_counter));
  if ((server_id == ::server_id && !rli->replicate_same_server_id) ||
      (rli->slave_skip_counter == 1 && rli->is_in_group()) ||
      (flags & LOG_EVENT_SKIP_REPLICATION_F &&
       opt_replicate_events_marked_for_skip != RPL_SKIP_REPLICATE))
    return EVENT_SKIP_IGNORE;
  if (rli->slave_skip_counter > 0)
    return EVENT_SKIP_COUNT;
  return EVENT_SKIP_NOT;
}


/*
  Log_event::pack_info()
*/

void Log_event::pack_info(THD *thd, Protocol *protocol)
{
  protocol->store("", &my_charset_bin);
}


/**
  Only called by SHOW BINLOG EVENTS
*/
int Log_event::net_send(THD *thd, Protocol *protocol, const char* log_name,
                        my_off_t pos)
{
  const char *p= strrchr(log_name, FN_LIBCHAR);
  const char *event_type;
  if (p)
    log_name = p + 1;

  protocol->prepare_for_resend();
  protocol->store(log_name, &my_charset_bin);
  protocol->store((ulonglong) pos);
  event_type = get_type_str();
  protocol->store(event_type, strlen(event_type), &my_charset_bin);
  protocol->store((uint32) server_id);
  protocol->store((ulonglong) log_pos);
  pack_info(thd, protocol);
  return protocol->write();
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
  my_bool ret;
  /* 
     few callers of Log_event::write 
     (incl FD::write, FD constructing code on the slave side, Rotate relay log
     and Stop event) 
     provides their checksum alg preference through Log_event::checksum_alg.
  */
  ret= ((checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF) ?
        (checksum_alg != BINLOG_CHECKSUM_ALG_OFF) :
        ((binlog_checksum_options != BINLOG_CHECKSUM_ALG_OFF) &&
         (cache_type == Log_event::EVENT_NO_CACHE)) ?
        test(binlog_checksum_options) : FALSE);

  /*
    FD calls the methods before data_written has been calculated.
    The following invariant claims if the current is not the first
    call (and therefore data_written is not zero) then `ret' must be
    TRUE. It may not be null because FD is always checksummed.
  */
  
  DBUG_ASSERT(get_type_code() != FORMAT_DESCRIPTION_EVENT || ret ||
              data_written == 0);

  if (checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF)
    checksum_alg= ret ? // calculated value stored
      (uint8) binlog_checksum_options : (uint8) BINLOG_CHECKSUM_ALG_OFF;

  DBUG_ASSERT(!ret || 
              ((checksum_alg == binlog_checksum_options ||
               /* 
                  Stop event closes the relay-log and its checksum alg
                  preference is set by the caller can be different
                  from the server's binlog_checksum_options.
               */
               get_type_code() == STOP_EVENT ||
               /* 
                  Rotate:s can be checksummed regardless of the server's
                  binlog_checksum_options. That applies to both
                  the local RL's Rotate and the master's Rotate
                  which IO thread instantiates via queue_binlog_ver_3_event.
               */
               get_type_code() == ROTATE_EVENT
               ||  /* FD is always checksummed */
               get_type_code() == FORMAT_DESCRIPTION_EVENT) && 
               checksum_alg != BINLOG_CHECKSUM_ALG_OFF));

  DBUG_ASSERT(checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);

  DBUG_ASSERT(((get_type_code() != ROTATE_EVENT &&
                get_type_code() != STOP_EVENT) ||
               get_type_code() != FORMAT_DESCRIPTION_EVENT) ||
              cache_type == Log_event::EVENT_NO_CACHE);

  DBUG_RETURN(ret);
}

bool Log_event::wrapper_my_b_safe_write(IO_CACHE* file, const uchar* buf, ulong size)
{
  if (need_checksum() && size != 0)
    crc= my_checksum(crc, buf, size);

  return my_b_safe_write(file, buf, size);
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

/*
  Log_event::write()
*/

bool Log_event::write_header(IO_CACHE* file, ulong event_data_length)
{
  uchar header[LOG_EVENT_HEADER_LEN];
  ulong now;
  bool ret;
  DBUG_ENTER("Log_event::write_header");
  DBUG_PRINT("enter", ("filepos: %lld  length: %lu type: %d",
                       (longlong) my_b_tell(file), event_data_length,
                       (int) get_type_code()));

  /* Store number of bytes that will be written by this event */
  data_written= event_data_length + sizeof(header);

  if (need_checksum())
  {
    crc= my_checksum(0L, NULL, 0);
    data_written += BINLOG_CHECKSUM_LEN;
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
    log_pos= 0;
  }
  else  if (!log_pos)
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

    log_pos= my_b_safe_tell(file)+data_written;
  }

  now= get_time();                               // Query start time

  /*
    Header will be of size LOG_EVENT_HEADER_LEN for all events, except for
    FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT, where it will be
    LOG_EVENT_MINIMAL_HEADER_LEN (remember these 2 have a frozen header,
    because we read them before knowing the format).
  */

  int4store(header, now);              // timestamp
  header[EVENT_TYPE_OFFSET]= get_type_code();
  int4store(header+ SERVER_ID_OFFSET, server_id);
  int4store(header+ EVENT_LEN_OFFSET, data_written);
  int4store(header+ LOG_POS_OFFSET, log_pos);
  /*
    recording checksum of FD event computed with dropped
    possibly active LOG_EVENT_BINLOG_IN_USE_F flag.
    Similar step at verication: the active flag is dropped before
    checksum computing.
  */
  if (header[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT ||
      !need_checksum() || !(flags & LOG_EVENT_BINLOG_IN_USE_F))
  {
    int2store(header+ FLAGS_OFFSET, flags);
    ret= wrapper_my_b_safe_write(file, header, sizeof(header)) != 0;
  }
  else
  {
    ret= (wrapper_my_b_safe_write(file, header, FLAGS_OFFSET) != 0);
    if (!ret)
    {
      flags &= ~LOG_EVENT_BINLOG_IN_USE_F;
      int2store(header + FLAGS_OFFSET, flags);
      crc= my_checksum(crc, header + FLAGS_OFFSET, sizeof(flags));
      flags |= LOG_EVENT_BINLOG_IN_USE_F;    
      int2store(header + FLAGS_OFFSET, flags);
      ret= (my_b_safe_write(file, header + FLAGS_OFFSET, sizeof(flags)) != 0);
    }
    if (!ret)
      ret= (wrapper_my_b_safe_write(file, header + FLAGS_OFFSET + sizeof(flags),
                                    sizeof(header)
                                    - (FLAGS_OFFSET + sizeof(flags))) != 0);
  }
  DBUG_RETURN( ret);
}


/**
  This needn't be format-tolerant, because we only read
  LOG_EVENT_MINIMAL_HEADER_LEN (we just want to read the event's length).
*/

int Log_event::read_log_event(IO_CACHE* file, String* packet,
                              mysql_mutex_t* log_lock,
                              uint8 checksum_alg_arg,
                              const char *log_file_name_arg,
                              bool* is_binlog_active)
{
  ulong data_len;
  int result=0;
  char buf[LOG_EVENT_MINIMAL_HEADER_LEN];
  uchar ev_offset= packet->length();
  DBUG_ENTER("Log_event::read_log_event");

  if (log_lock)
    mysql_mutex_lock(log_lock);

  if (log_file_name_arg)
    *is_binlog_active= mysql_bin_log.is_active(log_file_name_arg);

  if (my_b_read(file, (uchar*) buf, sizeof(buf)))
  {
    /*
      If the read hits eof, we must report it as eof so the caller
      will know it can go into cond_wait to be woken up on the next
      update to the log.
    */
    DBUG_PRINT("error",("file->error: %d", file->error));
    if (!file->error)
      result= LOG_READ_EOF;
    else
      result= (file->error > 0 ? LOG_READ_TRUNC : LOG_READ_IO);
    goto end;
  }
  data_len= uint4korr(buf + EVENT_LEN_OFFSET);
  if (data_len < LOG_EVENT_MINIMAL_HEADER_LEN ||
      data_len > max(current_thd->variables.max_allowed_packet,
                     opt_binlog_rows_event_max_size + MAX_LOG_EVENT_HEADER))
  {
    DBUG_PRINT("error",("data_len: %lu", data_len));
    result= ((data_len < LOG_EVENT_MINIMAL_HEADER_LEN) ? LOG_READ_BOGUS :
	     LOG_READ_TOO_LARGE);
    goto end;
  }

  /* Append the log event header to packet */
  if (packet->append(buf, sizeof(buf)))
  {
    /* Failed to allocate packet */
    result= LOG_READ_MEM;
    goto end;
  }
  data_len-= LOG_EVENT_MINIMAL_HEADER_LEN;
  if (data_len)
  {
    /* Append rest of event, read directly from file into packet */
    if (packet->append(file, data_len))
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
      result= (my_errno == ENOMEM ? LOG_READ_MEM :
               (file->error >= 0 ? LOG_READ_TRUNC: LOG_READ_IO));
      /* Implicit goto end; */
    }
    else
    {
      /* Corrupt the event for Dump thread*/
      DBUG_EXECUTE_IF("corrupt_read_log_event2",
	uchar *debug_event_buf_c = (uchar*) packet->ptr() + ev_offset;
        if (debug_event_buf_c[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT)
        {
          int debug_cor_pos = rand() % (data_len + sizeof(buf) - BINLOG_CHECKSUM_LEN);
          debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
          DBUG_PRINT("info", ("Corrupt the event at Log_event::read_log_event: byte on position %d", debug_cor_pos));
          DBUG_SET("-d,corrupt_read_log_event2");
	}
      );                                                                                           
      /*
        CRC verification of the Dump thread
      */
      if (opt_master_verify_checksum &&
          event_checksum_test((uchar*) packet->ptr() + ev_offset,
                              data_len + sizeof(buf),
                              checksum_alg_arg))
      {
        result= LOG_READ_CHECKSUM_FAILURE;
        goto end;
      }
    }
  }

end:
  if (log_lock)
    mysql_mutex_unlock(log_lock);
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
                                     my_bool crc_check)
#endif
{
  DBUG_ENTER("Log_event::read_log_event");
  DBUG_ASSERT(description_event != 0);
  char head[LOG_EVENT_MINIMAL_HEADER_LEN];
  /*
    First we only want to read at most LOG_EVENT_MINIMAL_HEADER_LEN, just to
    check the event for sanity and to know its length; no need to really parse
    it. We say "at most" because this could be a 3.23 master, which has header
    of 13 bytes, whereas LOG_EVENT_MINIMAL_HEADER_LEN is 19 bytes (it's
    "minimal" over the set {MySQL >=4.0}).
  */
  uint header_size= min(description_event->common_header_len,
                        LOG_EVENT_MINIMAL_HEADER_LEN);

  LOCK_MUTEX;
  DBUG_PRINT("info", ("my_b_tell: %lu", (ulong) my_b_tell(file)));
  if (my_b_read(file, (uchar *) head, header_size))
  {
    DBUG_PRINT("info", ("Log_event::read_log_event(IO_CACHE*,Format_desc*) \
failed my_b_read"));
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
#ifndef max_allowed_packet
  THD *thd=current_thd;
  uint max_allowed_packet= thd ? slave_max_allowed_packet:~(uint)0;
#endif

  if (data_len > max(max_allowed_packet,
                     opt_binlog_rows_event_max_size + MAX_LOG_EVENT_HEADER))
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
  if (!(buf = (char*) my_malloc(data_len+1, MYF(MY_WME))))
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
  if ((res= read_log_event(buf, data_len, &error, description_event, crc_check)))
    res->register_temp_buf(buf, TRUE);

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
  Log_event* ev;
  uint8 alg;
  DBUG_ENTER("Log_event::read_log_event(char*,...)");
  DBUG_ASSERT(description_event != 0);
  DBUG_PRINT("info", ("binlog_version: %d", description_event->binlog_version));
  DBUG_DUMP("data", (unsigned char*) buf, event_len);

  /* Check the integrity */
  if (event_len < EVENT_LEN_OFFSET ||
      (uchar)buf[EVENT_TYPE_OFFSET] >= ENUM_END_EVENT ||
      (uint) event_len != uint4korr(buf+EVENT_LEN_OFFSET))
  {
    *error="Sanity check failed";		// Needed to free buffer
    DBUG_RETURN(NULL); // general sanity check - will fail on a partial read
  }

  uint event_type= (uchar)buf[EVENT_TYPE_OFFSET];
  // all following START events in the current file are without checksum
  if (event_type == START_EVENT_V3)
    (const_cast< Format_description_log_event *>(description_event))->checksum_alg= BINLOG_CHECKSUM_ALG_OFF;
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
  alg= (event_type != FORMAT_DESCRIPTION_EVENT) ?
    description_event->checksum_alg : get_checksum_alg(buf, event_len);
  // Emulate the corruption during reading an event
  DBUG_EXECUTE_IF("corrupt_read_log_event_char",
    if (event_type != FORMAT_DESCRIPTION_EVENT)
    {
      char *debug_event_buf_c = (char *)buf;
      int debug_cor_pos = rand() % (event_len - BINLOG_CHECKSUM_LEN);
      debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
      DBUG_PRINT("info", ("Corrupt the event at Log_event::read_log_event(char*,...): byte on position %d", debug_cor_pos));
      DBUG_SET("-d,corrupt_read_log_event_char");
    }
  );                                                 
  if (crc_check &&
      event_checksum_test((uchar *) buf, event_len, alg))
  {
#ifdef MYSQL_CLIENT
    *error= "Event crc check failed! Most likely there is event corruption.";
    if (force_opt)
    {
      ev= new Unknown_log_event(buf, description_event);
      DBUG_RETURN(ev);
    }
    else
      DBUG_RETURN(NULL);
#else
    *error= ER(ER_BINLOG_READ_EVENT_CHECKSUM_FAILURE);
    sql_print_error("%s", ER(ER_BINLOG_READ_EVENT_CHECKSUM_FAILURE));
    DBUG_RETURN(NULL);
#endif
  }

  if (event_type > description_event->number_of_event_types &&
      event_type != FORMAT_DESCRIPTION_EVENT)
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
      int new_event_type= description_event->event_type_permutation[event_type];
      DBUG_PRINT("info", ("converting event type %d to %d (%s)",
                   event_type, new_event_type,
                   get_type_str((Log_event_type)new_event_type)));
      event_type= new_event_type;
    }

    if (alg != BINLOG_CHECKSUM_ALG_UNDEF &&
        (event_type == FORMAT_DESCRIPTION_EVENT ||
         alg != BINLOG_CHECKSUM_ALG_OFF))
      event_len= event_len - BINLOG_CHECKSUM_LEN;
    
    switch(event_type) {
    case QUERY_EVENT:
      ev  = new Query_log_event(buf, event_len, description_event, QUERY_EVENT);
      break;
    case LOAD_EVENT:
      ev = new Load_log_event(buf, event_len, description_event);
      break;
    case NEW_LOAD_EVENT:
      ev = new Load_log_event(buf, event_len, description_event);
      break;
    case ROTATE_EVENT:
      ev = new Rotate_log_event(buf, event_len, description_event);
      break;
#ifdef HAVE_REPLICATION
    case SLAVE_EVENT: /* can never happen (unused event) */
      ev = new Slave_log_event(buf, event_len, description_event);
      break;
#endif /* HAVE_REPLICATION */
    case CREATE_FILE_EVENT:
      ev = new Create_file_log_event(buf, event_len, description_event);
      break;
    case APPEND_BLOCK_EVENT:
      ev = new Append_block_log_event(buf, event_len, description_event);
      break;
    case DELETE_FILE_EVENT:
      ev = new Delete_file_log_event(buf, event_len, description_event);
      break;
    case EXEC_LOAD_EVENT:
      ev = new Execute_load_log_event(buf, event_len, description_event);
      break;
    case START_EVENT_V3: /* this is sent only by MySQL <=4.x */
      ev = new Start_log_event_v3(buf, event_len, description_event);
      break;
    case STOP_EVENT:
      ev = new Stop_log_event(buf, description_event);
      break;
    case INTVAR_EVENT:
      ev = new Intvar_log_event(buf, description_event);
      break;
    case XID_EVENT:
      ev = new Xid_log_event(buf, description_event);
      break;
    case RAND_EVENT:
      ev = new Rand_log_event(buf, description_event);
      break;
    case USER_VAR_EVENT:
      ev = new User_var_log_event(buf, event_len, description_event);
      break;
    case FORMAT_DESCRIPTION_EVENT:
      ev = new Format_description_log_event(buf, event_len, description_event);
      break;
#if defined(HAVE_REPLICATION) 
    case PRE_GA_WRITE_ROWS_EVENT:
      ev = new Write_rows_log_event_old(buf, event_len, description_event);
      break;
    case PRE_GA_UPDATE_ROWS_EVENT:
      ev = new Update_rows_log_event_old(buf, event_len, description_event);
      break;
    case PRE_GA_DELETE_ROWS_EVENT:
      ev = new Delete_rows_log_event_old(buf, event_len, description_event);
      break;
    case WRITE_ROWS_EVENT:
      ev = new Write_rows_log_event(buf, event_len, description_event);
      break;
    case UPDATE_ROWS_EVENT:
      ev = new Update_rows_log_event(buf, event_len, description_event);
      break;
    case DELETE_ROWS_EVENT:
      ev = new Delete_rows_log_event(buf, event_len, description_event);
      break;
    case TABLE_MAP_EVENT:
      ev = new Table_map_log_event(buf, event_len, description_event);
      break;
#endif
    case BEGIN_LOAD_QUERY_EVENT:
      ev = new Begin_load_query_log_event(buf, event_len, description_event);
      break;
    case EXECUTE_LOAD_QUERY_EVENT:
      ev= new Execute_load_query_log_event(buf, event_len, description_event);
      break;
    case INCIDENT_EVENT:
      ev = new Incident_log_event(buf, event_len, description_event);
      break;
    case ANNOTATE_ROWS_EVENT:
      ev = new Annotate_rows_log_event(buf, event_len, description_event);
      break;
    default:
      DBUG_PRINT("error",("Unknown event code: %d",
                          (int) buf[EVENT_TYPE_OFFSET]));
      ev= NULL;
      break;
    }
  }

  if (ev)
  {
    ev->checksum_alg= alg;
    if (ev->checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
        ev->checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
      ev->crc= uint4korr(buf + (event_len));
  }

  DBUG_PRINT("read_event", ("%s(type_code: %d; event_len: %d)",
                            ev ? ev->get_type_str() : "<unknown>",
                            buf[EVENT_TYPE_OFFSET],
                            event_len));
  /*
    is_valid() are small event-specific sanity tests which are
    important; for example there are some my_malloc() in constructors
    (e.g. Query_log_event::Query_log_event(char*...)); when these
    my_malloc() fail we can't return an error out of the constructor
    (because constructor is "void") ; so instead we leave the pointer we
    wanted to allocate (e.g. 'query') to 0 and we test it in is_valid().
    Same for Format_description_log_event, member 'post_header_len'.

    SLAVE_EVENT is never used, so it should not be read ever.
  */
  if (!ev || !ev->is_valid() || (event_type == SLAVE_EVENT))
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
                             bool is_more __attribute__((unused)))
{
  char llbuff[22];
  my_off_t hexdump_from= print_event_info->hexdump_from;
  DBUG_ENTER("Log_event::print_header");

  my_b_printf(file, "#");
  print_timestamp(file);
  my_b_printf(file, " server id %lu  end_log_pos %s ", (ulong) server_id,
              llstr(log_pos,llbuff));

  /* print the checksum */

  if (checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
      checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
  {
    char checksum_buf[BINLOG_CHECKSUM_LEN * 2 + 4]; // to fit to "0x%lx "
    size_t const bytes_written=
      my_snprintf(checksum_buf, sizeof(checksum_buf), "0x%08lx ", (ulong) crc);
    my_b_printf(file, "%s ", get_type(&binlog_checksum_typelib, checksum_alg));
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
    char *h, hex_string[LOG_EVENT_MINIMAL_HEADER_LEN*4]= {0};
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
      my_snprintf(h, 4, "%02x ", *ptr);
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
      else if (i % 8 == 7) *h++ = ' ';
    }
    *c= '\0';

    if (hex_string[0])
    {
      char emit_buf[256];
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
  Single-quote and backslash characters are escaped with a \
  
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
    if (*s > 0x1F)
      my_b_write(file, s, 1);
    else if (*s == '\'')
      my_b_write(file, "\\'", 2);
    else if (*s == '\\')
      my_b_write(file, "\\\\", 2);
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
      int32 si= sint4korr(ptr);
      uint32 ui= uint4korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      my_snprintf(typestr, typestr_length, "INT");
      return 4;
    }

  case MYSQL_TYPE_TINY:
    {
      my_b_write_sint32_and_uint32(file, (int) (signed char) *ptr,
                                  (uint) (unsigned char) *ptr);
      my_snprintf(typestr, typestr_length, "TINYINT");
      return 1;
    }

  case MYSQL_TYPE_SHORT:
    {
      int32 si= (int32) sint2korr(ptr);
      uint32 ui= (uint32) uint2korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      my_snprintf(typestr, typestr_length, "SHORTINT");
      return 2;
    }
  
  case MYSQL_TYPE_INT24:
    {
      int32 si= sint3korr(ptr);
      uint32 ui= uint3korr(ptr);
      my_b_write_sint32_and_uint32(file, si, ui);
      my_snprintf(typestr, typestr_length, "MEDIUMINT");
      return 3;
    }

  case MYSQL_TYPE_LONGLONG:
    {
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
      my_snprintf(typestr, typestr_length, "LONGINT");
      return 8;
    }

  case MYSQL_TYPE_NEWDECIMAL:
    {
      uint precision= meta >> 8;
      uint decimals= meta & 0xFF;
      uint bin_size= my_decimal_get_binary_size(precision, decimals);
      my_decimal dec;
      binary2my_decimal(E_DEC_FATAL_ERROR, (uchar*) ptr, &dec,
                        precision, decimals);
      int length= DECIMAL_MAX_STR_LENGTH;
      char buff[DECIMAL_MAX_STR_LENGTH + 1];
      decimal2string(&dec, buff, &length, 0, 0, 0);
      my_b_write(file, (uchar*)buff, length);
      my_snprintf(typestr, typestr_length, "DECIMAL(%d,%d)",
                  precision, decimals);
      return bin_size;
    }

  case MYSQL_TYPE_FLOAT:
    {
      float fl;
      float4get(fl, ptr);
      char tmp[320];
      sprintf(tmp, "%-20g", (double) fl);
      my_b_printf(file, "%s", tmp); /* my_snprintf doesn't support %-20g */
      my_snprintf(typestr, typestr_length, "FLOAT");
      return 4;
    }

  case MYSQL_TYPE_DOUBLE:
    {
      double dbl;
      float8get(dbl, ptr);
      char tmp[320];
      sprintf(tmp, "%-.20g", dbl); /* my_snprintf doesn't support %-20g */
      my_b_printf(file, "%s", tmp);
      strcpy(typestr, "DOUBLE");
      return 8;
    }
  
  case MYSQL_TYPE_BIT:
    {
      /* Meta-data: bit_len, bytes_in_rec, 2 bytes */
      uint nbits= ((meta >> 8) * 8) + (meta & 0xFF);
      length= (nbits + 7) / 8;
      my_b_write_bit(file, ptr, nbits);
      my_snprintf(typestr, typestr_length, "BIT(%d)", nbits);
      return length;
    }

  case MYSQL_TYPE_TIMESTAMP:
    {
      uint32 i32= uint4korr(ptr);
      my_b_printf(file, "%d", i32);
      my_snprintf(typestr, typestr_length, "TIMESTAMP");
      return 4;
    }

  case MYSQL_TYPE_DATETIME:
    {
      ulong d, t;
      uint64 i64= uint8korr(ptr); /* YYYYMMDDhhmmss */
      d= (ulong) (i64 / 1000000);
      t= (ulong) (i64 % 1000000);

      my_b_printf(file, "%04d-%02d-%02d %02d:%02d:%02d",
                  (int) (d / 10000), (int) (d % 10000) / 100, (int) (d % 100),
                  (int) (t / 10000), (int) (t % 10000) / 100, (int) t % 100);
      my_snprintf(typestr, typestr_length, "DATETIME");
      return 8;
    }

  case MYSQL_TYPE_TIME:
    {
      uint32 i32= uint3korr(ptr);
      my_b_printf(file, "'%02d:%02d:%02d'",
                  i32 / 10000, (i32 % 10000) / 100, i32 % 100);
      my_snprintf(typestr,  typestr_length, "TIME");
      return 3;
    }
    
  case MYSQL_TYPE_NEWDATE:
    {
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
      my_snprintf(typestr, typestr_length, "DATE");
      return 3;
    }
    
  case MYSQL_TYPE_DATE:
    {
      uint i32= uint3korr(ptr);
      my_b_printf(file , "'%04d:%02d:%02d'",
                  (i32 / (16L * 32L)), (i32 / 32L % 16L), (i32 % 32L));
      my_snprintf(typestr, typestr_length, "DATE");
      return 3;
    }
  
  case MYSQL_TYPE_YEAR:
    {
      uint32 i32= *ptr;
      my_b_printf(file, "%04d", i32+ 1900);
      my_snprintf(typestr, typestr_length, "YEAR");
      return 1;
    }
  
  case MYSQL_TYPE_ENUM:
    switch (meta & 0xFF) {
    case 1:
      my_b_printf(file, "%d", (int) *ptr);
      my_snprintf(typestr, typestr_length, "ENUM(1 byte)");
      return 1;
    case 2:
      {
        int32 i32= uint2korr(ptr);
        my_b_printf(file, "%d", i32);
        my_snprintf(typestr, typestr_length, "ENUM(2 bytes)");
        return 2;
      }
    default:
      my_b_printf(file, "!! Unknown ENUM packlen=%d", meta & 0xFF); 
      return 0;
    }
    break;
    
  case MYSQL_TYPE_SET:
    my_b_write_bit(file, ptr , (meta & 0xFF) * 8);
    my_snprintf(typestr, typestr_length, "SET(%d bytes)", meta & 0xFF);
    return meta & 0xFF;
  
  case MYSQL_TYPE_BLOB:
    switch (meta) {
    case 1:
      length= *ptr;
      my_b_write_quoted(file, ptr + 1, length);
      my_snprintf(typestr, typestr_length, "TINYBLOB/TINYTEXT");
      return length + 1;
    case 2:
      length= uint2korr(ptr);
      my_b_write_quoted(file, ptr + 2, length);
      my_snprintf(typestr, typestr_length, "BLOB/TEXT");
      return length + 2;
    case 3:
      length= uint3korr(ptr);
      my_b_write_quoted(file, ptr + 3, length);
      my_snprintf(typestr, typestr_length, "MEDIUMBLOB/MEDIUMTEXT");
      return length + 3;
    case 4:
      length= uint4korr(ptr);
      my_b_write_quoted(file, ptr + 4, length);
      my_snprintf(typestr, typestr_length, "LONGBLOB/LONGTEXT");
      return length + 4;
    default:
      my_b_printf(file, "!! Unknown BLOB packlen=%d", length);
      return 0;
    }

  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_VAR_STRING:
    length= meta;
    my_snprintf(typestr, typestr_length, "VARSTRING(%d)", length);
    return my_b_write_quoted_with_length(file, ptr, length);

  case MYSQL_TYPE_STRING:
    my_snprintf(typestr, typestr_length, "STRING(%d)", length);
    return my_b_write_quoted_with_length(file, ptr, length);

  case MYSQL_TYPE_DECIMAL:
    my_b_printf(file,
                "!! Old DECIMAL (mysql-4.1 or earlier). "
                "Not enough metadata to display the value. ");
    break;

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
  
  value+= (m_width + 7) / 8;
  
  my_b_printf(file, "%s", prefix);
  
  for (size_t i= 0; i < td->size(); i ++)
  {
    int is_null= (null_bits[null_bit_index / 8] 
                  >> (null_bit_index % 8))  & 0x01;

    if (bitmap_is_set(cols_bitmap, i) == 0)
      continue;
    
    if (is_null)
    {
      my_b_printf(file, "###   @%d=NULL", i + 1);
    }
    else
    {
      my_b_printf(file, "###   @%d=", i + 1);
      size_t fsize= td->calc_field_size((uint)i, (uchar*) value);
      if (value + fsize > m_rows_end)
      {
        my_b_printf(file, "***Corrupted replication event was detected."
                    " Not printing the value***\n");
        value+= fsize;
        return 0;
      }
      size_t size= log_event_print_value(file, value,
                                         td->type(i), td->field_metadata(i),
                                         typestr, sizeof(typestr));
      if (!size)
        return 0;

      value+= size;
    }

    if (print_event_info->verbose > 1)
    {
      my_b_printf(file, " /* ");

      if (typestr[0])
        my_b_printf(file, "%s ", typestr);
      else
        my_b_printf(file, "type=%d ", td->type(i));
      
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
  Table_map_log_event *map;
  table_def *td;
  const char *sql_command, *sql_clause1, *sql_clause2;
  Log_event_type type_code= get_type_code();
  
  switch (type_code) {
  case WRITE_ROWS_EVENT:
    sql_command= "INSERT INTO";
    sql_clause1= "### SET\n";
    sql_clause2= NULL;
    break;
  case DELETE_ROWS_EVENT:
    sql_command= "DELETE FROM";
    sql_clause1= "### WHERE\n";
    sql_clause2= NULL;
    break;
  case UPDATE_ROWS_EVENT:
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
    my_b_printf(file, "### Row event for unknown table #%d", m_table_id);
    return;
  }

  for (const uchar *value= m_rows_buf; value < m_rows_end; )
  {
    size_t length;
    my_b_printf(file, "### %s %`s.%`s\n",
                      sql_command,
                      map->get_db_name(), map->get_table_name());
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

void free_table_map_log_event(Table_map_log_event *event)
{
  delete event;
}

void Log_event::print_base64(IO_CACHE* file,
                             PRINT_EVENT_INFO* print_event_info,
                             bool more)
{
  const uchar *ptr= (const uchar *)temp_buf;
  uint32 size= uint4korr(ptr + EVENT_LEN_OFFSET);
  DBUG_ENTER("Log_event::print_base64");

  size_t const tmp_str_sz= base64_needed_encoded_length((int) size);
  char *const tmp_str= (char *) my_malloc(tmp_str_sz, MYF(MY_WME));
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
    if (checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF &&
        checksum_alg != BINLOG_CHECKSUM_ALG_OFF)
      size-= BINLOG_CHECKSUM_LEN; // checksum is displayed through the header
    
    if (ptr[4] == TABLE_MAP_EVENT)
    {
      Table_map_log_event *map; 
      map= new Table_map_log_event((const char*) ptr, size, 
                                   glob_description_event);
      print_event_info->m_table_map.set_table(map->get_table_id(), map);
    }
    else if (ptr[4] == WRITE_ROWS_EVENT)
    {
      ev= new Write_rows_log_event((const char*) ptr, size,
                                   glob_description_event);
    }
    else if (ptr[4] == DELETE_ROWS_EVENT)
    {
      ev= new Delete_rows_log_event((const char*) ptr, size,
                                    glob_description_event);
    }
    else if (ptr[4] == UPDATE_ROWS_EVENT)
    {
      ev= new Update_rows_log_event((const char*) ptr, size,
                                    glob_description_event);
    }
    
    if (ev)
    {
      ev->print_verbose(file, print_event_info);
      delete ev;
    }
  }
    
  my_free(tmp_str);
  DBUG_VOID_RETURN;
}


/*
  Log_event::print_timestamp()
*/

void Log_event::print_timestamp(IO_CACHE* file, time_t* ts)
{
  struct tm *res;
  time_t my_when= when;
  DBUG_ENTER("Log_event::print_timestamp");
  if (!ts)
    ts = &my_when;
  res=localtime(ts);

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

void Query_log_event::pack_info(THD *thd, Protocol *protocol)
{
  // TODO: show the catalog ??
  char buf_mem[1024];
  String buf(buf_mem, sizeof(buf_mem), system_charset_info);
  buf.real_alloc(9 + db_len + q_len);
  if (!(flags & LOG_EVENT_SUPPRESS_USE_F)
      && db && db_len)
  {
    buf.append(STRING_WITH_LEN("use "));
    append_identifier(thd, &buf, db, db_len);
    buf.append(STRING_WITH_LEN("; "));
  }
  if (query && q_len)
    buf.append(query, q_len);
  protocol->store(&buf);
}
#endif

#ifndef MYSQL_CLIENT

/**
  Utility function for the next method (Query_log_event::write()) .
*/
static void write_str_with_code_and_len(uchar **dst, const char *src,
                                        uint len, uint code)
{
  /*
    only 1 byte to store the length of catalog, so it should not
    surpass 255
  */
  DBUG_ASSERT(len <= 255);
  DBUG_ASSERT(src);
  *((*dst)++)= (uchar) code;
  *((*dst)++)= (uchar) len;
  bmove(*dst, src, len);
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
  uchar buf[QUERY_HEADER_LEN + MAX_SIZE_LOG_EVENT_STATUS];
  uchar *start, *start_of_status;
  ulong event_length;

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
    the same name were created simultaneously on the master (in the master
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
  start_of_status= start= buf+QUERY_HEADER_LEN;
  if (flags2_inited)
  {
    *start++= Q_FLAGS2_CODE;
    int4store(start, flags2);
    start+= 4;
  }
  if (sql_mode_inited)
  {
    *start++= Q_SQL_MODE_CODE;
    int8store(start, (ulonglong)sql_mode);
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
    int2store(start, auto_increment_increment);
    int2store(start+2, auto_increment_offset);
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
    DBUG_ASSERT(lc_time_names_number <= 0xFFFF);
    *start++= Q_LC_TIME_NAMES_CODE;
    int2store(start, lc_time_names_number);
    start+= 2;
  }
  if (charset_database_number)
  {
    DBUG_ASSERT(charset_database_number <= 0xFFFF);
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
    int4store(start, master_data_written);
    start+= 4;
  }

  if (thd && thd->need_binlog_invoker())
  {
    LEX_STRING user;
    LEX_STRING host;
    memset(&user, 0, sizeof(user));
    memset(&host, 0, sizeof(host));

    if (thd->slave_thread && thd->has_invoker())
    {
      /* user will be null, if master is older than this patch */
      user= thd->get_invoker_user();
      host= thd->get_invoker_host();
    }
    else if (thd->security_ctx->priv_user)
    {
      Security_context *ctx= thd->security_ctx;

      user.length= strlen(ctx->priv_user);
      user.str= ctx->priv_user;
      if (ctx->priv_host[0] != '\0')
      {
        host.str= ctx->priv_host;
        host.length= strlen(ctx->priv_host);
      }
    }

    if (user.length > 0)
    {
      *start++= Q_INVOKER;

      /*
        Store user length and user. The max length of use is 16, so 1 byte is
        enough to store the user's length.
       */
      *start++= (uchar)user.length;
      memcpy(start, user.str, user.length);
      start+= user.length;

      /*
        Store host length and host. The max length of host is 60, so 1 byte is
        enough to store the host's length.
       */
      *start++= (uchar)host.length;
      memcpy(start, host.str, host.length);
      start+= host.length;
    }
  }

  if (thd && thd->query_start_sec_part_used)
  {
    *start++= Q_HRNOW;
    get_time();
    int3store(start, when_sec_part);
    start+= 3;
  }
  /*
    NOTE: When adding new status vars, please don't forget to update
    the MAX_SIZE_LOG_EVENT_STATUS in log_event.h and update the function
    code_name() in this file.
   
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
          wrapper_my_b_safe_write(file, (uchar*) buf, QUERY_HEADER_LEN) ||
          write_post_header_for_derived(file) ||
          wrapper_my_b_safe_write(file, (uchar*) start_of_status,
                          (uint) (start-start_of_status)) ||
          wrapper_my_b_safe_write(file, (db) ? (uchar*) db : (uchar*)"", db_len + 1) ||
          wrapper_my_b_safe_write(file, (uchar*) query, q_len) ||
	  write_footer(file)) ? 1 : 0;
}

/**
  The simplest constructor that could possibly work.  This is used for
  creating static objects that have a special meaning and are invisible
  to the log.  
*/
Query_log_event::Query_log_event()
  :Log_event(), data_buf(0)
{
  memset(&user, 0, sizeof(user));
  memset(&host, 0, sizeof(host));
}


/*
  SYNOPSIS
    Query_log_event::Query_log_event()
      thd_arg           - thread handle
      query_arg         - array of char representing the query
      query_length      - size of the  `query_arg' array
      using_trans       - there is a modified transactional table
      suppress_use      - suppress the generation of 'USE' statements
      errcode           - the error code of the query
      
  DESCRIPTION
  Creates an event for binlogging
  The value for `errcode' should be supplied by caller.
*/
Query_log_event::Query_log_event(THD* thd_arg, const char* query_arg,
				 ulong query_length, bool using_trans,
				 bool direct, bool suppress_use, int errcode)

  :Log_event(thd_arg,
             (thd_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F :
              0) |
             (suppress_use ? LOG_EVENT_SUPPRESS_USE_F : 0),
	     using_trans),
   data_buf(0), query(query_arg), catalog(thd_arg->catalog),
   db(thd_arg->db), q_len((uint32) query_length),
   thread_id(thd_arg->thread_id),
   /* save the original thread id; we already know the server id */
   slave_proxy_id(thd_arg->variables.pseudo_thread_id),
   flags2_inited(1), sql_mode_inited(1), charset_inited(1),
   sql_mode(thd_arg->variables.sql_mode),
   auto_increment_increment(thd_arg->variables.auto_increment_increment),
   auto_increment_offset(thd_arg->variables.auto_increment_offset),
   lc_time_names_number(thd_arg->variables.lc_time_names->number),
   charset_database_number(0),
   table_map_for_update((ulonglong)thd_arg->table_map_for_update),
   master_data_written(0)
{
  time_t end_time;

  memset(&user, 0, sizeof(user));
  memset(&host, 0, sizeof(host));

  error_code= errcode;

  end_time= my_time(0);
  exec_time = (ulong) (end_time  - thd_arg->start_time);
  /**
    @todo this means that if we have no catalog, then it is replicated
    as an existing catalog of length zero. is that safe? /sven
  */
  catalog_len = (catalog) ? (uint32) strlen(catalog) : 0;
  /* status_vars_len is set just before writing the event */
  db_len = (db) ? (uint32) strlen(db) : 0;
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

  LEX *lex= thd->lex;
  /*
    Defines that the statement will be written directly to the binary log
    without being wrapped by a BEGIN...COMMIT. Otherwise, the statement
    will be written to either the trx-cache or stmt-cache.

    Note that a cache will not be used if the parameter direct is TRUE.
  */
  bool use_cache= FALSE;
  /*
    TRUE defines that the trx-cache must be used and by consequence the
    use_cache is TRUE.

    Note that a cache will not be used if the parameter direct is TRUE.
  */
  bool trx_cache= FALSE;
  cache_type= Log_event::EVENT_INVALID_CACHE;

  switch (lex->sql_command)
  {
    case SQLCOM_DROP_TABLE:
      use_cache= (lex->drop_temporary && thd->in_multi_stmt_transaction_mode());
    break;

    case SQLCOM_CREATE_TABLE:
      trx_cache= (lex->select_lex.item_list.elements &&
                  thd->is_current_stmt_binlog_format_row());
      use_cache= ((lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
                   thd->in_multi_stmt_transaction_mode()) || trx_cache;
      break;
    case SQLCOM_SET_OPTION:
      use_cache= trx_cache= (lex->autocommit ? FALSE : TRUE);
      break;
    case SQLCOM_RELEASE_SAVEPOINT:
    case SQLCOM_ROLLBACK_TO_SAVEPOINT:
    case SQLCOM_SAVEPOINT:
      use_cache= trx_cache= TRUE;
      break;
    default:
      use_cache= sqlcom_can_generate_row_events(thd);
      break;
  }

  if (!use_cache || direct)
  {
    cache_type= Log_event::EVENT_NO_CACHE;
  }
  else if (using_trans || trx_cache || stmt_has_updated_trans_table(thd) ||
           thd->lex->is_mixed_stmt_unsafe(thd->in_multi_stmt_transaction_mode(),
                                          thd->variables.binlog_direct_non_trans_update,
                                          trans_has_updated_trans_table(thd),
                                          thd->tx_isolation))
    cache_type= Log_event::EVENT_TRANSACTIONAL_CACHE;
  else
    cache_type= Log_event::EVENT_STMT_CACHE;
  DBUG_ASSERT(cache_type != Log_event::EVENT_INVALID_CACHE);
  DBUG_PRINT("info",("Query_log_event has flags2: %lu  sql_mode: %llu",
                     (ulong) flags2, sql_mode));
}
#endif /* MYSQL_CLIENT */


/* 2 utility functions for the next method */

/**
   Read a string with length from memory.

   This function reads the string-with-length stored at
   <code>src</code> and extract the length into <code>*len</code> and
   a pointer to the start of the string into <code>*dst</code>. The
   string can then be copied using <code>memcpy()</code> with the
   number of bytes given in <code>*len</code>.

   @param src Pointer to variable holding a pointer to the memory to
              read the string from.
   @param dst Pointer to variable holding a pointer where the actual
              string starts. Starting from this position, the string
              can be copied using @c memcpy().
   @param len Pointer to variable where the length will be stored.
   @param end One-past-the-end of the memory where the string is
              stored.

   @return    Zero if the entire string can be copied successfully,
              @c UINT_MAX if the length could not be read from memory
              (that is, if <code>*src >= end</code>), otherwise the
              number of bytes that are missing to read the full
              string, which happends <code>*dst + *len >= end</code>.
*/
static int
get_str_len_and_pointer(const Log_event::Byte **src,
                        const char **dst,
                        uint *len,
                        const Log_event::Byte *end)
{
  if (*src >= end)
    return -1;       // Will be UINT_MAX in two-complement arithmetics
  uint length= **src;
  if (length > 0)
  {
    if (*src + length >= end)
      return *src + length - end + 1;       // Number of bytes missing
    *dst= (char *)*src + 1;                    // Will be copied later
  }
  *len= length;
  *src+= length + 1;
  return 0;
}

static void copy_str_and_move(const char **src, 
                              Log_event::Byte **dst, 
                              uint len)
{
  memcpy(*dst, *src, len);
  *src= (const char *)*dst;
  (*dst)+= len;
  *(*dst)++= 0;
}


#ifndef DBUG_OFF
static char const *
code_name(int code)
{
  static char buf[255];
  switch (code) {
  case Q_FLAGS2_CODE: return "Q_FLAGS2_CODE";
  case Q_SQL_MODE_CODE: return "Q_SQL_MODE_CODE";
  case Q_CATALOG_CODE: return "Q_CATALOG_CODE";
  case Q_AUTO_INCREMENT: return "Q_AUTO_INCREMENT";
  case Q_CHARSET_CODE: return "Q_CHARSET_CODE";
  case Q_TIME_ZONE_CODE: return "Q_TIME_ZONE_CODE";
  case Q_CATALOG_NZ_CODE: return "Q_CATALOG_NZ_CODE";
  case Q_LC_TIME_NAMES_CODE: return "Q_LC_TIME_NAMES_CODE";
  case Q_CHARSET_DATABASE_CODE: return "Q_CHARSET_DATABASE_CODE";
  case Q_TABLE_MAP_FOR_UPDATE_CODE: return "Q_TABLE_MAP_FOR_UPDATE_CODE";
  case Q_MASTER_DATA_WRITTEN_CODE: return "Q_MASTER_DATA_WRITTEN_CODE";
  case Q_HRNOW: return "Q_HRNOW";
  }
  sprintf(buf, "CODE#%d", code);
  return buf;
}
#endif

/**
   Macro to check that there is enough space to read from memory.

   @param PTR Pointer to memory
   @param END End of memory
   @param CNT Number of bytes that should be read.
 */
#define CHECK_SPACE(PTR,END,CNT)                      \
  do {                                                \
    DBUG_PRINT("info", ("Read %s", code_name(pos[-1]))); \
    DBUG_ASSERT((PTR) + (CNT) <= (END));              \
    if ((PTR) + (CNT) > (END)) {                      \
      DBUG_PRINT("info", ("query= 0"));               \
      query= 0;                                       \
      DBUG_VOID_RETURN;                               \
    }                                                 \
  } while (0)


/**
  This is used by the SQL slave thread to prepare the event before execution.
*/
Query_log_event::Query_log_event(const char* buf, uint event_len,
                                 const Format_description_log_event
                                 *description_event,
                                 Log_event_type event_type)
  :Log_event(buf, description_event), data_buf(0), query(NullS),
   db(NullS), catalog_len(0), status_vars_len(0),
   flags2_inited(0), sql_mode_inited(0), charset_inited(0),
   auto_increment_increment(1), auto_increment_offset(1),
   time_zone_len(0), lc_time_names_number(0), charset_database_number(0),
   table_map_for_update(0), master_data_written(0)
{
  ulong data_len;
  uint32 tmp;
  uint8 common_header_len, post_header_len;
  Log_event::Byte *start;
  const Log_event::Byte *end;
  bool catalog_nz= 1;
  DBUG_ENTER("Query_log_event::Query_log_event(char*,...)");

  memset(&user, 0, sizeof(user));
  memset(&host, 0, sizeof(host));
  common_header_len= description_event->common_header_len;
  post_header_len= description_event->post_header_len[event_type-1];
  DBUG_PRINT("info",("event_len: %u  common_header_len: %d  post_header_len: %d",
                     event_len, common_header_len, post_header_len));
  
  /*
    We test if the event's length is sensible, and if so we compute data_len.
    We cannot rely on QUERY_HEADER_LEN here as it would not be format-tolerant.
    We use QUERY_HEADER_MINIMAL_LEN which is the same for 3.23, 4.0 & 5.0.
  */
  if (event_len < (uint)(common_header_len + post_header_len))
    DBUG_VOID_RETURN;				
  data_len = event_len - (common_header_len + post_header_len);
  buf+= common_header_len;
  
  slave_proxy_id= thread_id = uint4korr(buf + Q_THREAD_ID_OFFSET);
  exec_time = uint4korr(buf + Q_EXEC_TIME_OFFSET);
  db_len = (uchar)buf[Q_DB_LEN_OFFSET]; // TODO: add a check of all *_len vars
  error_code = uint2korr(buf + Q_ERR_CODE_OFFSET);

  /*
    5.0 format starts here.
    Depending on the format, we may or not have affected/warnings etc
    The remnent post-header to be parsed has length:
  */
  tmp= post_header_len - QUERY_HEADER_MINIMAL_LEN; 
  if (tmp)
  {
    status_vars_len= uint2korr(buf + Q_STATUS_VARS_LEN_OFFSET);
    /*
      Check if status variable length is corrupt and will lead to very
      wrong data. We could be even more strict and require data_len to
      be even bigger, but this will suffice to catch most corruption
      errors that can lead to a crash.
    */
    if (status_vars_len > min(data_len, MAX_SIZE_LOG_EVENT_STATUS))
    {
      DBUG_PRINT("info", ("status_vars_len (%u) > data_len (%lu); query= 0",
                          status_vars_len, data_len));
      query= 0;
      DBUG_VOID_RETURN;
    }
    data_len-= status_vars_len;
    DBUG_PRINT("info", ("Query_log_event has status_vars_len: %u",
                        (uint) status_vars_len));
    tmp-= 2;
  } 
  else
  {
    /*
      server version < 5.0 / binlog_version < 4 master's event is 
      relay-logged with storing the original size of the event in
      Q_MASTER_DATA_WRITTEN_CODE status variable.
      The size is to be restored at reading Q_MASTER_DATA_WRITTEN_CODE-marked
      event from the relay log.
    */
    DBUG_ASSERT(description_event->binlog_version < 4);
    master_data_written= data_written;
  }
  /*
    We have parsed everything we know in the post header for QUERY_EVENT,
    the rest of post header is either comes from older version MySQL or
    dedicated to derived events (e.g. Execute_load_query...)
  */

  /* variable-part: the status vars; only in MySQL 5.0  */
  
  start= (Log_event::Byte*) (buf+post_header_len);
  end= (const Log_event::Byte*) (start+status_vars_len);
  for (const Log_event::Byte* pos= start; pos < end;)
  {
    switch (*pos++) {
    case Q_FLAGS2_CODE:
      CHECK_SPACE(pos, end, 4);
      flags2_inited= 1;
      flags2= uint4korr(pos);
      DBUG_PRINT("info",("In Query_log_event, read flags2: %lu", (ulong) flags2));
      pos+= 4;
      break;
    case Q_SQL_MODE_CODE:
    {
#ifndef DBUG_OFF
      char buff[22];
#endif
      CHECK_SPACE(pos, end, 8);
      sql_mode_inited= 1;
      sql_mode= (ulong) uint8korr(pos); // QQ: Fix when sql_mode is ulonglong
      DBUG_PRINT("info",("In Query_log_event, read sql_mode: %s",
			 llstr(sql_mode, buff)));
      pos+= 8;
      break;
    }
    case Q_CATALOG_NZ_CODE:
      DBUG_PRINT("info", ("case Q_CATALOG_NZ_CODE; pos: 0x%lx; end: 0x%lx",
                          (ulong) pos, (ulong) end));
      if (get_str_len_and_pointer(&pos, &catalog, &catalog_len, end))
      {
        DBUG_PRINT("info", ("query= 0"));
        query= 0;
        DBUG_VOID_RETURN;
      }
      break;
    case Q_AUTO_INCREMENT:
      CHECK_SPACE(pos, end, 4);
      auto_increment_increment= uint2korr(pos);
      auto_increment_offset=    uint2korr(pos+2);
      pos+= 4;
      break;
    case Q_CHARSET_CODE:
    {
      CHECK_SPACE(pos, end, 6);
      charset_inited= 1;
      memcpy(charset, pos, 6);
      pos+= 6;
      break;
    }
    case Q_TIME_ZONE_CODE:
    {
      if (get_str_len_and_pointer(&pos, &time_zone_str, &time_zone_len, end))
      {
        DBUG_PRINT("info", ("Q_TIME_ZONE_CODE: query= 0"));
        query= 0;
        DBUG_VOID_RETURN;
      }
      break;
    }
    case Q_CATALOG_CODE: /* for 5.0.x where 0<=x<=3 masters */
      CHECK_SPACE(pos, end, 1);
      if ((catalog_len= *pos))
        catalog= (char*) pos+1;                           // Will be copied later
      CHECK_SPACE(pos, end, catalog_len + 2);
      pos+= catalog_len+2; // leap over end 0
      catalog_nz= 0; // catalog has end 0 in event
      break;
    case Q_LC_TIME_NAMES_CODE:
      CHECK_SPACE(pos, end, 2);
      lc_time_names_number= uint2korr(pos);
      pos+= 2;
      break;
    case Q_CHARSET_DATABASE_CODE:
      CHECK_SPACE(pos, end, 2);
      charset_database_number= uint2korr(pos);
      pos+= 2;
      break;
    case Q_TABLE_MAP_FOR_UPDATE_CODE:
      CHECK_SPACE(pos, end, 8);
      table_map_for_update= uint8korr(pos);
      pos+= 8;
      break;
    case Q_MASTER_DATA_WRITTEN_CODE:
      CHECK_SPACE(pos, end, 4);
      data_written= master_data_written= uint4korr(pos);
      pos+= 4;
      break;
    case Q_INVOKER:
    {
      CHECK_SPACE(pos, end, 1);
      user.length= *pos++;
      CHECK_SPACE(pos, end, user.length);
      user.str= (char *)pos;
      pos+= user.length;

      CHECK_SPACE(pos, end, 1);
      host.length= *pos++;
      CHECK_SPACE(pos, end, host.length);
      host.str= (char *)pos;
      pos+= host.length;
      break;
    }
    case Q_HRNOW:
    {
      CHECK_SPACE(pos, end, 3);
      when_sec_part= uint3korr(pos);
      pos+= 3;
      break;
    }
    default:
      /* That's why you must write status vars in growing order of code */
      DBUG_PRINT("info",("Query_log_event has unknown status vars (first has\
 code: %u), skipping the rest of them", (uint) *(pos-1)));
      pos= (const uchar*) end;                         // Break loop
    }
  }

  /**
    Layout for the data buffer is as follows
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
    */

#if !defined(MYSQL_CLIENT) && defined(HAVE_QUERY_CACHE)
  if (!(start= data_buf = (Log_event::Byte*) my_malloc(catalog_len + 1
                                                    +  time_zone_len + 1
                                                    +  user.length + 1
                                                    +  host.length + 1
                                                    +  data_len + 1
                                                    +  sizeof(size_t)//for db_len
                                                    +  db_len + 1
                                                    +  QUERY_CACHE_DB_LENGTH_SIZE
                                                    +  QUERY_CACHE_FLAGS_SIZE,
                                                       MYF(MY_WME))))
#else
  if (!(start= data_buf = (Log_event::Byte*) my_malloc(catalog_len + 1
                                                    +  time_zone_len + 1
                                                    +  user.length + 1
                                                    +  host.length + 1
                                                    +  data_len + 1,
                                                       MYF(MY_WME))))
#endif
      DBUG_VOID_RETURN;
  if (catalog_len)                                  // If catalog is given
  {
    /**
      @todo we should clean up and do only copy_str_and_move; it
      works for both cases.  Then we can remove the catalog_nz
      flag. /sven
    */
    if (likely(catalog_nz)) // true except if event comes from 5.0.0|1|2|3.
      copy_str_and_move(&catalog, &start, catalog_len);
    else
    {
      memcpy(start, catalog, catalog_len+1); // copy end 0
      catalog= (const char *)start;
      start+= catalog_len+1;
    }
  }
  if (time_zone_len)
    copy_str_and_move(&time_zone_str, &start, time_zone_len);

  if (user.length > 0)
    copy_str_and_move((const char **)&(user.str), &start, user.length);
  if (host.length > 0)
    copy_str_and_move((const char **)&(host.str), &start, host.length);

  /**
    if time_zone_len or catalog_len are 0, then time_zone and catalog
    are uninitialized at this point.  shouldn't they point to the
    zero-length null-terminated strings we allocated space for in the
    my_alloc call above? /sven
  */

  /* A 2nd variable part; this is common to all versions */ 
  memcpy((char*) start, end, data_len);          // Copy db and query
  start[data_len]= '\0';              // End query with \0 (For safetly)
  db= (char *)start;
  query= (char *)(start + db_len + 1);
  q_len= data_len - db_len -1;
  /**
    Append the db length at the end of the buffer. This will be used by
    Query_cache::send_result_to_client() in case the query cache is On.
   */
#if !defined(MYSQL_CLIENT) && defined(HAVE_QUERY_CACHE)
  size_t db_length= (size_t)db_len;
  memcpy(start + data_len + 1, &db_length, sizeof(size_t));
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
  char buff[64], *end;				// Enough for SET TIMESTAMP
  bool different_db= 1;
  uint32 tmp;

  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info, FALSE);
    my_b_printf(file, "\t%s\tthread_id=%lu\texec_time=%lu\terror_code=%d\n",
                get_type_str(), (ulong) thread_id, (ulong) exec_time,
                error_code);
  }

  if ((flags & LOG_EVENT_SUPPRESS_USE_F))
  {
    if (!is_trans_keyword())
      print_event_info->db[0]= '\0';
  }
  else if (db)
  {
    different_db= memcmp(print_event_info->db, db, db_len + 1);
    if (different_db)
      memcpy(print_event_info->db, db, db_len + 1);
    if (db[0] && different_db) 
      my_b_printf(file, "use %`s%s\n", db, print_event_info->delimiter);
  }

  end=int10_to_str((long) when, strmov(buff,"SET TIMESTAMP="),10);
  if (when_sec_part)
  {
    *end++= '.';
    end=int10_to_str(when_sec_part, end, 10);
  }
  end= strmov(end, print_event_info->delimiter);
  *end++='\n';
  my_b_write(file, (uchar*) buff, (uint) (end-buff));
  if ((!print_event_info->thread_id_printed ||
       ((flags & LOG_EVENT_THREAD_SPECIFIC_F) &&
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
    my_b_printf(file,"SET @@session.auto_increment_increment=%lu, @@session.auto_increment_offset=%lu%s\n",
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
    CHARSET_INFO *cs_info= get_charset(uint2korr(charset), MYF(MY_WME));
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
                uint2korr(charset),
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
}


void Query_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  /**
    reduce the size of io cache so that the write function is called
    for every call to my_b_write().
   */
  DBUG_EXECUTE_IF ("simulate_file_write_error",
                   {(&cache)->write_pos= (&cache)->write_end- 500;});
  print_query_header(&cache, print_event_info);
  my_b_write(&cache, (uchar*) query, q_len);
  my_b_printf(&cache, "\n%s\n", print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */


/*
  Query_log_event::do_apply_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

int Query_log_event::do_apply_event(Relay_log_info const *rli)
{
  return do_apply_event(rli, query, q_len);
}

/**
   Compare if two errors should be regarded as equal.
   This is to handle the case when you can get slightly different errors
   on master and slave for the same thing.
   @param
   expected_error	Error we got on master
   actual_error		Error we got on slave

   @return
   1 Errors are equal
   0 Errors are different
*/

bool test_if_equal_repl_errors(int expected_error, int actual_error)
{
  if (expected_error == actual_error)
    return 1;
  switch (expected_error) {
  case ER_DUP_ENTRY:
  case ER_AUTOINC_READ_FAILED:
    return (actual_error == ER_AUTOINC_READ_FAILED ||
            actual_error == HA_ERR_AUTOINC_ERANGE);
  default:
    break;
  }
  return 0;
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
                                      const char *query_arg, uint32 q_len_arg)
{
  LEX_STRING new_db;
  int expected_error,actual_error= 0;
  HA_CREATE_INFO db_options;
  DBUG_ENTER("Query_log_event::do_apply_event");

  /*
    Colleagues: please never free(thd->catalog) in MySQL. This would
    lead to bugs as here thd->catalog is a part of an alloced block,
    not an entire alloced block (see
    Query_log_event::do_apply_event()). Same for thd->db.  Thank
    you.
  */
  thd->catalog= catalog_len ? (char *) catalog : (char *)"";
  new_db.length= db_len;
  new_db.str= (char *) rpl_filter->get_rewrite_db(db, &new_db.length);
  thd->set_db(new_db.str, new_db.length);       /* allocates a copy of 'db' */

  /*
    Setting the character set and collation of the current database thd->db.
   */
  load_db_opt_by_name(thd, thd->db, &db_options);
  if (db_options.default_table_charset)
    thd->db_charset= db_options.default_table_charset;
  thd->variables.auto_increment_increment= auto_increment_increment;
  thd->variables.auto_increment_offset=    auto_increment_offset;

  /*
    InnoDB internally stores the master log position it has executed so far,
    i.e. the position just after the COMMIT event.
    When InnoDB will want to store, the positions in rli won't have
    been updated yet, so group_master_log_* will point to old BEGIN
    and event_master_log* will point to the beginning of current COMMIT.
    But log_pos of the COMMIT Query event is what we want, i.e. the pos of the
    END of the current log event (COMMIT). We save it in rli so that InnoDB can
    access it.
  */
  const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;
  DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));

  clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
  if (strcmp("COMMIT", query) == 0 && rli->tables_to_lock)
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
                  const_cast<Relay_log_info*>(rli)->group_master_log_name,
                  llstr(const_cast<Relay_log_info*>(rli)->group_master_log_pos,
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

  /*
    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::do_apply_event(), then the companion SET also have so
            we don't need to reset_one_shot_variables().
  */
  if (is_trans_keyword() || rpl_filter->db_ok(thd->db))
  {
    thd->set_time(when, when_sec_part);
    thd->set_query_and_id((char*)query_arg, q_len_arg,
                          thd->charset(), next_query_id());
    thd->variables.pseudo_thread_id= thread_id;		// for temp tables
    DBUG_PRINT("query",("%s", thd->query()));

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
        We do not replicate IGNORE_DIR_IN_CREATE. That is, if the master is a
        slave which runs with SQL_MODE=IGNORE_DIR_IN_CREATE, this should not
        force us to ignore the dir too. Imagine you are a ring of machines, and
        one has a disk problem so that you temporarily need
        IGNORE_DIR_IN_CREATE on this machine; you don't want it to propagate
        elsewhere (you don't want all slaves to start ignoring the dirs).
      */
      if (sql_mode_inited)
        thd->variables.sql_mode=
          (ulong) ((thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE) |
                   (sql_mode & ~(ulong) MODE_NO_DIR_IN_CREATE));
      if (charset_inited)
      {
        if (rli->cached_charset_compare(charset))
        {
          /* Verify that we support the charsets found in the event. */
          if (!(thd->variables.character_set_client=
                get_charset(uint2korr(charset), MYF(MY_WME))) ||
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
            Reset thd->query_string.cs to the newly set value.
            Note, there is a small flaw here. For a very short time frame
            if the new charset is different from the old charset and
            if another thread executes "SHOW PROCESSLIST" after
            the above thd->set_query_and_id() and before this thd->set_query(),
            and if the current query has some non-ASCII characters,
            the another thread may see some '?' marks in the PROCESSLIST
            result. This should be acceptable now. This is a reminder
            to fix this if any refactoring happens here sometime.
          */
          thd->set_query((char*) query_arg, q_len_arg, thd->charset());
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
      thd->set_invoker(&user, &host);
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
      if (!parser_state.init(thd, thd->query(), thd->query_length()))
      {
        mysql_parse(thd, thd->query(), thd->query_length(), &parser_state);
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
      if (mysql_test_parse_for_slave(thd, thd->query(), thd->query_length()))
        clear_all_errors(thd, const_cast<Relay_log_info*>(rli)); /* Can ignore query */
      else
      {
        rli->report(ERROR_LEVEL, expected_error, 
                          "\
Query partially completed on the master (error on master: %d) \
and was aborted. There is a chance that your master is inconsistent at this \
point. If you are sure that your master is ok, run this query manually on the \
slave and then restart the slave with SET GLOBAL SQL_SLAVE_SKIP_COUNTER=1; \
START SLAVE; . Query: '%s'", expected_error, thd->query());
        thd->is_slave_error= 1;
      }
      goto end;
    }

    /* If the query was not ignored, it is printed to the general log */
    if (!thd->is_error() || thd->stmt_da->sql_errno() != ER_SLAVE_IGNORED_TABLE)
      general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());
    else
    {
      /*
        Bug#54201: If we skip an INSERT query that uses auto_increment, then we
        should reset any @@INSERT_ID set by an Intvar_log_event associated with
        the query; otherwise the @@INSERT_ID will linger until the next INSERT
        that uses auto_increment and may affect extra triggers on the slave etc.

        We reset INSERT_ID unconditionally; it is probably cheaper than
        checking if it is necessary.
      */
      thd->auto_inc_intervals_forced.empty();
    }

compare_errors:
    /*
      In the slave thread, we may sometimes execute some DROP / * 40005
      TEMPORARY * / TABLE that come from parts of binlogs (likely if we
      use RESET SLAVE or CHANGE MASTER TO), while the temporary table
      has already been dropped. To ignore such irrelevant "table does
      not exist errors", we silently clear the error if TEMPORARY was used.
    */
    if (thd->lex->sql_command == SQLCOM_DROP_TABLE && thd->lex->drop_temporary &&
        thd->is_error() && thd->stmt_da->sql_errno() == ER_BAD_TABLE_ERROR &&
        !expected_error)
      thd->stmt_da->reset_diagnostics_area();
    /*
      If we expected a non-zero error code, and we don't get the same error
      code, and it should be ignored or is related to a concurrency issue.
    */
    actual_error= thd->is_error() ? thd->stmt_da->sql_errno() : 0;
    DBUG_PRINT("info",("expected_error: %d  sql_errno: %d",
                       expected_error, actual_error));

    if ((expected_error &&
         !test_if_equal_repl_errors(expected_error, actual_error) &&
         !concurrency_error_code(expected_error)) &&
        !ignored_error_code(actual_error) &&
        !ignored_error_code(expected_error))
    {
      rli->report(ERROR_LEVEL, 0,
                      "\
Query caused different errors on master and slave.     \
Error on master: message (format)='%s' error code=%d ; \
Error on slave: actual message='%s', error code=%d. \
Default database: '%s'. Query: '%s'",
                      ER_SAFE(expected_error),
                      expected_error,
                      actual_error ? thd->stmt_da->message() : "no error",
                      actual_error,
                      print_slave_db_safe(db), query_arg);
      thd->is_slave_error= 1;
    }
    /*
      If we get the same error code as expected and it is not a concurrency
      issue, or should be ignored.
    */
    else if ((test_if_equal_repl_errors(expected_error, actual_error) &&
              !concurrency_error_code(expected_error)) ||
             ignored_error_code(actual_error))
    {
      DBUG_PRINT("info",("error ignored"));
      clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      thd->reset_killed();
    }
    /*
      Other cases: mostly we expected no error and get one.
    */
    else if (thd->is_slave_error || thd->is_fatal_error)
    {
      rli->report(ERROR_LEVEL, actual_error,
                      "Error '%s' on query. Default database: '%s'. Query: '%s'",
                      (actual_error ? thd->stmt_da->message() :
                       "unexpected success or fatal error"),
                      print_slave_db_safe(thd->db), query_arg);
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
    DBUG_EXECUTE_IF("stop_slave_middle_group",
                    if (strcmp("COMMIT", query) != 0 &&
                        strcmp("BEGIN", query) != 0)
                    {
                      if (thd->transaction.all.modified_non_trans_table)
                        const_cast<Relay_log_info*>(rli)->abort_slave= 1;
                    };);
  }

end:
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
  thd->catalog= 0;
  thd->set_db(NULL, 0);                 /* will free the current database */
  thd->reset_query();
  DBUG_PRINT("info", ("end: query= 0"));
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
  /*
    Note that we will not increment group* positions if we are just
    after a SET ONE_SHOT, because SET ONE_SHOT should not be separated
    from its following updating query.
  */
  if (thd->one_shot_set)
  {
    rli->inc_event_relay_log_pos();
    return 0;
  }
  else
    return Log_event::do_update_pos(rli);
}


Log_event::enum_skip_reason
Query_log_event::do_shall_skip(Relay_log_info *rli)
{
  DBUG_ENTER("Query_log_event::do_shall_skip");
  DBUG_PRINT("debug", ("query: %s; q_len: %d", query, q_len));
  DBUG_ASSERT(query && q_len > 0);

  /*
    An event skipped due to @@skip_replication must not be counted towards the
    number of events to be skipped due to @@sql_slave_skip_counter.
  */
  if (flags & LOG_EVENT_SKIP_REPLICATION_F &&
      opt_replicate_events_marked_for_skip != RPL_SKIP_REPLICATE)
    DBUG_RETURN(Log_event::EVENT_SKIP_IGNORE);

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
  DBUG_RETURN(Log_event::do_shall_skip(rli));
}

#endif


/**************************************************************************
	Start_log_event_v3 methods
**************************************************************************/

#ifndef MYSQL_CLIENT
Start_log_event_v3::Start_log_event_v3()
  :Log_event(), created(0), binlog_version(BINLOG_VERSION),
   dont_set_created(0)
{
  memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
}
#endif

/*
  Start_log_event_v3::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Start_log_event_v3::pack_info(THD *thd, Protocol *protocol)
{
  char buf[12 + ST_SERVER_VER_LEN + 14 + 22], *pos;
  pos= strmov(buf, "Server ver: ");
  pos= strmov(pos, server_version);
  pos= strmov(pos, ", Binlog ver: ");
  pos= int10_to_str(binlog_version, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif


/*
  Start_log_event_v3::print()
*/

#ifdef MYSQL_CLIENT
void Start_log_event_v3::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  DBUG_ENTER("Start_log_event_v3::print");

  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (!print_event_info->short_form)
  {
    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tStart: binlog v %d, server v %s created ",
                binlog_version, server_version);
    print_timestamp(&cache);
    if (created)
      my_b_printf(&cache," at startup");
    my_b_printf(&cache, "\n");
    if (flags & LOG_EVENT_BINLOG_IN_USE_F)
      my_b_printf(&cache, "# Warning: this binlog is either in use or was not "
                  "closed properly.\n");
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
    my_b_printf(&cache,"RESET CONNECTION%s\n", print_event_info->delimiter);
#else
    my_b_printf(&cache,"ROLLBACK%s\n", print_event_info->delimiter);
#endif
  }
  if (temp_buf &&
      print_event_info->base64_output_mode != BASE64_OUTPUT_NEVER &&
      !print_event_info->short_form)
  {
    if (print_event_info->base64_output_mode != BASE64_OUTPUT_DECODE_ROWS)
      my_b_printf(&cache, "BINLOG '\n");
    print_base64(&cache, print_event_info, FALSE);
    print_event_info->printed_fd_event= TRUE;
  }
  DBUG_VOID_RETURN;
}
#endif /* MYSQL_CLIENT */

/*
  Start_log_event_v3::Start_log_event_v3()
*/

Start_log_event_v3::Start_log_event_v3(const char* buf, uint event_len,
                                       const Format_description_log_event
                                       *description_event)
  :Log_event(buf, description_event), binlog_version(BINLOG_VERSION)
{
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
  char buff[START_V3_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= get_time(); // this sets when and when_sec_part as a side effect
  int4store(buff + ST_CREATED_OFFSET,created);
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
    if (strncmp(rli->relay_log.description_event_for_exec->server_version,
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
    /* this case is impossible */
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
Format_description_log_event(uint8 binlog_ver, const char* server_ver)
  :Start_log_event_v3(), event_type_permutation(0)
{
  binlog_version= binlog_ver;
  switch (binlog_ver) {
  case 4: /* MySQL 5.0 */
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
    DBUG_EXECUTE_IF("pretend_version_50034_in_binlog",
                    strmov(server_version, "5.0.34"););
    common_header_len= LOG_EVENT_HEADER_LEN;
    number_of_event_types= LOG_EVENT_TYPES;
    /* we'll catch my_malloc() error in is_valid() */
    post_header_len=(uint8*) my_malloc(number_of_event_types*sizeof(uint8)
                                       + BINLOG_CHECKSUM_ALG_DESC_LEN,
                                       MYF(0));
    /*
      This long list of assignments is not beautiful, but I see no way to
      make it nicer, as the right members are #defines, not array members, so
      it's impossible to write a loop.
    */
    if (post_header_len)
    {
#ifndef DBUG_OFF
      // Allows us to sanity-check that all events initialized their
      // events (see the end of this 'if' block).
      memset(post_header_len, 255, number_of_event_types*sizeof(uint8));
#endif

      /* Note: all event types must explicitly fill in their lengths here. */
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_LEN;
      post_header_len[STOP_EVENT-1]= STOP_HEADER_LEN;
      post_header_len[ROTATE_EVENT-1]= ROTATE_HEADER_LEN;
      post_header_len[INTVAR_EVENT-1]= INTVAR_HEADER_LEN;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[SLAVE_EVENT-1]= SLAVE_HEADER_LEN;
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= NEW_LOAD_HEADER_LEN;
      post_header_len[RAND_EVENT-1]= RAND_HEADER_LEN;
      post_header_len[USER_VAR_EVENT-1]= USER_VAR_HEADER_LEN;
      post_header_len[FORMAT_DESCRIPTION_EVENT-1]= FORMAT_DESCRIPTION_HEADER_LEN;
      post_header_len[XID_EVENT-1]= XID_HEADER_LEN;
      post_header_len[BEGIN_LOAD_QUERY_EVENT-1]= BEGIN_LOAD_QUERY_HEADER_LEN;
      post_header_len[EXECUTE_LOAD_QUERY_EVENT-1]= EXECUTE_LOAD_QUERY_HEADER_LEN;
      /*
        The PRE_GA events are never be written to any binlog, but
        their lengths are included in Format_description_log_event.
        Hence, we need to be assign some value here, to avoid reading
        uninitialized memory when the array is written to disk.
      */
      post_header_len[PRE_GA_WRITE_ROWS_EVENT-1] = 0;
      post_header_len[PRE_GA_UPDATE_ROWS_EVENT-1] = 0;
      post_header_len[PRE_GA_DELETE_ROWS_EVENT-1] = 0;

      post_header_len[TABLE_MAP_EVENT-1]=    TABLE_MAP_HEADER_LEN;
      post_header_len[WRITE_ROWS_EVENT-1]=   ROWS_HEADER_LEN;
      post_header_len[UPDATE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      post_header_len[DELETE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      /*
        We here have the possibility to simulate a master of before we changed
        the table map id to be stored in 6 bytes: when it was stored in 4
        bytes (=> post_header_len was 6). This is used to test backward
        compatibility.
        This code can be removed after a few months (today is Dec 21st 2005),
        when we know that the 4-byte masters are not deployed anymore (check
        with Tomas Ulin first!), and the accompanying test (rpl_row_4_bytes)
        too.
      */
      DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                      post_header_len[TABLE_MAP_EVENT-1]=
                      post_header_len[WRITE_ROWS_EVENT-1]=
                      post_header_len[UPDATE_ROWS_EVENT-1]=
                      post_header_len[DELETE_ROWS_EVENT-1]= 6;);
      post_header_len[INCIDENT_EVENT-1]= INCIDENT_HEADER_LEN;
      post_header_len[HEARTBEAT_LOG_EVENT-1]= 0;

      // Set header length of the reserved events to 0
      memset(post_header_len + MYSQL_EVENTS_END - 1, 0,
             (MARIA_EVENTS_BEGIN - MYSQL_EVENTS_END)*sizeof(uint8));

      // Set header lengths of Maria events
      post_header_len[ANNOTATE_ROWS_EVENT-1]= ANNOTATE_ROWS_HEADER_LEN;

      // Sanity-check that all post header lengths are initialized.
      int i;
      for (i=0; i<number_of_event_types; i++)
        DBUG_ASSERT(post_header_len[i] != 255);
    }
    break;

  case 1: /* 3.23 */
  case 3: /* 4.0.x x>=2 */
    /*
      We build an artificial (i.e. not sent by the master) event, which
      describes what those old master versions send.
    */
    if (binlog_ver==1)
      strmov(server_version, server_ver ? server_ver : "3.23");
    else
      strmov(server_version, server_ver ? server_ver : "4.0");
    common_header_len= binlog_ver==1 ? OLD_HEADER_LEN :
      LOG_EVENT_MINIMAL_HEADER_LEN;
    /*
      The first new event in binlog version 4 is Format_desc. So any event type
      after that does not exist in older versions. We use the events known by
      version 3, even if version 1 had only a subset of them (this is not a
      problem: it uses a few bytes for nothing but unifies code; it does not
      make the slave detect less corruptions).
    */
    number_of_event_types= FORMAT_DESCRIPTION_EVENT - 1;
    post_header_len=(uint8*) my_malloc(number_of_event_types*sizeof(uint8),
                                       MYF(0));
    if (post_header_len)
    {
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_MINIMAL_LEN;
      post_header_len[STOP_EVENT-1]= 0;
      post_header_len[ROTATE_EVENT-1]= (binlog_ver==1) ? 0 : ROTATE_HEADER_LEN;
      post_header_len[INTVAR_EVENT-1]= 0;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[SLAVE_EVENT-1]= 0;
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= post_header_len[LOAD_EVENT-1];
      post_header_len[RAND_EVENT-1]= 0;
      post_header_len[USER_VAR_EVENT-1]= 0;
    }
    break;
  default: /* Includes binlog version 2 i.e. 4.0.x x<=1 */
    post_header_len= 0; /* will make is_valid() fail */
    break;
  }
  calc_server_version_split();
  checksum_alg= (uint8) BINLOG_CHECKSUM_ALG_UNDEF;
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
Format_description_log_event(const char* buf,
                             uint event_len,
                             const
                             Format_description_log_event*
                             description_event)
  :Start_log_event_v3(buf, event_len, description_event),
   common_header_len(0), post_header_len(NULL), event_type_permutation(0)
{
  DBUG_ENTER("Format_description_log_event::Format_description_log_event(char*,...)");
  if (!Start_log_event_v3::is_valid())
    DBUG_VOID_RETURN; /* sanity check */
  buf+= LOG_EVENT_MINIMAL_HEADER_LEN;
  if ((common_header_len=buf[ST_COMMON_HEADER_LEN_OFFSET]) < OLD_HEADER_LEN)
    DBUG_VOID_RETURN; /* sanity check */
  number_of_event_types=
    event_len - (LOG_EVENT_MINIMAL_HEADER_LEN + ST_COMMON_HEADER_LEN_OFFSET + 1);
  DBUG_PRINT("info", ("common_header_len=%d number_of_event_types=%d",
                      common_header_len, number_of_event_types));
  /* If alloc fails, we'll detect it in is_valid() */

  post_header_len= (uint8*) my_memdup((uchar*)buf+ST_COMMON_HEADER_LEN_OFFSET+1,
                                      number_of_event_types*
                                      sizeof(*post_header_len),
                                      MYF(0));
  calc_server_version_split();
  if (!is_version_before_checksum(&server_version_split))
  {
    /* the last bytes are the checksum alg desc and value (or value's room) */
    number_of_event_types -= BINLOG_CHECKSUM_ALG_DESC_LEN;
    checksum_alg= post_header_len[number_of_event_types];
  }
  else
  {
    checksum_alg= (uint8) BINLOG_CHECKSUM_ALG_UNDEF;
  }

  /*
    In some previous versions, the events were given other event type
    id numbers than in the present version. When replicating from such
    a version, we therefore set up an array that maps those id numbers
    to the id numbers of the present server.

    If post_header_len is null, it means malloc failed, and is_valid
    will fail, so there is no need to do anything.

    The trees in which events have wrong id's are:

    mysql-5.1-wl1012.old mysql-5.1-wl2325-5.0-drop6p13-alpha
    mysql-5.1-wl2325-5.0-drop6 mysql-5.1-wl2325-5.0
    mysql-5.1-wl2325-no-dd

    (this was found by grepping for two lines in sequence where the
    first matches "FORMAT_DESCRIPTION_EVENT," and the second matches
    "TABLE_MAP_EVENT," in log_event.h in all trees)

    In these trees, the following server_versions existed since
    TABLE_MAP_EVENT was introduced:

    5.1.1-a_drop5p3   5.1.1-a_drop5p4        5.1.1-alpha
    5.1.2-a_drop5p10  5.1.2-a_drop5p11       5.1.2-a_drop5p12
    5.1.2-a_drop5p13  5.1.2-a_drop5p14       5.1.2-a_drop5p15
    5.1.2-a_drop5p16  5.1.2-a_drop5p16b      5.1.2-a_drop5p16c
    5.1.2-a_drop5p17  5.1.2-a_drop5p4        5.1.2-a_drop5p5
    5.1.2-a_drop5p6   5.1.2-a_drop5p7        5.1.2-a_drop5p8
    5.1.2-a_drop5p9   5.1.3-a_drop5p17       5.1.3-a_drop5p17b
    5.1.3-a_drop5p17c 5.1.4-a_drop5p18       5.1.4-a_drop5p19
    5.1.4-a_drop5p20  5.1.4-a_drop6p0        5.1.4-a_drop6p1
    5.1.4-a_drop6p2   5.1.5-a_drop5p20       5.2.0-a_drop6p3
    5.2.0-a_drop6p4   5.2.0-a_drop6p5        5.2.0-a_drop6p6
    5.2.1-a_drop6p10  5.2.1-a_drop6p11       5.2.1-a_drop6p12
    5.2.1-a_drop6p6   5.2.1-a_drop6p7        5.2.1-a_drop6p8
    5.2.2-a_drop6p13  5.2.2-a_drop6p13-alpha 5.2.2-a_drop6p13b
    5.2.2-a_drop6p13c

    (this was found by grepping for "mysql," in all historical
    versions of configure.in in the trees listed above).

    There are 5.1.1-alpha versions that use the new event id's, so we
    do not test that version string.  So replication from 5.1.1-alpha
    with the other event id's to a new version does not work.
    Moreover, we can safely ignore the part after drop[56].  This
    allows us to simplify the big list above to the following regexes:

    5\.1\.[1-5]-a_drop5.*
    5\.1\.4-a_drop6.*
    5\.2\.[0-2]-a_drop6.*

    This is what we test for in the 'if' below.
  */
  if (post_header_len &&
      server_version[0] == '5' && server_version[1] == '.' &&
      server_version[3] == '.' &&
      strncmp(server_version + 5, "-a_drop", 7) == 0 &&
      ((server_version[2] == '1' &&
        server_version[4] >= '1' && server_version[4] <= '5' &&
        server_version[12] == '5') ||
       (server_version[2] == '1' &&
        server_version[4] == '4' &&
        server_version[12] == '6') ||
       (server_version[2] == '2' &&
        server_version[4] >= '0' && server_version[4] <= '2' &&
        server_version[12] == '6')))
  {
    if (number_of_event_types != 22)
    {
      DBUG_PRINT("info", (" number_of_event_types=%d",
                          number_of_event_types));
      /* this makes is_valid() return false. */
      my_free(post_header_len);
      post_header_len= NULL;
      DBUG_VOID_RETURN;
    }
    static const uint8 perm[23]=
      {
        UNKNOWN_EVENT, START_EVENT_V3, QUERY_EVENT, STOP_EVENT, ROTATE_EVENT,
        INTVAR_EVENT, LOAD_EVENT, SLAVE_EVENT, CREATE_FILE_EVENT,
        APPEND_BLOCK_EVENT, EXEC_LOAD_EVENT, DELETE_FILE_EVENT,
        NEW_LOAD_EVENT,
        RAND_EVENT, USER_VAR_EVENT,
        FORMAT_DESCRIPTION_EVENT,
        TABLE_MAP_EVENT,
        PRE_GA_WRITE_ROWS_EVENT,
        PRE_GA_UPDATE_ROWS_EVENT,
        PRE_GA_DELETE_ROWS_EVENT,
        XID_EVENT,
        BEGIN_LOAD_QUERY_EVENT,
        EXECUTE_LOAD_QUERY_EVENT,
      };
    event_type_permutation= perm;
    /*
      Since we use (permuted) event id's to index the post_header_len
      array, we need to permute the post_header_len array too.
    */
    uint8 post_header_len_temp[23];
    for (int i= 1; i < 23; i++)
      post_header_len_temp[perm[i] - 1]= post_header_len[i - 1];
    for (int i= 0; i < 22; i++)
      post_header_len[i] = post_header_len_temp[i];
  }
  DBUG_VOID_RETURN;
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
  uchar buff[FORMAT_DESCRIPTION_HEADER_LEN + BINLOG_CHECKSUM_ALG_DESC_LEN];
  size_t rec_size= sizeof(buff);
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy((char*) buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= get_time();
  int4store(buff + ST_CREATED_OFFSET,created);
  buff[ST_COMMON_HEADER_LEN_OFFSET]= LOG_EVENT_HEADER_LEN;
  memcpy((char*) buff+ST_COMMON_HEADER_LEN_OFFSET + 1, (uchar*) post_header_len,
         LOG_EVENT_TYPES);
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
  data_written= 0; // to prepare for need_checksum assert
#endif
  buff[FORMAT_DESCRIPTION_HEADER_LEN]= need_checksum() ?
    checksum_alg : (uint8) BINLOG_CHECKSUM_ALG_OFF;
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

  if ((no_checksum= (checksum_alg == BINLOG_CHECKSUM_ALG_OFF)))
  {
    checksum_alg= BINLOG_CHECKSUM_ALG_CRC32;  // Forcing (V) room to fill anyway
  }
  ret= (write_header(file, rec_size) ||
        wrapper_my_b_safe_write(file, buff, rec_size) ||
        write_footer(file));
  if (no_checksum)
    checksum_alg= BINLOG_CHECKSUM_ALG_OFF;
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
  if (!is_artificial_event() && created && thd->transaction.all.ha_list)
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
    delete rli->relay_log.description_event_for_exec;
    const_cast<Relay_log_info *>(rli)->relay_log.description_event_for_exec= this;
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

static inline void
do_server_version_split(char* version,
                        Format_description_log_event::master_version_split *split_versions)
{
  char *p= version, *r;
  ulong number;
  for (uint i= 0; i<=2; i++)
  {
    number= strtoul(p, &r, 10);
    /*
      It is an invalid version if any version number greater than 255 or
      first number is not followed by '.'.
    */
    if (number < 256 && (*r == '.' || i != 0))
      split_versions->ver[i]= (uchar) number;
    else
    {
      split_versions->ver[0]= 0;
      split_versions->ver[1]= 0;
      split_versions->ver[2]= 0;
      break;
    }

    p= r;
    if (*r == '.')
      p++; // skip the dot
  }
  if (strstr(p, "MariaDB") != 0 || strstr(p, "-maria-") != 0)
    split_versions->kind=
      Format_description_log_event::master_version_split::KIND_MARIADB;
  else
    split_versions->kind=
      Format_description_log_event::master_version_split::KIND_MYSQL;
}


/**
   Splits the event's 'server_version' string into three numeric pieces stored
   into 'server_version_split':
   X.Y.Zabc (X,Y,Z numbers, a not a digit) -> {X,Y,Z}
   X.Yabc -> {X,Y,0}
   'server_version_split' is then used for lookups to find if the server which
   created this event has some known bug.
*/
void Format_description_log_event::calc_server_version_split()
{
  do_server_version_split(server_version, &server_version_split);

  DBUG_PRINT("info",("Format_description_log_event::server_version_split:"
                     " '%s' %d %d %d", server_version,
                     server_version_split.ver[0],
                     server_version_split.ver[1], server_version_split.ver[2]));
}

static inline ulong
version_product(const Format_description_log_event::master_version_split* version_split)
{
  return ((version_split->ver[0] * 256 + version_split->ver[1]) * 256
          + version_split->ver[2]);
}

/**
   @return TRUE is the event's version is earlier than one that introduced
   the replication event checksum. FALSE otherwise.
*/
bool
Format_description_log_event::is_version_before_checksum(const master_version_split
                                                         *version_split)
{
  return version_product(version_split) <
    (version_split->kind == master_version_split::KIND_MARIADB ?
     checksum_version_product_mariadb : checksum_version_product_mysql);
}

/**
   @param buf buffer holding serialized FD event
   @param len netto (possible checksum is stripped off) length of the event buf
   
   @return  the version-safe checksum alg descriptor where zero
            designates no checksum, 255 - the orginator is
            checksum-unaware (effectively no checksum) and the actuall
            [1-254] range alg descriptor.
*/
uint8 get_checksum_alg(const char* buf, ulong len)
{
  uint8 ret;
  char version[ST_SERVER_VER_LEN];
  Format_description_log_event::master_version_split version_split;

  DBUG_ENTER("get_checksum_alg");
  DBUG_ASSERT(buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);

  memcpy(version, buf +
         buf[LOG_EVENT_MINIMAL_HEADER_LEN + ST_COMMON_HEADER_LEN_OFFSET]
         + ST_SERVER_VER_OFFSET, ST_SERVER_VER_LEN);
  version[ST_SERVER_VER_LEN - 1]= 0;
  
  do_server_version_split(version, &version_split);
  ret= Format_description_log_event::is_version_before_checksum(&version_split) ?
    (uint8) BINLOG_CHECKSUM_ALG_UNDEF :
    * (uint8*) (buf + len - BINLOG_CHECKSUM_LEN - BINLOG_CHECKSUM_ALG_DESC_LEN);
  DBUG_ASSERT(ret == BINLOG_CHECKSUM_ALG_OFF ||
              ret == BINLOG_CHECKSUM_ALG_UNDEF ||
              ret == BINLOG_CHECKSUM_ALG_CRC32);
  DBUG_RETURN(ret);
}
  

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

/*
  Load_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Load_log_event::print_query(THD *thd, bool need_db, const char *cs,
                                 String *buf, my_off_t *fn_start,
                                 my_off_t *fn_end, const char *qualify_db)
{
  if (need_db && db && db_len)
  {
    buf->append(STRING_WITH_LEN("use "));
    append_identifier(thd, buf, db, db_len);
    buf->append(STRING_WITH_LEN("; "));
  }

  buf->append(STRING_WITH_LEN("LOAD DATA "));

  if (is_concurrent)
    buf->append(STRING_WITH_LEN("CONCURRENT "));

  if (fn_start)
    *fn_start= buf->length();

  if (check_fname_outside_temp_buf())
    buf->append(STRING_WITH_LEN("LOCAL "));
  buf->append(STRING_WITH_LEN("INFILE '"));
  buf->append_for_single_quote(fname, fname_len);
  buf->append(STRING_WITH_LEN("' "));

  if (sql_ex.opt_flags & REPLACE_FLAG)
    buf->append(STRING_WITH_LEN("REPLACE "));
  else if (sql_ex.opt_flags & IGNORE_FLAG)
    buf->append(STRING_WITH_LEN("IGNORE "));

  buf->append(STRING_WITH_LEN("INTO"));

  if (fn_end)
    *fn_end= buf->length();

  buf->append(STRING_WITH_LEN(" TABLE "));
  if (qualify_db)
  {
    append_identifier(thd, buf, qualify_db, strlen(qualify_db));
    buf->append(STRING_WITH_LEN("."));
  }
  append_identifier(thd, buf, table_name, table_name_len);

  if (cs != NULL)
  {
    buf->append(STRING_WITH_LEN(" CHARACTER SET "));
    buf->append(cs, strlen(cs));
  }

  /* We have to create all optional fields as the default is not empty */
  buf->append(STRING_WITH_LEN(" FIELDS TERMINATED BY "));
  pretty_print_str(buf, sql_ex.field_term, sql_ex.field_term_len);
  if (sql_ex.opt_flags & OPT_ENCLOSED_FLAG)
    buf->append(STRING_WITH_LEN(" OPTIONALLY "));
  buf->append(STRING_WITH_LEN(" ENCLOSED BY "));
  pretty_print_str(buf, sql_ex.enclosed, sql_ex.enclosed_len);

  buf->append(STRING_WITH_LEN(" ESCAPED BY "));
  pretty_print_str(buf, sql_ex.escaped, sql_ex.escaped_len);

  buf->append(STRING_WITH_LEN(" LINES TERMINATED BY "));
  pretty_print_str(buf, sql_ex.line_term, sql_ex.line_term_len);
  if (sql_ex.line_start_len)
  {
    buf->append(STRING_WITH_LEN(" STARTING BY "));
    pretty_print_str(buf, sql_ex.line_start, sql_ex.line_start_len);
  }

  if ((long) skip_lines > 0)
  {
    buf->append(STRING_WITH_LEN(" IGNORE "));
    buf->append_ulonglong(skip_lines);
    buf->append(STRING_WITH_LEN(" LINES "));
  }

  if (num_fields)
  {
    uint i;
    const char *field= fields;
    buf->append(STRING_WITH_LEN(" ("));
    for (i = 0; i < num_fields; i++)
    {
      if (i)
      {
        /*
          Yes, the space and comma is reversed here. But this is mostly dead
          code, at most used when reading really old binlogs from old servers,
          so better just leave it as is...
        */
        buf->append(STRING_WITH_LEN(" ,"));
      }
      append_identifier(thd, buf, field, field_lens[i]);
      field+= field_lens[i]  + 1;
    }
    buf->append(STRING_WITH_LEN(")"));
  }
}


void Load_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char query_buffer[1024];
  String query_str(query_buffer, sizeof(query_buffer), system_charset_info);

  query_str.length(0);
  print_query(thd, TRUE, NULL, &query_str, 0, 0, NULL);
  protocol->store(query_str.ptr(), query_str.length(), &my_charset_bin);
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


#ifndef MYSQL_CLIENT

/*
  Load_log_event::write_data_header()
*/

bool Load_log_event::write_data_header(IO_CACHE* file)
{
  char buf[LOAD_HEADER_LEN];
  int4store(buf + L_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  return my_b_safe_write(file, (uchar*)buf, LOAD_HEADER_LEN) != 0;
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
  :Log_event(thd_arg,
             thd_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F : 0,
             using_trans),
   thread_id(thd_arg->thread_id),
   slave_proxy_id(thd_arg->variables.pseudo_thread_id),
   num_fields(0),fields(0),
   field_lens(0),field_block_len(0),
   table_name(table_name_arg ? table_name_arg : ""),
   db(db_arg), fname(ex->file_name), local_fname(FALSE),
   is_concurrent(is_concurrent_arg)
{
  time_t end_time;
  time(&end_time);
  exec_time = (ulong) (end_time  - thd_arg->start_time);
  /* db can never be a zero pointer in 4.0 */
  db_len = (uint32) strlen(db);
  table_name_len = (uint32) strlen(table_name);
  fname_len = (fname) ? (uint) strlen(fname) : 0;
  sql_ex.field_term = (char*) ex->field_term->ptr();
  sql_ex.field_term_len = (uint8) ex->field_term->length();
  sql_ex.enclosed = (char*) ex->enclosed->ptr();
  sql_ex.enclosed_len = (uint8) ex->enclosed->length();
  sql_ex.line_term = (char*) ex->line_term->ptr();
  sql_ex.line_term_len = (uint8) ex->line_term->length();
  sql_ex.line_start = (char*) ex->line_start->ptr();
  sql_ex.line_start_len = (uint8) ex->line_start->length();
  sql_ex.escaped = (char*) ex->escaped->ptr();
  sql_ex.escaped_len = (uint8) ex->escaped->length();
  sql_ex.opt_flags = 0;
  sql_ex.cached_new_format = -1;
    
  if (ex->dumpfile)
    sql_ex.opt_flags|= DUMPFILE_FLAG;
  if (ex->opt_enclosed)
    sql_ex.opt_flags|= OPT_ENCLOSED_FLAG;

  sql_ex.empty_flags= 0;

  switch (handle_dup) {
  case DUP_REPLACE:
    sql_ex.opt_flags|= REPLACE_FLAG;
    break;
  case DUP_UPDATE:				// Impossible here
  case DUP_ERROR:
    break;	
  }
  if (ignore)
    sql_ex.opt_flags|= IGNORE_FLAG;

  if (!ex->field_term->length())
    sql_ex.empty_flags |= FIELD_TERM_EMPTY;
  if (!ex->enclosed->length())
    sql_ex.empty_flags |= ENCLOSED_EMPTY;
  if (!ex->line_term->length())
    sql_ex.empty_flags |= LINE_TERM_EMPTY;
  if (!ex->line_start->length())
    sql_ex.empty_flags |= LINE_START_EMPTY;
  if (!ex->escaped->length())
    sql_ex.empty_flags |= ESCAPED_EMPTY;
    
  skip_lines = ex->skip_lines;

  List_iterator<Item> li(fields_arg);
  field_lens_buf.length(0);
  fields_buf.length(0);
  Item* item;
  while ((item = li++))
  {
    num_fields++;
    uchar len = (uchar) strlen(item->name);
    field_block_len += len + 1;
    fields_buf.append(item->name, len + 1);
    field_lens_buf.append((char*)&len, 1);
  }

  field_lens = (const uchar*)field_lens_buf.ptr();
  fields = fields_buf.ptr();
}
#endif /* !MYSQL_CLIENT */


/**
  @note
    The caller must do buf[event_len] = 0 before he starts using the
    constructed event.
*/
Load_log_event::Load_log_event(const char *buf, uint event_len,
                               const Format_description_log_event *description_event)
  :Log_event(buf, description_event), num_fields(0), fields(0),
   field_lens(0),field_block_len(0),
   table_name(0), db(0), fname(0), local_fname(FALSE),
   /*
     Load_log_event which comes from the binary log does not contain
     information about the type of insert which was used on the master.
     Assume that it was an ordinary, non-concurrent LOAD DATA.
    */
   is_concurrent(FALSE)
{
  DBUG_ENTER("Load_log_event");
  /*
    I (Guilhem) manually tested replication of LOAD DATA INFILE for 3.23->5.0,
    4.0->5.0 and 5.0->5.0 and it works.
  */
  if (event_len)
    copy_log_event(buf, event_len,
                   (((uchar)buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                   LOAD_HEADER_LEN + 
                    description_event->common_header_len :
                    LOAD_HEADER_LEN + LOG_EVENT_HEADER_LEN),
                   description_event);
  /* otherwise it's a derived class, will call copy_log_event() itself */
  DBUG_VOID_RETURN;
}


/*
  Load_log_event::copy_log_event()
*/

int Load_log_event::copy_log_event(const char *buf, ulong event_len,
                                   int body_offset,
                                   const Format_description_log_event *description_event)
{
  DBUG_ENTER("Load_log_event::copy_log_event");
  uint data_len;
  char* buf_end = (char*)buf + event_len;
  /* this is the beginning of the post-header */
  const char* data_head = buf + description_event->common_header_len;
  slave_proxy_id= thread_id= uint4korr(data_head + L_THREAD_ID_OFFSET);
  exec_time = uint4korr(data_head + L_EXEC_TIME_OFFSET);
  skip_lines = uint4korr(data_head + L_SKIP_LINES_OFFSET);
  table_name_len = (uint)data_head[L_TBL_LEN_OFFSET];
  db_len = (uint)data_head[L_DB_LEN_OFFSET];
  num_fields = uint4korr(data_head + L_NUM_FIELDS_OFFSET);
	  
  if ((int) event_len < body_offset)
    DBUG_RETURN(1);
  /*
    Sql_ex.init() on success returns the pointer to the first byte after
    the sql_ex structure, which is the start of field lengths array.
  */
  if (!(field_lens= (uchar*)sql_ex.init((char*)buf + body_offset,
                                        buf_end,
                                        (uchar)buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))
    DBUG_RETURN(1);
  
  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    DBUG_RETURN(1);
  for (uint i = 0; i < num_fields; i++)
    field_block_len += (uint)field_lens[i] + 1;

  fields = (char*)field_lens + num_fields;
  table_name  = fields + field_block_len;
  db = table_name + table_name_len + 1;
  DBUG_EXECUTE_IF ("simulate_invalid_address",
                   db_len = data_len;);
  fname = db + db_len + 1;
  if ((db_len > data_len) || (fname > buf_end))
    goto err;
  fname_len = (uint) strlen(fname);
  if ((fname_len > data_len) || (fname + fname_len > buf_end))
    goto err;
  // null termination is accomplished by the caller doing buf[event_len]=0

  DBUG_RETURN(0);

err:
  // Invalid event.
  table_name = 0;
  DBUG_RETURN(1);
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
  Write_on_release_cache cache(&print_event_info->head_cache, file_arg);

  DBUG_ENTER("Load_log_event::print");
  if (!print_event_info->short_form)
  {
    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tQuery\tthread_id=%ld\texec_time=%ld\n",
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
    my_b_printf(&cache, "%suse %`s%s\n",
            commented ? "# " : "",
            db, print_event_info->delimiter);

  if (flags & LOG_EVENT_THREAD_SPECIFIC_F)
    my_b_printf(&cache,"%sSET @@session.pseudo_thread_id=%lu%s\n",
            commented ? "# " : "", (ulong)thread_id,
            print_event_info->delimiter);
  my_b_printf(&cache, "%sLOAD DATA ",
              commented ? "# " : "");
  if (check_fname_outside_temp_buf())
    my_b_printf(&cache, "LOCAL ");
  my_b_printf(&cache, "INFILE '%-*s' ", fname_len, fname);

  if (sql_ex.opt_flags & REPLACE_FLAG)
    my_b_printf(&cache,"REPLACE ");
  else if (sql_ex.opt_flags & IGNORE_FLAG)
    my_b_printf(&cache,"IGNORE ");
  
  my_b_printf(&cache, "INTO TABLE `%s`", table_name);
  my_b_printf(&cache, " FIELDS TERMINATED BY ");
  pretty_print_str(&cache, sql_ex.field_term, sql_ex.field_term_len);

  if (sql_ex.opt_flags & OPT_ENCLOSED_FLAG)
    my_b_printf(&cache," OPTIONALLY ");
  my_b_printf(&cache, " ENCLOSED BY ");
  pretty_print_str(&cache, sql_ex.enclosed, sql_ex.enclosed_len);
     
  my_b_printf(&cache, " ESCAPED BY ");
  pretty_print_str(&cache, sql_ex.escaped, sql_ex.escaped_len);
     
  my_b_printf(&cache," LINES TERMINATED BY ");
  pretty_print_str(&cache, sql_ex.line_term, sql_ex.line_term_len);


  if (sql_ex.line_start)
  {
    my_b_printf(&cache," STARTING BY ");
    pretty_print_str(&cache, sql_ex.line_start, sql_ex.line_start_len);
  }
  if ((long) skip_lines > 0)
    my_b_printf(&cache, " IGNORE %ld LINES", (long) skip_lines);

  if (num_fields)
  {
    uint i;
    const char* field = fields;
    my_b_printf(&cache, " (");
    for (i = 0; i < num_fields; i++)
    {
      if (i)
        my_b_printf(&cache, ",");
      my_b_printf(&cache, "%`s", field);

      field += field_lens[i]  + 1;
    }
    my_b_printf(&cache, ")");
  }

  my_b_printf(&cache, "%s\n", print_event_info->delimiter);
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
                                     function to have RPL_LOG_NAME and
                                     rli->last_slave_error, both being used by
                                     error reports. rli's position advancing
                                     is skipped (done by the caller which is
                                     Execute_load_log_event::exec_event).
                                     If set to 0, rli is provided for full use,
                                     i.e. for error reports and position
                                     advancing.

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
  LEX_STRING new_db;
  DBUG_ENTER("Load_log_event::do_apply_event");

  new_db.length= db_len;
  new_db.str= (char *) rpl_filter->get_rewrite_db(db, &new_db.length);
  thd->set_db(new_db.str, new_db.length);
  DBUG_ASSERT(thd->query() == 0);
  thd->reset_query_inner();                    // Should not be needed
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

  if (!use_rli_only_for_errors)
  {
    /*
      Saved for InnoDB, see comment in
      Query_log_event::do_apply_event()
    */
    const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;
    DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));
  }
 
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


    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::do_apply_event(), then the companion SET also have so
            we don't need to reset_one_shot_variables().
  */
  if (rpl_filter->db_ok(thd->db))
  {
    thd->set_time(when, when_sec_part);
    thd->set_query_id(next_query_id());
    thd->warning_info->opt_clear_warning_info(thd->query_id);

    TABLE_LIST tables;
    tables.init_one_table(thd->strmake(thd->db, thd->db_length),
                          thd->db_length,
                          table_name, strlen(table_name),
                          table_name, TL_WRITE);
    tables.updating= 1;

    // the table will be opened in mysql_load    
    if (rpl_filter->is_on() && !rpl_filter->tables_ok(thd->db, &tables))
    {
      // TODO: this is a bug - this needs to be moved to the I/O thread
      if (net)
        skip_load_data_infile(net);
    }
    else
    {
      char llbuff[22];
      enum enum_duplicates handle_dup;
      bool ignore= 0;
      char query_buffer[1024];
      String query_str(query_buffer, sizeof(query_buffer), system_charset_info);
      char *load_data_query;

      query_str.length(0);
      /*
        Forge LOAD DATA INFILE query which will be used in SHOW PROCESS LIST
        and written to slave's binlog if binlogging is on.
      */
      print_query(thd, FALSE, NULL, &query_str, NULL, NULL, NULL);
      if (!(load_data_query= (char *)thd->strmake(query_str.ptr(),
                                                  query_str.length())))
      {
        /*
          This will set thd->fatal_error in case of OOM. So we surely will notice
          that something is wrong.
        */
        goto error;
      }

      thd->set_query(load_data_query, (uint) (query_str.length()));

      if (sql_ex.opt_flags & REPLACE_FLAG)
        handle_dup= DUP_REPLACE;
      else if (sql_ex.opt_flags & IGNORE_FLAG)
      {
        ignore= 1;
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

      sql_exchange ex((char*)fname, sql_ex.opt_flags & DUMPFILE_FLAG);
      String field_term(sql_ex.field_term,sql_ex.field_term_len,log_cs);
      String enclosed(sql_ex.enclosed,sql_ex.enclosed_len,log_cs);
      String line_term(sql_ex.line_term,sql_ex.line_term_len,log_cs);
      String line_start(sql_ex.line_start,sql_ex.line_start_len,log_cs);
      String escaped(sql_ex.escaped,sql_ex.escaped_len, log_cs);
      ex.field_term= &field_term;
      ex.enclosed= &enclosed;
      ex.line_term= &line_term;
      ex.line_start= &line_start;
      ex.escaped= &escaped;

      ex.opt_enclosed = (sql_ex.opt_flags & OPT_ENCLOSED_FLAG);
      if (sql_ex.empty_flags & FIELD_TERM_EMPTY)
        ex.field_term->length(0);

      ex.skip_lines = skip_lines;
      List<Item> field_list;
      thd->lex->select_lex.context.resolve_in_table_list_only(&tables);
      set_fields(tables.db, field_list, &thd->lex->select_lex.context);
      thd->variables.pseudo_thread_id= thread_id;
      if (net)
      {
        // mysql_load will use thd->net to read the file
        thd->net.vio = net->vio;
        // Make sure the client does not get confused about the packet sequence
        thd->net.pkt_nr = net->pkt_nr;
      }
      /*
        It is safe to use tmp_list twice because we are not going to
        update it inside mysql_load().
      */
      List<Item> tmp_list;
      if (mysql_load(thd, &ex, &tables, field_list, tmp_list, tmp_list,
                     handle_dup, ignore, net != 0))
        thd->is_slave_error= 1;
      if (thd->cuted_fields)
      {
        /* log_pos is the position of the LOAD event in the master log */
        sql_print_warning("Slave: load data infile on table '%s' at "
                          "log position %s in log '%s' produced %ld "
                          "warning(s). Default database: '%s'",
                          (char*) table_name,
                          llstr(log_pos,llbuff), RPL_LOG_NAME, 
                          (ulong) thd->cuted_fields,
                          print_slave_db_safe(thd->db));
      }
      if (net)
        net->pkt_nr= thd->net.pkt_nr;
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
  thd->net.vio = 0; 
  const char *remember_db= thd->db;
  thd->catalog= 0;
  thd->set_db(NULL, 0);                   /* will free the current database */
  thd->reset_query();
  thd->stmt_da->can_overwrite_status= TRUE;
  thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
  thd->stmt_da->can_overwrite_status= FALSE;
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
      err= thd->stmt_da->message();
      sql_errno= thd->stmt_da->sql_errno();
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
    DBUG_RETURN(1);
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
    DBUG_RETURN(1);
  }

  DBUG_RETURN( use_rli_only_for_errors ? 0 : Log_event::do_apply_event(rli) ); 
}
#endif


/**************************************************************************
  Rotate_log_event methods
**************************************************************************/

/*
  Rotate_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Rotate_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf1[256], buf[22];
  String tmp(buf1, sizeof(buf1), log_cs);
  tmp.length(0);
  tmp.append(new_log_ident, ident_len);
  tmp.append(STRING_WITH_LEN(";pos="));
  tmp.append(llstr(pos,buf));
  protocol->store(tmp.ptr(), tmp.length(), &my_charset_bin);
}
#endif


/*
  Rotate_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Rotate_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char buf[22];
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\tRotate to ");
  if (new_log_ident)
    my_b_write(&cache, (uchar*) new_log_ident, (uint)ident_len);
  my_b_printf(&cache, "  pos: %s\n", llstr(pos, buf));
}
#endif /* MYSQL_CLIENT */



/*
  Rotate_log_event::Rotate_log_event() (2 constructors)
*/


#ifndef MYSQL_CLIENT
Rotate_log_event::Rotate_log_event(const char* new_log_ident_arg,
                                   uint ident_len_arg, ulonglong pos_arg,
                                   uint flags_arg)
  :Log_event(), new_log_ident(new_log_ident_arg),
   pos(pos_arg),ident_len(ident_len_arg ? ident_len_arg :
                          (uint) strlen(new_log_ident_arg)), flags(flags_arg)
{
#ifndef DBUG_OFF
  char buff[22];
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(...,flags)");
  DBUG_PRINT("enter",("new_log_ident: %s  pos: %s  flags: %lu", new_log_ident_arg,
                      llstr(pos_arg, buff), (ulong) flags));
#endif
  cache_type= EVENT_NO_CACHE;
  if (flags & DUP_NAME)
    new_log_ident= my_strndup(new_log_ident_arg, ident_len, MYF(MY_WME));
  if (flags & RELAY_LOG)
    set_relay_log_event();
  DBUG_VOID_RETURN;
}
#endif


Rotate_log_event::Rotate_log_event(const char* buf, uint event_len,
                                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event) ,new_log_ident(0), flags(DUP_NAME)
{
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(char*,...)");
  // The caller will ensure that event_len is what we have at EVENT_LEN_OFFSET
  uint8 header_size= description_event->common_header_len;
  uint8 post_header_len= description_event->post_header_len[ROTATE_EVENT-1];
  uint ident_offset;
  if (event_len < header_size)
    DBUG_VOID_RETURN;
  buf += header_size;
  pos = post_header_len ? uint8korr(buf + R_POS_OFFSET) : 4;
  ident_len = (uint)(event_len -
                     (header_size+post_header_len)); 
  ident_offset = post_header_len; 
  set_if_smaller(ident_len,FN_REFLEN-1);
  new_log_ident= my_strndup(buf + ident_offset, (uint) ident_len, MYF(MY_WME));
  DBUG_PRINT("debug", ("new_log_ident: '%s'", new_log_ident));
  DBUG_VOID_RETURN;
}


/*
  Rotate_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Rotate_log_event::write(IO_CACHE* file)
{
  char buf[ROTATE_HEADER_LEN];
  int8store(buf + R_POS_OFFSET, pos);
  return (write_header(file, ROTATE_HEADER_LEN + ident_len) || 
          wrapper_my_b_safe_write(file, (uchar*) buf, ROTATE_HEADER_LEN) ||
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

  @retval
    0	ok
*/
int Rotate_log_event::do_update_pos(Relay_log_info *rli)
{
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
  if ((server_id != ::server_id || rli->replicate_same_server_id) &&
      !is_relay_log_event() &&
      !rli->is_in_group())
  {
    mysql_mutex_lock(&rli->data_lock);
    DBUG_PRINT("info", ("old group_master_log_name: '%s'  "
                        "old group_master_log_pos: %lu",
                        rli->group_master_log_name,
                        (ulong) rli->group_master_log_pos));
    memcpy(rli->group_master_log_name, new_log_ident, ident_len+1);
    rli->notify_group_master_log_name_update();
    rli->inc_group_relay_log_pos(pos, TRUE /* skip_lock */);
    DBUG_PRINT("info", ("new group_master_log_name: '%s'  "
                        "new group_master_log_pos: %lu",
                        rli->group_master_log_name,
                        (ulong) rli->group_master_log_pos));
    mysql_mutex_unlock(&rli->data_lock);
    flush_relay_log_info(rli);
    
    /*
      Reset thd->variables.option_bits and sql_mode etc, because this could be the signal of
      a master's downgrade from 5.0 to 4.0.
      However, no need to reset description_event_for_exec: indeed, if the next
      master is 5.0 (even 5.0.1) we will soon get a Format_desc; if the next
      master is 4.0 then the events are in the slave's format (conversion).
    */
    set_slave_thread_options(thd);
    set_slave_thread_default_charset(thd, rli);
    thd->variables.sql_mode= global_system_variables.sql_mode;
    thd->variables.auto_increment_increment=
      thd->variables.auto_increment_offset= 1;
  }
  else
    rli->inc_event_relay_log_pos();


  DBUG_RETURN(0);
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
void Intvar_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[256], *pos;
  pos= strmake(buf, get_var_type_name(), sizeof(buf)-23);
  *pos++= '=';
  pos= longlong10_to_str(val, pos, -10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif


/*
  Intvar_log_event::Intvar_log_event()
*/

Intvar_log_event::Intvar_log_event(const char* buf,
                                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  /* The Post-Header is empty. The Varible Data part begins immediately. */
  buf+= description_event->common_header_len +
    description_event->post_header_len[INTVAR_EVENT-1];
  type= buf[I_TYPE_OFFSET];
  val= uint8korr(buf+I_VAL_OFFSET);
}


/*
  Intvar_log_event::get_var_type_name()
*/

const char* Intvar_log_event::get_var_type_name()
{
  switch(type) {
  case LAST_INSERT_ID_EVENT: return "LAST_INSERT_ID";
  case INSERT_ID_EVENT: return "INSERT_ID";
  default: /* impossible */ return "UNKNOWN";
  }
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
  const char *msg;
  LINT_INIT(msg);
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (!print_event_info->short_form)
  {
    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tIntvar\n");
  }

  my_b_printf(&cache, "SET ");
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
  my_b_printf(&cache, "%s=%s%s\n",
              msg, llstr(val,llbuff), print_event_info->delimiter);
}
#endif


#if defined(HAVE_REPLICATION)&& !defined(MYSQL_CLIENT)

/*
  Intvar_log_event::do_apply_event()
*/

int Intvar_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Intvar_log_event::do_apply_event");
  /*
    We are now in a statement until the associated query log event has
    been processed.
   */
  const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

  if (rli->deferred_events_collecting)
  {
    DBUG_PRINT("info",("deferring event"));
    DBUG_RETURN(rli->deferred_events->add(this));
  }

  switch (type) {
  case LAST_INSERT_ID_EVENT:
    thd->first_successful_insert_id_in_prev_stmt= val;
    DBUG_PRINT("info",("last_insert_id_event: %ld", (long) val));
    break;
  case INSERT_ID_EVENT:
    thd->force_one_auto_inc_interval(val);
    break;
  }
  DBUG_RETURN(0);
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
void Rand_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf1[256], *pos;
  pos= strmov(buf1,"rand_seed1=");
  pos= int10_to_str((long) seed1, pos, 10);
  pos= strmov(pos, ",rand_seed2=");
  pos= int10_to_str((long) seed2, pos, 10);
  protocol->store(buf1, (uint) (pos-buf1), &my_charset_bin);
}
#endif


Rand_log_event::Rand_log_event(const char* buf,
                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  /* The Post-Header is empty. The Variable Data part begins immediately. */
  buf+= description_event->common_header_len +
    description_event->post_header_len[RAND_EVENT-1];
  seed1= uint8korr(buf+RAND_SEED1_OFFSET);
  seed2= uint8korr(buf+RAND_SEED2_OFFSET);
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
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  char llbuff[22],llbuff2[22];
  if (!print_event_info->short_form)
  {
    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tRand\n");
  }
  my_b_printf(&cache, "SET @@RAND_SEED1=%s, @@RAND_SEED2=%s%s\n",
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

  return res;
}

#endif /* !MYSQL_CLIENT */


/**************************************************************************
  Xid_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Xid_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[128], *pos;
  pos= strmov(buf, "COMMIT /* xid=");
  pos= longlong10_to_str(xid, pos, 10);
  pos= strmov(pos, " */");
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif

/**
  @note
  It's ok not to use int8store here,
  as long as xid_t::set(ulonglong) and
  xid_t::get_my_xid doesn't do it either.
  We don't care about actual values of xids as long as
  identical numbers compare identically
*/

Xid_log_event::
Xid_log_event(const char* buf,
              const Format_description_log_event *description_event)
  :Log_event(buf, description_event)
{
  /* The Post-Header is empty. The Variable Data part begins immediately. */
  buf+= description_event->common_header_len +
    description_event->post_header_len[XID_EVENT-1];
  memcpy((char*) &xid, buf, sizeof(xid));
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
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (!print_event_info->short_form)
  {
    char buf[64];
    longlong10_to_str(xid, buf, 10);

    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tXid = %s\n", buf);
  }
  my_b_printf(&cache, "COMMIT%s\n", print_event_info->delimiter);
}
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Xid_log_event::do_apply_event(Relay_log_info const *rli)
{
  bool res;
  /* For a slave Xid_log_event is COMMIT */
  general_log_print(thd, COM_QUERY,
                    "COMMIT /* implicit, from Xid_log_event */");
  res= trans_commit(thd); /* Automatically rolls back on error. */
  thd->mdl_context.release_transactional_locks();

  /*
    Increment the global status commit count variable
  */
  status_var_increment(thd->status_var.com_stat[SQLCOM_COMMIT]);

  return res;
}

Log_event::enum_skip_reason
Xid_log_event::do_shall_skip(Relay_log_info *rli)
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
  User_var_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
static bool
user_var_append_name_part(THD *thd, String *buf,
                          const char *name, size_t name_len)
{
  return buf->append("@") ||
    append_identifier(thd, buf, name, name_len) ||
    buf->append("=");
}

void User_var_log_event::pack_info(THD *thd, Protocol* protocol)
{
  if (is_null)
  {
    char buf_mem[FN_REFLEN+7];
    String buf(buf_mem, sizeof(buf_mem), system_charset_info);
    buf.length(0);
    if (user_var_append_name_part(thd, &buf, name, name_len) ||
        buf.append("NULL"))
      return;
    protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
    {
      double real_val;
      char buf2[MY_GCVT_MAX_FIELD_WIDTH+1];
      char buf_mem[FN_REFLEN + MY_GCVT_MAX_FIELD_WIDTH + 1];
      String buf(buf_mem, sizeof(buf_mem), system_charset_info);
      float8get(real_val, val);
      buf.length(0);
      if (user_var_append_name_part(thd, &buf, name, name_len) ||
          buf.append(buf2, my_gcvt(real_val, MY_GCVT_ARG_DOUBLE,
                                   MY_GCVT_MAX_FIELD_WIDTH, buf2, NULL)))
        return;
      protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
      break;
    }
    case INT_RESULT:
    {
      char buf2[22];
      char buf_mem[FN_REFLEN + 22];
      String buf(buf_mem, sizeof(buf_mem), system_charset_info);
      buf.length(0);
      if (user_var_append_name_part(thd, &buf, name, name_len) ||
          buf.append(buf2,
                 longlong10_to_str(uint8korr(val), buf2,
                   ((flags & User_var_log_event::UNSIGNED_F) ? 10 : -10))-buf2))
        return;
      protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
      break;
    }
    case DECIMAL_RESULT:
    {
      char buf_mem[FN_REFLEN + DECIMAL_MAX_STR_LENGTH];
      String buf(buf_mem, sizeof(buf_mem), system_charset_info);
      char buf2[DECIMAL_MAX_STR_LENGTH+1];
      String str(buf2, sizeof(buf2), &my_charset_bin);
      my_decimal dec;
      buf.length(0);
      binary2my_decimal(E_DEC_FATAL_ERROR, (uchar*) (val+2), &dec, val[0],
                        val[1]);
      my_decimal2string(E_DEC_FATAL_ERROR, &dec, 0, 0, 0, &str);
      if (user_var_append_name_part(thd, &buf, name, name_len) ||
          buf.append(buf2))
        return;
      protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
      break;
    }
    case STRING_RESULT:
    {
      /* 15 is for 'COLLATE' and other chars */
      char buf_mem[FN_REFLEN + 512 + 1 + 2*MY_CS_NAME_SIZE+15];
      String buf(buf_mem, sizeof(buf_mem), system_charset_info);
      CHARSET_INFO *cs;
      buf.length(0);
      if (!(cs= get_charset(charset_number, MYF(0))))
      {
        if (buf.append("???"))
          return;
      }
      else
      {
        size_t old_len;
        char *beg, *end;
        if (user_var_append_name_part(thd, &buf, name, name_len) ||
            buf.append("_") ||
            buf.append(cs->csname) ||
            buf.append(" "))
          return;
        old_len= buf.length();
        if (buf.reserve(old_len + val_len * 2 + 3 + sizeof(" COLLATE ") +
                        MY_CS_NAME_SIZE))
          return;
        beg= const_cast<char *>(buf.ptr()) + old_len;
        end= str_to_hex(beg, val, val_len);
        buf.length(old_len + (end - beg));
        if (buf.append(" COLLATE ") ||
            buf.append(cs->name))
          return;
      }
      protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
      break;
    }
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0);
      return;
    }
  }
}
#endif /* !MYSQL_CLIENT */


User_var_log_event::
User_var_log_event(const char* buf, uint event_len,
                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
#ifndef MYSQL_CLIENT
  , deferred(false), query_id(0)
#endif
{
  bool error= false;
  const char* buf_start= buf;
  /* The Post-Header is empty. The Variable Data part begins immediately. */
  const char *start= buf;
  buf+= description_event->common_header_len +
    description_event->post_header_len[USER_VAR_EVENT-1];
  name_len= uint4korr(buf);
  name= (char *) buf + UV_NAME_LEN_SIZE;

  /*
    We don't know yet is_null value, so we must assume that name_len
    may have the bigger value possible, is_null= True and there is no
    payload for val, or even that name_len is 0.
  */
  if (!valid_buffer_range<uint>(name_len, buf_start, name,
                                event_len - UV_VAL_IS_NULL))
  {
    error= true;
    goto err;
  }

  buf+= UV_NAME_LEN_SIZE + name_len;
  is_null= (bool) *buf;
  flags= User_var_log_event::UNDEF_F;    // defaults to UNDEF_F
  if (is_null)
  {
    type= STRING_RESULT;
    charset_number= my_charset_bin.number;
    val_len= 0;
    val= 0;  
  }
  else
  {
    if (!valid_buffer_range<uint>(UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE
                                  + UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE,
                                  buf_start, buf, event_len))
    {
      error= true;
      goto err;
    }

    type= (Item_result) buf[UV_VAL_IS_NULL];
    charset_number= uint4korr(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE);
    val_len= uint4korr(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
                       UV_CHARSET_NUMBER_SIZE);
    val= (char *) (buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
                   UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE);

    if (!valid_buffer_range<uint>(val_len, buf_start, val, event_len))
    {
      error= true;
      goto err;
    }

    /**
      We need to check if this is from an old server
      that did not pack information for flags.
      We do this by checking if there are extra bytes
      after the packed value. If there are we take the
      extra byte and it's value is assumed to contain
      the flags value.

      Old events will not have this extra byte, thence,
      we keep the flags set to UNDEF_F.
    */
    uint bytes_read= ((val + val_len) - start);
#ifndef DBUG_OFF
    bool old_pre_checksum_fd= description_event->is_version_before_checksum(
        &description_event->server_version_split);
#endif
    DBUG_ASSERT((bytes_read == data_written -
                 (old_pre_checksum_fd ||
                  (description_event->checksum_alg ==
                   BINLOG_CHECKSUM_ALG_OFF)) ?
                 0 : BINLOG_CHECKSUM_LEN)
                ||
                (bytes_read == data_written -1 -
                 (old_pre_checksum_fd ||
                  (description_event->checksum_alg ==
                   BINLOG_CHECKSUM_ALG_OFF)) ?
                 0 : BINLOG_CHECKSUM_LEN));
    if ((data_written - bytes_read) > 0)
    {
      flags= (uint) *(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
                    UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE +
                    val_len);
    }
  }

err:
  if (error)
    name= 0;
}


#ifndef MYSQL_CLIENT
bool User_var_log_event::write(IO_CACHE* file)
{
  char buf[UV_NAME_LEN_SIZE];
  char buf1[UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE + 
	    UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE];
  uchar buf2[max(8, DECIMAL_MAX_FIELD_SIZE + 2)], *pos= buf2;
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
      dec->fix_buffer_pointer();
      buf2[0]= (char)(dec->intg + dec->frac);
      buf2[1]= (char)dec->frac;
      decimal2bin((decimal_t*)val, buf2+2, buf2[0], buf2[1]);
      val_len= decimal_bin_size(buf2[0], buf2[1]) + 2;
      break;
    }
    case STRING_RESULT:
      pos= (uchar*) val;
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0);
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
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (!print_event_info->short_form)
  {
    print_header(&cache, print_event_info, FALSE);
    my_b_printf(&cache, "\tUser_var\n");
  }

  my_b_printf(&cache, "SET @");
  my_b_write_backtick_quote(&cache, name, name_len);

  if (is_null)
  {
    my_b_printf(&cache, ":=NULL%s\n", print_event_info->delimiter);
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      double real_val;
      char real_buf[FMT_G_BUFSIZE(14)];
      float8get(real_val, val);
      sprintf(real_buf, "%.14g", real_val);
      my_b_printf(&cache, ":=%s%s\n", real_buf, print_event_info->delimiter);
      break;
    case INT_RESULT:
      char int_buf[22];
      longlong10_to_str(uint8korr(val), int_buf, 
                        ((flags & User_var_log_event::UNSIGNED_F) ? 10 : -10));
      my_b_printf(&cache, ":=%s%s\n", int_buf, print_event_info->delimiter);
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
      my_b_printf(&cache, ":=%s%s\n", str_buf, print_event_info->delimiter);
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

      // 2 hex digits / byte
      hex_str= (char *) my_malloc(2 * val_len + 1 + 3, MYF(MY_WME));
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
        my_b_printf(&cache, ":=???%s\n", print_event_info->delimiter);
      else
        my_b_printf(&cache, ":=_%s %s COLLATE `%s`%s\n",
                    cs->csname, hex_str, cs->name,
                    print_event_info->delimiter);
      my_free(hex_str);
    }
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0);
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
  Item *it= 0;
  CHARSET_INFO *charset;
  DBUG_ENTER("User_var_log_event::do_apply_event");
  query_id_t sav_query_id= 0; /* memorize orig id when deferred applying */

  if (rli->deferred_events_collecting)
  {
    set_deferred(current_thd->query_id);
    DBUG_RETURN(rli->deferred_events->add(this));
  }
  else if (is_deferred())
  {
    sav_query_id= current_thd->query_id;
    current_thd->query_id= query_id; /* recreating original time context */
  }

  if (!(charset= get_charset(charset_number, MYF(MY_WME))))
    DBUG_RETURN(1);
  LEX_STRING user_var_name;
  user_var_name.str= name;
  user_var_name.length= name_len;
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
      float8get(real_val, val);
      it= new Item_float(real_val, 0);
      val= (char*) &real_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case INT_RESULT:
      int_val= (longlong) uint8korr(val);
      it= new Item_int(int_val);
      val= (char*) &int_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case DECIMAL_RESULT:
    {
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
      DBUG_ASSERT(0);
      DBUG_RETURN(0);
    }
  }

  Item_func_set_user_var *e= new Item_func_set_user_var(user_var_name, it);
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
  e->update_hash(val, val_len, type, charset, DERIVATION_IMPLICIT,
                 (flags & User_var_log_event::UNSIGNED_F));
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
  Slave_log_event methods
**************************************************************************/

#ifdef HAVE_REPLICATION
#ifdef MYSQL_CLIENT
void Unknown_log_event::print(FILE* file_arg, PRINT_EVENT_INFO* print_event_info)
{
  Write_on_release_cache cache(&print_event_info->head_cache, file_arg);

  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n# %s", "Unknown event\n");
}
#endif  

#ifndef MYSQL_CLIENT
void Slave_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[256+HOSTNAME_LENGTH], *pos;
  pos= strmov(buf, "host=");
  pos= strnmov(pos, master_host, HOSTNAME_LENGTH);
  pos= strmov(pos, ",port=");
  pos= int10_to_str((long) master_port, pos, 10);
  pos= strmov(pos, ",log=");
  pos= strmov(pos, master_log);
  pos= strmov(pos, ",pos=");
  pos= longlong10_to_str(master_pos, pos, 10);
  protocol->store(buf, pos-buf, &my_charset_bin);
}
#endif /* !MYSQL_CLIENT */


#ifndef MYSQL_CLIENT
/**
  @todo
  re-write this better without holding both locks at the same time
*/
Slave_log_event::Slave_log_event(THD* thd_arg,
				 Relay_log_info* rli)
  :Log_event(thd_arg, 0, 0) , mem_pool(0), master_host(0)
{
  DBUG_ENTER("Slave_log_event");
  if (!rli->inited)				// QQ When can this happen ?
    DBUG_VOID_RETURN;

  Master_info* mi = rli->mi;
  // TODO: re-write this better without holding both locks at the same time
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&rli->data_lock);
  master_host_len = strlen(mi->host);
  master_log_len = strlen(rli->group_master_log_name);
  // on OOM, just do not initialize the structure and print the error
  if ((mem_pool = (char*)my_malloc(get_data_size() + 1,
                                   MYF(MY_WME))))
  {
    master_host = mem_pool + SL_MASTER_HOST_OFFSET ;
    memcpy(master_host, mi->host, master_host_len + 1);
    master_log = master_host + master_host_len + 1;
    memcpy(master_log, rli->group_master_log_name, master_log_len + 1);
    master_port = mi->port;
    master_pos = rli->group_master_log_pos;
    DBUG_PRINT("info", ("master_log: %s  pos: %lu", master_log,
                        (ulong) master_pos));
  }
  else
    sql_print_error("Out of memory while recording slave event");
  mysql_mutex_unlock(&rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  DBUG_VOID_RETURN;
}
#endif /* !MYSQL_CLIENT */


Slave_log_event::~Slave_log_event()
{
  my_free(mem_pool);
}


#ifdef MYSQL_CLIENT
void Slave_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  char llbuff[22];
  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n\
Slave: master_host: '%s'  master_port: %d  master_log: '%s'  master_pos: %s\n",
	  master_host, master_port, master_log, llstr(master_pos, llbuff));
}
#endif /* MYSQL_CLIENT */


int Slave_log_event::get_data_size()
{
  return master_host_len + master_log_len + 1 + SL_MASTER_HOST_OFFSET;
}


#ifndef MYSQL_CLIENT
bool Slave_log_event::write(IO_CACHE* file)
{
  ulong event_length= get_data_size();
  int8store(mem_pool + SL_MASTER_POS_OFFSET, master_pos);
  int2store(mem_pool + SL_MASTER_PORT_OFFSET, master_port);
  // log and host are already there

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (uchar*) mem_pool, event_length));
}
#endif


void Slave_log_event::init_from_mem_pool(int data_size)
{
  master_pos = uint8korr(mem_pool + SL_MASTER_POS_OFFSET);
  master_port = uint2korr(mem_pool + SL_MASTER_PORT_OFFSET);
  master_host = mem_pool + SL_MASTER_HOST_OFFSET;
  master_host_len = (uint) strlen(master_host);
  // safety
  master_log = master_host + master_host_len + 1;
  if (master_log > mem_pool + data_size)
  {
    master_host = 0;
    return;
  }
  master_log_len = (uint) strlen(master_log);
}


/** This code is not used, so has not been updated to be format-tolerant. */
/* We are using description_event so that slave does not crash on Log_event
  constructor */
Slave_log_event::Slave_log_event(const char* buf, 
                                 uint event_len,
                                 const Format_description_log_event* description_event)
  :Log_event(buf,description_event),mem_pool(0),master_host(0)
{
  if (event_len < LOG_EVENT_HEADER_LEN)
    return;
  event_len -= LOG_EVENT_HEADER_LEN;
  if (!(mem_pool = (char*) my_malloc(event_len + 1, MYF(MY_WME))))
    return;
  memcpy(mem_pool, buf + LOG_EVENT_HEADER_LEN, event_len);
  mem_pool[event_len] = 0;
  init_from_mem_pool(event_len);
}


#ifndef MYSQL_CLIENT
int Slave_log_event::do_apply_event(Relay_log_info const *rli)
{
  if (mysql_bin_log.is_open())
    return mysql_bin_log.write(this);
  return 0;
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
	Stop_log_event methods
**************************************************************************/

/*
  Stop_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Stop_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  Write_on_release_cache cache(&print_event_info->head_cache, file,
                               Write_on_release_cache::FLUSH_F);

  if (print_event_info->short_form)
    return;

  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\tStop\n");
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
*/
int Stop_log_event::do_update_pos(Relay_log_info *rli)
{
  /*
    We do not want to update master_log pos because we get a rotate event
    before stop, so by now group_master_log_name is set to the next log.
    If we updated it, we will have incorrect master coordinates and this
    could give false triggers in MASTER_POS_WAIT() that we have reached
    the target position when in fact we have not.
  */
  if (thd->variables.option_bits & OPTION_BEGIN)
    rli->inc_event_relay_log_pos();
  else
  {
    rli->inc_group_relay_log_pos(0);
    flush_relay_log_info(rli);
  }
  return 0;
}

#endif /* !MYSQL_CLIENT */
#endif /* HAVE_REPLICATION */


/**************************************************************************
	Create_file_log_event methods
**************************************************************************/

/*
  Create_file_log_event ctor
*/

#ifndef MYSQL_CLIENT
Create_file_log_event::
Create_file_log_event(THD* thd_arg, sql_exchange* ex,
		      const char* db_arg, const char* table_name_arg,
                      List<Item>& fields_arg,
                      bool is_concurrent_arg,
                      enum enum_duplicates handle_dup,
                      bool ignore,
		      uchar* block_arg, uint block_len_arg, bool using_trans)
  :Load_log_event(thd_arg, ex, db_arg, table_name_arg, fields_arg,
                  is_concurrent_arg,
                  handle_dup, ignore, using_trans),
   fake_base(0), block(block_arg), event_buf(0), block_len(block_len_arg),
   file_id(thd_arg->file_id = mysql_bin_log.next_file_id())
{
  DBUG_ENTER("Create_file_log_event");
  sql_ex.force_new_format();
  DBUG_VOID_RETURN;
}


/*
  Create_file_log_event::write_data_body()
*/

bool Create_file_log_event::write_data_body(IO_CACHE* file)
{
  bool res;
  if ((res= Load_log_event::write_data_body(file)) || fake_base)
    return res;
  return (my_b_safe_write(file, (uchar*) "", 1) ||
          my_b_safe_write(file, (uchar*) block, block_len));
}


/*
  Create_file_log_event::write_data_header()
*/

bool Create_file_log_event::write_data_header(IO_CACHE* file)
{
  bool res;
  uchar buf[CREATE_FILE_HEADER_LEN];
  if ((res= Load_log_event::write_data_header(file)) || fake_base)
    return res;
  int4store(buf + CF_FILE_ID_OFFSET, file_id);
  return my_b_safe_write(file, buf, CREATE_FILE_HEADER_LEN) != 0;
}


/*
  Create_file_log_event::write_base()
*/

bool Create_file_log_event::write_base(IO_CACHE* file)
{
  bool res;
  fake_base= 1;                                 // pretend we are Load event
  res= write(file);
  fake_base= 0;
  return res;
}

#endif /* !MYSQL_CLIENT */

/*
  Create_file_log_event ctor
*/

Create_file_log_event::Create_file_log_event(const char* buf, uint len,
                                             const Format_description_log_event* description_event)
  :Load_log_event(buf,0,description_event),fake_base(0),block(0),inited_from_old(0)
{
  DBUG_ENTER("Create_file_log_event::Create_file_log_event(char*,...)");
  uint block_offset;
  uint header_len= description_event->common_header_len;
  uint8 load_header_len= description_event->post_header_len[LOAD_EVENT-1];
  uint8 create_file_header_len= description_event->post_header_len[CREATE_FILE_EVENT-1];
  if (!(event_buf= (char*) my_memdup(buf, len, MYF(MY_WME))) ||
      copy_log_event(event_buf,len,
                     (((uchar)buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len+load_header_len) :
                       (header_len+load_header_len) +
                       create_file_header_len)),
                     description_event))
    DBUG_VOID_RETURN;
  if (description_event->binlog_version!=1)
  {
    file_id= uint4korr(buf + 
                       header_len +
		       load_header_len + CF_FILE_ID_OFFSET);
    /*
      Note that it's ok to use get_data_size() below, because it is computed
      with values we have already read from this event (because we called
      copy_log_event()); we are not using slave's format info to decode
      master's format, we are really using master's format info.
      Anyway, both formats should be identical (except the common_header_len)
      as these Load events are not changed between 4.0 and 5.0 (as logging of
      LOAD DATA INFILE does not use Load_log_event in 5.0).

      The + 1 is for \0 terminating fname  
    */
    block_offset= (description_event->common_header_len +
                   Load_log_event::get_data_size() +
                   create_file_header_len + 1);
    if (len < block_offset)
      DBUG_VOID_RETURN;
    block = (uchar*)buf + block_offset;
    block_len = len - block_offset;
  }
  else
  {
    sql_ex.force_new_format();
    inited_from_old = 1;
  }
  DBUG_VOID_RETURN;
}


/*
  Create_file_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Create_file_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info,
				  bool enable_local)
{
  Write_on_release_cache cache(&print_event_info->head_cache, file);

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
                     {(&cache)->write_pos= (&cache)->write_end;
                     DBUG_SET("+d,simulate_file_write_error");});
    /*
      That one is for "file_id: etc" below: in mysqlbinlog we want the #, in
      SHOW BINLOG EVENTS we don't.
     */
    my_b_printf(&cache, "#");
  }

  my_b_printf(&cache, " file_id: %d  block_len: %d\n", file_id, block_len);
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
void Create_file_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[SAFE_NAME_LEN*2 + 30 + 21*2], *pos;
  pos= strmov(buf, "db=");
  memcpy(pos, db, db_len);
  pos= strmov(pos + db_len, ";table=");
  memcpy(pos, table_name, table_name_len);
  pos= strmov(pos + table_name_len, ";file_id=");
  pos= int10_to_str((long) file_id, pos, 10);
  pos= strmov(pos, ";block_len=");
  pos= int10_to_str((long) block_len, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
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
  char proc_info[17+FN_REFLEN+10], *fname_buf;
  char *ext;
  int fd = -1;
  IO_CACHE file;
  int error = 1;

  bzero((char*)&file, sizeof(file));
  fname_buf= strmov(proc_info, "Making temp file ");
  ext= slave_load_file_stem(fname_buf, file_id, server_id, ".info");
  thd_proc_info(thd, proc_info);
  /* old copy may exist already */
  mysql_file_delete(key_file_log_event_info, fname_buf, MYF(0));
  if ((fd= mysql_file_create(key_file_log_event_info,
                             fname_buf, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, WRITE_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in Create_file event: could not open file '%s'",
                fname_buf);
    goto err;
  }
  
  // a trick to avoid allocating another buffer
  fname= fname_buf;
  fname_len= (uint) (strmov(ext, ".data") - fname);
  if (write_base(&file))
  {
    strmov(ext, ".info"); // to have it right in the error message
    rli->report(ERROR_LEVEL, my_errno,
                "Error in Create_file event: could not write to file '%s'",
                fname_buf);
    goto err;
  }
  end_io_cache(&file);
  mysql_file_close(fd, MYF(0));
  
  // fname_buf now already has .data, not .info, because we did our trick
  /* old copy may exist already */
  mysql_file_delete(key_file_log_event_data, fname_buf, MYF(0));
  if ((fd= mysql_file_create(key_file_log_event_data,
                             fname_buf, CREATE_MODE,
                             O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                             MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in Create_file event: could not open file '%s'",
                fname_buf);
    goto err;
  }
  if (mysql_file_write(fd, (uchar*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in Create_file event: write to '%s' failed",
                fname_buf);
    goto err;
  }
  error=0;					// Everything is ok

err:
  if (error)
    end_io_cache(&file);
  if (fd >= 0)
    mysql_file_close(fd, MYF(0));
  thd_proc_info(thd, 0);
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
  :Log_event(thd_arg,0, using_trans), block(block_arg),
   block_len(block_len_arg), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif


/*
  Append_block_log_event ctor
*/

Append_block_log_event::Append_block_log_event(const char* buf, uint len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event),block(0)
{
  DBUG_ENTER("Append_block_log_event::Append_block_log_event(char*,...)");
  uint8 common_header_len= description_event->common_header_len; 
  uint8 append_block_header_len=
    description_event->post_header_len[APPEND_BLOCK_EVENT-1];
  uint total_header_len= common_header_len+append_block_header_len;
  if (len < total_header_len)
    DBUG_VOID_RETURN;
  file_id= uint4korr(buf + common_header_len + AB_FILE_ID_OFFSET);
  block= (uchar*)buf + total_header_len;
  block_len= len - total_header_len;
  DBUG_VOID_RETURN;
}


/*
  Append_block_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Append_block_log_event::write(IO_CACHE* file)
{
  uchar buf[APPEND_BLOCK_HEADER_LEN];
  int4store(buf + AB_FILE_ID_OFFSET, file_id);
  return (write_header(file, APPEND_BLOCK_HEADER_LEN + block_len) ||
          wrapper_my_b_safe_write(file, buf, APPEND_BLOCK_HEADER_LEN) ||
	  wrapper_my_b_safe_write(file, (uchar*) block, block_len) ||
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
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n#%s: file_id: %d  block_len: %d\n",
              get_type_str(), file_id, block_len);
}
#endif /* MYSQL_CLIENT */


/*
  Append_block_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Append_block_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[256];
  uint length;
  length= (uint) sprintf(buf, ";file_id=%u;block_len=%u", file_id, block_len);
  protocol->store(buf, length, &my_charset_bin);
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
  char proc_info[17+FN_REFLEN+10], *fname= proc_info+17;
  int fd;
  int error = 1;
  DBUG_ENTER("Append_block_log_event::do_apply_event");

  fname= strmov(proc_info, "Making temp file ");
  slave_load_file_stem(fname, file_id, server_id, ".data");
  thd_proc_info(thd, proc_info);
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
    if ((fd= mysql_file_create(key_file_log_event_data,
                               fname, CREATE_MODE,
                               O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                               MYF(MY_WME))) < 0)
    {
      rli->report(ERROR_LEVEL, my_errno,
                  "Error in %s event: could not create file '%s'",
                  get_type_str(), fname);
      goto err;
    }
  }
  else if ((fd= mysql_file_open(key_file_log_event_data,
                                fname,
                                O_WRONLY | O_APPEND | O_BINARY | O_NOFOLLOW,
                                MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in %s event: could not open file '%s'",
                get_type_str(), fname);
    goto err;
  }

  DBUG_EXECUTE_IF("remove_slave_load_file_before_write",
                  {
                    my_delete(fname, MYF(0));
                  });

  if (mysql_file_write(fd, (uchar*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in %s event: write to '%s' failed",
                get_type_str(), fname);
    goto err;
  }
  error=0;

err:
  if (fd >= 0)
    mysql_file_close(fd, MYF(0));
  thd_proc_info(thd, 0);
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
  :Log_event(thd_arg, 0, using_trans), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif

/*
  Delete_file_log_event ctor
*/

Delete_file_log_event::Delete_file_log_event(const char* buf, uint len,
                                             const Format_description_log_event* description_event)
  :Log_event(buf, description_event),file_id(0)
{
  uint8 common_header_len= description_event->common_header_len;
  uint8 delete_file_header_len= description_event->post_header_len[DELETE_FILE_EVENT-1];
  if (len < (uint)(common_header_len + delete_file_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + DF_FILE_ID_OFFSET);
}


/*
  Delete_file_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Delete_file_log_event::write(IO_CACHE* file)
{
 uchar buf[DELETE_FILE_HEADER_LEN];
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
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n#Delete_file: file_id=%u\n", file_id);
}
#endif /* MYSQL_CLIENT */

/*
  Delete_file_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Delete_file_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[64];
  uint length;
  length= (uint) sprintf(buf, ";file_id=%u", (uint) file_id);
  protocol->store(buf, (int32) length, &my_charset_bin);
}
#endif

/*
  Delete_file_log_event::do_apply_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Delete_file_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+10];
  char *ext= slave_load_file_stem(fname, file_id, server_id, ".data");
  mysql_file_delete(key_file_log_event_data, fname, MYF(MY_WME));
  strmov(ext, ".info");
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
  :Log_event(thd_arg, 0, using_trans), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif
  

/*
  Execute_load_log_event ctor
*/

Execute_load_log_event::Execute_load_log_event(const char* buf, uint len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event), file_id(0)
{
  uint8 common_header_len= description_event->common_header_len;
  uint8 exec_load_header_len= description_event->post_header_len[EXEC_LOAD_EVENT-1];
  if (len < (uint)(common_header_len+exec_load_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + EL_FILE_ID_OFFSET);
}


/*
  Execute_load_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Execute_load_log_event::write(IO_CACHE* file)
{
  uchar buf[EXEC_LOAD_HEADER_LEN];
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
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  if (print_event_info->short_form)
    return;
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n#Exec_load: file_id=%d\n",
              file_id);
}
#endif

/*
  Execute_load_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Execute_load_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[64];
  uint length;
  length= (uint) sprintf(buf, ";file_id=%u", (uint) file_id);
  protocol->store(buf, (int32) length, &my_charset_bin);
}


/*
  Execute_load_log_event::do_apply_event()
*/

int Execute_load_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+10];
  char *ext;
  int fd;
  int error= 1;
  IO_CACHE file;
  Load_log_event *lev= 0;

  ext= slave_load_file_stem(fname, file_id, server_id, ".info");
  if ((fd= mysql_file_open(key_file_log_event_info,
                           fname, O_RDONLY | O_BINARY | O_NOFOLLOW,
                           MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, READ_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                "Error in Exec_load event: could not open file '%s'",
                fname);
    goto err;
  }
  if (!(lev= (Load_log_event*)
        Log_event::read_log_event(&file,
                                  (mysql_mutex_t*)0,
                                  rli->relay_log.description_event_for_exec,
                                  opt_slave_sql_verify_checksum)) ||
      lev->get_type_code() != NEW_LOAD_EVENT)
  {
    rli->report(ERROR_LEVEL, 0, "Error in Exec_load event: "
                    "file '%s' appears corrupted", fname);
    goto err;
  }
  lev->thd = thd;
  /*
    lev->do_apply_event should use rli only for errors i.e. should
    not advance rli's position.

    lev->do_apply_event is the place where the table is loaded (it
    calls mysql_load()).
  */

  const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;
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
    char *tmp= my_strdup(rli->last_error().message, MYF(MY_WME));
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
  mysql_file_delete(key_file_log_event_info, fname, MYF(MY_WME));
  memcpy(ext, ".data", 6);
  mysql_file_delete(key_file_log_event_data, fname, MYF(MY_WME));
  error = 0;

err:
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
  :Append_block_log_event(thd_arg, db_arg, block_arg, block_len_arg,
                          using_trans)
{
   file_id= thd_arg->file_id= mysql_bin_log.next_file_id();
}
#endif


Begin_load_query_log_event::
Begin_load_query_log_event(const char* buf, uint len,
                           const Format_description_log_event* desc_event)
  :Append_block_log_event(buf, len, desc_event)
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
                             enum_load_dup_handling dup_handling_arg,
                             bool using_trans, bool direct, bool suppress_use,
                             int errcode):
  Query_log_event(thd_arg, query_arg, query_length_arg, using_trans, direct,
                  suppress_use, errcode),
  file_id(thd_arg->file_id), fn_pos_start(fn_pos_start_arg),
  fn_pos_end(fn_pos_end_arg), dup_handling(dup_handling_arg)
{
}
#endif /* !MYSQL_CLIENT */


Execute_load_query_log_event::
Execute_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_log_event* desc_event):
  Query_log_event(buf, event_len, desc_event, EXECUTE_LOAD_QUERY_EVENT),
  file_id(0), fn_pos_start(0), fn_pos_end(0)
{
  if (!Query_log_event::is_valid())
    return;

  buf+= desc_event->common_header_len;

  fn_pos_start= uint4korr(buf + ELQ_FN_POS_START_OFFSET);
  fn_pos_end= uint4korr(buf + ELQ_FN_POS_END_OFFSET);
  dup_handling= (enum_load_dup_handling)(*(buf + ELQ_DUP_HANDLING_OFFSET));

  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    return;

  file_id= uint4korr(buf + ELQ_FILE_ID_OFFSET);
}


ulong Execute_load_query_log_event::get_post_header_size_for_derived()
{
  return EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN;
}


#ifndef MYSQL_CLIENT
bool
Execute_load_query_log_event::write_post_header_for_derived(IO_CACHE* file)
{
  uchar buf[EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN];
  int4store(buf, file_id);
  int4store(buf + 4, fn_pos_start);
  int4store(buf + 4 + 4, fn_pos_end);
  *(buf + 4 + 4 + 4)= (uchar) dup_handling;
  return wrapper_my_b_safe_write(file, buf, EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN);
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
  Write_on_release_cache cache(&print_event_info->head_cache, file);

  print_query_header(&cache, print_event_info);
  /**
    reduce the size of io cache so that the write function is called
    for every call to my_b_printf().
   */
  DBUG_EXECUTE_IF ("simulate_execute_event_write_error",
                   {(&cache)->write_pos= (&cache)->write_end;
                   DBUG_SET("+d,simulate_file_write_error");});

  if (local_fname)
  {
    my_b_write(&cache, (uchar*) query, fn_pos_start);
    my_b_printf(&cache, " LOCAL INFILE ");
    pretty_print_str(&cache, local_fname, strlen(local_fname));

    if (dup_handling == LOAD_DUP_REPLACE)
      my_b_printf(&cache, " REPLACE");
    my_b_printf(&cache, " INTO");
    my_b_write(&cache, (uchar*) query + fn_pos_end, q_len-fn_pos_end);
    my_b_printf(&cache, "\n%s\n", print_event_info->delimiter);
  }
  else
  {
    my_b_write(&cache, (uchar*) query, q_len);
    my_b_printf(&cache, "\n%s\n", print_event_info->delimiter);
  }

  if (!print_event_info->short_form)
    my_b_printf(&cache, "# file_id: %d \n", file_id);
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Execute_load_query_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf_mem[1024];
  String buf(buf_mem, sizeof(buf_mem), system_charset_info);
  buf.real_alloc(9 + db_len + q_len + 10 + 21);
  if (db && db_len)
  {
    if (buf.append(STRING_WITH_LEN("use ")) ||
        append_identifier(thd, &buf, db, db_len) ||
        buf.append(STRING_WITH_LEN("; ")))
      return;
  }
  if (query && q_len && buf.append(query, q_len))
    return;
  if (buf.append(" ;file_id=") ||
      buf.append_ulonglong(file_id))
    return;
  protocol->store(buf.ptr(), buf.length(), &my_charset_bin);
}


int
Execute_load_query_log_event::do_apply_event(Relay_log_info const *rli)
{
  char *p;
  char *buf;
  char *fname;
  char *fname_end;
  int error;

  buf= (char*) my_malloc(q_len + 1 - (fn_pos_end - fn_pos_start) +
                         (FN_REFLEN + 10) + 10 + 8 + 5, MYF(MY_WME));

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
  case LOAD_DUP_IGNORE:
    p= strmake(p, STRING_WITH_LEN(" IGNORE"));
    break;
  case LOAD_DUP_REPLACE:
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
  if (new_format())
  {
    return (write_str(file, field_term, (uint) field_term_len) ||
	    write_str(file, enclosed,   (uint) enclosed_len) ||
	    write_str(file, line_term,  (uint) line_term_len) ||
	    write_str(file, line_start, (uint) line_start_len) ||
	    write_str(file, escaped,    (uint) escaped_len) ||
	    my_b_safe_write(file,(uchar*) &opt_flags,1));
  }
  else
  {
    /**
      @todo This is sensitive to field padding. We should write a
      char[7], not an old_sql_ex. /sven
    */
    old_sql_ex old_ex;
    old_ex.field_term= *field_term;
    old_ex.enclosed=   *enclosed;
    old_ex.line_term=  *line_term;
    old_ex.line_start= *line_start;
    old_ex.escaped=    *escaped;
    old_ex.opt_flags=  opt_flags;
    old_ex.empty_flags=empty_flags;
    return my_b_safe_write(file, (uchar*) &old_ex, sizeof(old_ex)) != 0;
  }
}


/*
  sql_ex_info::init()
*/

const char *sql_ex_info::init(const char *buf, const char *buf_end,
                              bool use_new_format)
{
  cached_new_format = use_new_format;
  if (use_new_format)
  {
    empty_flags=0;
    /*
      The code below assumes that buf will not disappear from
      under our feet during the lifetime of the event. This assumption
      holds true in the slave thread if the log is in new format, but is not
      the case when we have old format because we will be reusing net buffer
      to read the actual file before we write out the Create_file event.
    */
    if (read_str(&buf, buf_end, &field_term, &field_term_len) ||
        read_str(&buf, buf_end, &enclosed,   &enclosed_len) ||
        read_str(&buf, buf_end, &line_term,  &line_term_len) ||
        read_str(&buf, buf_end, &line_start, &line_start_len) ||
        read_str(&buf, buf_end, &escaped,    &escaped_len))
      return 0;
    opt_flags = *buf++;
  }
  else
  {
    field_term_len= enclosed_len= line_term_len= line_start_len= escaped_len=1;
    field_term = buf++;			// Use first byte in string
    enclosed=	 buf++;
    line_term=   buf++;
    line_start=  buf++;
    escaped=     buf++;
    opt_flags =  *buf++;
    empty_flags= *buf++;
    if (empty_flags & FIELD_TERM_EMPTY)
      field_term_len=0;
    if (empty_flags & ENCLOSED_EMPTY)
      enclosed_len=0;
    if (empty_flags & LINE_TERM_EMPTY)
      line_term_len=0;
    if (empty_flags & LINE_START_EMPTY)
      line_start_len=0;
    if (empty_flags & ESCAPED_EMPTY)
      escaped_len=0;
  }
  return buf;
}


/**************************************************************************
	Rows_log_event member functions
**************************************************************************/

#ifndef MYSQL_CLIENT
Rows_log_event::Rows_log_event(THD *thd_arg, TABLE *tbl_arg, ulong tid,
                               MY_BITMAP const *cols, bool is_transactional)
  : Log_event(thd_arg, 0, is_transactional),
    m_row_count(0),
    m_table(tbl_arg),
    m_table_id(tid),
    m_width(tbl_arg ? tbl_arg->s->fields : 1),
    m_rows_buf(0), m_rows_cur(0), m_rows_end(0), m_flags(0) 
#ifdef HAVE_REPLICATION
    , m_curr_row(NULL), m_curr_row_end(NULL),
    m_key(NULL), m_key_info(NULL), m_key_nr(0)
#endif
{
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

Rows_log_event::Rows_log_event(const char *buf, uint event_len,
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
    , m_curr_row(NULL), m_curr_row_end(NULL),
    m_key(NULL), m_key_info(NULL), m_key_nr(0)
#endif
{
  DBUG_ENTER("Rows_log_event::Rows_log_event(const char*,...)");
  uint8 const common_header_len= description_event->common_header_len;
  uint8 const post_header_len= description_event->post_header_len[event_type-1];

  DBUG_PRINT("enter",("event_len: %u  common_header_len: %d  "
		      "post_header_len: %d",
		      event_len, common_header_len,
		      post_header_len));

  const char *post_start= buf + common_header_len;
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

  m_cols_ai.bitmap= m_cols.bitmap; /* See explanation in is_valid() */

  if (event_type == UPDATE_ROWS_EVENT)
  {
    DBUG_PRINT("debug", ("Reading from %p", ptr_after_width));

    /* if bitmap_init fails, caught in is_valid() */
    if (likely(!bitmap_init(&m_cols_ai,
                            m_width <= sizeof(m_bitbuf_ai)*8 ? m_bitbuf_ai : NULL,
                            m_width,
                            false)))
    {
      DBUG_PRINT("debug", ("Reading from %p", ptr_after_width));
      memcpy(m_cols_ai.bitmap, ptr_after_width, (m_width + 7) / 8);
      create_last_word_mask(&m_cols_ai);
      ptr_after_width+= (m_width + 7) / 8;
      DBUG_DUMP("m_cols_ai", (uchar*) m_cols_ai.bitmap,
                no_bytes_in_map(&m_cols_ai));
    }
    else
    {
      // Needed because bitmap_init() does not set it to null on failure
      m_cols_ai.bitmap= 0;
      DBUG_VOID_RETURN;
    }
  }

  const uchar* const ptr_rows_data= (const uchar*) ptr_after_width;

  size_t const data_size= event_len - (ptr_rows_data - (const uchar *) buf);
  DBUG_PRINT("info",("m_table_id: %lu  m_flags: %d  m_width: %lu  data_size: %lu",
                     m_table_id, m_flags, m_width, (ulong) data_size));

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

Rows_log_event::~Rows_log_event()
{
  if (m_cols.bitmap == m_bitbuf) // no my_malloc happened
    m_cols.bitmap= 0; // so no my_free in bitmap_free
  bitmap_free(&m_cols); // To pair with bitmap_init().
  my_free(m_rows_buf);
}

int Rows_log_event::get_data_size()
{
  int const type_code= get_type_code();

  uchar buf[sizeof(m_width) + 1];
  uchar *end= net_store_length(buf, m_width);

  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  return 6 + no_bytes_in_map(&m_cols) + (end - buf) +
                  (type_code == UPDATE_ROWS_EVENT ? no_bytes_in_map(&m_cols_ai) : 0) +
                  (m_rows_cur - m_rows_buf););
  int data_size= ROWS_HEADER_LEN;
  data_size+= no_bytes_in_map(&m_cols);
  data_size+= (uint) (end - buf);

  if (type_code == UPDATE_ROWS_EVENT)
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
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_valgrind
  DBUG_DUMP("row_data", row_data, min(length, 32));
#endif

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
    ulong const new_alloc= 
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
int Rows_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Rows_log_event::do_apply_event(Relay_log_info*)");
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
      Lock_tables() reads the contents of thd->lex, so they must be
      initialized.

      We also call the mysql_reset_thd_for_next_command(), since this
      is the logical start of the next "statement". Note that this
      call might reset the value of current_stmt_binlog_format, so
      we need to do any changes to that value after this function.
    */
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);
    /*
      The current statement is just about to begin and 
      has not yet modified anything. Note, all.modified is reset
      by mysql_reset_thd_for_next_command.
    */
    thd->transaction.stmt.modified_non_trans_table= FALSE;
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
    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(thd->variables.option_bits) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    if (open_and_lock_tables(thd, rli->tables_to_lock, FALSE, 0))
    {
      uint actual_error= thd->stmt_da->sql_errno();
      if (thd->is_slave_error || thd->is_fatal_error)
      {
        /*
          Error reporting borrowed from Query_log_event with many excessive
          simplifications. 
          We should not honour --slave-skip-errors at this point as we are
          having severe errors which should not be skiped.
        */
        rli->report(ERROR_LEVEL, actual_error,
                    "Error executing row event: '%s'",
                    (actual_error ? thd->stmt_da->message() :
                     "unexpected success or fatal error"));
        thd->is_slave_error= 1;
      }
      const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
      DBUG_RETURN(actual_error);
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
          /*
            We should not honour --slave-skip-errors at this point as we are
            having severe errors which should not be skiped.
          */
          thd->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->slave_close_thread_tables(thd);
          DBUG_RETURN(ERR_BAD_TABLE_DEF);
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

#ifdef HAVE_QUERY_CACHE
    query_cache.invalidate_locked_for_write(thd, rli->tables_to_lock);
#endif
  }

  TABLE* 
    table= 
    m_table= const_cast<Relay_log_info*>(rli)->m_table_map.get_table(m_table_id);

  DBUG_PRINT("debug", ("m_table: 0x%lx, m_table_id: %lu", (ulong) m_table, m_table_id));

  if (table)
  {
    bool transactional_table= table->file->has_transactions();
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
    thd->set_time(when, when_sec_part);

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

    this->slave_exec_mode= slave_exec_mode_options; // fix the mode

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
    ulonglong saved_sql_mode= thd->variables.sql_mode;
    if (!is_auto_inc_in_extra_columns())
      thd->variables.sql_mode= MODE_NO_AUTO_VALUE_ON_ZERO;

    // row processing loop

    /* 
      set the initial time of this ROWS statement if it was not done
      before in some other ROWS event. 
     */
    const_cast<Relay_log_info*>(rli)->set_row_stmt_start_timestamp();

    while (error == 0 && m_curr_row < m_rows_end)
    {
      /* in_use can have been set to NULL in close_tables_for_reopen */
      THD* old_thd= table->in_use;
      if (!table->in_use)
        table->in_use= thd;

      error= do_exec_row(rli);

      if (error)
        DBUG_PRINT("info", ("error: %s", HA_ERR(error)));
      DBUG_ASSERT(error != HA_ERR_RECORD_DELETED);

      table->in_use = old_thd;

      if (error)
      {
        int actual_error= convert_handler_error(error, thd, table);
        bool idempotent_error= (idempotent_error_code(error) &&
                               (slave_exec_mode == SLAVE_EXEC_MODE_IDEMPOTENT));
        bool ignored_error= (idempotent_error == 0 ?
                             ignored_error_code(actual_error) : 0);

        if (idempotent_error || ignored_error)
        {
          if (global_system_variables.log_warnings)
            slave_rows_error_report(WARNING_LEVEL, error, rli, thd, table,
                                    get_type_str(),
                                    RPL_LOG_NAME, (ulong) log_pos);
          clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
          error= 0;
          if (idempotent_error == 0)
            break;
        }
      }

      /*
       If m_curr_row_end  was not set during event execution (e.g., because
       of errors) we can't proceed to the next row. If the error is transient
       (i.e., error==0 at this point) we must call unpack_current_row() to set 
       m_curr_row_end.
      */ 
   
      DBUG_PRINT("info", ("curr_row: 0x%lu; curr_row_end: 0x%lu; rows_end: 0x%lu",
                          (ulong) m_curr_row, (ulong) m_curr_row_end, (ulong) m_rows_end));

      if (!m_curr_row_end && !error)
        error= unpack_current_row(rli);
  
      // at this moment m_curr_row_end should be set
      DBUG_ASSERT(error || m_curr_row_end != NULL); 
      DBUG_ASSERT(error || m_curr_row < m_curr_row_end);
      DBUG_ASSERT(error || m_curr_row_end <= m_rows_end);
  
      m_curr_row= m_curr_row_end;
 
      if (error == 0 && !transactional_table)
        thd->transaction.all.modified_non_trans_table=
          thd->transaction.stmt.modified_non_trans_table= TRUE;
    } // row processing loop

    /*
      Restore the sql_mode after the rows event is processed.
    */
    thd->variables.sql_mode= saved_sql_mode;

    {/**
         The following failure injecion works in cooperation with tests 
         setting @@global.debug= 'd,stop_slave_middle_group'.
         The sql thread receives the killed status and will proceed 
         to shutdown trying to finish incomplete events group.
     */
      DBUG_EXECUTE_IF("stop_slave_middle_group",
                      if (thd->transaction.all.modified_non_trans_table)
                        const_cast<Relay_log_info*>(rli)->abort_slave= 1;);
    }

    if ((error= do_after_row_operations(rli, error)) &&
        ignored_error_code(convert_handler_error(error, thd, table)))
    {

      if (global_system_variables.log_warnings)
        slave_rows_error_report(WARNING_LEVEL, error, rli, thd, table,
                                get_type_str(),
                                RPL_LOG_NAME, (ulong) log_pos);
      clear_all_errors(thd, const_cast<Relay_log_info*>(rli));
      error= 0;
    }
  } // if (table)

  
  if (error)
  {
    slave_rows_error_report(ERROR_LEVEL, error, rli, thd, table,
                             get_type_str(),
                             RPL_LOG_NAME, (ulong) log_pos);
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

  if (get_flags(STMT_END_F) && (error= rows_event_stmt_cleanup(rli, thd)))
    slave_rows_error_report(ERROR_LEVEL,
                            thd->is_error() ? 0 : error,
                            rli, thd, table,
                            get_type_str(),
                            RPL_LOG_NAME, (ulong) log_pos);
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
bool Rows_log_event::write_data_header(IO_CACHE *file)
{
  uchar buf[ROWS_HEADER_LEN];	// No need to init the buffer
  DBUG_ASSERT(m_table_id != ~0UL);
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, m_table_id);
                    int2store(buf + 4, m_flags);
                    return (wrapper_my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + RW_MAPID_OFFSET, (ulonglong)m_table_id);
  int2store(buf + RW_FLAGS_OFFSET, m_flags);
  return (wrapper_my_b_safe_write(file, buf, ROWS_HEADER_LEN));
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
  if (get_type_code() == UPDATE_ROWS_EVENT)
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
void Rows_log_event::pack_info(THD *thd, Protocol *protocol)
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

/**************************************************************************
	Annotate_rows_log_event member functions
**************************************************************************/

#ifndef MYSQL_CLIENT
Annotate_rows_log_event::Annotate_rows_log_event(THD *thd,
                                                 bool using_trans,
                                                 bool direct)
  : Log_event(thd, 0, using_trans),
    m_save_thd_query_txt(0),
    m_save_thd_query_len(0)
{
  m_query_txt= thd->query();
  m_query_len= thd->query_length();
  if (direct)
    cache_type= Log_event::EVENT_NO_CACHE;
}
#endif

Annotate_rows_log_event::Annotate_rows_log_event(const char *buf,
                                                 uint event_len,
                                      const Format_description_log_event *desc)
  : Log_event(buf, desc),
    m_save_thd_query_txt(0),
    m_save_thd_query_len(0)
{
  m_query_len= event_len - desc->common_header_len;
  m_query_txt= (char*) buf + desc->common_header_len;
}

Annotate_rows_log_event::~Annotate_rows_log_event()
{
#ifndef MYSQL_CLIENT
  if (m_save_thd_query_txt)
    thd->set_query(m_save_thd_query_txt, m_save_thd_query_len);
#endif
}

int Annotate_rows_log_event::get_data_size()
{
  return m_query_len;
}

Log_event_type Annotate_rows_log_event::get_type_code()
{
  return ANNOTATE_ROWS_EVENT;
}

bool Annotate_rows_log_event::is_valid() const
{
  return (m_query_txt != NULL && m_query_len != 0);
}

#ifndef MYSQL_CLIENT
bool Annotate_rows_log_event::write_data_header(IO_CACHE *file)
{ 
  return 0;
}
#endif

#ifndef MYSQL_CLIENT
bool Annotate_rows_log_event::write_data_body(IO_CACHE *file)
{
  return wrapper_my_b_safe_write(file, (uchar*) m_query_txt, m_query_len);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
void Annotate_rows_log_event::pack_info(THD *thd, Protocol* protocol)
{
  if (m_query_txt && m_query_len)
    protocol->store(m_query_txt, m_query_len, &my_charset_bin);
}
#endif

#ifdef MYSQL_CLIENT
void Annotate_rows_log_event::print(FILE *file, PRINT_EVENT_INFO *pinfo)
{
  if (pinfo->short_form)
    return;

  print_header(&pinfo->head_cache, pinfo, TRUE);
  my_b_printf(&pinfo->head_cache, "\tAnnotate_rows:\n");

  char *pbeg;   // beginning of the next line
  char *pend;   // end of the next line
  uint cnt= 0;  // characters counter

  for (pbeg= m_query_txt; ; pbeg= pend)
  {
    // skip all \r's and \n's at the beginning of the next line
    for (;; pbeg++)
    {
      if (++cnt > m_query_len)
        return;

      if (*pbeg != '\r' && *pbeg != '\n')
        break;
    }

    // find end of the next line
    for (pend= pbeg + 1;
         ++cnt <= m_query_len && *pend != '\r' && *pend != '\n';
         pend++)
      ;

    // print next line
    my_b_write(&pinfo->head_cache, (const uchar*) "#Q> ", 4);
    my_b_write(&pinfo->head_cache, (const uchar*) pbeg, pend - pbeg);
    my_b_write(&pinfo->head_cache, (const uchar*) "\n", 1);
  }
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Annotate_rows_log_event::do_apply_event(Relay_log_info const *rli)
{
  m_save_thd_query_txt= thd->query();
  m_save_thd_query_len= thd->query_length();
  thd->set_query(m_query_txt, m_query_len);
  return 0;
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Annotate_rows_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
Log_event::enum_skip_reason
Annotate_rows_log_event::do_shall_skip(Relay_log_info *rli)
{
  return continue_group(rli);
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
  for (unsigned int i= 0 ; i < m_table->s->fields ; i++)
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
Table_map_log_event::Table_map_log_event(THD *thd, TABLE *tbl, ulong tid,
                                         bool is_transactional)
  : Log_event(thd, 0, is_transactional),
    m_table(tbl),
    m_dbnam(tbl->s->db.str),
    m_dblen(m_dbnam ? tbl->s->db.length : 0),
    m_tblnam(tbl->s->table_name.str),
    m_tbllen(tbl->s->table_name.length),
    m_colcnt(tbl->s->fields),
    m_memory(NULL),
    m_table_id(tid),
    m_flags(TM_BIT_LEN_EXACT_F),
    m_data_size(0),
    m_field_metadata(0),
    m_field_metadata_size(0),
    m_null_bits(0),
    m_meta_memory(NULL)
{
  uchar cbuf[sizeof(m_colcnt) + 1];
  uchar *cbuf_end;
  DBUG_ASSERT(m_table_id != ~0UL);
  /*
    In TABLE_SHARE, "db" and "table_name" are 0-terminated (see this comment in
    table.cc / alloc_table_share():
      Use the fact the key is db/0/table_name/0
    As we rely on this let's assert it.
  */
  DBUG_ASSERT((tbl->s->db.str == 0) ||
              (tbl->s->db.str[tbl->s->db.length] == 0));
  DBUG_ASSERT(tbl->s->table_name.str[tbl->s->table_name.length] == 0);


  m_data_size=  TABLE_MAP_HEADER_LEN;
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master", m_data_size= 6;);
  m_data_size+= m_dblen + 2;	// Include length and terminating \0
  m_data_size+= m_tbllen + 2;	// Include length and terminating \0
  cbuf_end= net_store_length(cbuf, (size_t) m_colcnt);
  DBUG_ASSERT(static_cast<size_t>(cbuf_end - cbuf) <= sizeof(cbuf));
  m_data_size+= (cbuf_end - cbuf) + m_colcnt;	// COLCNT and column types

  /* If malloc fails, caught in is_valid() */
  if ((m_memory= (uchar*) my_malloc(m_colcnt, MYF(MY_WME))))
  {
    m_coltype= reinterpret_cast<uchar*>(m_memory);
    for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
      m_coltype[i]= m_table->field[i]->type();
  }

  /*
    Calculate a bitmap for the results of maybe_null() for all columns.
    The bitmap is used to determine when there is a column from the master
    that is not on the slave and is null and thus not in the row data during
    replication.
  */
  uint num_null_bytes= (m_table->s->fields + 7) / 8;
  m_data_size+= num_null_bytes;
  m_meta_memory= (uchar *)my_multi_malloc(MYF(MY_WME),
                                 &m_null_bits, num_null_bytes,
                                 &m_field_metadata, (m_colcnt * 2),
                                 NULL);

  bzero(m_field_metadata, (m_colcnt * 2));

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

  bzero(m_null_bits, num_null_bytes);
  for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
    if (m_table->field[i]->maybe_null())
      m_null_bits[(i / 8)]+= 1 << (i % 8);

}
#endif /* !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#if defined(HAVE_REPLICATION)
Table_map_log_event::Table_map_log_event(const char *buf, uint event_len,
                                         const Format_description_log_event
                                         *description_event)

  : Log_event(buf, description_event),
#ifndef MYSQL_CLIENT
    m_table(NULL),
#endif
    m_dbnam(NULL), m_dblen(0), m_tblnam(NULL), m_tbllen(0),
    m_colcnt(0), m_coltype(0),
    m_memory(NULL), m_table_id(ULONG_MAX), m_flags(0),
    m_data_size(0), m_field_metadata(0), m_field_metadata_size(0),
    m_null_bits(0), m_meta_memory(NULL)
{
  unsigned int bytes_read= 0;
  DBUG_ENTER("Table_map_log_event::Table_map_log_event(const char*,uint,...)");

  uint8 common_header_len= description_event->common_header_len;
  uint8 post_header_len= description_event->post_header_len[TABLE_MAP_EVENT-1];
  DBUG_PRINT("info",("event_len: %u  common_header_len: %d  post_header_len: %d",
                     event_len, common_header_len, post_header_len));

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_valgrind
  DBUG_DUMP("event buffer", (uchar*) buf, event_len);
#endif

  /* Read the post-header */
  const char *post_start= buf + common_header_len;

  post_start+= TM_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    DBUG_ASSERT(post_header_len == TABLE_MAP_HEADER_LEN);
    m_table_id= (ulong) uint6korr(post_start);
    post_start+= TM_FLAGS_OFFSET;
  }

  DBUG_ASSERT(m_table_id != ~0UL);

  m_flags= uint2korr(post_start);

  /* Read the variable part of the event */
  const char *const vpart= buf + common_header_len + post_header_len;

  /* Extract the length of the various parts from the buffer */
  uchar const *const ptr_dblen= (uchar const*)vpart + 0;
  m_dblen= *(uchar*) ptr_dblen;

  /* Length of database name + counter + terminating null */
  uchar const *const ptr_tbllen= ptr_dblen + m_dblen + 2;
  m_tbllen= *(uchar*) ptr_tbllen;

  /* Length of table name + counter + terminating null */
  uchar const *const ptr_colcnt= ptr_tbllen + m_tbllen + 2;
  uchar *ptr_after_colcnt= (uchar*) ptr_colcnt;
  m_colcnt= net_field_length(&ptr_after_colcnt);

  DBUG_PRINT("info",("m_dblen: %lu  off: %ld  m_tbllen: %lu  off: %ld  m_colcnt: %lu  off: %ld",
                     (ulong) m_dblen, (long) (ptr_dblen-(const uchar*)vpart), 
                     (ulong) m_tbllen, (long) (ptr_tbllen-(const uchar*)vpart),
                     m_colcnt, (long) (ptr_colcnt-(const uchar*)vpart)));

  /* Allocate mem for all fields in one go. If fails, caught in is_valid() */
  m_memory= (uchar*) my_multi_malloc(MYF(MY_WME),
                                     &m_dbnam, (uint) m_dblen + 1,
                                     &m_tblnam, (uint) m_tbllen + 1,
                                     &m_coltype, (uint) m_colcnt,
                                     NullS);

  if (m_memory)
  {
    /* Copy the different parts into their memory */
    strncpy(const_cast<char*>(m_dbnam), (const char*)ptr_dblen  + 1, m_dblen + 1);
    strncpy(const_cast<char*>(m_tblnam), (const char*)ptr_tbllen + 1, m_tbllen + 1);
    memcpy(m_coltype, ptr_after_colcnt, m_colcnt);

    ptr_after_colcnt= ptr_after_colcnt + m_colcnt;
    bytes_read= (uint) (ptr_after_colcnt - (uchar *)buf);
    DBUG_PRINT("info", ("Bytes read: %d.\n", bytes_read));
    if (bytes_read < event_len)
    {
      m_field_metadata_size= net_field_length(&ptr_after_colcnt);
      DBUG_ASSERT(m_field_metadata_size <= (m_colcnt * 2));
      uint num_null_bytes= (m_colcnt + 7) / 8;
      m_meta_memory= (uchar *)my_multi_malloc(MYF(MY_WME),
                                     &m_null_bits, num_null_bytes,
                                     &m_field_metadata, m_field_metadata_size,
                                     NULL);
      memcpy(m_field_metadata, ptr_after_colcnt, m_field_metadata_size);
      ptr_after_colcnt= (uchar*)ptr_after_colcnt + m_field_metadata_size;
      memcpy(m_null_bits, ptr_after_colcnt, num_null_bytes);
    }
  }

  DBUG_VOID_RETURN;
}
#endif

Table_map_log_event::~Table_map_log_event()
{
  my_free(m_meta_memory);
  my_free(m_memory);
}


#ifdef MYSQL_CLIENT

/*
  Rewrite database name for the event to name specified by new_db
  SYNOPSIS
    new_db   Database name to change to
    new_len  Length
    desc     Event describing binlog that we're writing to.

  DESCRIPTION
    Reset db name. This function assumes that temp_buf member contains event
    representation taken from a binary log. It resets m_dbnam and m_dblen and
    rewrites temp_buf with new db name.

  RETURN 
    0     - Success
    other - Error
*/

int Table_map_log_event::rewrite_db(const char* new_db, size_t new_len,
                                    const Format_description_log_event* desc)
{
  DBUG_ENTER("Table_map_log_event::rewrite_db");
  DBUG_ASSERT(temp_buf);

  uint header_len= min(desc->common_header_len,
                       LOG_EVENT_MINIMAL_HEADER_LEN) + TABLE_MAP_HEADER_LEN;
  int len_diff;

  if (!(len_diff= new_len - m_dblen))
  {
    memcpy((void*) (temp_buf + header_len + 1), new_db, m_dblen + 1);
    memcpy((void*) m_dbnam, new_db, m_dblen + 1);
    DBUG_RETURN(0);
  }

  // Create new temp_buf
  ulong event_cur_len= uint4korr(temp_buf + EVENT_LEN_OFFSET);
  ulong event_new_len= event_cur_len + len_diff;
  char* new_temp_buf= (char*) my_malloc(event_new_len, MYF(MY_WME));

  if (!new_temp_buf)
  {
    sql_print_error("Table_map_log_event::rewrite_db: "
                    "failed to allocate new temp_buf (%d bytes required)",
                    event_new_len);
    DBUG_RETURN(-1);
  }

  // Rewrite temp_buf
  char* ptr= new_temp_buf;
  ulong cnt= 0;

  // Copy header and change event length
  memcpy(ptr, temp_buf, header_len);
  int4store(ptr + EVENT_LEN_OFFSET, event_new_len);
  ptr += header_len;
  cnt += header_len;

  // Write new db name length and new name
  *ptr++ = new_len;
  memcpy(ptr, new_db, new_len + 1);
  ptr += new_len + 1;
  cnt += m_dblen + 2;

  // Copy rest part
  memcpy(ptr, temp_buf + cnt, event_cur_len - cnt);

  // Reregister temp buf
  free_temp_buf();
  register_temp_buf(new_temp_buf, TRUE);

  // Reset m_dbnam and m_dblen members
  m_dblen= new_len;

  // m_dbnam resides in m_memory together with m_tblnam and m_coltype
  uchar* memory= m_memory;
  char const* tblnam= m_tblnam;
  uchar* coltype= m_coltype;

  m_memory= (uchar*) my_multi_malloc(MYF(MY_WME),
                                     &m_dbnam, (uint) m_dblen + 1,
                                     &m_tblnam, (uint) m_tbllen + 1,
                                     &m_coltype, (uint) m_colcnt,
                                     NullS);

  if (!m_memory)
  {
    sql_print_error("Table_map_log_event::rewrite_db: "
                    "failed to allocate new m_memory (%d + %d + %d bytes required)",
                    m_dblen + 1, m_tbllen + 1, m_colcnt);
    DBUG_RETURN(-1);
  }

  memcpy((void*)m_dbnam, new_db, m_dblen + 1);
  memcpy((void*)m_tblnam, tblnam, m_tbllen + 1);
  memcpy(m_coltype, coltype, m_colcnt);

  my_free(memory);
  DBUG_RETURN(0);
}
#endif /* MYSQL_CLIENT */


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
  SAME_ID_MAPPING_SAME_TABLE= 3
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

  if (rli->sql_thd->slave_thread /* filtering is for slave only */ &&
      (!rpl_filter->db_ok(table_list->db) ||
       (rpl_filter->is_on() && !rpl_filter->tables_ok("", table_list))))
    res= FILTERED_OUT;
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
  char *db_mem, *tname_mem;
  size_t dummy_len;
  void *memory;
  DBUG_ENTER("Table_map_log_event::do_apply_event(Relay_log_info*)");
  DBUG_ASSERT(rli->sql_thd == thd);

  /* Step the query id to mark what columns that are actually used. */
  thd->set_query_id(next_query_id());

  if (!(memory= my_multi_malloc(MYF(MY_WME),
                                &table_list, (uint) sizeof(RPL_TABLE_LIST),
                                &db_mem, (uint) NAME_LEN + 1,
                                &tname_mem, (uint) NAME_LEN + 1,
                                NullS)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  strmov(db_mem, rpl_filter->get_rewrite_db(m_dbnam, &dummy_len));
  strmov(tname_mem, m_tblnam);

  table_list->init_one_table(db_mem, strlen(db_mem),
                             tname_mem, strlen(tname_mem),
                             tname_mem, TL_WRITE);

  table_list->table_id= DBUG_EVALUATE_IF("inject_tblmap_same_id_maps_diff_table", 0, m_table_id);
  table_list->updating= 1;
  table_list->required_type= FRMTYPE_TABLE;
  DBUG_PRINT("debug", ("table: %s is mapped to %u", table_list->table_name, table_list->table_id));
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
    /*
      If mapped already but with different properties, we raise an
      error.
      If mapped already but with same properties we skip the event.
      If filtered out we skip the event.

      In all three cases, we need to free the memory previously 
      allocated.
     */
    if (tblmap_status == SAME_ID_MAPPING_DIFFERENT_TABLE)
    {
      /*
        Something bad has happened. We need to stop the slave as strange things
        could happen if we proceed: slave crash, wrong table being updated, ...
        As a consequence we push an error in this case.
       */

      char buf[256];

      my_snprintf(buf, sizeof(buf), 
                  "Found table map event mapping table id %u which "
                  "was already mapped but with different settings.",
                  table_list->table_id);

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
  DBUG_ASSERT(m_table_id != ~0UL);
  uchar buf[TABLE_MAP_HEADER_LEN];
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, m_table_id);
                    int2store(buf + 4, m_flags);
                    return (wrapper_my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + TM_MAPID_OFFSET, (ulonglong)m_table_id);
  int2store(buf + TM_FLAGS_OFFSET, m_flags);
  return (wrapper_my_b_safe_write(file, buf, TABLE_MAP_HEADER_LEN));
}

bool Table_map_log_event::write_data_body(IO_CACHE *file)
{
  DBUG_ASSERT(m_dbnam != NULL);
  DBUG_ASSERT(m_tblnam != NULL);
  /* We use only one byte per length for storage in event: */
  DBUG_ASSERT(m_dblen <= min(NAME_LEN, 255));
  DBUG_ASSERT(m_tbllen <= min(NAME_LEN, 255));

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

  return (wrapper_my_b_safe_write(file, dbuf,      sizeof(dbuf)) ||
          wrapper_my_b_safe_write(file, (const uchar*)m_dbnam,   m_dblen+1) ||
          wrapper_my_b_safe_write(file, tbuf,      sizeof(tbuf)) ||
          wrapper_my_b_safe_write(file, (const uchar*)m_tblnam,  m_tbllen+1) ||
          wrapper_my_b_safe_write(file, cbuf, (size_t) (cbuf_end - cbuf)) ||
          wrapper_my_b_safe_write(file, m_coltype, m_colcnt) ||
          wrapper_my_b_safe_write(file, mbuf, (size_t) (mbuf_end - mbuf)) ||
          wrapper_my_b_safe_write(file, m_field_metadata, m_field_metadata_size),
          wrapper_my_b_safe_write(file, m_null_bits, (m_colcnt + 7) / 8));
 }
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/*
  Print some useful information for the SHOW BINARY LOG information
  field.
 */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Table_map_log_event::pack_info(THD *thd, Protocol *protocol)
{
    char buf[256];
    size_t bytes= my_snprintf(buf, sizeof(buf),
                                 "table_id: %lu (%s.%s)",
                              m_table_id, m_dbnam, m_tblnam);
    protocol->store(buf, bytes, &my_charset_bin);
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
                "\tTable_map: %`s.%`s mapped to number %lu\n",
                m_dbnam, m_tblnam, m_table_id);
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
                                           ulong tid_arg,
                                           MY_BITMAP const *cols,
                                           bool is_transactional)
  : Rows_log_event(thd_arg, tbl_arg, tid_arg, cols, is_transactional)
{
}
#endif

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Write_rows_log_event::Write_rows_log_event(const char *buf, uint event_len,
                                           const Format_description_log_event
                                           *description_event)
: Rows_log_event(buf, event_len, WRITE_ROWS_EVENT, description_event)
{
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
    status_var_increment(thd->status_var.com_stat[SQLCOM_INSERT]);

  /**
     todo: to introduce a property for the event (handler?) which forces
     applying the event in the replace (idempotent) fashion.
  */
  if ((slave_exec_mode == SLAVE_EXEC_MODE_IDEMPOTENT) ||
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
  if ((slave_exec_mode == SLAVE_EXEC_MODE_IDEMPOTENT) ||
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
Rows_log_event::write_row(const Relay_log_info *const rli,
                          const bool overwrite)
{
  DBUG_ENTER("write_row");
  DBUG_ASSERT(m_table != NULL && thd != NULL);

  TABLE *table= m_table;  // pointer to event's table
  int error;
  int UNINIT_VAR(keynum);
  auto_afree_ptr<char> key(NULL);

  prepare_record(table, m_width,
                 table->file->ht->db_type != DB_TYPE_NDBCLUSTER);

  /* unpack row into table->record[0] */
  if ((error= unpack_current_row(rli)))
    DBUG_RETURN(error);

  if (m_curr_row == m_rows_buf)
  {
    /* this is the first row to be inserted, we estimate the rows with
       the size of the first row and use that value to initialize
       storage engine for bulk insertion */
    ulong estimated_rows= (m_rows_end - m_curr_row) / (m_curr_row_end - m_curr_row);
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
      error= table->file->ha_rnd_pos(table->record[1], table->file->dup_ref);
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
      error= table->file->ha_index_read_idx_map(table->record[1], keynum,
                                                (const uchar*)key.get(),
                                                HA_WHOLE_KEY,
                                                HA_READ_KEY_EXACT);
      if (error)
      {
        DBUG_PRINT("info",("index_read_idx() returns %s", HA_ERR(error)));
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

#endif

int
Write_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  DBUG_ASSERT(m_table != NULL);
  int error= write_row(rli, slave_exec_mode == SLAVE_EXEC_MODE_IDEMPOTENT);

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

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
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

  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_DUMP("record[1]", table->record[1], table->s->reclength);

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

  /**
    Compare full record only if:
    - there are no blob fields (otherwise we would also need 
      to compare blobs contents as well);
    - there are no varchar fields (otherwise we would also need
      to compare varchar contents as well);
    - there are no null fields, otherwise NULLed fields 
      contents (i.e., the don't care bytes) may show arbitrary 
      values, depending on how each engine handles internally.
    */
  if ((table->s->blob_fields + 
       table->s->varchar_fields + 
       table->s->null_fields) == 0)
  {
    result= cmp_record(table,record[1]);
    goto record_compare_exit;
  }

  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->s->rec_buff_length,
	     table->s->null_bytes))
  {
    result= TRUE;				// Diff in NULL value
    goto record_compare_exit;
  }

  /* Compare fields */
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {

    /**
      We only compare field contents that are not null.
      NULL fields (i.e., their null bits) were compared 
      earlier.
    */
    if (!(*(ptr))->is_null())
    {
      if ((*ptr)->cmp_binary_offset(table->s->rec_buff_length))
      {
        result= TRUE;
        goto record_compare_exit;
      }
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

      if (table->s->last_null_bit_pos)
        table->record[i][table->s->null_bytes - 1]= saved_filler[i];
    }
  }

  return result;
}


/**
  Find the best key to use when locating the row in @c find_row().

  A primary key is preferred if it exists; otherwise a unique index is
  preferred. Else we pick the index with the smalles rec_per_key value.

  If a suitable key is found, set @c m_key, @c m_key_nr and @c m_key_info
  member fields appropriately.

  @returns Error code on failure, 0 on success.
*/
int Rows_log_event::find_key()
{
  uint i, best_key_nr, last_part;
  KEY *key, *best_key;
  ulong best_rec_per_key, tmp;
  DBUG_ENTER("Rows_log_event::find_key");
  DBUG_ASSERT(m_table);

  best_key_nr= MAX_KEY;
  LINT_INIT(best_key);
  LINT_INIT(best_rec_per_key);

  /*
    Keys are sorted so that any primary key is first, followed by unique keys,
    followed by any other. So we will automatically pick the primary key if
    it exists.
  */
  for (i= 0, key= m_table->key_info; i < m_table->s->keys; i++, key++)
  {
    if (!m_table->s->keys_in_use.is_set(i))
      continue;
    /*
      We cannot use a unique key with NULL-able columns to uniquely identify
      a row (but we can still select it for range scan below if nothing better
      is available).
    */
    if ((key->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
    {
      best_key_nr= i;
      best_key= key;
      break;
    }
    /*
      We can only use a non-unique key if it allows range scans (ie. skip
      FULLTEXT indexes and such).
    */
    last_part= key->key_parts - 1;
    DBUG_PRINT("info", ("Index %s rec_per_key[%u]= %lu",
                        key->name, last_part, key->rec_per_key[last_part]));
    if (!(m_table->file->index_flags(i, last_part, 1) & HA_READ_NEXT))
      continue;

    tmp= key->rec_per_key[last_part];
    if (best_key_nr == MAX_KEY || (tmp > 0 && tmp < best_rec_per_key))
    {
      best_key_nr= i;
      best_key= key;
      best_rec_per_key= tmp;
    }
  }

  if (best_key_nr == MAX_KEY)
  {
    m_key_info= NULL;
    DBUG_RETURN(0);
  }

  // Allocate buffer for key searches
  m_key= (uchar *) my_malloc(best_key->key_length, MYF(MY_WME));
  if (m_key == NULL)
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  m_key_info= best_key;
  m_key_nr= best_key_nr;

  DBUG_RETURN(0);;
}


/* 
  Check if we are already spending too much time on this statement.
  if we are, warn user that it might be because table does not have
  a PK, but only if the warning was not printed before for this STMT.

  @param type          The event type code.
  @param table_name    The name of the table that the slave is 
                       operating.
  @param is_index_scan States whether the slave is doing an index scan 
                       or not.
  @param rli           The relay metadata info.
*/
static inline 
void issue_long_find_row_warning(Log_event_type type, 
                                 const char *table_name,
                                 bool is_index_scan,
                                 const Relay_log_info *rli)
{
  if ((global_system_variables.log_warnings > 1 && 
      !const_cast<Relay_log_info*>(rli)->is_long_find_row_note_printed()))
  {
    time_t now= my_time(0);
    time_t stmt_ts= const_cast<Relay_log_info*>(rli)->get_row_stmt_start_timestamp();
    
    DBUG_EXECUTE_IF("inject_long_find_row_note", 
                    stmt_ts-=(LONG_FIND_ROW_THRESHOLD*2););

    long delta= (long) (now - stmt_ts);

    if (delta > LONG_FIND_ROW_THRESHOLD)
    {
      const_cast<Relay_log_info*>(rli)->set_long_find_row_note_printed();
      const char* evt_type= type == DELETE_ROWS_EVENT ? " DELETE" : "n UPDATE";
      const char* scan_type= is_index_scan ? "scanning an index" : "scanning the table";

      sql_print_information("The slave is applying a ROW event on behalf of a%s statement "
                            "on table %s and is currently taking a considerable amount "
                            "of time (%ld seconds). This is due to the fact that it is %s "
                            "while looking up records to be processed. Consider adding a "
                            "primary key (or unique key) to the table to improve "
                            "performance.", evt_type, table_name, delta, scan_type);
    }
  }
}


/**
  Locate the current row in event's table.

  The current row is pointed by @c m_curr_row. Member @c m_width tells
  how many columns are there in the row (this can be differnet from
  the number of columns in the table). It is assumed that event's
  table is already open and pointed by @c m_table.

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

  Note that one MUST call ha_index_or_rnd_end() after this function if
  it returns 0 as we must leave the row position in the handler intact
  for any following update/delete command.
*/

int Rows_log_event::find_row(const Relay_log_info *rli)
{
  DBUG_ENTER("Rows_log_event::find_row");

  DBUG_ASSERT(m_table && m_table->in_use != NULL);

  TABLE *table= m_table;
  int error= 0;
  bool is_table_scan= false, is_index_scan= false;

  /*
    rpl_row_tabledefs.test specifies that
    if the extra field on the slave does not have a default value
    and this is okay with Delete or Update events.
    Todo: fix wl3228 hld that requires defauls for all types of events
  */
  
  prepare_record(table, m_width, FALSE);
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
              int error= table->file->ha_rnd_pos(table->record[0],
              table->file->ref);
      ADD>>>  DBUG_ASSERT(memcmp(table->record[1], table->record[0],
                                 table->s->reclength) == 0);

    */
    DBUG_PRINT("info",("locating record using primary key (position)"));
    int error= table->file->ha_rnd_pos_by_record(table->record[0]);
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

  if (m_key_info)
  {
    DBUG_PRINT("info",("locating record using key #%u [%s] (index_read)",
                       m_key_nr, m_key_info->name));
    /* We use this to test that the correct key is used in test cases. */
    DBUG_EXECUTE_IF("slave_crash_if_wrong_index",
                    if(0 != strcmp(m_key_info->name,"expected_key")) abort(););

    /* The key is active: search the table using the index */
    if (!table->file->inited &&
        (error= table->file->ha_index_init(m_key_nr, FALSE)))
    {
      DBUG_PRINT("info",("ha_index_init returns error %d",error));
      table->file->print_error(error, MYF(0));
      goto end;
    }

    /* Fill key data for the row */

    DBUG_ASSERT(m_key);
    key_copy(m_key, table->record[0], m_key_info, 0);

    /*
      Don't print debug messages when running valgrind since they can
      trigger false warnings.
     */
#ifndef HAVE_valgrind
    DBUG_DUMP("key data", m_key, m_key_info->key_length);
#endif

    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    if (table->s->null_bytes > 0)
      table->record[0][table->s->null_bytes - 1]|=
        256U - (1U << table->s->last_null_bit_pos);

    if ((error= table->file->ha_index_read_map(table->record[0], m_key, 
                                               HA_WHOLE_KEY,
                                               HA_READ_KEY_EXACT)))
    {
      DBUG_PRINT("info",("no record matching the key found in the table"));
      if (error == HA_ERR_RECORD_DELETED)
        error= HA_ERR_KEY_NOT_FOUND;
      table->file->print_error(error, MYF(0));
      table->file->ha_index_end();
      goto end;
    }

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_valgrind
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
        error= 0;
        goto end;
      }
      else
      {
        KEY *keyinfo= table->key_info;
        /*
          Unique has nullable part. We need to check if there is any
          field in the BI image that is null and part of UNNI.
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
          error= 0;
          goto end;
        }

        /* else fall through to index scan */
      }
    }

    is_index_scan=true;

    /*
      In case key is not unique, we still have to iterate over records found
      and find the one which is identical to the row given. A copy of the 
      record we are looking for is stored in record[1].
     */ 
    DBUG_PRINT("info",("non-unique index, scanning it to find matching record")); 
    /* We use this to test that the correct key is used in test cases. */
    DBUG_EXECUTE_IF("slave_crash_if_index_scan", abort(););

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

      while ((error= table->file->ha_index_next(table->record[0])))
      {
        /* We just skip records that has already been deleted */
        if (error == HA_ERR_RECORD_DELETED)
          continue;
        DBUG_PRINT("info",("no record matching the given row found"));
        table->file->print_error(error, MYF(0));
        table->file->ha_index_end();
        goto end;
      }
    }
  }
  else
  {
    DBUG_PRINT("info",("locating record using table scan (rnd_next)"));
    /* We use this to test that the correct key is used in test cases. */
    DBUG_EXECUTE_IF("slave_crash_if_table_scan", abort(););

    /* We don't have a key: search the table using rnd_next() */
    if ((error= table->file->ha_rnd_init_with_error(1)))
    {
      DBUG_PRINT("info",("error initializing table scan"
                         " (ha_rnd_init returns %d)",error));
      goto end;
    }

    is_table_scan= true;

    /* Continue until we find the right record or have made a full loop */
    do
    {
  restart_rnd_next:
      error= table->file->ha_rnd_next(table->record[0]);

      if (error)
        DBUG_PRINT("info", ("error: %s", HA_ERR(error)));
      switch (error) {

      case 0:
        DBUG_DUMP("record found", table->record[0], table->s->reclength);
        break;

      case HA_ERR_END_OF_FILE:
        DBUG_PRINT("info", ("Record not found"));
        table->file->ha_rnd_end();
        goto end;

      /*
        If the record was deleted, we pick the next one without doing
        any comparisons.
      */
      case HA_ERR_RECORD_DELETED:
        goto restart_rnd_next;

      default:
        DBUG_PRINT("info", ("Failed to get next record"
                            " (rnd_next returns %d)",error));
        table->file->print_error(error, MYF(0));
        table->file->ha_rnd_end();
        goto end;
      }
    }
    while (record_compare(table));
    
    /* 
      Note: above record_compare will take into accout all record fields 
      which might be incorrect in case a partial row was given in the event
     */

    DBUG_ASSERT(error == HA_ERR_END_OF_FILE || error == 0);
  }

end:
  if (is_table_scan || is_index_scan)
    issue_long_find_row_warning(get_type_code(), m_table->alias.c_ptr(), 
                                is_index_scan, rli);
  table->default_column_bitmaps();
  DBUG_RETURN(error);
}

#endif

/*
  Constructor used to build an event for writing to the binary log.
 */

#ifndef MYSQL_CLIENT
Delete_rows_log_event::Delete_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             ulong tid, MY_BITMAP const *cols,
                                             bool is_transactional)
  : Rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional)
{
}
#endif /* #if !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Delete_rows_log_event::Delete_rows_log_event(const char *buf, uint event_len,
                                             const Format_description_log_event
                                             *description_event)
  : Rows_log_event(buf, event_len, DELETE_ROWS_EVENT, description_event)
{
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int 
Delete_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  /*
    Increment the global status delete count variable
   */
  if (get_flags(STMT_END_F))
    status_var_increment(thd->status_var.com_stat[SQLCOM_DELETE]);

  if ((m_table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      m_table->s->primary_key < MAX_KEY)
  {
    /*
      We don't need to allocate any memory for m_key since it is not used.
    */
    return 0;
  }

  return find_key();
}

int 
Delete_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const, 
                                               int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  my_free(m_key);
  m_key= NULL;
  m_key_info= NULL;

  return error;
}

int Delete_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  int error;
  DBUG_ASSERT(m_table != NULL);

  if (!(error= find_row(rli))) 
  { 
    /*
      Delete the record found, located in record[0]
    */
    error= m_table->file->ha_delete_row(m_table->record[0]);
    m_table->file->ha_index_or_rnd_end();
  }
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
                                             ulong tid,
                                             MY_BITMAP const *cols_bi,
                                             MY_BITMAP const *cols_ai,
                                             bool is_transactional)
: Rows_log_event(thd_arg, tbl_arg, tid, cols_bi, is_transactional)
{
  init(cols_ai);
}

Update_rows_log_event::Update_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             ulong tid,
                                             MY_BITMAP const *cols,
                                             bool is_transactional)
: Rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional)
{
  init(cols);
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
  if (m_cols_ai.bitmap == m_bitbuf_ai) // no my_malloc happened
    m_cols_ai.bitmap= 0; // so no my_free in bitmap_free
  bitmap_free(&m_cols_ai); // To pair with bitmap_init().
}


/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Update_rows_log_event::Update_rows_log_event(const char *buf, uint event_len,
                                             const
                                             Format_description_log_event
                                             *description_event)
  : Rows_log_event(buf, event_len, UPDATE_ROWS_EVENT, description_event)
{
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)

int 
Update_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  /*
    Increment the global status update count variable
  */
  if (get_flags(STMT_END_F))
    status_var_increment(thd->status_var.com_stat[SQLCOM_UPDATE]);

  int err;
  if ((err= find_key()))
    return err;

  m_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return 0;
}

int 
Update_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const, 
                                               int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  my_free(m_key); // Free for multi_malloc
  m_key= NULL;
  m_key_info= NULL;

  return error;
}

int 
Update_rows_log_event::do_exec_row(const Relay_log_info *const rli)
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
  /* this also updates m_curr_row_end */
  if ((error= unpack_current_row(rli)))
    goto err;

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row is in record[0].
  */
#ifndef HAVE_valgrind
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

err:
  m_table->file->ha_index_or_rnd_end();
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


Incident_log_event::Incident_log_event(const char *buf, uint event_len,
                                       const Format_description_log_event *descr_event)
  : Log_event(buf, descr_event)
{
  DBUG_ENTER("Incident_log_event::Incident_log_event");
  uint8 const common_header_len=
    descr_event->common_header_len;
  uint8 const post_header_len=
    descr_event->post_header_len[INCIDENT_EVENT-1];

  DBUG_PRINT("info",("event_len: %u; common_header_len: %d; post_header_len: %d",
                     event_len, common_header_len, post_header_len));

  int incident_number= uint2korr(buf + common_header_len);
  if (incident_number >= INCIDENT_COUNT ||
      incident_number <= INCIDENT_NONE)
  {
    // If the incident is not recognized, this binlog event is
    // invalid.  If we set incident_number to INCIDENT_NONE, the
    // invalidity will be detected by is_valid().
    m_incident= INCIDENT_NONE;
    DBUG_VOID_RETURN;
  }
  m_incident= static_cast<Incident>(incident_number);
  char const *ptr= buf + common_header_len + post_header_len;
  char const *const str_end= buf + event_len;
  uint8 len= 0;                   // Assignment to keep compiler happy
  const char *str= NULL;          // Assignment to keep compiler happy
  read_str(&ptr, str_end, &str, &len);
  m_message.str= const_cast<char*>(str);
  m_message.length= len;
  DBUG_PRINT("info", ("m_incident: %d", m_incident));
  DBUG_VOID_RETURN;
}


Incident_log_event::~Incident_log_event()
{
}


const char *
Incident_log_event::description() const
{
  static const char *const description[]= {
    "NOTHING",                                  // Not used
    "LOST_EVENTS"
  };

  DBUG_PRINT("info", ("m_incident: %d", m_incident));
  return description[m_incident];
}


#ifndef MYSQL_CLIENT
void Incident_log_event::pack_info(THD *thd, Protocol *protocol)
{
  char buf[256];
  size_t bytes;
  if (m_message.length > 0)
    bytes= my_snprintf(buf, sizeof(buf), "#%d (%s)",
                       m_incident, description());
  else
    bytes= my_snprintf(buf, sizeof(buf), "#%d (%s): %s",
                       m_incident, description(), m_message.str);
  protocol->store(buf, bytes, &my_charset_bin);
}
#endif


#ifdef MYSQL_CLIENT
void
Incident_log_event::print(FILE *file,
                          PRINT_EVENT_INFO *print_event_info)
{
  if (print_event_info->short_form)
    return;

  Write_on_release_cache cache(&print_event_info->head_cache, file);
  print_header(&cache, print_event_info, FALSE);
  my_b_printf(&cache, "\n# Incident: %s\nRELOAD DATABASE; # Shall generate syntax error\n", description());
}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int
Incident_log_event::do_apply_event(Relay_log_info const *rli)
{
  DBUG_ENTER("Incident_log_event::do_apply_event");
  rli->report(ERROR_LEVEL, ER_SLAVE_INCIDENT,
              ER(ER_SLAVE_INCIDENT),
              description(),
              m_message.length > 0 ? m_message.str : "<none>");
  DBUG_RETURN(1);
}
#endif

bool
Incident_log_event::write_data_header(IO_CACHE *file)
{
  DBUG_ENTER("Incident_log_event::write_data_header");
  DBUG_PRINT("enter", ("m_incident: %d", m_incident));
  uchar buf[sizeof(int16)];
  int2store(buf, (int16) m_incident);
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
  tmp[0]= (uchar) m_message.length;
  crc= my_checksum(crc, (uchar*) tmp, 1);
  if (m_message.length > 0)
  {
    crc= my_checksum(crc, (uchar*) m_message.str, m_message.length);
    // todo: report a bug on write_str accepts uint but treats it as uchar
  }
  DBUG_RETURN(write_str(file, m_message.str, (uint) m_message.length));
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
   thread_id(0), thread_id_printed(false), skip_replication(0),
   base64_output_mode(BASE64_OUTPUT_UNSPEC), printed_fd_event(FALSE)
{
  /*
    Currently we only use static PRINT_EVENT_INFO objects, so zeroed at
    program's startup, but these explicit bzero() is for the day someone
    creates dynamic instances.
  */
  bzero(db, sizeof(db));
  bzero(charset, sizeof(charset));
  bzero(time_zone_str, sizeof(time_zone_str));
  delimiter[0]= ';';
  delimiter[1]= 0;
  myf const flags = MYF(MY_WME | MY_NABP);
  open_cached_file(&head_cache, NULL, NULL, 0, flags);
  open_cached_file(&body_cache, NULL, NULL, 0, flags);
}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
Heartbeat_log_event::Heartbeat_log_event(const char* buf, uint event_len,
                    const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  uint8 header_size= description_event->common_header_len;
  ident_len = event_len - header_size;
  set_if_smaller(ident_len,FN_REFLEN-1);
  log_ident= buf + header_size;
}
#endif

#if defined(MYSQL_SERVER)
/*
  Access to the current replication position.

  There is a dummy replacement for this in the embedded library that returns
  FALSE; this is used by XtraDB to allow it to access replication stuff while
  still being able to use the same plugin in both stand-alone and embedded.
*/
bool rpl_get_position_info(const char **log_file_name, ulonglong *log_pos,
                           const char **group_relay_log_name,
                           ulonglong *relay_log_pos)
{
#if defined(EMBEDDED_LIBRARY) || !defined(HAVE_REPLICATION)
  return FALSE;
#else
  const Relay_log_info *rli= &(active_mi->rli);
  *log_file_name= rli->group_master_log_name;
  *log_pos= rli->group_master_log_pos +
    (rli->future_event_relay_log_pos - rli->group_relay_log_pos);
  *group_relay_log_name= rli->group_relay_log_name;
  *relay_log_pos= rli->future_event_relay_log_pos;
  return TRUE;
#endif
}
#endif
