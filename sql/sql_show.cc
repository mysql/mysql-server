/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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


/* Function with list databases, tables or fields */

#include "sql_show.h"

#include "mutex_lock.h"                     // Mutex_lock
#include "my_dir.h"                         // MY_DIR
#include "prealloced_array.h"               // Prealloced_array
#include "template_utils.h"                 // delete_container_pointers
#include "auth_common.h"                    // check_grant_db
#include "datadict.h"                       // dd_frm_type
#include "debug_sync.h"                     // DEBUG_SYNC
#include "field.h"                          // Field
#include "filesort.h"                       // filesort_free_buffers
#include "item.h"                           // Item_empty_string
#include "item_cmpfunc.h"                   // Item_cond
#include "log.h"                            // sql_print_warning
#include "mysqld_thd_manager.h"             // Global_THD_manager
#include "opt_trace.h"                      // fill_optimizer_trace_info
#include "protocol.h"                       // Protocol
#include "sp.h"                             // MYSQL_PROC_FIELD_DB
#include "sp_head.h"                        // sp_head
#include "sql_audit.h"                      // audit_global_variable_get
#include "sql_base.h"                       // close_thread_tables
#include "sql_class.h"                      // THD
#include "sql_db.h"                         // check_db_dir_existence
#include "sql_optimizer.h"                  // JOIN
#include "sql_parse.h"                      // command_name
#include "sql_plugin.h"                     // PLUGIN_IS_DELTED
#include "sql_table.h"                      // filename_to_tablename
#include "sql_time.h"                       // interval_type_to_name
#include "sql_tmp_table.h"                  // create_tmp_table
#include "sql_view.h"                       // open_and_read_view
#include "table_trigger_dispatcher.h"       // Table_trigger_dispatcher
#include "trigger.h"                        // Trigger
#include "trigger_chain.h"                  // Trigger_chain
#include "trigger_loader.h"                 // Trigger_loader
#include "tztime.h"                         // Time_zone

#ifndef EMBEDDED_LIBRARY
#include "events.h"                         // Events
#include "event_data_objects.h"             // Event_timed
#include "event_parse_data.h"               // Event_parse_data
#endif

#include "partition_info.h"                 // partition_info
#include "partitioning/partition_handler.h" // Partition_handler

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"
#ifndef EMBEDDED_LIBRARY
#include "srv_session.h"
#endif

#include <algorithm>
#include <functional>
using std::max;
using std::min;

#define STR_OR_NIL(S) ((S) ? (S) : "<nil>")

enum enum_i_s_events_fields
{
  ISE_EVENT_CATALOG= 0,
  ISE_EVENT_SCHEMA,
  ISE_EVENT_NAME,
  ISE_DEFINER,
  ISE_TIME_ZONE,
  ISE_EVENT_BODY,
  ISE_EVENT_DEFINITION,
  ISE_EVENT_TYPE,
  ISE_EXECUTE_AT,
  ISE_INTERVAL_VALUE,
  ISE_INTERVAL_FIELD,
  ISE_SQL_MODE,
  ISE_STARTS,
  ISE_ENDS,
  ISE_STATUS,
  ISE_ON_COMPLETION,
  ISE_CREATED,
  ISE_LAST_ALTERED,
  ISE_LAST_EXECUTED,
  ISE_EVENT_COMMENT,
  ISE_ORIGINATOR,
  ISE_CLIENT_CS,
  ISE_CONNECTION_CL,
  ISE_DB_CL
};


static const LEX_STRING trg_action_time_type_names[]=
{
  { C_STRING_WITH_LEN("BEFORE") },
  { C_STRING_WITH_LEN("AFTER") }
};

static const LEX_STRING trg_event_type_names[]=
{
  { C_STRING_WITH_LEN("INSERT") },
  { C_STRING_WITH_LEN("UPDATE") },
  { C_STRING_WITH_LEN("DELETE") }
};

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static const char *grant_names[]={
  "select","insert","update","delete","create","drop","reload","shutdown",
  "process","file","grant","references","index","alter"};

static TYPELIB grant_types = { sizeof(grant_names)/sizeof(char **),
                               "grant_types",
                               grant_names, NULL};
#endif

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info);

static void get_cs_converted_string_value(THD *thd,
                                          String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs,
                                          bool use_hex);

static void
append_algorithm(TABLE_LIST *table, String *buff);

static Item * make_cond_for_info_schema(Item *cond, TABLE_LIST *table);

/***************************************************************************
** List all table types supported
***************************************************************************/

static size_t make_version_string(char *buf, size_t buf_length, uint version)
{
  return my_snprintf(buf, buf_length, "%d.%d", version>>8,version&0xff);
}

static my_bool show_plugins(THD *thd, plugin_ref plugin,
                            void *arg)
{
  TABLE *table= (TABLE*) arg;
  struct st_mysql_plugin *plug= plugin_decl(plugin);
  struct st_plugin_dl *plugin_dl= plugin_dlib(plugin);
  CHARSET_INFO *cs= system_charset_info;
  char version_buf[20];

  restore_record(table, s->default_values);

  table->field[0]->store(plugin_name(plugin)->str,
                         plugin_name(plugin)->length, cs);

  table->field[1]->store(version_buf,
        make_version_string(version_buf, sizeof(version_buf), plug->version),
        cs);


  switch (plugin_state(plugin)) {
  /* case PLUGIN_IS_FREED: does not happen */
  case PLUGIN_IS_DELETED:
    table->field[2]->store(STRING_WITH_LEN("DELETED"), cs);
    break;
  case PLUGIN_IS_UNINITIALIZED:
    table->field[2]->store(STRING_WITH_LEN("INACTIVE"), cs);
    break;
  case PLUGIN_IS_READY:
    table->field[2]->store(STRING_WITH_LEN("ACTIVE"), cs);
    break;
  case PLUGIN_IS_DISABLED:
    table->field[2]->store(STRING_WITH_LEN("DISABLED"), cs);
    break;
  default:
    DBUG_ASSERT(0);
  }

  table->field[3]->store(plugin_type_names[plug->type].str,
                         plugin_type_names[plug->type].length,
                         cs);
  table->field[4]->store(version_buf,
        make_version_string(version_buf, sizeof(version_buf),
                            *(uint *)plug->info), cs);

  if (plugin_dl)
  {
    table->field[5]->store(plugin_dl->dl.str, plugin_dl->dl.length, cs);
    table->field[5]->set_notnull();
    table->field[6]->store(version_buf,
          make_version_string(version_buf, sizeof(version_buf),
                              plugin_dl->version),
          cs);
    table->field[6]->set_notnull();
  }
  else
  {
    table->field[5]->set_null();
    table->field[6]->set_null();
  }


  if (plug->author)
  {
    table->field[7]->store(plug->author, strlen(plug->author), cs);
    table->field[7]->set_notnull();
  }
  else
    table->field[7]->set_null();

  if (plug->descr)
  {
    table->field[8]->store(plug->descr, strlen(plug->descr), cs);
    table->field[8]->set_notnull();
  }
  else
    table->field[8]->set_null();

  switch (plug->license) {
  case PLUGIN_LICENSE_GPL:
    table->field[9]->store(PLUGIN_LICENSE_GPL_STRING, 
                           strlen(PLUGIN_LICENSE_GPL_STRING), cs);
    break;
  case PLUGIN_LICENSE_BSD:
    table->field[9]->store(PLUGIN_LICENSE_BSD_STRING, 
                           strlen(PLUGIN_LICENSE_BSD_STRING), cs);
    break;
  default:
    table->field[9]->store(PLUGIN_LICENSE_PROPRIETARY_STRING, 
                           strlen(PLUGIN_LICENSE_PROPRIETARY_STRING), cs);
    break;
  }
  table->field[9]->set_notnull();

  table->field[10]->store(
    global_plugin_typelib_names[plugin_load_option(plugin)],
    strlen(global_plugin_typelib_names[plugin_load_option(plugin)]),
    cs);

  return schema_table_store_record(thd, table);
}


int fill_plugins(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_plugins");

  if (plugin_foreach_with_mask(thd, show_plugins, MYSQL_ANY_PLUGIN,
                               ~PLUGIN_IS_FREED, tables->table))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/***************************************************************************
 List all privileges supported
***************************************************************************/

struct show_privileges_st {
  const char *privilege;
  const char *context;
  const char *comment;
};

static struct show_privileges_st sys_privileges[]=
{
  {"Alter", "Tables",  "To alter the table"},
  {"Alter routine", "Functions,Procedures",  "To alter or drop stored functions/procedures"},
  {"Create", "Databases,Tables,Indexes",  "To create new databases and tables"},
  {"Create routine","Databases","To use CREATE FUNCTION/PROCEDURE"},
  {"Create temporary tables","Databases","To use CREATE TEMPORARY TABLE"},
  {"Create view", "Tables",  "To create new views"},
  {"Create user", "Server Admin",  "To create new users"},
  {"Delete", "Tables",  "To delete existing rows"},
  {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
#ifndef EMBEDDED_LIBRARY
  {"Event","Server Admin","To create, alter, drop and execute events"},
#endif
  {"Execute", "Functions,Procedures", "To execute stored routines"},
  {"File", "File access on server",   "To read and write files on the server"},
  {"Grant option",  "Databases,Tables,Functions,Procedures", "To give to other users those privileges you possess"},
  {"Index", "Tables",  "To create or drop indexes"},
  {"Insert", "Tables",  "To insert data into tables"},
  {"Lock tables","Databases","To use LOCK TABLES (together with SELECT privilege)"},
  {"Process", "Server Admin", "To view the plain text of currently executing queries"},
  {"Proxy", "Server Admin", "To make proxy user possible"},
  {"References", "Databases,Tables", "To have references on tables"},
  {"Reload", "Server Admin", "To reload or refresh tables, logs and privileges"},
  {"Replication client","Server Admin","To ask where the slave or master servers are"},
  {"Replication slave","Server Admin","To read binary log events from the master"},
  {"Select", "Tables",  "To retrieve rows from table"},
  {"Show databases","Server Admin","To see all databases with SHOW DATABASES"},
  {"Show view","Tables","To see views with SHOW CREATE VIEW"},
  {"Shutdown","Server Admin", "To shut down the server"},
  {"Super","Server Admin","To use KILL thread, SET GLOBAL, CHANGE MASTER, etc."},
  {"Trigger","Tables", "To use triggers"},
  {"Create tablespace", "Server Admin", "To create/alter/drop tablespaces"},
  {"Update", "Tables",  "To update existing rows"},
  {"Usage","Server Admin","No privileges - allow connect only"},
  {NullS, NullS, NullS}
};

bool mysqld_show_privileges(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->get_protocol();
  DBUG_ENTER("mysqld_show_privileges");

  field_list.push_back(new Item_empty_string("Privilege",10));
  field_list.push_back(new Item_empty_string("Context",15));
  field_list.push_back(new Item_empty_string("Comment",NAME_CHAR_LEN));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  show_privileges_st *privilege= sys_privileges;
  for (privilege= sys_privileges; privilege->privilege ; privilege++)
  {
    protocol->start_row();
    protocol->store(privilege->privilege, system_charset_info);
    protocol->store(privilege->context, system_charset_info);
    protocol->store(privilege->comment, system_charset_info);
    if (protocol->end_row())
      DBUG_RETURN(TRUE);
  }
  my_eof(thd);
  DBUG_RETURN(FALSE);
}


/** Hash of LEX_STRINGs used to search for ignored db directories. */
static HASH ignore_db_dirs_hash;

/** 
  An array of LEX_STRING pointers to collect the options at 
  option parsing time.
*/
typedef Prealloced_array<LEX_STRING *, 16> Ignore_db_dirs_array;
static Ignore_db_dirs_array *ignore_db_dirs_array;

/**
  A value for the read only system variable to show a list of
  ignored directories.
*/
char *opt_ignore_db_dirs= NULL;


/**
  Sets up the data structures for collection of directories at option
  processing time.
  We need to collect the directories in an array first, because
  we need the character sets initialized before setting up the hash.
*/

void ignore_db_dirs_init()
{
  ignore_db_dirs_array= new Ignore_db_dirs_array(key_memory_ignored_db);
}


/**
  Retrieves the key (the string itself) from the LEX_STRING hash members.

  Needed by hash_init().

  @param     data         the data element from the hash
  @param out len_ret      Placeholder to return the length of the key
  @param                  unused
  @return                 a pointer to the key
*/

static uchar *
db_dirs_hash_get_key(const uchar *data, size_t *len_ret,
                     my_bool MY_ATTRIBUTE((unused)))
{
  LEX_STRING *e= (LEX_STRING *) data;

  *len_ret= e->length;
  return (uchar *) e->str;
}


/**
  Wrap a directory name into a LEX_STRING and push it to the array.

  Called at option processing time for each --ignore-db-dir option.

  @param    path  the name of the directory to push
  @return state
  @retval TRUE  failed
  @retval FALSE success
*/

bool
push_ignored_db_dir(char *path)
{
  LEX_STRING *new_elt;
  char *new_elt_buffer;
  size_t path_len= strlen(path);

  if (!path_len || path_len >= FN_REFLEN)
    return true;

  // No need to normalize, it's only a directory name, not a path.
  if (!my_multi_malloc(key_memory_ignored_db,
                       0,
                       &new_elt, sizeof(LEX_STRING),
                       &new_elt_buffer, path_len + 1,
                       NullS))
    return true;
  new_elt->str= new_elt_buffer;
  memcpy(new_elt_buffer, path, path_len);
  new_elt_buffer[path_len]= 0;
  new_elt->length= path_len;
  return ignore_db_dirs_array->push_back(new_elt);
}


/**
  Clean up the directory ignore options accumulated so far.

  Called at option processing time for each --ignore-db-dir option
  with an empty argument.
*/

void
ignore_db_dirs_reset()
{
  my_free_container_pointers(*ignore_db_dirs_array);
}


/**
  Free the directory ignore option variables.

  Called at server shutdown.
*/

void
ignore_db_dirs_free()
{
  if (opt_ignore_db_dirs)
  {
    my_free(opt_ignore_db_dirs);
    opt_ignore_db_dirs= NULL;
  }
  ignore_db_dirs_reset();
  delete ignore_db_dirs_array;
  my_hash_free(&ignore_db_dirs_hash);
}


/**
  Initialize the ignore db directories hash and status variable from
  the options collected in the array.

  Called when option processing is over and the server's in-memory 
  structures are fully initialized.

  @return state
  @retval TRUE  failed
  @retval FALSE success
*/

bool
ignore_db_dirs_process_additions()
{
  size_t len;
  char *ptr;

  DBUG_ASSERT(opt_ignore_db_dirs == NULL);

  if (my_hash_init(&ignore_db_dirs_hash, 
                   lower_case_table_names ?
                     character_set_filesystem : &my_charset_bin,
                   0, 0, 0, db_dirs_hash_get_key,
                   my_free,
                   HASH_UNIQUE,
                   key_memory_ignored_db))
    return true;

  /* len starts from 1 because of the terminating zero. */
  len= 1;
  LEX_STRING **iter;
  for (iter= ignore_db_dirs_array->begin();
       iter != ignore_db_dirs_array->end(); ++iter)
  {
    len+= (*iter)->length + 1;                      // +1 for the comma
  }

  /* No delimiter for the last directory. */
  if (len > 1)
    len--;

  /* +1 the terminating zero */
  ptr= opt_ignore_db_dirs= (char *) my_malloc(key_memory_ignored_db,
                                              len + 1, MYF(0));
  if (!ptr)
    return true;

  /* Make sure we have an empty string to start with. */
  *ptr= 0;

  for (iter= ignore_db_dirs_array->begin();
       iter != ignore_db_dirs_array->end(); ++iter)
  {
    LEX_STRING *dir= *iter;
    if (my_hash_insert(&ignore_db_dirs_hash, (uchar *)dir))
    {
      /* ignore duplicates from the config file */
      if (my_hash_search(&ignore_db_dirs_hash, (uchar *)dir->str, dir->length))
      {
        sql_print_warning("Duplicate ignore-db-dir directory name '%.*s' "
                          "found in the config file(s). Ignoring the duplicate.",
                          (int) dir->length, dir->str);
        /*
          Free the excess element since the array will just be reset at
          the end of the function, not destructed.
        */
        my_free(dir);
        (*iter)= NULL;
        continue;
      }
      return true;
    }
    ptr= my_stpnmov(ptr, dir->str, dir->length);
    /* It's safe to always do, since the last one will be repalced with a 0 */
    *ptr++ = ',';

    /*
      Set the transferred array element to NULL to avoid double free
      in case of error.
    */
    (*iter)= NULL;
  }

  /* get back to the last comma, if there is one */
  if (ptr > opt_ignore_db_dirs)
  {
    ptr--;
    DBUG_ASSERT(*ptr == ',');
  }
  /* make sure the string is terminated */
  DBUG_ASSERT(ptr - opt_ignore_db_dirs <= (ptrdiff_t) len);
  *ptr= 0;

  /* 
    It's OK to empty the array here as the allocated elements are
    referenced through the hash now.
  */
  ignore_db_dirs_array->clear();

  return false;
}


/**
  Check if a directory name is in the hash of ignored directories.

  @return search result
  @retval TRUE  found
  @retval FALSE not found
*/

bool
is_in_ignore_db_dirs_list(const char *directory)
{
  return ignore_db_dirs_hash.records &&
    NULL != my_hash_search(&ignore_db_dirs_hash, (const uchar *) directory, 
                           strlen(directory));
}


/*
  find_files() - find files in a given directory.

  SYNOPSIS
    find_files()
    thd                 thread handler
    files               put found files in this list
    db                  database name to set in TABLE_LIST structure
    path                path to database
    wild                filter for found files
    dir                 read databases in path if TRUE, read .frm files in
                        database otherwise

  RETURN
    FIND_FILES_OK       success
    FIND_FILES_OOM      out of memory error
    FIND_FILES_DIR      no such directory, or directory can't be read
*/


find_files_result
find_files(THD *thd, List<LEX_STRING> *files, const char *db,
           const char *path, const char *wild, bool dir, MEM_ROOT *tmp_mem_root)
{
  uint i;
  MY_DIR *dirp;
  MEM_ROOT **root_ptr= NULL, *old_root= NULL;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint col_access=thd->col_access;
#endif
  size_t wild_length= 0;
  TABLE_LIST table_list;
  DBUG_ENTER("find_files");

  if (wild)
  {
    if (!wild[0])
      wild= 0;
    else
      wild_length= strlen(wild);
  }



  if (!(dirp = my_dir(path,MYF(dir ? MY_WANT_STAT : 0))))
  {
    if (my_errno() == ENOENT)
      my_error(ER_BAD_DB_ERROR, MYF(0), db);
    else
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_CANT_READ_DIR, MYF(0), path,
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
    DBUG_RETURN(FIND_FILES_DIR);
  }

  if (tmp_mem_root)
  {
    root_ptr= my_thread_get_THR_MALLOC();
    old_root= *root_ptr;
    *root_ptr= tmp_mem_root;
  }

  for (i=0 ; i < dirp->number_off_files  ; i++)
  {
    char uname[NAME_LEN + 1];                   /* Unencoded name */
    FILEINFO *file;
    LEX_STRING *file_name= 0;
    size_t file_name_len;
    char *ext;

    file=dirp->dir_entry+i;
    if (dir)
    {                                           /* Return databases */
      /*
        Ignore all the directories having names that start with a  dot (.).
        This covers '.' and '..' and other cases like e.g. '.mysqlgui'.
        Note that since 5.1 database directory names can't start with a
        dot (.) thanks to table name encoding.
      */
      if (file->name[0]  == '.')
        continue;
      if (!MY_S_ISDIR(file->mystat->st_mode))
        continue;

      if (is_in_ignore_db_dirs_list(file->name))
        continue;

    }
    else
    {
        // Return only .frm files which aren't temp files.
      if (my_strcasecmp(system_charset_info, ext=fn_rext(file->name),reg_ext) ||
          is_prefix(file->name, tmp_file_prefix))
        continue;
      *ext=0;
    }

    file_name_len= filename_to_tablename(file->name, uname, sizeof(uname));

    if (wild)
    {
      if (lower_case_table_names)
      {
        if (my_wildcmp(files_charset_info,
                       uname, uname + file_name_len,
                       wild, wild + wild_length,
                       wild_prefix, wild_one,wild_many))
          continue;
      }
      else if (wild_compare(uname, wild, 0))
        continue;
    }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /* Don't show tables where we don't have any privileges */
    if (db && !(col_access & TABLE_ACLS))
    {
      table_list.db= (char*) db;
      table_list.db_length= strlen(db);
      table_list.table_name= uname;
      table_list.table_name_length= file_name_len;
      table_list.grant.privilege=col_access;
      if (check_grant(thd, TABLE_ACLS, &table_list, TRUE, 1, TRUE))
        continue;
    }
#endif
    if (!(file_name= tmp_mem_root ?
                     make_lex_string_root(tmp_mem_root, file_name, uname,
                                          file_name_len, TRUE) :
                     thd->make_lex_string(file_name, uname,
                                          file_name_len, TRUE)) ||
        files->push_back(file_name))
    {
      my_dirend(dirp);
      DBUG_RETURN(FIND_FILES_OOM);
    }
  }
  DBUG_PRINT("info",("found: %d files", files->elements));
  my_dirend(dirp);

  (void) ha_find_files(thd, db, path, wild, dir, files);

  if (tmp_mem_root)
    *root_ptr= old_root;

  DBUG_RETURN(FIND_FILES_OK);
}


/**
   An Internal_error_handler that suppresses errors regarding views'
   underlying tables that occur during privilege checking within SHOW CREATE
   VIEW commands. This happens in the cases when

   - A view's underlying table (e.g. referenced in its SELECT list) does not
     exist or columns of underlying table are altered. There should not be an
     error as no attempt was made to access it per se.

   - Access is denied for some table, column, function or stored procedure
     such as mentioned above. This error gets raised automatically, since we
     can't untangle its access checking from that of the view itself.
 */
class Show_create_error_handler : public Internal_error_handler
{
  TABLE_LIST *m_top_view;
  bool m_handling;
  Security_context *m_sctx;

  char m_view_access_denied_message[MYSQL_ERRMSG_SIZE];
  const char *m_view_access_denied_message_ptr;

public:

  /**
     Creates a new Show_create_error_handler for the particular security
     context and view.

     @thd Thread context, used for security context information if needed.
     @top_view The view. We do not verify at this point that top_view is in
     fact a view since, alas, these things do not stay constant.
  */
  explicit Show_create_error_handler(THD *thd, TABLE_LIST *top_view) :
    m_top_view(top_view), m_handling(false),
    m_view_access_denied_message_ptr(NULL)
  {
    m_sctx = MY_TEST(m_top_view->security_ctx) ?
      m_top_view->security_ctx : thd->security_context();
  }

private:
  /**
     Lazy instantiation of 'view access denied' message. The purpose of the
     Show_create_error_handler is to hide details of underlying tables for
     which we have no privileges behind ER_VIEW_INVALID messages. But this
     obviously does not apply if we lack privileges on the view itself.
     Unfortunately the information about for which table privilege checking
     failed is not available at this point. The only way for us to check is by
     reconstructing the actual error message and see if it's the same.
  */
  const char* get_view_access_denied_message()
  {
    if (!m_view_access_denied_message_ptr)
    {
      m_view_access_denied_message_ptr= m_view_access_denied_message;
      my_snprintf(m_view_access_denied_message, MYSQL_ERRMSG_SIZE,
                  ER(ER_TABLEACCESS_DENIED_ERROR), "SHOW VIEW",
                  m_sctx->priv_user().str,
                  m_sctx->host_or_ip().str, m_top_view->get_table_name());
    }
    return m_view_access_denied_message_ptr;
  }

public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char *sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char *msg)
  {
    /*
       The handler does not handle the errors raised by itself.
       At this point we know if top_view is really a view.
    */
    if (m_handling || !m_top_view->is_view())
      return false;

    m_handling= true;

    bool is_handled;

    switch (sql_errno)
    {
    case ER_TABLEACCESS_DENIED_ERROR:
      if (!strcmp(get_view_access_denied_message(), msg))
      {
        /* Access to top view is not granted, don't interfere. */
        is_handled= false;
        break;
      }
      // Fall through
    case ER_COLUMNACCESS_DENIED_ERROR:
    // ER_VIEW_NO_EXPLAIN cannot happen here.
    case ER_PROCACCESS_DENIED_ERROR:
      is_handled= true;
      break;

    case ER_BAD_FIELD_ERROR:
      /*
        Established behavior: warn if column of underlying table is altered.
      */
    case ER_NO_SUCH_TABLE:
      /* Established behavior: warn if underlying tables are missing. */
    case ER_SP_DOES_NOT_EXIST:
      /* Established behavior: warn if underlying functions are missing. */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_VIEW_INVALID,
                          ER(ER_VIEW_INVALID),
                          m_top_view->get_db_name(),
                          m_top_view->get_table_name());
      is_handled= true;
      break;
    default:
      is_handled= false;
    }

    m_handling= false;
    return is_handled;
  }
};


class Silence_deprecation_warnings : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_WARN_DEPRECATED_SYNTAX)
      return true;

    return false;
  }
};


bool
mysqld_show_create(THD *thd, TABLE_LIST *table_list)
{
  Protocol *protocol= thd->get_protocol();
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  List<Item> field_list;
  bool error= TRUE;
  DBUG_ENTER("mysqld_show_create");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->table_name));

  /*
    Metadata locks taken during SHOW CREATE should be released when
    the statmement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();

  /* We want to preserve the tree for views. */
  thd->lex->context_analysis_only|= CONTEXT_ANALYSIS_ONLY_VIEW;

  {
    /*
      If there is an error during processing of an underlying view, an
      error message is wanted, but it has to be converted to a warning,
      so that execution can continue.
      This is handled by the Show_create_error_handler class.

      Use open_tables() instead of open_tables_for_query(). If an error occurs,
      this will ensure that tables are not closed on error, but remain open
      for the rest of the processing of the SHOW statement.
    */
    Show_create_error_handler view_error_suppressor(thd, table_list);
    thd->push_internal_handler(&view_error_suppressor);

    /*
      Filter out deprecation warnings caused by deprecation of
      the partition engine. The presence of these depend on TDC
      cache behavior. Instead, push a warning later to get
      deterministic and repeatable behavior.
    */
    Silence_deprecation_warnings deprecation_silencer;
    thd->push_internal_handler(&deprecation_silencer);

    uint counter;
    bool open_error= open_tables(thd, &table_list, &counter,
                                 MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL);
    if (!open_error && table_list->is_view_or_derived())
    {
      /*
        Prepare result table for view so that we can read the column list.
        Notice that Show_create_error_handler remains active, so that any
        errors due to missing underlying objects are converted to warnings.
      */
      open_error= table_list->resolve_derived(thd, true);
    }
    thd->pop_internal_handler();
    thd->pop_internal_handler();
    if (open_error && (thd->killed || thd->is_error()))
      goto exit;
  }

  /* TODO: add environment variables show when it become possible */
  if (thd->lex->only_view && !table_list->is_view())
  {
    my_error(ER_WRONG_OBJECT, MYF(0),
             table_list->db, table_list->table_name, "VIEW");
    goto exit;
  }

  buffer.length(0);

  if (table_list->is_view())
    buffer.set_charset(table_list->view_creation_ctx->get_client_cs());

  /*
    Push deprecation warnings for non-natively partitioned tables. Done here
    instead of in open_binary_frm (silenced by error handler) to get
    predictable and repeatable results without having to flush tables.
  */
  if (!table_list->is_view() && table_list->table->s->db_type() &&
      is_ha_partition_handlerton(table_list->table->s->db_type()))
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_DEPRECATED_SYNTAX,
                        ER_THD(thd,
                               ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                        table_list->db, table_list->table_name);

  if ((table_list->is_view() ?
       view_store_create_info(thd, table_list, &buffer) :
       store_create_info(thd, table_list, &buffer, NULL,
                         FALSE /* show_database */)))
    goto exit;

  if (table_list->is_view())
  {
    field_list.push_back(new Item_empty_string("View",NAME_CHAR_LEN));
    field_list.push_back(new Item_empty_string("Create View",
                                               max<uint>(buffer.length(), 1024U)));
    field_list.push_back(new Item_empty_string("character_set_client",
                                               MY_CS_NAME_SIZE));
    field_list.push_back(new Item_empty_string("collation_connection",
                                               MY_CS_NAME_SIZE));
  }
  else
  {
    field_list.push_back(new Item_empty_string("Table",NAME_CHAR_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string("Create Table",
                                               max<size_t>(buffer.length(), 1024U)));
  }

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    goto exit;

  protocol->start_row();
  if (table_list->is_view())
    protocol->store(table_list->view_name.str, system_charset_info);
  else
  {
    if (table_list->schema_table)
      protocol->store(table_list->schema_table->table_name,
                      system_charset_info);
    else
      protocol->store(table_list->table->alias, system_charset_info);
  }

  if (table_list->is_view())
  {
    protocol->store(buffer.ptr(), buffer.length(),
                    table_list->view_creation_ctx->get_client_cs());

    protocol->store(table_list->view_creation_ctx->get_client_cs()->csname,
                    system_charset_info);

    protocol->store(table_list->view_creation_ctx->get_connection_cl()->name,
                    system_charset_info);
  }
  else
    protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row())
    goto exit;

  error= FALSE;
  my_eof(thd);

exit:
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  DBUG_RETURN(error);
}

bool mysqld_show_create_db(THD *thd, char *dbname,
                           HA_CREATE_INFO *create_info)
{
  char buff[2048], orig_dbname[NAME_LEN];
  String buffer(buff, sizeof(buff), system_charset_info);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *sctx= thd->security_context();
  uint db_access;
#endif
  HA_CREATE_INFO create;
  uint create_options = create_info ? create_info->options : 0;
  Protocol *protocol=thd->get_protocol();
  DBUG_ENTER("mysql_show_create_db");

  strcpy(orig_dbname, dbname);
  if (lower_case_table_names && dbname != any_db)
    my_casedn_str(files_charset_info, dbname);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (sctx->check_access(DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(sctx->host().str, sctx->ip().str,
                        sctx->priv_user().str, dbname, 0) |
                sctx->master_access());
  if (!(db_access & DB_ACLS) && check_grant_db(thd,dbname))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user().str, sctx->host_or_ip().str, dbname);
    query_logger.general_log_print(thd,COM_INIT_DB,ER(ER_DBACCESS_DENIED_ERROR),
                                   sctx->priv_user().str,
                                   sctx->host_or_ip().str, dbname);
    DBUG_RETURN(TRUE);
  }
#endif
  if (is_infoschema_db(dbname))
  {
    dbname= INFORMATION_SCHEMA_NAME.str;
    create.default_table_charset= system_charset_info;
  }
  else
  {
    if (check_db_dir_existence(dbname))
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), dbname);
      DBUG_RETURN(TRUE);
    }

    load_db_opt_by_name(thd, dbname, &create);
  }
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database",NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database",1024));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  protocol->start_row();
  protocol->store(orig_dbname, strlen(orig_dbname), system_charset_info);
  buffer.length(0);
  buffer.append(STRING_WITH_LEN("CREATE DATABASE "));
  if (create_options & HA_LEX_CREATE_IF_NOT_EXISTS)
    buffer.append(STRING_WITH_LEN("/*!32312 IF NOT EXISTS*/ "));
  append_identifier(thd, &buffer, orig_dbname, strlen(orig_dbname));

  if (create.default_table_charset)
  {
    buffer.append(STRING_WITH_LEN(" /*!40100"));
    buffer.append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    buffer.append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY))
    {
      buffer.append(STRING_WITH_LEN(" COLLATE "));
      buffer.append(create.default_table_charset->name);
    }
    buffer.append(STRING_WITH_LEN(" */"));
  }
  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row())
    DBUG_RETURN(TRUE);
  my_eof(thd);
  DBUG_RETURN(FALSE);
}



/****************************************************************************
  Return only fields for API mysql_list_fields
  Use "show table wildcard" in mysql instead of this
****************************************************************************/

void
mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild)
{
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter",("table: %s",table_list->table_name));

  if (open_tables_for_query(thd, table_list,
                            MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL))
    DBUG_VOID_RETURN;

  if (table_list->is_view_or_derived())
  {
    // Setup materialized result table so that we can read the column list
    if (table_list->resolve_derived(thd, false))
      DBUG_VOID_RETURN;                /* purecov: inspected */
    if (table_list->setup_materialized_derived(thd))
      DBUG_VOID_RETURN;                /* purecov: inspected */
  }
  TABLE *table= table_list->table;

  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
    {
      if (table_list->is_view())
        field_list.push_back(new Item_ident_for_show(field,
                                                     table_list->view_db.str,
                                                     table_list->view_name.str));
      else
        field_list.push_back(new Item_field(field));
    }
  }
  restore_record(table, s->default_values);              // Get empty record
  table->use_all_columns();
  if (thd->send_result_metadata(&field_list, Protocol::SEND_DEFAULTS))
    DBUG_VOID_RETURN;
  my_eof(thd);
  DBUG_VOID_RETURN;
}

/*
  Go through all character combinations and ensure that sql_lex.cc can
  parse it as an identifier.

  SYNOPSIS
  require_quotes()
  name			attribute name
  name_length		length of name

  RETURN
    #	Pointer to conflicting character
    0	No conflicting character
*/

static const char *require_quotes(const char *name, size_t name_length)
{
  bool pure_digit= TRUE;
  const char *end= name + name_length;

  for (; name < end ; name++)
  {
    uchar chr= (uchar) *name;
    uint length= my_mbcharlen(system_charset_info, chr);
    if (length == 0 || (length == 1 && !system_charset_info->ident_map[chr]))
      return name;
    if (length == 1 && (chr < '0' || chr > '9'))
      pure_digit= FALSE;
  }
  if (pure_digit)
    return name;
  return 0;
}


/**
  Convert and quote the given identifier if needed and append it to the
  target string. If the given identifier is empty, it will be quoted.

  @thd                         thread handler
  @packet                      target string
  @name                        the identifier to be appended
  @length                      length of the appending identifier
  @param from_cs               Charset information about the input string
  @param to_cs                 Charset information about the target string
*/

void
append_identifier(THD *thd, String *packet, const char *name, size_t length,
                  const CHARSET_INFO *from_cs, const CHARSET_INFO *to_cs)
{
  const char *name_end;
  char quote_char;
  int q;

  const CHARSET_INFO *cs_info= system_charset_info;
  const char *to_name= name;
  size_t to_length= length;
  String to_string(name,length, from_cs);

  if (from_cs != NULL && to_cs != NULL && from_cs != to_cs)
    thd->convert_string(&to_string, from_cs, to_cs);

  if (to_cs != NULL)
  {
    to_name= to_string.c_ptr();
    to_length= to_string.length();
    cs_info= to_cs;
  }

  q= thd != NULL ? get_quote_char_for_identifier(thd, to_name, to_length) :
                   '`';

  if (q == EOF)
  {
    packet->append(to_name, to_length, packet->charset());
    return;
  }

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  (void) packet->reserve(to_length*2 + 2);
  quote_char= (char) q;
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end= to_name+to_length ; to_name < name_end ; to_name+= to_length)
  {
    uchar chr= static_cast<uchar>(*to_name);
    to_length= my_mbcharlen(cs_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!to_length)
      to_length= 1;
    if (to_length == 1 && chr == static_cast<uchar>(quote_char))
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(to_name, to_length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}

/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()
    thd		Thread handler
    name	name to quote
    length	length of name

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier(THD *thd, const char *name, size_t length)
{
  if (length &&
      !is_keyword(name,length) &&
      !require_quotes(name, length) &&
      !(thd->variables.option_bits & OPTION_QUOTE_SHOW_CREATE))
    return EOF;
  if (thd->variables.sql_mode & MODE_ANSI_QUOTES)
    return '"';
  return '`';
}


/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
			     const char *filename)
{
  if (filename && !(thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE))
  {
    size_t length= dirname_length(filename);
    packet->append(' ');
    packet->append(dir_type);
    packet->append(STRING_WITH_LEN(" DIRECTORY='"));
#ifdef _WIN32
    /* Convert \ to / to be able to create table on unix */
    char *winfilename= (char*) thd->memdup(filename, length);
    char *pos, *end;
    for (pos= winfilename, end= pos+length ; pos < end ; pos++)
    {
      if (*pos == '\\')
        *pos = '/';
    }
    filename= winfilename;
#endif
    packet->append(filename, length);
    packet->append('\'');
  }
}


#define LIST_PROCESS_HOST_LEN 64

/**
  Print "ON UPDATE" clause of a field into a string.

  @param timestamp_field   Pointer to timestamp field of a table.
  @param field             The field to generate ON UPDATE clause for.
  @bool  lcase             Whether to print in lower case.
  @return                  false on success, true on error.
*/
static bool print_on_update_clause(Field *field, String *val, bool lcase)
{
  DBUG_ASSERT(val->charset()->mbminlen == 1);
  val->length(0);
  if (field->has_update_default_function())
  {
    if (lcase)
      val->copy(STRING_WITH_LEN("on update "), val->charset());
    else
      val->copy(STRING_WITH_LEN("ON UPDATE "), val->charset());
    val->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
    if (field->decimals() > 0)
      val->append_parenthesized(field->decimals());
    return true;
  }
  return false;
}


static bool print_default_clause(THD *thd, Field *field, String *def_value,
                                 bool quoted)
{
  enum enum_field_types field_type= field->type();

  const bool has_now_default= field->has_insert_default_function();
  const bool has_default=
    (field_type != FIELD_TYPE_BLOB &&
     !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
     field->unireg_check != Field::NEXT_NUMBER &&
     !((thd->variables.sql_mode & (MODE_MYSQL323 | MODE_MYSQL40))
       && has_now_default));

  if (field->gcol_info)
    return false;

  def_value->length(0);
  if (has_default)
  {
    if (has_now_default)
      /*
        We are using CURRENT_TIMESTAMP instead of NOW because it is the SQL
        standard.
      */
    {
      def_value->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
      if (field->decimals() > 0)
        def_value->append_parenthesized(field->decimals());
    }
    else if (!field->is_null())
    {                                             // Not null by default
      char tmp[MAX_FIELD_WIDTH];
      String type(tmp, sizeof(tmp), field->charset());
      if (field_type == MYSQL_TYPE_BIT)
      {
        longlong dec= field->val_int();
        char *ptr= longlong2str(dec, tmp + 2, 2);
        uint32 length= (uint32) (ptr - tmp);
        tmp[0]= 'b';
        tmp[1]= '\'';        
        tmp[length]= '\'';
        type.length(length + 1);
        quoted= 0;
      }
      else
        field->val_str(&type);
      if (type.length())
      {
        String def_val;
        uint dummy_errors;
        /* convert to system_charset_info == utf8 */
        def_val.copy(type.ptr(), type.length(), field->charset(),
                     system_charset_info, &dummy_errors);
        if (quoted)
          append_unescaped(def_value, def_val.ptr(), def_val.length());
        else
          def_value->append(def_val.ptr(), def_val.length());
      }
      else if (quoted)
        def_value->append(STRING_WITH_LEN("''"));
    }
    else if (field->maybe_null() && quoted)
      def_value->append(STRING_WITH_LEN("NULL"));    // Null as default
    else
      return 0;

  }
  return has_default;
}


/*
  Build a CREATE TABLE statement for a table.

  SYNOPSIS
    store_create_info()
    thd               The thread
    table_list        A list containing one table to write statement
                      for.
    packet            Pointer to a string where statement will be
                      written.
    create_info_arg   Pointer to create information that can be used
                      to tailor the format of the statement.  Can be
                      NULL, in which case only SQL_MODE is considered
                      when building the statement.
    show_database     If true, then print the database before the table
                      name. The database name is only printed in the event
                      that it is different from the current database.
                      If false, then do not print the database before
                      the table name.
  
  NOTE
    Currently always return 0, but might return error code in the
    future.
    
  RETURN
    0       OK
 */

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO *create_info_arg, bool show_database)
{
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, buff[128], def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr,*field;
  uint primary_key;
  KEY *key_info;
  TABLE *table= table_list->table;
  handler *file= table->file;
  TABLE_SHARE *share= table->s;
  HA_CREATE_INFO create_info;
  bool show_table_options= FALSE;
  bool foreign_db_mode=  (thd->variables.sql_mode & (MODE_POSTGRESQL |
                                                     MODE_ORACLE |
                                                     MODE_MSSQL |
                                                     MODE_DB2 |
                                                     MODE_MAXDB |
                                                     MODE_ANSI)) != 0;
  bool limited_mysql_mode= (thd->variables.sql_mode & (MODE_NO_FIELD_OPTIONS |
                                                       MODE_MYSQL323 |
                                                       MODE_MYSQL40)) != 0;
  my_bitmap_map *old_map;
  int error= 0;
  DBUG_ENTER("store_create_info");
  DBUG_PRINT("enter",("table: %s", table->s->table_name.str));

  restore_record(table, s->default_values); // Get empty record

  if (share->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (create_info_arg &&
      (create_info_arg->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (table_list->schema_table)
    alias= table_list->schema_table->table_name;
  else
  {
    if (lower_case_table_names == 2)
      alias= table->alias;
    else
    {
      alias= share->table_name.str;
    }
  }

  /*
    Print the database before the table name if told to do that. The
    database name is only printed in the event that it is different
    from the current database.  The main reason for doing this is to
    avoid having to update gazillions of tests and result files, but
    it also saves a few bytes of the binary log.
   */
  if (show_database)
  {

    const LEX_STRING *const db=
      table_list->schema_table ? &INFORMATION_SCHEMA_NAME : &table->s->db;
    if (!thd->db().str || strcmp(db->str, thd->db().str))
    {
      append_identifier(thd, packet, db->str, db->length);
      packet->append(STRING_WITH_LEN("."));
    }
  }

  append_identifier(thd, packet, alias, strlen(alias));
  packet->append(STRING_WITH_LEN(" (\n"));
  /*
    We need this to get default values from the table
    We have to restore the read_set if we are called from insert in case
    of row based replication.
  */
  old_map= tmp_use_all_columns(table, table->read_set);

  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    uint flags = field->flags;
    enum_field_types field_type= field->real_type();

    if (ptr != table->field)
      packet->append(STRING_WITH_LEN(",\n"));

    packet->append(STRING_WITH_LEN("  "));
    append_identifier(thd,packet,field->field_name, strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    /*
      If the session variable 'show_old_temporals' is enabled and the field
      is a temporal type of old format, add a comment to indicate the same.
    */
    if (thd->variables.show_old_temporals &&
        (field_type == MYSQL_TYPE_TIME || field_type == MYSQL_TYPE_DATETIME ||
         field_type == MYSQL_TYPE_TIMESTAMP))
      type.append(" /* 5.5 binary format */");
    packet->append(type.ptr(), type.length(), system_charset_info);

    if (field->has_charset() && 
        !(thd->variables.sql_mode & (MODE_MYSQL323 | MODE_MYSQL40)))
    {
      if (field->charset() != share->table_charset)
      {
	packet->append(STRING_WITH_LEN(" CHARACTER SET "));
	packet->append(field->charset()->csname);
      }
      /* 
	For string types dump collation name only if 
	collation is not primary for the given charset
      */
      if (!(field->charset()->state & MY_CS_PRIMARY))
      {
	packet->append(STRING_WITH_LEN(" COLLATE "));
	packet->append(field->charset()->name);
      }
    }

    if (field->gcol_info)
    {
      packet->append(STRING_WITH_LEN(" GENERATED ALWAYS"));
      packet->append(STRING_WITH_LEN(" AS ("));
      packet->append(field->gcol_info->expr_str.str,
                     field->gcol_info->expr_str.length,
                     system_charset_info);
      packet->append(STRING_WITH_LEN(")"));
      if (field->stored_in_db)
        packet->append(STRING_WITH_LEN(" STORED"));
      else
        packet->append(STRING_WITH_LEN(" VIRTUAL"));
    }

    if (flags & NOT_NULL_FLAG)
      packet->append(STRING_WITH_LEN(" NOT NULL"));
    else if (field->type() == MYSQL_TYPE_TIMESTAMP)
    {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(STRING_WITH_LEN(" NULL"));
    }

    switch(field->field_storage_type()){
    case HA_SM_DEFAULT:
      break;
    case HA_SM_DISK:
      packet->append(STRING_WITH_LEN(" /*!50606 STORAGE DISK */"));
      break;
    case HA_SM_MEMORY:
      packet->append(STRING_WITH_LEN(" /*!50606 STORAGE MEMORY */"));
      break;
    default:
      DBUG_ASSERT(0);
      break;
    }

    switch(field->column_format()){
    case COLUMN_FORMAT_TYPE_DEFAULT:
      break;
    case COLUMN_FORMAT_TYPE_FIXED:
      packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT FIXED */"));
      break;
    case COLUMN_FORMAT_TYPE_DYNAMIC:
      packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT DYNAMIC */"));
      break;
    default:
      DBUG_ASSERT(0);
      break;
    }

    if (print_default_clause(thd, field, &def_value, true))
    {
      packet->append(STRING_WITH_LEN(" DEFAULT "));
      packet->append(def_value.ptr(), def_value.length(), system_charset_info);
    }

    if (!limited_mysql_mode &&
        print_on_update_clause(field, &def_value, false))
    {
      packet->append(STRING_WITH_LEN(" "));
      packet->append(def_value);
    }

    if (field->unireg_check == Field::NEXT_NUMBER && 
        !(thd->variables.sql_mode & MODE_NO_FIELD_OPTIONS))
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT"));

    if (field->comment.length)
    {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, field->comment.str, field->comment.length);
    }
  }

  key_info= table->key_info;
  /* Allow update_create_info to update row type */
  create_info.row_type= share->row_type;
  file->update_create_info(&create_info);
  primary_key= share->primary_key;

  for (uint i=0 ; i < share->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    bool found_primary=0;
    packet->append(STRING_WITH_LEN(",\n  "));

    if (i == primary_key && !strcmp(key_info->name, primary_key_name))
    {
      found_primary=1;
      /*
        No space at end, because a space will be added after where the
        identifier would go, but that is not added for primary key.
      */
      packet->append(STRING_WITH_LEN("PRIMARY KEY"));
    }
    else if (key_info->flags & HA_NOSAME)
      packet->append(STRING_WITH_LEN("UNIQUE KEY "));
    else if (key_info->flags & HA_FULLTEXT)
      packet->append(STRING_WITH_LEN("FULLTEXT KEY "));
    else if (key_info->flags & HA_SPATIAL)
      packet->append(STRING_WITH_LEN("SPATIAL KEY "));
    else
      packet->append(STRING_WITH_LEN("KEY "));

    if (!found_primary)
     append_identifier(thd, packet, key_info->name, strlen(key_info->name));

    packet->append(STRING_WITH_LEN(" ("));

    for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
    {
      if (j)
        packet->append(',');

      if (key_part->field)
        append_identifier(thd,packet,key_part->field->field_name,
			  strlen(key_part->field->field_name));
      if (key_part->field &&
          (key_part->length !=
           table->field[key_part->fieldnr-1]->key_length() &&
           !(key_info->flags & (HA_FULLTEXT | HA_SPATIAL))))
      {
        packet->append_parenthesized((long) key_part->length /
                                      key_part->field->charset()->mbmaxlen);
      }
    }
    packet->append(')');
    store_key_options(thd, packet, table, key_info);
    if (key_info->parser)
    {
      LEX_STRING *parser_name= plugin_name(key_info->parser);
      packet->append(STRING_WITH_LEN(" /*!50100 WITH PARSER "));
      append_identifier(thd, packet, parser_name->str, parser_name->length);
      packet->append(STRING_WITH_LEN(" */ "));
    }
  }

  /*
    Get possible foreign key definitions stored in InnoDB and append them
    to the CREATE TABLE statement
  */

  if ((for_str= file->get_foreign_key_create_info()))
  {
    packet->append(for_str, strlen(for_str));
    file->free_foreign_key_create_info(for_str);
  }

  packet->append(STRING_WITH_LEN("\n)"));
  if (!(thd->variables.sql_mode & MODE_NO_TABLE_OPTIONS) && !foreign_db_mode)
  {
    show_table_options= TRUE;

    /* TABLESPACE and STORAGE */
    if (share->tablespace ||
        share->default_storage_media != HA_SM_DEFAULT)
    {
      packet->append(STRING_WITH_LEN(" /*!50100"));
      if (share->tablespace)
      {
        packet->append(STRING_WITH_LEN(" TABLESPACE "));
        append_identifier(thd, packet, share->tablespace,
                          strlen(share->tablespace));
      }

      if (share->default_storage_media == HA_SM_DISK)
        packet->append(STRING_WITH_LEN(" STORAGE DISK"));
      if (share->default_storage_media == HA_SM_MEMORY)
        packet->append(STRING_WITH_LEN(" STORAGE MEMORY"));

      packet->append(STRING_WITH_LEN(" */"));
    }

    /*
      IF   check_create_info
      THEN add ENGINE only if it was used when creating the table
    */
    if (!create_info_arg ||
        (create_info_arg->used_fields & HA_CREATE_USED_ENGINE))
    {
      if (thd->variables.sql_mode & (MODE_MYSQL323 | MODE_MYSQL40))
        packet->append(STRING_WITH_LEN(" TYPE="));
      else
        packet->append(STRING_WITH_LEN(" ENGINE="));
      /*
        TODO: Replace this if with the else branch. Not done yet since
        NDB handlerton says "ndbcluster" and ha_ndbcluster says "NDBCLUSTER".
      */
      if (table->part_info)
      {
        packet->append(ha_resolve_storage_engine_name(
                       table->part_info->default_engine_type));
      }
      else
      {
        packet->append(file->table_type());
      }
    }

    /*
      Add AUTO_INCREMENT=... if there is an AUTO_INCREMENT column,
      and NEXT_ID > 1 (the default).  We must not print the clause
      for engines that do not support this as it would break the
      import of dumps, but as of this writing, the test for whether
      AUTO_INCREMENT columns are allowed and wether AUTO_INCREMENT=...
      is supported is identical, !(file->table_flags() & HA_NO_AUTO_INCREMENT))
      Because of that, we do not explicitly test for the feature,
      but may extrapolate its existence from that of an AUTO_INCREMENT column.
    */

    if (create_info.auto_increment_value > 1)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT="));
      end= longlong10_to_str(create_info.auto_increment_value, buff,10);
      packet->append(buff, (uint) (end - buff));
    }
    
    if (share->table_charset &&
	!(thd->variables.sql_mode & MODE_MYSQL323) &&
	!(thd->variables.sql_mode & MODE_MYSQL40))
    {
      /*
        IF   check_create_info
        THEN add DEFAULT CHARSET only if it was used when creating the table
      */
      if (!create_info_arg ||
          (create_info_arg->used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
      {
        packet->append(STRING_WITH_LEN(" DEFAULT CHARSET="));
        packet->append(share->table_charset->csname);
        if (!(share->table_charset->state & MY_CS_PRIMARY))
        {
          packet->append(STRING_WITH_LEN(" COLLATE="));
          packet->append(table->s->table_charset->name);
        }
      }
    }

    if (share->min_rows)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" MIN_ROWS="));
      end= longlong10_to_str(share->min_rows, buff, 10);
      packet->append(buff, (uint) (end- buff));
    }

    if (share->max_rows && !table_list->schema_table)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" MAX_ROWS="));
      end= longlong10_to_str(share->max_rows, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }

    if (share->avg_row_length)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" AVG_ROW_LENGTH="));
      end= longlong10_to_str(share->avg_row_length, buff,10);
      packet->append(buff, (uint) (end - buff));
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=1"));
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=0"));
    if (share->db_create_options & HA_OPTION_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=1"));
    if (share->db_create_options & HA_OPTION_NO_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=0"));
    if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_ON)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=1"));
    else if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_OFF)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=0"));
    if (share->stats_sample_pages != 0)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" STATS_SAMPLE_PAGES="));
      end= longlong10_to_str(share->stats_sample_pages, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }
    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compability */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      packet->append(STRING_WITH_LEN(" CHECKSUM=1"));
    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      packet->append(STRING_WITH_LEN(" DELAY_KEY_WRITE=1"));

    /*
      If 'show_create_table_verbosity' is enabled, the row format would
      be displayed in the output of SHOW CREATE TABLE even if default
      row format is used. Otherwise only the explicitly mentioned
      row format would be displayed.
    */
    if (thd->variables.show_create_table_verbosity)
    {
      enum row_type row_type = file->get_row_type();
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      if (row_type == ROW_TYPE_NOT_USED || row_type == ROW_TYPE_DEFAULT)
      {
        row_type= ((share->db_options_in_use & HA_OPTION_COMPRESS_RECORD) ?
                   ROW_TYPE_COMPRESSED :
                   (share->db_options_in_use & HA_OPTION_PACK_RECORD) ?
                   ROW_TYPE_DYNAMIC : ROW_TYPE_FIXED);
      }
      packet->append(ha_row_type[(uint) row_type]);
    }
    else if (create_info.row_type != ROW_TYPE_DEFAULT)
    {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint) create_info.row_type]);
    }

    if (table->s->key_block_size)
    {
      char *end;
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end= longlong10_to_str(table->s->key_block_size, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }
    if (table->s->compress.length)
    {
      packet->append(STRING_WITH_LEN(" COMPRESSION="));
      append_unescaped(packet, share->compress.str, share->compress.length);
    }
    if (table->s->encrypt_type.length)
    {
      packet->append(STRING_WITH_LEN(" ENCRYPTION="));
      append_unescaped(packet, share->encrypt_type.str, share->encrypt_type.length);
    }
    table->file->append_create_info(packet);
    if (share->comment.length)
    {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, share->comment.str, share->comment.length);
    }
    if (share->connect_string.length)
    {
      packet->append(STRING_WITH_LEN(" CONNECTION="));
      append_unescaped(packet, share->connect_string.str, share->connect_string.length);
    }
    append_directory(thd, packet, "DATA",  create_info.data_file_name);
    append_directory(thd, packet, "INDEX", create_info.index_file_name);
  }
  {
    if (table->part_info &&
        !(table->s->db_type()->partition_flags &&
	  (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION) &&
          table->part_info->is_auto_partitioned))
    {
      /*
        Partition syntax for CREATE TABLE is at the end of the syntax.
      */
      uint part_syntax_len;
      char *part_syntax;
      String comment_start;
      table->part_info->set_show_version_string(&comment_start);
      if ((part_syntax= generate_partition_syntax(table->part_info,
                                                  &part_syntax_len,
                                                  FALSE,
                                                  show_table_options,
                                                  NULL, NULL,
                                                  comment_start.c_ptr())))
      {
         packet->append(comment_start);
         if (packet->append(part_syntax, part_syntax_len) ||
             packet->append(STRING_WITH_LEN(" */")))
          error= 1;
         my_free(part_syntax);
      }
    }
  }
  tmp_restore_column_map(table->read_set, old_map);
  DBUG_RETURN(error);
}


static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info)
{
  bool limited_mysql_mode= (thd->variables.sql_mode &
                            (MODE_NO_FIELD_OPTIONS | MODE_MYSQL323 |
                             MODE_MYSQL40)) != 0;
  bool foreign_db_mode=  (thd->variables.sql_mode & (MODE_POSTGRESQL |
                                                     MODE_ORACLE |
                                                     MODE_MSSQL |
                                                     MODE_DB2 |
                                                     MODE_MAXDB |
                                                     MODE_ANSI)) != 0;
  char *end, buff[32];

  if (!(thd->variables.sql_mode & MODE_NO_KEY_OPTIONS) &&
      !limited_mysql_mode && !foreign_db_mode)
  {

    if (key_info->algorithm == HA_KEY_ALG_BTREE)
      packet->append(STRING_WITH_LEN(" USING BTREE"));

    if (key_info->algorithm == HA_KEY_ALG_HASH)
      packet->append(STRING_WITH_LEN(" USING HASH"));

    /* send USING only in non-default case: non-spatial rtree */
    if ((key_info->algorithm == HA_KEY_ALG_RTREE) &&
        !(key_info->flags & HA_SPATIAL))
      packet->append(STRING_WITH_LEN(" USING RTREE"));

    if ((key_info->flags & HA_USES_BLOCK_SIZE) &&
        table->s->key_block_size != key_info->block_size)
    {
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end= longlong10_to_str(key_info->block_size, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }
    DBUG_ASSERT(MY_TEST(key_info->flags & HA_USES_COMMENT) == 
               (key_info->comment.length > 0));
    if (key_info->flags & HA_USES_COMMENT)
    {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, key_info->comment.str, 
                       key_info->comment.length);
    }
  }
}


void
view_store_options(THD *thd, TABLE_LIST *table, String *buff)
{
  append_algorithm(table, buff);
  append_definer(thd, buff, table->definer.user, table->definer.host);
  if (table->view_suid)
    buff->append(STRING_WITH_LEN("SQL SECURITY DEFINER "));
  else
    buff->append(STRING_WITH_LEN("SQL SECURITY INVOKER "));
}


/*
  Append DEFINER clause to the given buffer.
  
  SYNOPSIS
    append_definer()
    thd           [in] thread handle
    buffer        [inout] buffer to hold DEFINER clause
    definer_user  [in] user name part of definer
    definer_host  [in] host name part of definer
*/

static void append_algorithm(TABLE_LIST *table, String *buff)
{
  buff->append(STRING_WITH_LEN("ALGORITHM="));
  switch ((int8)table->algorithm) {
  case VIEW_ALGORITHM_UNDEFINED:
    buff->append(STRING_WITH_LEN("UNDEFINED "));
    break;
  case VIEW_ALGORITHM_TEMPTABLE:
    buff->append(STRING_WITH_LEN("TEMPTABLE "));
    break;
  case VIEW_ALGORITHM_MERGE:
    buff->append(STRING_WITH_LEN("MERGE "));
    break;
  default:
    DBUG_ASSERT(0); // never should happen
  }
}

/*
  Append DEFINER clause to the given buffer.
  
  SYNOPSIS
    append_definer()
    thd           [in] thread handle
    buffer        [inout] buffer to hold DEFINER clause
    definer_user  [in] user name part of definer
    definer_host  [in] host name part of definer
*/

void append_definer(THD *thd, String *buffer, const LEX_CSTRING &definer_user,
                    const LEX_CSTRING &definer_host)
{
  buffer->append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, buffer, definer_user.str, definer_user.length);
  buffer->append('@');
  append_identifier(thd, buffer, definer_host.str, definer_host.length);
  buffer->append(' ');
}


int
view_store_create_info(THD *thd, TABLE_LIST *table, String *buff)
{
  my_bool compact_view_name= TRUE;
  my_bool compact_view_format= TRUE;
  my_bool foreign_db_mode= (thd->variables.sql_mode & (MODE_POSTGRESQL |
                                                       MODE_ORACLE |
                                                       MODE_MSSQL |
                                                       MODE_DB2 |
                                                       MODE_MAXDB |
                                                       MODE_ANSI)) != 0;

  if (!thd->db().str || strcmp(thd->db().str, table->view_db.str))
    /*
      print compact view name if the view belongs to the current database
    */
    compact_view_format= compact_view_name= FALSE;
  else
  {
    /*
      Compact output format for view body can be used
      if this view only references table inside it's own db
    */
    TABLE_LIST *tbl;
    for (tbl= thd->lex->query_tables;
         tbl;
         tbl= tbl->next_global)
    {
      if (strcmp(table->view_db.str,
                 tbl->is_view() ? tbl->view_db.str : tbl->db) != 0)
      {
        compact_view_format= FALSE;
        break;
      }
    }
  }

  buff->append(STRING_WITH_LEN("CREATE "));
  if (!foreign_db_mode)
  {
    view_store_options(thd, table, buff);
  }
  buff->append(STRING_WITH_LEN("VIEW "));
  if (!compact_view_name)
  {
    append_identifier(thd, buff, table->view_db.str, table->view_db.length);
    buff->append('.');
  }
  append_identifier(thd, buff, table->view_name.str, table->view_name.length);
  buff->append(STRING_WITH_LEN(" AS "));

  /*
    We can't just use table->query, because our SQL_MODE may trigger
    a different syntax, like when ANSI_QUOTES is defined.
  */
  table->view_query()->unit->print(buff, 
                           enum_query_type(QT_TO_ARGUMENT_CHARSET | 
                                           (compact_view_format ?
                                            QT_NO_DB : 0)));

  if (table->with_check != VIEW_CHECK_NONE)
  {
    if (table->with_check == VIEW_CHECK_LOCAL)
      buff->append(STRING_WITH_LEN(" WITH LOCAL CHECK OPTION"));
    else
      buff->append(STRING_WITH_LEN(" WITH CASCADED CHECK OPTION"));
  }
  return 0;
}


/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/
class thread_info : public Sql_alloc
{
public:
  thread_info()
    : thread_id(0), start_time(0), command(0),
      user(NULL), host(NULL), db(NULL), proc_info(NULL), state_info(NULL)
  { }

  my_thread_id thread_id;
  time_t start_time;
  uint   command;
  const char *user,*host,*db,*proc_info,*state_info;
  CSET_STRING query_string;
};

// For sorting by thread_id.
class thread_info_compare :
  public std::binary_function<const thread_info*, const thread_info*, bool>
{
public:
  bool operator() (const thread_info* p1, const thread_info* p2)
  {
    return p1->thread_id < p2->thread_id;
  }
};

static const char *thread_state_info(THD *tmp)
{
#ifndef EMBEDDED_LIBRARY
  if (tmp->get_protocol()->get_rw_status())
  {
    if (tmp->get_protocol()->get_rw_status() == 2)
      return "Sending to client";
    else if (tmp->get_command() == COM_SLEEP)
      return "";
    else
      return "Receiving from client";
  }
  else
#endif
  {
    Mutex_lock lock(&tmp->LOCK_current_cond);
    if (tmp->proc_info)
      return tmp->proc_info;
    else if (tmp->current_cond)
      return "Waiting on cond";
    else
      return NULL;
  }
}

/**
  This class implements callback function used by mysqld_list_processes() to
  list all the client process information.
*/
typedef Mem_root_array<thread_info*, true> Thread_info_array;
class List_process_list : public Do_THD_Impl
{
private:
  /* Username of connected client. */
  const char *m_user;
  Thread_info_array *m_thread_infos;
  /* THD of connected client. */
  THD *m_client_thd;
  size_t m_max_query_length;

public:
  List_process_list(const char *user_value, Thread_info_array *thread_infos,
                    THD *thd_value, size_t max_query_length) :
                    m_user(user_value), m_thread_infos(thread_infos),
                    m_client_thd(thd_value),
                    m_max_query_length(max_query_length)
  {}

  virtual void operator()(THD *inspect_thd)
  {
    Security_context *inspect_sctx= inspect_thd->security_context();
    LEX_CSTRING inspect_sctx_user= inspect_sctx->user();
    LEX_CSTRING inspect_sctx_host= inspect_sctx->host();
    LEX_CSTRING inspect_sctx_host_or_ip= inspect_sctx->host_or_ip();

    if ((!inspect_thd->get_protocol()->connection_alive() &&
         !inspect_thd->system_thread) ||
        (m_user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                    strcmp(inspect_sctx_user.str, m_user))))
      return;

    thread_info *thd_info= new thread_info;

    /* ID */
    thd_info->thread_id= inspect_thd->thread_id();

    /* USER */
    if (inspect_sctx_user.str)
      thd_info->user= m_client_thd->mem_strdup(inspect_sctx_user.str);
    else if (inspect_thd->system_thread)
      thd_info->user= "system user";
    else
      thd_info->user= "unauthenticated user";

    /* HOST */
    if (inspect_thd->peer_port &&
        (inspect_sctx_host.length ||
         inspect_sctx->ip().length) &&
        m_client_thd->security_context()->host_or_ip().str[0])
    {
      if ((thd_info->host=
           (char*) m_client_thd->alloc(LIST_PROCESS_HOST_LEN+1)))
        my_snprintf((char *) thd_info->host, LIST_PROCESS_HOST_LEN, "%s:%u",
                    inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
    }
    else
      thd_info->host=
        m_client_thd->mem_strdup(inspect_sctx_host_or_ip.str[0] ?
                                 inspect_sctx_host_or_ip.str :
                                 inspect_sctx_host.length ?
                                 inspect_sctx_host.str : "");

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data",
                    {
                    if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
                        inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
                    DEBUG_SYNC(m_client_thd, "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
                    });
    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db= inspect_thd->db().str;
    if (db)
      thd_info->db= m_client_thd->mem_strdup(db);

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION)
      thd_info->proc_info= "Killed";
    thd_info->command=(int) inspect_thd->get_command(); // Used for !killed.

    /* STATE */
    thd_info->state_info= thread_state_info(inspect_thd);

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str= inspect_thd->query().str;
      size_t query_length= inspect_thd->query().length;
#ifndef EMBEDDED_LIBRARY
      String buf;
      if (inspect_thd->is_a_srv_session())
      {
        buf.append(query_length? "PLUGIN: ":"PLUGIN");

        if (query_length)
          buf.append(query_str, query_length);

        query_str= buf.c_ptr();
        query_length= buf.length();
      }
      /* No else. We need fall-through */
#endif
      if (query_str)
      {
        const size_t width= min<size_t>(m_max_query_length, query_length);
        const char *q= m_client_thd->strmake(query_str, width);
        /* Safety: in case strmake failed, we set length to 0. */
        thd_info->query_string=
          CSET_STRING(q, q ? width : 0, inspect_thd->charset());
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    thd_info->start_time= inspect_thd->start_time.tv_sec;

    m_thread_infos->push_back(thd_info);
  }
};

void mysqld_list_processes(THD *thd,const char *user, bool verbose)
{
  Item *field;
  List<Item> field_list;
  Thread_info_array thread_infos(thd->mem_root);
  size_t max_query_length= (verbose ? thd->variables.max_allowed_packet :
                            PROCESS_LIST_WIDTH);
  Protocol *protocol= thd->get_protocol();
  DBUG_ENTER("mysqld_list_processes");

  field_list.push_back(new Item_int(NAME_STRING("Id"),
                                    0, MY_INT64_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("User",USERNAME_CHAR_LENGTH));
  field_list.push_back(new Item_empty_string("Host",LIST_PROCESS_HOST_LEN));
  field_list.push_back(field=new Item_empty_string("db",NAME_CHAR_LEN));
  field->maybe_null=1;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(field= new Item_return_int("Time",7, MYSQL_TYPE_LONG));
  field->unsigned_flag= 0;
  field_list.push_back(field=new Item_empty_string("State",30));
  field->maybe_null=1;
  field_list.push_back(field=new Item_empty_string("Info",max_query_length));
  field->maybe_null=1;
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_VOID_RETURN;

  if (!thd->killed)
  {
    thread_infos.reserve(Global_THD_manager::get_instance()->get_thd_count());
    List_process_list list_process_list(user, &thread_infos, thd,
                                        max_query_length);
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&list_process_list);
  }

  // Return list sorted by thread_id.
  std::sort(thread_infos.begin(), thread_infos.end(), thread_info_compare());

  time_t now= my_time(0);
  for (size_t ix= 0; ix < thread_infos.size(); ++ix)
  {
    thread_info *thd_info= thread_infos.at(ix);
    protocol->start_row();
    protocol->store((ulonglong) thd_info->thread_id);
    protocol->store(thd_info->user, system_charset_info);
    protocol->store(thd_info->host, system_charset_info);
    protocol->store(thd_info->db, system_charset_info);
    if (thd_info->proc_info)
      protocol->store(thd_info->proc_info, system_charset_info);
    else
      protocol->store(command_name[thd_info->command].str, system_charset_info);
    if (thd_info->start_time)
      protocol->store_long ((longlong) (now - thd_info->start_time));
    else
      protocol->store_null();
    protocol->store(thd_info->state_info, system_charset_info);
    protocol->store(thd_info->query_string.str(),
                    thd_info->query_string.charset());
    if (protocol->end_row())
      break; /* purecov: inspected */
  }
  my_eof(thd);
  DBUG_VOID_RETURN;
}


/**
  This class implements callback function used by fill_schema_processlist()
  to populate all the client process information into I_S table.
*/
class Fill_process_list : public Do_THD_Impl
{
private:
  /* THD of connected client. */
  THD *m_client_thd;
  /* Information of each process is added as records into this table. */
  TABLE_LIST *m_tables;

public:
  Fill_process_list(THD *thd_value, TABLE_LIST *tables_value) :
                    m_client_thd(thd_value), m_tables(tables_value) {}

  virtual void operator()(THD *inspect_thd)
  {
    Security_context *inspect_sctx= inspect_thd->security_context();
    LEX_CSTRING inspect_sctx_user= inspect_sctx->user();
    LEX_CSTRING inspect_sctx_host= inspect_sctx->host();
    LEX_CSTRING inspect_sctx_host_or_ip= inspect_sctx->host_or_ip();
    const char* client_priv_user=
      m_client_thd->security_context()->priv_user().str;
    const char *user=
      m_client_thd->security_context()->check_access(PROCESS_ACL) ?
        NullS : client_priv_user;

    if ((!inspect_thd->get_protocol()->connection_alive() &&
         !inspect_thd->system_thread) ||
        (user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                  strcmp(inspect_sctx_user.str, user))))
      return;

    TABLE *table= m_tables->table;
    restore_record(table, s->default_values);

    /* ID */
    table->field[0]->store((ulonglong) inspect_thd->thread_id(), true);

    /* USER */
    const char *val= NULL;
    if (inspect_sctx_user.str)
      val= inspect_sctx_user.str;
    else if (inspect_thd->system_thread)
      val= "system user";
    else
      val= "unauthenticated user";
    table->field[1]->store(val, strlen(val), system_charset_info);

    /* HOST */
    if (inspect_thd->peer_port &&
        (inspect_sctx_host.length ||
         inspect_sctx->ip().length) &&
        m_client_thd->security_context()->host_or_ip().str[0])
    {
      char host[LIST_PROCESS_HOST_LEN + 1];
      my_snprintf(host, LIST_PROCESS_HOST_LEN, "%s:%u",
                  inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
      table->field[2]->store(host, strlen(host), system_charset_info);
    }
    else
      table->field[2]->store(inspect_sctx_host_or_ip.str,
                             inspect_sctx_host_or_ip.length,
                             system_charset_info);

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data",
                    {
                    if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
                        inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
                    DEBUG_SYNC(m_client_thd, "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
                    });
    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db= inspect_thd->db().str;
    if (db)
    {
      table->field[3]->store(db, strlen(db), system_charset_info);
      table->field[3]->set_notnull();
    }

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION)
    {
      val= "Killed";
      table->field[4]->store(val, strlen(val), system_charset_info);
    }
    else
      table->field[4]->store(command_name[inspect_thd->get_command()].str,
                             command_name[inspect_thd->get_command()].length,
                             system_charset_info);

    /* STATE */
    val= thread_state_info(inspect_thd);
    if (val)
    {
      table->field[6]->store(val, strlen(val), system_charset_info);
      table->field[6]->set_notnull();
    }

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str= inspect_thd->query().str;
      size_t query_length= inspect_thd->query().length;
#ifndef EMBEDDED_LIBRARY
      String buf;
      if (inspect_thd->is_a_srv_session())
      {
        buf.append(query_length? "PLUGIN: ":"PLUGIN");

        if (query_length)
          buf.append(query_str, query_length);

        query_str= buf.c_ptr();
        query_length= buf.length();
      }
      /* No else. We need fall-through */
#endif
      if (query_str)
      {
        const size_t width= min<size_t>(PROCESS_LIST_INFO_WIDTH, query_length);
        table->field[7]->store(query_str, width, inspect_thd->charset());
        table->field[7]->set_notnull();
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    if (inspect_thd->start_time.tv_sec)
      table->field[5]->
        store((longlong) (my_time(0) - inspect_thd->start_time.tv_sec), false);
    else
      table->field[5]->store(0, false);

    schema_table_store_record(m_client_thd, table);
  }
};

int fill_schema_processlist(THD* thd, TABLE_LIST* tables, Item* cond)
{
  DBUG_ENTER("fill_schema_processlist");

  Fill_process_list fill_process_list(thd, tables);
  if (!thd->killed)
  {
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&fill_process_list);
  }
  DBUG_RETURN(0);
}

/*****************************************************************************
  Status functions
*****************************************************************************/
Status_var_array all_status_vars(0);
bool status_vars_inited= 0;
/* Version counter, protected by LOCK_STATUS. */
ulonglong status_var_array_version= 0;

static inline int show_var_cmp(const SHOW_VAR *var1, const SHOW_VAR *var2)
{
  return strcmp(var1->name, var2->name);
}

class Show_var_cmp :
  public std::binary_function<const st_mysql_show_var &,
                              const st_mysql_show_var &, bool>
{
public:
  bool operator()(const st_mysql_show_var &var1,
                  const st_mysql_show_var &var2)
  {
    return show_var_cmp(&var1, &var2) < 0;
  }
};


static inline bool is_show_undef(const st_mysql_show_var &var)
{
  return var.type == SHOW_UNDEF;
}

/*
  Deletes all the SHOW_UNDEF elements from the array.
  Shrinks array capacity to zero if it is completely empty.
*/
static void shrink_var_array(Status_var_array *array)
{
  /* remove_if maintains order for the elements that are *not* removed */
  array->erase(std::remove_if(array->begin(), array->end(), is_show_undef),
               array->end());
  if (array->empty())
    Status_var_array().swap(*array);
}

/*
  Adds an array of SHOW_VAR entries to the output of SHOW STATUS

  SYNOPSIS
    add_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to add to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    The handling of all_status_vars[] is completely internal, it's allocated
    automatically when something is added to it, and deleted completely when
    the last entry is removed.

    As a special optimization, if add_status_vars() is called before
    init_status_vars(), it assumes "startup mode" - neither concurrent access
    to the array nor SHOW STATUS are possible (thus it skips locks and sort)

    The last entry of the all_status_vars[] should always be {0,0,SHOW_UNDEF}
*/
int add_status_vars(const SHOW_VAR *list)
{
  Mutex_lock lock(status_vars_inited ? &LOCK_status : NULL);

  try
  {
    while (list->name)
      all_status_vars.push_back(*list++);
  }
  catch (const std::bad_alloc &)
  {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(Status_var_array::value_type)));
    return 1;
  }

  if (status_vars_inited)
    std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());
  
  status_var_array_version++;
  return 0;
}

/*
  Make all_status_vars[] usable for SHOW STATUS

  NOTE
    See add_status_vars(). Before init_status_vars() call, add_status_vars()
    works in a special fast "startup" mode. Thus init_status_vars()
    should be called as late as possible but before enabling multi-threading.
*/
void init_status_vars()
{
  status_vars_inited=1;
  std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());
  status_var_array_version++;
}

void reset_status_vars()
{
  Status_var_array::iterator ptr= all_status_vars.begin();
  Status_var_array::iterator last= all_status_vars.end();
  for (; ptr < last; ptr++)
  {
    /* Note that SHOW_LONG_NOFLUSH variables are not reset */
    if (ptr->type == SHOW_LONG || ptr->type == SHOW_SIGNED_LONG)
      *(ulong*) ptr->value= 0;
  }
}

/*
  Current version of the all_status_vars.
*/
ulonglong get_status_vars_version(void)
{
  return (status_var_array_version);
}

/*
  catch-all cleanup function, cleans up everything no matter what

  DESCRIPTION
    This function is not strictly required if all add_to_status/
    remove_status_vars are properly paired, but it's a safety measure that
    deletes everything from the all_status_vars[] even if some
    remove_status_vars were forgotten
*/
void free_status_vars()
{
  Status_var_array().swap(all_status_vars);
  status_var_array_version++;
}

/**
  @brief           Get the value of given status variable

  @param[in]       thd        thread handler
  @param[in]       list       list of SHOW_VAR objects in which function should
                              search
  @param[in]       name       name of the status variable
  @param[in]       var_type   Variable type
  @param[in/out]   value      buffer in which value of the status variable
                              needs to be filled in
  @param[in/out]   length     filled with buffer length

  @return          status
    @retval        FALSE      if variable is not found in the list
    @retval        TRUE       if variable is found in the list
*/

bool get_status_var(THD *thd, SHOW_VAR *list, const char * name,
                    char * const value, enum_var_type var_type, size_t *length)
{
  for (; list->name; list++)
  {
    int res= strcmp(list->name, name);
    if (res == 0)
    {
      /*
        if var->type is SHOW_FUNC, call the function.
        Repeat as necessary, if new var is again SHOW_FUNC
       */
      SHOW_VAR tmp;
      for (; list->type == SHOW_FUNC; list= &tmp)
        ((mysql_show_var_func)(list->value))(thd, &tmp, value);

      get_one_variable(thd, list, var_type, list->type, NULL, NULL, value, length);
      return TRUE;
    }
  }
  return FALSE;
}

/*
  Removes an array of SHOW_VAR entries from the output of SHOW STATUS

  SYNOPSIS
    remove_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to remove to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    there's lots of room for optimizing this, especially in non-sorted mode,
    but nobody cares - it may be called only in case of failed plugin
    initialization in the mysqld startup.
*/

void remove_status_vars(SHOW_VAR *list)
{
  if (status_vars_inited)
  {
    mysql_mutex_lock(&LOCK_status);
    size_t a= 0, b= all_status_vars.size(), c= (a+b)/2;

    for (; list->name; list++)
    {
      int res= 0;
      for (a= 0, b= all_status_vars.size(); b-a > 1; c= (a+b)/2)
      {
        res= show_var_cmp(list, &all_status_vars[c]);
        if (res < 0)
          b= c;
        else if (res > 0)
          a= c;
        else
          break;
      }
      if (res == 0)
        all_status_vars[c].type= SHOW_UNDEF;
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
    mysql_mutex_unlock(&LOCK_status);
  }
  else
  {
    uint i;
    for (; list->name; list++)
    {
      for (i= 0; i < all_status_vars.size(); i++)
      {
        if (show_var_cmp(list, &all_status_vars[i]))
          continue;
        all_status_vars[i].type= SHOW_UNDEF;
        break;
      }
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
  }
}

inline void make_upper(char *buf)
{
  for (; *buf; buf++)
    *buf= my_toupper(system_charset_info, *buf);
}

/**
  @brief Returns the value of a system or a status variable.

  @param thd        [in]     The handle of the current THD.
  @param variable   [in]     Details of the variable.
  @param value_type [in]     Variable type.
  @param show_type  [in]     Variable show type.
  @param charset    [out]    Character set of the value.
  @param buff       [in,out] Buffer to store the value.
                             (Needs to have enough memory
                             to hold the value of variable.)
  @param length     [out]    Length of the value.

  @return                    Pointer to the value buffer.
*/

const char* get_one_variable(THD *thd, const SHOW_VAR *variable,
                             enum_var_type value_type, SHOW_TYPE show_type,
                             system_status_var *status_var,
                             const CHARSET_INFO **charset, char *buff,
                             size_t *length)
{
  return get_one_variable_ext(thd, thd, variable, value_type, show_type,
                              status_var, charset, buff, length);
}

/**
  @brief Returns the value of a system or a status variable.

  @param running_thd [in]     The handle of the current THD.
  @param target_thd  [in]     The handle of the remote THD.
  @param variable    [in]     Details of the variable.
  @param value_type  [in]     Variable type.
  @param show_type   [in]     Variable show type.
  @param charset     [out]    Character set of the value.
  @param buff        [in,out] Buffer to store the value.
                              (Needs to have enough memory
                              to hold the value of variable.)
  @param length      [out]    Length of the value.

  @return                     Pointer to the value buffer.
*/

const char* get_one_variable_ext(THD *running_thd, THD *target_thd,
                                 const SHOW_VAR *variable,
                                 enum_var_type value_type, SHOW_TYPE show_type,
                                 system_status_var *status_var,
                                 const CHARSET_INFO **charset, char *buff,
                                 size_t *length)
{
  const char *value;
  const CHARSET_INFO *value_charset;

  if (show_type == SHOW_SYS)
  {
    LEX_STRING null_lex_str;
    null_lex_str.str= 0;                        // For sys_var->value_ptr()
    null_lex_str.length= 0;
    sys_var *var= ((sys_var *) variable->value);
    show_type= var->show_type();
    value= (char*) var->value_ptr(running_thd, target_thd, value_type, &null_lex_str);
    value_charset= var->charset(target_thd);
  }
  else
  {
    value= variable->value;
    value_charset= system_charset_info;
  }

  const char *pos= buff;
  const char *end= buff;

  /*
    Note that value may == buff. All SHOW_xxx code below should still work.
  */
  switch (show_type)
  {
    case SHOW_DOUBLE_STATUS:
      value= ((char *) status_var + (ulong) value);
      /* fall through */

    case SHOW_DOUBLE:
      /* 6 is the default precision for '%f' in sprintf() */
      end= buff + my_fcvt(*(double *) value, 6, buff, NULL);
      value_charset= system_charset_info;
      break;

    case SHOW_LONG_STATUS:
      value= ((char *) status_var + (ulong) value);
      /* fall through */

    case SHOW_LONG:
     /* the difference lies in refresh_status() */
    case SHOW_LONG_NOFLUSH:
      end= int10_to_str(*(long*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_SIGNED_LONG:
      end= int10_to_str(*(long*) value, buff, -10);
      value_charset= system_charset_info;
      break;

    case SHOW_LONGLONG_STATUS:
      value= ((char *) status_var + (ulong) value);
      /* fall through */

    case SHOW_LONGLONG:
      end= longlong10_to_str(*(longlong*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_HA_ROWS:
      end= longlong10_to_str((longlong) *(ha_rows*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_BOOL:
      end= my_stpcpy(buff, *(bool*) value ? "ON" : "OFF");
      value_charset= system_charset_info;
      break;

    case SHOW_MY_BOOL:
      end= my_stpcpy(buff, *(my_bool*) value ? "ON" : "OFF");
      value_charset= system_charset_info;
      break;

    case SHOW_INT:
      end= int10_to_str((long) *(uint32*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_HAVE:
    {
      SHOW_COMP_OPTION tmp= *(SHOW_COMP_OPTION*) value;
      pos= show_comp_option_name[(int) tmp];
      end= strend(pos);
      value_charset= system_charset_info;
      break;
    }

    case SHOW_CHAR:
    {
      if (!(pos= value))
        pos= "";
      end= strend(pos);
      break;
    }

    case SHOW_CHAR_PTR:
    {
      if (!(pos= *(char**) value))
        pos= "";

      end= strend(pos);
      break;
    }

    case SHOW_LEX_STRING:
    {
      LEX_STRING *ls=(LEX_STRING*)value;
      if (!(pos= ls->str))
        end= pos= "";
      else
        end= pos + ls->length;
      break;
    }

    case SHOW_KEY_CACHE_LONG:
      value= (char*) dflt_key_cache + (ulong)value;
      end= int10_to_str(*(long*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_KEY_CACHE_LONGLONG:
      value= (char*) dflt_key_cache + (ulong)value;
      end= longlong10_to_str(*(longlong*) value, buff, 10);
      value_charset= system_charset_info;
      break;

    case SHOW_UNDEF:
      break;                /* Return empty string */

    case SHOW_SYS:          /* Cannot happen */

    default:
      DBUG_ASSERT(0);
      break;
  }

  *length= (size_t) (end - pos);
  /* Some callers do not use the result. */
  if (charset != NULL)
  {
    DBUG_ASSERT(value_charset != NULL);
    *charset= value_charset;
  }
  return pos;
}

static bool show_status_array(THD *thd, const char *wild,
                              SHOW_VAR *variables,
                              enum enum_var_type value_type,
                              struct system_status_var *status_var,
                              const char *prefix, TABLE_LIST *tl,
                              bool ucase_names,
                              Item *cond)
{
  my_aligned_storage<SHOW_VAR_FUNC_BUFF_SIZE, MY_ALIGNOF(longlong)> buffer;
  char * const buff= buffer.data;
  char *prefix_end;
  /* the variable name should not be longer than 64 characters */
  char name_buffer[SHOW_VAR_MAX_NAME_LEN];
  size_t len;
  SHOW_VAR tmp, *var;
  Item *partial_cond= 0;
  enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  bool res= FALSE;
  const CHARSET_INFO *charset= system_charset_info;
  DBUG_ENTER("show_status_array");

  TABLE *const table= tl->table;

  thd->count_cuted_fields= CHECK_FIELD_WARN;  

  prefix_end=my_stpnmov(name_buffer, prefix, sizeof(name_buffer)-1);
  if (*prefix)
    *prefix_end++= '_';
  len= (int)(name_buffer + sizeof(name_buffer) - prefix_end);
  partial_cond= make_cond_for_info_schema(cond, tl);

  for (; variables->name; variables++)
  {
    my_stpnmov(prefix_end, variables->name, len);
    name_buffer[sizeof(name_buffer)-1]=0;       /* Safety */
    if (ucase_names)
      make_upper(name_buffer);

    restore_record(table, s->default_values);
    table->field[0]->store(name_buffer, strlen(name_buffer),
                           system_charset_info);
    /*
      if var->type is SHOW_FUNC, call the function.
      Repeat as necessary, if new var is again SHOW_FUNC
    */
    for (var=variables; var->type == SHOW_FUNC; var= &tmp)
      ((mysql_show_var_func)(var->value))(thd, &tmp, buff);

    SHOW_TYPE show_type=var->type;
    if (show_type == SHOW_ARRAY)
    {
      show_status_array(thd, wild, (SHOW_VAR *) var->value, value_type,
                        status_var, name_buffer, tl, ucase_names, partial_cond);
    }
    else
    {
      if (!(wild && wild[0] && wild_case_compare(system_charset_info,
                                                 name_buffer, wild)) &&
          (!partial_cond || partial_cond->val_int()))
      {
        const char *pos;
        size_t length;

        mysql_mutex_lock(&LOCK_global_system_variables);
        pos= get_one_variable(thd, var, value_type, show_type, status_var,
                              &charset, buff, &length);
        table->field[1]->store(pos, (uint32) length, charset);
        thd->count_cuted_fields= CHECK_FIELD_IGNORE;
        table->field[1]->set_notnull();
        mysql_mutex_unlock(&LOCK_global_system_variables);

        if (schema_table_store_record(thd, table))
        {
          res= TRUE;
          goto end;
        }

#ifndef EMBEDDED_LIBRARY
        if (variables->type != SHOW_FUNC && value_type == OPT_GLOBAL &&
            mysql_audit_notify(thd,
                               AUDIT_EVENT(MYSQL_AUDIT_GLOBAL_VARIABLE_GET),
                               var->name, pos, length))
        {
          res= TRUE;
          goto end;
        }
#endif
      }
    }
  }
end:
  thd->count_cuted_fields= save_count_cuted_fields;
  DBUG_RETURN(res);
}


/**
  Collect status for all running threads.
*/
class Add_status : public Do_THD_Impl
{
public:
  Add_status(STATUS_VAR* value) : m_stat_var(value) {}
  virtual void operator()(THD *thd)
  {
    if (!thd->status_var_aggregated)
      add_to_status(m_stat_var, &thd->status_var, false);
  }
private:
  /* Status of all threads are summed into this. */
  STATUS_VAR* m_stat_var;
};

void calc_sum_of_all_status(STATUS_VAR *to)
{
  DBUG_ENTER("calc_sum_of_all_status");
  mysql_mutex_assert_owner(&LOCK_status);
  /* Get global values as base. */
  *to= global_status_var;
  Add_status add_status(to);
  Global_THD_manager::get_instance()->do_for_all_thd_copy(&add_status);
  DBUG_VOID_RETURN;
}

/* This is only used internally, but we need it here as a forward reference */
extern ST_SCHEMA_TABLE schema_tables[];

/**
  Condition pushdown used for INFORMATION_SCHEMA / SHOW queries.
  This structure is to implement an optimization when
  accessing data dictionary data in the INFORMATION_SCHEMA
  or SHOW commands.
  When the query contain a TABLE_SCHEMA or TABLE_NAME clause,
  narrow the search for data based on the constraints given.
*/
typedef struct st_lookup_field_values
{
  /**
    Value of a TABLE_SCHEMA clause.
    Note that this value length may exceed @c NAME_LEN.
    @sa wild_db_value
  */
  LEX_STRING db_value;
  /**
    Value of a TABLE_NAME clause.
    Note that this value length may exceed @c NAME_LEN.
    @sa wild_table_value
  */
  LEX_STRING table_value;
  /**
    True when @c db_value is a LIKE clause,
    false when @c db_value is an '=' clause.
  */
  bool wild_db_value;
  /**
    True when @c table_value is a LIKE clause,
    false when @c table_value is an '=' clause.
  */
  bool wild_table_value;
} LOOKUP_FIELD_VALUES;

/*
  Store record to I_S table, convert HEAP table
  to MyISAM if necessary

  SYNOPSIS
    schema_table_store_record()
    thd                   thread handler
    table                 Information schema table to be updated

  RETURN
    0	                  success
    1	                  error
*/

bool schema_table_store_record(THD *thd, TABLE *table)
{
  int error;
  if ((error= table->file->ha_write_row(table->record[0])))
  {
    Temp_table_param *param= table->pos_in_table_list->schema_table_param;

    if (create_ondisk_from_heap(thd, table, param->start_recinfo, 
                                &param->recinfo, error, FALSE, NULL))
      return 1;
  }
  return 0;
}

/**
  Store record to I_S table, convert HEAP table to InnoDB table if necessary.

  @param[in]  thd            thread handler
  @param[in]  table          Information schema table to be updated
  @param[in]  make_ondisk    if true, convert heap table to on disk table.
                             default value is true.
  @return 0 on success
  @return error code on failure.
*/
int schema_table_store_record2(THD *thd, TABLE *table, bool make_ondisk)
{
  int error;
  if ((error= table->file->ha_write_row(table->record[0])))
  {
    if (!make_ondisk)
        return error;

    if (convert_heap_table_to_ondisk(thd, table, error))
        return 1;
  }
  return 0;
}

/**
  Convert HEAP table to InnoDB table if necessary

  @param[in] thd     thread handler
  @param[in] table   Information schema table to be converted.
  @param[in] error   the error code returned previously.
  @return false on success, true on error.
*/
bool convert_heap_table_to_ondisk(THD *thd, TABLE *table, int error)
{
  Temp_table_param *param= table->pos_in_table_list->schema_table_param;

  return (create_ondisk_from_heap(thd, table, param->start_recinfo,
                              &param->recinfo, error, FALSE, NULL));
}


static int make_table_list(THD *thd, SELECT_LEX *sel,
                           const LEX_CSTRING &db_name,
                           const LEX_CSTRING &table_name)
{
  Table_ident *table_ident;
  table_ident= new Table_ident(thd, db_name, table_name, 1);
  if (!sel->add_table_to_list(thd, table_ident, 0, 0, TL_READ, MDL_SHARED_READ))
    return 1;
  return 0;
}


/**
  @brief    Get lookup value from the part of 'WHERE' condition 

  @details This function gets lookup value from 
           the part of 'WHERE' condition if it's possible and 
           fill appropriate lookup_field_vals struct field
           with this value.

  @param[in]      thd                   thread handler
  @param[in]      item_func             part of WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool get_lookup_value(THD *thd, Item_func *item_func,
                      TABLE_LIST *table, 
                      LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  ST_SCHEMA_TABLE *schema_table= table->schema_table;
  ST_FIELD_INFO *field_info= schema_table->fields_info;
  const char *field_name1= schema_table->idx_field1 >= 0 ?
    field_info[schema_table->idx_field1].field_name : "";
  const char *field_name2= schema_table->idx_field2 >= 0 ?
    field_info[schema_table->idx_field2].field_name : "";

  if (item_func->functype() == Item_func::EQ_FUNC ||
      item_func->functype() == Item_func::EQUAL_FUNC)
  {
    int idx_field, idx_val;
    char tmp[MAX_FIELD_WIDTH];
    String *tmp_str, str_buff(tmp, sizeof(tmp), system_charset_info);
    Item_field *item_field;
    CHARSET_INFO *cs= system_charset_info;

    if (item_func->arguments()[0]->type() == Item::FIELD_ITEM &&
        item_func->arguments()[1]->const_item())
    {
      idx_field= 0;
      idx_val= 1;
    }
    else if (item_func->arguments()[1]->type() == Item::FIELD_ITEM &&
             item_func->arguments()[0]->const_item())
    {
      idx_field= 1;
      idx_val= 0;
    }
    else
      return 0;

    item_field= (Item_field*) item_func->arguments()[idx_field];
    if (table->table != item_field->field->table)
      return 0;
    tmp_str= item_func->arguments()[idx_val]->val_str(&str_buff);

    /* impossible value */
    if (!tmp_str)
      return 1;

    /* Lookup value is database name */
    if (!cs->coll->strnncollsp(cs, (uchar *) field_name1, strlen(field_name1),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0))
    {
      thd->make_lex_string(&lookup_field_vals->db_value, tmp_str->ptr(),
                           tmp_str->length(), FALSE);
    }
    /* Lookup value is table name */
    else if (!cs->coll->strnncollsp(cs, (uchar *) field_name2,
                                    strlen(field_name2),
                                    (uchar *) item_field->field_name,
                                    strlen(item_field->field_name), 0))
    {
      thd->make_lex_string(&lookup_field_vals->table_value, tmp_str->ptr(),
                           tmp_str->length(), FALSE);
    }
  }
  return 0;
}


/**
  @brief    Calculates lookup values from 'WHERE' condition 

  @details This function calculates lookup value(database name, table name)
           from 'WHERE' condition if it's possible and 
           fill lookup_field_vals struct fields with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool calc_lookup_values_from_cond(THD *thd, Item *cond, TABLE_LIST *table,
                                  LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  if (!cond)
    return 0;

  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        if (item->type() == Item::FUNC_ITEM)
        {
          if (get_lookup_value(thd, (Item_func*)item, table, lookup_field_vals))
            return 1;
        }
        else
        {
          if (calc_lookup_values_from_cond(thd, item, table, lookup_field_vals))
            return 1;
        }
      }
    }
    return 0;
  }
  else if (cond->type() == Item::FUNC_ITEM &&
           get_lookup_value(thd, (Item_func*) cond, table, lookup_field_vals))
    return 1;
  return 0;
}


bool uses_only_table_name_fields(Item *item, TABLE_LIST *table)
{
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *item_func= (Item_func*)item;
    for (uint i=0; i<item_func->argument_count(); i++)
    {
      if (!uses_only_table_name_fields(item_func->arguments()[i], table))
        return 0;
    }
  }
  else if (item->type() == Item::FIELD_ITEM)
  {
    Item_field *item_field= (Item_field*)item;
    CHARSET_INFO *cs= system_charset_info;
    ST_SCHEMA_TABLE *schema_table= table->schema_table;
    ST_FIELD_INFO *field_info= schema_table->fields_info;
    const char *field_name1= schema_table->idx_field1 >= 0 ?
      field_info[schema_table->idx_field1].field_name : "";
    const char *field_name2= schema_table->idx_field2 >= 0 ?
      field_info[schema_table->idx_field2].field_name : "";
    if (table->table != item_field->field->table ||
        (cs->coll->strnncollsp(cs, (uchar *) field_name1, strlen(field_name1),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0) &&
         cs->coll->strnncollsp(cs, (uchar *) field_name2, strlen(field_name2),
                               (uchar *) item_field->field_name,
                               strlen(item_field->field_name), 0)))
      return 0;
  }
  else if (item->type() == Item::REF_ITEM)
    return uses_only_table_name_fields(item->real_item(), table);

  if (item->type() == Item::SUBSELECT_ITEM && !item->const_item())
    return 0;

  return 1;
}


static Item * make_cond_for_info_schema(Item *cond, TABLE_LIST *table)
{
  if (!cond)
    return (Item*) 0;
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_info_schema(item, table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (Item*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_info_schema(item, table);
	if (!fix)
	  return (Item*) 0;
	new_cond->argument_list()->push_back(fix);
      }
      new_cond->quick_fix_field();
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_only_table_name_fields(cond, table))
    return (Item*) 0;
  return cond;
}


/**
  @brief   Calculate lookup values(database name, table name)

  @details This function calculates lookup values(database name, table name)
           from 'WHERE' condition or wild values (for 'SHOW' commands only)
           from LEX struct and fill lookup_field_vals struct field
           with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      tables                I_S table
  @param[in, out] lookup_field_values   Struct which holds lookup values 

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool get_lookup_field_values(THD *thd, Item *cond, TABLE_LIST *tables,
                             LOOKUP_FIELD_VALUES *lookup_field_values)
{
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  bool rc= 0;

  memset(lookup_field_values, 0, sizeof(LOOKUP_FIELD_VALUES));
  switch (lex->sql_command) {
  case SQLCOM_SHOW_DATABASES:
    if (wild)
    {
      thd->make_lex_string(&lookup_field_values->db_value, 
                           wild, strlen(wild), 0);
      lookup_field_values->wild_db_value= 1;
    }
    break;
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_TRIGGERS:
  case SQLCOM_SHOW_EVENTS:
    thd->make_lex_string(&lookup_field_values->db_value, 
                         lex->select_lex->db, strlen(lex->select_lex->db), 0);
    if (wild)
    {
      thd->make_lex_string(&lookup_field_values->table_value, 
                           wild, strlen(wild), 0);
      lookup_field_values->wild_table_value= 1;
    }
    break;
  default:
    /*
      The "default" is for queries over I_S.
      All previous cases handle SHOW commands.
    */
    rc= calc_lookup_values_from_cond(thd, cond, tables, lookup_field_values);
    break;
  }

  if (lower_case_table_names && !rc)
  {
    /* 
      We can safely do in-place upgrades here since all of the above cases
      are allocating a new memory buffer for these strings.
    */  
    if (lookup_field_values->db_value.str && lookup_field_values->db_value.str[0])
      my_casedn_str(system_charset_info, lookup_field_values->db_value.str);
    if (lookup_field_values->table_value.str && 
        lookup_field_values->table_value.str[0])
      my_casedn_str(system_charset_info, lookup_field_values->table_value.str);
  }

  return rc;
}


enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table)
{
  return (enum enum_schema_tables) (schema_table - &schema_tables[0]);
}


/*
  Create db names list. Information schema name always is first in list

  SYNOPSIS
    make_db_list()
    thd                   thread handler
    files                 list of db names
    wild                  wild string
    idx_field_vals        idx_field_vals->db_name contains db name or
                          wild string
    with_i_schema         returns 1 if we added 'IS' name to list
                          otherwise returns 0 

  RETURN
    zero                  success
    non-zero              error
*/

int make_db_list(THD *thd, List<LEX_STRING> *files,
                 LOOKUP_FIELD_VALUES *lookup_field_vals,
                 bool *with_i_schema, MEM_ROOT *tmp_mem_root)
{
  LEX_STRING *i_s_name_copy= 0;
  i_s_name_copy= thd->make_lex_string(i_s_name_copy,
                                      INFORMATION_SCHEMA_NAME.str,
                                      INFORMATION_SCHEMA_NAME.length, TRUE);
  *with_i_schema= 0;
  if (lookup_field_vals->wild_db_value)
  {
    /*
      This part of code is only for SHOW DATABASES command.
      idx_field_vals->db_value can be 0 when we don't use
      LIKE clause (see also get_index_field_values() function)
    */
    if (!lookup_field_vals->db_value.str ||
        !wild_case_compare(system_charset_info, 
                           INFORMATION_SCHEMA_NAME.str,
                           lookup_field_vals->db_value.str))
    {
      *with_i_schema= 1;
      if (files->push_back(i_s_name_copy))
        return 1;
    }
    return (find_files(thd, files, NullS, mysql_data_home,
                       lookup_field_vals->db_value.str, 1, tmp_mem_root) !=
                      FIND_FILES_OK);
  }


  /*
    If we have db lookup value we just add it to list and
    exit from the function.
    We don't do this for database names longer than the maximum
    name length.
  */
  if (lookup_field_vals->db_value.str)
  {
    if (lookup_field_vals->db_value.length > NAME_LEN)
    {
      /*
        Impossible value for a database name,
        found in a WHERE DATABASE_NAME = 'xxx' clause.
      */
      return 0;
    }

    if (is_infoschema_db(lookup_field_vals->db_value.str,
                         lookup_field_vals->db_value.length))
    {
      *with_i_schema= 1;
      if (files->push_back(i_s_name_copy))
        return 1;
      return 0;
    }
    if (files->push_back(&lookup_field_vals->db_value))
      return 1;
    return 0;
  }

  /*
    Create list of existing databases. It is used in case
    of select from information schema table
  */
  if (files->push_back(i_s_name_copy))
    return 1;
  *with_i_schema= 1;
  return (find_files(thd, files, NullS,
                     mysql_data_home, NullS, 1, tmp_mem_root) != FIND_FILES_OK);
}


struct st_add_schema_table 
{
  List<LEX_STRING> *files;
  const char *wild;
};


static my_bool add_schema_table(THD *thd, plugin_ref plugin,
                                void* p_data)
{
  LEX_STRING *file_name= 0;
  st_add_schema_table *data= (st_add_schema_table *)p_data;
  List<LEX_STRING> *file_list= data->files;
  const char *wild= data->wild;
  ST_SCHEMA_TABLE *schema_table= plugin_data<ST_SCHEMA_TABLE*>(plugin);
  DBUG_ENTER("add_schema_table");

  if (schema_table->hidden)
      DBUG_RETURN(0);
  if (wild)
  {
    if (lower_case_table_names)
    {
      if (wild_case_compare(files_charset_info,
                            schema_table->table_name,
                            wild))
        DBUG_RETURN(0);
    }
    else if (wild_compare(schema_table->table_name, wild, 0))
      DBUG_RETURN(0);
  }

  if ((file_name= thd->make_lex_string(file_name, schema_table->table_name,
                                       strlen(schema_table->table_name),
                                       TRUE)) &&
      !file_list->push_back(file_name))
    DBUG_RETURN(0);
  DBUG_RETURN(1);
}


int schema_tables_add(THD *thd, List<LEX_STRING> *files, const char *wild)
{
  LEX_STRING *file_name= 0;
  ST_SCHEMA_TABLE *tmp_schema_table= schema_tables;
  st_add_schema_table add_data;
  DBUG_ENTER("schema_tables_add");

  for (; tmp_schema_table->table_name; tmp_schema_table++)
  {
    if (tmp_schema_table->hidden)
      continue;
    if (wild)
    {
      if (lower_case_table_names)
      {
        if (wild_case_compare(files_charset_info,
                              tmp_schema_table->table_name,
                              wild))
          continue;
      }
      else if (wild_compare(tmp_schema_table->table_name, wild, 0))
        continue;
    }
    if ((file_name= 
         thd->make_lex_string(file_name, tmp_schema_table->table_name,
                              strlen(tmp_schema_table->table_name), TRUE)) &&
        !files->push_back(file_name))
      continue;
    DBUG_RETURN(1);
  }

  add_data.files= files;
  add_data.wild= wild;
  if (plugin_foreach(thd, add_schema_table,
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &add_data))
      DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/**
  @brief          Create table names list

  @details        The function creates the list of table names in
                  database

  @param[in]      thd                   thread handler
  @param[in]      table_names           List of table names in database
  @param[in]      lex                   pointer to LEX struct
  @param[in]      lookup_field_vals     pointer to LOOKUP_FIELD_VALUE struct
  @param[in]      with_i_schema         TRUE means that we add I_S tables to list
  @param[in]      db_name               database name

  @return         Operation status
    @retval       0           ok
    @retval       1           fatal error
    @retval       2           Not fatal error; Safe to ignore this file list
*/

static int
make_table_name_list(THD *thd, List<LEX_STRING> *table_names, LEX *lex,
                     LOOKUP_FIELD_VALUES *lookup_field_vals,
                     bool with_i_schema, LEX_STRING *db_name,
                     MEM_ROOT *tmp_mem_root)
{
  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, db_name->str, "", "", 0);
  if (!lookup_field_vals->wild_table_value &&
      lookup_field_vals->table_value.str)
  {
    if (lookup_field_vals->table_value.length > NAME_LEN)
    {
      /*
        Impossible value for a table name,
        found in a WHERE TABLE_NAME = 'xxx' clause.
      */
      return 0;
    }

    if (with_i_schema)
    {
      LEX_STRING *name= NULL;
      ST_SCHEMA_TABLE *schema_table=
        find_schema_table(thd, lookup_field_vals->table_value.str);
      if (schema_table && !schema_table->hidden)
      {
        if (!(name=
              thd->make_lex_string(name, schema_table->table_name,
                                   strlen(schema_table->table_name), TRUE)) ||
            table_names->push_back(name))
          return 1;
      }
    }
    else
    {    
      if (table_names->push_back(&lookup_field_vals->table_value))
        return 1;
      /*
        Check that table is relevant in current transaction.
        (used for ndb engine, see ndbcluster_find_files(), ha_ndbcluster.cc)
      */
      (void) ha_find_files(thd, db_name->str, path,
                         lookup_field_vals->table_value.str, 0,
                         table_names);
    }
    return 0;
  }

  /*
    This call will add all matching the wildcards (if specified) IS tables
    to the list
  */
  if (with_i_schema)
    return (schema_tables_add(thd, table_names,
                              lookup_field_vals->table_value.str));

  find_files_result res= find_files(thd, table_names, db_name->str, path,
                                    lookup_field_vals->table_value.str, 0,
                                    tmp_mem_root);
  if (res != FIND_FILES_OK)
  {
    /*
      Downgrade errors about problems with database directory to
      warnings if this is not a 'SHOW' command.  Another thread
      may have dropped database, and we may still have a name
      for that directory.
    */
    if (res == FIND_FILES_DIR)
    {
      if (sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND)
        return 1;
      thd->clear_error();
      return 2;
    }
    return 1;
  }
  return 0;
}


/**
  Fill I_S table with data obtained by performing full-blown table open.

  @param  thd                       Thread handler.
  @param  is_show_fields_or_keys    Indicates whether it is a legacy SHOW
                                    COLUMNS or SHOW KEYS statement.
  @param  table                     TABLE object for I_S table to be filled.
  @param  schema_table              I_S table description structure.
  @param  orig_db_name              Database name.
  @param  orig_table_name           Table name.
  @param  open_tables_state_backup  Open_tables_state object which is used
                                    to save/restore original status of
                                    variables related to open tables state.
  @param  can_deadlock              Indicates that deadlocks are possible
                                    due to metadata locks, so to avoid
                                    them we should not wait in case if
                                    conflicting lock is present.

  @retval FALSE - Success.
  @retval TRUE  - Failure.
*/
static bool
fill_schema_table_by_open(THD *thd, MEM_ROOT *mem_root, 
                          bool is_show_fields_or_keys,
                          TABLE *table, ST_SCHEMA_TABLE *schema_table,
                          LEX_STRING *orig_db_name,
                          LEX_STRING *orig_table_name,
                          Open_tables_backup *open_tables_state_backup,
                          bool can_deadlock)
{
  Query_arena i_s_arena(mem_root,
                        Query_arena::STMT_CONVENTIONAL_EXECUTION),
              backup_arena, *old_arena;
  LEX *old_lex= thd->lex, temp_lex, *lex;
  LEX_CSTRING db_name_lex_cstr, table_name_lex_cstr;
  TABLE_LIST *table_list;
  bool result= true;

  DBUG_ENTER("fill_schema_table_by_open");
  /*
    When a view is opened its structures are allocated on a permanent
    statement arena and linked into the LEX tree for the current statement
    (this happens even in cases when view is handled through TEMPTABLE
    algorithm).

    To prevent this process from unnecessary hogging of memory in the permanent
    arena of our I_S query and to avoid damaging its LEX we use temporary
    arena and LEX for table/view opening.

    Use temporary arena instead of statement permanent arena. Also make
    it active arena and save original one for successive restoring.
  */
  old_arena= thd->stmt_arena;
  thd->stmt_arena= &i_s_arena;
  thd->set_n_backup_active_arena(&i_s_arena, &backup_arena);

  /* Prepare temporary LEX. */
  thd->lex= lex= &temp_lex;
  lex_start(thd);

  /* Disable constant subquery evaluation as we won't be locking tables. */
  lex->context_analysis_only= CONTEXT_ANALYSIS_ONLY_VIEW;

  /*
    Some of process_table() functions rely on wildcard being passed from
    old LEX (or at least being initialized).
  */
  lex->wild= old_lex->wild;

  /*
    Since make_table_list() might change database and table name passed
    to it we create copies of orig_db_name and orig_table_name here.
    These copies are used for make_table_list() while unaltered values
    are passed to process_table() functions.
  */
  if (!thd->make_lex_string(&db_name_lex_cstr, orig_db_name->str,
                            orig_db_name->length, FALSE) ||
      !thd->make_lex_string(&table_name_lex_cstr, orig_table_name->str,
                            orig_table_name->length, FALSE))
    goto end;

  /*
    Create table list element for table to be open. Link it with the
    temporary LEX. The latter is required to correctly open views and
    produce table describing their structure.
  */
  if (make_table_list(thd, lex->select_lex, db_name_lex_cstr,
                      table_name_lex_cstr))
    goto end;

  table_list= lex->select_lex->table_list.first;

  if (is_show_fields_or_keys)
  {
    /*
      Restore thd->temporary_tables to be able to process
      temporary tables (only for 'show index' & 'show columns').
      This should be changed when processing of temporary tables for
      I_S tables will be done.
    */
    thd->temporary_tables= open_tables_state_backup->temporary_tables;
  }
  else
  {
    /*
      Apply optimization flags for table opening which are relevant for
      this I_S table. We can't do this for SHOW COLUMNS/KEYS because of
      backward compatibility.
    */
    table_list->i_s_requested_object= schema_table->i_s_requested_object;
  }

  /*
    Let us set fake sql_command so views won't try to merge
    themselves into main statement. If we don't do this,
    SELECT * from information_schema.xxxx will cause problems.
    SQLCOM_SHOW_FIELDS is used because it satisfies
    'only_view_structure()'.
  */
  lex->sql_command= SQLCOM_SHOW_FIELDS;

  /*
    Filter out deprecation warnings caused by deprecation of
    the partition engine. The presence of these depend on TDC
    cache behavior. Instead, push a warning later to get
    deterministic and repeatable behavior.
  */
  {
    // Put in separate scope due to gotos crossing the initialization.
    Silence_deprecation_warnings deprecation_silencer;
    thd->push_internal_handler(&deprecation_silencer);

    result= open_temporary_tables(thd, table_list);

    if (!result)
      result= open_tables_for_query(thd, table_list,
                                    MYSQL_OPEN_IGNORE_FLUSH |
                                    MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL |
                                    (can_deadlock ?
                                     MYSQL_OPEN_FAIL_ON_MDL_CONFLICT : 0));
    thd->pop_internal_handler();
  }

  if (!result && table_list->is_view_or_derived())
  {
    result= table_list->resolve_derived(thd, false);
    if (!result)
      result= table_list->setup_materialized_derived(thd);
  }
  /*
    Restore old value of sql_command back as it is being looked at in
    process_table() function.
  */
  lex->sql_command= old_lex->sql_command;

  DEBUG_SYNC(thd, "after_open_table_ignore_flush");

  /*
    XXX:  show_table_list has a flag i_is_requested,
    and when it's set, open_tables_for_query()
    can return an error without setting an error message
    in THD, which is a hack. This is why we have to
    check for res, then for thd->is_error() and only then
    for thd->main_da.sql_errno().

    Again we don't do this for SHOW COLUMNS/KEYS because
    of backward compatibility.
  */
  if (!is_show_fields_or_keys && result && thd->is_error() &&
      thd->get_stmt_da()->mysql_errno() == ER_NO_SUCH_TABLE)
  {
    /*
      Hide error for a non-existing table.
      For example, this error can occur when we use a where condition
      with a db name and table, but the table does not exist.
    */
    result= false;
    thd->clear_error();
  }
  else
  {
    result= schema_table->process_table(thd, table_list,
                                        table, result,
                                        orig_db_name,
                                        orig_table_name);
  }


end:
  lex->unit->cleanup(true);

  /* Restore original LEX value, statement's arena and THD arena values. */
  lex_end(thd->lex);

  // Free items, before restoring backup_arena below.
  DBUG_ASSERT(i_s_arena.free_list == NULL);
  thd->free_items();

  /*
    For safety reset list of open temporary tables before closing
    all tables open within this Open_tables_state.
  */
  thd->temporary_tables= NULL;
  close_thread_tables(thd);
  /*
    Release metadata lock we might have acquired.
    See comment in fill_schema_table_from_frm() for details.
  */
  thd->mdl_context.rollback_to_savepoint(open_tables_state_backup->mdl_system_tables_svp);

  thd->lex= old_lex;

  thd->stmt_arena= old_arena;
  thd->restore_active_arena(&i_s_arena, &backup_arena);

  DBUG_RETURN(result);
}


/**
  @brief          Fill I_S table for SHOW TABLE NAMES commands

  @param[in]      thd                      thread handler
  @param[in]      table                    TABLE struct for I_S table
  @param[in]      db_name                  database name
  @param[in]      table_name               table name
  @param[in]      with_i_schema            I_S table if TRUE

  @return         Operation status
    @retval       0           success
    @retval       1           error
*/

static int fill_schema_table_names(THD *thd, TABLE *table,
                                   LEX_STRING *db_name, LEX_STRING *table_name,
                                   bool with_i_schema,
                                   bool need_table_type)
{
  /* Avoid opening FRM files if table type is not needed. */
  if (need_table_type)
  {
    if (with_i_schema)
    {
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"),
                             system_charset_info);
    }
    else
    {
      enum legacy_db_type not_used;
      char path[FN_REFLEN + 1];
      (void) build_table_filename(path, sizeof(path) - 1, db_name->str, 
                                  table_name->str, reg_ext, 0);
      switch (dd_frm_type(thd, path, &not_used)) {
      case FRMTYPE_ERROR:
        table->field[3]->store(STRING_WITH_LEN("ERROR"),
                               system_charset_info);
        break;
      case FRMTYPE_TABLE:
        table->field[3]->store(STRING_WITH_LEN("BASE TABLE"),
                               system_charset_info);
        break;
      case FRMTYPE_VIEW:
        table->field[3]->store(STRING_WITH_LEN("VIEW"),
                               system_charset_info);
        break;
      default:
        DBUG_ASSERT(0);
      }
    if (thd->is_error() &&
        thd->get_stmt_da()->mysql_errno() == ER_NO_SUCH_TABLE)
      {
        thd->clear_error();
        return 0;
      }
    }
  }
  if (schema_table_store_record(thd, table))
    return 1;
  return 0;
}


/**
  @brief          Get open table method

  @details        The function calculates the method which will be used
                  for table opening:
                  SKIP_OPEN_TABLE - do not open table
                  OPEN_FRM_ONLY   - open FRM file only
                  OPEN_FULL_TABLE - open FRM, data, index files
  @param[in]      tables               I_S table table_list
  @param[in]      schema_table         I_S table struct
  @param[in]      schema_table_idx     I_S table index

  @return         return a set of flags
    @retval       SKIP_OPEN_TABLE | OPEN_FRM_ONLY | OPEN_FULL_TABLE
*/

uint get_table_open_method(TABLE_LIST *tables,
                                  ST_SCHEMA_TABLE *schema_table,
                                  enum enum_schema_tables schema_table_idx)
{
  /*
    determine which method will be used for table opening
  */
  if (schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE)
  {
    Field **ptr, *field;
    int table_open_method= 0, field_indx= 0;
    uint star_table_open_method= OPEN_FULL_TABLE;
    bool used_star= true;                  // true if '*' is used in select
    for (ptr=tables->table->field; (field= *ptr) ; ptr++)
    {
      star_table_open_method=
        min(star_table_open_method,
            schema_table->fields_info[field_indx].open_method);
      if (bitmap_is_set(tables->table->read_set, field->field_index))
      {
        used_star= false;
        table_open_method|= schema_table->fields_info[field_indx].open_method;
      }
      field_indx++;
    }
    if (used_star)
      return star_table_open_method;
    return table_open_method;
  }
  /* I_S tables which use get_all_tables but can not be optimized */
  return (uint) OPEN_FULL_TABLE;
}


/**
   Try acquire high priority share metadata lock on a table (with
   optional wait for conflicting locks to go away).

   @param thd            Thread context.
   @param mdl_request    Pointer to memory to be used for MDL_request
                         object for a lock request.
   @param table          Table list element for the table
   @param can_deadlock   Indicates that deadlocks are possible due to
                         metadata locks, so to avoid them we should not
                         wait in case if conflicting lock is present.

   @note This is an auxiliary function to be used in cases when we want to
         access table's description by looking up info in TABLE_SHARE without
         going through full-blown table open.
   @note This function assumes that there are no other metadata lock requests
         in the current metadata locking context.

   @retval FALSE  No error, if lock was obtained TABLE_LIST::mdl_request::ticket
                  is set to non-NULL value.
   @retval TRUE   Some error occured (probably thread was killed).
*/

static bool
try_acquire_high_prio_shared_mdl_lock(THD *thd, TABLE_LIST *table,
                                      bool can_deadlock)
{
  bool error;
  MDL_REQUEST_INIT(&table->mdl_request,
                   MDL_key::TABLE, table->db, table->table_name,
                   MDL_SHARED_HIGH_PRIO, MDL_TRANSACTION);

  if (can_deadlock)
  {
    /*
      When .FRM is being open in order to get data for an I_S table,
      we might have some tables not only open but also locked.
      E.g. this happens when a SHOW or I_S statement is run
      under LOCK TABLES or inside a stored function.
      By waiting for the conflicting metadata lock to go away we
      might create a deadlock which won't entirely belong to the
      MDL subsystem and thus won't be detectable by this subsystem's
      deadlock detector. To avoid such situation, when there are
      other locked tables, we prefer not to wait on a conflicting
      lock.
    */
    error= thd->mdl_context.try_acquire_lock(&table->mdl_request);
  }
  else
    error= thd->mdl_context.acquire_lock(&table->mdl_request,
                                         thd->variables.lock_wait_timeout);

  return error;
}


/**
  @brief          Fill I_S table with data from FRM file only

  @param[in]      thd                      thread handler
  @param[in]      table                    TABLE struct for I_S table
  @param[in]      schema_table             I_S table struct
  @param[in]      db_name                  database name
  @param[in]      table_name               table name
  @param[in]      schema_table_idx         I_S table index
  @param[in]      open_tables_state_backup Open_tables_state object which is used
                                           to save/restore original state of metadata
                                           locks.
  @param[in]      can_deadlock             Indicates that deadlocks are possible
                                           due to metadata locks, so to avoid
                                           them we should not wait in case if
                                           conflicting lock is present.

  @return         Operation status
    @retval       0           Table is processed and we can continue
                              with new table
    @retval       1           It's view and we have to use
                              open_tables function for this table
*/

static int fill_schema_table_from_frm(THD *thd, TABLE_LIST *tables,
                                      ST_SCHEMA_TABLE *schema_table,
                                      LEX_STRING *db_name,
                                      LEX_STRING *table_name,
                                      enum enum_schema_tables schema_table_idx,
                                      Open_tables_backup *open_tables_state_backup,
                                      bool can_deadlock)
{
  TABLE *table= tables->table;
  TABLE_SHARE *share;
  TABLE_LIST table_list;
  uint res= 0;
  int not_used;
  my_hash_value_type hash_value;
  const char *key;
  size_t key_length;
  char db_name_buff[NAME_LEN + 1], table_name_buff[NAME_LEN + 1];

  DBUG_ASSERT(db_name->length <= NAME_LEN);
  DBUG_ASSERT(table_name->length <= NAME_LEN);

  if (lower_case_table_names)
  {
    /*
      In lower_case_table_names > 0 metadata locking and table definition
      cache subsystems require normalized (lowercased) database and table
      names as input.
    */
    my_stpcpy(db_name_buff, db_name->str);
    my_stpcpy(table_name_buff, table_name->str);
    my_casedn_str(files_charset_info, db_name_buff);
    my_casedn_str(files_charset_info, table_name_buff);
    table_list.db= db_name_buff;
    table_list.table_name= table_name_buff;
  }
  else
  {
    table_list.table_name= table_name->str;
    table_list.db= db_name->str;
  }

  /*
    TODO: investigate if in this particular situation we can get by
          simply obtaining internal lock of the data-dictionary
          instead of obtaining full-blown metadata lock.
  */
  if (try_acquire_high_prio_shared_mdl_lock(thd, &table_list, can_deadlock))
  {
    /*
      Some error occured (most probably we have been killed while
      waiting for conflicting locks to go away), let the caller to
      handle the situation.
    */
    return 1;
  }

  if (! table_list.mdl_request.ticket)
  {
    /*
      We are in situation when we have encountered conflicting metadata
      lock and deadlocks can occur due to waiting for it to go away.
      So instead of waiting skip this table with an appropriate warning.
    */
    DBUG_ASSERT(can_deadlock);

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_I_S_SKIPPED_TABLE,
                        ER(ER_WARN_I_S_SKIPPED_TABLE),
                        table_list.db, table_list.table_name);
    return 0;
  }

  if (schema_table->i_s_requested_object & OPEN_TRIGGER_ONLY)
  {
    if (!Trigger_loader::trg_file_exists(db_name->str, table_name->str))
      goto end;

    Table_trigger_dispatcher d(db_name->str, table_name->str);

    if (!d.check_n_load(thd, true))
    {
      TABLE tbl;

      init_sql_alloc(key_memory_table_triggers_list,
                     &tbl.mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);

      tbl.triggers= &d;
      table_list.table= &tbl;

      res= schema_table->process_table(thd, &table_list, table,
                                       res, db_name, table_name);

      table_list.table= NULL;
      tbl.triggers= NULL;
    }

    goto end;
  }

  key_length= get_table_def_key(&table_list, &key);
  hash_value= my_calc_hash(&table_def_cache, (uchar*) key, key_length);
  mysql_mutex_lock(&LOCK_open);

  /*
    Filter out deprecation warnings caused by deprecation of
    the partition engine. The presence of these depend on TDC
    cache behavior. Instead, push a warning later to get
    deterministic and repeatable behavior.
  */
  {
    // Put in separate scope due to gotos crossing the initialization.
    Silence_deprecation_warnings deprecation_silencer;
    thd->push_internal_handler(&deprecation_silencer);

    share= get_table_share(thd, &table_list, key,
                           key_length, OPEN_VIEW, &not_used, hash_value);

    thd->pop_internal_handler();
  }

  if (!share)
  {
    res= 0;
    goto end_unlock;
  }

  if (share->is_view)
  {
    if (schema_table->i_s_requested_object & OPEN_TABLE_ONLY)
    {
      /* skip view processing */
      res= 0;
      goto end_share;
    }
    else if (schema_table->i_s_requested_object & OPEN_VIEW_FULL)
    {
      /*
        tell get_all_tables() to fall back to open_tables_for_query()
      */
      res= 1;
      goto end_share;
    }
  }

  if (share->is_view)
  {
    bool view_open_result= open_and_read_view(thd, share, &table_list);

    release_table_share(share);
    mysql_mutex_unlock(&LOCK_open);

    if (!view_open_result)
    {
      // Actual view query is not needed, just indicate that this is a view:
      table_list.set_view_query((LEX *) 1);
      res= schema_table->process_table(thd, &table_list, table,
                                       res, db_name, table_name);
    }
    goto end;
  }

  {
    TABLE tbl;
    init_sql_alloc(key_memory_table_triggers_list,
                   &tbl.mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);

    if (!open_table_from_share(thd, share, table_name->str, 0,
                               (EXTRA_RECORD | OPEN_FRM_FILE_ONLY),
                               thd->open_options, &tbl, false))
    {
      tbl.s= share;
      table_list.table= &tbl;
      table_list.set_view_query((LEX*) share->is_view);
      res= schema_table->process_table(thd, &table_list, table,
                                       res, db_name, table_name);
      closefrm(&tbl, 0);
      free_root(&tbl.mem_root, MYF(0));
      my_free((void *) tbl.alias);
    }
  }

end_share:
  release_table_share(share);

end_unlock:
  mysql_mutex_unlock(&LOCK_open);

end:
  /*
    Release metadata lock we might have acquired.

    Without this step metadata locks acquired for each table processed
    will be accumulated. In situation when a lot of tables are processed
    by I_S query this will result in transaction with too many metadata
    locks. As result performance of acquisition of new lock will suffer.

    Of course, the fact that we don't hold metadata lock on tables which
    were processed till the end of I_S query makes execution less isolated
    from concurrent DDL. Consequently one might get 'dirty' results from
    such a query. But we have never promised serializability of I_S queries
    anyway.

    We don't have any tables open since we took backup, so rolling back to
    savepoint is safe.
  */
  DBUG_ASSERT(thd->open_tables == NULL);
  thd->mdl_context.rollback_to_savepoint(open_tables_state_backup->mdl_system_tables_svp);
  thd->clear_error();
  return res;
}


/**
  Trigger_error_handler is intended to intercept and silence SQL conditions
  that might happen during trigger loading for SHOW statements.
  The potential SQL conditions are:

    - ER_PARSE_ERROR -- this error is thrown if a trigger definition file
      is damaged or contains invalid CREATE TRIGGER statement. That should
      not happen in normal life.

    - ER_TRG_NO_DEFINER -- this warning is thrown when we're loading a
      trigger created/imported in/from the version of MySQL, which does not
      support trigger definers.

    - ER_TRG_NO_CREATION_CTX -- this warning is thrown when we're loading a
      trigger created/imported in/from the version of MySQL, which does not
      support trigger creation contexts.
*/

class Trigger_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_PARSE_ERROR ||
        sql_errno == ER_TRG_NO_DEFINER ||
        sql_errno == ER_TRG_NO_CREATION_CTX)
      return true;

    return false;
  }
};


class Silence_deprecation_no_replacement_warnings: public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (sql_errno == ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT)
      return true;

    return false;
  }
};


/**
  @brief          Fill I_S tables whose data are retrieved
                  from frm files and storage engine

  @details        The information schema tables are internally represented as
                  temporary tables that are filled at query execution time.
                  Those I_S tables whose data are retrieved
                  from frm files and storage engine are filled by the function
                  get_all_tables().

  @param[in]      thd                      thread handler
  @param[in]      tables                   I_S table
  @param[in]      cond                     'WHERE' condition

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/

int get_all_tables(THD *thd, TABLE_LIST *tables, Item *cond)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  SELECT_LEX *lsel= tables->schema_select_lex;
  ST_SCHEMA_TABLE *schema_table= tables->schema_table;
  LOOKUP_FIELD_VALUES lookup_field_vals;
  LEX_STRING *db_name, *table_name;
  bool with_i_schema;
  enum enum_schema_tables schema_table_idx;
  List<LEX_STRING> db_names;
  List_iterator_fast<LEX_STRING> it(db_names);
  Item *partial_cond= 0;
  int error= 1;
  Open_tables_backup open_tables_state_backup;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *sctx= thd->security_context();
#endif
  uint table_open_method;
  bool can_deadlock;

  DBUG_ENTER("get_all_tables");

  MEM_ROOT tmp_mem_root;
  init_sql_alloc(key_memory_get_all_tables, &tmp_mem_root,
                 TABLE_ALLOC_BLOCK_SIZE, 0);

  /*
    In cases when SELECT from I_S table being filled by this call is
    part of statement which also uses other tables or is being executed
    under LOCK TABLES or is part of transaction which also uses other
    tables waiting for metadata locks which happens below might result
    in deadlocks.
    To avoid them we don't wait if conflicting metadata lock is
    encountered and skip table with emitting an appropriate warning.
  */
  can_deadlock= thd->mdl_context.has_locks();

  /*
    We should not introduce deadlocks even if we already have some
    tables open and locked, since we won't lock tables which we will
    open and will ignore pending exclusive metadata locks for these
    tables by using high-priority requests for shared metadata locks.
  */
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup);

  schema_table_idx= get_schema_table_idx(schema_table);
  tables->table_open_method= table_open_method=
    get_table_open_method(tables, schema_table, schema_table_idx);
  DBUG_PRINT("open_method", ("%d", tables->table_open_method));
  /* 
    this branch processes SHOW FIELDS, SHOW INDEXES commands.
    see sql_parse.cc, prepare_schema_table() function where
    this values are initialized
  */
  if (lsel && lsel->table_list.first)
  {
    LEX_STRING db_name, table_name;

    db_name.str= const_cast<char*>(lsel->table_list.first->db);
    db_name.length= lsel->table_list.first->db_length;

    table_name.str= const_cast<char*>(lsel->table_list.first->table_name);
    table_name.length= lsel->table_list.first->table_name_length;

    error= fill_schema_table_by_open(thd, &tmp_mem_root, TRUE,
                                     table, schema_table,
                                     &db_name, &table_name,
                                     &open_tables_state_backup,
                                     can_deadlock);
    goto err;
  }

  if (get_lookup_field_values(thd, cond, tables, &lookup_field_vals))
  {
    error= 0;
    goto err;
  }

  DBUG_PRINT("INDEX VALUES",("db_name='%s', table_name='%s'",
                             STR_OR_NIL(lookup_field_vals.db_value.str),
                             STR_OR_NIL(lookup_field_vals.table_value.str)));

  if (!lookup_field_vals.wild_db_value && !lookup_field_vals.wild_table_value)
  {
    /* 
      if lookup value is empty string then
      it's impossible table name or db name
    */
    if ((lookup_field_vals.db_value.str &&
         !lookup_field_vals.db_value.str[0]) ||
        (lookup_field_vals.table_value.str &&
         !lookup_field_vals.table_value.str[0]))
    {
      error= 0;
      goto err;
    }
  }

  if (lookup_field_vals.db_value.length &&
      !lookup_field_vals.wild_db_value)
    tables->has_db_lookup_value= TRUE;
  if (lookup_field_vals.table_value.length &&
      !lookup_field_vals.wild_table_value) 
    tables->has_table_lookup_value= TRUE;

  if (tables->has_db_lookup_value && tables->has_table_lookup_value)
    partial_cond= 0;
  else
    partial_cond= make_cond_for_info_schema(cond, tables);

  if (lex->describe)
  {
    /* EXPLAIN SELECT */
    error= 0;
    goto err;
  }

  if (make_db_list(thd, &db_names, &lookup_field_vals, &with_i_schema, &tmp_mem_root))
    goto err;
  it.rewind(); /* To get access to new elements in basis list */
  while ((db_name= it++))
  {
    DBUG_ASSERT(db_name->length <= NAME_LEN);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (!(check_access(thd, SELECT_ACL, db_name->str,
                       &thd->col_access, NULL, 0, 1) ||
          (!thd->col_access && check_grant_db(thd, db_name->str))) ||
        sctx->check_access(DB_ACLS | SHOW_DB_ACL, true) ||
        acl_get(sctx->host().str, sctx->ip().str,
                sctx->priv_user().str, db_name->str, 0))
#endif
    {
      List<LEX_STRING> table_names;
      int res= make_table_name_list(thd, &table_names, lex,
                                    &lookup_field_vals,
                                    with_i_schema, db_name, &tmp_mem_root);
      if (res == 2)   /* Not fatal error, continue */
        continue;
      if (res)
        goto err;

      List_iterator_fast<LEX_STRING> it_files(table_names);
      while ((table_name= it_files++))
      {
        DBUG_ASSERT(table_name->length <= NAME_LEN);
	restore_record(table, s->default_values);
        table->field[schema_table->idx_field1]->
          store(db_name->str, db_name->length, system_charset_info);
        table->field[schema_table->idx_field2]->
          store(table_name->str, table_name->length, system_charset_info);

        if (!partial_cond || partial_cond->val_int())
        {
          /*
            If table is I_S.tables and open_table_method is 0 (eg SKIP_OPEN)
            we can skip table opening and we don't have lookup value for 
            table name or lookup value is wild string(table name list is
            already created by make_table_name_list() function).
          */
          if (!table_open_method && schema_table_idx == SCH_TABLES &&
              (!lookup_field_vals.table_value.length ||
               lookup_field_vals.wild_table_value))
          {
            table->field[0]->store(STRING_WITH_LEN("def"), system_charset_info);
            if (schema_table_store_record(thd, table))
              goto err;      /* Out of space in temporary table */
            continue;
          }

          /* SHOW TABLE NAMES command */
          if (schema_table_idx == SCH_TABLE_NAMES)
          {
            if (fill_schema_table_names(thd, tables->table, db_name,
                                        table_name, with_i_schema,
                                        lex->verbose))
              continue;
          }
          else
          {
            if (!(table_open_method & ~OPEN_FRM_ONLY) &&
                !with_i_schema)
            {
              /*
                Here we need to filter out warnings, which can happen
                during loading of triggers in fill_schema_table_from_frm(),
                because we don't need those warnings to pollute output of
                SELECT from I_S / SHOW-statements.
              */

              Trigger_error_handler err_handler;
              thd->push_internal_handler(&err_handler);

              int res= fill_schema_table_from_frm(thd, tables, schema_table,
                                                  db_name, table_name,
                                                  schema_table_idx,
                                                  &open_tables_state_backup,
                                                  can_deadlock);

              thd->pop_internal_handler();

              if (!res)
                continue;
            }

            DEBUG_SYNC(thd, "before_open_in_get_all_tables");

            if (fill_schema_table_by_open(thd, &tmp_mem_root, FALSE,
                                          table, schema_table,
                                          db_name, table_name,
                                          &open_tables_state_backup,
                                          can_deadlock))
              goto err;
          }
        }
      }
      /*
        If we have information schema its always the first table and only
        the first table. Reset for other tables.
      */
      with_i_schema= 0;
    }
  }
  error= 0;
err:

  free_root(&tmp_mem_root, MYF(0));
  thd->restore_backup_open_tables_state(&open_tables_state_backup);

  DBUG_RETURN(error);
}


bool store_schema_shemata(THD* thd, TABLE *table, LEX_STRING *db_name,
                          const CHARSET_INFO *cs)
{
  restore_record(table, s->default_values);
  table->field[0]->store(STRING_WITH_LEN("def"), system_charset_info);
  table->field[1]->store(db_name->str, db_name->length, system_charset_info);
  table->field[2]->store(cs->csname, strlen(cs->csname), system_charset_info);
  table->field[3]->store(cs->name, strlen(cs->name), system_charset_info);
  return schema_table_store_record(thd, table);
}


int fill_schema_schemata(THD *thd, TABLE_LIST *tables, Item *cond)
{
  /*
    TODO: fill_schema_shemata() is called when new client is connected.
    Returning error status in this case leads to client hangup.
  */

  /*
   * A temporary class is created to free tmp_mem_root when we return from
   * this function, since we have 'return' from this function from many
   * places. This is just to avoid goto.
   */
  class free_tmp_mem_root
  {
  public:
    free_tmp_mem_root()
    {
      init_sql_alloc(key_memory_fill_schema_schemata, &tmp_mem_root,
                     TABLE_ALLOC_BLOCK_SIZE, 0);
    }
    ~free_tmp_mem_root()
    {
      free_root(&tmp_mem_root, MYF(0));
    }
    MEM_ROOT tmp_mem_root;
  };

  free_tmp_mem_root dummy_member;

  LOOKUP_FIELD_VALUES lookup_field_vals;
  List<LEX_STRING> db_names;
  LEX_STRING *db_name;
  bool with_i_schema;
  HA_CREATE_INFO create;
  TABLE *table= tables->table;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *sctx= thd->security_context();
#endif
  DBUG_ENTER("fill_schema_shemata");

  if (get_lookup_field_values(thd, cond, tables, &lookup_field_vals))
    DBUG_RETURN(0);

  DBUG_PRINT("INDEX VALUES",("db_name='%s', table_name='%s'",
                             lookup_field_vals.db_value.str,
                             lookup_field_vals.table_value.str));
  if (make_db_list(thd, &db_names, &lookup_field_vals,
                   &with_i_schema, &dummy_member.tmp_mem_root))
    DBUG_RETURN(1);

  /*
    If we have lookup db value we should check that the database exists
  */
  if(lookup_field_vals.db_value.str && !lookup_field_vals.wild_db_value &&
     !with_i_schema)
  {
    char path[FN_REFLEN+16];
    size_t path_len;
    MY_STAT stat_info;
    if (!lookup_field_vals.db_value.str[0])
      DBUG_RETURN(0);
    path_len= build_table_filename(path, sizeof(path) - 1,
                                   lookup_field_vals.db_value.str, "", "", 0);
    path[path_len-1]= 0;
    if (!mysql_file_stat(key_file_misc, path, &stat_info, MYF(0)))
      DBUG_RETURN(0);
  }

  List_iterator_fast<LEX_STRING> it(db_names);
  while ((db_name=it++))
  {
    DBUG_ASSERT(db_name->length <= NAME_LEN);
    if (with_i_schema)       // information schema name is always first in list
    {
      if (store_schema_shemata(thd, table, db_name,
                               system_charset_info))
        DBUG_RETURN(1);
      with_i_schema= 0;
      continue;
    }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (sctx->check_access(DB_ACLS | SHOW_DB_ACL, true) ||
	acl_get(sctx->host().str, sctx->ip().str,
                sctx->priv_user().str, db_name->str, 0) ||
                !check_grant_db(thd, db_name->str))
#endif
    {
      load_db_opt_by_name(thd, db_name->str, &create);
      if (store_schema_shemata(thd, table, db_name,
                               create.default_table_charset))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


static int get_schema_tables_record(THD *thd, TABLE_LIST *tables,
				    TABLE *table, bool res,
				    LEX_STRING *db_name,
				    LEX_STRING *table_name)
{
  const char *tmp_buff;
  MYSQL_TIME time;
  int info_error= 0;
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_tables_record");

  restore_record(table, s->default_values);
  table->field[0]->store(STRING_WITH_LEN("def"), cs);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(table_name->str, table_name->length, cs);

  if (res)
  {
    /* There was a table open error, so set the table type and return */
    if (tables->is_view())
      table->field[3]->store(STRING_WITH_LEN("VIEW"), cs);
    else if (tables->schema_table)
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    else
      table->field[3]->store(STRING_WITH_LEN("BASE TABLE"), cs);

    goto err;
  }

  if (tables->is_view())
  {
    table->field[3]->store(STRING_WITH_LEN("VIEW"), cs);
    table->field[20]->store(STRING_WITH_LEN("VIEW"), cs);
  }
  else
  {
    char option_buff[350],*ptr;
    TABLE *show_table= tables->table;
    TABLE_SHARE *share= show_table->s;
    handler *file= show_table->file;
    handlerton *tmp_db_type= share->db_type();
    bool is_partitioned= FALSE;

    if (share->tmp_table == SYSTEM_TMP_TABLE)
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    else if (share->tmp_table)
      table->field[3]->store(STRING_WITH_LEN("LOCAL TEMPORARY"), cs);
    else
      table->field[3]->store(STRING_WITH_LEN("BASE TABLE"), cs);

    for (int i= 4; i < 20; i++)
    {
      if (i == 7 || (i > 12 && i < 17) || i == 18)
        continue;
      table->field[i]->set_notnull();
    }

    /* Collect table info from the table share */

    if (share->partition_info_str_len)
    {
      tmp_db_type= share->default_part_db_type;
      is_partitioned= TRUE;
    }

    tmp_buff= (char *) ha_resolve_storage_engine_name(tmp_db_type);
    table->field[4]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[5]->store((longlong) share->frm_version, TRUE);

    ptr=option_buff;

    if (share->min_rows)
    {
      ptr=my_stpcpy(ptr," min_rows=");
      ptr=longlong10_to_str(share->min_rows,ptr,10);
    }

    if (share->max_rows)
    {
      ptr=my_stpcpy(ptr," max_rows=");
      ptr=longlong10_to_str(share->max_rows,ptr,10);
    }

    if (share->avg_row_length)
    {
      ptr=my_stpcpy(ptr," avg_row_length=");
      ptr=longlong10_to_str(share->avg_row_length,ptr,10);
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      ptr=my_stpcpy(ptr," pack_keys=1");

    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      ptr=my_stpcpy(ptr," pack_keys=0");

    if (share->db_create_options & HA_OPTION_STATS_PERSISTENT)
      ptr=my_stpcpy(ptr," stats_persistent=1");

    if (share->db_create_options & HA_OPTION_NO_STATS_PERSISTENT)
      ptr=my_stpcpy(ptr," stats_persistent=0");

    if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_ON)
      ptr=my_stpcpy(ptr," stats_auto_recalc=1");
    else if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_OFF)
      ptr=my_stpcpy(ptr," stats_auto_recalc=0");

    if (share->stats_sample_pages != 0)
    {
      ptr= my_stpcpy(ptr, " stats_sample_pages=");
      ptr= longlong10_to_str(share->stats_sample_pages, ptr, 10);
    }

    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compability */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      ptr=my_stpcpy(ptr," checksum=1");

    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      ptr=my_stpcpy(ptr," delay_key_write=1");

    if (share->row_type != ROW_TYPE_DEFAULT)
      ptr=strxmov(ptr, " row_format=", 
                  ha_row_type[(uint) share->row_type],
                  NullS);

    if (share->key_block_size)
    {
      ptr= my_stpcpy(ptr, " KEY_BLOCK_SIZE=");
      ptr= longlong10_to_str(share->key_block_size, ptr, 10);
    }

    if (share->compress.length > 0)
    {
      /* In the .frm file this option has a max length of 2K. Currently,
      InnoDB uses only the first 5 bytes and the only supported values
      are (ZLIB | LZ4 | NONE). */
      ptr= my_stpcpy(ptr, " COMPRESSION=\"");
      ptr= strxnmov(ptr, 7, share->compress.str, NullS);
      ptr= my_stpcpy(ptr, "\"");
    }

    if (share->encrypt_type.length > 0)
    {
      /* In the .frm file this option has a max length of 2K. Currently,
      InnoDB uses only the first 1 bytes and the only supported values
      are (Y | N). */
      ptr= my_stpcpy(ptr, " ENCRYPTION=\"");
      ptr= strxnmov(ptr, 3, share->encrypt_type.str, NullS);
      ptr= my_stpcpy(ptr, "\"");
    }

    if (is_partitioned)
    {
      ptr= my_stpcpy(ptr, " partitioned");
      /*
        Push deprecation warnings for non-natively partitioned tables. Done here
        instead of in open_binary_frm (silenced by error handler) to get
        predictable and repeatable results without having to flush tables.
      */
      if (share->db_type() && is_ha_partition_handlerton(share->db_type()))
      {
        /*
          For a bootstrap thread, we only print to the error log, otherwise,
          the warning is lost since there is no client connection.
        */
        if (thd->bootstrap)
          sql_print_warning(ER_THD(thd,
                                   ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                            share->db.str, share->table_name.str);
        else
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_DEPRECATED_SYNTAX,
                              ER_THD(thd,
                                     ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                              share->db.str, share->table_name.str);
      }
    }

    table->field[19]->store(option_buff+1,
                            (ptr == option_buff ? 0 : 
                             (uint) (ptr-option_buff)-1), cs);

    tmp_buff= (share->table_charset ?
               share->table_charset->name : "default");

    table->field[17]->store(tmp_buff, strlen(tmp_buff), cs);

    if (share->comment.str)
      table->field[20]->store(share->comment.str, share->comment.length, cs);

    /* Collect table info from the storage engine  */

    if(file)
    {
      /* If info() fails, then there's nothing else to do */
      if ((info_error= file->info(HA_STATUS_VARIABLE |
                                  HA_STATUS_TIME |
                                  HA_STATUS_VARIABLE_EXTRA |
                                  HA_STATUS_AUTO)) != 0)
        goto err;

      enum row_type row_type = file->get_row_type();
      switch (row_type) {
      case ROW_TYPE_NOT_USED:
      case ROW_TYPE_DEFAULT:
        tmp_buff= ((share->db_options_in_use &
                    HA_OPTION_COMPRESS_RECORD) ? "Compressed" :
                   (share->db_options_in_use & HA_OPTION_PACK_RECORD) ?
                   "Dynamic" : "Fixed");
        break;
      case ROW_TYPE_FIXED:
        tmp_buff= "Fixed";
        break;
      case ROW_TYPE_DYNAMIC:
        tmp_buff= "Dynamic";
        break;
      case ROW_TYPE_COMPRESSED:
        tmp_buff= "Compressed";
        break;
      case ROW_TYPE_REDUNDANT:
        tmp_buff= "Redundant";
        break;
      case ROW_TYPE_COMPACT:
        tmp_buff= "Compact";
        break;
      case ROW_TYPE_PAGE:
        tmp_buff= "Paged";
        break;
      }

      table->field[6]->store(tmp_buff, strlen(tmp_buff), cs);

      if (!tables->schema_table)
      {
        table->field[7]->store((longlong) file->stats.records, TRUE);
        table->field[7]->set_notnull();
      }
      table->field[8]->store((longlong) file->stats.mean_rec_length, TRUE);
      table->field[9]->store((longlong) file->stats.data_file_length, TRUE);
      if (file->stats.max_data_file_length)
      {
        table->field[10]->store((longlong) file->stats.max_data_file_length,
                                TRUE);
      }
      table->field[11]->store((longlong) file->stats.index_file_length, TRUE);
      table->field[12]->store((longlong) file->stats.delete_length, TRUE);
      if (show_table->found_next_number_field)
      {
        table->field[13]->store((longlong) file->stats.auto_increment_value,
                                TRUE);
        table->field[13]->set_notnull();
      }
      if (file->stats.create_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.create_time);
        table->field[14]->store_time(&time);
        table->field[14]->set_notnull();
      }
      if (file->stats.update_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.update_time);
        table->field[15]->store_time(&time);
        table->field[15]->set_notnull();
      }
      if (file->stats.check_time)
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (my_time_t) file->stats.check_time);
        table->field[16]->store_time(&time);
        table->field[16]->set_notnull();
      }
      if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
      {
        table->field[18]->store((longlong) file->checksum(), TRUE);
        table->field[18]->set_notnull();
      }
    }
  }

err:
  if (res || info_error)
  {
    /*
      If an error was encountered, push a warning, set the TABLE COMMENT
      column with the error text, and clear the error so that the operation
      can continue.
    */
    const char *error= thd->is_error() ? thd->get_stmt_da()->message_text() : "";
    table->field[20]->store(error, strlen(error), cs);

    if (thd->is_error())
    {
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
      thd->clear_error();
    }
  }

  DBUG_RETURN(schema_table_store_record(thd, table));
}


/**
  @brief    Store field characteristics into appropriate I_S table columns
            starting from DATA_TYPE column till DTD_IDENTIFIER column.

  @param[in]      thd               Thread context.
  @param[in]      table             I_S table
  @param[in]      field             processed field
  @param[in]      cs                I_S table charset
  @param[in]      offset            offset from beginning of table
                                    to DATE_TYPE column in I_S table
  @return         void
*/

void store_column_type(THD *thd, TABLE *table, Field *field, CHARSET_INFO *cs,
                       uint offset)
{
  bool is_blob;
  int decimals, field_length;
  const char *tmp_buff;
  char column_type_buff[MAX_FIELD_WIDTH];
  String column_type(column_type_buff, sizeof(column_type_buff), cs);
  enum_field_types field_type= field->real_type();
  uint32 orig_column_type_length;

  field->sql_type(column_type);
  orig_column_type_length= column_type.length();

  /*
    If the session variable 'show_old_temporals' is enabled and the field
    is a temporal type of old format, add a comment to the COLUMN_TYPE
    indicate the same.
  */
  if (thd->variables.show_old_temporals &&
      (field_type == MYSQL_TYPE_TIME || field_type == MYSQL_TYPE_DATETIME ||
       field_type == MYSQL_TYPE_TIMESTAMP))
    column_type.append(" /* 5.5 binary format */");

  /* DTD_IDENTIFIER column */
  table->field[offset + 8]->store(column_type.ptr(), column_type.length(), cs);
  column_type.length(orig_column_type_length);
  table->field[offset + 8]->set_notnull();
  /*
    DATA_TYPE column:
    MySQL column type has the following format:
    base_type [(dimension)] [unsigned] [zerofill].
    For DATA_TYPE column we extract only base type.
  */
  tmp_buff= strchr(column_type.ptr(), '(');
  if (!tmp_buff)
    /*
      if there is no dimention part then check the presence of
      [unsigned] [zerofill] attributes and cut them of if exist.
    */
    tmp_buff= strchr(column_type.ptr(), ' ');
  table->field[offset]->store(column_type.ptr(),
                              (tmp_buff ? tmp_buff - column_type.ptr() :
                               column_type.length()), cs);

  is_blob= (field->type() == MYSQL_TYPE_BLOB);
  if (field->has_charset() || is_blob ||
      field->real_type() == MYSQL_TYPE_VARCHAR ||  // For varbinary type
      field->real_type() == MYSQL_TYPE_STRING)     // For binary type
  {
    uint32 octet_max_length= field->max_display_length();
    if (is_blob && octet_max_length != 4294967295U)
      octet_max_length /= field->charset()->mbmaxlen;
    longlong char_max_len= is_blob ? 
      (longlong) octet_max_length / field->charset()->mbminlen :
      (longlong) octet_max_length / field->charset()->mbmaxlen;
    /* CHARACTER_MAXIMUM_LENGTH column*/
    table->field[offset + 1]->store(char_max_len, TRUE);
    table->field[offset + 1]->set_notnull();
    /* CHARACTER_OCTET_LENGTH column */
    table->field[offset + 2]->store((longlong) octet_max_length, TRUE);
    table->field[offset + 2]->set_notnull();
  }

  /*
    Calculate field_length and decimals.
    They are set to -1 if they should not be set (we should return NULL)
  */

  decimals= field->decimals();
  switch (field->type()) {
  case MYSQL_TYPE_NEWDECIMAL:
    field_length= ((Field_new_decimal*) field)->precision;
    break;
  case MYSQL_TYPE_DECIMAL:
    field_length= field->field_length - (decimals  ? 2 : 1);
    break;
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_INT24:
    field_length= field->max_display_length() - 1;
    break;
  case MYSQL_TYPE_LONGLONG:
    field_length= field->max_display_length() - 
      ((field->flags & UNSIGNED_FLAG) ? 0 : 1);
    break;
  case MYSQL_TYPE_BIT:
    field_length= field->max_display_length();
    decimals= -1;                             // return NULL
    break;
  case MYSQL_TYPE_FLOAT:  
  case MYSQL_TYPE_DOUBLE:
    field_length= field->field_length;
    if (decimals == NOT_FIXED_DEC)
      decimals= -1;                           // return NULL
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_TIME:
    /* DATETIME_PRECISION column */
    table->field[offset + 5]->store(field->decimals(), TRUE);
    table->field[offset + 5]->set_notnull();
    field_length= decimals= -1;
    break;
  default:
    field_length= decimals= -1;
    break;
  }

  /* NUMERIC_PRECISION column */
  if (field_length >= 0)
  {
    table->field[offset + 3]->store((longlong) field_length, TRUE);
    table->field[offset + 3]->set_notnull();
  }
  /* NUMERIC_SCALE column */
  if (decimals >= 0)
  {
    table->field[offset + 4]->store((longlong) decimals, TRUE);
    table->field[offset + 4]->set_notnull();
  }
  if (field->has_charset())
  {
    /* CHARACTER_SET_NAME column*/
    tmp_buff= field->charset()->csname;
    table->field[offset + 6]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 6]->set_notnull();
    /* COLLATION_NAME column */
    tmp_buff= field->charset()->name;
    table->field[offset + 7]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 7]->set_notnull();
  }
}


static int get_schema_column_record(THD *thd, TABLE_LIST *tables,
				    TABLE *table, bool res,
				    LEX_STRING *db_name,
				    LEX_STRING *table_name)
{
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  CHARSET_INFO *cs= system_charset_info;
  TABLE *show_table;
  Field **ptr, *field;
  int count;
  DBUG_ENTER("get_schema_column_record");

  if (res)
  {
    if (lex->sql_command != SQLCOM_SHOW_FIELDS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.COLUMS
        rather than in SHOW COLUMNS
      */
      if (thd->is_error())
        push_warning(thd, Sql_condition::SL_WARNING,
                     thd->get_stmt_da()->mysql_errno(),
                     thd->get_stmt_da()->message_text());
      thd->clear_error();
      res= 0;
    }
    DBUG_RETURN(res);
  }

  show_table= tables->table;
  count= 0;
  ptr= show_table->field;
  show_table->use_all_columns();               // Required for default
  restore_record(show_table, s->default_values);

  for (; (field= *ptr) ; ptr++)
  {
    uchar *pos;
    char tmp[MAX_FIELD_WIDTH];
    String type(tmp,sizeof(tmp), system_charset_info);

    DEBUG_SYNC(thd, "get_schema_column");

    if (wild && wild[0] &&
        wild_case_compare(system_charset_info, field->field_name,wild))
      continue;

    count++;
    /* Get default row, with all NULL fields set to NULL */
    restore_record(table, s->default_values);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    uint col_access;
    check_access(thd,SELECT_ACL, db_name->str,
                 &tables->grant.privilege, 0, 0, MY_TEST(tables->schema_table));
    col_access= get_column_grant(thd, &tables->grant,
                                 db_name->str, table_name->str,
                                 field->field_name) & COL_ACLS;
    if (!tables->schema_table && !col_access)
      continue;
    char *end= tmp;
    for (uint bitnr=0; col_access ; col_access>>=1,bitnr++)
    {
      if (col_access & 1)
      {
        *end++=',';
        end=my_stpcpy(end,grant_types.type_names[bitnr]);
      }
    }
    table->field[IS_COLUMNS_PRIVILEGES]->store(tmp+1,
                                               end == tmp ? 0 : 
                                               (uint) (end-tmp-1), cs);

#endif
    table->field[IS_COLUMNS_TABLE_CATALOG]->store(STRING_WITH_LEN("def"), cs);
    table->field[IS_COLUMNS_TABLE_SCHEMA]->store(db_name->str,
                                                 db_name->length, cs);
    table->field[IS_COLUMNS_TABLE_NAME]->store(table_name->str,
                                               table_name->length, cs);
    table->field[IS_COLUMNS_COLUMN_NAME]->store(field->field_name,
                                                strlen(field->field_name), cs);
    table->field[IS_COLUMNS_ORDINAL_POSITION]->store((longlong) count, TRUE);
    field->sql_type(type);
    table->field[IS_COLUMNS_COLUMN_TYPE]->store(type.ptr(), type.length(), cs);

    if (print_default_clause(thd, field, &type, false))
    {
      table->field[IS_COLUMNS_COLUMN_DEFAULT]->store(type.ptr(), type.length(),
                                                    cs);
      table->field[IS_COLUMNS_COLUMN_DEFAULT]->set_notnull();
    }
    pos=(uchar*) ((field->flags & NOT_NULL_FLAG) ?  "NO" : "YES");
    table->field[IS_COLUMNS_IS_NULLABLE]->store((const char*) pos,
                           strlen((const char*) pos), cs);
    store_column_type(thd, table, field, cs, IS_COLUMNS_DATA_TYPE);
    pos=(uchar*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
                 (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
                 (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
    table->field[IS_COLUMNS_COLUMN_KEY]->store((const char*) pos,
                            strlen((const char*) pos), cs);

    if (field->unireg_check == Field::NEXT_NUMBER)
      table->field[IS_COLUMNS_EXTRA]->store(STRING_WITH_LEN("auto_increment"),
                                            cs);
    if (print_on_update_clause(field, &type, true))
      table->field[IS_COLUMNS_EXTRA]->store(type.ptr(), type.length(), cs);
    if (field->gcol_info)
    {
      if (field->stored_in_db)
        table->field[IS_COLUMNS_EXTRA]->
          store(STRING_WITH_LEN("STORED GENERATED"), cs);
      else
        table->field[IS_COLUMNS_EXTRA]->
          store(STRING_WITH_LEN("VIRTUAL GENERATED"), cs);
      table->field[IS_COLUMNS_GENERATION_EXPRESSION]->
        store(field->gcol_info->expr_str.str,field->gcol_info->expr_str.length,
              cs);
    }
    else
      table->field[IS_COLUMNS_GENERATION_EXPRESSION]->set_null();
    table->field[IS_COLUMNS_COLUMN_COMMENT]->store(field->comment.str,
                                                   field->comment.length, cs);
    if (schema_table_store_record(thd, table))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int fill_schema_charsets(THD *thd, TABLE_LIST *tables, Item *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;

  for (cs= all_charsets ;
       cs < all_charsets + array_elements(all_charsets) ;
       cs++)
  {
    CHARSET_INFO *tmp_cs= cs[0];
    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) && 
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        !(tmp_cs->state & MY_CS_HIDDEN) &&
        !(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cs->csname,wild)))
    {
      const char *comment;
      restore_record(table, s->default_values);
      table->field[0]->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      table->field[1]->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      comment= tmp_cs->comment ? tmp_cs->comment : "";
      table->field[2]->store(comment, strlen(comment), scs);
      table->field[3]->store((longlong) tmp_cs->mbmaxlen, TRUE);
      if (schema_table_store_record(thd, table))
        return 1;
    }
  }
  return 0;
}


static my_bool iter_schema_engines(THD *thd, plugin_ref plugin,
                                   void *ptable)
{
  TABLE *table= (TABLE *) ptable;
  handlerton *hton= plugin_data<handlerton*>(plugin);
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  CHARSET_INFO *scs= system_charset_info;
  handlerton *default_type= ha_default_handlerton(thd);
  DBUG_ENTER("iter_schema_engines");


  /* Disabled plugins */
  if (plugin_state(plugin) != PLUGIN_IS_READY)
  {

    struct st_mysql_plugin *plug= plugin_decl(plugin);
    if (!(wild && wild[0] &&
          wild_case_compare(scs, plug->name,wild)))
    {
      restore_record(table, s->default_values);
      table->field[0]->store(plug->name, strlen(plug->name), scs);
      table->field[1]->store(C_STRING_WITH_LEN("NO"), scs);
      table->field[2]->store(plug->descr, strlen(plug->descr), scs);
      if (schema_table_store_record(thd, table))
        DBUG_RETURN(1);
    }
    DBUG_RETURN(0);
  }

  if (!(hton->flags & HTON_HIDDEN))
  {
    LEX_STRING *name= plugin_name(plugin);
    if (!(wild && wild[0] &&
          wild_case_compare(scs, name->str,wild)))
    {
      LEX_STRING yesno[2]= {{ C_STRING_WITH_LEN("NO") },
                            { C_STRING_WITH_LEN("YES") }};
      LEX_STRING *tmp;
      const char *option_name= show_comp_option_name[(int) hton->state];
      restore_record(table, s->default_values);

      table->field[0]->store(name->str, name->length, scs);
      if (hton->state == SHOW_OPTION_YES && default_type == hton)
        option_name= "DEFAULT";
      table->field[1]->store(option_name, strlen(option_name), scs);
      table->field[2]->store(plugin_decl(plugin)->descr,
                             strlen(plugin_decl(plugin)->descr), scs);
      tmp= &yesno[MY_TEST(hton->commit)];
      table->field[3]->store(tmp->str, tmp->length, scs);
      table->field[3]->set_notnull();
      tmp= &yesno[MY_TEST(hton->prepare)];
      table->field[4]->store(tmp->str, tmp->length, scs);
      table->field[4]->set_notnull();
      tmp= &yesno[MY_TEST(hton->savepoint_set)];
      table->field[5]->store(tmp->str, tmp->length, scs);
      table->field[5]->set_notnull();

      if (schema_table_store_record(thd, table))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

int fill_schema_engines(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_schema_engines");
  if (plugin_foreach_with_mask(thd, iter_schema_engines,
                               MYSQL_STORAGE_ENGINE_PLUGIN,
                               ~PLUGIN_IS_FREED, tables->table))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


int fill_schema_collation(THD *thd, TABLE_LIST *tables, Item *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for (cs= all_charsets ;
       cs < all_charsets + array_elements(all_charsets)  ;
       cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) ||
         (tmp_cs->state & MY_CS_HIDDEN) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets;
         cl < all_charsets + array_elements(all_charsets)  ;
         cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) || 
          !my_charset_same(tmp_cs, tmp_cl))
	continue;
      if (!(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cl->name,wild)))
      {
	const char *tmp_buff;
	restore_record(table, s->default_values);
	table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
        table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
        table->field[2]->store((longlong) tmp_cl->number, TRUE);
        tmp_buff= (tmp_cl->state & MY_CS_PRIMARY) ? "Yes" : "";
	table->field[3]->store(tmp_buff, strlen(tmp_buff), scs);
        tmp_buff= (tmp_cl->state & MY_CS_COMPILED)? "Yes" : "";
	table->field[4]->store(tmp_buff, strlen(tmp_buff), scs);
        table->field[5]->store((longlong) tmp_cl->strxfrm_multiply, TRUE);
        if (schema_table_store_record(thd, table))
          return 1;
      }
    }
  }
  return 0;
}


int fill_schema_coll_charset_app(THD *thd, TABLE_LIST *tables, Item *cond)
{
  CHARSET_INFO **cs;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for (cs= all_charsets ;
       cs < all_charsets + array_elements(all_charsets) ;
       cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) || 
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets;
         cl < all_charsets + array_elements(all_charsets) ;
         cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) ||
          (tmp_cl->state & MY_CS_HIDDEN) ||
          !my_charset_same(tmp_cs,tmp_cl))
	continue;
      restore_record(table, s->default_values);
      table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
      if (schema_table_store_record(thd, table))
        return 1;
    }
  }
  return 0;
}


static inline void copy_field_as_string(Field *to_field, Field *from_field)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp_str(buff, sizeof(buff), system_charset_info);
  from_field->val_str(&tmp_str);
  to_field->store(tmp_str.ptr(), tmp_str.length(), system_charset_info);
}


/**
  @brief Store record into I_S.PARAMETERS table

  @param[in]      thd                   thread handler
  @param[in]      table                 I_S table
  @param[in]      proc_table            'mysql.proc' table
  @param[in]      wild                  wild string, not used for now,
                                        will be useful
                                        if we add 'SHOW PARAMETERs'
  @param[in]      full_access           if 1 user has privileges on the routine
  @param[in]      sp_user               user in 'user@host' format

  @return         Operation status
    @retval       0                     ok
    @retval       1                     error
*/

bool store_schema_params(THD *thd, TABLE *table, TABLE *proc_table,
                         const char *wild, bool full_access,
                         const char *sp_user)
{
  TABLE_SHARE share;
  TABLE tbl;
  CHARSET_INFO *cs= system_charset_info;
  char params_buff[MAX_FIELD_WIDTH], returns_buff[MAX_FIELD_WIDTH],
    sp_db_buff[NAME_LEN], sp_name_buff[NAME_LEN], path[FN_REFLEN],
    definer_buff[USERNAME_LENGTH + HOSTNAME_LENGTH + 1];
  String params(params_buff, sizeof(params_buff), cs);
  String returns(returns_buff, sizeof(returns_buff), cs);
  String sp_db(sp_db_buff, sizeof(sp_db_buff), cs);
  String sp_name(sp_name_buff, sizeof(sp_name_buff), cs);
  String definer(definer_buff, sizeof(definer_buff), cs);
  sp_head *sp;
  enum_sp_type routine_type;
  bool free_sp_head;
  DBUG_ENTER("store_schema_params");

  (void) build_table_filename(path, sizeof(path), "", "", "", 0);
  init_tmp_table_share(thd, &share, "", 0, "", path);

  get_field(thd->mem_root, proc_table->field[MYSQL_PROC_FIELD_DB], &sp_db);
  get_field(thd->mem_root, proc_table->field[MYSQL_PROC_FIELD_NAME], &sp_name);
  get_field(thd->mem_root,proc_table->field[MYSQL_PROC_FIELD_DEFINER],&definer);
  routine_type= (enum_sp_type) proc_table->field[MYSQL_PROC_MYSQL_TYPE]->val_int();

  if (!full_access)
    full_access= !strcmp(sp_user, definer.ptr());
  if (!full_access &&
      check_some_routine_access(thd, sp_db.ptr(),sp_name.ptr(),
                                routine_type == SP_TYPE_PROCEDURE))
    DBUG_RETURN(0);

  params.length(0);
  get_field(thd->mem_root, proc_table->field[MYSQL_PROC_FIELD_PARAM_LIST],
            &params);
  returns.length(0);
  if (routine_type == SP_TYPE_FUNCTION)
    get_field(thd->mem_root, proc_table->field[MYSQL_PROC_FIELD_RETURNS],
              &returns);

  sp= sp_load_for_information_schema(thd, proc_table, &sp_db, &sp_name,
                                     (sql_mode_t) proc_table->
                                     field[MYSQL_PROC_FIELD_SQL_MODE]->val_int(),
                                     routine_type,
                                     returns.c_ptr_safe(),
                                     params.c_ptr_safe(),
                                     &free_sp_head);

  if (sp)
  {
    Field *field;
    Create_field *field_def;
    String tmp_string;
    if (routine_type == SP_TYPE_FUNCTION)
    {
      restore_record(table, s->default_values);
      table->field[IS_PARAMETERS_SPECIFIC_CATALOG]->store(STRING_WITH_LEN
                                                          ("def"), cs);
      table->field[IS_PARAMETERS_SPECIFIC_SCHEMA]->store(sp_db.ptr(),
                                                         sp_db.length(), cs);
      table->field[IS_PARAMETERS_SPECIFIC_NAME]->store(sp_name.ptr(),
                                                       sp_name.length(), cs);
      table->field[IS_PARAMETERS_ORDINAL_POSITION]->store((longlong) 0, TRUE);
      get_field(thd->mem_root, proc_table->field[MYSQL_PROC_MYSQL_TYPE],
                &tmp_string);
      table->field[IS_PARAMETERS_ROUTINE_TYPE]->store(tmp_string.ptr(),
                                                      tmp_string.length(), cs);
      field_def= &sp->m_return_field_def;
      field= make_field(&share, (uchar*) 0, field_def->length,
                        (uchar*) "", 0, field_def->pack_flag,
                        field_def->sql_type, field_def->charset,
                        field_def->geom_type, Field::NONE,
                        field_def->interval, "");

      field->table= &tbl;
      field->gcol_info= field_def->gcol_info;
      field->stored_in_db= field_def->stored_in_db;
      tbl.in_use= thd;
      store_column_type(thd, table, field, cs, IS_PARAMETERS_DATA_TYPE);
      if (schema_table_store_record(thd, table))
      {
        free_table_share(&share);
        if (free_sp_head)
          delete sp;
        DBUG_RETURN(1);
      }
    }

    sp_pcontext *sp_root_parsing_ctx= sp->get_root_parsing_context();

    for (uint i= 0; i < sp_root_parsing_ctx->context_var_count(); i++)
    {
      const char *tmp_buff;
      sp_variable *spvar= sp_root_parsing_ctx->find_variable(i);
      field_def= &spvar->field_def;
      switch (spvar->mode) {
      case sp_variable::MODE_IN:
        tmp_buff= "IN";
        break;
      case sp_variable::MODE_OUT:
        tmp_buff= "OUT";
        break;
      case sp_variable::MODE_INOUT:
        tmp_buff= "INOUT";
        break;
      default:
        tmp_buff= "";
        break;
      }  

      restore_record(table, s->default_values);
      table->field[IS_PARAMETERS_SPECIFIC_CATALOG]->store(STRING_WITH_LEN
                                                          ("def"), cs);
      table->field[IS_PARAMETERS_SPECIFIC_SCHEMA]->store(sp_db.ptr(),
                                                         sp_db.length(), cs);
      table->field[IS_PARAMETERS_SPECIFIC_NAME]->store(sp_name.ptr(),
                                                       sp_name.length(), cs);
      table->field[IS_PARAMETERS_ORDINAL_POSITION]->store((longlong) i + 1,
                                                          TRUE);
      table->field[IS_PARAMETERS_PARAMETER_MODE]->store(tmp_buff,
                                                        strlen(tmp_buff), cs);
      table->field[IS_PARAMETERS_PARAMETER_MODE]->set_notnull();
      table->field[IS_PARAMETERS_PARAMETER_NAME]->store(spvar->name.str,
                                                        spvar->name.length, cs);
      table->field[IS_PARAMETERS_PARAMETER_NAME]->set_notnull();
      get_field(thd->mem_root, proc_table->field[MYSQL_PROC_MYSQL_TYPE],
                &tmp_string);
      table->field[IS_PARAMETERS_ROUTINE_TYPE]->store(tmp_string.ptr(),
                                                      tmp_string.length(), cs);

      field= make_field(&share, (uchar*) 0, field_def->length,
                        (uchar*) "", 0, field_def->pack_flag,
                        field_def->sql_type, field_def->charset,
                        field_def->geom_type, Field::NONE,
                        field_def->interval, spvar->name.str);

      field->table= &tbl;
      field->gcol_info= field_def->gcol_info;
      field->stored_in_db= field_def->stored_in_db;
      tbl.in_use= thd;
      store_column_type(thd, table, field, cs, IS_PARAMETERS_DATA_TYPE);
      if (schema_table_store_record(thd, table))
      {
        free_table_share(&share);
        if (free_sp_head)
          delete sp;
        DBUG_RETURN(1);
      }
    }
    if (free_sp_head)
      delete sp;
  }
  free_table_share(&share);
  DBUG_RETURN(0);
}


bool store_schema_proc(THD *thd, TABLE *table, TABLE *proc_table,
                       const char *wild, bool full_access, const char *sp_user)
{
  MYSQL_TIME time;
  LEX *lex= thd->lex;
  CHARSET_INFO *cs= system_charset_info;
  char sp_db_buff[NAME_LEN + 1], sp_name_buff[NAME_LEN + 1],
    definer_buff[USERNAME_LENGTH + HOSTNAME_LENGTH + 2],
    returns_buff[MAX_FIELD_WIDTH];

  String sp_db(sp_db_buff, sizeof(sp_db_buff), cs);
  String sp_name(sp_name_buff, sizeof(sp_name_buff), cs);
  String definer(definer_buff, sizeof(definer_buff), cs);
  String returns(returns_buff, sizeof(returns_buff), cs);

  proc_table->field[MYSQL_PROC_FIELD_DB]->val_str(&sp_db);
  proc_table->field[MYSQL_PROC_FIELD_NAME]->val_str(&sp_name);
  proc_table->field[MYSQL_PROC_FIELD_DEFINER]->val_str(&definer);

  enum_sp_type sp_type=
    (enum_sp_type) proc_table->field[MYSQL_PROC_MYSQL_TYPE]->val_int();

  if (!full_access)
    full_access= !strcmp(sp_user, definer.c_ptr_safe());
  if (!full_access &&
      check_some_routine_access(thd, sp_db.c_ptr_safe(), sp_name.c_ptr_safe(),
                                sp_type == SP_TYPE_PROCEDURE))
    return 0;

  if ((lex->sql_command == SQLCOM_SHOW_STATUS_PROC &&
      sp_type == SP_TYPE_PROCEDURE) ||
      (lex->sql_command == SQLCOM_SHOW_STATUS_FUNC &&
      sp_type == SP_TYPE_FUNCTION) ||
      (sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) == 0)
  {
    restore_record(table, s->default_values);
    if (!wild || !wild[0] || !wild_case_compare(system_charset_info,
                                                sp_name.c_ptr_safe(), wild))
    {
      int enum_idx= (int) proc_table->field[MYSQL_PROC_FIELD_ACCESS]->val_int();
      table->field[IS_ROUTINES_ROUTINE_NAME]->store(sp_name.ptr(),
                                                    sp_name.length(), cs);

      copy_field_as_string(table->field[IS_ROUTINES_SPECIFIC_NAME],
                           proc_table->field[MYSQL_PROC_FIELD_SPECIFIC_NAME]);
      table->field[IS_ROUTINES_ROUTINE_CATALOG]->store(STRING_WITH_LEN("def"),
                                                       cs);
      table->field[IS_ROUTINES_ROUTINE_SCHEMA]->store(sp_db.ptr(), sp_db.length(), cs);
      copy_field_as_string(table->field[IS_ROUTINES_ROUTINE_TYPE],
                           proc_table->field[MYSQL_PROC_MYSQL_TYPE]);

      if (sp_type == SP_TYPE_FUNCTION)
      {
        sp_head *sp;
        bool free_sp_head;
        proc_table->field[MYSQL_PROC_FIELD_RETURNS]->val_str(&returns);
        sp= sp_load_for_information_schema(thd, proc_table, &sp_db, &sp_name,
                                           (sql_mode_t) proc_table->
                                           field[MYSQL_PROC_FIELD_SQL_MODE]->
                                           val_int(),
                                           SP_TYPE_FUNCTION,
                                           returns.c_ptr_safe(),
                                           "", &free_sp_head);

        if (sp)
        {
          char path[FN_REFLEN];
          TABLE_SHARE share;
          TABLE tbl;
          Field *field;
          Create_field *field_def= &sp->m_return_field_def;

          (void) build_table_filename(path, sizeof(path), "", "", "", 0);
          init_tmp_table_share(thd, &share, "", 0, "", path);
          field= make_field(&share, (uchar*) 0, field_def->length,
                            (uchar*) "", 0, field_def->pack_flag,
                            field_def->sql_type, field_def->charset,
                            field_def->geom_type, Field::NONE,
                            field_def->interval, "");

          field->table= &tbl;
          field->gcol_info= field_def->gcol_info;
          field->stored_in_db= field_def->stored_in_db;
          tbl.in_use= thd;
          store_column_type(thd, table, field, cs, IS_ROUTINES_DATA_TYPE);
          free_table_share(&share);
          if (free_sp_head)
            delete sp;
        }
      }

      if (full_access)
      {
        copy_field_as_string(table->field[IS_ROUTINES_ROUTINE_DEFINITION],
                             proc_table->field[MYSQL_PROC_FIELD_BODY_UTF8]);
        table->field[IS_ROUTINES_ROUTINE_DEFINITION]->set_notnull();
      }
      table->field[IS_ROUTINES_ROUTINE_BODY]->store(STRING_WITH_LEN("SQL"), cs);
      table->field[IS_ROUTINES_PARAMETER_STYLE]->store(STRING_WITH_LEN("SQL"),
                                                       cs);
      copy_field_as_string(table->field[IS_ROUTINES_IS_DETERMINISTIC],
                           proc_table->field[MYSQL_PROC_FIELD_DETERMINISTIC]);
      table->field[IS_ROUTINES_SQL_DATA_ACCESS]->
                   store(sp_data_access_name[enum_idx].str, 
                         sp_data_access_name[enum_idx].length , cs);
      copy_field_as_string(table->field[IS_ROUTINES_SECURITY_TYPE],
                           proc_table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]);

      memset(&time, 0, sizeof(time));
      proc_table->field[MYSQL_PROC_FIELD_CREATED]->get_time(&time);
      table->field[IS_ROUTINES_CREATED]->store_time(&time);
      memset(&time, 0, sizeof(time));
      proc_table->field[MYSQL_PROC_FIELD_MODIFIED]->get_time(&time);
      table->field[IS_ROUTINES_LAST_ALTERED]->store_time(&time);
      copy_field_as_string(table->field[IS_ROUTINES_SQL_MODE],
                           proc_table->field[MYSQL_PROC_FIELD_SQL_MODE]);
      copy_field_as_string(table->field[IS_ROUTINES_ROUTINE_COMMENT],
                           proc_table->field[MYSQL_PROC_FIELD_COMMENT]);

      table->field[IS_ROUTINES_DEFINER]->store(definer.ptr(),
                                               definer.length(), cs);
      copy_field_as_string(table->field[IS_ROUTINES_CHARACTER_SET_CLIENT],
                           proc_table->
                           field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT]);
      copy_field_as_string(table->field[IS_ROUTINES_COLLATION_CONNECTION],
                           proc_table->
                           field[MYSQL_PROC_FIELD_COLLATION_CONNECTION]);
      copy_field_as_string(table->field[IS_ROUTINES_DATABASE_COLLATION],
			   proc_table->field[MYSQL_PROC_FIELD_DB_COLLATION]);

      return schema_table_store_record(thd, table);
    }
  }
  return 0;
}


int fill_schema_proc(THD *thd, TABLE_LIST *tables, Item *cond)
{
  TABLE *proc_table;
  TABLE_LIST proc_tables;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  int error, res= 0;
  TABLE *table= tables->table;
  bool full_access;
  char definer[USER_HOST_BUFF_SIZE];
  Open_tables_backup open_tables_state_backup;
  enum enum_schema_tables schema_table_idx=
    get_schema_table_idx(tables->schema_table);
  sql_mode_t old_sql_mode= thd->variables.sql_mode;

  DBUG_ENTER("fill_schema_proc");

  strxmov(definer, thd->security_context()->priv_user().str, "@",
          thd->security_context()->priv_host().str, NullS);
  /* We use this TABLE_LIST instance only for checking of privileges. */
  proc_tables.db= (char*) "mysql";
  proc_tables.db_length= 5;
  proc_tables.table_name= proc_tables.alias= (char*) "proc";
  proc_tables.table_name_length= 4;
  proc_tables.lock_type= TL_READ;
  full_access= !check_table_access(thd, SELECT_ACL, &proc_tables, FALSE,
                                   1, TRUE);
  if (!(proc_table= open_proc_table_for_read(thd, &open_tables_state_backup)))
  {
    DBUG_RETURN(1);
  }

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  if ((error= proc_table->file->ha_index_init(0, 1)))
  {
    proc_table->file->print_error(error, MYF(0));
    res= 1;
    goto err;
  }
  if ((error= proc_table->file->ha_index_first(proc_table->record[0])))
  {
    res= (error == HA_ERR_END_OF_FILE) ? 0 : 1;
    if (res)
      proc_table->file->print_error(error, MYF(0));
    goto err;
  }

  if (schema_table_idx == SCH_PROCEDURES ?
      store_schema_proc(thd, table, proc_table, wild, full_access, definer) :
      store_schema_params(thd, table, proc_table, wild, full_access, definer))
  {
    res= 1;
    goto err;
  }
  while (!proc_table->file->ha_index_next(proc_table->record[0]))
  {
    if (schema_table_idx == SCH_PROCEDURES ?
        store_schema_proc(thd, table, proc_table, wild, full_access, definer): 
        store_schema_params(thd, table, proc_table, wild, full_access, definer))
    {
      res= 1;
      goto err;
    }
  }

err:
  if (proc_table->file->inited)
    (void) proc_table->file->ha_index_end();
  thd->variables.sql_mode= old_sql_mode;
  close_nontrans_system_tables(thd, &open_tables_state_backup);
  DBUG_RETURN(res);
}


static int get_schema_stat_record(THD *thd, TABLE_LIST *tables,
				  TABLE *table, bool res,
				  LEX_STRING *db_name,
				  LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_stat_record");
  if (res)
  {
    if (thd->lex->sql_command != SQLCOM_SHOW_KEYS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.STATISTICS
        rather than in SHOW KEYS
      */
      if (thd->is_error())
        push_warning(thd, Sql_condition::SL_WARNING,
                     thd->get_stmt_da()->mysql_errno(),
                     thd->get_stmt_da()->message_text());
      thd->clear_error();
      res= 0;
    }
    DBUG_RETURN(res);
  }
  else if (!tables->is_view())
  {
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->s->key_info;
    if (show_table->file)
      show_table->file->info(HA_STATUS_VARIABLE |
                             HA_STATUS_NO_LOCK |
                             HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->s->keys ; i++,key_info++)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      const char *str;
      for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
      {
        restore_record(table, s->default_values);
        table->field[0]->store(STRING_WITH_LEN("def"), cs);
        table->field[1]->store(db_name->str, db_name->length, cs);
        table->field[2]->store(table_name->str, table_name->length, cs);
        table->field[3]->store((longlong) ((key_info->flags &
                                            HA_NOSAME) ? 0 : 1), TRUE);
        table->field[4]->store(db_name->str, db_name->length, cs);
        table->field[5]->store(key_info->name, strlen(key_info->name), cs);
        table->field[6]->store((longlong) (j+1), TRUE);
        str=(key_part->field ? key_part->field->field_name :
             "?unknown field?");
        table->field[7]->store(str, strlen(str), cs);
        if (show_table->file)
        {
          if (show_table->file->index_flags(i, j, 0) & HA_READ_ORDER)
          {
            table->field[8]->store(((key_part->key_part_flag &
                                     HA_REVERSE_SORT) ?
                                    "D" : "A"), 1, cs);
            table->field[8]->set_notnull();
          }
          KEY *key=show_table->key_info+i;
          if (key->has_records_per_key(j))
          {
            double records= (show_table->file->stats.records /
                             key->records_per_key(j));
            table->field[9]->store(static_cast<longlong>(round(records)), TRUE);
            table->field[9]->set_notnull();
          }
          str= show_table->file->index_type(i);
          table->field[13]->store(str, strlen(str), cs);
        }
        if (!(key_info->flags & HA_FULLTEXT) &&
            (key_part->field &&
             key_part->length !=
             show_table->s->field[key_part->fieldnr-1]->key_length()))
        {
          table->field[10]->store((longlong) key_part->length /
                                  key_part->field->charset()->mbmaxlen, TRUE);
          table->field[10]->set_notnull();
        }
        uint flags= key_part->field ? key_part->field->flags : 0;
        const char *pos=(char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
        table->field[12]->store(pos, strlen(pos), cs);
        if (!show_table->s->keys_in_use.is_set(i))
          table->field[14]->store(STRING_WITH_LEN("disabled"), cs);
        else
          table->field[14]->store("", 0, cs);
        table->field[14]->set_notnull();
        DBUG_ASSERT(MY_TEST(key_info->flags & HA_USES_COMMENT) ==
                   (key_info->comment.length > 0));
        if (key_info->flags & HA_USES_COMMENT)
          table->field[15]->store(key_info->comment.str, 
                                  key_info->comment.length, cs);
        if (schema_table_store_record(thd, table))
          DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(res);
}


static int get_schema_views_record(THD *thd, TABLE_LIST *tables,
				   TABLE *table, bool res,
				   LEX_STRING *db_name,
				   LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;
  char definer[USER_HOST_BUFF_SIZE];
  size_t definer_len;
  bool updatable_view;
  DBUG_ENTER("get_schema_views_record");

  if (tables->is_view())
  {
    Security_context *sctx= thd->security_context();
    if (!tables->allowed_show)
    {
      if (!my_strcasecmp(system_charset_info, tables->definer.user.str,
                         sctx->priv_user().str) &&
          !my_strcasecmp(system_charset_info, tables->definer.host.str,
                         sctx->priv_host().str))
        tables->allowed_show= TRUE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      else
      {
        if ((thd->col_access & (SHOW_VIEW_ACL|SELECT_ACL)) ==
            (SHOW_VIEW_ACL|SELECT_ACL))
          tables->allowed_show= TRUE;
        else
        {
          TABLE_LIST table_list;
          uint view_access;
          table_list.db= tables->db;
          table_list.table_name= tables->table_name;
          table_list.grant.privilege= thd->col_access;
          view_access= get_table_grant(thd, &table_list);
	  if ((view_access & (SHOW_VIEW_ACL|SELECT_ACL)) ==
	      (SHOW_VIEW_ACL|SELECT_ACL))
	    tables->allowed_show= TRUE;
        }
      }
#endif
    }
    restore_record(table, s->default_values);
    table->field[0]->store(STRING_WITH_LEN("def"), cs);
    table->field[1]->store(db_name->str, db_name->length, cs);
    table->field[2]->store(table_name->str, table_name->length, cs);

    if (tables->allowed_show)
    {
      table->field[3]->store(tables->view_body_utf8.str,
                             tables->view_body_utf8.length,
                             cs);
    }

    if (tables->with_check != VIEW_CHECK_NONE)
    {
      if (tables->with_check == VIEW_CHECK_LOCAL)
        table->field[4]->store(STRING_WITH_LEN("LOCAL"), cs);
      else
        table->field[4]->store(STRING_WITH_LEN("CASCADED"), cs);
    }
    else
      table->field[4]->store(STRING_WITH_LEN("NONE"), cs);

    /*
      Only try to fill in the information about view updatability
      if it is requested as part of the top-level query (i.e.
      it's select * from i_s.views, as opposed to, say, select
      security_type from i_s.views).  Do not try to access the
      underlying tables if there was an error when opening the
      view: all underlying tables are released back to the table
      definition cache on error inside open_tables_for_query().
      If a field is not assigned explicitly, it defaults to NULL.
    */
    if (res == FALSE &&
        table->pos_in_table_list->table_open_method & OPEN_FULL_TABLE)
    {
      updatable_view= 0;
      if (tables->algorithm != VIEW_ALGORITHM_TEMPTABLE)
      {
        /*
          We should use tables->view_query()->select_lex->item_list here
          and can not use Field_iterator_view because the view
          always uses temporary algorithm during opening for I_S
          and TABLE_LIST fields 'field_translation'
          & 'field_translation_end' are uninitialized is this
          case.
        */
        List<Item> *fields= &tables->view_query()->select_lex->item_list;
        List_iterator<Item> it(*fields);
        Item *item;
        /*
          check that at least one column in view is updatable
        */
        while ((item= it++))
        {
          Item_field *item_field= item->field_for_view_update();
          if (item_field && !item_field->table_ref->schema_table)
          {
            updatable_view= 1;
            break;
          }
        }
        if (updatable_view && !tables->view_query()->unit->is_mergeable())
          updatable_view= 0;
      }
      if (updatable_view)
        table->field[5]->store(STRING_WITH_LEN("YES"), cs);
      else
        table->field[5]->store(STRING_WITH_LEN("NO"), cs);
    }

    definer_len= (strxmov(definer, tables->definer.user.str, "@",
                          tables->definer.host.str, NullS) - definer);
    table->field[6]->store(definer, definer_len, cs);
    if (tables->view_suid)
      table->field[7]->store(STRING_WITH_LEN("DEFINER"), cs);
    else
      table->field[7]->store(STRING_WITH_LEN("INVOKER"), cs);

    table->field[8]->store(tables->view_creation_ctx->get_client_cs()->csname,
                           strlen(tables->view_creation_ctx->
                                  get_client_cs()->csname), cs);

    table->field[9]->store(tables->view_creation_ctx->
                           get_connection_cl()->name,
                           strlen(tables->view_creation_ctx->
                                  get_connection_cl()->name), cs);


    if (schema_table_store_record(thd, table))
      DBUG_RETURN(1);
    if (res && thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
  }
  if (res)
    thd->clear_error();
  DBUG_RETURN(0);
}


bool store_constraints(THD *thd, TABLE *table, LEX_STRING *db_name,
                       LEX_STRING *table_name, const char *key_name,
                       size_t key_len, const char *con_type, size_t con_len)
{
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, s->default_values);
  table->field[0]->store(STRING_WITH_LEN("def"), cs);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[3]->store(db_name->str, db_name->length, cs);
  table->field[4]->store(table_name->str, table_name->length, cs);
  table->field[5]->store(con_type, con_len, cs);
  return schema_table_store_record(thd, table);
}


static int get_schema_constraints_record(THD *thd, TABLE_LIST *tables,
					 TABLE *table, bool res,
					 LEX_STRING *db_name,
					 LEX_STRING *table_name)
{
  DBUG_ENTER("get_schema_constraints_record");
  if (res)
  {
    if (thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
    thd->clear_error();
    DBUG_RETURN(0);
  }
  else if (!tables->is_view())
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->s->primary_key;
    for (uint i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;

      if (i == primary_key && !strcmp(key_info->name, primary_key_name))
      {
        if (store_constraints(thd, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("PRIMARY KEY")))
          DBUG_RETURN(1);
      }
      else if (key_info->flags & HA_NOSAME)
      {
        if (store_constraints(thd, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("UNIQUE")))
          DBUG_RETURN(1);
      }
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info=it++))
    {
      if (store_constraints(thd, table, db_name, table_name, 
                            f_key_info->foreign_id->str,
                            strlen(f_key_info->foreign_id->str),
                            "FOREIGN KEY", 11))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(res);
}


static bool store_trigger(THD *thd, TABLE *table, Trigger *trigger)
{
  CHARSET_INFO *cs= system_charset_info;

  restore_record(table, s->default_values);
  table->field[0]->store(STRING_WITH_LEN("def"), cs);
  table->field[1]->store(trigger->get_db_name().str,
                         trigger->get_db_name().length, cs);
  table->field[2]->store(trigger->get_trigger_name().str,
                         trigger->get_trigger_name().length, cs);

  {
    const LEX_STRING &s= trg_event_type_names[trigger->get_event()];
    table->field[3]->store(s.str, s.length, cs);
  }

  table->field[4]->store(STRING_WITH_LEN("def"), cs);
  table->field[5]->store(trigger->get_db_name().str,
                         trigger->get_db_name().length, cs);
  table->field[6]->store(trigger->get_subject_table_name().str,
                         trigger->get_subject_table_name().length, cs);

  table->field[7]->set_notnull();
  table->field[7]->store(trigger->get_action_order(), true);

  {
    const LEX_STRING &s= trigger->get_sp()->m_body_utf8;
    table->field[9]->store(s.str, s.length, cs);
  }

  table->field[10]->store(STRING_WITH_LEN("ROW"), cs);

  {
    const LEX_STRING &s= trg_action_time_type_names[trigger->get_action_time()];
    table->field[11]->store(s.str, s.length, cs);
  }

  table->field[14]->store(STRING_WITH_LEN("OLD"), cs);
  table->field[15]->store(STRING_WITH_LEN("NEW"), cs);

  if (!trigger->is_created_timestamp_null())
  {
    timeval epoche_timestamp= trigger->get_created_timestamp();

    table->field[16]->set_notnull();
    table->field[16]->store_timestamp(&epoche_timestamp);
  }

  {
    LEX_STRING s;
    sql_mode_string_representation(thd, trigger->get_sql_mode(), &s);
    table->field[17]->store(s.str, s.length, cs);
  }

  table->field[18]->store(trigger->get_definer().str,
                          trigger->get_definer().length, cs);
  table->field[19]->store(trigger->get_client_cs_name().str,
                          trigger->get_client_cs_name().length, cs);
  table->field[20]->store(trigger->get_connection_cl_name().str,
                          trigger->get_connection_cl_name().length, cs);
  table->field[21]->store(trigger->get_db_cl_name().str,
                          trigger->get_db_cl_name().length, cs);

  return schema_table_store_record(thd, table);
}


static int get_schema_triggers_record(THD *thd, TABLE_LIST *tables,
				      TABLE *table, bool res,
				      LEX_STRING *db_name,
				      LEX_STRING *table_name)
{
  DBUG_ENTER("get_schema_triggers_record");
  /*
    res can be non zero value when processed table is a view or
    error happened during opening of processed table.
  */
  if (res)
  {
    if (thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
    thd->clear_error();
    DBUG_RETURN(0);
  }

  if (tables->is_view() || !tables->table->triggers)
    DBUG_RETURN(0);

  if (check_table_access(thd, TRIGGER_ACL, tables, false, 1, true))
    DBUG_RETURN(0);

  Table_trigger_dispatcher *triggers= tables->table->triggers;

  for (int event= 0; event < (int) TRG_EVENT_MAX; ++event)
  {
    for (int timing= 0; timing < (int) TRG_ACTION_MAX; ++timing)
    {
      Trigger_chain *trigger_chain= triggers->get_triggers(event, timing);

      if (!trigger_chain)
        continue;

      List_iterator<Trigger> it(trigger_chain->get_trigger_list());
      Trigger *trigger;

      while ((trigger= it++))
      {
        if (trigger->has_parse_error())
          continue;

        if (store_trigger(thd, table, trigger))
          DBUG_RETURN(1);
      }
    }
  }

  DBUG_RETURN(0);
}


void store_key_column_usage(TABLE *table, LEX_STRING *db_name,
                            LEX_STRING *table_name, const char *key_name,
                            size_t key_len, const char *con_type, size_t con_len,
                            longlong idx)
{
  CHARSET_INFO *cs= system_charset_info;
  table->field[0]->store(STRING_WITH_LEN("def"), cs);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[3]->store(STRING_WITH_LEN("def"), cs);
  table->field[4]->store(db_name->str, db_name->length, cs);
  table->field[5]->store(table_name->str, table_name->length, cs);
  table->field[6]->store(con_type, con_len, cs);
  table->field[7]->store(idx, TRUE);
}


static int get_schema_key_column_usage_record(THD *thd,
					      TABLE_LIST *tables,
					      TABLE *table, bool res,
					      LEX_STRING *db_name,
					      LEX_STRING *table_name)
{
  DBUG_ENTER("get_schema_key_column_usage_record");
  if (res)
  {
    if (thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
    thd->clear_error();
    DBUG_RETURN(0);
  }
  else if (!tables->is_view())
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->s->primary_key;
    for (uint i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;
      uint f_idx= 0;
      KEY_PART_INFO *key_part= key_info->key_part;
      for (uint j=0 ; j < key_info->user_defined_key_parts ; j++,key_part++)
      {
        if (key_part->field)
        {
          f_idx++;
          restore_record(table, s->default_values);
          store_key_column_usage(table, db_name, table_name,
                                 key_info->name,
                                 strlen(key_info->name), 
                                 key_part->field->field_name, 
                                 strlen(key_part->field->field_name),
                                 (longlong) f_idx);
          if (schema_table_store_record(thd, table))
            DBUG_RETURN(1);
        }
      }
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> fkey_it(f_key_list);
    while ((f_key_info= fkey_it++))
    {
      LEX_STRING *f_info;
      LEX_STRING *r_info;
      List_iterator_fast<LEX_STRING> it(f_key_info->foreign_fields),
        it1(f_key_info->referenced_fields);
      uint f_idx= 0;
      while ((f_info= it++))
      {
        r_info= it1++;
        f_idx++;
        restore_record(table, s->default_values);
        store_key_column_usage(table, db_name, table_name,
                               f_key_info->foreign_id->str,
                               f_key_info->foreign_id->length,
                               f_info->str, f_info->length,
                               (longlong) f_idx);
        table->field[8]->store((longlong) f_idx, TRUE);
        table->field[8]->set_notnull();
        table->field[9]->store(f_key_info->referenced_db->str,
                               f_key_info->referenced_db->length,
                               system_charset_info);
        table->field[9]->set_notnull();
        table->field[10]->store(f_key_info->referenced_table->str,
                                f_key_info->referenced_table->length, 
                                system_charset_info);
        table->field[10]->set_notnull();
        table->field[11]->store(r_info->str, r_info->length,
                                system_charset_info);
        table->field[11]->set_notnull();
        if (schema_table_store_record(thd, table))
          DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(res);
}


static void collect_partition_expr(THD *thd, List<char> &field_list,
                                   String *str)
{
  List_iterator<char> part_it(field_list);
  ulong no_fields= field_list.elements;
  const char *field_str;
  str->length(0);
  while ((field_str= part_it++))
  {
    append_identifier(thd, str, field_str, strlen(field_str));
    if (--no_fields != 0)
      str->append(",");
  }
  return;
}


/*
  Convert a string in a given character set to a string which can be
  used for FRM file storage in which case use_hex is TRUE and we store
  the character constants as hex strings in the character set encoding
  their field have. In the case of SHOW CREATE TABLE and the
  PARTITIONS information schema table we instead provide utf8 strings
  to the user and convert to the utf8 character set.

  SYNOPSIS
    get_cs_converted_part_value_from_string()
    item                           Item from which constant comes
    input_str                      String as provided by val_str after
                                   conversion to character set
    output_str                     Out value: The string created
    cs                             Character set string is encoded in
                                   NULL for INT_RESULT's here
    use_hex                        TRUE => hex string created
                                   FALSE => utf8 constant string created

  RETURN VALUES
    TRUE                           Error
    FALSE                          Ok
*/

int get_cs_converted_part_value_from_string(THD *thd,
                                            Item *item,
                                            String *input_str,
                                            String *output_str,
                                            const CHARSET_INFO *cs,
                                            bool use_hex)
{
  if (item->result_type() == INT_RESULT)
  {
    longlong value= item->val_int();
    output_str->set(value, system_charset_info);
    return FALSE;
  }
  if (!input_str)
  {
    my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
    return TRUE;
  }
  get_cs_converted_string_value(thd,
                                input_str,
                                output_str,
                                cs,
                                use_hex);
  return FALSE;
}


static void store_schema_partitions_record(THD *thd, TABLE *schema_table,
                                           TABLE *showing_table,
                                           partition_element *part_elem,
                                           handler *file, uint part_id)
{
  TABLE* table= schema_table;
  CHARSET_INFO *cs= system_charset_info;
  ha_statistics stat_info;
  ha_checksum check_sum = 0;
  MYSQL_TIME time;
  Partition_handler *part_handler= file->get_partition_handler();


  if (!part_handler)
  {
    /* Not a partitioned table, get the stats from the full table! */
    file->info(HA_STATUS_CONST | HA_STATUS_TIME | HA_STATUS_VARIABLE |
               HA_STATUS_NO_LOCK);
    stat_info.records=              file->stats.records;
    stat_info.mean_rec_length=      file->stats.mean_rec_length;
    stat_info.data_file_length=     file->stats.data_file_length;
    stat_info.max_data_file_length= file->stats.max_data_file_length;
    stat_info.index_file_length=    file->stats.index_file_length;
    stat_info.delete_length=        file->stats.delete_length;
    stat_info.create_time=          file->stats.create_time;
    stat_info.update_time=          file->stats.update_time;
    stat_info.check_time=           file->stats.check_time;
    if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
      check_sum= file->checksum();
  }
  else
    part_handler->get_dynamic_partition_info(&stat_info, &check_sum, part_id);

  table->field[0]->store(STRING_WITH_LEN("def"), cs);
  table->field[12]->store((longlong) stat_info.records, TRUE);
  table->field[13]->store((longlong) stat_info.mean_rec_length, TRUE);
  table->field[14]->store((longlong) stat_info.data_file_length, TRUE);
  if (stat_info.max_data_file_length)
  {
    table->field[15]->store((longlong) stat_info.max_data_file_length, TRUE);
    table->field[15]->set_notnull();
  }
  table->field[16]->store((longlong) stat_info.index_file_length, TRUE);
  table->field[17]->store((longlong) stat_info.delete_length, TRUE);
  if (stat_info.create_time)
  {
    thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                              (my_time_t)stat_info.create_time);
    table->field[18]->store_time(&time);
    table->field[18]->set_notnull();
  }
  else
  {
    table->field[18]->set_null();
  }
  if (stat_info.update_time)
  {
    thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                              (my_time_t)stat_info.update_time);
    table->field[19]->store_time(&time);
    table->field[19]->set_notnull();
  }
  else
  {
    table->field[19]->set_null();
  }
  if (stat_info.check_time)
  {
    thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                              (my_time_t)stat_info.check_time);
    table->field[20]->store_time(&time);
    table->field[20]->set_notnull();
  }
  else
  {
    table->field[20]->set_null();
  }
  if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
  {
    table->field[21]->store((longlong) check_sum, TRUE);
    table->field[21]->set_notnull();
  }
  if (part_elem)
  {
    if (part_elem->part_comment)
      table->field[22]->store(part_elem->part_comment,
                              strlen(part_elem->part_comment), cs);
    else
      table->field[22]->store(STRING_WITH_LEN(""), cs);
    if (part_elem->nodegroup_id != UNDEF_NODEGROUP)
      table->field[23]->store((longlong) part_elem->nodegroup_id, TRUE);
    else
      table->field[23]->store(STRING_WITH_LEN("default"), cs);

    table->field[24]->set_notnull();
    if (part_elem->tablespace_name)
      table->field[24]->store(part_elem->tablespace_name,
                              strlen(part_elem->tablespace_name), cs);
    else
    {
      char *ts= showing_table->s->tablespace;
      if(ts)
        table->field[24]->store(ts, strlen(ts), cs);
      else
        table->field[24]->set_null();
    }
  }
  return;
}

static int
get_partition_column_description(THD *thd,
                                 partition_info *part_info,
                                 part_elem_value *list_value,
                                 String &tmp_str)
{
  uint num_elements= part_info->part_field_list.elements;
  uint i;
  DBUG_ENTER("get_partition_column_description");

  for (i= 0; i < num_elements; i++)
  {
    part_column_list_val *col_val= &list_value->col_val_array[i];
    if (col_val->max_value)
      tmp_str.append(partition_keywords[PKW_MAXVALUE].str);
    else if (col_val->null_value)
      tmp_str.append("NULL");
    else
    {
      char buffer[MAX_KEY_LENGTH];
      String str(buffer, sizeof(buffer), &my_charset_bin);
      String val_conv;
      Item *item= col_val->item_expression;

      if (!(item= part_info->get_column_item(item,
                              part_info->part_field_array[i])))
      {
        DBUG_RETURN(1);
      }
      String *res= item->val_str(&str);
      if (get_cs_converted_part_value_from_string(thd, item, res, &val_conv,
                              part_info->part_field_array[i]->charset(),
                              FALSE))
      {
        DBUG_RETURN(1);
      }
      tmp_str.append(val_conv);
    }
    if (i != num_elements - 1)
      tmp_str.append(",");
  }
  DBUG_RETURN(0);
}

static int get_schema_partitions_record(THD *thd, TABLE_LIST *tables,
                                        TABLE *table, bool res,
                                        LEX_STRING *db_name,
                                        LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;
  char buff[61];
  String tmp_res(buff, sizeof(buff), cs);
  String tmp_str;
  TABLE *show_table= tables->table;
  handler *file;
  partition_info *part_info;
  DBUG_ENTER("get_schema_partitions_record");

  if (res)
  {
    if (thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
    thd->clear_error();
    DBUG_RETURN(0);
  }
  file= show_table->file;
  part_info= show_table->part_info;
  if (part_info)
  {
    partition_element *part_elem;
    List_iterator<partition_element> part_it(part_info->partitions);
    uint part_pos= 0, part_id= 0;

    restore_record(table, s->default_values);
    table->field[0]->store(STRING_WITH_LEN("def"), cs);
    table->field[1]->store(db_name->str, db_name->length, cs);
    table->field[2]->store(table_name->str, table_name->length, cs);


    /* Partition method*/
    switch (part_info->part_type) {
    case RANGE_PARTITION:
    case LIST_PARTITION:
      tmp_res.length(0);
      if (part_info->part_type == RANGE_PARTITION)
        tmp_res.append(partition_keywords[PKW_RANGE].str,
                       partition_keywords[PKW_RANGE].length);
      else
        tmp_res.append(partition_keywords[PKW_LIST].str,
                       partition_keywords[PKW_LIST].length);
      if (part_info->column_list)
        tmp_res.append(partition_keywords[PKW_COLUMNS].str,
                       partition_keywords[PKW_COLUMNS].length);
      table->field[7]->store(tmp_res.ptr(), tmp_res.length(), cs);
      break;
    case HASH_PARTITION:
      tmp_res.length(0);
      if (part_info->linear_hash_ind)
        tmp_res.append(partition_keywords[PKW_LINEAR].str,
                       partition_keywords[PKW_LINEAR].length);
      if (part_info->list_of_part_fields)
        tmp_res.append(partition_keywords[PKW_KEY].str,
                       partition_keywords[PKW_KEY].length);
      else
        tmp_res.append(partition_keywords[PKW_HASH].str, 
                       partition_keywords[PKW_HASH].length);
      table->field[7]->store(tmp_res.ptr(), tmp_res.length(), cs);
      break;
    default:
      DBUG_ASSERT(0);
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      DBUG_RETURN(1);
    }
    table->field[7]->set_notnull();

    /* Partition expression */
    if (part_info->part_expr)
    {
      table->field[9]->store(part_info->part_func_string,
                             part_info->part_func_len, cs);
    }
    else if (part_info->list_of_part_fields)
    {
      collect_partition_expr(thd, part_info->part_field_list, &tmp_str);
      table->field[9]->store(tmp_str.ptr(), tmp_str.length(), cs);
    }
    table->field[9]->set_notnull();

    if (part_info->is_sub_partitioned())
    {
      /* Subpartition method */
      tmp_res.length(0);
      if (part_info->linear_hash_ind)
        tmp_res.append(partition_keywords[PKW_LINEAR].str,
                       partition_keywords[PKW_LINEAR].length);
      if (part_info->list_of_subpart_fields)
        tmp_res.append(partition_keywords[PKW_KEY].str,
                       partition_keywords[PKW_KEY].length);
      else
        tmp_res.append(partition_keywords[PKW_HASH].str, 
                       partition_keywords[PKW_HASH].length);
      table->field[8]->store(tmp_res.ptr(), tmp_res.length(), cs);
      table->field[8]->set_notnull();

      /* Subpartition expression */
      if (part_info->subpart_expr)
      {
        table->field[10]->store(part_info->subpart_func_string,
                                part_info->subpart_func_len, cs);
      }
      else if (part_info->list_of_subpart_fields)
      {
        collect_partition_expr(thd, part_info->subpart_field_list, &tmp_str);
        table->field[10]->store(tmp_str.ptr(), tmp_str.length(), cs);
      }
      table->field[10]->set_notnull();
    }

    while ((part_elem= part_it++))
    {
      table->field[3]->store(part_elem->partition_name,
                             strlen(part_elem->partition_name), cs);
      table->field[3]->set_notnull();
      /* PARTITION_ORDINAL_POSITION */
      table->field[5]->store((longlong) ++part_pos, TRUE);
      table->field[5]->set_notnull();

      /* Partition description */
      if (part_info->part_type == RANGE_PARTITION)
      {
        if (part_info->column_list)
        {
          List_iterator<part_elem_value> list_val_it(part_elem->list_val_list);
          part_elem_value *list_value= list_val_it++;
          tmp_str.length(0);
          if (get_partition_column_description(thd,
                                               part_info,
                                               list_value,
                                               tmp_str))
          {
            DBUG_RETURN(1);
          }
          table->field[11]->store(tmp_str.ptr(), tmp_str.length(), cs);
        }
        else
        {
          if (part_elem->range_value != LLONG_MAX)
            table->field[11]->store(part_elem->range_value, FALSE);
          else
            table->field[11]->store(partition_keywords[PKW_MAXVALUE].str,
                                 partition_keywords[PKW_MAXVALUE].length, cs);
        }
        table->field[11]->set_notnull();
      }
      else if (part_info->part_type == LIST_PARTITION)
      {
        List_iterator<part_elem_value> list_val_it(part_elem->list_val_list);
        part_elem_value *list_value;
        uint num_items= part_elem->list_val_list.elements;
        tmp_str.length(0);
        tmp_res.length(0);
        if (part_elem->has_null_value)
        {
          tmp_str.append("NULL");
          if (num_items > 0)
            tmp_str.append(",");
        }
        while ((list_value= list_val_it++))
        {
          if (part_info->column_list)
          {
            if (part_info->part_field_list.elements > 1U)
              tmp_str.append("(");
            if (get_partition_column_description(thd,
                                                 part_info,
                                                 list_value,
                                                 tmp_str))
            {
              DBUG_RETURN(1);
            }
            if (part_info->part_field_list.elements > 1U)
              tmp_str.append(")");
          }
          else
          {
            if (!list_value->unsigned_flag)
              tmp_res.set(list_value->value, cs);
            else
              tmp_res.set((ulonglong)list_value->value, cs);
            tmp_str.append(tmp_res);
          }
          if (--num_items != 0)
            tmp_str.append(",");
        }
        table->field[11]->store(tmp_str.ptr(), tmp_str.length(), cs);
        table->field[11]->set_notnull();
      }

      if (part_elem->subpartitions.elements)
      {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        partition_element *subpart_elem;
        uint subpart_pos= 0;

        while ((subpart_elem= sub_it++))
        {
          table->field[4]->store(subpart_elem->partition_name,
                                 strlen(subpart_elem->partition_name), cs);
          table->field[4]->set_notnull();
          /* SUBPARTITION_ORDINAL_POSITION */
          table->field[6]->store((longlong) ++subpart_pos, TRUE);
          table->field[6]->set_notnull();
          
          store_schema_partitions_record(thd, table, show_table, subpart_elem,
                                         file, part_id);
          part_id++;
          if(schema_table_store_record(thd, table))
            DBUG_RETURN(1);
        }
      }
      else
      {
        store_schema_partitions_record(thd, table, show_table, part_elem,
                                       file, part_id);
        part_id++;
        if(schema_table_store_record(thd, table))
          DBUG_RETURN(1);
      }
    }
    DBUG_RETURN(0);
  }
  else
  {
    store_schema_partitions_record(thd, table, show_table, 0, file, 0);
    if(schema_table_store_record(thd, table))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


#ifndef EMBEDDED_LIBRARY
/*
  Loads an event from mysql.event and copies it's data to a row of
  I_S.EVENTS

  Synopsis
    copy_event_to_schema_table()
      thd         Thread
      sch_table   The schema table (information_schema.event)
      event_table The event table to use for loading (mysql.event).

  Returns
    0  OK
    1  Error
*/

int
copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table)
{
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  CHARSET_INFO *scs= system_charset_info;
  MYSQL_TIME time;
  Event_timed et;
  DBUG_ENTER("copy_event_to_schema_table");

  restore_record(sch_table, s->default_values);

  if (et.load_from_row(thd, event_table))
  {
    my_error(ER_CANNOT_LOAD_FROM_TABLE_V2, MYF(0), "mysql", "event");
    DBUG_RETURN(1);
  }

  if (!(!wild || !wild[0] || !wild_case_compare(scs, et.name.str, wild)))
    DBUG_RETURN(0);

  /*
    Skip events in schemas one does not have access to. The check is
    optimized. It's guaranteed in case of SHOW EVENTS that the user
    has access.
  */
  if (thd->lex->sql_command != SQLCOM_SHOW_EVENTS &&
      check_access(thd, EVENT_ACL, et.dbname.str, NULL, NULL, 0, 1))
    DBUG_RETURN(0);

  sch_table->field[ISE_EVENT_CATALOG]->store(STRING_WITH_LEN("def"), scs);
  sch_table->field[ISE_EVENT_SCHEMA]->
                                store(et.dbname.str, et.dbname.length,scs);
  sch_table->field[ISE_EVENT_NAME]->
                                store(et.name.str, et.name.length, scs);
  sch_table->field[ISE_DEFINER]->
                                store(et.definer.str, et.definer.length, scs);
  const String *tz_name= et.time_zone->get_name();
  sch_table->field[ISE_TIME_ZONE]->
                                store(tz_name->ptr(), tz_name->length(), scs);
  sch_table->field[ISE_EVENT_BODY]->
                                store(STRING_WITH_LEN("SQL"), scs);
  sch_table->field[ISE_EVENT_DEFINITION]->store(
    et.body_utf8.str, et.body_utf8.length, scs);

  /* SQL_MODE */
  {
    LEX_STRING sql_mode;
    sql_mode_string_representation(thd, et.sql_mode, &sql_mode);
    sch_table->field[ISE_SQL_MODE]->
                                store(sql_mode.str, sql_mode.length, scs);
  }

  int not_used=0;

  if (et.expression)
  {
    String show_str;
    /* type */
    sch_table->field[ISE_EVENT_TYPE]->store(STRING_WITH_LEN("RECURRING"), scs);

    if (Events::reconstruct_interval_expression(&show_str, et.interval,
                                                et.expression))
      DBUG_RETURN(1);

    sch_table->field[ISE_INTERVAL_VALUE]->set_notnull();
    sch_table->field[ISE_INTERVAL_VALUE]->
                                store(show_str.ptr(), show_str.length(), scs);

    LEX_STRING *ival= &interval_type_to_name[et.interval];
    sch_table->field[ISE_INTERVAL_FIELD]->set_notnull();
    sch_table->field[ISE_INTERVAL_FIELD]->store(ival->str, ival->length, scs);

    /* starts & ends . STARTS is always set - see sql_yacc.yy */
    et.time_zone->gmt_sec_to_TIME(&time, et.starts);
    sch_table->field[ISE_STARTS]->set_notnull();
    sch_table->field[ISE_STARTS]->store_time(&time);

    if (!et.ends_null)
    {
      et.time_zone->gmt_sec_to_TIME(&time, et.ends);
      sch_table->field[ISE_ENDS]->set_notnull();
      sch_table->field[ISE_ENDS]->store_time(&time);
    }
  }
  else
  {
    /* type */
    sch_table->field[ISE_EVENT_TYPE]->store(STRING_WITH_LEN("ONE TIME"), scs);

    et.time_zone->gmt_sec_to_TIME(&time, et.execute_at);
    sch_table->field[ISE_EXECUTE_AT]->set_notnull();
    sch_table->field[ISE_EXECUTE_AT]->store_time(&time);
  }

  /* status */

  switch (et.status)
  {
    case Event_parse_data::ENABLED:
      sch_table->field[ISE_STATUS]->store(STRING_WITH_LEN("ENABLED"), scs);
      break;
    case Event_parse_data::SLAVESIDE_DISABLED:
      sch_table->field[ISE_STATUS]->store(STRING_WITH_LEN("SLAVESIDE_DISABLED"),
                                          scs);
      break;
    case Event_parse_data::DISABLED:
      sch_table->field[ISE_STATUS]->store(STRING_WITH_LEN("DISABLED"), scs);
      break;
    default:
      DBUG_ASSERT(0);
  }
  sch_table->field[ISE_ORIGINATOR]->store(et.originator, TRUE);

  /* on_completion */
  if (et.on_completion == Event_parse_data::ON_COMPLETION_DROP)
    sch_table->field[ISE_ON_COMPLETION]->
                                store(STRING_WITH_LEN("NOT PRESERVE"), scs);
  else
    sch_table->field[ISE_ON_COMPLETION]->
                                store(STRING_WITH_LEN("PRESERVE"), scs);
    
  number_to_datetime(et.created, &time, 0, &not_used);
  DBUG_ASSERT(not_used==0);
  sch_table->field[ISE_CREATED]->store_time(&time);

  number_to_datetime(et.modified, &time, 0, &not_used);
  DBUG_ASSERT(not_used==0);
  sch_table->field[ISE_LAST_ALTERED]->store_time(&time);

  if (et.last_executed)
  {
    et.time_zone->gmt_sec_to_TIME(&time, et.last_executed);
    sch_table->field[ISE_LAST_EXECUTED]->set_notnull();
    sch_table->field[ISE_LAST_EXECUTED]->store_time(&time);
  }

  sch_table->field[ISE_EVENT_COMMENT]->
                      store(et.comment.str, et.comment.length, scs);

  sch_table->field[ISE_CLIENT_CS]->set_notnull();
  sch_table->field[ISE_CLIENT_CS]->store(
    et.creation_ctx->get_client_cs()->csname,
    strlen(et.creation_ctx->get_client_cs()->csname),
    scs);

  sch_table->field[ISE_CONNECTION_CL]->set_notnull();
  sch_table->field[ISE_CONNECTION_CL]->store(
    et.creation_ctx->get_connection_cl()->name,
    strlen(et.creation_ctx->get_connection_cl()->name),
    scs);

  sch_table->field[ISE_DB_CL]->set_notnull();
  sch_table->field[ISE_DB_CL]->store(
    et.creation_ctx->get_db_cl()->name,
    strlen(et.creation_ctx->get_db_cl()->name),
    scs);

  if (schema_table_store_record(thd, sch_table))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}
#endif

int fill_open_tables(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_open_tables");
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *cs= system_charset_info;
  OPEN_TABLE_LIST *open_list;
  if (!(open_list=list_open_tables(thd,thd->lex->select_lex->db, wild))
            && thd->is_fatal_error)
    DBUG_RETURN(1);

  for (; open_list ; open_list=open_list->next)
  {
    restore_record(table, s->default_values);
    table->field[0]->store(open_list->db, strlen(open_list->db), cs);
    table->field[1]->store(open_list->table, strlen(open_list->table), cs);
    table->field[2]->store((longlong) open_list->in_use, TRUE);
    table->field[3]->store((longlong) open_list->locked, TRUE);
    if (schema_table_store_record(thd, table))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

/**
  Issue a deprecation warning for SELECT commands for status and system variables.
*/
void push_select_warning(THD *thd, enum enum_var_type option_type, bool status)
{
  const char *old_name;
  const char *new_name;
  if (option_type == OPT_GLOBAL)
  {
    old_name= (status ? "INFORMATION_SCHEMA.GLOBAL_STATUS" : "INFORMATION_SCHEMA.GLOBAL_VARIABLES");
    new_name= (status ? "performance_schema.global_status" : "performance_schema.global_variables");
  }
  else
  {
    old_name= (status ? "INFORMATION_SCHEMA.SESSION_STATUS" : "INFORMATION_SCHEMA.SESSION_VARIABLES");
    new_name= (status ? "performance_schema.session_status" : "performance_schema.session_variables");
  }

  push_warning_printf(thd, Sql_condition::SL_WARNING,
                      ER_WARN_DEPRECATED_SYNTAX,
                      ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX),
                      old_name, new_name);
}

/**
  Issue an error for SELECT commands for status and system variables.
*/
void push_select_error(THD *thd, enum enum_var_type option_type, bool status)
{
  const char *old_name;
  const char *doc= "show_compatibility_56";
  if (option_type == OPT_GLOBAL)
  {
    old_name= (status ? "INFORMATION_SCHEMA.GLOBAL_STATUS" : "INFORMATION_SCHEMA.GLOBAL_VARIABLES");
  }
  else
  {
    old_name= (status ? "INFORMATION_SCHEMA.SESSION_STATUS" : "INFORMATION_SCHEMA.SESSION_VARIABLES");
  }

  thd->raise_error_printf(ER_FEATURE_DISABLED_SEE_DOC, old_name, doc);
}

int fill_variables(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_variables");
  Show_var_array sys_var_array(PSI_INSTRUMENT_ME);
  int res= 0;

  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  enum enum_schema_tables schema_table_idx=
    get_schema_table_idx(tables->schema_table);
  enum enum_var_type option_type;
  bool upper_case_names= (schema_table_idx != SCH_VARIABLES);
  bool sorted_vars= (schema_table_idx == SCH_VARIABLES);

  if (schema_table_idx == SCH_VARIABLES)
  {
    option_type= lex->option_type;
  }
  else if (schema_table_idx == SCH_GLOBAL_VARIABLES)
  {
    option_type= OPT_GLOBAL;
  }
  else
  {
    DBUG_ASSERT(schema_table_idx == SCH_SESSION_VARIABLES);
    option_type= OPT_SESSION;
  }

#ifndef EMBEDDED_LIBRARY
  /* I_S: Raise error with SHOW_COMPATIBILITY_56=OFF */
  if (! show_compatibility_56)
  {
    push_select_error(thd, option_type, false);
    DBUG_RETURN(1);
  }
  /* I_S: Raise deprecation warning with SHOW_COMPATIBILITY_56=ON */
  if (lex->sql_command != SQLCOM_SHOW_VARIABLES)
  {
    push_select_warning(thd, option_type, false);
  }
#endif /* EMBEDDED_LIBRARY */


  /*
    Some system variables, for example sql_log_bin
    and gtid_executed, have special behavior because
    of deprecation.
    - SELECT @@global.sql_log_bin and
      SELECT @@session.gtid_executed
      MUST print a deprecation warning,
      because such usage needs to be abandoned.
    - SELECT * from INFORMATION_SCHEMA.GLOBAL_VARIABLES
      and SELECT * from INFORMATION_SCHEMA.SESSION_VARIABLES
      MUST NOT print a deprecation warning,
      since the application may not be looking for
      the 'sql_log_bin' or the 'gtid_executed' row anyway,
      and we do not want to create spurious warning noise.
  */
  Silence_deprecation_warnings silencer;
  Silence_deprecation_no_replacement_warnings silencer_no_replacement;
  thd->push_internal_handler(&silencer);
  thd->push_internal_handler(&silencer_no_replacement);

  /*
    Lock LOCK_plugin_delete to avoid deletion of any plugins while creating
    SHOW_VAR array and hold it until all variables are stored in the table.
  */
  if (thd->fill_variables_recursion_level++ == 0)
  {
    mysql_mutex_lock(&LOCK_plugin_delete);
  }

  // Lock LOCK_system_variables_hash to prepare SHOW_VARs array.
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  DEBUG_SYNC(thd, "acquired_LOCK_system_variables_hash");
  enumerate_sys_vars(thd, &sys_var_array, sorted_vars, option_type, false);
  mysql_rwlock_unlock(&LOCK_system_variables_hash);

  res= show_status_array(thd, wild, sys_var_array.begin(), option_type, NULL, "",
                         tables, upper_case_names, cond);

  if (thd->fill_variables_recursion_level-- == 1)
  {
    mysql_mutex_unlock(&LOCK_plugin_delete);
  }

  thd->pop_internal_handler();
  thd->pop_internal_handler();

  DBUG_RETURN(res);
}


int fill_status(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_status");
  LEX *lex= thd->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  int res= 0;

  STATUS_VAR *status_var_ptr;
  STATUS_VAR current_global_status_var;
  enum enum_schema_tables schema_table_idx=
    get_schema_table_idx(tables->schema_table);
  enum enum_var_type option_type;
  bool upper_case_names= (schema_table_idx != SCH_STATUS);

  if (schema_table_idx == SCH_STATUS)
  {
    option_type= lex->option_type;
    if (option_type == OPT_GLOBAL)
      status_var_ptr= &current_global_status_var;
    else
      status_var_ptr= thd->initial_status_var;
  }
  else if (schema_table_idx == SCH_GLOBAL_STATUS)
  {
    option_type= OPT_GLOBAL;
    status_var_ptr= &current_global_status_var;
  }
  else
  {
    DBUG_ASSERT(schema_table_idx == SCH_SESSION_STATUS);
    option_type= OPT_SESSION;
    status_var_ptr= &thd->status_var;
  }

#ifndef EMBEDDED_LIBRARY
  /* I_S: Raise error with SHOW_COMPATIBILITY_56=OFF */
  if (! show_compatibility_56)
  {
    push_select_error(thd, option_type, true);
    DBUG_RETURN(1);
  }
  /* I_S: Raise deprecation warning with SHOW_COMPATIBILITY_56=ON */
  if (lex->sql_command != SQLCOM_SHOW_STATUS)
  {
    push_select_warning(thd, option_type, true);
  }
  if (!show_compatibility_56)
    DBUG_RETURN(res);
#endif /* EMBEDDED_LIBRARY */

  /*
    Avoid recursive acquisition of LOCK_status in cases when WHERE clause
    represented by "cond" contains subquery on I_S.SESSION/GLOBAL_STATUS.
  */
  DEBUG_SYNC(thd, "before_preparing_global_status_array");

  if (thd->fill_status_recursion_level++ == 0) 
    mysql_mutex_lock(&LOCK_status);
  if (option_type == OPT_GLOBAL)
    calc_sum_of_all_status(status_var_ptr);
  // Push an empty tail element
  all_status_vars.push_back(st_mysql_show_var());
  res= show_status_array(thd, wild,
                         &all_status_vars[0],
                         option_type, status_var_ptr, "", tables,
                         upper_case_names, cond);
  all_status_vars.pop_back(); // Pop the empty element.

  if (thd->fill_status_recursion_level-- == 1) 
    mysql_mutex_unlock(&LOCK_status);

  DEBUG_SYNC(thd, "after_preparing_global_status_array");
  DBUG_RETURN(res);
}


/*
  Fill and store records into I_S.referential_constraints table

  SYNOPSIS
    get_referential_constraints_record()
    thd                 thread handle
    tables              table list struct(processed table)
    table               I_S table
    res                 1 means the error during opening of the processed table
                        0 means processed table is opened without error
    base_name           db name
    file_name           table name

  RETURN
    0	ok
    #   error
*/

static int
get_referential_constraints_record(THD *thd, TABLE_LIST *tables,
                                   TABLE *table, bool res,
                                   LEX_STRING *db_name, LEX_STRING *table_name)
{
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_referential_constraints_record");

  if (res)
  {
    if (thd->is_error())
      push_warning(thd, Sql_condition::SL_WARNING,
                   thd->get_stmt_da()->mysql_errno(),
                   thd->get_stmt_da()->message_text());
    thd->clear_error();
    DBUG_RETURN(0);
  }
  if (!tables->is_view())
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      restore_record(table, s->default_values);
      table->field[0]->store(STRING_WITH_LEN("def"), cs);
      table->field[1]->store(db_name->str, db_name->length, cs);
      table->field[9]->store(table_name->str, table_name->length, cs);
      table->field[2]->store(f_key_info->foreign_id->str,
                             f_key_info->foreign_id->length, cs);
      table->field[3]->store(STRING_WITH_LEN("def"), cs);
      table->field[4]->store(f_key_info->referenced_db->str, 
                             f_key_info->referenced_db->length, cs);
      table->field[10]->store(f_key_info->referenced_table->str, 
                             f_key_info->referenced_table->length, cs);
      if (f_key_info->referenced_key_name)
      {
        table->field[5]->store(f_key_info->referenced_key_name->str, 
                               f_key_info->referenced_key_name->length, cs);
        table->field[5]->set_notnull();
      }
      else
        table->field[5]->set_null();
      table->field[6]->store(STRING_WITH_LEN("NONE"), cs);
      table->field[7]->store(f_key_info->update_method->str, 
                             f_key_info->update_method->length, cs);
      table->field[8]->store(f_key_info->delete_method->str, 
                             f_key_info->delete_method->length, cs);
      if (schema_table_store_record(thd, table))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

struct schema_table_ref 
{
  const char *table_name;
  ST_SCHEMA_TABLE *schema_table;
};


/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table_in_plugin()
    thd                 thread handler
    plugin              plugin
    table_name          table name

  RETURN
    0	table not found
    1   found the schema table
*/
static my_bool find_schema_table_in_plugin(THD *thd, plugin_ref plugin,
                                           void* p_table)
{
  schema_table_ref *p_schema_table= (schema_table_ref *)p_table;
  const char* table_name= p_schema_table->table_name;
  ST_SCHEMA_TABLE *schema_table= plugin_data<ST_SCHEMA_TABLE*>(plugin);
  DBUG_ENTER("find_schema_table_in_plugin");

  if (!my_strcasecmp(system_charset_info,
                     schema_table->table_name,
                     table_name)) {
    p_schema_table->schema_table= schema_table;
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table()
    thd                 thread handler
    table_name          table name

  RETURN
    0	table not found
    #   pointer to 'schema_tables' element
*/

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char* table_name)
{
  schema_table_ref schema_table_a;
  ST_SCHEMA_TABLE *schema_table= schema_tables;
  DBUG_ENTER("find_schema_table");

  for (; schema_table->table_name; schema_table++)
  {
    if (!my_strcasecmp(system_charset_info,
                       schema_table->table_name,
                       table_name))
      DBUG_RETURN(schema_table);
  }

  schema_table_a.table_name= table_name;
  if (plugin_foreach(thd, find_schema_table_in_plugin, 
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &schema_table_a))
    DBUG_RETURN(schema_table_a.schema_table);

  DBUG_RETURN(NULL);
}


ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx)
{
  return &schema_tables[schema_table_idx];
}


/**
  Create information_schema table using schema_table data.

  @note
    For MYSQL_TYPE_DECIMAL fields only, the field_length member has encoded
    into it two numbers, based on modulus of base-10 numbers.  In the ones
    position is the number of decimals.  Tens position is unused.  In the
    hundreds and thousands position is a two-digit decimal number representing
    length.  Encode this value with  (decimals*100)+length  , where
    0<decimals<10 and 0<=length<100 .

  @param
    thd	       	          thread handler

  @param table_list Used to pass I_S table information(fields info, tables
  parameters etc) and table name.

  @retval  \#             Pointer to created table
  @retval  NULL           Can't create table
*/

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list)
{
  int field_count= 0;
  Item *item;
  TABLE *table;
  List<Item> field_list;
  ST_SCHEMA_TABLE *schema_table= table_list->schema_table;
  ST_FIELD_INFO *fields_info= schema_table->fields_info;
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("create_schema_table");

  for (; fields_info->field_name; fields_info++)
  {
    switch (fields_info->field_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
      if (!(item= new Item_return_int(fields_info->field_name,
                                      fields_info->field_length,
                                      fields_info->field_type,
                                      fields_info->value)))
      {
        DBUG_RETURN(0);
      }
      item->unsigned_flag= (fields_info->field_flags & MY_I_S_UNSIGNED);
      break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
    {
      const Name_string field_name(fields_info->field_name,
                                   strlen(fields_info->field_name));
      if (!(item=new Item_temporal(fields_info->field_type, field_name, 0, 0)))
        DBUG_RETURN(0);

      if (fields_info->field_type == MYSQL_TYPE_TIMESTAMP ||
          fields_info->field_type == MYSQL_TYPE_DATETIME)
        item->decimals= fields_info->field_length;

      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    {
      const Name_string field_name(fields_info->field_name,
                                   strlen(fields_info->field_name));
      if ((item= new Item_float(field_name, 0.0, NOT_FIXED_DEC, 
                                fields_info->field_length)) == NULL)
        DBUG_RETURN(NULL);
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      if (!(item= new Item_decimal((longlong) fields_info->value, false)))
      {
        DBUG_RETURN(0);
      }
      item->unsigned_flag= (fields_info->field_flags & MY_I_S_UNSIGNED);
      item->decimals= fields_info->field_length%10;
      item->max_length= (fields_info->field_length/100)%100;
      if (item->unsigned_flag == 0)
        item->max_length+= 1;
      if (item->decimals > 0)
        item->max_length+= 1;
      item->item_name.copy(fields_info->field_name);
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      if (!(item= new Item_blob(fields_info->field_name,
                                fields_info->field_length)))
      {
        DBUG_RETURN(0);
      }
      break;
    default:
      /* Don't let unimplemented types pass through. Could be a grave error. */
      DBUG_ASSERT(fields_info->field_type == MYSQL_TYPE_STRING);

      if (!(item= new Item_empty_string("", fields_info->field_length, cs)))
      {
        DBUG_RETURN(0);
      }
      item->item_name.copy(fields_info->field_name);
      break;
    }
    field_list.push_back(item);
    item->maybe_null= (fields_info->field_flags & MY_I_S_MAYBE_NULL);
    field_count++;
  }
  Temp_table_param *tmp_table_param = new (thd->mem_root) Temp_table_param;
  if (!tmp_table_param)
    DBUG_RETURN(0);

  tmp_table_param->table_charset= cs;
  tmp_table_param->field_count= field_count;
  tmp_table_param->schema_table= 1;
  SELECT_LEX *select_lex= thd->lex->current_select();
  if (!(table= create_tmp_table(thd, tmp_table_param,
                                field_list, (ORDER*) 0, 0, 0, 
                                select_lex->active_options() |
                                TMP_TABLE_ALL_COLUMNS,
                                HA_POS_ERROR, table_list->alias)))
    DBUG_RETURN(0);
  my_bitmap_map* bitmaps=
    (my_bitmap_map*) thd->alloc(bitmap_buffer_size(field_count));
  bitmap_init(&table->def_read_set, bitmaps, field_count,
              FALSE);
  table->read_set= &table->def_read_set;
  bitmap_clear_all(table->read_set);
  table_list->schema_table_param= tmp_table_param;
  DBUG_RETURN(table);
}


/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    make_old_format()
    thd			thread handler
    schema_table        pointer to 'schema_tables' element

  RETURN
   1	error
   0	success
*/

int make_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  ST_FIELD_INFO *field_info= schema_table->fields_info;
  Name_resolution_context *context= &thd->lex->select_lex->context;
  for (; field_info->field_name; field_info++)
  {
    if (field_info->old_name)
    {
      Item_field *field= new Item_field(context,
                                        NullS, NullS, field_info->field_name);
      if (field)
      {
        field->item_name.copy(field_info->old_name);
        if (add_item_to_list(thd, field))
          return 1;
      }
    }
  }
  return 0;
}


int make_schemata_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  LEX *lex= thd->lex;
  SELECT_LEX *sel= lex->current_select();
  Name_resolution_context *context= &sel->context;

  if (!sel->item_list.elements)
  {
    ST_FIELD_INFO *field_info= &schema_table->fields_info[1];
    String buffer(tmp,sizeof(tmp), system_charset_info);
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (!field || add_item_to_list(thd, field))
      return 1;
    buffer.length(0);
    buffer.append(field_info->old_name);
    if (lex->wild && lex->wild->ptr())
    {
      buffer.append(STRING_WITH_LEN(" ("));
      buffer.append(lex->wild->ptr());
      buffer.append(')');
    }
    field->item_name.copy(buffer.ptr(), buffer.length(), system_charset_info);
  }
  return 0;
}


int make_table_names_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  String buffer(tmp,sizeof(tmp), thd->charset());
  LEX *lex= thd->lex;
  Name_resolution_context *context= &lex->select_lex->context;

  ST_FIELD_INFO *field_info= &schema_table->fields_info[2];
  buffer.length(0);
  buffer.append(field_info->old_name);
  buffer.append(lex->select_lex->db);
  if (lex->wild && lex->wild->ptr())
  {
    buffer.append(STRING_WITH_LEN(" ("));
    buffer.append(lex->wild->ptr());
    buffer.append(')');
  }
  Item_field *field= new Item_field(context,
                                    NullS, NullS, field_info->field_name);
  if (add_item_to_list(thd, field))
    return 1;
  field->item_name.copy(buffer.ptr(), buffer.length(), system_charset_info);
  if (thd->lex->verbose)
  {
    field->item_name.copy(buffer.ptr(), buffer.length(), system_charset_info);
    field_info= &schema_table->fields_info[3];
    field= new Item_field(context, NullS, NullS, field_info->field_name);
    if (add_item_to_list(thd, field))
      return 1;
    field->item_name.copy(field_info->old_name);
  }
  return 0;
}


int make_columns_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int fields_arr[]= {IS_COLUMNS_COLUMN_NAME,
                     IS_COLUMNS_COLUMN_TYPE,
                     IS_COLUMNS_COLLATION_NAME,
                     IS_COLUMNS_IS_NULLABLE,
                     IS_COLUMNS_COLUMN_KEY,
                     IS_COLUMNS_COLUMN_DEFAULT,
                     IS_COLUMNS_EXTRA,
                     IS_COLUMNS_PRIVILEGES,
                     IS_COLUMNS_COLUMN_COMMENT,
                     -1};
  int *field_num= fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context= &thd->lex->select_lex->context;

  for (; *field_num >= 0; field_num++)
  {
    field_info= &schema_table->fields_info[*field_num];
    if (!thd->lex->verbose && (*field_num == IS_COLUMNS_COLLATION_NAME ||
                               *field_num == IS_COLUMNS_PRIVILEGES     ||
                               *field_num == IS_COLUMNS_COLUMN_COMMENT))
      continue;
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (field)
    {
      field->item_name.copy(field_info->old_name);
      if (add_item_to_list(thd, field))
        return 1;
    }
  }
  return 0;
}


int make_character_sets_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int fields_arr[]= {0, 2, 1, 3, -1};
  int *field_num= fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context= &thd->lex->select_lex->context;

  for (; *field_num >= 0; field_num++)
  {
    field_info= &schema_table->fields_info[*field_num];
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (field)
    {
      field->item_name.copy(field_info->old_name);
      if (add_item_to_list(thd, field))
        return 1;
    }
  }
  return 0;
}


int make_proc_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int fields_arr[]= {IS_ROUTINES_ROUTINE_SCHEMA,
                     IS_ROUTINES_ROUTINE_NAME,
                     IS_ROUTINES_ROUTINE_TYPE,
                     IS_ROUTINES_DEFINER,
                     IS_ROUTINES_LAST_ALTERED,
                     IS_ROUTINES_CREATED,
                     IS_ROUTINES_SECURITY_TYPE,
                     IS_ROUTINES_ROUTINE_COMMENT,
                     IS_ROUTINES_CHARACTER_SET_CLIENT,
                     IS_ROUTINES_COLLATION_CONNECTION,
                     IS_ROUTINES_DATABASE_COLLATION,
                     -1};
  int *field_num= fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context= &thd->lex->select_lex->context;

  for (; *field_num >= 0; field_num++)
  {
    field_info= &schema_table->fields_info[*field_num];
    Item_field *field= new Item_field(context,
                                      NullS, NullS, field_info->field_name);
    if (field)
    {
      field->item_name.copy(field_info->old_name);
      if (add_item_to_list(thd, field))
        return 1;
    }
  }
  return 0;
}


/*
  Create information_schema table

  SYNOPSIS
  mysql_schema_table()
    thd                thread handler
    lex                pointer to LEX
    table_list         pointer to table_list

  RETURN
    0	success
    1   error
*/

int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list)
{
  TABLE *table;
  DBUG_ENTER("mysql_schema_table");
  if (!(table= table_list->schema_table->create_table(thd, table_list)))
    DBUG_RETURN(1);
  table->s->tmp_table= SYSTEM_TMP_TABLE;
  table->grant.privilege= table_list->grant.privilege= SELECT_ACL;
  /*
    This test is necessary to make
    case insensitive file systems +
    upper case table names(information schema tables) +
    views
    working correctly
  */
  if (table_list->schema_table_name)
    table->alias_name_used= my_strcasecmp(table_alias_charset,
                                          table_list->schema_table_name,
                                          table_list->alias);
  table_list->table_name= table->s->table_name.str;
  table_list->table_name_length= table->s->table_name.length;
  table_list->table= table;
  table->pos_in_table_list= table_list;
  table->next= thd->derived_tables;
  thd->derived_tables= table;
  if (table_list->select_lex->first_execution)
    table_list->select_lex->add_base_options(OPTION_SCHEMA_TABLE);
  lex->safe_to_cache_query= 0;

  if (table_list->schema_table_reformed) // show command
  {
    SELECT_LEX *sel= lex->current_select();
    Item *item;
    Field_translator *transl, *org_transl;

    ulonglong want_privilege_saved= thd->want_privilege;
    thd->want_privilege= SELECT_ACL;
    enum enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
    thd->mark_used_columns= MARK_COLUMNS_READ;

    if (table_list->field_translation)
    {
      Field_translator *end= table_list->field_translation_end;
      for (transl= table_list->field_translation; transl < end; transl++)
      {
        if (!transl->item->fixed &&
            transl->item->fix_fields(thd, &transl->item))
          DBUG_RETURN(1);
      }

      thd->want_privilege= want_privilege_saved;
      thd->mark_used_columns= save_mark_used_columns;

      DBUG_RETURN(0);
    }
    List_iterator_fast<Item> it(sel->item_list);
    if (!(transl=
          (Field_translator*)(thd->stmt_arena->
                              alloc(sel->item_list.elements *
                                    sizeof(Field_translator)))))
    {
      DBUG_RETURN(1);
    }
    for (org_transl= transl; (item= it++); transl++)
    {
      transl->item= item;
      transl->name= item->item_name.ptr();
      if (!item->fixed && item->fix_fields(thd, &transl->item))
      {
        DBUG_RETURN(1);
      }
    }

    thd->want_privilege= want_privilege_saved;
    thd->mark_used_columns= save_mark_used_columns;
    table_list->field_translation= org_transl;
    table_list->field_translation_end= transl;
  }

  DBUG_RETURN(0);
}


/*
  Generate select from information_schema table

  SYNOPSIS
    make_schema_select()
    thd                  thread handler
    sel                  pointer to SELECT_LEX
    schema_table_idx     index of 'schema_tables' element

  RETURN
    0	success
    1   error
*/

int make_schema_select(THD *thd, SELECT_LEX *sel,
		       enum enum_schema_tables schema_table_idx)
{
  ST_SCHEMA_TABLE *schema_table= get_schema_table(schema_table_idx);
  LEX_STRING db, table;
  DBUG_ENTER("make_schema_select");
  DBUG_PRINT("enter", ("mysql_schema_select: %s", schema_table->table_name));
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  thd->make_lex_string(&db, INFORMATION_SCHEMA_NAME.str,
                       INFORMATION_SCHEMA_NAME.length, 0);
  thd->make_lex_string(&table, schema_table->table_name,
                       strlen(schema_table->table_name), 0);

  if (schema_table->old_format(thd, schema_table) ||   /* Handle old syntax */
      !sel->add_table_to_list(thd,
                              new Table_ident(thd,
                                              to_lex_cstring(db),
                                              to_lex_cstring(table),
                                              0),
                              0, 0, TL_READ, MDL_SHARED_READ))
  {
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  Fill INFORMATION_SCHEMA-table, leave correct Diagnostics_area
  state after itself.

  This function is a wrapper around ST_SCHEMA_TABLE::fill_table(), which
  may "partially silence" some errors. The thing is that during
  fill_table() many errors might be emitted. These errors stem from the
  nature of fill_table().

  For example, SELECT ... FROM INFORMATION_SCHEMA.xxx WHERE TABLE_NAME = 'xxx'
  results in a number of 'Table <db name>.xxx does not exist' errors,
  because fill_table() tries to open the 'xxx' table in every possible
  database.

  Those errors are cleared (the error status is cleared from
  Diagnostics_area) inside fill_table(), but they remain in the
  Diagnostics_area condition list (the list is not cleared because
  it may contain useful warnings).

  This function is responsible for making sure that Diagnostics_area
  does not contain warnings corresponding to the cleared errors.

  @note: THD::no_warnings_for_error used to be set before calling
  fill_table(), thus those errors didn't go to Diagnostics_area. This is not
  the case now (THD::no_warnings_for_error was eliminated as a hack), so we
  need to take care of those warnings here.

  @param thd            Thread context.
  @param table_list     I_S table.
  @param join_table     JOIN/SELECT table.

  @return Error status.
  @retval TRUE Error.
  @retval FALSE Success.
*/
static bool do_fill_table(THD *thd,
                          TABLE_LIST *table_list,
                          QEP_TAB *qep_tab)
{
  // NOTE: fill_table() may generate many "useless" warnings, which will be
  // ignored afterwards. On the other hand, there might be "useful"
  // warnings, which should be presented to the user. Diagnostics_area usually
  // stores no more than THD::variables.max_error_count warnings.
  // The problem is that "useless warnings" may occupy all the slots in the
  // Diagnostics_area, so "useful warnings" get rejected. In order to avoid
  // that problem we create a Diagnostics_area instance, which is capable of
  // storing "unlimited" number of warnings.
  Diagnostics_area *da= thd->get_stmt_da();
  Diagnostics_area tmp_da(true);

  // Don't copy existing conditions from the old DA so we don't get them twice
  // when we call copy_non_errors_from_da below.
  thd->push_diagnostics_area(&tmp_da, false);

  /*
    We pass a condition, which can be used to do less file manipulations (for
    example, WHERE TABLE_SCHEMA='test' allows to open only directory 'test',
    not other database directories). Filling schema tables is done before
    QEP_TAB::sort_table() (=filesort, for ORDER BY), so we can trust
    that condition() is complete, has not been zeroed by filesort:
  */
  DBUG_ASSERT(qep_tab->condition() == qep_tab->condition_optim());

  bool res= table_list->schema_table->fill_table(
    thd, table_list, qep_tab->condition());

  thd->pop_diagnostics_area();

  // Pass an error if any.
  if (tmp_da.is_error())
  {
    da->set_error_status(tmp_da.mysql_errno(),
                         tmp_da.message_text(),
                         tmp_da.returned_sqlstate());
    da->push_warning(thd,
                     tmp_da.mysql_errno(),
                     tmp_da.returned_sqlstate(),
                     Sql_condition::SL_ERROR,
                     tmp_da.message_text());
  }

  // Pass warnings (if any).
  //
  // Filter out warnings with SL_ERROR level, because they
  // correspond to the errors which were filtered out in fill_table().
  da->copy_non_errors_from_da(thd, &tmp_da);

  return res;
}


/*
  Fill temporary schema tables before SELECT

  SYNOPSIS
    get_schema_tables_result()
    join  join which use schema tables
    executed_place place where I_S table processed

  RETURN
    FALSE success
    TRUE  error
*/

bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place)
{
  THD *thd= join->thd;
  bool result= 0;
  DBUG_ENTER("get_schema_tables_result");

  /* Check if the schema table is optimized away */
  if (!join->qep_tab)
    DBUG_RETURN(result);

  for (uint i= 0; i < join->tables; i++)
  {
    QEP_TAB *const tab= join->qep_tab + i;
    if (!tab->table() || !tab->table_ref)
      continue;

    TABLE_LIST *const table_list= tab->table_ref;
    if (table_list->schema_table && thd->fill_information_schema_tables())
    {
      bool is_subselect= join->select_lex->master_unit() &&
                         join->select_lex->master_unit()->item;

      /* A value of 0 indicates a dummy implementation */
      if (table_list->schema_table->fill_table == 0)
        continue;

      /* skip I_S optimizations specific to get_all_tables */
      if (thd->lex->describe &&
          (table_list->schema_table->fill_table != get_all_tables))
        continue;

      /*
        If schema table is already processed and
        the statement is not a subselect then
        we don't need to fill this table again.
        If schema table is already processed and
        schema_table_state != executed_place then
        table is already processed and
        we should skip second data processing.
      */
      if (table_list->schema_table_state &&
          (!is_subselect || table_list->schema_table_state != executed_place))
        continue;

      /*
        if table is used in a subselect and
        table has been processed earlier with the same
        'executed_place' value then we should refresh the table.
      */
      if (table_list->schema_table_state && is_subselect)
      {
        table_list->table->file->extra(HA_EXTRA_NO_CACHE);
        table_list->table->file->extra(HA_EXTRA_RESET_STATE);
        table_list->table->file->ha_delete_all_rows();
        free_io_cache(table_list->table);
        filesort_free_buffers(table_list->table,1);
        table_list->table->reset_null_row();
      }
      else
        table_list->table->file->stats.records= 0;

      /* To be removed after 5.7 */
      if (is_infoschema_db(table_list->db, table_list->db_length))
      {
        static LEX_STRING INNODB_LOCKS= {C_STRING_WITH_LEN("INNODB_LOCKS")};
        static LEX_STRING INNODB_LOCK_WAITS= {C_STRING_WITH_LEN("INNODB_LOCK_WAITS")};

        if (my_strcasecmp(system_charset_info,
                          table_list->schema_table_name,
                          INNODB_LOCKS.str) == 0)
        {
          /* Deprecated in 5.7 */
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
                              ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
                              "INFORMATION_SCHEMA.INNODB_LOCKS");
        }
        else if (my_strcasecmp(system_charset_info,
                               table_list->schema_table_name,
                               INNODB_LOCK_WAITS.str) == 0)
        {
          /* Deprecated in 5.7 */
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
                              ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
                              "INFORMATION_SCHEMA.INNODB_LOCK_WAITS");
        }
      }

      if (do_fill_table(thd, table_list, tab))
      {
        result= 1;
        join->error= 1;
        table_list->schema_table_state= executed_place;
        break;
      }
      table_list->schema_table_state= executed_place;
    }
  }
  DBUG_RETURN(result);
}

struct run_hton_fill_schema_table_args
{
  TABLE_LIST *tables;
  Item *cond;
};

static my_bool run_hton_fill_schema_table(THD *thd, plugin_ref plugin,
                                          void *arg)
{
  struct run_hton_fill_schema_table_args *args=
    (run_hton_fill_schema_table_args *) arg;
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->fill_is_table && hton->state == SHOW_OPTION_YES)
      hton->fill_is_table(hton, thd, args->tables, args->cond,
            get_schema_table_idx(args->tables->schema_table));
  return false;
}

int hton_fill_schema_table(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("hton_fill_schema_table");

  struct run_hton_fill_schema_table_args args;
  args.tables= tables;
  args.cond= cond;

  plugin_foreach(thd, run_hton_fill_schema_table,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args);

  DBUG_RETURN(0);
}


ST_FIELD_INFO schema_fields_info[]=
{
  {"CATALOG_NAME", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"SCHEMA_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database",
   SKIP_OPEN_TABLE},
  {"DEFAULT_CHARACTER_SET_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"DEFAULT_COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"SQL_PATH", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO tables_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"TABLE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"ENGINE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Engine", OPEN_FRM_ONLY},
  {"VERSION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Version", OPEN_FRM_ONLY},
  {"ROW_FORMAT", 10, MYSQL_TYPE_STRING, 0, 1, "Row_format", OPEN_FULL_TABLE},
  {"TABLE_ROWS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Rows", OPEN_FULL_TABLE},
  {"AVG_ROW_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Avg_row_length", OPEN_FULL_TABLE},
  {"DATA_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_length", OPEN_FULL_TABLE},
  {"MAX_DATA_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Max_data_length", OPEN_FULL_TABLE},
  {"INDEX_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Index_length", OPEN_FULL_TABLE},
  {"DATA_FREE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_free", OPEN_FULL_TABLE},
  {"AUTO_INCREMENT", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Auto_increment", OPEN_FULL_TABLE},
  {"CREATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Create_time", OPEN_FULL_TABLE},
  {"UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Update_time", OPEN_FULL_TABLE},
  {"CHECK_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Check_time", OPEN_FULL_TABLE},
  {"TABLE_COLLATION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 1, "Collation",
   OPEN_FRM_ONLY},
  {"CHECKSUM", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Checksum", OPEN_FULL_TABLE},
  {"CREATE_OPTIONS", 255, MYSQL_TYPE_STRING, 0, 1, "Create_options",
   OPEN_FRM_ONLY},
  {"TABLE_COMMENT", TABLE_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, 
   "Comment", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO columns_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Field",
   OPEN_FRM_ONLY},
  {"ORDINAL_POSITION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_UNSIGNED, 0, OPEN_FRM_ONLY},
  {"COLUMN_DEFAULT", MAX_FIELD_VARCHARLENGTH, MYSQL_TYPE_STRING, 0,
   1, "Default", OPEN_FRM_ONLY},
  {"IS_NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
  {"DATA_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"CHARACTER_MAXIMUM_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"CHARACTER_OCTET_LENGTH", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"NUMERIC_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"NUMERIC_SCALE", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FRM_ONLY},
  {"DATETIME_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"CHARACTER_SET_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FRM_ONLY},
  {"COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 1, "Collation",
   OPEN_FRM_ONLY},
  {"COLUMN_TYPE", 65535, MYSQL_TYPE_STRING, 0, 0, "Type", OPEN_FRM_ONLY},
  {"COLUMN_KEY", 3, MYSQL_TYPE_STRING, 0, 0, "Key", OPEN_FRM_ONLY},
  {"EXTRA", 30, MYSQL_TYPE_STRING, 0, 0, "Extra", OPEN_FRM_ONLY},
  {"PRIVILEGES", 80, MYSQL_TYPE_STRING, 0, 0, "Privileges", OPEN_FRM_ONLY},
  {"COLUMN_COMMENT", COLUMN_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, 
   "Comment", OPEN_FRM_ONLY},
  {"GENERATION_EXPRESSION", GENERATED_COLUMN_EXPRESSION_MAXLEN, MYSQL_TYPE_STRING,
   0, 0, "Generation expression", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO charsets_fields_info[]=
{
  {"CHARACTER_SET_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, "Charset",
   SKIP_OPEN_TABLE},
  {"DEFAULT_COLLATE_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "Default collation", SKIP_OPEN_TABLE},
  {"DESCRIPTION", 60, MYSQL_TYPE_STRING, 0, 0, "Description",
   SKIP_OPEN_TABLE},
  {"MAXLEN", 3, MYSQL_TYPE_LONGLONG, 0, 0, "Maxlen", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO collation_fields_info[]=
{
  {"COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, "Collation",
   SKIP_OPEN_TABLE},
  {"CHARACTER_SET_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, "Charset",
   SKIP_OPEN_TABLE},
  {"ID", MY_INT32_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, "Id",
   SKIP_OPEN_TABLE},
  {"IS_DEFAULT", 3, MYSQL_TYPE_STRING, 0, 0, "Default", SKIP_OPEN_TABLE},
  {"IS_COMPILED", 3, MYSQL_TYPE_STRING, 0, 0, "Compiled", SKIP_OPEN_TABLE},
  {"SORTLEN", 3, MYSQL_TYPE_LONGLONG, 0, 0, "Sortlen", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO engines_fields_info[]=
{
  {"ENGINE", 64, MYSQL_TYPE_STRING, 0, 0, "Engine", SKIP_OPEN_TABLE},
  {"SUPPORT", 8, MYSQL_TYPE_STRING, 0, 0, "Support", SKIP_OPEN_TABLE},
  {"COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, "Comment", SKIP_OPEN_TABLE},
  {"TRANSACTIONS", 3, MYSQL_TYPE_STRING, 0, 1, "Transactions", SKIP_OPEN_TABLE},
  {"XA", 3, MYSQL_TYPE_STRING, 0, 1, "XA", SKIP_OPEN_TABLE},
  {"SAVEPOINTS", 3 ,MYSQL_TYPE_STRING, 0, 1, "Savepoints", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO events_fields_info[]=
{
  {"EVENT_CATALOG", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EVENT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Db",
   SKIP_OPEN_TABLE},
  {"EVENT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"DEFINER", 93, MYSQL_TYPE_STRING, 0, 0, "Definer", SKIP_OPEN_TABLE},
  {"TIME_ZONE", 64, MYSQL_TYPE_STRING, 0, 0, "Time zone", SKIP_OPEN_TABLE},
  {"EVENT_BODY", 8, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EVENT_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EVENT_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
  {"EXECUTE_AT", 0, MYSQL_TYPE_DATETIME, 0, 1, "Execute at", SKIP_OPEN_TABLE},
  {"INTERVAL_VALUE", 256, MYSQL_TYPE_STRING, 0, 1, "Interval value",
   SKIP_OPEN_TABLE},
  {"INTERVAL_FIELD", 18, MYSQL_TYPE_STRING, 0, 1, "Interval field",
   SKIP_OPEN_TABLE},
  {"SQL_MODE", 32*256, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"STARTS", 0, MYSQL_TYPE_DATETIME, 0, 1, "Starts", SKIP_OPEN_TABLE},
  {"ENDS", 0, MYSQL_TYPE_DATETIME, 0, 1, "Ends", SKIP_OPEN_TABLE},
  {"STATUS", 18, MYSQL_TYPE_STRING, 0, 0, "Status", SKIP_OPEN_TABLE},
  {"ON_COMPLETION", 12, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"CREATED", 0, MYSQL_TYPE_DATETIME, 0, 0, 0, SKIP_OPEN_TABLE},
  {"LAST_ALTERED", 0, MYSQL_TYPE_DATETIME, 0, 0, 0, SKIP_OPEN_TABLE},
  {"LAST_EXECUTED", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EVENT_COMMENT", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ORIGINATOR", 10, MYSQL_TYPE_LONGLONG, 0, 0, "Originator", SKIP_OPEN_TABLE},
  {"CHARACTER_SET_CLIENT", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "character_set_client", SKIP_OPEN_TABLE},
  {"COLLATION_CONNECTION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "collation_connection", SKIP_OPEN_TABLE},
  {"DATABASE_COLLATION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "Database Collation", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};



ST_FIELD_INFO coll_charset_app_fields_info[]=
{
  {"COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"CHARACTER_SET_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO proc_fields_info[]=
{
  {"SPECIFIC_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROUTINE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROUTINE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Db",
   SKIP_OPEN_TABLE},
  {"ROUTINE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"ROUTINE_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
  {"DATA_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"CHARACTER_MAXIMUM_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"CHARACTER_OCTET_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"NUMERIC_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, SKIP_OPEN_TABLE},
  {"NUMERIC_SCALE", 21 , MYSQL_TYPE_LONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"DATETIME_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, SKIP_OPEN_TABLE},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"DTD_IDENTIFIER", 65535, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"ROUTINE_BODY", 8, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROUTINE_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EXTERNAL_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EXTERNAL_LANGUAGE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   SKIP_OPEN_TABLE},
  {"PARAMETER_STYLE", 8, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_DETERMINISTIC", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"SQL_DATA_ACCESS", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"SQL_PATH", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"SECURITY_TYPE", 7, MYSQL_TYPE_STRING, 0, 0, "Security_type",
   SKIP_OPEN_TABLE},
  {"CREATED", 0, MYSQL_TYPE_DATETIME, 0, 0, "Created", SKIP_OPEN_TABLE},
  {"LAST_ALTERED", 0, MYSQL_TYPE_DATETIME, 0, 0, "Modified", SKIP_OPEN_TABLE},
  {"SQL_MODE", 32*256, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"ROUTINE_COMMENT", 65535, MYSQL_TYPE_STRING, 0, 0, "Comment",
   SKIP_OPEN_TABLE},
  {"DEFINER", 93, MYSQL_TYPE_STRING, 0, 0, "Definer", SKIP_OPEN_TABLE},
  {"CHARACTER_SET_CLIENT", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "character_set_client", SKIP_OPEN_TABLE},
  {"COLLATION_CONNECTION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "collation_connection", SKIP_OPEN_TABLE},
  {"DATABASE_COLLATION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "Database Collation", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO stat_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", OPEN_FRM_ONLY},
  {"NON_UNIQUE", 1, MYSQL_TYPE_LONGLONG, 0, 0, "Non_unique", OPEN_FRM_ONLY},
  {"INDEX_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"INDEX_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Key_name",
   OPEN_FRM_ONLY},
  {"SEQ_IN_INDEX", 2, MYSQL_TYPE_LONGLONG, 0, 0, "Seq_in_index", OPEN_FRM_ONLY},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Column_name",
   OPEN_FRM_ONLY},
  {"COLLATION", 1, MYSQL_TYPE_STRING, 0, 1, "Collation", OPEN_FRM_ONLY},
  {"CARDINALITY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 1,
   "Cardinality", OPEN_FULL_TABLE},
  {"SUB_PART", 3, MYSQL_TYPE_LONGLONG, 0, 1, "Sub_part", OPEN_FRM_ONLY},
  {"PACKED", 10, MYSQL_TYPE_STRING, 0, 1, "Packed", OPEN_FRM_ONLY},
  {"NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
  {"INDEX_TYPE", 16, MYSQL_TYPE_STRING, 0, 0, "Index_type", OPEN_FULL_TABLE},
  {"COMMENT", 16, MYSQL_TYPE_STRING, 0, 1, "Comment", OPEN_FRM_ONLY},
  {"INDEX_COMMENT", INDEX_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0, 
   "Index_comment", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO view_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"VIEW_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"CHECK_OPTION", 8, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"IS_UPDATABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"DEFINER", 93, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"SECURITY_TYPE", 7, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"CHARACTER_SET_CLIENT", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FRM_ONLY},
  {"COLLATION_CONNECTION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO user_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO schema_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO column_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_constraints_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO key_column_usage_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ORDINAL_POSITION", 10 ,MYSQL_TYPE_LONGLONG, 0, 0, 0, OPEN_FULL_TABLE},
  {"POSITION_IN_UNIQUE_CONSTRAINT", 10 ,MYSQL_TYPE_LONGLONG, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"REFERENCED_COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO table_names_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Tables_in_",
   SKIP_OPEN_TABLE},
  {"TABLE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table_type",
   OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO open_tables_fields_info[]=
{
  {"Database", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database",
   SKIP_OPEN_TABLE},
  {"Table",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", SKIP_OPEN_TABLE},
  {"In_use", 1, MYSQL_TYPE_LONGLONG, 0, 0, "In_use", SKIP_OPEN_TABLE},
  {"Name_locked", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Name_locked", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO triggers_fields_info[]=
{
  {"TRIGGER_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TRIGGER_SCHEMA",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"TRIGGER_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Trigger",
   OPEN_FRM_ONLY},
  {"EVENT_MANIPULATION", 6, MYSQL_TYPE_STRING, 0, 0, "Event", OPEN_FRM_ONLY},
  {"EVENT_OBJECT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FRM_ONLY},
  {"EVENT_OBJECT_SCHEMA",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FRM_ONLY},
  {"EVENT_OBJECT_TABLE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table",
   OPEN_FRM_ONLY},
  {"ACTION_ORDER", 4, MYSQL_TYPE_LONGLONG, 0, 0, 0, OPEN_FRM_ONLY},
  {"ACTION_CONDITION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FRM_ONLY},
  {"ACTION_STATEMENT", 65535, MYSQL_TYPE_STRING, 0, 0, "Statement",
   OPEN_FRM_ONLY},
  {"ACTION_ORIENTATION", 9, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"ACTION_TIMING", 6, MYSQL_TYPE_STRING, 0, 0, "Timing", OPEN_FRM_ONLY},
  {"ACTION_REFERENCE_OLD_TABLE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FRM_ONLY},
  {"ACTION_REFERENCE_NEW_TABLE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FRM_ONLY},
  {"ACTION_REFERENCE_OLD_ROW", 3, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  {"ACTION_REFERENCE_NEW_ROW", 3, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
  /*
    Set field_length to the value of 2 for field type MYSQL_TYPE_DATETIME.
    It allows later during instantiation of class Item_temporal to remember
    the number of digits in the fractional part of time and use it when
    the value of MYSQL_TYPE_DATETIME is stored in the Field.
  */
  {"CREATED", 2, MYSQL_TYPE_DATETIME, 0, 1, "Created", OPEN_FRM_ONLY},
  {"SQL_MODE", 32*256, MYSQL_TYPE_STRING, 0, 0, "sql_mode", OPEN_FRM_ONLY},
  {"DEFINER", 93, MYSQL_TYPE_STRING, 0, 0, "Definer", OPEN_FRM_ONLY},
  {"CHARACTER_SET_CLIENT", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "character_set_client", OPEN_FRM_ONLY},
  {"COLLATION_CONNECTION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "collation_connection", OPEN_FRM_ONLY},
  {"DATABASE_COLLATION", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 0,
   "Database Collation", OPEN_FRM_ONLY},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO partitions_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_SCHEMA",NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"PARTITION_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"SUBPARTITION_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"PARTITION_ORDINAL_POSITION", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"SUBPARTITION_ORDINAL_POSITION", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"PARTITION_METHOD", 18, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"SUBPARTITION_METHOD", 12, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"PARTITION_EXPRESSION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"SUBPARTITION_EXPRESSION", 65535, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {"PARTITION_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"TABLE_ROWS", 21 , MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
   OPEN_FULL_TABLE},
  {"AVG_ROW_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
   OPEN_FULL_TABLE},
  {"DATA_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
   OPEN_FULL_TABLE},
  {"MAX_DATA_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"INDEX_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
   OPEN_FULL_TABLE},
  {"DATA_FREE", 21 , MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
   OPEN_FULL_TABLE},
  {"CREATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
  {"UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
  {"CHECK_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
  {"CHECKSUM", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"PARTITION_COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"NODEGROUP", 12 , MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLESPACE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO variables_fields_info[]=
{
  {"VARIABLE_NAME", 64, MYSQL_TYPE_STRING, 0, 0, "Variable_name",
   SKIP_OPEN_TABLE},
  {"VARIABLE_VALUE", 1024, MYSQL_TYPE_STRING, 0, 1, "Value", SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO processlist_fields_info[]=
{
  {"ID", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, "Id", SKIP_OPEN_TABLE},
  {"USER", USERNAME_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, "User", SKIP_OPEN_TABLE},
  {"HOST", LIST_PROCESS_HOST_LEN,  MYSQL_TYPE_STRING, 0, 0, "Host",
   SKIP_OPEN_TABLE},
  {"DB", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Db", SKIP_OPEN_TABLE},
  {"COMMAND", 16, MYSQL_TYPE_STRING, 0, 0, "Command", SKIP_OPEN_TABLE},
  {"TIME", 7, MYSQL_TYPE_LONG, 0, 0, "Time", SKIP_OPEN_TABLE},
  {"STATE", 64, MYSQL_TYPE_STRING, 0, 1, "State", SKIP_OPEN_TABLE},
  {"INFO", PROCESS_LIST_INFO_WIDTH, MYSQL_TYPE_STRING, 0, 1, "Info",
   SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO plugin_fields_info[]=
{
  {"PLUGIN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
   SKIP_OPEN_TABLE},
  {"PLUGIN_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_STATUS", 10, MYSQL_TYPE_STRING, 0, 0, "Status", SKIP_OPEN_TABLE},
  {"PLUGIN_TYPE", 80, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
  {"PLUGIN_TYPE_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_LIBRARY", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Library",
   SKIP_OPEN_TABLE},
  {"PLUGIN_LIBRARY_VERSION", 20, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_AUTHOR", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"PLUGIN_LICENSE", 80, MYSQL_TYPE_STRING, 0, 1, "License", SKIP_OPEN_TABLE},
  {"LOAD_OPTION", 64, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};

ST_FIELD_INFO files_fields_info[]=
{
  {"FILE_ID", 4, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"FILE_NAME", FN_REFLEN_SE, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"FILE_TYPE", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLESPACE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   SKIP_OPEN_TABLE},
  {"TABLE_CATALOG", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"LOGFILE_GROUP_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
   SKIP_OPEN_TABLE},
  {"LOGFILE_GROUP_NUMBER", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"ENGINE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"FULLTEXT_KEYS", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {"DELETED_ROWS", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"UPDATE_COUNT", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"FREE_EXTENTS", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TOTAL_EXTENTS", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"EXTENT_SIZE", 4, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"INITIAL_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, SKIP_OPEN_TABLE},
  {"MAXIMUM_SIZE", 21, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, SKIP_OPEN_TABLE},
  {"AUTOEXTEND_SIZE", 21, MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, SKIP_OPEN_TABLE},
  {"CREATION_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, SKIP_OPEN_TABLE},
  {"LAST_UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, SKIP_OPEN_TABLE},
  {"LAST_ACCESS_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, SKIP_OPEN_TABLE},
  {"RECOVER_TIME", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"TRANSACTION_COUNTER", 4, MYSQL_TYPE_LONGLONG, 0, 1, 0, SKIP_OPEN_TABLE},
  {"VERSION", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Version", SKIP_OPEN_TABLE},
  {"ROW_FORMAT", 10, MYSQL_TYPE_STRING, 0, 1, "Row_format", SKIP_OPEN_TABLE},
  {"TABLE_ROWS", 21 , MYSQL_TYPE_LONGLONG, 0,
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Rows", SKIP_OPEN_TABLE},
  {"AVG_ROW_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Avg_row_length", SKIP_OPEN_TABLE},
  {"DATA_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_length", SKIP_OPEN_TABLE},
  {"MAX_DATA_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Max_data_length", SKIP_OPEN_TABLE},
  {"INDEX_LENGTH", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Index_length", SKIP_OPEN_TABLE},
  {"DATA_FREE", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Data_free", SKIP_OPEN_TABLE},
  {"CREATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Create_time", SKIP_OPEN_TABLE},
  {"UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Update_time", SKIP_OPEN_TABLE},
  {"CHECK_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, "Check_time", SKIP_OPEN_TABLE},
  {"CHECKSUM", 21 , MYSQL_TYPE_LONGLONG, 0, 
   (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), "Checksum", SKIP_OPEN_TABLE},
  {"STATUS", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"EXTRA", 255, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};

void init_fill_schema_files_row(TABLE* table)
{
  int i;
  for(i=0; files_fields_info[i].field_name!=NULL; i++)
    table->field[i]->set_null();

  table->field[IS_FILES_STATUS]->set_notnull();
  table->field[IS_FILES_STATUS]->store("NORMAL", 6, system_charset_info);
}

ST_FIELD_INFO referential_constraints_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"UNIQUE_CONSTRAINT_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0,
   MY_I_S_MAYBE_NULL, 0, OPEN_FULL_TABLE},
  {"MATCH_OPTION", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"UPDATE_RULE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"DELETE_RULE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"REFERENCED_TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


ST_FIELD_INFO parameters_fields_info[]=
{
  {"SPECIFIC_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"SPECIFIC_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   OPEN_FULL_TABLE},
  {"SPECIFIC_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ORDINAL_POSITION", 21 , MYSQL_TYPE_LONG, 0, 0, 0, OPEN_FULL_TABLE},
  {"PARAMETER_MODE", 5, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"PARAMETER_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"DATA_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"CHARACTER_MAXIMUM_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"CHARACTER_OCTET_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"NUMERIC_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"NUMERIC_SCALE", 21 , MYSQL_TYPE_LONG, 0, 1, 0, OPEN_FULL_TABLE},
  {"DATETIME_PRECISION", MY_INT64_NUM_DECIMAL_DIGITS , MYSQL_TYPE_LONGLONG,
   0, (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
  {"CHARACTER_SET_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"COLLATION_NAME", 64, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
  {"DTD_IDENTIFIER", 65535, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {"ROUTINE_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE}
};


ST_FIELD_INFO tablespaces_fields_info[]=
{
  {"TABLESPACE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
   SKIP_OPEN_TABLE},
  {"ENGINE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLESPACE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL,
   0, SKIP_OPEN_TABLE},
  {"LOGFILE_GROUP_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL,
   0, SKIP_OPEN_TABLE},
  {"EXTENT_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
  {"AUTOEXTEND_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
  {"MAXIMUM_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
  {"NODEGROUP_ID", 21, MYSQL_TYPE_LONGLONG, 0,
   MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
  {"TABLESPACE_COMMENT", 2048, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL, 0,
   SKIP_OPEN_TABLE},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};


/** For creating fields of information_schema.OPTIMIZER_TRACE */
extern ST_FIELD_INFO optimizer_trace_info[];

/*
  Description of ST_FIELD_INFO in table.h

  Make sure that the order of schema_tables and enum_schema_tables are the same.

*/

ST_SCHEMA_TABLE schema_tables[]=
{
  {"CHARACTER_SETS", charsets_fields_info, create_schema_table, 
   fill_schema_charsets, make_character_sets_old_format, 0, -1, -1, 0, 0},
  {"COLLATIONS", collation_fields_info, create_schema_table, 
   fill_schema_collation, make_old_format, 0, -1, -1, 0, 0},
  {"COLLATION_CHARACTER_SET_APPLICABILITY", coll_charset_app_fields_info,
   create_schema_table, fill_schema_coll_charset_app, 0, 0, -1, -1, 0, 0},
  {"COLUMNS", columns_fields_info, create_schema_table, 
   get_all_tables, make_columns_old_format, get_schema_column_record, 1, 2, 0,
   OPTIMIZE_I_S_TABLE|OPEN_VIEW_FULL},
  {"COLUMN_PRIVILEGES", column_privileges_fields_info, create_schema_table,
   fill_schema_column_privileges, 0, 0, -1, -1, 0, 0},
  {"ENGINES", engines_fields_info, create_schema_table,
   fill_schema_engines, make_old_format, 0, -1, -1, 0, 0},
#ifndef EMBEDDED_LIBRARY
  {"EVENTS", events_fields_info, create_schema_table,
   Events::fill_schema_events, make_old_format, 0, -1, -1, 0, 0},
#else // for alignment with enum_schema_tables
  {"EVENTS", events_fields_info, create_schema_table,
   0, make_old_format, 0, -1, -1, 0, 0},
#endif
  {"FILES", files_fields_info, create_schema_table,
   hton_fill_schema_table, 0, 0, -1, -1, 0, 0},
  {"GLOBAL_STATUS", variables_fields_info, create_schema_table,
   fill_status, make_old_format, 0, 0, -1, 0, 0},
  {"GLOBAL_VARIABLES", variables_fields_info, create_schema_table,
   fill_variables, make_old_format, 0, 0, -1, 0, 0},
  {"KEY_COLUMN_USAGE", key_column_usage_fields_info, create_schema_table,
   get_all_tables, 0, get_schema_key_column_usage_record, 4, 5, 0,
   OPTIMIZE_I_S_TABLE|OPEN_TABLE_ONLY},
  {"OPEN_TABLES", open_tables_fields_info, create_schema_table,
   fill_open_tables, make_old_format, 0, -1, -1, 1, 0},
#ifdef OPTIMIZER_TRACE
  {"OPTIMIZER_TRACE", optimizer_trace_info, create_schema_table,
   fill_optimizer_trace_info, NULL, NULL, -1, -1, false, 0},
#else // for alignment with enum_schema_tables
  {"OPTIMIZER_TRACE", optimizer_trace_info, create_schema_table,
   NULL, NULL, NULL, -1, -1, false, 0},
#endif
  {"PARAMETERS", parameters_fields_info, create_schema_table,
   fill_schema_proc, 0, 0, -1, -1, 0, 0},
  {"PARTITIONS", partitions_fields_info, create_schema_table,
   get_all_tables, 0, get_schema_partitions_record, 1, 2, 0,
   OPTIMIZE_I_S_TABLE|OPEN_TABLE_ONLY},
  {"PLUGINS", plugin_fields_info, create_schema_table,
   fill_plugins, make_old_format, 0, -1, -1, 0, 0},
  {"PROCESSLIST", processlist_fields_info, create_schema_table,
   fill_schema_processlist, make_old_format, 0, -1, -1, 0, 0},
  {"PROFILING", query_profile_statistics_info, create_schema_table,
    fill_query_profile_statistics_info, make_profile_table_for_show, 
    NULL, -1, -1, false, 0},
  {"REFERENTIAL_CONSTRAINTS", referential_constraints_fields_info,
   create_schema_table, get_all_tables, 0, get_referential_constraints_record,
   1, 9, 0, OPTIMIZE_I_S_TABLE|OPEN_TABLE_ONLY},
  {"ROUTINES", proc_fields_info, create_schema_table, 
   fill_schema_proc, make_proc_old_format, 0, -1, -1, 0, 0},
  {"SCHEMATA", schema_fields_info, create_schema_table,
   fill_schema_schemata, make_schemata_old_format, 0, 1, -1, 0, 0},
  {"SCHEMA_PRIVILEGES", schema_privileges_fields_info, create_schema_table,
   fill_schema_schema_privileges, 0, 0, -1, -1, 0, 0},
  {"SESSION_STATUS", variables_fields_info, create_schema_table,
   fill_status, make_old_format, 0, 0, -1, 0, 0},
  {"SESSION_VARIABLES", variables_fields_info, create_schema_table,
   fill_variables, make_old_format, 0, 0, -1, 0, 0},
  {"STATISTICS", stat_fields_info, create_schema_table, 
   get_all_tables, make_old_format, get_schema_stat_record, 1, 2, 0,
   OPEN_TABLE_ONLY|OPTIMIZE_I_S_TABLE},
  {"STATUS", variables_fields_info, create_schema_table, fill_status, 
   make_old_format, 0, 0, -1, 1, 0},
  {"TABLES", tables_fields_info, create_schema_table, 
   get_all_tables, make_old_format, get_schema_tables_record, 1, 2, 0,
   OPTIMIZE_I_S_TABLE},
  {"TABLESPACES", tablespaces_fields_info, create_schema_table,
   hton_fill_schema_table, 0, 0, -1, -1, 0, 0},
  {"TABLE_CONSTRAINTS", table_constraints_fields_info, create_schema_table,
   get_all_tables, 0, get_schema_constraints_record, 3, 4, 0,
   OPTIMIZE_I_S_TABLE|OPEN_TABLE_ONLY},
  {"TABLE_NAMES", table_names_fields_info, create_schema_table,
   get_all_tables, make_table_names_old_format, 0, 1, 2, 1, 0},
  {"TABLE_PRIVILEGES", table_privileges_fields_info, create_schema_table,
   fill_schema_table_privileges, 0, 0, -1, -1, 0, 0},
  {"TRIGGERS", triggers_fields_info, create_schema_table,
   get_all_tables, make_old_format, get_schema_triggers_record, 5, 6, 0,
   OPEN_TRIGGER_ONLY|OPTIMIZE_I_S_TABLE},
  {"USER_PRIVILEGES", user_privileges_fields_info, create_schema_table, 
   fill_schema_user_privileges, 0, 0, -1, -1, 0, 0},
  {"VARIABLES", variables_fields_info, create_schema_table, fill_variables,
   make_old_format, 0, 0, -1, 1, 0},
  {"VIEWS", view_fields_info, create_schema_table, 
   get_all_tables, 0, get_schema_views_record, 1, 2, 0,
   OPEN_VIEW_ONLY|OPTIMIZE_I_S_TABLE},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


int initialize_schema_table(st_plugin_int *plugin)
{
  ST_SCHEMA_TABLE *schema_table;
  DBUG_ENTER("initialize_schema_table");

  if (!(schema_table= (ST_SCHEMA_TABLE *)my_malloc(key_memory_ST_SCHEMA_TABLE,
                                                   sizeof(ST_SCHEMA_TABLE),
                                MYF(MY_WME | MY_ZEROFILL))))
      DBUG_RETURN(1);
  /* Historical Requirement */
  plugin->data= schema_table; // shortcut for the future
  if (plugin->plugin->init)
  {
    schema_table->create_table= create_schema_table;
    schema_table->old_format= make_old_format;
    schema_table->idx_field1= -1, 
    schema_table->idx_field2= -1; 

    /* Make the name available to the init() function. */
    schema_table->table_name= plugin->name.str;

    if (plugin->plugin->init(schema_table))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      plugin->data= NULL;
      my_free(schema_table);
      DBUG_RETURN(1);
    }
    
    /* Make sure the plugin name is not set inside the init() function. */
    schema_table->table_name= plugin->name.str;
  }
  DBUG_RETURN(0);
}

int finalize_schema_table(st_plugin_int *plugin)
{
  ST_SCHEMA_TABLE *schema_table= (ST_SCHEMA_TABLE *)plugin->data;
  DBUG_ENTER("finalize_schema_table");

  if (schema_table)
  {
    if (plugin->plugin->deinit)
    {
      DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
      if (plugin->plugin->deinit(NULL))
      {
        DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                               plugin->name.str));
      }
    }
    my_free(schema_table);
  }
  DBUG_RETURN(0);
}


/**
  Output trigger information (SHOW CREATE TRIGGER) to the client.

  @param thd          Thread context.
  @param triggers     table trigger to dump.

  @return Operation status
    @retval TRUE Error.
    @retval FALSE Success.
*/

static bool show_create_trigger_impl(THD *thd, Trigger *trigger)
{
  Protocol *p= thd->get_protocol();
  List<Item> fields;

  // Construct sql_mode string.

  LEX_STRING sql_mode_str;

  sql_mode_string_representation(thd, trigger->get_sql_mode(), &sql_mode_str);

  // Send header.

  fields.push_back(new Item_empty_string("Trigger", NAME_LEN));
  fields.push_back(new Item_empty_string("sql_mode", sql_mode_str.length));

  {
    /*
      NOTE: SQL statement field must be not less than 1024 in order not to
      confuse old clients.
    */

    Item_empty_string *stmt_fld=
      new Item_empty_string("SQL Original Statement",
                            max<size_t>(trigger->get_definition().length,
                                        1024));

    stmt_fld->maybe_null= TRUE;

    fields.push_back(stmt_fld);
  }

  fields.push_back(new Item_empty_string("character_set_client",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("collation_connection",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("Database Collation",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_temporal(MYSQL_TYPE_TIMESTAMP,
                                     Name_string("Created",
                                                 sizeof("created")-1),
                                     0, 0));

  if (thd->send_result_metadata(&fields,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return TRUE;

  // Resolve trigger client character set.

  const CHARSET_INFO *client_cs;

  if (resolve_charset(trigger->get_client_cs_name().str, NULL, &client_cs))
    return true;

  // Send data.

  p->start_row();

  p->store(trigger->get_trigger_name(), system_charset_info);
  p->store(sql_mode_str, system_charset_info);
  p->store(trigger->get_definition(), client_cs);
  p->store(trigger->get_client_cs_name(), system_charset_info);
  p->store(trigger->get_connection_cl_name(), system_charset_info);
  p->store(trigger->get_db_cl_name(), system_charset_info);

  if (!trigger->is_created_timestamp_null())
  {
    MYSQL_TIME timestamp;
    my_tz_SYSTEM->gmt_sec_to_TIME(&timestamp,
                                  trigger->get_created_timestamp());
    p->store(&timestamp, 2);
  }
  else
    p->store_null();

  int rc= p->end_row();

  if (!rc)
    my_eof(thd);

  return rc != 0;
}


/**
  Read TRN and TRG files to obtain base table name for the specified
  trigger name and construct TABE_LIST object for the base table.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return TABLE_LIST object corresponding to the base table.

  TODO: This function is a copy&paste from add_table_to_list() and
  sp_add_to_query_tables(). The problem is that in order to be compatible
  with Stored Programs (Prepared Statements), we should not touch thd->lex.
  The "source" functions also add created TABLE_LIST object to the
  thd->lex->query_tables.

  The plan to eliminate this copy&paste is to:

    - get rid of sp_add_to_query_tables() and use Lex::add_table_to_list().
      Only add_table_to_list() must be used to add tables from the parser
      into Lex::query_tables list.

    - do not update Lex::query_tables in add_table_to_list().
*/

static
TABLE_LIST *get_trigger_table(THD *thd, const sp_name *trg_name)
{
  char trn_path_buff[FN_REFLEN];
  LEX_CSTRING db;
  LEX_STRING tbl_name;
  TABLE_LIST *table;

  LEX_STRING trn_path=
    Trigger_loader::build_trn_path(trn_path_buff, FN_REFLEN,
                                   trg_name->m_db.str,
                                   trg_name->m_name.str);

  if (Trigger_loader::check_trn_exists(trn_path))
  {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    return NULL;
  }

  if (Trigger_loader::load_trn_file(thd, trg_name->m_name, trn_path, &tbl_name))
    return NULL;

  /* We need to reset statement table list to be PS/SP friendly. */
  if (!(table= (TABLE_LIST*) thd->alloc(sizeof(TABLE_LIST))))
    return NULL;

  db= trg_name->m_db;

  db.str= thd->strmake(db.str, db.length);
  tbl_name.str= thd->strmake(tbl_name.str, tbl_name.length);

  if (db.str == NULL || tbl_name.str == NULL)
    return NULL;

  table->init_one_table(db.str, db.length, tbl_name.str, tbl_name.length,
                        tbl_name.str, TL_IGNORE);

  return table;
}


/**
  SHOW CREATE TRIGGER high-level implementation.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return Operation status
    @retval TRUE Error.
    @retval FALSE Success.
*/

bool show_create_trigger(THD *thd, const sp_name *trg_name)
{
  TABLE_LIST *lst= get_trigger_table(thd, trg_name);
  uint num_tables; /* NOTE: unused, only to pass to open_tables(). */
  Table_trigger_dispatcher *triggers;
  bool error= true;
  Trigger *trigger;

  if (!lst)
    return true;

  if (check_table_access(thd, TRIGGER_ACL, lst, FALSE, 1, TRUE))
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "TRIGGER");
    return true;
  }

  /*
    Metadata locks taken during SHOW CREATE TRIGGER should be released when
    the statement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();

  /*
    Open the table by name in order to load Table_trigger_dispatcher object.
  */
  if (open_tables(thd, &lst, &num_tables,
                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL))
  {
    my_error(ER_TRG_CANT_OPEN_TABLE, MYF(0),
             trg_name->m_db.str,
             lst->table_name);

    goto exit;

    /* Perform closing actions and return error status. */
  }

  triggers= lst->table->triggers;

  if (!triggers)
  {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    goto exit;
  }

  trigger= triggers->find_trigger(trg_name->m_name);

  if (!trigger)
  {
    my_error(ER_TRG_CORRUPTED_FILE, MYF(0),
             trg_name->m_db.str,
             lst->table_name);

    goto exit;
  }

  error= show_create_trigger_impl(thd, trigger);

  /*
    NOTE: if show_create_trigger_impl() failed, that means we could not
    send data to the client. In this case we simply raise the error
    status and client connection will be closed.
  */

exit:
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE TRIGGER. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  return error;
}

static IS_internal_schema_access is_internal_schema_access;

void initialize_information_schema_acl()
{
  ACL_internal_schema_registry::register_schema(INFORMATION_SCHEMA_NAME,
                                                &is_internal_schema_access);
}

/*
  Convert a string in character set in column character set format
  to utf8 character set if possible, the utf8 character set string
  will later possibly be converted to character set used by client.
  Thus we attempt conversion from column character set to both
  utf8 and to character set client.

  Examples of strings that should fail conversion to utf8 are unassigned
  characters as e.g. 0x81 in cp1250 (Windows character set for for countries
  like Czech and Poland). Example of string that should fail conversion to
  character set on client (e.g. if this is latin1) is 0x2020 (daggger) in
  ucs2.

  If the conversion fails we will as a fall back convert the string to
  hex encoded format. The caller of the function can also ask for hex
  encoded format of output string unconditionally.

  SYNOPSIS
    get_cs_converted_string_value()
    thd                             Thread object
    input_str                       Input string in cs character set
    output_str                      Output string to be produced in utf8
    cs                              Character set of input string
    use_hex                         Use hex string unconditionally
 

  RETURN VALUES
    No return value
*/

static void get_cs_converted_string_value(THD *thd,
                                          String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs,
                                          bool use_hex)
{

  output_str->length(0);
  if (input_str->length() == 0)
  {
    output_str->append("''");
    return;
  }
  if (!use_hex)
  {
    String try_val;
    uint try_conv_error= 0;

    try_val.copy(input_str->ptr(), input_str->length(), cs,
                 thd->variables.character_set_client, &try_conv_error);
    if (!try_conv_error)
    {
      String val;
      uint conv_error= 0;

      val.copy(input_str->ptr(), input_str->length(), cs,
               system_charset_info, &conv_error);
      if (!conv_error)
      {
        append_unescaped(output_str, val.ptr(), val.length());
        return;
      }
    }
    /* We had a conversion error, use hex encoded string for safety */
  }
  {
    const uchar *ptr;
    size_t i, len;
    char buf[3];

    output_str->append("_");
    output_str->append(cs->csname);
    output_str->append(" ");
    output_str->append("0x");
    len= input_str->length();
    ptr= (uchar*)input_str->ptr();
    for (i= 0; i < len; i++)
    {
      uint high, low;

      high= (*ptr) >> 4;
      low= (*ptr) & 0x0F;
      buf[0]= _dig_vec_upper[high];
      buf[1]= _dig_vec_upper[low];
      buf[2]= 0;
      output_str->append((const char*)buf);
      ptr++;
    }
  }
  return;
}
