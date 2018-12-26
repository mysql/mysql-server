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

#include "table.h"

#include "my_md5.h"                      // compute_md5_hash
#include "myisam.h"                      // MI_MAX_KEY_LENGTH
#include "mysql_version.h"               // MYSQL_VERSION_ID

#include "auth_common.h"                 // acl_getroot
#include "binlog.h"                      // mysql_bin_log
#include "debug_sync.h"                  // DEBUG_SYNC
#include "item_cmpfunc.h"                // and_conds
#include "key.h"                         // find_ref_key
#include "log.h"                         // sql_print_warning
#include "opt_trace.h"                   // opt_trace_disable_if_no_security_...
#include "parse_file.h"                  // sql_parse_prepare
#include "partition_info.h"              // partition_info
#include "sql_base.h"                    // OPEN_VIEW_ONLY
#include "sql_class.h"                   // THD
#include "sql_parse.h"                   // check_stack_overrun
#include "sql_partition.h"               // mysql_unpack_partition,
                                         // get_partition_tablespace_names
#include "sql_plugin.h"                  // plugin_unlock
#include "sql_select.h"                  // actual_key_parts
#include "sql_table.h"                   // build_table_filename
#include "sql_tablespace.h"              // check_tablespace_name())
#include "sql_view.h"                    // view_type
#include "strfunc.h"                     // unhex_type2
#include "table_cache.h"                 // table_cache_manager
#include "table_trigger_dispatcher.h"    // Table_trigger_dispatcher
#include "template_utils.h"              // down_cast

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include "pfs_table_provider.h"
#include "mysql/psi/mysql_table.h"


/* INFORMATION_SCHEMA name */
LEX_STRING INFORMATION_SCHEMA_NAME= {C_STRING_WITH_LEN("information_schema")};

/* PERFORMANCE_SCHEMA name */
LEX_STRING PERFORMANCE_SCHEMA_DB_NAME= {C_STRING_WITH_LEN("performance_schema")};

/* MYSQL_SCHEMA name */
LEX_STRING MYSQL_SCHEMA_NAME= {C_STRING_WITH_LEN("mysql")};

/* GENERAL_LOG name */
LEX_STRING GENERAL_LOG_NAME= {C_STRING_WITH_LEN("general_log")};

/* SLOW_LOG name */
LEX_STRING SLOW_LOG_NAME= {C_STRING_WITH_LEN("slow_log")};

/* RLI_INFO name */
LEX_STRING RLI_INFO_NAME= {C_STRING_WITH_LEN("slave_relay_log_info")};

/* MI_INFO name */
LEX_STRING MI_INFO_NAME= {C_STRING_WITH_LEN("slave_master_info")};

/* WORKER_INFO name */
LEX_STRING WORKER_INFO_NAME= {C_STRING_WITH_LEN("slave_worker_info")};

/* GTID_EXECUTED name */
LEX_STRING GTID_EXECUTED_NAME= {C_STRING_WITH_LEN("gtid_executed")};

/* Keyword for parsing generated column functions */
LEX_STRING PARSE_GCOL_KEYWORD= {C_STRING_WITH_LEN("parse_gcol_expr")};


	/* Functions defined in this file */

void open_table_error(TABLE_SHARE *share, int error, int db_errno,
                      myf errortype, int errarg);
static int open_binary_frm(THD *thd, TABLE_SHARE *share,
                           uchar *head, File file);
static void fix_type_pointers(const char ***array, TYPELIB *point_to_type,
			      uint types, char **names);
static uint find_field(Field **fields, uchar *record, uint start, uint length);

static Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
                               const char *name,
                               Name_resolution_context *context);

inline bool is_system_table_name(const char *name, size_t length);

static ulong get_form_pos(File file, uchar *head);

/**************************************************************************
  Object_creation_ctx implementation.
**************************************************************************/

Object_creation_ctx *Object_creation_ctx::set_n_backup(THD *thd)
{
  Object_creation_ctx *backup_ctx;
  DBUG_ENTER("Object_creation_ctx::set_n_backup");

  backup_ctx= create_backup_ctx(thd);
  change_env(thd);

  DBUG_RETURN(backup_ctx);
}

void Object_creation_ctx::restore_env(THD *thd, Object_creation_ctx *backup_ctx)
{
  if (!backup_ctx)
    return;

  backup_ctx->change_env(thd);

  delete backup_ctx;
}

/**************************************************************************
  Default_object_creation_ctx implementation.
**************************************************************************/

Default_object_creation_ctx::Default_object_creation_ctx(THD *thd)
  : m_client_cs(thd->variables.character_set_client),
    m_connection_cl(thd->variables.collation_connection)
{ }

Default_object_creation_ctx::Default_object_creation_ctx(
  const CHARSET_INFO *client_cs, const CHARSET_INFO *connection_cl)
  : m_client_cs(client_cs),
    m_connection_cl(connection_cl)
{ }

Object_creation_ctx *
Default_object_creation_ctx::create_backup_ctx(THD *thd) const
{
  return new Default_object_creation_ctx(thd);
}

void Default_object_creation_ctx::change_env(THD *thd) const
{
  thd->variables.character_set_client= m_client_cs;
  thd->variables.collation_connection= m_connection_cl;

  thd->update_charset();
}

/**************************************************************************
  View_creation_ctx implementation.
**************************************************************************/

View_creation_ctx *View_creation_ctx::create(THD *thd)
{
  View_creation_ctx *ctx= new (thd->mem_root) View_creation_ctx(thd);

  return ctx;
}

/*************************************************************************/

View_creation_ctx * View_creation_ctx::create(THD *thd,
                                              TABLE_LIST *view)
{
  View_creation_ctx *ctx= new (thd->mem_root) View_creation_ctx(thd);

  /* Throw a warning if there is NULL cs name. */

  if (!view->view_client_cs_name.str ||
      !view->view_connection_cl_name.str)
  {
    push_warning_printf(thd, Sql_condition::SL_NOTE,
                        ER_VIEW_NO_CREATION_CTX,
                        ER(ER_VIEW_NO_CREATION_CTX),
                        view->db,
                        view->table_name);

    ctx->m_client_cs= system_charset_info;
    ctx->m_connection_cl= system_charset_info;

    return ctx;
  }

  /* Resolve cs names. Throw a warning if there is unknown cs name. */

  bool invalid_creation_ctx;

  invalid_creation_ctx= resolve_charset(view->view_client_cs_name.str,
                                        system_charset_info,
                                        &ctx->m_client_cs);

  invalid_creation_ctx= resolve_collation(view->view_connection_cl_name.str,
                                          system_charset_info,
                                          &ctx->m_connection_cl) ||
                        invalid_creation_ctx;

  if (invalid_creation_ctx)
  {
    sql_print_warning("View '%s'.'%s': there is unknown charset/collation "
                      "names (client: '%s'; connection: '%s').",
                      view->db,
                      view->table_name,
                      view->view_client_cs_name.str,
                      view->view_connection_cl_name.str);

    push_warning_printf(thd, Sql_condition::SL_NOTE,
                        ER_VIEW_INVALID_CREATION_CTX,
                        ER(ER_VIEW_INVALID_CREATION_CTX),
                        view->db,
                        view->table_name);
  }

  return ctx;
}

/*************************************************************************/

GRANT_INFO::GRANT_INFO()
{
  grant_table= 0;
  version= 0;
  privilege= NO_ACCESS;
#ifndef DBUG_OFF
  want_privilege= 0;
#endif
}


/* Get column name from column hash */

static uchar *get_field_name(Field **buff, size_t *length,
                             my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= strlen((*buff)->field_name);
  return (uchar*) (*buff)->field_name;
}


/*
  Returns pointer to '.frm' extension of the file name.

  SYNOPSIS
    fn_rext()
    name       file name

  DESCRIPTION
    Checks file name part starting with the rightmost '.' character,
    and returns it if it is equal to '.frm'. 

  TODO
    It is a good idea to get rid of this function modifying the code
    to garantee that the functions presently calling fn_rext() always
    get arguments in the same format: either with '.frm' or without '.frm'.

  RETURN VALUES
    Pointer to the '.frm' extension. If there is no extension,
    or extension is not '.frm', pointer at the end of file name.
*/

char *fn_rext(char *name)
{
  char *res= strrchr(name, '.');
  if (res && !strcmp(res, reg_ext))
    return res;
  return name + strlen(name);
}


static TABLE_CATEGORY get_table_category(const LEX_STRING &db,
                                         const LEX_STRING &name)
{
  DBUG_ASSERT(db.str != NULL);
  DBUG_ASSERT(name.str != NULL);

  if (is_infoschema_db(db.str, db.length))
    return TABLE_CATEGORY_INFORMATION;

  if (is_perfschema_db(db.str, db.length))
    return TABLE_CATEGORY_PERFORMANCE;

  if ((db.length == MYSQL_SCHEMA_NAME.length) &&
      (my_strcasecmp(system_charset_info,
                     MYSQL_SCHEMA_NAME.str,
                     db.str) == 0))
  {
    if (is_system_table_name(name.str, name.length))
      return TABLE_CATEGORY_SYSTEM;

    if ((name.length == GENERAL_LOG_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                       GENERAL_LOG_NAME.str,
                       name.str) == 0))
      return TABLE_CATEGORY_LOG;

    if ((name.length == SLOW_LOG_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                       SLOW_LOG_NAME.str,
                       name.str) == 0))
      return TABLE_CATEGORY_LOG;

    if ((name.length == RLI_INFO_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                      RLI_INFO_NAME.str,
                      name.str) == 0))
      return TABLE_CATEGORY_RPL_INFO;

    if ((name.length == MI_INFO_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                      MI_INFO_NAME.str,
                      name.str) == 0))
      return TABLE_CATEGORY_RPL_INFO;

    if ((name.length == WORKER_INFO_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                      WORKER_INFO_NAME.str,
                      name.str) == 0))
      return TABLE_CATEGORY_RPL_INFO;

    if ((name.length == GTID_EXECUTED_NAME.length) &&
        (my_strcasecmp(system_charset_info,
                       GTID_EXECUTED_NAME.str,
                       name.str) == 0))
      return TABLE_CATEGORY_GTID;

  }

  return TABLE_CATEGORY_USER;
}


/**
  Allocate and setup a TABLE_SHARE structure

  @param table_list  structure from which database and table 
                     name can be retrieved
  @param key         table cache key (db \0 table_name \0...)
  @param key_length  length of the key

  @return            pointer to allocated table share
    @retval NULL     error (out of memory, too long path name)
*/

TABLE_SHARE *alloc_table_share(TABLE_LIST *table_list, const char *key,
                               size_t key_length)
{
  MEM_ROOT mem_root;
  TABLE_SHARE *share= NULL;
  char *key_buff, *path_buff;
  char path[FN_REFLEN + 1];
  size_t path_length;
  Table_cache_element **cache_element_array;
  bool was_truncated= false;
  DBUG_ENTER("alloc_table_share");
  DBUG_PRINT("enter", ("table: '%s'.'%s'",
                       table_list->db, table_list->table_name));

  /*
    There are FN_REFLEN - reg_ext_length bytes available for the 
    file path and the trailing '\0', which may be padded to the right 
    of the length indicated by the length parameter. The returned 
    path length does not include the trailing '\0'.
  */
  path_length= build_table_filename(path, sizeof(path) - 1 - reg_ext_length,
                                    table_list->db,
                                    table_list->table_name, "", 0,
                                    &was_truncated);

  /*
    The path now misses extension, but includes '\0'. Unless it was
    truncated, everything should be ok. 
  */
  if (was_truncated)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(path) - 1, path);
    DBUG_RETURN(NULL);
  }

  init_sql_alloc(key_memory_table_share, &mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (multi_alloc_root(&mem_root,
                       &share, sizeof(*share),
                       &key_buff, key_length,
                       &path_buff, path_length + 1,
                       &cache_element_array,
                       table_cache_instances * sizeof(*cache_element_array),
                       NULL))
  {
    new (share) TABLE_SHARE;

    share->set_table_cache_key(key_buff, key, key_length);

    share->path.str= path_buff;
    share->path.length= path_length;
    my_stpcpy(share->path.str, path);
    share->normalized_path.str=    share->path.str;
    share->normalized_path.length= path_length;

    share->version=       refresh_version;

    /*
      Since alloc_table_share() can be called without any locking (for
      example, ha_create_table... functions), we do not assign a table
      map id here.  Instead we assign a value that is not used
      elsewhere, and then assign a table map id inside open_table()
      under the protection of the LOCK_open mutex.
    */
    share->table_map_id= ~0ULL;
    share->cached_row_logging_check= -1;

    share->m_flush_tickets.empty();

    memset(cache_element_array, 0,
           table_cache_instances * sizeof(*cache_element_array));
    share->cache_element= cache_element_array;

    memcpy((char*) &share->mem_root, (char*) &mem_root, sizeof(mem_root));
    mysql_mutex_init(key_TABLE_SHARE_LOCK_ha_data,
                     &share->LOCK_ha_data, MY_MUTEX_INIT_FAST);
  }
  DBUG_RETURN(share);
}


/*
  Initialize share for temporary tables

  SYNOPSIS
    init_tmp_table_share()
    thd         thread handle
    share	Share to fill
    key		Table_cache_key, as generated from create_table_def_key.
		must start with db name.    
    key_length	Length of key
    table_name	Table name
    path	Path to file (possible in lower case) without .frm

  NOTES
    This is different from alloc_table_share() because temporary tables
    don't have to be shared between threads or put into the table def
    cache, so we can do some things notable simpler and faster

    If table is not put in thd->temporary_tables (happens only when
    one uses OPEN TEMPORARY) then one can specify 'db' as key and
    use key_length= 0 as neither table_cache_key or key_length will be used).
*/

void init_tmp_table_share(THD *thd, TABLE_SHARE *share, const char *key,
                          size_t key_length, const char *table_name,
                          const char *path)
{
  DBUG_ENTER("init_tmp_table_share");
  DBUG_PRINT("enter", ("table: '%s'.'%s'", key, table_name));

  new (share) TABLE_SHARE;
  init_sql_alloc(key_memory_table_share,
                 &share->mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  share->table_category=         TABLE_CATEGORY_TEMPORARY;
  share->tmp_table=              INTERNAL_TMP_TABLE;
  share->db.str=                 (char*) key;
  share->db.length=		 strlen(key);
  share->table_cache_key.str=    (char*) key;
  share->table_cache_key.length= key_length;
  share->table_name.str=         (char*) table_name;
  share->table_name.length=      strlen(table_name);
  share->path.str=               (char*) path;
  share->normalized_path.str=    (char*) path;
  share->path.length= share->normalized_path.length= strlen(path);
  share->frm_version= 		 FRM_VER_TRUE_VARCHAR;

  share->cached_row_logging_check= -1;

  /*
    table_map_id is also used for MERGE tables to suppress repeated
    compatibility checks.
  */
  share->table_map_id= (ulonglong) thd->query_id;

  share->m_flush_tickets.empty();

  DBUG_VOID_RETURN;
}


/**
  Release resources (plugins) used by the share and free its memory.
  TABLE_SHARE is self-contained -- it's stored in its own MEM_ROOT.
  Free this MEM_ROOT.
*/

void TABLE_SHARE::destroy()
{
  uint idx;
  KEY *info_it;

  DBUG_ENTER("TABLE_SHARE::destroy");
  DBUG_PRINT("info", ("db: %s table: %s", db.str, table_name.str));
  if (ha_share)
  {
    delete ha_share;
    ha_share= NULL;
  }
  /* The mutex is initialized only for shares that are part of the TDC */
  if (tmp_table == NO_TMP_TABLE)
    mysql_mutex_destroy(&LOCK_ha_data);
  my_hash_free(&name_hash);

  plugin_unlock(NULL, db_plugin);
  db_plugin= NULL;

  /* Release fulltext parsers */
  info_it= key_info;
  for (idx= keys; idx; idx--, info_it++)
  {
    if (info_it->flags & HA_USES_PARSER)
    {
      plugin_unlock(NULL, info_it->parser);
      info_it->flags= 0;
    }
  }

#ifdef HAVE_PSI_TABLE_INTERFACE
  PSI_TABLE_CALL(release_table_share)(m_psi);
#endif

  /*
    Make a copy since the share is allocated in its own root,
    and free_root() updates its argument after freeing the memory.
  */
  MEM_ROOT own_root= mem_root;
  free_root(&own_root, MYF(0));
  DBUG_VOID_RETURN;
}

/*
  Free table share and memory used by it

  SYNOPSIS
    free_table_share()
    share		Table share
*/

void free_table_share(TABLE_SHARE *share)
{
  DBUG_ENTER("free_table_share");
  DBUG_PRINT("enter", ("table: %s.%s", share->db.str, share->table_name.str));
  DBUG_ASSERT(share->ref_count == 0);

  if (share->m_flush_tickets.is_empty())
  {
    /*
      No threads are waiting for this share to be flushed (the
      share is not old, is for a temporary table, or just nobody
      happens to be waiting for it). Destroy it.
    */
    share->destroy();
  }
  else
  {
    Wait_for_flush_list::Iterator it(share->m_flush_tickets);
    Wait_for_flush *ticket;
    /*
      We're about to iterate over a list that is used
      concurrently. Make sure this never happens without a lock.
    */
    mysql_mutex_assert_owner(&LOCK_open);

    while ((ticket= it++))
      (void) ticket->get_ctx()->m_wait.set_status(MDL_wait::GRANTED);
    /*
      If there are threads waiting for this share to be flushed,
      the last one to receive the notification will destroy the
      share. At this point the share is removed from the table
      definition cache, so is OK to proceed here without waiting
      for this thread to do the work.
    */
  }
  DBUG_VOID_RETURN;
}


/**
  Return TRUE if a table name matches one of the system table names.
  Currently these are:

  help_category, help_keyword, help_relation, help_topic,
  proc, event
  time_zone, time_zone_leap_second, time_zone_name, time_zone_transition,
  time_zone_transition_type

  This function trades accuracy for speed, so may return false
  positives. Presumably mysql.* database is for internal purposes only
  and should not contain user tables.
*/

inline bool is_system_table_name(const char *name, size_t length)
{
  CHARSET_INFO *ci= system_charset_info;

  return (
           /* mysql.proc table */
           (length == 4 &&
             my_tolower(ci, name[0]) == 'p' && 
             my_tolower(ci, name[1]) == 'r' &&
             my_tolower(ci, name[2]) == 'o' &&
             my_tolower(ci, name[3]) == 'c') ||

           (length > 4 &&
             (
               /* one of mysql.help* tables */
               (my_tolower(ci, name[0]) == 'h' &&
                 my_tolower(ci, name[1]) == 'e' &&
                 my_tolower(ci, name[2]) == 'l' &&
                 my_tolower(ci, name[3]) == 'p') ||

               /* one of mysql.time_zone* tables */
               (my_tolower(ci, name[0]) == 't' &&
                 my_tolower(ci, name[1]) == 'i' &&
                 my_tolower(ci, name[2]) == 'm' &&
                 my_tolower(ci, name[3]) == 'e') ||

               /* mysql.event table */
               (my_tolower(ci, name[0]) == 'e' &&
                 my_tolower(ci, name[1]) == 'v' &&
                 my_tolower(ci, name[2]) == 'e' &&
                 my_tolower(ci, name[3]) == 'n' &&
                 my_tolower(ci, name[4]) == 't')
             )
           )
         );
}


/**
  Check if a string contains path elements
*/  

static inline bool has_disabled_path_chars(const char *str)
{
  for (; *str; str++)
    switch (*str)
    {
      case FN_EXTCHAR:
      case '/':
      case '\\':
      case '~':
      case '@':
        return TRUE;
    }
  return FALSE;
}


/*
  Read table definition from a binary / text based .frm file
  
  SYNOPSIS
  open_table_def()
  thd		Thread handler
  share		Fill this with table definition
  db_flags	Bit mask of the following flags: OPEN_VIEW

  NOTES
    This function is called when the table definition is not cached in
    table_def_cache
    The data is returned in 'share', which is alloced by
    alloc_table_share().. The code assumes that share is initialized.

  RETURN VALUES
   0	ok
   1	Error (see open_table_error)
   2    Error (see open_table_error)
   3    Wrong data in .frm file
   4    Error (see open_table_error)
   5    Error (see open_table_error: charset unavailable)
   6    Unknown .frm version
   8    Error while reading view definition from .FRM file.
   9    Wrong type in view's .frm file.
*/

int open_table_def(THD *thd, TABLE_SHARE *share, uint db_flags)
{
  int error, table_type;
  bool error_given;
  File file;
  uchar head[64];
  char	path[FN_REFLEN + 1];
  MEM_ROOT **root_ptr, *old_root;
  DBUG_ENTER("open_table_def");
  DBUG_PRINT("enter", ("table: '%s'.'%s'  path: '%s'", share->db.str,
                       share->table_name.str, share->normalized_path.str));

  error= 1;
  error_given= 0;

  strxnmov(path, sizeof(path) - 1, share->normalized_path.str, reg_ext, NullS);
  if ((file= mysql_file_open(key_file_frm,
                             path, O_RDONLY | O_SHARE, MYF(0))) < 0)
  {
    /*
      We don't try to open 5.0 unencoded name, if
      - non-encoded name contains '@' signs, 
        because '@' can be misinterpreted.
        It is not clear if '@' is escape character in 5.1,
        or a normal character in 5.0.
        
      - non-encoded db or table name contain "#mysql50#" prefix.
        This kind of tables must have been opened only by the
        mysql_file_open() above.
    */
    if (has_disabled_path_chars(share->table_name.str) ||
        has_disabled_path_chars(share->db.str) ||
        !strncmp(share->db.str, MYSQL50_TABLE_NAME_PREFIX,
                 MYSQL50_TABLE_NAME_PREFIX_LENGTH) ||
        !strncmp(share->table_name.str, MYSQL50_TABLE_NAME_PREFIX,
                 MYSQL50_TABLE_NAME_PREFIX_LENGTH))
      goto err_not_open;

    /*
      Trying unencoded 5.0 name for temporary tables does not
      make sense since such tables are not persistent.
    */
    if (share->tmp_table)
      goto err_not_open;

    /* Try unencoded 5.0 name */
    size_t length;
    strxnmov(path, sizeof(path)-1,
             mysql_data_home, "/", share->db.str, "/",
             share->table_name.str, reg_ext, NullS);
    length= unpack_filename(path, path) - reg_ext_length;
    /*
      The following is a safety test and should never fail
      as the old file name should never be longer than the new one.
    */
    DBUG_ASSERT(length <= share->normalized_path.length);
    /*
      If the old and the new names have the same length,
      then table name does not have tricky characters,
      so no need to check the old file name.
    */
    if (length == share->normalized_path.length ||
        ((file= mysql_file_open(key_file_frm,
                                path, O_RDONLY | O_SHARE, MYF(0))) < 0))
      goto err_not_open;

    /* Unencoded 5.0 table name found */
    path[length]= '\0'; // Remove .frm extension
    my_stpcpy(share->normalized_path.str, path);
    share->normalized_path.length= length;
  }

  error= 4;
  if (mysql_file_read(file, head, 64, MYF(MY_NABP)))
    goto err;

  if (head[0] == (uchar) 254 && head[1] == 1)
  {
    if (head[2] == FRM_VER || head[2] == FRM_VER+1 ||
        (head[2] >= FRM_VER+3 && head[2] <= FRM_VER+4))
    {
      /* Open view only */
      if (db_flags & OPEN_VIEW_ONLY)
      {
        error_given= 1;
        goto err;
      }
      table_type= 1;
    }
    else
    {
      error= 6;                                 // Unkown .frm version
      goto err;
    }
  }
  else if (memcmp(head, STRING_WITH_LEN("TYPE=")) == 0)
  {
    error= 5;
    if (memcmp(head+5, "VIEW", 4) == 0)
    {
      share->is_view= 1;
      if (db_flags & OPEN_VIEW)
        table_type= 2;
      else
        goto err;
    }
    else
      goto err;
  }
  else
    goto err;

  if (table_type == 1)
  {
    root_ptr= my_thread_get_THR_MALLOC();
    old_root= *root_ptr;
    *root_ptr= &share->mem_root;
    error= open_binary_frm(thd, share, head, file);
    *root_ptr= old_root;
    error_given= 1;
  }
  else if (table_type == 2)
  {
    LEX_STRING pathstr= { path, strlen(path) };

    /*
      Create view file parser and hold it in TABLE_SHARE member
      view_def.
      */ 
    share->view_def= sql_parse_prepare(&pathstr, &share->mem_root, true);
    if (!share->view_def)
      error= 8;
    else if (!is_equal(&view_type, share->view_def->type()))
      error= 9;
    else
      error= 0;
  }

  share->table_category= get_table_category(share->db, share->table_name);

  if (!error)
    thd->status_var.opened_shares++;

err:
  mysql_file_close(file, MYF(MY_WME));

err_not_open:
  if (error && !error_given)
  {
    share->error= error;
    open_table_error(share, error, (share->open_errno= my_errno()), 0);
  }

  DBUG_RETURN(error);
}


/**
  Initialize key_part_flag from source field.
*/

void KEY_PART_INFO::init_flags()
{
  DBUG_ASSERT(field);
  if (field->type() == MYSQL_TYPE_BLOB ||
      field->type() == MYSQL_TYPE_GEOMETRY)
    key_part_flag|= HA_BLOB_PART;
  else if (field->real_type() == MYSQL_TYPE_VARCHAR)
    key_part_flag|= HA_VAR_LENGTH_PART;
  else if (field->type() == MYSQL_TYPE_BIT)
    key_part_flag|= HA_BIT_PART;
}


/**
  Initialize KEY_PART_INFO from the given field.

  @param fld The field to initialize keypart from
*/

void KEY_PART_INFO::init_from_field(Field *fld)
{
  field= fld;
  fieldnr= field->field_index + 1;
  null_bit= field->null_bit;
  null_offset= field->null_offset();
  offset= field->offset(field->table->record[0]);
  length= (uint16) field->key_length();
  store_length= length;
  key_part_flag= 0;

  if (field->real_maybe_null())
    store_length+= HA_KEY_NULL_LENGTH;
  if (field->type() == MYSQL_TYPE_BLOB ||
      field->real_type() == MYSQL_TYPE_VARCHAR ||
      field->type() == MYSQL_TYPE_GEOMETRY)
  {
    store_length+= HA_KEY_BLOB_LENGTH;
  }
  init_flags();

  type=  (uint8) field->key_type();
  key_type =
    ((ha_base_keytype) type == HA_KEYTYPE_TEXT ||
     (ha_base_keytype) type == HA_KEYTYPE_VARTEXT1 ||
     (ha_base_keytype) type == HA_KEYTYPE_VARTEXT2) ?
    0 : FIELDFLAG_BINARY;
}


/**
  Setup key-related fields of Field object for given key and key part.

  @param[in]     share                    Pointer to TABLE_SHARE
  @param[in]     handler_file             Pointer to handler
  @param[in]     primary_key_n            Primary key number
  @param[in]     keyinfo                  Pointer to processed key
  @param[in]     key_n                    Processed key number
  @param[in]     key_part_n               Processed key part number
  @param[in,out] usable_parts             Pointer to usable_parts variable
  @param[in]     part_of_key_not_extended Set when column is part of the Key
                                          and not appended by the storage
                                          engine from primary key columns.
*/

static void setup_key_part_field(TABLE_SHARE *share, handler *handler_file,
                                 uint primary_key_n, KEY *keyinfo, uint key_n,
                                 uint key_part_n, uint *usable_parts,
                                 bool part_of_key_not_extended)
{
  KEY_PART_INFO *key_part= &keyinfo->key_part[key_part_n];
  Field *field= key_part->field;

  /* Flag field as unique if it is the only keypart in a unique index */
  if (key_part_n == 0 && key_n != primary_key_n)
    field->flags |= (((keyinfo->flags & HA_NOSAME) &&
                      (keyinfo->user_defined_key_parts == 1)) ?
                     UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
  if (key_part_n == 0)
    field->key_start.set_bit(key_n);
  field->m_indexed= true;
  if (field->key_length() == key_part->length &&
      !(field->flags & BLOB_FLAG))
  {
    if (handler_file->index_flags(key_n, key_part_n, 0) & HA_KEYREAD_ONLY)
    {
      share->keys_for_keyread.set_bit(key_n);
      field->part_of_key.set_bit(key_n);
      if (part_of_key_not_extended)
        field->part_of_key_not_extended.set_bit(key_n);
    }
    if (handler_file->index_flags(key_n, key_part_n, 1) & HA_READ_ORDER)
      field->part_of_sortkey.set_bit(key_n);
  }

  if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
      *usable_parts == key_part_n)
    (*usable_parts)++;			// For FILESORT
}


/**
  Generate extended secondary keys by adding primary key parts to the
  existing secondary key. A primary key part is added if such part doesn't
  present in the secondary key or the part in the secondary key is a
  prefix of the key field. Key parts are added till:
  .) all parts were added
  .) number of key parts became bigger that MAX_REF_PARTS
  .) total key length became longer than MAX_REF_LENGTH
  depending on what occurs first first.
  Unlike existing secondary key parts which are initialized at
  open_binary_frm(), newly added ones are initialized here by copying
  KEY_PART_INFO structure from primary key part and calling
  setup_key_part_field().

  Function updates sk->actual/unused_key_parts and sk->actual_flags.

  @param[in]     sk            Secondary key
  @param[in]     sk_n          Secondary key number
  @param[in]     pk            Primary key
  @param[in]     pk_n          Primary key number
  @param[in]     share         Pointer to TABLE_SHARE
  @param[in]     handler       Pointer to handler
  @param[in,out] usable_parts  Pointer to usable_parts variable

  @retval                      Number of added key parts
*/

static uint add_pk_parts_to_sk(KEY *sk, uint sk_n, KEY *pk, uint pk_n,
                               TABLE_SHARE *share, handler *handler_file,
                               uint *usable_parts)
{
  uint max_key_length= sk->key_length;
  bool is_unique_key= false;
  KEY_PART_INFO *current_key_part= &sk->key_part[sk->user_defined_key_parts];

  /* 
     For each keypart in the primary key: check if the keypart is
     already part of the secondary key and add it if not.
  */
  for (uint pk_part= 0; pk_part < pk->user_defined_key_parts; pk_part++)
  {
    KEY_PART_INFO *pk_key_part= &pk->key_part[pk_part];
    /* MySQL does not supports more key parts than MAX_REF_LENGTH */
    if (sk->actual_key_parts >= MAX_REF_PARTS)
      goto end;

    bool pk_field_is_in_sk= false;
    for (uint j= 0; j < sk->user_defined_key_parts; j++)
    {
      if (sk->key_part[j].fieldnr == pk_key_part->fieldnr &&
          share->field[pk_key_part->fieldnr - 1]->key_length() ==
          sk->key_part[j].length)
      {
        pk_field_is_in_sk= true;
        break;
      }
    }

    /* Add PK field to secondary key if it's not already  part of the key. */
    if (!pk_field_is_in_sk)
    {
      /* MySQL does not supports keys longer than MAX_KEY_LENGTH */
      if (max_key_length + pk_key_part->length > MAX_KEY_LENGTH)
        goto end;

      *current_key_part= *pk_key_part;
      setup_key_part_field(share, handler_file, pk_n, sk, sk_n,
                           sk->actual_key_parts, usable_parts, false);
      sk->actual_key_parts++;
      sk->unused_key_parts--;
      sk->rec_per_key[sk->actual_key_parts - 1]= 0;
      sk->set_records_per_key(sk->actual_key_parts - 1, REC_PER_KEY_UNKNOWN);
      current_key_part++;
      max_key_length+= pk_key_part->length;
      /*
        Secondary key will be unique if the key  does not exceed
        key length limitation and key parts limitation.
      */
      is_unique_key= true;
    }
  }
  if (is_unique_key)
    sk->actual_flags|= HA_NOSAME;

end:
  return (sk->actual_key_parts - sk->user_defined_key_parts);
}


/**
  After retrieving the tablespace name, the tablespace name is validated.
  If the name is invalid, it is ignored. The function used to validate
  the name, 'check_tablespace_name()', emits errors. In the context of
  reading .FRM files, the errors must be ignored. This error handler makes
  sure this is done.
*/

class Tablespace_name_error_handler : public Internal_error_handler
{
public:
  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level *level,
                        const char* msg)
  {
    if (sql_errno == ER_WRONG_TABLESPACE_NAME ||
        sql_errno == ER_TOO_LONG_IDENT)
      return true;
    return false;
  }
};


bool get_table_and_parts_tablespace_names(
       THD *thd,
       TABLE_LIST *table,
       Tablespace_hash_set *tablespace_set)
{
  // Prepare the path to the .FRM file and open the file
  char path[FN_REFLEN + 1];           //< Path to .FRM file
  build_table_filename(path, sizeof(path) - 1,
                       table->db, table->table_name, reg_ext, 0);

  // First, we open the file, and return upon failure. No need to close
  // the file in this case.
  File file= mysql_file_open(key_file_frm, path, O_RDONLY | O_SHARE, MYF(0));
  if (file < 0)
    return false;

  // Next, we read the header and do some basic verification of the
  // header fields.
  uchar head[64];
  if (mysql_file_read(file, head, sizeof(head), MYF(MY_NABP)) ||
      head[0] != (uchar) 254 || head[1] != 1                  ||
      !(head[2] == FRM_VER || head[2] == FRM_VER + 1 ||
        (head[2] >= FRM_VER + 3 && head[2] <= FRM_VER + 4)))
  {
    // Upon failure, return NULL, but here, we have to close the file first.
    mysql_file_close(file, MYF(MY_WME));
    return false;
  }

  // For mysql versions before 50120, NDB stored the tablespace names only
  // in the NDB dictionary. Thus, we have to get the tablespace name from
  // the engine in this case.

  // Get the relevant db type value.
  enum legacy_db_type db_type= static_cast<enum legacy_db_type>(*(head + 3));

  // Tablespace name to be returned.
  const char *tablespace_name= NULL;

  if (db_type == DB_TYPE_NDBCLUSTER &&            // Cluster table.
      uint4korr(head + 51) < 50120)               // Version before 50120.
  {
    // Lock the plugin, and get the handlerton.
    plugin_ref se_plugin= ha_lock_engine(NULL,
                                         ha_checktype(thd,
                                                      db_type, false, false));
    handlerton *se_hton= plugin_data<handlerton*>(se_plugin);
    DBUG_ASSERT(se_hton);

    // Now, assemble the parameters:
    // 1. The tablespace name (to be retrieved).
    LEX_CSTRING ts_name= {NULL, 0};

    // 2. The schema name for the table.
    LEX_CSTRING schema_name= {table->db, table->db_length};

    // 3. The table name.
    LEX_CSTRING table_name= {table->table_name, table->table_name_length};

    // If the handlerton supports the required function, invoke it.
    if (se_hton->get_tablespace &&
        !se_hton->get_tablespace(thd, schema_name, table_name, &ts_name))
    {
      Tablespace_name_error_handler error_handler;
      thd->push_internal_handler(&error_handler);
      // If an empty or valid tablespace name, assign the name to the
      // output parameter. The string is allocated in THD::mem_root,
      // so it is safe to return it.
      if (ts_name.length == 0 ||
          check_tablespace_name(ts_name.str) == IDENT_NAME_OK)
        tablespace_name= ts_name.str;
      thd->pop_internal_handler();
    }
    plugin_unlock(NULL, se_plugin);

    if (tablespace_name &&
        strlen(tablespace_name) &&
        tablespace_set->insert(const_cast<char*>(tablespace_name)))
    {
      mysql_file_close(file, MYF(MY_WME));
      return true;
    }

    // Proceed to read tablespace names used by // partitions.
    // Reading them from partition_info_str string in .FRM
  }

  // For other engines, and for cluster tables with version >= 50120, we
  // continue by checking that we have an extra data segment and a proper
  // form position.
  const ulong pos= get_form_pos(file, head);   //< Position of form info
  const uint n_length= uint4korr(head + 55);   //< Length of extra segment
  if (n_length == 0 || pos == 0)
  {
    // We close the file and return success, as we no form info
    // or extra segment.
    mysql_file_close(file, MYF(MY_WME));
    return false;
  }

  // Now, we are done with the basic verification. The outline of the
  // processing below is as follows:
  //
  // 1. Scan the key information buffer to determine how many keys
  //    that will have an associated parser name represented in the
  //    extra segment (see below).
  // 2. Read the form information, allocate a buffer for the extra
  //    data segment and read it into the buffer.
  // 3. Get the length of various elements and advance the reading
  //    position accordingly.
  // 4. Loop over the full text key fields that have parser names
  //    stored in the extra segment.
  // 5. Finally, read the tablespace name from the format section.

  // Read the number of keys. Needed to advance read position correctly
  const uint key_info_length= uint2korr(head + 28);
  uint n_keys= 0;
  uchar *disk_buff= NULL;
  mysql_file_seek(file, uint2korr(head + 6), MY_SEEK_SET, MYF(0));
  if (!read_string(file, &disk_buff, key_info_length))
  {
    if (disk_buff[0] & 0x80)
      n_keys= (disk_buff[1] << 7) | (disk_buff[0] & 0x7f);
    else
      n_keys= disk_buff[0];
  }

  // Get the FRM version, needed to interpret key fields correctly
  const uint new_frm_ver= (head[2] - FRM_VER);

  // Number of key fields with parser name in the extra segment, this is
  // the relevant key information in this context (see below).
  uint n_keys_parser= 0;

  // Position to read from
  uchar *strpos= disk_buff + 6;

  // Find which keys have a parser name in the extra segment. Loop over
  // all keys, get the flags, and interpret them according to the .FRM
  // version present
  for (uint i= 0; i < n_keys; i++)
  {
    uint user_defined_key_parts= 0;
    // First, get the key information. Here, we only care about the
    // flags (needed to increment n_keys_parser) and the number of
    // user defined key parts (needed to advance read position).
    if (new_frm_ver >= 3)
    {
      if (HA_USES_PARSER & uint2korr(strpos))
        n_keys_parser++;
      user_defined_key_parts= strpos[4];
      strpos+= 8;
    }
    else
    {
      if (HA_USES_PARSER & strpos[0])
        n_keys_parser++;
      user_defined_key_parts= strpos[3];
      strpos+= 4;
    }

    // Advance read position correctly
    if (new_frm_ver >= 1)
      strpos+= 9 * user_defined_key_parts;
    else
      strpos+= 7 * user_defined_key_parts;
  }

  // Read the form information, allocate and read the extra segment.
  bool error= true;
  mysql_file_seek(file, pos, MY_SEEK_SET,MYF(0));
  uchar forminfo[288];
  uchar *extra_segment_buff= static_cast<uchar*>(
          my_malloc(key_memory_frm_extra_segment_buff,
                    n_length, MYF(MY_WME)));
  const uint reclength= uint2korr(head + 16);
  const uint record_offset= uint2korr(head + 6) +
          ((uint2korr(head + 14) == 0xffff ?
            uint4korr(head + 47) : uint2korr(head + 14)));
  if (!mysql_file_read(file, forminfo, sizeof(forminfo), MYF(MY_NABP)) &&
      extra_segment_buff &&
      !mysql_file_pread(file, extra_segment_buff, n_length,
                        record_offset + reclength, MYF(MY_NABP)))
  {
    uchar *next_chunk= extra_segment_buff;                //< Read pos
    const uchar *buff_end= extra_segment_buff + n_length; //< Buffer end

    next_chunk+= uint2korr(next_chunk) + 2;   // Connect string
    if (next_chunk + 2 < buff_end)
      next_chunk+= uint2korr(next_chunk) + 2; // DB type
    if (next_chunk + 5 < buff_end) // Partitioning
    {
      uint32 partition_info_str_len = uint4korr(next_chunk);
      const char *partition_info_str= NULL;

      if (partition_info_str_len)
      {
        if (!(partition_info_str= (const char*)
              memdup_root(thd->mem_root, next_chunk + 4,
                          partition_info_str_len + 1)))
        {
          goto err;
        }

        // Fill tablespace names used by partition into tablespace_set.
        if (get_partition_tablespace_names(
              thd, partition_info_str, partition_info_str_len, tablespace_set))
        {
          goto err;
        }
      }
      next_chunk+= 5 + partition_info_str_len;
    }
    if (uint4korr(head + 51) >= 50110 && next_chunk < buff_end)
      next_chunk++;                           // Auto_partitioned

    // Read parser names for full text keys (this is why we needed to
    // get the key information above)
    for (uint i= 0; i < n_keys_parser; i++)
      if (next_chunk < buff_end)
        next_chunk+= strlen(reinterpret_cast<char*>(next_chunk)) + 1;

    if (forminfo[46] == static_cast<uchar>(255) &&
        (next_chunk + 2 < buff_end))
      next_chunk+= 2 + uint2korr(next_chunk); // Long table comment

    // At last we got to the point where the tablespace name is located
    const uint format_section_header_size= 8;
    if (next_chunk + format_section_header_size < buff_end)
    {
      const uint format_section_length= uint2korr(next_chunk);
      if (next_chunk + format_section_length <= buff_end)
      {
        tablespace_name= thd->mem_strdup((char*)next_chunk +
                format_section_header_size);
        if (strlen(tablespace_name) > 0)
        {
          Tablespace_name_error_handler error_handler;
          thd->push_internal_handler(&error_handler);
          if (check_tablespace_name(tablespace_name) != IDENT_NAME_OK)
            tablespace_name= NULL; // Allocated memory is implicitly freed.
          thd->pop_internal_handler();
        }
      }
    }
  }

  // Fill tablespace name used by table, if present.
  if (tablespace_name &&
      strlen(tablespace_name) &&
      tablespace_set->insert(const_cast<char*>(tablespace_name)))
    error= true;
  else
    error= false;

err:
  // Free the dynamically allocated buffers and close the .FRM file
  my_free(extra_segment_buff);
  my_free(disk_buff);
  mysql_file_close(file, MYF(MY_WME));

  return error;
}


/**
  Auxiliary function which creates Field object from in-memory
  representation of .FRM file.

  @param         thd                   Connection context.
  @param         share                 TABLE_SHARE for which Field object
                                       needs to be constructed.
  @param         new_frm_ver           .FRM file version.
  @param         use_hash              Indicates whether we use hash or linear
                                       search to lookup fields by name.
  @param         field_idx             Field index in TABLE_SHARE::field array.
  @param         strpos                Pointer to part of .FRM's screens
                                       section describing the field to be
                                       created.
  @param         format_section_fields Array where each byte contains packed
                                       values of COLUMN_FORMAT/STORAGE options
                                       for corresponding column.
  @param[in/out] comment_pos           Pointer to part of column comments
                                       section of .FRM which corresponds
                                       to current field. Advanced to the
                                       position corresponding to comment
                                       for the next column.
  @param[in/out] gcol_screen_pos       Pointer to part of generated columns
                                       section of .FRM which corresponds
                                       to current generated field. If field
                                       to be created is generated advanced
                                       to the position for the next column
  @param[in/out] null_pos              Current byte in the record preamble
                                       to be used for field's null/leftover
                                       bits if necessary.
  @param[in/out] null_bit_pos          Current bit in the current preamble
                                       byte to be used for field's null/
                                       leftover bits if necessary.
  @param[out]    errarg                Additional argument for the error to
                                       be reported.

  @retval 0      Success.
  @retval non-0  Error number (@sa open_table_def() for details).
*/

static int make_field_from_frm(THD *thd,
                               TABLE_SHARE *share,
                               uint new_frm_ver,
                               bool use_hash,
                               uint field_idx,
                               uchar *strpos,
                               uchar *format_section_fields,
                               char **comment_pos,
                               char **gcol_screen_pos,
                               uchar **null_pos,
                               uint *null_bit_pos,
                               int *errarg)
{
  uint pack_flag, interval_nr, unireg_type, recpos, field_length;
  uint gcol_info_length=0;
  enum_field_types field_type;
  const CHARSET_INFO *charset=NULL;
  Field::geometry_type geom_type= Field::GEOM_GEOMETRY;
  LEX_STRING comment;
  Generated_column *gcol_info= 0;
  bool fld_stored_in_db= true;
  Field *reg_field;

  if (new_frm_ver >= 3)
  {
    /* new frm file in 4.1 */
    field_length= uint2korr(strpos+3);
    recpos=	  uint3korr(strpos+5);
    pack_flag=    uint2korr(strpos+8);
    unireg_type=  (uint) strpos[10];
    interval_nr=  (uint) strpos[12];
    uint comment_length=uint2korr(strpos+15);
    field_type=(enum_field_types) (uint) strpos[13];

    /* charset and geometry_type share the same byte in frm */
    if (field_type == MYSQL_TYPE_GEOMETRY)
    {
      geom_type= (Field::geometry_type) strpos[14];
      charset= &my_charset_bin;
    }
    else
    {
      uint csid= strpos[14] + (((uint) strpos[11]) << 8);
      if (!csid)
        charset= &my_charset_bin;
      else if (!(charset= get_charset(csid, MYF(0))))
      {
        // Unknown or unavailable charset
        *errarg= (int) csid;
        return 5;
      }
    }

    if (!comment_length)
    {
      comment.str= (char*) "";
      comment.length=0;
    }
    else
    {
      comment.str= *comment_pos;
      comment.length= comment_length;
      (*comment_pos)+= comment_length;
    }

    if (unireg_type & Field::GENERATED_FIELD)
    {
      /*
        Get generated column data stored in the .frm file as follows:
        byte 1      = 1 (always 1 to allow for future extensions)
        byte 2,3    = expression length
        byte 4      = flags, as of now:
                        0 - no flags
                        1 - field is physically stored
        byte 5-...  = generated column expression (text data)
      */
      gcol_info= new Generated_column();
      if ((uint)(*gcol_screen_pos)[0] != 1)
        return 4;

      gcol_info_length= uint2korr(*gcol_screen_pos + 1);
      DBUG_ASSERT(gcol_info_length); // Expect non-null expression

      fld_stored_in_db= (bool) (uint) (*gcol_screen_pos)[3];
      gcol_info->set_field_stored(fld_stored_in_db);
      gcol_info->dup_expr_str(&share->mem_root,
                              *gcol_screen_pos + (uint)FRM_GCOL_HEADER_SIZE,
                              gcol_info_length);
      (*gcol_screen_pos)+= gcol_info_length + FRM_GCOL_HEADER_SIZE;
      share->vfields++;
    }
  }
  else
  {
    field_length= (uint) strpos[3];
    recpos=	    uint2korr(strpos+4),
    pack_flag=    uint2korr(strpos+6);
    pack_flag&=   ~FIELDFLAG_NO_DEFAULT;     // Safety for old files
    unireg_type=  (uint) strpos[8];
    interval_nr=  (uint) strpos[10];

    /* old frm file */
    field_type= (enum_field_types) f_packtype(pack_flag);
    if (f_is_binary(pack_flag))
    {
      /*
        Try to choose the best 4.1 type:
        - for 4.0 "CHAR(N) BINARY" or "VARCHAR(N) BINARY"
          try to find a binary collation for character set.
        - for other types (e.g. BLOB) just use my_charset_bin.
      */
      if (!f_is_blob(pack_flag))
      {
        // 3.23 or 4.0 string
        if (!(charset= get_charset_by_csname(share->table_charset->csname,
                                             MY_CS_BINSORT, MYF(0))))
          charset= &my_charset_bin;
      }
      else
        charset= &my_charset_bin;
    }
    else
      charset= share->table_charset;
    memset(&comment, 0, sizeof(comment));
  }

  if (interval_nr && charset->mbminlen > 1)
  {
    /* Unescape UCS2 intervals from HEX notation */
    TYPELIB *interval= share->intervals + interval_nr - 1;
    unhex_type2(interval);
  }

  if (field_type == MYSQL_TYPE_NEWDECIMAL && !share->mysql_version)
  {
    /*
      Fix pack length of old decimal values from 5.0.3 -> 5.0.4
      The difference is that in the old version we stored precision
      in the .frm table while we now store the display_length
    */
    uint decimals= f_decimals(pack_flag);
    field_length= my_decimal_precision_to_length(field_length,
                                                 decimals,
                                                 f_is_dec(pack_flag) == 0);
    sql_print_error("Found incompatible DECIMAL field '%s' in %s; "
                    "Please do \"ALTER TABLE `%s` FORCE\" to fix it!",
                    share->fieldnames.type_names[field_idx],
                    share->table_name.str,
                    share->table_name.str);
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_CRASHED_ON_USAGE,
                        "Found incompatible DECIMAL field '%s' in %s; "
                        "Please do \"ALTER TABLE `%s` FORCE\" to fix it!",
                        share->fieldnames.type_names[field_idx],
                        share->table_name.str,
                        share->table_name.str);
    share->crashed= 1;                        // Marker for CHECK TABLE
  }

  if (field_type == MYSQL_TYPE_YEAR && field_length != 4)
  {
    sql_print_error("Found incompatible YEAR(x) field '%s' in %s; "
                    "Please do \"ALTER TABLE `%s` FORCE\" to fix it!",
                    share->fieldnames.type_names[field_idx],
                    share->table_name.str,
                    share->table_name.str);
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_CRASHED_ON_USAGE,
                        "Found incompatible YEAR(x) field '%s' in %s; "
                        "Please do \"ALTER TABLE `%s` FORCE\" to fix it!",
                        share->fieldnames.type_names[field_idx],
                        share->table_name.str,
                        share->table_name.str);
    share->crashed= 1;
  }

  share->field[field_idx]= reg_field=
    make_field(share,
               share->default_values - 1 + recpos, // recpos starts from 1.
               (uint32) field_length,
               *null_pos, *null_bit_pos,
               pack_flag,
               field_type,
               charset,
               geom_type,
               (Field::utype) MTYP_TYPENR(unireg_type),
               (interval_nr ?
                share->intervals+interval_nr-1 :
                (TYPELIB*) 0),
               share->fieldnames.type_names[field_idx]);
  if (!reg_field)
  {
    // Not supported field type
    return 4;
  }

  reg_field->field_index= field_idx;
  reg_field->comment=comment;
  reg_field->gcol_info= gcol_info;
  reg_field->stored_in_db= fld_stored_in_db;
  if (field_type == MYSQL_TYPE_BIT && !f_bit_as_char(pack_flag))
  {
    if (((*null_bit_pos)+= field_length & 7) > 7)
    {
      (*null_pos)++;
      (*null_bit_pos)-= 8;
    }
  }
  if (!(reg_field->flags & NOT_NULL_FLAG))
  {
    if (!(*null_bit_pos= (*null_bit_pos + 1) & 7))
      (*null_pos)++;
  }
  if (f_no_default(pack_flag))
    reg_field->flags|= NO_DEFAULT_VALUE_FLAG;

  if (reg_field->unireg_check == Field::NEXT_NUMBER)
    share->found_next_number_field= share->field + field_idx;

  if (use_hash)
    if (my_hash_insert(&share->name_hash, (uchar*)(share->field + field_idx)))
    {
      /*
        Set return code 8 here to indicate that an error has
        occurred but that the error message already has been
        sent (OOM).
      */
      return 8;
    }

  if (format_section_fields)
  {
    const uchar field_flags= format_section_fields[field_idx];
    const uchar field_storage= (field_flags & STORAGE_TYPE_MASK);
    const uchar field_column_format=
      ((field_flags >> COLUMN_FORMAT_SHIFT)& COLUMN_FORMAT_MASK);
    DBUG_PRINT("debug", ("field flags: %u, storage: %u, column_format: %u",
                         field_flags, field_storage, field_column_format));
    reg_field->set_storage_type((ha_storage_media)field_storage);
    reg_field->set_column_format((column_format_type)field_column_format);
  }

  if (!reg_field->stored_in_db)
  {
    share->stored_fields--;
    if (share->stored_rec_length>=recpos)
      share->stored_rec_length= recpos-1;
  }

  return 0;
}


/*
  Read data from a binary .frm file from MySQL 3.23 - 5.0 into TABLE_SHARE

  NOTE: Much of the logic here is duplicated in create_tmp_table()
  (see sql_select.cc). Hence, changes to this function may have to be
  repeated there.
*/

static int open_binary_frm(THD *thd, TABLE_SHARE *share, uchar *head,
                           File file)
{
  int error, errarg= 0;
  uint new_frm_ver, field_pack_length, new_field_pack_flag;
  uint interval_count, interval_parts, read_length, int_length;
  uint db_create_options, keys, key_parts, n_length;
  uint key_info_length, com_length, null_bit_pos, gcol_screen_length;
  uint extra_rec_buf_length;
  uint i,j;
  bool use_extended_sk;   // Supported extending of secondary keys with PK parts
  bool use_hash;
  char *keynames, *names, *comment_pos, *gcol_screen_pos;
  char *orig_comment_pos, *orig_gcol_screen_pos;
  uchar forminfo[288];
  uchar *record;
  uchar *disk_buff, *strpos, *null_flags, *null_pos;
  ulong pos, record_offset, *rec_per_key, rec_buff_length;
  rec_per_key_t *rec_per_key_float;
  handler *handler_file= 0;
  KEY	*keyinfo;
  KEY_PART_INFO *key_part;
  Field  **field_ptr;
  const char **interval_array;
  enum legacy_db_type legacy_db_type;
  my_bitmap_map *bitmaps;
  uchar *extra_segment_buff= 0;
  const uint format_section_header_size= 8;
  uchar *format_section_fields= 0;
  bool has_vgc= false;
  DBUG_ENTER("open_binary_frm");

  new_field_pack_flag= head[27];
  new_frm_ver= (head[2] - FRM_VER);
  field_pack_length= new_frm_ver < 2 ? 11 : 17;
  disk_buff= 0;

  error= 3;
  /* Position of the form in the form file. */
  if (!(pos= get_form_pos(file, head)))
    goto err;                                   /* purecov: inspected */

  mysql_file_seek(file,pos,MY_SEEK_SET,MYF(0));
  if (mysql_file_read(file, forminfo,288,MYF(MY_NABP)))
    goto err;
  share->frm_version= head[2];
  /*
    Check if .frm file created by MySQL 5.0. In this case we want to
    display CHAR fields as CHAR and not as VARCHAR.
    We do it this way as we want to keep the old frm version to enable
    MySQL 4.1 to read these files.
  */
  if (share->frm_version == FRM_VER_TRUE_VARCHAR -1 && head[33] == 5)
    share->frm_version= FRM_VER_TRUE_VARCHAR;

  if (*(head+61) &&
      !(share->default_part_db_type= 
        ha_checktype(thd, (enum legacy_db_type) (uint) *(head+61), 1, 0)))
    goto err;
  DBUG_PRINT("info", ("default_part_db_type = %u", head[61]));
  legacy_db_type= (enum legacy_db_type) (uint) *(head+3);
  DBUG_ASSERT(share->db_plugin == NULL);
  /*
    if the storage engine is dynamic, no point in resolving it by its
    dynamically allocated legacy_db_type. We will resolve it later by name.
  */
  if (legacy_db_type > DB_TYPE_UNKNOWN && 
      legacy_db_type < DB_TYPE_FIRST_DYNAMIC)
    share->db_plugin= ha_lock_engine(NULL, 
                                     ha_checktype(thd, legacy_db_type, 0, 0));
  share->db_create_options= db_create_options= uint2korr(head+30);
  share->db_options_in_use= share->db_create_options;
  share->mysql_version= uint4korr(head+51);
  share->null_field_first= 0;
  if (!head[32])				// New frm file in 3.23
  {
    share->avg_row_length= uint4korr(head+34);
    share->row_type= (row_type) head[40];
    share->table_charset= get_charset((((uint) head[41]) << 8) + 
                                        (uint) head[38],MYF(0));
    share->null_field_first= 1;
    share->stats_sample_pages= uint2korr(head+42);
    share->stats_auto_recalc= static_cast<enum_stats_auto_recalc>(head[44]);
  }
  if (!share->table_charset)
  {
    /* unknown charset in head[38] or pre-3.23 frm */
    if (use_mb(default_charset_info))
    {
      /* Warn that we may be changing the size of character columns */
      sql_print_warning("'%s' had no or invalid character set, "
                        "and default character set is multi-byte, "
                        "so character column sizes may have changed",
                        share->path.str);
    }
    share->table_charset= default_charset_info;
  }
  share->db_record_offset= 1;
  /* Set temporarily a good value for db_low_byte_first */
  share->db_low_byte_first= MY_TEST(legacy_db_type != DB_TYPE_ISAM);
  error=4;
  share->max_rows= uint4korr(head+18);
  share->min_rows= uint4korr(head+22);

  /* Read keyinformation */
  key_info_length= (uint) uint2korr(head+28);
  mysql_file_seek(file, (ulong) uint2korr(head+6), MY_SEEK_SET, MYF(0));
  if (read_string(file, &disk_buff,key_info_length))
    goto err;                                   /* purecov: inspected */
  if (disk_buff[0] & 0x80)
  {
    share->keys=      keys=      (disk_buff[1] << 7) | (disk_buff[0] & 0x7f);
    share->key_parts= key_parts= uint2korr(disk_buff+2);
  }
  else
  {
    share->keys=      keys=      disk_buff[0];
    share->key_parts= key_parts= disk_buff[1];
  }
  share->keys_for_keyread.init(0);
  share->keys_in_use.init(keys);

  strpos=disk_buff+6;  

  use_extended_sk=
    ha_check_storage_engine_flag(share->db_type(),
                                 HTON_SUPPORTS_EXTENDED_KEYS);

  uint total_key_parts;
  if (use_extended_sk)
  {
    uint primary_key_parts= keys ?
      (new_frm_ver >= 3) ? (uint) strpos[4] : (uint) strpos[3] : 0;
    total_key_parts= key_parts + primary_key_parts * (keys - 1);
  }
  else
    total_key_parts= key_parts;
  n_length= keys * sizeof(KEY) + total_key_parts * sizeof(KEY_PART_INFO);

  /*
    Allocate memory for the KEY object, the key part array, and the
    two rec_per_key arrays.
  */
  if (!multi_alloc_root(&share->mem_root, 
                        &keyinfo, n_length + uint2korr(disk_buff + 4),
                        &rec_per_key, sizeof(ulong) * total_key_parts,
                        &rec_per_key_float,
                        sizeof(rec_per_key_t) * total_key_parts,
                        NULL))
    goto err;                                   /* purecov: inspected */

  memset(keyinfo, 0, n_length);
  share->key_info= keyinfo;
  key_part= reinterpret_cast<KEY_PART_INFO*>(keyinfo+keys);

  for (i=0 ; i < keys ; i++, keyinfo++)
  {
    keyinfo->table= 0;                           // Updated in open_frm
    if (new_frm_ver >= 3)
    {
      keyinfo->flags=	   (uint) uint2korr(strpos) ^ HA_NOSAME;
      keyinfo->key_length= (uint) uint2korr(strpos+2);
      keyinfo->user_defined_key_parts= (uint) strpos[4];
      keyinfo->algorithm=  (enum ha_key_alg) strpos[5];
      keyinfo->block_size= uint2korr(strpos+6);
      strpos+=8;
    }
    else
    {
      keyinfo->flags=	 ((uint) strpos[0]) ^ HA_NOSAME;
      keyinfo->key_length= (uint) uint2korr(strpos+1);
      keyinfo->user_defined_key_parts= (uint) strpos[3];
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      strpos+=4;
    }

    keyinfo->key_part= key_part;
    keyinfo->set_rec_per_key_array(rec_per_key, rec_per_key_float);
    keyinfo->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);

    for (j=keyinfo->user_defined_key_parts ; j-- ; key_part++)
    {
      *rec_per_key++ = 0;
      *rec_per_key_float++ = REC_PER_KEY_UNKNOWN;

      key_part->fieldnr=	(uint16) (uint2korr(strpos) & FIELD_NR_MASK);
      key_part->offset= (uint) uint2korr(strpos+2)-1;
      key_part->key_type=	(uint) uint2korr(strpos+5);
      // key_part->field=	(Field*) 0;	// Will be fixed later
      if (new_frm_ver >= 1)
      {
	key_part->key_part_flag= *(strpos+4);
	key_part->length=	(uint) uint2korr(strpos+7);
	strpos+=9;
      }
      else
      {
	key_part->length=	*(strpos+4);
	key_part->key_part_flag=0;
	if (key_part->length > 128)
	{
	  key_part->length&=127;		/* purecov: inspected */
	  key_part->key_part_flag=HA_REVERSE_SORT; /* purecov: inspected */
	}
	strpos+=7;
      }
      key_part->store_length=key_part->length;
    }
    /*
      Add primary key parts if engine supports primary key extension for
      secondary keys. Here we add unique first key parts to the end of
      secondary key parts array and increase actual number of key parts.
      Note that primary key is always first if exists. Later if there is no
      primary key in the table then number of actual keys parts is set to
      user defined key parts.
    */
    keyinfo->actual_key_parts= keyinfo->user_defined_key_parts;
    keyinfo->actual_flags= keyinfo->flags;
    if (use_extended_sk && i && !(keyinfo->flags & HA_NOSAME))
    {
      const uint primary_key_parts= share->key_info->user_defined_key_parts;
      keyinfo->unused_key_parts= primary_key_parts;
      key_part+= primary_key_parts;
      rec_per_key+= primary_key_parts;
      rec_per_key_float+= primary_key_parts;
      share->key_parts+= primary_key_parts;
    }
  }
  keynames=(char*) key_part;
  strpos+= (my_stpcpy(keynames, (char *) strpos) - keynames)+1;

  //reading index comments
  for (keyinfo= share->key_info, i=0; i < keys; i++, keyinfo++)
  {
    if (keyinfo->flags & HA_USES_COMMENT)
    {
      keyinfo->comment.length= uint2korr(strpos);
      keyinfo->comment.str= strmake_root(&share->mem_root, (char*) strpos+2,
                                         keyinfo->comment.length);
      strpos+= 2 + keyinfo->comment.length;
    } 
    DBUG_ASSERT(MY_TEST(keyinfo->flags & HA_USES_COMMENT) ==
               (keyinfo->comment.length > 0));
  }

  share->reclength = uint2korr((head+16));
  share->stored_rec_length= share->reclength;
  if (*(head+26) == 1)
    share->system= 1;				/* one-record-database */

  record_offset= (ulong) (uint2korr(head+6)+
                          ((uint2korr(head+14) == 0xffff ?
                            uint4korr(head+47) : uint2korr(head+14))));
 
  if ((n_length= uint4korr(head+55)))
  {
    /* Read extra data segment */
    uchar *next_chunk, *buff_end;
    DBUG_PRINT("info", ("extra segment size is %u bytes", n_length));
    if (!(extra_segment_buff= (uchar*) my_malloc(key_memory_frm_extra_segment_buff,
                                                 n_length, MYF(MY_WME))))
      goto err;
    next_chunk= extra_segment_buff;
    if (mysql_file_pread(file, extra_segment_buff,
                         n_length, record_offset + share->reclength,
                         MYF(MY_NABP)))
    {
      goto err;
    }
    share->connect_string.length= uint2korr(next_chunk);
    if (!(share->connect_string.str= strmake_root(&share->mem_root,
                                                  (char*) next_chunk + 2,
                                                  share->connect_string.
                                                  length)))
    {
      goto err;
    }
    next_chunk+= share->connect_string.length + 2;
    buff_end= extra_segment_buff + n_length;
    if (next_chunk + 2 < buff_end)
    {
      uint str_db_type_length= uint2korr(next_chunk);
      LEX_STRING name;
      name.str= (char*) next_chunk + 2;
      name.length= str_db_type_length;

      plugin_ref tmp_plugin= ha_resolve_by_name(thd, &name, FALSE);
      if (tmp_plugin != NULL && !plugin_equals(tmp_plugin, share->db_plugin))
      {
        if (legacy_db_type > DB_TYPE_UNKNOWN &&
            legacy_db_type < DB_TYPE_FIRST_DYNAMIC &&
            legacy_db_type !=
            ha_legacy_type(plugin_data<handlerton*>(tmp_plugin)))
        {
          /* bad file, legacy_db_type did not match the name */
          goto err;
        }
        /*
          tmp_plugin is locked with a local lock.
          we unlock the old value of share->db_plugin before
          replacing it with a globally locked version of tmp_plugin
        */
        plugin_unlock(NULL, share->db_plugin);
        share->db_plugin= my_plugin_lock(NULL, &tmp_plugin);
        DBUG_PRINT("info", ("setting dbtype to '%.*s' (%d)",
                            str_db_type_length, next_chunk + 2,
                            ha_legacy_type(share->db_type())));
      }
      else if (!tmp_plugin && str_db_type_length == 9 &&
               !strncmp((char *) next_chunk + 2, "partition", 9))
      {
        /*
          An I_S query during server restart will provoke deprecation warnings.
          Since there is no client connection for this query, make sure we
          write the deprecation warning in the error log. Otherwise, push
          warnings to the client.
        */
        if (mysqld_server_started)
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_DEPRECATED_SYNTAX,
                              ER_THD(thd,
                                     ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                              share->db.str, share->table_name.str);
        else
          /*
            Use the same string as above, not for localization, but for
            making sure the wording is equal.
          */
          sql_print_warning(
                  ER_DEFAULT(ER_PARTITION_ENGINE_DEPRECATED_FOR_TABLE),
                  share->db.str, share->table_name.str);

        /* Check if the partitioning engine is ready */
        if (!ha_checktype(thd, DB_TYPE_PARTITION_DB, true, false))
        {
          error= 8;
          my_error(ER_FEATURE_NOT_AVAILABLE, MYF(0), "partitioning",
                   "--skip-partition", "-DWITH_PARTITION_STORAGE_ENGINE=1");
          goto err;
        }
	/*
          Partition engine is ready, share->db_plugin must already contain a
          properly locked reference to it.
        */
	DBUG_ASSERT(is_ha_partition_handlerton(plugin_data<handlerton*>(
                                                 share->db_plugin)));
        DBUG_PRINT("info", ("setting dbtype to '%.*s' (%d)",
                            str_db_type_length, next_chunk + 2,
                            ha_legacy_type(share->db_type())));
      }
      else if (!tmp_plugin && name.length == 18 &&
               !strncmp(name.str, "PERFORMANCE_SCHEMA", name.length))
      {
        /*
          A FRM file is present on disk,
          for a PERFORMANCE_SCHEMA table,
          but this server binary is not compiled with the performance_schema,
          as ha_resolve_by_name() did not find the storage engine.
          This can happen:
          - (a) during tests with mysql-test-run,
            because the same database installed image is used
            for regular builds (with P_S) and embedded builds (without P_S)
          - (b) in production, when random binaries (without P_S) are thrown
            on top of random installed database instances on disk (with P_S).
          For the sake of robustness, pretend the table simply does not exist,
          so that in particular it does not pollute the information_schema
          with errors when scanning the disk for FRM files.
          Note that ER_NO_SUCH_TABLE has a special treatment
          in fill_schema_table_by_open()
        */
        error= 1;
        my_error(ER_NO_SUCH_TABLE, MYF(0), share->db.str, share->table_name.str);
        goto err;
      }
      else if (!tmp_plugin)
      {
        /* purecov: begin inspected */
        error= 8;
        name.str[name.length]=0;
        my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), name.str);
        goto err;
        /* purecov: end */
      }
      next_chunk+= str_db_type_length + 2;
    }
    if (next_chunk + 5 < buff_end)
    {
      uint32 partition_info_str_len = uint4korr(next_chunk);
      if ((share->partition_info_buffer_size=
             share->partition_info_str_len= partition_info_str_len))
      {
        if (!(share->partition_info_str= (char*)
              memdup_root(&share->mem_root, next_chunk + 4,
                          partition_info_str_len + 1)))
        {
          goto err;
        }
      }
      next_chunk+= 5 + partition_info_str_len;
    }
    if (share->mysql_version >= 50110 && next_chunk < buff_end)
    {
      /* New auto_partitioned indicator introduced in 5.1.11 */
      share->auto_partitioned= *next_chunk;
      next_chunk++;
    }
    keyinfo= share->key_info;
    for (i= 0; i < keys; i++, keyinfo++)
    {
      if (keyinfo->flags & HA_USES_PARSER)
      {
        if (next_chunk >= buff_end)
        {
          DBUG_PRINT("error",
                     ("fulltext key uses parser that is not defined in .frm"));
          goto err;
        }
        LEX_CSTRING parser_name= {reinterpret_cast<char*>(next_chunk),
                                  strlen(reinterpret_cast<char*>(next_chunk))};
        next_chunk+= parser_name.length + 1;
        keyinfo->parser= my_plugin_lock_by_name(NULL, parser_name,
                                                MYSQL_FTPARSER_PLUGIN);
        if (! keyinfo->parser)
        {
          my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), parser_name.str);
          goto err;
        }
      }
    }
    if (forminfo[46] == (uchar)255)
    {
      //reading long table comment
      if (next_chunk + 2 > buff_end)
      {
          DBUG_PRINT("error",
                     ("long table comment is not defined in .frm"));
          goto err;
      }
      share->comment.length = uint2korr(next_chunk);
      if (! (share->comment.str= strmake_root(&share->mem_root,
             (char*)next_chunk + 2, share->comment.length)))
      {
          goto err;
      }
      next_chunk+= 2 + share->comment.length;
    }

    if (next_chunk + format_section_header_size < buff_end)
    {
      /*
        New extra data segment called "format section" with additional
        table and column properties introduced by MySQL Cluster
        based on 5.1.20

        Table properties:
        TABLESPACE <ts> and STORAGE [DISK|MEMORY]

        Column properties:
        COLUMN_FORMAT [DYNAMIC|FIXED] and STORAGE [DISK|MEMORY]
      */
      DBUG_PRINT("info", ("Found format section"));

      /* header */
      const uint format_section_length= uint2korr(next_chunk);
      const uint format_section_flags= uint4korr(next_chunk+2);
      /* 2 bytes unused */

      if (next_chunk + format_section_length > buff_end)
      {
        DBUG_PRINT("error", ("format section length too long: %u",
                             format_section_length));
        goto err;
      }
      DBUG_PRINT("info", ("format_section_length: %u, format_section_flags: %u",
                          format_section_length, format_section_flags));

      share->default_storage_media=
        (enum ha_storage_media) (format_section_flags & 0x7);

      /* tablespace */
      const char *tablespace=
        (const char*)next_chunk + format_section_header_size;
      const size_t tablespace_length= strlen(tablespace);
      share->tablespace= NULL;
      if (tablespace_length)
      {
        Tablespace_name_error_handler error_handler;
        thd->push_internal_handler(&error_handler);
        enum_ident_name_check name_check= check_tablespace_name(tablespace);
        thd->pop_internal_handler();
        if (name_check == IDENT_NAME_OK &&
          !(share->tablespace= strmake_root(&share->mem_root,
                                            tablespace, tablespace_length+1)))
        {
          goto err;
        }
      }
      DBUG_PRINT("info", ("tablespace: '%s'",
                          share->tablespace ? share->tablespace : "<null>"));

      /* pointer to format section for fields */
      format_section_fields=
        next_chunk + format_section_header_size + tablespace_length + 1;

      next_chunk+= format_section_length;
    }

    if (next_chunk + 2 <= buff_end)
    {
      share->compress.length = uint2korr(next_chunk);
      if (! (share->compress.str= strmake_root(&share->mem_root,
             (char*)next_chunk + 2, share->compress.length)))
      {
          goto err;
      }
      next_chunk+= 2 + share->compress.length;
    }

    if (next_chunk + 2 <= buff_end)
    {
      share->encrypt_type.length = uint2korr(next_chunk);
      if (! (share->encrypt_type.str= strmake_root(&share->mem_root,
             (char*)next_chunk + 2, share->encrypt_type.length)))
      {
          goto err;
      }
      next_chunk+= 2 + share->encrypt_type.length;
    }
  }
  share->key_block_size= uint2korr(head+62);

  error=4;
  extra_rec_buf_length= uint2korr(head+59);
  rec_buff_length= ALIGN_SIZE(share->reclength + 1 + extra_rec_buf_length);
  share->rec_buff_length= rec_buff_length;
  if (!(record= (uchar *) alloc_root(&share->mem_root,
                                     rec_buff_length)))
    goto err;                                   /* purecov: inspected */
  share->default_values= record;
  if (mysql_file_pread(file, record, (size_t) share->reclength,
                       record_offset, MYF(MY_NABP)))
    goto err;                                   /* purecov: inspected */

  mysql_file_seek(file, pos+288, MY_SEEK_SET, MYF(0));

  share->fields= uint2korr(forminfo+258);
  pos= uint2korr(forminfo+260);   /* Length of all screens */
  n_length= uint2korr(forminfo+268);
  interval_count= uint2korr(forminfo+270);
  interval_parts= uint2korr(forminfo+272);
  int_length= uint2korr(forminfo+274);
  share->null_fields= uint2korr(forminfo+282);
  com_length= uint2korr(forminfo+284);
  gcol_screen_length= uint2korr(forminfo+286);
  share->vfields= 0;
  share->stored_fields= share->fields;
  if (forminfo[46] != (uchar)255)
  {
    share->comment.length=  (int) (forminfo[46]);
    share->comment.str= strmake_root(&share->mem_root, (char*) forminfo+47,
                                     share->comment.length);
  }

  DBUG_PRINT("info",("i_count: %d  i_parts: %d  index: %d  n_length: %d  int_length: %d  com_length: %d  gcol_screen_length: %d", interval_count,interval_parts, share->keys,n_length,int_length, com_length, gcol_screen_length));
  if (!(field_ptr = (Field **)
	alloc_root(&share->mem_root,
		   (uint) ((share->fields+1)*sizeof(Field*)+
			   interval_count*sizeof(TYPELIB)+
			   (share->fields+interval_parts+
			    keys+3)*sizeof(char *)+
			   (n_length+int_length+com_length+
			       gcol_screen_length)))))
    goto err;                                   /* purecov: inspected */

  share->field= field_ptr;
  read_length=(uint) (share->fields * field_pack_length +
		      pos+ (uint) (n_length+int_length+com_length+
		                   gcol_screen_length));
  if (read_string(file, &disk_buff,read_length))
    goto err;                                   /* purecov: inspected */

  strpos= disk_buff+pos;

  share->intervals= (TYPELIB*) (field_ptr+share->fields+1);
  interval_array= (const char **) (share->intervals+interval_count);
  names= (char*) (interval_array+share->fields+interval_parts+keys+3);
  if (!interval_count)
    share->intervals= 0;			// For better debugging
  memcpy(names, strpos+(share->fields*field_pack_length),
	 (uint) (n_length+int_length));
  orig_comment_pos= comment_pos= names+(n_length+int_length);
  memcpy(comment_pos, disk_buff+read_length-com_length-gcol_screen_length, 
         com_length);
  orig_gcol_screen_pos= gcol_screen_pos= names+(n_length+int_length+com_length);
  memcpy(gcol_screen_pos, disk_buff+read_length-gcol_screen_length, 
         gcol_screen_length);

  fix_type_pointers(&interval_array, &share->fieldnames, 1, &names);
  if (share->fieldnames.count != share->fields)
    goto err;
  fix_type_pointers(&interval_array, share->intervals, interval_count,
		    &names);

  {
    /* Set ENUM and SET lengths */
    TYPELIB *interval;
    for (interval= share->intervals;
         interval < share->intervals + interval_count;
         interval++)
    {
      uint count= (uint) (interval->count + 1) * sizeof(uint);
      if (!(interval->type_lengths= (uint *) alloc_root(&share->mem_root,
                                                        count)))
        goto err;
      for (count= 0; count < interval->count; count++)
      {
        char *val= (char*) interval->type_names[count];
        interval->type_lengths[count]= strlen(val);
      }
      interval->type_lengths[count]= 0;
    }
  }

  if (keynames)
    fix_type_pointers(&interval_array, &share->keynames, 1, &keynames);

 /* Allocate handler */
  if (!(handler_file= get_new_handler(share, thd->mem_root,
                                      share->db_type())))
    goto err;

  if (handler_file->set_ha_share_ref(&share->ha_share))
    goto err;

  if (share->null_field_first)
  {
    null_flags= null_pos= share->default_values;
    null_bit_pos= (db_create_options & HA_OPTION_PACK_RECORD) ? 0 : 1;
    /*
      null_bytes below is only correct under the condition that
      there are no bit fields.  Correct values is set below after the
      table struct is initialized
    */
    share->null_bytes= (share->null_fields + null_bit_pos + 7) / 8;
  }
  else
  {
    share->null_bytes= (share->null_fields+7)/8;
    null_flags= null_pos= share->default_values + share->reclength -
                          share->null_bytes;
    null_bit_pos= 0;
  }

  use_hash= share->fields >= MAX_FIELDS_BEFORE_HASH;
  if (use_hash)
    use_hash= !my_hash_init(&share->name_hash,
                            system_charset_info,
                            share->fields,0,0,
                            (my_hash_get_key) get_field_name,0,0,
                            PSI_INSTRUMENT_ME);

  for (i=0 ; i < share->fields; i++, strpos+=field_pack_length)
  {
    if (new_frm_ver >= 3 &&
        (strpos[10] & Field::GENERATED_FIELD) && // Field::unireg_check
        ! (bool) (uint) (gcol_screen_pos[3]))    // Field::stored_in_db
    {
      /*
        Skip virtual generated columns as we will do separate pass for them.

        We still need to advance pointers to current comment and generated
        column info in for such fields.
      */
      comment_pos+= uint2korr(strpos+15);
      gcol_screen_pos+= uint2korr(gcol_screen_pos + 1) + FRM_GCOL_HEADER_SIZE;
      has_vgc= true;
    }
    else
    {
      if ((error= make_field_from_frm(thd, share,
                                      new_frm_ver, use_hash,
                                      i, strpos,
                                      format_section_fields,
                                      &comment_pos,
                                      &gcol_screen_pos,
                                      &null_pos,
                                      &null_bit_pos,
                                      &errarg)))
        goto err;
    }
  }

  if (has_vgc)
  {
    /*
      We need to do separate pass through field descriptions for virtual
      generated columns to ensure that they get allocated null/leftover
      bits at the tail of record preamble.
    */
    strpos= disk_buff+pos;
    comment_pos= orig_comment_pos;
    gcol_screen_pos= orig_gcol_screen_pos;
    // Generated columns can be present only in new .FRMs.
    DBUG_ASSERT(new_frm_ver >= 3);
    for (i=0 ; i < share->fields; i++, strpos+=field_pack_length)
    {
      if ((strpos[10] & Field::GENERATED_FIELD) && // Field::unireg_check
          !(bool) (uint) (gcol_screen_pos[3]))     // Field::stored_in_db
      {
        if ((error= make_field_from_frm(thd, share,
                                        new_frm_ver, use_hash,
                                        i, strpos,
                                        format_section_fields,
                                        &comment_pos,
                                        &gcol_screen_pos,
                                        &null_pos,
                                        &null_bit_pos,
                                        &errarg)))
          goto err;
      }
      else
      {
        /*
          Advance pointers to current comment and generated columns
          info for stored fields.
        */
        comment_pos+= uint2korr(strpos+15);
        if (strpos[10] & Field::GENERATED_FIELD) // Field::unireg_check
        {
          gcol_screen_pos+= uint2korr(gcol_screen_pos + 1) +
                            FRM_GCOL_HEADER_SIZE;
        }
      }
    }
  }
  error= 4;
  share->field[share->fields]= 0; // End marker
  /* Sanity checks: */
  DBUG_ASSERT(share->fields >= share->stored_fields);
  DBUG_ASSERT(share->reclength >= share->stored_rec_length);

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    const int pk_off= find_type(primary_key_name, &share->keynames,
                                  FIND_TYPE_NO_PREFIX);
    uint primary_key= (pk_off > 0 ? pk_off-1 : MAX_KEY);

    longlong ha_option= handler_file->ha_table_flags();
    keyinfo= share->key_info;
    key_part= keyinfo->key_part;

    for (uint key=0 ; key < share->keys ; key++,keyinfo++)
    {
      uint usable_parts= 0;
      keyinfo->name=(char*) share->keynames.type_names[key];
      /* Fix fulltext keys for old .frm files */
      if (share->key_info[key].flags & HA_FULLTEXT)
	share->key_info[key].algorithm= HA_KEY_ALG_FULLTEXT;

      if (primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
	/*
	  If the UNIQUE key doesn't have NULL columns and is not a part key
	  declare this as a primary key.
	*/
	primary_key=key;
	for (i=0 ; i < keyinfo->user_defined_key_parts ;i++)
	{
          DBUG_ASSERT(key_part[i].fieldnr > 0);
          // Table field corresponding to the i'th key part.
          Field *table_field= share->field[key_part[i].fieldnr - 1];

          // Index on virtual generated columns is not allowed to be PK
          // even when the conditions below are true, so this case must be
          // rejected here.
          if (table_field->is_virtual_gcol())
          {
            primary_key= MAX_KEY;		// Can't be used
            break;
          }

          /*
            If the key column is of NOT NULL BLOB type, then it
            will definitly have key prefix. And if key part prefix size
            is equal to the BLOB column max size, then we can promote
            it to primary key.
          */
          if (!table_field->real_maybe_null() &&
              table_field->type() == MYSQL_TYPE_BLOB &&
              table_field->field_length == key_part[i].length)
            continue;
          /*
            If the key column is of NOT NULL GEOMETRY type, specifically POINT
            type whose length is known internally (which is 25). And key part
            prefix size is equal to the POINT column max size, then we can
            promote it to primary key.
          */
          if (!table_field->real_maybe_null() &&
              table_field->type() == MYSQL_TYPE_GEOMETRY &&
              table_field->get_geometry_type() == Field::GEOM_POINT &&
              key_part[i].length == MAX_LEN_GEOM_POINT_FIELD)
            continue;

	  if (table_field->real_maybe_null() ||
	      table_field->key_length() != key_part[i].length)
 	  {
	    primary_key= MAX_KEY;		// Can't be used
	    break;
	  }
	}
      }

      for (i=0 ; i < keyinfo->user_defined_key_parts ; key_part++,i++)
      {
        Field *field;
	if (new_field_pack_flag <= 1)
	  key_part->fieldnr= (uint16) find_field(share->field,
                                                 share->default_values,
                                                 (uint) key_part->offset,
                                                 (uint) key_part->length);
	if (!key_part->fieldnr)
        {
          error= 4;                             // Wrong file
          goto err;
        }
        field= key_part->field= share->field[key_part->fieldnr-1];
        key_part->type= field->key_type();
        if (field->real_maybe_null())
        {
          key_part->null_offset=field->null_offset(share->default_values);
          key_part->null_bit= field->null_bit;
          key_part->store_length+=HA_KEY_NULL_LENGTH;
          keyinfo->flags|=HA_NULL_PART_KEY;
          keyinfo->key_length+= HA_KEY_NULL_LENGTH;
        }
        if (field->type() == MYSQL_TYPE_BLOB ||
            field->real_type() == MYSQL_TYPE_VARCHAR ||
            field->type() == MYSQL_TYPE_GEOMETRY)
        {
          key_part->store_length+=HA_KEY_BLOB_LENGTH;
          if (i + 1 <= keyinfo->user_defined_key_parts)
            keyinfo->key_length+= HA_KEY_BLOB_LENGTH;
        }
        key_part->init_flags();

        if (field->is_virtual_gcol())
          keyinfo->flags|= HA_VIRTUAL_GEN_KEY;

        setup_key_part_field(share, handler_file, primary_key,
                             keyinfo, key, i, &usable_parts, true);

        field->flags|= PART_KEY_FLAG;
        if (key == primary_key)
        {
          field->flags|= PRI_KEY_FLAG;
          /*
            If this field is part of the primary key and all keys contains
            the primary key, then we can use any key to find this column
          */
          if (ha_option & HA_PRIMARY_KEY_IN_READ_INDEX)
          {
            if (field->key_length() == key_part->length &&
                !(field->flags & BLOB_FLAG))
              field->part_of_key= share->keys_in_use;
            if (field->part_of_sortkey.is_set(key))
              field->part_of_sortkey= share->keys_in_use;
          }
        }
        if (field->key_length() != key_part->length)
        {
          if (field->type() == MYSQL_TYPE_NEWDECIMAL)
          {
            /*
              Fix a fatal error in decimal key handling that causes crashes
              on Innodb. We fix it by reducing the key length so that
              InnoDB never gets a too big key when searching.
              This allows the end user to do an ALTER TABLE to fix the
              error.
            */
            keyinfo->key_length-= (key_part->length - field->key_length());
            key_part->store_length-= (uint16)(key_part->length -
                                              field->key_length());
            key_part->length= (uint16)field->key_length();
            sql_print_error("Found wrong key definition in %s; "
                            "Please do \"ALTER TABLE `%s` FORCE \" to fix it!",
                            share->table_name.str,
                            share->table_name.str);
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_CRASHED_ON_USAGE,
                                "Found wrong key definition in %s; "
                                "Please do \"ALTER TABLE `%s` FORCE\" to fix "
                                "it!",
                                share->table_name.str,
                                share->table_name.str);
            share->crashed= 1;                // Marker for CHECK TABLE
            continue;
          }
          key_part->key_part_flag|= HA_PART_KEY_SEG;
        }
      }


      if (use_extended_sk && primary_key < MAX_KEY &&
          key && !(keyinfo->flags & HA_NOSAME))
        key_part+= add_pk_parts_to_sk(keyinfo, key, share->key_info, primary_key,
                                      share,  handler_file, &usable_parts);

      /* Skip unused key parts if they exist */
      key_part+= keyinfo->unused_key_parts;

      keyinfo->usable_key_parts= usable_parts; // Filesort

      set_if_bigger(share->max_key_length,keyinfo->key_length+
                    keyinfo->user_defined_key_parts);
      share->total_key_length+= keyinfo->key_length;
      /*
        MERGE tables do not have unique indexes. But every key could be
        an unique index on the underlying MyISAM table. (Bug #10400)
      */
      if ((keyinfo->flags & HA_NOSAME) ||
          (ha_option & HA_ANY_INDEX_MAY_BE_UNIQUE))
        set_if_bigger(share->max_unique_length,keyinfo->key_length);
    }
    if (primary_key < MAX_KEY &&
	(share->keys_in_use.is_set(primary_key)))
    {
      share->primary_key= primary_key;
      /*
	If we are using an integer as the primary key then allow the user to
	refer to it as '_rowid'
      */
      if (share->key_info[primary_key].user_defined_key_parts == 1)
      {
	Field *field= share->key_info[primary_key].key_part[0].field;
	if (field && field->result_type() == INT_RESULT)
        {
          /* note that fieldnr here (and rowid_field_offset) starts from 1 */
	  share->rowid_field_offset= (share->key_info[primary_key].key_part[0].
                                      fieldnr);
        }
      }
    }
    else
      share->primary_key = MAX_KEY; // we do not have a primary key
  }
  else
    share->primary_key= MAX_KEY;
  my_free(disk_buff);
  disk_buff=0;
  if (new_field_pack_flag <= 1)
  {
    /* Old file format with default as not null */
    uint null_length= (share->null_fields+7)/8;
    memset(share->default_values + (null_flags - record), 255,
           null_length);
  }

  if (share->found_next_number_field)
  {
    Field *reg_field= *share->found_next_number_field;
    if ((int) (share->next_number_index= (uint)
	       find_ref_key(share->key_info, share->keys,
                            share->default_values, reg_field,
			    &share->next_number_key_offset,
                            &share->next_number_keypart)) < 0)
    {
      /* Wrong field definition */
      error= 4;
      goto err;
    }
    else
      reg_field->flags |= AUTO_INCREMENT_FLAG;
  }

  if (share->blob_fields)
  {
    Field **ptr;
    uint k, *save;

    /* Store offsets to blob fields to find them fast */
    if (!(share->blob_field= save=
	  (uint*) alloc_root(&share->mem_root,
                             (uint) (share->blob_fields* sizeof(uint)))))
      goto err;
    for (k=0, ptr= share->field ; *ptr ; ptr++, k++)
    {
      if ((*ptr)->flags & BLOB_FLAG)
	(*save++)= k;
    }
  }

  /*
    the correct null_bytes can now be set, since bitfields have been taken
    into account
  */
  share->null_bytes= (null_pos - null_flags +
                      (null_bit_pos + 7) / 8);
  share->last_null_bit_pos= null_bit_pos;

  share->db_low_byte_first= handler_file->low_byte_first();
  share->column_bitmap_size= bitmap_buffer_size(share->fields);

  if (!(bitmaps= (my_bitmap_map*) alloc_root(&share->mem_root,
                                             share->column_bitmap_size)))
    goto err;
  bitmap_init(&share->all_set, bitmaps, share->fields, FALSE);
  bitmap_set_all(&share->all_set);

  delete handler_file;
#ifndef DBUG_OFF
  if (use_hash)
    (void) my_hash_check(&share->name_hash);
#endif
  my_free(extra_segment_buff);
  DBUG_RETURN (0);

 err:
  share->error= error;
  share->open_errno= my_errno();
  share->errarg= errarg;
  my_free(disk_buff);
  my_free(extra_segment_buff);
  delete handler_file;
  my_hash_free(&share->name_hash);

  open_table_error(share, error, share->open_errno, errarg);
  DBUG_RETURN(error);
} /* open_binary_frm */

/**
  @brief validate_generated_expr
    Validate the generated expression to see whether there are invalid
    Item objects.
  @note
    Needs to be done after fix_fields to allow checking references
    to other generated columns.

  @param field  Pointer of generated column

  @return  TRUE  The generated expression has some invalid objects
  @return  FALSE No illegal objects in the generated expression
 */
static bool validate_generated_expr(Field *field)
{
  DBUG_ENTER("validate_generate_expr");
  Item* expr= field->gcol_info->expr_item;
  const char *field_name= field->field_name;
  DBUG_ASSERT(expr);

  /**
    These are not allowed:
    1) SP/UDF
    2) System variables and parameters
    3) ROW values
    4) Subquery (already checked by parser, assert the condition)
   */
  if (expr->has_stored_program() ||             // 1)
      (expr->used_tables() &
       (RAND_TABLE_BIT | PARAM_TABLE_BIT)) ||   // 2)
      (expr->cols() != 1))                      // 3)
  {
    my_error(ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED, MYF(0), field_name);
    DBUG_RETURN(TRUE);
  }
  DBUG_ASSERT(!expr->has_subquery());           // 4)
  /*
    Walk through the Item tree, checking the validity of items
    belonging to the generated column.
  */
  int args[2];
  args[0]= field->field_index;
  args[1]= ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED; // default error code.
  if (expr->walk(&Item::check_gcol_func_processor, Item::WALK_POSTFIX,
                 pointer_cast<uchar*>(&args)))
  {
    my_error(args[1], MYF(0), field_name);
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}

/**
  @brief  fix_fields_gcol_func
    Process generated expression of the field.

  @param thd                The thread object
  @param field              The processed field

  @return
    TRUE                 An error occurred, something was wrong with the
                         function.
  @return
    FALSE                Ok, generated expression is fixed sucessfully 
 */
static bool fix_fields_gcol_func(THD *thd, Field *field)
{
  uint dir_length, home_dir_length;
  bool result= TRUE;
  Item* func_expr= field->gcol_info->expr_item;
  TABLE *table= field->table;
  TABLE_LIST tables;
  TABLE_LIST *save_table_list, *save_first_table, *save_last_table;
  int error;
  Name_resolution_context *context;
  const char *save_where;
  char* db_name;
  char db_name_string[FN_REFLEN];
  bool save_use_only_table_context;
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
  DBUG_ASSERT(func_expr);
  DBUG_ENTER("fix_fields_gcol_func");

  /*
    Set-up the TABLE_LIST object to be a list with a single table
    Set the object to zero to create NULL pointers and set alias
    and real name to table name and get database name from file name.
  */

  memset((void*)&tables, 0, sizeof(TABLE_LIST));
  tables.alias= tables.table_name= table->s->table_name.str;
  tables.table= table;
  tables.next_local= 0;
  tables.next_name_resolution_table= 0;
  my_stpmov(db_name_string, table->s->normalized_path.str);
  dir_length= dirname_length(db_name_string);
  db_name_string[dir_length - 1]= 0;
  home_dir_length= dirname_length(db_name_string);
  db_name= &db_name_string[home_dir_length];
  tables.db= db_name;

  thd->mark_used_columns= MARK_COLUMNS_NONE;

  context= thd->lex->current_context();
  table->get_fields_in_item_tree= TRUE;
  save_table_list= context->table_list;
  save_first_table= context->first_name_resolution_table;
  save_last_table= context->last_name_resolution_table;
  context->table_list= &tables;
  context->first_name_resolution_table= &tables;
  context->last_name_resolution_table= NULL;
  func_expr->walk(&Item::change_context_processor, Item::WALK_POSTFIX,
                  (uchar*) context);
  save_where= thd->where;
  thd->where= "generated column function";

  /* Save the context before fixing the fields*/
  save_use_only_table_context= thd->lex->use_only_table_context;
  thd->lex->use_only_table_context= TRUE;

  bool charset_switched= false;
  const CHARSET_INFO *saved_collation_connection= func_expr->default_charset();
  if (saved_collation_connection != table->s->table_charset)
  {
   thd->variables.collation_connection= table->s->table_charset;
   charset_switched= true;
  }

  Item *new_func= func_expr;
  error= func_expr->fix_fields(thd, &new_func);

  /* Restore the current connection character set and collation. */
  if (charset_switched)
    thd->variables.collation_connection=  saved_collation_connection;

  /* Restore the original context*/
  thd->lex->use_only_table_context= save_use_only_table_context;
  context->table_list= save_table_list;
  context->first_name_resolution_table= save_first_table;
  context->last_name_resolution_table= save_last_table;

  if (unlikely(error))
  {
    DBUG_PRINT("info", ("Field in generated column function not part of table"));
    goto end;
  }
  thd->where= save_where;
  /*
    Checking if all items are valid to be part of the generated column.
  */
  if (validate_generated_expr(field))
    goto end;

  // Virtual columns expressions that substitute themselves are invalid
  DBUG_ASSERT(new_func == func_expr);
  result= FALSE;

end:
  table->get_fields_in_item_tree= FALSE;
  thd->mark_used_columns= save_mark_used_columns;
  DBUG_RETURN(result);
}

/**
  Calculate the base_columns_map and num_non_virtual_base_cols members of
  this generated column

  @param table    Table with the checked field

  @return true if error
 */

bool Generated_column::register_base_columns(TABLE *table)
{
  DBUG_ENTER("register_base_columns");
  my_bitmap_map *bitbuf=
    static_cast<my_bitmap_map *>(alloc_root(&table->mem_root,
                                bitmap_buffer_size(table->s->fields)));
  DBUG_ASSERT(num_non_virtual_base_cols == 0);
  bitmap_init(&base_columns_map, bitbuf, table->s->fields, 0);

  MY_BITMAP *save_old_read_set= table->read_set;
  table->read_set= &base_columns_map;
  Mark_field mark_fld(MARK_COLUMNS_TEMP);
  expr_item->walk(&Item::mark_field_in_map,
                  Item::WALK_PREFIX, (uchar *) &mark_fld);
  table->read_set= save_old_read_set;

  /* Calculate the number of non-virtual base columns */
  for (uint i= 0; i < table->s->fields; i++)
  {
    Field *field= table->field[i];
    if (bitmap_is_set(&base_columns_map, field->field_index) &&
        field->stored_in_db)
      num_non_virtual_base_cols++;
  }
  DBUG_RETURN(false);
}


void Generated_column::dup_expr_str(MEM_ROOT *root, const char *src,
                                    size_t len)
{
  if (!root)
    root= m_expr_str_mem_root;
  else
    m_expr_str_mem_root= root;
  expr_str.str= pointer_cast<char*>(memdup_root(root, src, len));
  expr_str.length= len;
}


/**
  @brief  unpack_gcol_info_from_frm
    Unpack the definition of a virtual column

  @param thd                  Thread handler
  @param table                Table with the checked field
  @param field                Pointer to Field object
  @param is_create_table      Indicates that table is opened as part
                              of CREATE or ALTER and does not yet exist in SE
  @param error_reported       updated flag for the caller that no other error
                              messages are to be generated.

  @return
    TRUE            Failure
  @return
    FALSE           Success
 */

static bool unpack_gcol_info_from_frm(THD *thd,
                                      TABLE *table,
                                      Field *field,
                                      bool is_create_table,
                                      bool *error_reported)
{
  DBUG_ENTER("unpack_gcol_info_from_frm");
  DBUG_ASSERT(field->table == table);
  LEX_STRING *gcol_expr= &field->gcol_info->expr_str;
  DBUG_ASSERT(gcol_expr);
  /*
    Step 1: Construct a statement for the parser.
    The parsed string needs to take the following format:
    "PARSE_GCOL_EXPR (<expr_string_from_frm>)"
  */
  char *gcol_expr_str;
  int str_len= 0;
  const CHARSET_INFO *old_character_set_client;
  bool disable_strict_mode= false;
  bool status;

  Strict_error_handler strict_handler;

  if (!(gcol_expr_str= (char*) alloc_root(&table->mem_root,
                                          gcol_expr->length +
                                            PARSE_GCOL_KEYWORD.length + 3)))
  {
    DBUG_RETURN(TRUE);
  }
  memcpy(gcol_expr_str,
         PARSE_GCOL_KEYWORD.str,
         PARSE_GCOL_KEYWORD.length);
  str_len= PARSE_GCOL_KEYWORD.length;
  memcpy(gcol_expr_str + str_len, "(", 1);
  str_len++;
  memcpy(gcol_expr_str + str_len,
         gcol_expr->str,
         gcol_expr->length);
  str_len+= gcol_expr->length;
  memcpy(gcol_expr_str + str_len, ")", 1);
  str_len++;
  memcpy(gcol_expr_str + str_len, "\0", 1);
  str_len++;
  Parser_state parser_state;
  parser_state.init(thd, gcol_expr_str, str_len);

  /*
    Step 2: Setup thd for parsing.
  */
  Query_arena *backup_stmt_arena_ptr= thd->stmt_arena;
  Query_arena backup_arena;
  Query_arena gcol_arena(&table->mem_root,
                         Query_arena::STMT_CONVENTIONAL_EXECUTION);
  thd->set_n_backup_active_arena(&gcol_arena, &backup_arena);
  thd->stmt_arena= &gcol_arena;
  ulong save_old_privilege= thd->want_privilege;
  thd->want_privilege= 0;

  thd->lex->parse_gcol_expr= TRUE;
  old_character_set_client= thd->variables.character_set_client;
  // Subquery is not allowed in generated expression
  const bool save_allow_subselects= thd->lex->expr_allows_subselect;
  thd->lex->expr_allows_subselect= false;

  /*
    Step 3: Use the parser to build an Item object from.
  */
  if (parse_sql(thd, &parser_state, NULL))
  {
    goto parse_err;
  }
  thd->lex->expr_allows_subselect= save_allow_subselects;

  /* Keep attribute of generated column */
  thd->lex->gcol_info->set_field_stored(field->stored_in_db);
  /* From now on use gcol_info generated by the parser. */
  field->gcol_info= thd->lex->gcol_info;

  /* Use strict mode regardless of strict mode setting when validating */
  if (!thd->is_strict_mode())
  {
    thd->variables.sql_mode|= MODE_STRICT_ALL_TABLES;
    thd->push_internal_handler(&strict_handler);
    disable_strict_mode= true;
  }

  /* Validate the Item tree. */
  status= fix_fields_gcol_func(thd, field);

  // Permanent changes to the item_tree are completed.
  if (!thd->lex->is_ps_or_view_context_analysis())
    field->gcol_info->permanent_changes_completed= true;

  if (disable_strict_mode)
  {
    thd->pop_internal_handler();
    thd->variables.sql_mode&= ~MODE_STRICT_ALL_TABLES;
  }
  if (status)
  {
    if (is_create_table)
    {
      /*
        During CREATE/ALTER TABLE it is ok to receive errors here.
        It is not ok if it happens during the opening of an frm
        file as part of a normal query.
      */
      *error_reported= TRUE;
    }
    // Any memory allocated in this function is freed in parse_err
    field->gcol_info= 0;
    goto parse_err;
  }
  if (field->gcol_info->register_base_columns(table))
    goto parse_err;
  thd->stmt_arena= backup_stmt_arena_ptr;
  thd->restore_active_arena(&gcol_arena, &backup_arena);
  field->gcol_info->item_free_list= gcol_arena.free_list;
  thd->want_privilege= save_old_privilege;
  thd->lex->expr_allows_subselect= save_allow_subselects;

  DBUG_RETURN(FALSE);

parse_err:
  thd->lex->parse_gcol_expr= FALSE;
  thd->free_items();
  thd->stmt_arena= backup_stmt_arena_ptr;
  thd->restore_active_arena(&gcol_arena, &backup_arena);
  thd->variables.character_set_client= old_character_set_client;
  thd->want_privilege= save_old_privilege;
  thd->lex->expr_allows_subselect= save_allow_subselects;
  DBUG_RETURN(TRUE);
}

/*
  Read data from a binary .frm file from MySQL 3.23 - 5.0 into TABLE_SHARE
*/

/*
  Open a table based on a TABLE_SHARE

  SYNOPSIS
    open_table_from_share()
    thd			Thread handler
    share		Table definition
    alias       	Alias for table
    db_stat		open flags (for example HA_OPEN_KEYFILE|
    			HA_OPEN_RNDFILE..) can be 0 (example in
                        ha_example_table)
    prgflag   		READ_ALL etc..
    ha_open_flags	HA_OPEN_ABORT_IF_LOCKED etc..
    outparam       	result table
    is_create_table     Indicates that table is opened as part
                        of CREATE or ALTER and does not yet exist in SE

  RETURN VALUES
   0	ok
   1	Error (see open_table_error)
   2    Error (see open_table_error)
   3    Wrong data in .frm file
   4    Error (see open_table_error)
   5    Error (see open_table_error: charset unavailable)
   7    Table definition has changed in engine
*/

int open_table_from_share(THD *thd, TABLE_SHARE *share, const char *alias,
                          uint db_stat, uint prgflag, uint ha_open_flags,
                          TABLE *outparam, bool is_create_table)
{
  int error;
  uint records, i, bitmap_size;
  bool error_reported= FALSE;
  uchar *record, *bitmaps;
  Field **field_ptr, **vfield_ptr= NULL;
  Field *fts_doc_id_field = NULL;
  DBUG_ENTER("open_table_from_share");
  DBUG_PRINT("enter",("name: '%s.%s'  form: 0x%lx", share->db.str,
                      share->table_name.str, (long) outparam));

  error= 1;
  new (outparam) TABLE;
  outparam->in_use= thd;
  outparam->s= share;
  outparam->db_stat= db_stat;
  outparam->write_row_record= NULL;

  init_sql_alloc(key_memory_TABLE,
                 &outparam->mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  if (!(outparam->alias= my_strdup(key_memory_TABLE,
                                   alias, MYF(MY_WME))))
    goto err;
  outparam->quick_keys.init();
  outparam->possible_quick_keys.init();
  outparam->covering_keys.init();
  outparam->merge_keys.init();
  outparam->keys_in_use_for_query.init();

  /* Allocate handler */
  outparam->file= 0;
  if (!(prgflag & OPEN_FRM_FILE_ONLY))
  {
    if (!(outparam->file= get_new_handler(share, &outparam->mem_root,
                                          share->db_type())))
      goto err;
    if (outparam->file->set_ha_share_ref(&share->ha_share))
      goto err;
  }
  else
  {
    DBUG_ASSERT(!db_stat);
  }

  error= 4;
  outparam->reginfo.lock_type= TL_UNLOCK;
  outparam->current_lock= F_UNLCK;
  records=0;
  if ((db_stat & HA_OPEN_KEYFILE) || (prgflag & DELAYED_OPEN))
    records=1;
  if (prgflag & (READ_ALL+EXTRA_RECORD))
    records++;

  if (!(record= (uchar*) alloc_root(&outparam->mem_root,
                                   share->rec_buff_length * records)))
    goto err;                                   /* purecov: inspected */

  if (records == 0)
  {
    /* We are probably in hard repair, and the buffers should not be used */
    outparam->record[0]= outparam->record[1]= share->default_values;
  }
  else
  {
    outparam->record[0]= record;
    if (records > 1)
      outparam->record[1]= record+ share->rec_buff_length;
    else
      outparam->record[1]= outparam->record[0];   // Safety
  }

  if (!(field_ptr = (Field **) alloc_root(&outparam->mem_root,
                                          (uint) ((share->fields+1)*
                                                  sizeof(Field*)))))
    goto err;                                   /* purecov: inspected */

  outparam->field= field_ptr;

  record= outparam->record[0]-1;	/* Fieldstart = 1 */
  if (share->null_field_first)
    outparam->null_flags= record+1;
  else
    outparam->null_flags= (record+ 1+ share->reclength -
                           share->null_bytes);

  /* Setup copy of fields from share, but use the right alias and record */
  for (i=0 ; i < share->fields; i++, field_ptr++)
  {
    Field *new_field= share->field[i]->clone(&outparam->mem_root);
    *field_ptr= new_field;
    if (new_field == NULL)
      goto err;
    new_field->init(outparam);
    new_field->move_field_offset((my_ptrdiff_t) (outparam->record[0] -
                                                 outparam->s->default_values));
    /* Check if FTS_DOC_ID column is present in the table */
    if (outparam->file &&
        (outparam->file->ha_table_flags() & HA_CAN_FULLTEXT_EXT) &&
        !strcmp(outparam->field[i]->field_name, FTS_DOC_ID_COL_NAME))
      fts_doc_id_field= new_field;
  }
  (*field_ptr)= 0;                              // End marker

  if (share->found_next_number_field)
    outparam->found_next_number_field=
      outparam->field[(uint) (share->found_next_number_field - share->field)];

  /* Fix key->name and key_part->field */
  if (share->key_parts)
  {
    KEY	*key_info, *key_info_end;
    KEY_PART_INFO *key_part;
    uint n_length;
    n_length= share->keys * sizeof(KEY) +
      share->key_parts * sizeof(KEY_PART_INFO);

    if (!(key_info= (KEY*) alloc_root(&outparam->mem_root, n_length)))
      goto err;
    outparam->key_info= key_info;
    key_part= (reinterpret_cast<KEY_PART_INFO*>(key_info+share->keys));

    memcpy(key_info, share->key_info, sizeof(*key_info)*share->keys);
    memcpy(key_part, share->key_info[0].key_part, (sizeof(*key_part) *
                                                   share->key_parts));

    for (key_info_end= key_info + share->keys ;
         key_info < key_info_end ;
         key_info++)
    {
      KEY_PART_INFO *key_part_end;

      key_info->table= outparam;
      key_info->key_part= key_part;

      for (key_part_end= key_part + key_info->actual_key_parts ;
           key_part < key_part_end ;
           key_part++)
      {
        Field *field= key_part->field= outparam->field[key_part->fieldnr-1];

        if (field->key_length() != key_part->length &&
            !(field->flags & BLOB_FLAG))
        {
          /*
            We are using only a prefix of the column as a key:
            Create a new field for the key part that matches the index
          */
          field= key_part->field=field->new_field(&outparam->mem_root,
                                                  outparam, 0);
          field->field_length= key_part->length;
        }
      }
      /* Skip unused key parts if they exist */
      key_part+= key_info->unused_key_parts;
      
      /* Set TABLE::fts_doc_id_field for tables with FT KEY */
      if ((key_info->flags & HA_FULLTEXT))
        outparam->fts_doc_id_field= fts_doc_id_field;
    }
  }

  if (share->partition_info_str_len && outparam->file)
  {
  /*
    In this execution we must avoid calling thd->change_item_tree since
    we might release memory before statement is completed. We do this
    by changing to a new statement arena. As part of this arena we also
    set the memory root to be the memory root of the table since we
    call the parser and fix_fields which both can allocate memory for
    item objects. We keep the arena to ensure that we can release the
    free_list when closing the table object.
    SEE Bug #21658
  */

    Query_arena *backup_stmt_arena_ptr= thd->stmt_arena;
    Query_arena backup_arena;
    Query_arena part_func_arena(&outparam->mem_root,
                                Query_arena::STMT_INITIALIZED);
    thd->set_n_backup_active_arena(&part_func_arena, &backup_arena);
    thd->stmt_arena= &part_func_arena;
    bool tmp;
    bool work_part_info_used;

    tmp= mysql_unpack_partition(thd, share->partition_info_str,
                                share->partition_info_str_len,
                                outparam, is_create_table,
                                share->default_part_db_type,
                                &work_part_info_used);
    if (tmp)
    {
      thd->stmt_arena= backup_stmt_arena_ptr;
      thd->restore_active_arena(&part_func_arena, &backup_arena);
      goto partititon_err;
    }
    outparam->part_info->is_auto_partitioned= share->auto_partitioned;
    DBUG_PRINT("info", ("autopartitioned: %u", share->auto_partitioned));
    /*
      We should perform the fix_partition_func in either local or
      caller's arena depending on work_part_info_used value.
    */
    if (!work_part_info_used)
      tmp= fix_partition_func(thd, outparam, is_create_table);
    thd->stmt_arena= backup_stmt_arena_ptr;
    thd->restore_active_arena(&part_func_arena, &backup_arena);
    if (!tmp)
    {
      if (work_part_info_used)
        tmp= fix_partition_func(thd, outparam, is_create_table);
    }
    outparam->part_info->item_free_list= part_func_arena.free_list;
partititon_err:
    if (tmp)
    {
      if (is_create_table)
      {
        /*
          During CREATE/ALTER TABLE it is ok to receive errors here.
          It is not ok if it happens during the opening of an frm
          file as part of a normal query.
        */
        error_reported= TRUE;
      }
      goto err;
    }
  }
  /* Check generated columns against table's storage engine. */
  if (share->vfields && outparam->file &&
      !(outparam->file->ha_table_flags() & HA_GENERATED_COLUMNS))
  {
    my_error(ER_UNSUPPORTED_ACTION_ON_GENERATED_COLUMN,
             MYF(0),
             "Specified storage engine");
    error_reported= TRUE;
    goto err;
  }

  /*
    Allocate bitmaps
    This needs to be done prior to generated columns as they'll call
    fix_fields and functions might want to access bitmaps.
  */

  bitmap_size= share->column_bitmap_size;
  if (!(bitmaps= (uchar*) alloc_root(&outparam->mem_root, bitmap_size * 5)))
    goto err;
  bitmap_init(&outparam->def_read_set,
              (my_bitmap_map*) bitmaps, share->fields, FALSE);
  bitmap_init(&outparam->def_write_set,
              (my_bitmap_map*) (bitmaps+bitmap_size), share->fields, FALSE);
  bitmap_init(&outparam->tmp_set,
              (my_bitmap_map*) (bitmaps+bitmap_size*2), share->fields, FALSE);
  bitmap_init(&outparam->cond_set,
              (my_bitmap_map*) (bitmaps+bitmap_size*3), share->fields, FALSE);
  bitmap_init(&outparam->def_fields_set_during_insert,
              (my_bitmap_map*) (bitmaps + bitmap_size * 4), share->fields,
              FALSE);
  outparam->default_column_bitmaps();

  /*
    Process generated columns, if any.
  */
  outparam->vfield= vfield_ptr;
  if (share->vfields)
  {
    if (!(vfield_ptr = (Field **) alloc_root(&outparam->mem_root,
                                             (uint) ((share->vfields+1)*
                                                     sizeof(Field*)))))
      goto err;

    outparam->vfield= vfield_ptr;

    for (field_ptr= outparam->field; *field_ptr; field_ptr++)
    {
      if ((*field_ptr)->gcol_info)
      {
        if (unpack_gcol_info_from_frm(thd,
                                      outparam,
                                      *field_ptr,
                                      is_create_table,
                                      &error_reported))
        {
          *vfield_ptr= NULL;
          error= 4; // in case no error is reported
          goto err;
        }
        *(vfield_ptr++)= *field_ptr;
      }
    }
    *vfield_ptr= 0;                              // End marker
  }
  /* The table struct is now initialized;  Open the table */
  error= 2;
  if (db_stat)
  {
    int ha_err;
    if ((ha_err= (outparam->file->
                  ha_open(outparam, share->normalized_path.str,
                          (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
                          (db_stat & HA_OPEN_TEMPORARY ? HA_OPEN_TMP_TABLE :
                           (db_stat & HA_WAIT_IF_LOCKED) ?
                           HA_OPEN_WAIT_IF_LOCKED :
                           (db_stat & (HA_ABORT_IF_LOCKED | HA_GET_INFO)) ?
                          HA_OPEN_ABORT_IF_LOCKED :
                           HA_OPEN_IGNORE_IF_LOCKED) | ha_open_flags))))
    {
      /* Set a flag if the table is crashed and it can be auto. repaired */
      share->crashed= ((ha_err == HA_ERR_CRASHED_ON_USAGE) &&
                       outparam->file->auto_repair() &&
                       !(ha_open_flags & HA_OPEN_FOR_REPAIR));

      switch (ha_err)
      {
	case HA_ERR_TABLESPACE_MISSING:
          /*
            In case of Innodb table space header may be corrupted or
	    ibd file might be missing
          */
          error= 1;
          DBUG_ASSERT(my_errno() == HA_ERR_TABLESPACE_MISSING);
          break;
        case HA_ERR_NO_SUCH_TABLE:
	  /*
            The table did not exists in storage engine, use same error message
            as if the .frm file didn't exist
          */
	  error= 1;
	  set_my_errno(ENOENT);
          break;
        case EMFILE:
	  /*
            Too many files opened, use same error message as if the .frm
            file can't open
           */
          DBUG_PRINT("error", ("open file: %s failed, too many files opened (errno: %d)", 
		  share->normalized_path.str, ha_err));
	  error= 1;
	  set_my_errno(EMFILE);
          break;
        default:
          outparam->file->print_error(ha_err, MYF(0));
          error_reported= TRUE;
          if (ha_err == HA_ERR_TABLE_DEF_CHANGED)
            error= 7;
          break;
      }
      goto err;                                 /* purecov: inspected */
    }
  }

  if ((share->table_category == TABLE_CATEGORY_LOG) ||
      (share->table_category == TABLE_CATEGORY_RPL_INFO) ||
      (share->table_category == TABLE_CATEGORY_GTID))
  {
    outparam->no_replicate= TRUE;
  }
  else if (outparam->file)
  {
    handler::Table_flags flags= outparam->file->ha_table_flags();
    outparam->no_replicate= ! MY_TEST(flags & (HA_BINLOG_STMT_CAPABLE
                                               | HA_BINLOG_ROW_CAPABLE))
                            || MY_TEST(flags & HA_HAS_OWN_BINLOGGING);
  }
  else
  {
    outparam->no_replicate= FALSE;
  }

  /* Increment the opened_tables counter, only when open flags set. */
  if (db_stat)
    thd->status_var.opened_tables++;

  DBUG_RETURN (0);

 err:
  if (! error_reported)
    open_table_error(share, error, my_errno(), 0);
  delete outparam->file;
  if (outparam->part_info)
    free_items(outparam->part_info->item_free_list);
  if (outparam->vfield)
  {
    for (Field **vfield= outparam->vfield; *vfield; vfield++)
      free_items((*vfield)->gcol_info->item_free_list);
  }
  outparam->file= 0;				// For easier error checking
  outparam->db_stat=0;
  free_root(&outparam->mem_root, MYF(0));
  my_free((void *) outparam->alias);
  DBUG_RETURN (error);
}


/*
  Free information allocated by openfrm

  SYNOPSIS
    closefrm()
    table		TABLE object to free
    free_share		Is 1 if we also want to free table_share
*/

int closefrm(TABLE *table, bool free_share)
{
  int error=0;
  DBUG_ENTER("closefrm");
  DBUG_PRINT("enter", ("table: 0x%lx", (long) table));

  if (table->db_stat)
    error=table->file->ha_close();
  my_free((void *) table->alias);
  table->alias= 0;
  if (table->field)
  {
    for (Field **ptr=table->field ; *ptr ; ptr++)
    {
      if ((*ptr)->gcol_info)
        free_items((*ptr)->gcol_info->item_free_list);
      delete *ptr;
    }
    table->field= 0;
  }
  delete table->file;
  table->file= 0;				/* For easier errorchecking */
  if (table->part_info)
  {
    /* Allocated through table->mem_root, freed below */
    free_items(table->part_info->item_free_list);
    table->part_info->item_free_list= 0;
    table->part_info= 0;
  }
  if (free_share)
  {
    if (table->s->tmp_table == NO_TMP_TABLE)
      release_table_share(table->s);
    else
      free_table_share(table->s);
  }
  free_root(&table->mem_root, MYF(0));
  DBUG_RETURN(error);
}


/* Deallocate temporary blob storage */

void free_blobs(TABLE *table)
{
  uint *ptr, *end;
  for (ptr= table->s->blob_field, end=ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    /*
      Reduced TABLE objects which are used by row-based replication for
      type conversion might have some fields missing. Skip freeing BLOB
      buffers for such missing fields.
    */
    if (table->field[*ptr])
      ((Field_blob*) table->field[*ptr])->mem_free();
  }
}


/**
  Reclaims temporary blob storage which is bigger than a threshold.
  Resets blob pointer. Unsets m_keep_old_value.

  @param table A handle to the TABLE object containing blob fields
  @param size The threshold value.
*/

void free_blob_buffers_and_reset(TABLE *table, uint32 size)
{
  uint *ptr, *end;
  for (ptr= table->s->blob_field, end= ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    Field_blob *blob= down_cast<Field_blob*>(table->field[*ptr]);
    if (blob->get_field_buffer_size() > size)
      blob->mem_free();
    blob->reset();

    if (blob->is_virtual_gcol())
      blob->set_keep_old_value(false);
  }
}

/**
  Find where a form starts.

  @param head The start of the form file.

  @remark If formname is NULL then only formnames is read.

  @retval The form position.
*/

static ulong get_form_pos(File file, uchar *head)
{
  uchar *pos, *buf;
  uint names, length;
  ulong ret_value=0;
  DBUG_ENTER("get_form_pos");

  names= uint2korr(head+8);

  if (!(names= uint2korr(head+8)))
    DBUG_RETURN(0);

  length= uint2korr(head+4);

  mysql_file_seek(file, 64L, MY_SEEK_SET, MYF(0));

  if (!(buf= (uchar*) my_malloc(key_memory_frm_form_pos,
                                length+names*4, MYF(MY_WME))))
    DBUG_RETURN(0);

  if (mysql_file_read(file, buf, length+names*4, MYF(MY_NABP)))
  {
    my_free(buf);
    DBUG_RETURN(0);
  }

  pos= buf+length;
  ret_value= uint4korr(pos);

  my_free(buf);

  DBUG_RETURN(ret_value);
}


/*
  Read string from a file with malloc

  NOTES:
    We add an \0 at end of the read string to make reading of C strings easier
*/

int read_string(File file, uchar**to, size_t length)
{
  DBUG_ENTER("read_string");

  my_free(*to);
  if (!(*to= (uchar*) my_malloc(key_memory_frm_string,
                                length+1,MYF(MY_WME))) ||
      mysql_file_read(file, *to, length, MYF(MY_NABP)))
  {
     my_free(*to);                            /* purecov: inspected */
    *to= 0;                                   /* purecov: inspected */
    DBUG_RETURN(1);                           /* purecov: inspected */
  }
  *((char*) *to+length)= '\0';
  DBUG_RETURN (0);
} /* read_string */


	/* Add a new form to a form file */

ulong make_new_entry(File file, uchar *fileinfo, TYPELIB *formnames,
		     const char *newname)
{
  uint i,bufflength,maxlength,n_length,length,names;
  ulong endpos,newpos;
  uchar buff[IO_SIZE];
  uchar *pos;
  DBUG_ENTER("make_new_entry");

  length=(uint) strlen(newname)+1;
  n_length=uint2korr(fileinfo+4);
  maxlength=uint2korr(fileinfo+6);
  names=uint2korr(fileinfo+8);
  newpos=uint4korr(fileinfo+10);

  if (64+length+n_length+(names+1)*4 > maxlength)
  {						/* Expand file */
    newpos+=IO_SIZE;
    int4store(fileinfo+10,newpos);
    /* Copy from file-end */
    endpos= (ulong) mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
    bufflength= (uint) (endpos & (IO_SIZE-1));	/* IO_SIZE is a power of 2 */

    while (endpos > maxlength)
    {
      mysql_file_seek(file, (ulong) (endpos-bufflength), MY_SEEK_SET, MYF(0));
      if (mysql_file_read(file, buff, bufflength, MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
      mysql_file_seek(file, (ulong) (endpos-bufflength+IO_SIZE), MY_SEEK_SET,
                      MYF(0));
      if ((mysql_file_write(file, buff, bufflength, MYF(MY_NABP+MY_WME))))
	DBUG_RETURN(0);
      endpos-=bufflength; bufflength=IO_SIZE;
    }
    memset(buff, 0, IO_SIZE);			/* Null new block */
    mysql_file_seek(file, (ulong) maxlength, MY_SEEK_SET, MYF(0));
    if (mysql_file_write(file, buff, bufflength, MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
    maxlength+=IO_SIZE;				/* Fix old ref */
    int2store(fileinfo+6,maxlength);
    for (i=names, pos= (uchar*) *formnames->type_names+n_length-1; i-- ;
	 pos+=4)
    {
      endpos=uint4korr(pos)+IO_SIZE;
      int4store(pos,endpos);
    }
  }

  if (n_length == 1 )
  {						/* First name */
    length++;
    (void) strxmov((char*) buff,"/",newname,"/",NullS);
  }
  else
    (void) strxmov((char*) buff,newname,"/",NullS); /* purecov: inspected */
  mysql_file_seek(file, 63L+(ulong) n_length, MY_SEEK_SET, MYF(0));
  if (mysql_file_write(file, buff, (size_t) length+1, MYF(MY_NABP+MY_WME)) ||
      (names && mysql_file_write(file,
                                 (uchar*) (*formnames->type_names+n_length-1),
                                 names*4, MYF(MY_NABP+MY_WME))) ||
      mysql_file_write(file, fileinfo+10, 4, MYF(MY_NABP+MY_WME)))
    DBUG_RETURN(0L); /* purecov: inspected */

  int2store(fileinfo+8,names+1);
  int2store(fileinfo+4,n_length+length);
  (void) mysql_file_chsize(file, newpos, 0, MYF(MY_WME));/* Append file with '\0' */
  DBUG_RETURN(newpos);
} /* make_new_entry */


	/* error message when opening a form file */

void open_table_error(TABLE_SHARE *share, int error, int db_errno, int errarg)
{
  int err_no;
  char buff[FN_REFLEN];
  char errbuf[MYSYS_STRERROR_SIZE];
  myf errortype= ME_ERRORLOG;
  DBUG_ENTER("open_table_error");

  switch (error) {
  case 7:
  case 1:
    switch (db_errno) {
    case ENOENT:
      my_error(ER_NO_SUCH_TABLE, MYF(0), share->db.str, share->table_name.str);
      break;
    case HA_ERR_TABLESPACE_MISSING:
      my_snprintf(errbuf, MYSYS_STRERROR_SIZE, "`%s`.`%s`", share->db.str,
                  share->table_name.str);
      my_error(ER_TABLESPACE_MISSING, MYF(0), errbuf);
      break;
    default:
      strxmov(buff, share->normalized_path.str, reg_ext, NullS);
      my_error((db_errno == EMFILE) ? ER_CANT_OPEN_FILE : ER_FILE_NOT_FOUND,
               errortype, buff,
               db_errno, my_strerror(errbuf, sizeof(errbuf), db_errno));
    }
    break;
  case 2:
  {
    handler *file= 0;
    const char *datext= "";

    if (share->db_type() != NULL)
    {
      if ((file= get_new_handler(share, current_thd->mem_root,
                                 share->db_type())))
      {
        if (!(datext= *file->bas_ext()))
          datext= "";
      }
    }
    err_no= (db_errno == ENOENT) ? ER_FILE_NOT_FOUND : (db_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    strxmov(buff, share->normalized_path.str, datext, NullS);
    my_error(err_no,errortype, buff,
             db_errno, my_strerror(errbuf, sizeof(errbuf), db_errno));
    delete file;
    break;
  }
  case 5:
  {
    const char *csname= get_charset_name((uint) errarg);
    char tmp[10];
    if (!csname || csname[0] =='?')
    {
      my_snprintf(tmp, sizeof(tmp), "#%d", errarg);
      csname= tmp;
    }
    my_printf_error(ER_UNKNOWN_COLLATION,
                    "Unknown collation '%s' in table '%-.64s' definition", 
                    MYF(0), csname, share->table_name.str);
    break;
  }
  case 6:
    strxmov(buff, share->normalized_path.str, reg_ext, NullS);
    my_printf_error(ER_NOT_FORM_FILE,
                    "Table '%-.64s' was created with a different version "
                    "of MySQL and cannot be read", 
                    MYF(0), buff);
    break;
  case 8:
    break;
  case 9:
    /* Unknown FRM type read while preparing File_parser object for view*/
    my_error(ER_FRM_UNKNOWN_TYPE, MYF(0), share->path.str,
             share->view_def->type()->str);
    break;
  default:				/* Better wrong error than none */
  case 4:
    strxmov(buff, share->normalized_path.str, reg_ext, NullS);
    my_error(ER_NOT_FORM_FILE, errortype, buff);
    break;
  }
  DBUG_VOID_RETURN;
} /* open_table_error */


	/*
	** fix a str_type to a array type
	** typeparts separated with some char. differents types are separated
	** with a '\0'
	*/

static void
fix_type_pointers(const char ***array, TYPELIB *point_to_type, uint types,
		  char **names)
{
  char *type_name, *ptr;
  char chr;

  ptr= *names;
  while (types--)
  {
    point_to_type->name=0;
    point_to_type->type_names= *array;

    if ((chr= *ptr))			/* Test if empty type */
    {
      while ((type_name=strchr(ptr+1,chr)) != NullS)
      {
	*((*array)++) = ptr+1;
	*type_name= '\0';		/* End string */
	ptr=type_name;
      }
      ptr+=2;				/* Skip end mark and last 0 */
    }
    else
      ptr++;
    point_to_type->count= (uint) (*array - point_to_type->type_names);
    point_to_type++;
    *((*array)++)= NullS;		/* End of type */
  }
  *names=ptr;				/* Update end */
  return;
} /* fix_type_pointers */


TYPELIB *typelib(MEM_ROOT *mem_root, List<String> &strings)
{
  TYPELIB *result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
  if (!result)
    return 0;
  result->count=strings.elements;
  result->name="";
  uint nbytes= (sizeof(char*) + sizeof(uint)) * (result->count + 1);
  if (!(result->type_names= (const char**) alloc_root(mem_root, nbytes)))
    return 0;
  result->type_lengths= (uint*) (result->type_names + result->count + 1);
  List_iterator<String> it(strings);
  String *tmp;
  for (uint i=0; (tmp=it++) ; i++)
  {
    result->type_names[i]= tmp->ptr();
    result->type_lengths[i]= tmp->length();
  }
  result->type_names[result->count]= 0;		// End marker
  result->type_lengths[result->count]= 0;
  return result;
}


/*
 Search after a field with given start & length
 If an exact field isn't found, return longest field with starts
 at right position.
 
 NOTES
   This is needed because in some .frm fields 'fieldnr' was saved wrong

 RETURN
   0  error
   #  field number +1
*/

static uint find_field(Field **fields, uchar *record, uint start, uint length)
{
  Field **field;
  uint i, pos;

  pos= 0;
  for (field= fields, i=1 ; *field ; i++,field++)
  {
    if ((*field)->offset(record) == start)
    {
      if ((*field)->key_length() == length)
	return (i);
      if (!pos || fields[pos-1]->pack_length() <
	  (*field)->pack_length())
	pos= i;
    }
  }
  return (pos);
}


	/* Check that the integer is in the internal */

int set_zone(int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */

	/* Adjust number to next larger disk buffer */

ulong next_io_size(ulong pos)
{
  ulong offset;
  if ((offset= pos & (IO_SIZE-1)))
    return pos-offset+IO_SIZE;
  return pos;
} /* next_io_size */


/*
  Store an SQL quoted string.

  SYNOPSIS  
    append_unescaped()
    res		result String
    pos		string to be quoted
    length	it's length

  NOTE
    This function works correctly with utf8 or single-byte charset strings.
    May fail with some multibyte charsets though.
*/

void append_unescaped(String *res, const char *pos, size_t length)
{
  const char *end= pos+length;
  res->append('\'');

  for (; pos != end ; pos++)
  {
    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->append('\\');
      res->append('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->append('\\');
      res->append('n');
      break;
    case '\r':
      res->append('\\');		/* This gives better readability */
      res->append('r');
      break;
    case '\\':
      res->append('\\');		/* Because of the sql syntax */
      res->append('\\');
      break;
    case '\'':
      res->append('\'');		/* Because of the sql syntax */
      res->append('\'');
      break;
    default:
      res->append(*pos);
      break;
    }
  }
  res->append('\'');
}


	/* Create a .frm file */

File create_frm(THD *thd, const char *name, const char *db,
                const char *table, uint reclength, uchar *fileinfo,
  		HA_CREATE_INFO *create_info, uint keys, KEY *key_info)
{
  File file;
  ulong length;
  uchar fill[IO_SIZE];
  int create_flags= O_RDWR | O_TRUNC;
  size_t key_comment_total_bytes= 0;
  uint i;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= O_EXCL | O_NOFOLLOW;

  /* Fix this when we have new .frm files;  Current limit is 4G rows (QQ) */
  if (create_info->max_rows > UINT_MAX32)
    create_info->max_rows= UINT_MAX32;
  if (create_info->min_rows > UINT_MAX32)
    create_info->min_rows= UINT_MAX32;

  if ((file= mysql_file_create(key_file_frm,
                               name, CREATE_MODE, create_flags, MYF(0))) >= 0)
  {
    size_t key_length, tmp_key_length;
    uint tmp, csid;
    memset(fileinfo, 0, 64);
    /* header */
    fileinfo[0]=(uchar) 254;
    fileinfo[1]= 1;
    fileinfo[2]= FRM_VER+3+ MY_TEST(create_info->varchar);

    fileinfo[3]= (uchar) ha_legacy_type(
          ha_checktype(thd,ha_legacy_type(create_info->db_type),0,0));
    fileinfo[4]=1;
    int2store(fileinfo+6,IO_SIZE);		/* Next block starts here */
    /*
      Keep in sync with pack_keys() in unireg.cc
      For each key:
      8 bytes for the key header
      9 bytes for each key-part (MAX_REF_PARTS)
      NAME_LEN bytes for the name
      1 byte for the NAMES_SEP_CHAR (before the name)
      For all keys:
      6 bytes for the header
      1 byte for the NAMES_SEP_CHAR (after the last name)
      9 extra bytes (padding for safety? alignment?)
    */
    for (i= 0; i < keys; i++)
    {
      DBUG_ASSERT(MY_TEST(key_info[i].flags & HA_USES_COMMENT) ==
                 (key_info[i].comment.length > 0));
      if (key_info[i].flags & HA_USES_COMMENT)
        key_comment_total_bytes += 2 + key_info[i].comment.length;
    }

    key_length= keys * (8 + MAX_REF_PARTS * 9 + NAME_LEN + 1) + 16
                + key_comment_total_bytes;

    length= next_io_size((ulong) (IO_SIZE+key_length+reclength+
                                  create_info->extra_size));
    int4store(fileinfo+10,length);
    tmp_key_length= (key_length < 0xffff) ? key_length : 0xffff;
    int2store(fileinfo+14, static_cast<uint16>(tmp_key_length));
    int2store(fileinfo+16,reclength);
    int4store(fileinfo+18, static_cast<uint32>(create_info->max_rows));
    int4store(fileinfo+22, static_cast<uint32>(create_info->min_rows));
    /* fileinfo[26] is set in mysql_create_frm() */
    fileinfo[27]=2;				// Use long pack-fields
    /* fileinfo[28 & 29] is set to key_info_length in mysql_create_frm() */
    create_info->table_options|=HA_OPTION_LONG_BLOB_PTR; // Use portable blob pointers
    int2store(fileinfo+30, static_cast<uint16>(create_info->table_options));
    fileinfo[32]=0;				// No filename anymore
    fileinfo[33]=5;                             // Mark for 5.0 frm file
    int4store(fileinfo+34,create_info->avg_row_length);
    csid= (create_info->default_table_charset ?
           create_info->default_table_charset->number : 0);
    fileinfo[38]= (uchar) csid;
    /*
      In future versions, we will store in fileinfo[39] the values of the
      TRANSACTIONAL and PAGE_CHECKSUM clauses of CREATE TABLE.
    */
    fileinfo[39]= 0;
    fileinfo[40]= (uchar) create_info->row_type;
    /* Bytes 41-46 were for RAID support; now reused for other purposes */
    fileinfo[41]= (uchar) (csid >> 8);
    int2store(fileinfo+42, create_info->stats_sample_pages & 0xffff);
    fileinfo[44]= (uchar) create_info->stats_auto_recalc;
    fileinfo[45]= 0;
    fileinfo[46]= 0;
    int4store(fileinfo+47, static_cast<uint32>(key_length));
    tmp= MYSQL_VERSION_ID;          // Store to avoid warning from int4store
    int4store(fileinfo+51, tmp);
    int4store(fileinfo+55, create_info->extra_size);
    /*
      59-60 is reserved for extra_rec_buf_length,
      61 for default_part_db_type
    */
    int2store(fileinfo+62, static_cast<uint16>(create_info->key_block_size));
    memset(fill, 0, IO_SIZE);
    for (; length > IO_SIZE ; length-= IO_SIZE)
    {
      if (mysql_file_write(file, fill, IO_SIZE, MYF(MY_WME | MY_NABP)))
      {
        (void) mysql_file_close(file, MYF(0));
        (void) mysql_file_delete(key_file_frm, name, MYF(0));
	return(-1);
      }
    }
  }
  else
  {
    if (my_errno() == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_TABLE,MYF(0),table,my_errno());
  }
  return (file);
} /* create_frm */


void update_create_info_from_table(HA_CREATE_INFO *create_info, TABLE *table)
{
  TABLE_SHARE *share= table->s;
  DBUG_ENTER("update_create_info_from_table");

  create_info->max_rows= share->max_rows;
  create_info->min_rows= share->min_rows;
  create_info->table_options= share->db_create_options;
  create_info->avg_row_length= share->avg_row_length;
  create_info->row_type= share->row_type;
  create_info->default_table_charset= share->table_charset;
  create_info->table_charset= 0;
  create_info->comment= share->comment;
  create_info->storage_media= share->default_storage_media;
  create_info->tablespace= share->tablespace;

  DBUG_VOID_RETURN;
}

int
rename_file_ext(const char * from,const char * to,const char * ext)
{
  char from_b[FN_REFLEN],to_b[FN_REFLEN];
  (void) strxmov(from_b,from,ext,NullS);
  (void) strxmov(to_b,to,ext,NullS);
  return (mysql_file_rename(key_file_frm, from_b, to_b, MYF(MY_WME)));
}


/*
  Allocate string field in MEM_ROOT and return it as String

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string
    res         result String

  RETURN VALUES
    1   string is empty
    0	all ok
*/

bool get_field(MEM_ROOT *mem, Field *field, String *res)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  size_t length;

  field->val_str(&str);
  if (!(length= str.length()))
  {
    res->length(0);
    return 1;
  }
  if (!(to= strmake_root(mem, str.ptr(), length)))
    length= 0;                                  // Safety fix
  res->set(to, length, field->charset());
  return 0;
}


/*
  Allocate string field in MEM_ROOT and return it as NULL-terminated string

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string

  RETURN VALUES
    NullS  string is empty
    #      pointer to NULL-terminated string value of field
*/

char *get_field(MEM_ROOT *mem, Field *field)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  size_t length;

  field->val_str(&str);
  length= str.length();
  if (!length || !(to= (char*) alloc_root(mem,length+1)))
    return NullS;
  memcpy(to,str.ptr(), length);
  to[length]=0;
  return to;
}


/**
  Check if database name is valid

  @param org_name             Name of database and length
  @param preserve_lettercase  Preserve lettercase if true

  @note If lower_case_table_names is true and preserve_lettercase
  is false then database is converted to lower case

  @retval  IDENT_NAME_OK        Identifier name is Ok (Success)
  @retval  IDENT_NAME_WRONG     Identifier name is Wrong (ER_WRONG_TABLE_NAME)
  @retval  IDENT_NAME_TOO_LONG  Identifier name is too long if it is greater
                                than 64 characters (ER_TOO_LONG_IDENT)

  @note In case of IDENT_NAME_WRONG and IDENT_NAME_TOO_LONG, this
  function reports an error (my_error)
*/

enum_ident_name_check check_and_convert_db_name(LEX_STRING *org_name,
                                                bool preserve_lettercase)
{
  char *name= org_name->str;
  size_t name_length= org_name->length;
  bool check_for_path_chars;
  enum_ident_name_check ident_check_status;

  if (!name_length || name_length > NAME_LEN)
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), org_name->str);
    return IDENT_NAME_WRONG;
  }

  if ((check_for_path_chars= check_mysql50_prefix(name)))
  {
    name+= MYSQL50_TABLE_NAME_PREFIX_LENGTH;
    name_length-= MYSQL50_TABLE_NAME_PREFIX_LENGTH;
  }

  if (!preserve_lettercase && lower_case_table_names && name != any_db)
    my_casedn_str(files_charset_info, name);

  ident_check_status= check_table_name(name, name_length, check_for_path_chars);
  if (ident_check_status == IDENT_NAME_WRONG)
    my_error(ER_WRONG_DB_NAME, MYF(0), org_name->str);
  else if (ident_check_status == IDENT_NAME_TOO_LONG)
    my_error(ER_TOO_LONG_IDENT, MYF(0), org_name->str);
  return ident_check_status;
}


/**
  Function to check if table name is valid or not. If it is invalid,
  return appropriate error in each case to the caller.

  @param name                  Table name
  @param length                Length of table name
  @param check_for_path_chars  Check if the table name contains path chars

  @retval  IDENT_NAME_OK        Identifier name is Ok (Success)
  @retval  IDENT_NAME_WRONG     Identifier name is Wrong (ER_WRONG_TABLE_NAME)
  @retval  IDENT_NAME_TOO_LONG  Identifier name is too long if it is greater
                                than 64 characters (ER_TOO_LONG_IDENT)

  @note Reporting error to the user is the responsiblity of the caller.
*/

enum_ident_name_check check_table_name(const char *name, size_t length,
                                       bool check_for_path_chars)
{
  // name length in symbols
  size_t name_length= 0;
  const char *end= name+length;
  if (!length || length > NAME_LEN)
    return IDENT_NAME_WRONG;
  bool last_char_is_space= FALSE;

  while (name != end)
  {
    last_char_is_space= my_isspace(system_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, end);
      if (len)
      {
        name += len;
        name_length++;
        continue;
      }
    }
    if (check_for_path_chars &&
        (*name == '/' || *name == '\\' || *name == '~' || *name == FN_EXTCHAR))
      return IDENT_NAME_WRONG;
    name++;
    name_length++;
  }
  if (last_char_is_space)
   return IDENT_NAME_WRONG;
  else if (name_length > NAME_CHAR_LEN)
   return IDENT_NAME_TOO_LONG;
  return IDENT_NAME_OK;
}


bool check_column_name(const char *name)
{
  // name length in symbols
  size_t name_length= 0;
  bool last_char_is_space= TRUE;

  while (*name)
  {
    last_char_is_space= my_isspace(system_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, 
                          name+system_charset_info->mbmaxlen);
      if (len)
      {
        name += len;
        name_length++;
        continue;
      }
    }
    if (*name == NAMES_SEP_CHAR)
      return 1;
    name++;
    name_length++;
  }
  /* Error if empty or too long column name */
  return last_char_is_space || (name_length > NAME_CHAR_LEN);
}


/**
  Checks whether a table is intact. Should be done *just* after the table has
  been opened.

  @param[in] table             The table to check
  @param[in] table_def         Expected structure of the table (column name
                               and type)

  @retval  FALSE  OK
  @retval  TRUE   There was an error.
*/

bool
Table_check_intact::check(TABLE *table, const TABLE_FIELD_DEF *table_def)
{
  uint i;
  my_bool error= FALSE;
  const TABLE_FIELD_TYPE *field_def= table_def->field;
  DBUG_ENTER("table_check_intact");
  DBUG_PRINT("info",("table: %s  expected_count: %d",
                     table->alias, table_def->count));

  /* Whether the table definition has already been validated. */
  if (table->s->table_field_def_cache == table_def)
    goto end;

  if (table->s->fields != table_def->count)
  {
    DBUG_PRINT("info", ("Column count has changed, checking the definition"));

    /* previous MySQL version */
    if (MYSQL_VERSION_ID > table->s->mysql_version)
    {
      report_error(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2,
                   ER(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE_V2),
                   table->s->db.str, table->alias,
                   table_def->count, table->s->fields,
                   static_cast<int>(table->s->mysql_version),
                   MYSQL_VERSION_ID);
      DBUG_RETURN(TRUE);
    }
    else if (MYSQL_VERSION_ID == table->s->mysql_version)
    {
      report_error(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2,
                   ER(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2),
                   table->s->db.str, table->s->table_name.str,
                   table_def->count, table->s->fields);
      DBUG_RETURN(TRUE);
    }
    /*
      Something has definitely changed, but we're running an older
      version of MySQL with new system tables.
      Let's check column definitions. If a column was added at
      the end of the table, then we don't care much since such change
      is backward compatible.
    */
  }
  char buffer[STRING_BUFFER_USUAL_SIZE];
  for (i=0 ; i < table_def->count; i++, field_def++)
  {
    String sql_type(buffer, sizeof(buffer), system_charset_info);
    sql_type.length(0);
    if (i < table->s->fields)
    {
      Field *field= table->field[i];

      if (strncmp(field->field_name, field_def->name.str,
                  field_def->name.length))
      {
        /*
          Name changes are not fatal, we use ordinal numbers to access columns.
          Still this can be a sign of a tampered table, output an error
          to the error log.
        */
        report_error(0, "Incorrect definition of table %s.%s: "
                     "expected column '%s' at position %d, found '%s'.",
                     table->s->db.str, table->alias, field_def->name.str, i,
                     field->field_name);
      }
      field->sql_type(sql_type);
      /*
        Generally, if column types don't match, then something is
        wrong.

        However, we only compare column definitions up to the
        length of the original definition, since we consider the
        following definitions compatible:

        1. DATETIME and DATETIM
        2. INT(11) and INT(11
        3. SET('one', 'two') and SET('one', 'two', 'more')

        For SETs or ENUMs, if the same prefix is there it's OK to
        add more elements - they will get higher ordinal numbers and
        the new table definition is backward compatible with the
        original one.
       */
      if (strncmp(sql_type.c_ptr_safe(), field_def->type.str,
                  field_def->type.length - 1))
      {
        report_error(ER_CANNOT_LOAD_FROM_TABLE_V2, "Incorrect definition of "
                     "table %s.%s: expected column '%s' at position %d to "
                     "have type %s, found type %s.", table->s->db.str,
                     table->alias, field_def->name.str, i, field_def->type.str,
                     sql_type.c_ptr_safe());
        error= TRUE;
      }
      else if (field_def->cset.str && !field->has_charset())
      {
        report_error(ER_CANNOT_LOAD_FROM_TABLE_V2, "Incorrect definition of "
                     "table %s.%s: expected the type of column '%s' at "
                     "position %d to have character set '%s' but the type "
                     "has no character set.", table->s->db.str, table->alias,
                     field_def->name.str, i, field_def->cset.str);
        error= TRUE;
      }
      else if (field_def->cset.str &&
               strcmp(field->charset()->csname, field_def->cset.str))
      {
        report_error(ER_CANNOT_LOAD_FROM_TABLE_V2, "Incorrect definition of "
                     "table %s.%s: expected the type of column '%s' at "
                     "position %d to have character set '%s' but found "
                     "character set '%s'.", table->s->db.str, table->alias,
                     field_def->name.str, i, field_def->cset.str,
                     field->charset()->csname);
        error= TRUE;
      }
    }
    else
    {
      report_error(ER_CANNOT_LOAD_FROM_TABLE_V2, "Incorrect definition of "
                   "table %s.%s: expected column '%s' at position %d to "
                   "have type %s but the column is not found.",
                   table->s->db.str, table->alias,
                   field_def->name.str, i, field_def->type.str);
      error= TRUE;
    }
  }

  if (! error)
    table->s->table_field_def_cache= table_def;

end:

  if (has_keys && !error && !table->key_info)
  {
    my_error(ER_MISSING_KEY, MYF(0), table->s->db.str,
             table->s->table_name.str);
    error= TRUE;
  }

  DBUG_RETURN(error);
}


/**
  Traverse portion of wait-for graph which is reachable through edge
  represented by this flush ticket in search for deadlocks.

  @retval TRUE  A deadlock is found. A victim is remembered
                by the visitor.
  @retval FALSE Success, no deadlocks.
*/

bool Wait_for_flush::accept_visitor(MDL_wait_for_graph_visitor *gvisitor)
{
  return m_share->visit_subgraph(this, gvisitor);
}


uint Wait_for_flush::get_deadlock_weight() const
{
  return m_deadlock_weight;
}


/**
  Traverse portion of wait-for graph which is reachable through this
  table share in search for deadlocks.

  @param waiting_ticket  Ticket representing wait for this share.
  @param dvisitor        Deadlock detection visitor.

  @retval TRUE  A deadlock is found. A victim is remembered
                by the visitor.
  @retval FALSE No deadlocks, it's OK to begin wait.
*/

bool TABLE_SHARE::visit_subgraph(Wait_for_flush *wait_for_flush,
                                 MDL_wait_for_graph_visitor *gvisitor)
{
  TABLE *table;
  MDL_context *src_ctx= wait_for_flush->get_ctx();
  bool result= TRUE;
  bool locked= FALSE;

  /*
    To protect used_tables list from being concurrently modified
    while we are iterating through it we acquire LOCK_open.
    This does not introduce deadlocks in the deadlock detector
    because we won't try to acquire LOCK_open while
    holding a write-lock on MDL_lock::m_rwlock.
  */
  if (gvisitor->m_lock_open_count++ == 0)
  {
    locked= TRUE;
    table_cache_manager.lock_all_and_tdc();
  }

  Table_cache_iterator tables_it(this);

  /*
    In case of multiple searches running in parallel, avoid going
    over the same loop twice and shortcut the search.
    Do it after taking the lock to weed out unnecessary races.
  */
  if (src_ctx->m_wait.get_status() != MDL_wait::EMPTY)
  {
    result= FALSE;
    goto end;
  }

  if (gvisitor->enter_node(src_ctx))
    goto end;

  while ((table= tables_it++))
  {
    if (gvisitor->inspect_edge(&table->in_use->mdl_context))
    {
      goto end_leave_node;
    }
  }

  tables_it.rewind();
  while ((table= tables_it++))
  {
    if (table->in_use->mdl_context.visit_subgraph(gvisitor))
    {
      goto end_leave_node;
    }
  }

  result= FALSE;

end_leave_node:
  gvisitor->leave_node(src_ctx);

end:
  gvisitor->m_lock_open_count--;
  if (locked)
  {
    DBUG_ASSERT(gvisitor->m_lock_open_count == 0);
    table_cache_manager.unlock_all_and_tdc();
  }

  return result;
}


/**
  Wait until the subject share is removed from the table
  definition cache and make sure it's destroyed.

  @note This method may access the share concurrently with another
  thread if the share is in the process of being opened, i.e., that
  m_open_in_progress is true. In this case, close_cached_tables() may
  iterate over elements in the table definition cache, and call this
  method regardless of the share being opened or not. This works anyway
  since a new flush ticket is added below, and LOCK_open ensures
  that the share may not be destroyed by another thread in the time
  between finding this share (having an old version) and adding the flush
  ticket. Thus, after this thread has added the flush ticket, the thread
  opening the table will eventually call free_table_share (as a result of
  releasing the share after using it, or as a result of a failing
  open_table_def()), which will notify the owners of the flush tickets,
  and the last one being notified will actually destroy the share.

  @param mdl_context     MDL context for thread which is going to wait.
  @param abstime         Timeout for waiting as absolute time value.
  @param deadlock_weight Weight of this wait for deadlock detector.

  @pre LOCK_open is write locked, the share is used (has
       non-zero reference count), is marked for flush and
       this connection does not reference the share.
       LOCK_open will be unlocked temporarily during execution.

  @retval FALSE - Success.
  @retval TRUE  - Error (OOM, deadlock, timeout, etc...).
*/

bool TABLE_SHARE::wait_for_old_version(THD *thd, struct timespec *abstime,
                                       uint deadlock_weight)
{
  MDL_context *mdl_context= &thd->mdl_context;
  Wait_for_flush ticket(mdl_context, this, deadlock_weight);
  MDL_wait::enum_wait_status wait_status;

  mysql_mutex_assert_owner(&LOCK_open);
  /*
    We should enter this method only when share's version is not
    up to date and the share is referenced. Otherwise our
    thread will never be woken up from wait.
  */
  DBUG_ASSERT(version != refresh_version && ref_count != 0);

  m_flush_tickets.push_front(&ticket);

  mdl_context->m_wait.reset_status();

  mysql_mutex_unlock(&LOCK_open);

  mdl_context->will_wait_for(&ticket);

  mdl_context->find_deadlock();

  DEBUG_SYNC(thd, "flush_complete");

  wait_status= mdl_context->m_wait.timed_wait(thd, abstime, TRUE,
                                              &stage_waiting_for_table_flush);

  mdl_context->done_waiting_for();

  mysql_mutex_lock(&LOCK_open);

  m_flush_tickets.remove(&ticket);

  if (m_flush_tickets.is_empty() && ref_count == 0)
  {
    /*
      If our thread was the last one using the share,
      we must destroy it here.
    */
    destroy();
  }

  DEBUG_SYNC(thd, "share_destroyed");

  /*
    In cases when our wait was aborted by KILL statement,
    a deadlock or a timeout, the share might still be referenced,
    so we don't delete it. Note, that we can't determine this
    condition by checking wait_status alone, since, for example,
    a timeout can happen after all references to the table share
    were released, but before the share is removed from the
    cache and we receive the notification. This is why
    we first destroy the share, and then look at
    wait_status.
  */
  switch (wait_status)
  {
  case MDL_wait::GRANTED:
    return FALSE;
  case MDL_wait::VICTIM:
    my_error(ER_LOCK_DEADLOCK, MYF(0));
    return TRUE;
  case MDL_wait::TIMEOUT:
    my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    return TRUE;
  case MDL_wait::KILLED:
    return TRUE;
  default:
    DBUG_ASSERT(0);
    return TRUE;
  }
}


/**
  Initialize TABLE instance (newly created, or coming either from table
  cache or THD::temporary_tables list) and prepare it for further use
  during statement execution. Set the 'alias' attribute from the specified
  TABLE_LIST element. Remember the TABLE_LIST element in the
  TABLE::pos_in_table_list member.

  @param thd  Thread context.
  @param tl   TABLE_LIST element.
*/

void TABLE::init(THD *thd, TABLE_LIST *tl)
{
  DBUG_ASSERT(s->ref_count > 0 || s->tmp_table != NO_TMP_TABLE);

  if (thd->lex->need_correct_ident())
    alias_name_used= my_strcasecmp(table_alias_charset,
                                   s->table_name.str,
                                   tl->alias);
  /* Fix alias if table name changes. */
  if (strcmp(alias, tl->alias))
  {
    size_t length= strlen(tl->alias)+1;
    alias= (char*) my_realloc(key_memory_TABLE,
                              (char*) alias, length, MYF(MY_WME));
    memcpy((char*) alias, tl->alias, length);
  }

  const_table= 0;
  null_row= 0;
  nullable= 0;
  force_index= 0;
  force_index_order= 0;
  force_index_group= 0;
  status= STATUS_GARBAGE | STATUS_NOT_FOUND;
  insert_values= 0;
  fulltext_searched= 0;
  file->ft_handler= 0;
  reginfo.impossible_range= 0;

  /* Catch wrong handling of the auto_increment_field_not_null. */
  DBUG_ASSERT(!auto_increment_field_not_null);
  auto_increment_field_not_null= FALSE;

  pos_in_table_list= tl;

  clear_column_bitmaps();

  DBUG_ASSERT(key_read == 0);
  no_keyread= false;

  /* Tables may be reused in a sub statement. */
  DBUG_ASSERT(!file->extra(HA_EXTRA_IS_ATTACHED_CHILDREN));
  
  /*
    Do not call refix_gc_items() for tables which are not directly used by the
    statement (i.e. used by the substatements of routines or triggers to be
    invoked by the statement).

    Firstly, there will be call to refix_gc_items() at the start of execution
    of substatement which directly uses this table anyway.Secondly, cleanup of
    generated column (call to cleanup_gc_items()) for the table will be done
    only at the end of execution of substatement which uses it. Because of this
    call to refix_gc_items() for prelocking placeholder will miss corresponding
    call to cleanup_gc_items() if substatement which uses the table is not
    executed for some reason.
  */
  if (!pos_in_table_list->prelocking_placeholder)
  {
    bool error MY_ATTRIBUTE((unused))= refix_gc_items(thd);
    DBUG_ASSERT(!error);
  }
}


bool TABLE::refix_gc_items(THD *thd)
{
  if (vfield)
  {
    for (Field **vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
    {
      Field *vfield= *vfield_ptr;
      DBUG_ASSERT(vfield->gcol_info && vfield->gcol_info->expr_item);
      if (!vfield->gcol_info->expr_item->fixed)
      {
        bool res= false;
        /*
          The call to fix_fields_gcol_func() may create new item objects in the
          item tree for the generated column expression. If these are permanent
          changes to the item tree, the new items must have the same life-span
          as the ones created during parsing of the generated expression
          string. We achieve this by temporarily switching to use the TABLE's
          mem_root if the permanent changes to the item tree haven't been
          completed (by checking the status of
          gcol_info->permanent_changes_completed) and this call is not part of
          context analysis (like prepare or show create table).
        */
        Query_arena *backup_stmt_arena_ptr= thd->stmt_arena;
        Query_arena backup_arena;
        Query_arena gcol_arena(&vfield->table->mem_root,
                               Query_arena::STMT_CONVENTIONAL_EXECUTION);
        if (!vfield->gcol_info->permanent_changes_completed &&
            !thd->lex->is_ps_or_view_context_analysis())
        {
          thd->set_n_backup_active_arena(&gcol_arena, &backup_arena);
          thd->stmt_arena= &gcol_arena;
        }

        /* 
          Temporarily disable privileges check; already done when first fixed,
          and then based on definer's (owner's) rights: this thread has
          invoker's rights
        */
        ulong sav_want_priv= thd->want_privilege;
        thd->want_privilege= 0;

        if (fix_fields_gcol_func(thd, vfield))
          res= true;

        if (!vfield->gcol_info->permanent_changes_completed &&
            !thd->lex->is_ps_or_view_context_analysis())
        {
          // Switch back to the original stmt_arena.
          thd->stmt_arena= backup_stmt_arena_ptr;
          thd->restore_active_arena(&gcol_arena, &backup_arena);

          // Append the new items to the original item_free_list.
          Item *item= vfield->gcol_info->item_free_list;
          while (item->next)
            item= item->next;
          item->next= gcol_arena.free_list;

          // Permanent changes to the item_tree are completed.
          vfield->gcol_info->permanent_changes_completed= true;
        }

        // Restore any privileges check
        thd->want_privilege= sav_want_priv;
        get_fields_in_item_tree= FALSE;

        /* error occurs */
        if (res)
          return res;
      }
    }
  }
  return false;
}
  

void TABLE::cleanup_gc_items()
{
  if (!has_gcol())
    return;

  for (Field **vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
    cleanup_items((*vfield_ptr)->gcol_info->item_free_list);
}

/*
  Create Item_field for each column in the table.

  SYNPOSIS
    TABLE::fill_item_list()
      item_list          a pointer to an empty list used to store items

  DESCRIPTION
    Create Item_field object for each column in the table and
    initialize it with the corresponding Field. New items are
    created in the current THD memory root.

  RETURN VALUE
    0                    success
    1                    out of memory
*/

bool TABLE::fill_item_list(List<Item> *item_list) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  uint i= 0;
  for (Field **ptr= visible_field_ptr(); *ptr; ptr++, i++)
  {
    Item_field *item= new Item_field(*ptr);
    if (!item || item_list->push_back(item))
      return TRUE;
  }
  return FALSE;
}

/*
  Reset an existing list of Item_field items to point to the
  Fields of this table.

  SYNPOSIS
    TABLE::reset_item_list()
      item_list          a non-empty list with Item_fields

  DESCRIPTION
    This is a counterpart of fill_item_list used to redirect
    Item_fields to the fields of a newly created table.
*/

void TABLE::reset_item_list(List<Item> *item_list) const
{
  List_iterator_fast<Item> it(*item_list);
  uint i= 0;
  for (Field **ptr= visible_field_ptr(); *ptr; ptr++, i++)
  {
    Item_field *item_field= (Item_field*) it++;
    DBUG_ASSERT(item_field != 0);
    item_field->reset_field(*ptr);
  }
}

/**
  Create a TABLE_LIST object representing a nested join

  @param allocator  Mem root allocator that object is created from.
  @param alias      Name of nested join object
  @param embedding  Pointer to embedding join nest (or NULL if top-most)
  @param belongs_to List of tables this nest belongs to (never NULL).
  @param select     The query block that this join nest belongs within.

  @returns Pointer to created join nest object, or NULL if error.
*/

TABLE_LIST *TABLE_LIST::new_nested_join(MEM_ROOT *allocator,
                            const char *alias,
                            TABLE_LIST *embedding,
                            List<TABLE_LIST> *belongs_to,
                            class st_select_lex *select)
{
  DBUG_ASSERT(belongs_to && select);

  TABLE_LIST *const join_nest=
    (TABLE_LIST*) alloc_root(allocator, sizeof(TABLE_LIST));
  if (join_nest == NULL)
    return NULL;
  new (join_nest) TABLE_LIST;

  join_nest->nested_join=
    (NESTED_JOIN*) alloc_root(allocator, sizeof(NESTED_JOIN));
  if (join_nest->nested_join == NULL)
    return NULL;
  new (join_nest->nested_join) NESTED_JOIN;

  join_nest->db= (char *)"";
  join_nest->db_length= 0;
  join_nest->table_name= (char *)"";
  join_nest->table_name_length= 0;
  join_nest->alias= (char *)alias;
  
  join_nest->embedding= embedding;
  join_nest->join_list= belongs_to;
  join_nest->select_lex= select;

  join_nest->nested_join->join_list.empty();

  return join_nest;
}

/**
  Merge tables from a query block into a nested join structure.

  @param select Query block containing tables to be merged into nested join

  @return false if success, true if error
*/

bool TABLE_LIST::merge_underlying_tables(class st_select_lex *select)
{
  DBUG_ASSERT(nested_join->join_list.is_empty());

  List_iterator_fast<TABLE_LIST> li(select->top_join_list);
  TABLE_LIST *tl;
  while ((tl= li++))
  {
    tl->embedding= this;
    tl->join_list= &nested_join->join_list;
    if (nested_join->join_list.push_back(tl))
      return true;                  /* purecov: inspected */
  }

  return false;
}



/*
  calculate md5 of query

  SYNOPSIS
    TABLE_LIST::calc_md5()
    buffer	buffer for md5 writing
*/

void  TABLE_LIST::calc_md5(char *buffer)
{
  uchar digest[MD5_HASH_SIZE];
  compute_md5_hash((char *) digest, (const char *) select_stmt.str,
                   select_stmt.length);
  array_to_hex(buffer, digest, MD5_HASH_SIZE);
}


/**
   Reset a table before starting optimization
*/
void TABLE_LIST::reset()
{
  // @todo If TABLE::init() was always called, this would not be necessary:
  table->const_table= 0;
  table->null_row= 0;
  table->status= STATUS_GARBAGE | STATUS_NOT_FOUND;

  table->force_index= force_index;
  table->force_index_order= table->force_index_group= 0;
  table->covering_keys= table->s->keys_for_keyread;
  table->merge_keys.clear_all();
}


/**
  Merge WHERE condition of view or derived table into outer query.

  If the derived table is on the inner side of an outer join, its WHERE
  condition is merged into the respective join operation's join condition,
  otherwise the WHERE condition is merged with the derived table's
  join condition.

  @param thd    thread handler

  @return false if success, true if error
*/

bool TABLE_LIST::merge_where(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::merge_where");

  DBUG_ASSERT(is_merged());

  Item *const condition= derived_unit()->first_select()->where_cond();

  if (!condition)
    DBUG_RETURN(false);

  /*
    Save the WHERE condition separately. This is needed because it is already
    resolved, so we need to explicitly update used tables information after
    merging this derived table into the outer query.
  */
  derived_where_cond= condition;

  Prepared_stmt_arena_holder ps_arena_holder(thd);

  /*
    Merge WHERE condition with the join condition of the outer join nest
    and attach it to join nest representing this derived table.
  */
  set_join_cond(and_conds(join_cond(), condition));
  if (!join_cond())
    DBUG_RETURN(true);        /* purecov: inspected */

  DBUG_RETURN(false);
}


/**
  Create field translation for merged derived table/view.

  @param thd  Thread handle

  @return false if success, true if error.
*/

bool TABLE_LIST::create_field_translation(THD *thd)
{
  Item *item;
  SELECT_LEX *select= derived->first_select();
  List_iterator_fast<Item> it(select->item_list);
  uint field_count= 0;

  DBUG_ASSERT(derived->is_prepared());

  DBUG_ASSERT(!field_translation);

  Prepared_stmt_arena_holder ps_arena_holder(thd);

  // Create view fields translation table
  Field_translator *transl=
    (Field_translator *)thd->stmt_arena->alloc(select->item_list.elements *
                                               sizeof(Field_translator));
  if (!transl)
    return true;                        /* purecov: inspected */

  while ((item= it++))
  {
    /*
      Notice that all items keep their nullability here.
      All items are later wrapped within Item_direct_view objects.
      If the view is used on the inner side of an outer join, these
      objects will reflect the correct nullability of the selected expressions.
    */
    transl[field_count].name= item->item_name.ptr();
    transl[field_count++].item= item;
  }
  field_translation= transl;
  field_translation_end= transl + field_count;

  return false;
}


/**
  Return merged WHERE clause and join conditions for a view

  @param thd          thread handle
  @param table        table for the VIEW
  @param[out] pcond   Pointer to the built condition (NULL if none)

  This function returns the result of ANDing the WHERE clause and the
  join conditions of the given view.

  @returns  false for success, true for error
*/

static bool merge_join_conditions(THD *thd, TABLE_LIST *table, Item **pcond)
{
  DBUG_ENTER("merge_join_conditions");

  *pcond= NULL;
  DBUG_PRINT("info", ("alias: %s", table->alias));
  if (table->join_cond())
  {
    if (!(*pcond= table->join_cond()->copy_andor_structure(thd)))
      DBUG_RETURN(true);                   /* purecov: inspected */
  }
  if (!table->nested_join)
    DBUG_RETURN(false);
  List_iterator<TABLE_LIST> li(table->nested_join->join_list);
  while (TABLE_LIST *tbl= li++)
  {
    if (tbl->is_view())
      continue;
    Item *cond;
    if (merge_join_conditions(thd, tbl, &cond))
      DBUG_RETURN(true);                   /* purecov: inspected */
    if (cond && !(*pcond= and_conds(*pcond, cond)))
      DBUG_RETURN(true);                   /* purecov: inspected */
  }
  DBUG_RETURN(false);
}


/**
  Prepare check option expression of table

  @param thd            thread handler
  @param is_cascaded     True if parent view requests that this view's
  filtering condition be treated as WITH CASCADED CHECK OPTION; this is for
  recursive calls; user code should omit this argument.

  @details

  This function builds check option condition for use in regular execution or
  subsequent SP/PS executions.

  This function must be called after the WHERE clause and join condition
  of this and all underlying derived tables/views have been resolved.

  The function will always call itself recursively for all underlying views
  and base tables.

  On first invocation, the check option condition is built bottom-up in
  statement mem_root, and check_option_processed is set true.

  On subsequent executions, check_option_processed is true and no
  expression building is necessary. However, the function needs to assure that
  the expression is resolved by calling fix_fields() on it.

  @returns false if success, true if error
*/

bool TABLE_LIST::prepare_check_option(THD *thd, bool is_cascaded)
{
  DBUG_ENTER("TABLE_LIST::prepare_check_option");
  DBUG_ASSERT(is_view());

  /*
    True if conditions of underlying views should be treated as WITH CASCADED
    CHECK OPTION
  */
  is_cascaded|= (with_check == VIEW_CHECK_CASCADED);

  for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
  {
    if (tbl->is_view() && tbl->prepare_check_option(thd, is_cascaded))
      DBUG_RETURN(true);                  /* purecov: inspected */
  }

  if (!check_option_processed)
  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    if ((with_check || is_cascaded) &&
        merge_join_conditions(thd, this, &check_option))
      DBUG_RETURN(true);                  /* purecov: inspected */

    for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
    {
      if (tbl->check_option &&
          !(check_option= and_conds(check_option, tbl->check_option)))
          DBUG_RETURN(true);            /* purecov: inspected */
    }

    check_option_processed= true;
  }

  if (check_option && !check_option->fixed)
  {
    const char *save_where= thd->where;
    thd->where= "check option";
    if (check_option->fix_fields(thd, &check_option) ||
        check_option->check_cols(1))
      DBUG_RETURN(true);                  /* purecov: inspected */
    thd->where= save_where;
  }

  DBUG_RETURN(false);
}


/**
  Prepare replace filter for a table that is inserted into via a view.

  Used with REPLACE command to filter out rows that should not be deleted.
  Concatenate WHERE clauses from multiple views into one permanent field:
  TABLE::replace_filter.

  Since REPLACE is not possible against a join view, there is no need to
  process join conditions, only WHERE clause is needed. But we still call
  merge_join_conditions() since this is a general function that handles both
  join conditions (if any) and the original WHERE clause.

  @param thd            thread handler

  @returns false if success, true if error
*/

bool TABLE_LIST::prepare_replace_filter(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::prepare_replace_filter");

  for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
  {
    if (tbl->is_view() && tbl->prepare_replace_filter(thd))
      DBUG_RETURN(true);
  }

  if (!replace_filter_processed)
  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    if (merge_join_conditions(thd, this, &replace_filter))
      DBUG_RETURN(true);                 /* purecov: inspected */
    for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
    {
      if (tbl->replace_filter)
      {
        if (!(replace_filter= and_conds(replace_filter, tbl->replace_filter)))
          DBUG_RETURN(true);
      }
    }
    replace_filter_processed= true;
  }

  if (replace_filter && !replace_filter->fixed)
  {
    const char *save_where= thd->where;
    thd->where= "replace filter";
    if (replace_filter->fix_fields(thd, &replace_filter) ||
        replace_filter->check_cols(1))
      DBUG_RETURN(true);
    thd->where= save_where;
  }

  DBUG_RETURN(false);
}


/**
  Cleanup items belonged to view fields translation table
*/

void TABLE_LIST::cleanup_items()
{
  if (!field_translation)
    return;

  for (Field_translator *transl= field_translation;
       transl < field_translation_end;
       transl++)
    transl->item->walk(&Item::cleanup_processor, Item::WALK_POSTFIX, NULL);
}


/**
  Check CHECK OPTION condition

  @param thd       thread handler

  @retval VIEW_CHECK_OK     OK
  @retval VIEW_CHECK_ERROR  FAILED
  @retval VIEW_CHECK_SKIP   FAILED, but continue
*/

int TABLE_LIST::view_check_option(THD *thd) const
{
  if (check_option && check_option->val_int() == 0)
  {
    const TABLE_LIST *main_view= top_table();
    my_error(ER_VIEW_CHECK_FAILED, MYF(0), main_view->view_db.str,
             main_view->view_name.str);
    if (thd->lex->is_ignore())
      return(VIEW_CHECK_SKIP);
    return(VIEW_CHECK_ERROR);
  }
  return(VIEW_CHECK_OK);
}


/**
  Find table in underlying tables by map and check that only this
  table belong to given map.

  @param[out] table_ref reference to found table
		        (must be set to NULL by caller)
  @param      map       bit mask of tables

  @retval false table not found or found only one (table_ref is non-NULL)
  @retval true  found several tables
*/

bool TABLE_LIST::check_single_table(TABLE_LIST **table_ref, table_map map)
{
  for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
  {
    if (tbl->is_view_or_derived() && tbl->is_merged())
    {
      if (tbl->check_single_table(table_ref, map))
        return true;
    }
    else if (tbl->map() & map)
    {
      if (*table_ref)
        return true;

      *table_ref= tbl;
    }
  }
  return false;
}


/**
  Set insert_values buffer

  @param mem_root   memory pool for allocating

  @returns false if success, true if error (out of memory)
*/

bool TABLE_LIST::set_insert_values(MEM_ROOT *mem_root)
{
  if (table)
  {
    if (!table->insert_values &&
        !(table->insert_values= (uchar *)alloc_root(mem_root,
                                                    table->s->rec_buff_length)))
      return true;                       /* purecov: inspected */
  }
  else
  {
    DBUG_ASSERT(view && merge_underlying_list);
    for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
      if (tbl->set_insert_values(mem_root))
        return true;                     /* purecov: inspected */
  }
  return false;
}


/*
  Test if this is a leaf with respect to name resolution.

  SYNOPSIS
    TABLE_LIST::is_leaf_for_name_resolution()

  DESCRIPTION
    A table reference is a leaf with respect to name resolution if
    it is either a leaf node in a nested join tree (table, view,
    schema table, subquery), or an inner node that represents a
    NATURAL/USING join, or a nested join with materialized join
    columns.

  RETURN
    TRUE if a leaf, FALSE otherwise.
*/
bool TABLE_LIST::is_leaf_for_name_resolution() const
{
  return (is_view_or_derived() || is_natural_join || is_join_columns_complete ||
          !nested_join);
}


/*
  Retrieve the first (left-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TABLE_LIST::first_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the left-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The left-most child of a nested table reference is the last element
    in the list of children because the children are inserted in
    reverse order.

  RETURN
    If 'this' is a nested table reference - the left-most child of
      the tree rooted in 'this',
    else return 'this'
*/

TABLE_LIST *TABLE_LIST::first_leaf_for_name_resolution()
{
  TABLE_LIST *cur_table_ref= NULL;
  NESTED_JOIN *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  DBUG_ASSERT(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    List_iterator_fast<TABLE_LIST> it(cur_nested_join->join_list);
    cur_table_ref= it++;
    /*
      If the current nested join is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the first operand is
      already at the front of the list. Otherwise the first operand
      is in the end of the list of join operands.
    */
    if (!(cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      TABLE_LIST *next;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*
  Retrieve the last (right-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TABLE_LIST::last_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the right-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The right-most child of a nested table reference is the first
    element in the list of children because the children are inserted
    in reverse order.

  RETURN
    - If 'this' is a nested table reference - the right-most child of
      the tree rooted in 'this',
    - else - 'this'
*/

TABLE_LIST *TABLE_LIST::last_leaf_for_name_resolution()
{
  TABLE_LIST *cur_table_ref= this;
  NESTED_JOIN *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  DBUG_ASSERT(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    cur_table_ref= cur_nested_join->join_list.head();
    /*
      If the current nested is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the last operand is in the
      end of the list.
    */
    if ((cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      List_iterator_fast<TABLE_LIST> it(cur_nested_join->join_list);
      TABLE_LIST *next;
      cur_table_ref= it++;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/**
  Set privileges needed for columns of underlying tables

  @param want_privilege  Required privileges
*/

void TABLE_LIST::set_want_privilege(ulong want_privilege)
{
#ifndef DBUG_OFF
  // Remove SHOW_VIEW_ACL, because it will be checked during making view
  want_privilege&= ~SHOW_VIEW_ACL;

  grant.want_privilege= want_privilege & ~grant.privilege;
  if (table)
    table->grant.want_privilege= want_privilege & ~table->grant.privilege;
  for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
    tbl->set_want_privilege(want_privilege);
#endif
}


/*
  Load security context information for this view

  SYNOPSIS
    TABLE_LIST::prepare_view_securety_context()
    thd                  [in] thread handler

  RETURN
    FALSE  OK
    TRUE   Error
*/

#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool TABLE_LIST::prepare_view_securety_context(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::prepare_view_securety_context");
  DBUG_PRINT("enter", ("table: %s", alias));

  DBUG_ASSERT(!prelocking_placeholder && view);
  if (view_suid)
  {
    DBUG_PRINT("info", ("This table is suid view => load contest"));
    DBUG_ASSERT(view && view_sctx);
    if (acl_getroot(view_sctx,
                    const_cast<char*>(definer.user.str),
                    const_cast<char*>(definer.host.str),
                    const_cast<char*>(definer.host.str),
                    thd->db().str))
    {
      if ((thd->lex->sql_command == SQLCOM_SHOW_CREATE) ||
          (thd->lex->sql_command == SQLCOM_SHOW_FIELDS))
      {
        push_warning_printf(thd, Sql_condition::SL_NOTE,
                            ER_NO_SUCH_USER, 
                            ER(ER_NO_SUCH_USER),
                            definer.user.str, definer.host.str);
      }
      else
      {
        if (thd->security_context()->check_access(SUPER_ACL))
        {
          my_error(ER_NO_SUCH_USER, MYF(0), definer.user.str, definer.host.str);

        }
        else
        {
          if (thd->password == 2)
            my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
                     thd->security_context()->priv_user().str,
                     thd->security_context()->priv_host().str);
          else
            my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
                     thd->security_context()->priv_user().str,
                     thd->security_context()->priv_host().str,
                     (thd->password ?  ER(ER_YES) : ER(ER_NO)));
        }
        DBUG_RETURN(TRUE);
      }
    }
  }
  DBUG_RETURN(FALSE);
}
#endif


/*
  Find security context of current view

  SYNOPSIS
    TABLE_LIST::find_view_security_context()
    thd                  [in] thread handler

*/

#ifndef NO_EMBEDDED_ACCESS_CHECKS
Security_context *TABLE_LIST::find_view_security_context(THD *thd)
{
  Security_context *sctx;
  TABLE_LIST *upper_view= this;
  DBUG_ENTER("TABLE_LIST::find_view_security_context");

  DBUG_ASSERT(view);
  while (upper_view && !upper_view->view_suid)
  {
    DBUG_ASSERT(!upper_view->prelocking_placeholder);
    upper_view= upper_view->referencing_view;
  }
  if (upper_view)
  {
    DBUG_PRINT("info", ("Securety context of view %s will be used",
                        upper_view->alias));
    sctx= upper_view->view_sctx;
    DBUG_ASSERT(sctx);
  }
  else
  {
    DBUG_PRINT("info", ("Current global context will be used"));
    sctx= thd->security_context();
  }
  DBUG_RETURN(sctx);
}
#endif


/*
  Prepare security context and load underlying tables priveleges for view

  SYNOPSIS
    TABLE_LIST::prepare_security()
    thd                  [in] thread handler

  RETURN
    FALSE  OK
    TRUE   Error
*/

bool TABLE_LIST::prepare_security(THD *thd)
{
  List_iterator_fast<TABLE_LIST> tb(*view_tables);
  TABLE_LIST *tbl;
  DBUG_ENTER("TABLE_LIST::prepare_security");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx= thd->security_context();

  DBUG_ASSERT(!prelocking_placeholder);
  if (prepare_view_securety_context(thd))
    DBUG_RETURN(TRUE);
  thd->set_security_context(find_view_security_context(thd));
  opt_trace_disable_if_no_security_context_access(thd);
  while ((tbl= tb++))
  {
    DBUG_ASSERT(tbl->referencing_view);
    const char *local_db, *local_table_name;
    if (tbl->is_view())
    {
      local_db= tbl->view_db.str;
      local_table_name= tbl->view_name.str;
    }
    else if (tbl->is_derived())
    {
      /* Initialize privileges for derived tables */
      tbl->grant.privilege= SELECT_ACL;
      continue;
    }
    else
    {
      local_db= tbl->db;
      local_table_name= tbl->table_name;
    }
    fill_effective_table_privileges(thd, &tbl->grant, local_db,
                                    local_table_name);
    if (tbl->table)
      tbl->table->grant= grant;
  }
  thd->set_security_context(save_security_ctx);
#else
  while ((tbl= tb++))
    tbl->grant.privilege= ~NO_ACCESS;
#endif
  DBUG_RETURN(FALSE);
}


Natural_join_column::Natural_join_column(Field_translator *field_param,
                                         TABLE_LIST *tab)
{
  DBUG_ASSERT(tab->field_translation);
  view_field= field_param;
  table_field= NULL;
  table_ref= tab;
  is_common= FALSE;
}


Natural_join_column::Natural_join_column(Item_field *field_param,
                                         TABLE_LIST *tab)
{
  DBUG_ASSERT(tab->table == field_param->field->table);
  table_field= field_param;
  /*
    Cache table, to have no resolution problem after natural join nests have
    been changed to ordinary join nests.
  */
  if (tab->cacheable_table)
    field_param->cached_table= tab;
  view_field= NULL;
  table_ref= tab;
  is_common= FALSE;
}


const char *Natural_join_column::name()
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    return view_field->name;
  }

  return table_field->field_name;
}


Item *Natural_join_column::create_item(THD *thd)
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    SELECT_LEX *select= thd->lex->current_select();
    return create_view_field(thd, table_ref, &view_field->item,
                             view_field->name, &select->context);
  }
  return table_field;
}


Field *Natural_join_column::field()
{
  if (view_field)
  {
    DBUG_ASSERT(table_field == NULL);
    return NULL;
  }
  return table_field->field;
}


const char *Natural_join_column::table_name()
{
  DBUG_ASSERT(table_ref);
  return table_ref->alias;
}


const char *Natural_join_column::db_name()
{
  if (view_field)
    return table_ref->view_db.str;

  /*
    Test that TABLE_LIST::db is the same as TABLE_SHARE::db to
    ensure consistency. An exception are I_S schema tables, which
    are inconsistent in this respect.
  */
  DBUG_ASSERT(!strcmp(table_ref->db,
                      table_ref->table->s->db.str) ||
              (table_ref->schema_table &&
               is_infoschema_db(table_ref->table->s->db.str,
                                table_ref->table->s->db.length)));
  return table_ref->db;
}


GRANT_INFO *Natural_join_column::grant()
{
  if (view_field)
    return &(table_ref->grant);
  return &(table_ref->table->grant);
}


void Field_iterator_view::set(TABLE_LIST *table)
{
  DBUG_ASSERT(table->field_translation);
  view= table;
  ptr= table->field_translation;
  array_end= table->field_translation_end;
}


const char *Field_iterator_table::name()
{
  return (*ptr)->field_name;
}

Item *Field_iterator_table::create_item(THD *thd)
{
  SELECT_LEX *select= thd->lex->current_select();

  Item_field *item= new Item_field(thd, &select->context, *ptr);
  /*
    This function creates Item-s which don't go through fix_fields(); see same
    code in Item_field::fix_fields().
    */
  if (item && !thd->lex->in_sum_func &&
      select->resolve_place == st_select_lex::RESOLVE_SELECT_LIST)
  {
    if (select->with_sum_func && !select->group_list.elements)
      item->maybe_null= true;
  }
  return item;
}


const char *Field_iterator_view::name()
{
  return ptr->name;
}


Item *Field_iterator_view::create_item(THD *thd)
{
  SELECT_LEX *select= thd->lex->current_select();
  return create_view_field(thd, view, &ptr->item, ptr->name,
                           &select->context);
}

static Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
                               const char *name,
                               Name_resolution_context *context)
{
  Item *field= *field_ref;
  const char *table_name;
  DBUG_ENTER("create_view_field");

  if (view->schema_table_reformed)
  {
    /*
      Translation table items are always Item_fields and already fixed
      ('mysql_schema_table' function). So we can return directly the
      field. This case happens only for 'show & where' commands.
    */
    DBUG_ASSERT(field && field->fixed);
    DBUG_RETURN(field);
  }

  DBUG_ASSERT(field);
  if (!field->fixed)
  {
    if (field->fix_fields(thd, field_ref))
      DBUG_RETURN(NULL);               /* purecov: inspected */
    field= *field_ref;
  }

  /*
    Original table name of a field is calculated as follows:
    - For a view or base table, the view or base table name.
    - For a derived table, the base table name.
    - For an expression that is not a simple column reference, an empty string.
  */
  if (view->is_derived())
  {
    while (field->type() == Item::REF_ITEM)
    {
      field= down_cast<Item_ref *>(field)->ref[0];
    }
    if (field->type() == Item::FIELD_ITEM)
      table_name= down_cast<Item_field *>(field)->table_name;
    else
      table_name= "";
  }
  else
  {
    table_name= view->table_name;
  }
  /*
    @note Creating an Item_direct_view_ref object on top of an Item_field
          means that the underlying Item_field object may be shared by
          multiple occurrences of superior fields. This is a vulnerable
          practice, so special precaution must be taken to avoid programming
          mistakes, such as forgetting to mark the use of a field in both
          read_set and write_set (may happen e.g in an UPDATE statement).
  */
  Item *item= new Item_direct_view_ref(context, field_ref,
                                       view->alias, table_name,
                                       name, view);
  DBUG_RETURN(item);
}


void Field_iterator_natural_join::set(TABLE_LIST *table_ref)
{
  DBUG_ASSERT(table_ref->join_columns);
  column_ref_it.init(*(table_ref->join_columns));
  cur_column_ref= column_ref_it++;
}


void Field_iterator_natural_join::next()
{
  cur_column_ref= column_ref_it++;
  DBUG_ASSERT(!cur_column_ref || ! cur_column_ref->table_field ||
              cur_column_ref->table_ref->table ==
              cur_column_ref->table_field->field->table);
}


void Field_iterator_table_ref::set_field_iterator()
{
  DBUG_ENTER("Field_iterator_table_ref::set_field_iterator");
  /*
    If the table reference we are iterating over is a natural join, or it is
    an operand of a natural join, and TABLE_LIST::join_columns contains all
    the columns of the join operand, then we pick the columns from
    TABLE_LIST::join_columns, instead of the  orginial container of the
    columns of the join operator.
  */
  if (table_ref->is_join_columns_complete)
  {
    /* Necesary, but insufficient conditions. */
    DBUG_ASSERT(table_ref->is_natural_join ||
                table_ref->nested_join ||
                (table_ref->join_columns &&
                /* This is a merge view. */
                ((table_ref->field_translation &&
                  table_ref->join_columns->elements ==
                  (ulong)(table_ref->field_translation_end -
                          table_ref->field_translation)) ||
                 /* This is stored table or a tmptable view. */
                 (!table_ref->field_translation &&
                  table_ref->join_columns->elements ==
                  table_ref->table->s->fields))));
    field_it= &natural_join_it;
    DBUG_PRINT("info",("field_it for '%s' is Field_iterator_natural_join",
                       table_ref->alias));
  }
  /* This is a merge view, so use field_translation. */
  else if (table_ref->field_translation)
  {
    DBUG_ASSERT(table_ref->is_merged());
    field_it= &view_field_it;
    DBUG_PRINT("info", ("field_it for '%s' is Field_iterator_view",
                        table_ref->alias));
  }
  /* This is a base table or stored view. */
  else
  {
    DBUG_ASSERT(table_ref->table || table_ref->is_view());
    field_it= &table_field_it;
    DBUG_PRINT("info", ("field_it for '%s' is Field_iterator_table",
                        table_ref->alias));
  }
  field_it->set(table_ref);
  DBUG_VOID_RETURN;
}


void Field_iterator_table_ref::set(TABLE_LIST *table)
{
  DBUG_ASSERT(table);
  first_leaf= table->first_leaf_for_name_resolution();
  last_leaf=  table->last_leaf_for_name_resolution();
  DBUG_ASSERT(first_leaf && last_leaf);
  table_ref= first_leaf;
  set_field_iterator();
}


void Field_iterator_table_ref::next()
{
  /* Move to the next field in the current table reference. */
  field_it->next();
  /*
    If all fields of the current table reference are exhausted, move to
    the next leaf table reference.
  */
  if (field_it->end_of_fields() && table_ref != last_leaf)
  {
    table_ref= table_ref->next_name_resolution_table;
    DBUG_ASSERT(table_ref);
    set_field_iterator();
  }
}


const char *Field_iterator_table_ref::get_table_name()
{
  if (table_ref->is_view())
    return table_ref->view_name.str;
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->table_name();

  DBUG_ASSERT(!strcmp(table_ref->table_name,
                      table_ref->table->s->table_name.str));
  return table_ref->table_name;
}


const char *Field_iterator_table_ref::get_db_name()
{
  if (table_ref->is_view())
    return table_ref->view_db.str;
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->db_name();

  /*
    Test that TABLE_LIST::db is the same as TABLE_SHARE::db to
    ensure consistency. An exception are I_S schema tables, which
    are inconsistent in this respect.
  */
  DBUG_ASSERT(!strcmp(table_ref->db, table_ref->table->s->db.str) ||
              (table_ref->schema_table &&
               is_infoschema_db(table_ref->table->s->db.str,
                                table_ref->table->s->db.length)));

  return table_ref->db;
}


GRANT_INFO *Field_iterator_table_ref::grant()
{
  if (table_ref->is_view())
    return &(table_ref->grant);
  else if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->grant();
  return &(table_ref->table->grant);
}


/*
  Create new or return existing column reference to a column of a
  natural/using join.

  SYNOPSIS
    Field_iterator_table_ref::get_or_create_column_ref()
    parent_table_ref  the parent table reference over which the
                      iterator is iterating

  DESCRIPTION
    Create a new natural join column for the current field of the
    iterator if no such column was created, or return an already
    created natural join column. The former happens for base tables or
    views, and the latter for natural/using joins. If a new field is
    created, then the field is added to 'parent_table_ref' if it is
    given, or to the original table referene of the field if
    parent_table_ref == NULL.

  NOTES
    This method is designed so that when a Field_iterator_table_ref
    walks through the fields of a table reference, all its fields
    are created and stored as follows:
    - If the table reference being iterated is a stored table, view or
      natural/using join, store all natural join columns in a list
      attached to that table reference.
    - If the table reference being iterated is a nested join that is
      not natural/using join, then do not materialize its result
      fields. This is OK because for such table references
      Field_iterator_table_ref iterates over the fields of the nested
      table references (recursively). In this way we avoid the storage
      of unnecessay copies of result columns of nested joins.

  RETURN
    #     Pointer to a column of a natural join (or its operand)
    NULL  No memory to allocate the column
*/

Natural_join_column *
Field_iterator_table_ref::get_or_create_column_ref(THD *thd, TABLE_LIST *parent_table_ref)
{
  Natural_join_column *nj_col;
  bool is_created= TRUE;
  uint field_count= 0;
  TABLE_LIST *add_table_ref= parent_table_ref ?
                             parent_table_ref : table_ref;

  if (field_it == &table_field_it)
  {
    /* The field belongs to a stored table. */
    Field *tmp_field= table_field_it.field();
    Item_field *tmp_item=
      new Item_field(thd, &thd->lex->current_select()->context, tmp_field);
    if (!tmp_item)
      return NULL;
    nj_col= new Natural_join_column(tmp_item, table_ref);
    field_count= table_ref->table->s->fields;
  }
  else if (field_it == &view_field_it)
  {
    /* The field belongs to a merge view or information schema table. */
    Field_translator *translated_field= view_field_it.field_translator();
    nj_col= new Natural_join_column(translated_field, table_ref);
    field_count= table_ref->field_translation_end -
                 table_ref->field_translation;
  }
  else
  {
    /*
      The field belongs to a NATURAL join, therefore the column reference was
      already created via one of the two constructor calls above. In this case
      we just return the already created column reference.
    */
    DBUG_ASSERT(table_ref->is_join_columns_complete);
    is_created= FALSE;
    nj_col= natural_join_it.column_ref();
    DBUG_ASSERT(nj_col);
  }
  DBUG_ASSERT(!nj_col->table_field ||
              nj_col->table_ref->table == nj_col->table_field->field->table);

  /*
    If the natural join column was just created add it to the list of
    natural join columns of either 'parent_table_ref' or to the table
    reference that directly contains the original field.
  */
  if (is_created)
  {
    /* Make sure not all columns were materialized. */
    DBUG_ASSERT(!add_table_ref->is_join_columns_complete);
    if (!add_table_ref->join_columns)
    {
      /* Create a list of natural join columns on demand. */
      if (!(add_table_ref->join_columns= new List<Natural_join_column>))
        return NULL;
      add_table_ref->is_join_columns_complete= FALSE;
    }
    add_table_ref->join_columns->push_back(nj_col);
    /*
      If new fields are added to their original table reference, mark if
      all fields were added. We do it here as the caller has no easy way
      of knowing when to do it.
      If the fields are being added to parent_table_ref, then the caller
      must take care to mark when all fields are created/added.
    */
    if (!parent_table_ref &&
        add_table_ref->join_columns->elements == field_count)
      add_table_ref->is_join_columns_complete= TRUE;
  }

  return nj_col;
}


/*
  Return an existing reference to a column of a natural/using join.

  SYNOPSIS
    Field_iterator_table_ref::get_natural_column_ref()

  DESCRIPTION
    The method should be called in contexts where it is expected that
    all natural join columns are already created, and that the column
    being retrieved is a Natural_join_column.

  RETURN
    #     Pointer to a column of a natural join (or its operand)
    NULL  No memory to allocate the column
*/

Natural_join_column *
Field_iterator_table_ref::get_natural_column_ref()
{
  Natural_join_column *nj_col;

  DBUG_ASSERT(field_it == &natural_join_it);
  /*
    The field belongs to a NATURAL join, therefore the column reference was
    already created via one of the two constructor calls above. In this case
    we just return the already created column reference.
  */
  nj_col= natural_join_it.column_ref();
  DBUG_ASSERT(nj_col &&
              (!nj_col->table_field ||
               nj_col->table_ref->table == nj_col->table_field->field->table));
  return nj_col;
}

/*****************************************************************************
  Functions to handle column usage bitmaps (read_set, write_set etc...)
*****************************************************************************/

/* Reset all columns bitmaps */

void TABLE::clear_column_bitmaps()
{
  /*
    Reset column read/write usage. It's identical to:
    bitmap_clear_all(&table->def_read_set);
    bitmap_clear_all(&table->def_write_set);
  */
  memset(def_read_set.bitmap, 0, s->column_bitmap_size*2);
  column_bitmaps_set(&def_read_set, &def_write_set);

  bitmap_clear_all(&def_fields_set_during_insert);
  fields_set_during_insert= &def_fields_set_during_insert;

  bitmap_clear_all(&tmp_set);
}


/**
  Tell handler we are going to call position() and rnd_pos() later.
  
  This is needed for handlers that uses the primary key to find the
  row. In this case we have to extend the read bitmap with the primary
  key fields.

  @note: Calling this function does not initialize the table for
  reading using rnd_pos(). rnd_init() still has to be called before
  rnd_pos().
*/

void TABLE::prepare_for_position()
{
  DBUG_ENTER("TABLE::prepare_for_position");

  if ((file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      s->primary_key < MAX_KEY)
  {
    mark_columns_used_by_index_no_reset(s->primary_key, read_set);
    /* signal change */
    file->column_bitmaps_signal();
  }
  DBUG_VOID_RETURN;
}


/**
  Mark column as either read or written (or none) according to mark_used.

  @note If marking a written field, set thd->dup_field if the column is
        already marked.

  @note If TABLE::get_fields_in_item_tree is set, set the flag bit
        GET_FIXED_FIELDS_FLAG for the field.

  @param thd      Thread handler (only used for duplicate handling)
  @param field    The column to be marked as used
  @param mark_used =MARK_COLUMNS_NONE: Only update flag field, if applicable
                   =MARK_COLUMNS_READ: Mark column as read
                   =MARK_COLUMNS_WRITE: Mark column as written
                   =MARK_COLUMNS_TEMP: Mark column as read, used by filesort()
                                       and processing of generated columns
*/

void TABLE::mark_column_used(THD *thd, Field *field,
                             enum enum_mark_columns mark)
{
  DBUG_ENTER("TABLE::mark_column_used");

  switch (mark)
  {
  case MARK_COLUMNS_NONE:
    if (get_fields_in_item_tree)
      field->flags|= GET_FIXED_FIELDS_FLAG;
    break;

  case MARK_COLUMNS_READ:
    bitmap_set_bit(read_set, field->field_index);

    // Update covering_keys and merge_keys based on all fields that are read:
    covering_keys.intersect(field->part_of_key);
    merge_keys.merge(field->part_of_key);
    if (get_fields_in_item_tree)
      field->flags|= GET_FIXED_FIELDS_FLAG;
    if (field->is_virtual_gcol())
      mark_gcol_in_maps(field);
    break;

  case MARK_COLUMNS_WRITE:
    if (bitmap_fast_test_and_set(write_set, field->field_index))
    {
      /*
        This is relevant for INSERT only, but duplicate indication is set
        for all fields that are updated.
      */
      DBUG_PRINT("warning", ("Found duplicated field"));
      thd->dup_field= field;
    }
    DBUG_ASSERT(!get_fields_in_item_tree);

    if (field->is_gcol())
      mark_gcol_in_maps(field);
    break;

  case MARK_COLUMNS_TEMP:
    bitmap_set_bit(read_set, field->field_index);
    if (field->is_virtual_gcol())
      mark_gcol_in_maps(field);
    break;
  }
  DBUG_VOID_RETURN;
}


/*
  Mark that only fields from one key is used

  NOTE:
    This changes the bitmap to use the tmp bitmap
    After this, you can't access any other columns in the table until
    bitmaps are reset, for example with TABLE::clear_column_bitmaps().
*/

void TABLE::mark_columns_used_by_index(uint index)
{
  MY_BITMAP *bitmap= &tmp_set;
  DBUG_ENTER("TABLE::mark_columns_used_by_index");

  set_keyread(TRUE);
  bitmap_clear_all(bitmap);
  mark_columns_used_by_index_no_reset(index, bitmap);
  column_bitmaps_set(bitmap, bitmap);
  DBUG_VOID_RETURN;
}


/*
  mark columns used by key, but don't reset other fields

  The parameter key_parts is used for controlling how many of the
  key_parts that will be marked in the bitmap. It has the following
  interpretation:

  = 0:                 Use all regular key parts from the key 
                       (user_defined_key_parts)
  >= actual_key_parts: Use all regular and extended columns
  < actual_key_parts:  Use this exact number of key parts
 
  To use all regular key parts, the caller can use the default value (0).
  To use all regular and extended key parts, use UINT_MAX. 

  @note The bit map is not cleared by this function. Only bits
  corresponding to a column used by the index will be set. Bits
  representing columns not used by the index will not be changed.

  @param index     index number
  @param bitmap    bitmap to mark
  @param key_parts number of leading key parts to mark. Default is 0.

  @todo consider using actual_key_parts(key_info[index]) instead of
  key_info[index].user_defined_key_parts: if the PK suffix of a secondary
  index is usable it should be marked.
*/

void TABLE::mark_columns_used_by_index_no_reset(uint index,
                                                MY_BITMAP *bitmap,
                                                uint key_parts)
{
  // If key_parts has the default value, then include user defined key parts
  if (key_parts == 0)
    key_parts= key_info[index].user_defined_key_parts;
  else if (key_parts > key_info[index].actual_key_parts)
    key_parts= key_info[index].actual_key_parts;

  KEY_PART_INFO *key_part= key_info[index].key_part;
  KEY_PART_INFO *key_part_end= key_part + key_parts;
  for (;key_part != key_part_end; key_part++)
    bitmap_set_bit(bitmap, key_part->fieldnr-1);
}


/*
  Mark auto-increment fields as used fields in both read and write maps

  NOTES
    This is needed in insert & update as the auto-increment field is
    always set and sometimes read.
*/

void TABLE::mark_auto_increment_column()
{
  DBUG_ASSERT(found_next_number_field);
  /*
    We must set bit in read set as update_auto_increment() is using the
    store() to check overflow of auto_increment values
  */
  bitmap_set_bit(read_set, found_next_number_field->field_index);
  bitmap_set_bit(write_set, found_next_number_field->field_index);
  if (s->next_number_keypart)
    mark_columns_used_by_index_no_reset(s->next_number_index, read_set);
  file->column_bitmaps_signal();
}


/*
  Mark columns needed for doing an delete of a row

  DESCRIPTON
    Some table engines don't have a cursor on the retrieve rows
    so they need either to use the primary key or all columns to
    be able to delete a row.

    If the engine needs this, the function works as follows:
    - If primary key exits, mark the primary key columns to be read.
    - If not, mark all columns to be read

    If the engine has HA_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all keys and doesn't have to
    retrieve the row again.
*/

void TABLE::mark_columns_needed_for_delete()
{
  mark_columns_per_binlog_row_image();

  if (triggers && triggers->mark_fields(TRG_EVENT_DELETE))
    return;

  if (file->ha_table_flags() & HA_REQUIRES_KEY_COLUMNS_FOR_DELETE)
  {
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      if ((*reg_field)->flags & PART_KEY_FLAG)
        bitmap_set_bit(read_set, (*reg_field)->field_index);
    }
    file->column_bitmaps_signal();
  }
  if (file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_DELETE)
  {

    /*
      If the handler has no cursor capabilites we have to read
      either the primary key, the hidden primary key or all columns to
      be able to do an delete
    */
    if (s->primary_key == MAX_KEY)
    {
      /*
        If in RBR, we have alreay marked the full before image
        in mark_columns_per_binlog_row_image, if not, then use
        the hidden primary key
      */
      if (!(mysql_bin_log.is_open() && in_use &&
          in_use->is_current_stmt_binlog_format_row()))
        file->use_hidden_primary_key();
    }
    else
      mark_columns_used_by_index_no_reset(s->primary_key, read_set);

    file->column_bitmaps_signal();
  }
  if (vfield)
  {
    /*
      InnoDB's delete_row may need to log pre-image of the index entries to
      its UNDO log. Thus, indexed virtual generated column must be made ready
      for evaluation.
    */
    mark_generated_columns(true);
  }
}


/**
  @brief
  Mark columns needed for doing an update of a row

  @details
    Some engines needs to have all columns in an update (to be able to
    build a complete row). If this is the case, we mark all not
    updated columns to be read.

    If this is not the case, we do like in the delete case and mark
    if neeed, either the primary key column or all columns to be read.
    (see mark_columns_needed_for_delete() for details)

    If the engine has HA_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all USED key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all changed keys and doesn't have to
    retrieve the row again.

    Unlike other similar methods, it doesn't mark fields used by triggers,
    that is the responsibility of the caller to do, by using
    Table_trigger_dispatcher::mark_used_fields(TRG_EVENT_UPDATE)!

    Note: Marking additional columns as per binlog_row_image requirements will
    influence query execution plan. For example in the case of
    binlog_row_image=FULL the entire read_set and write_set needs to be flagged.
    This will influence update query to think that 'used key is being modified'
    and query will create a temporary table to process the update operation.
    Which will result in performance degradation. Hence callers who don't want
    their query execution to be influenced as per binlog_row_image requirements
    can skip marking binlog specific columns here and they should make an
    explicit call to 'mark_columns_per_binlog_row_image()' function to mark
    binlog_row_image specific columns.
*/

void TABLE::mark_columns_needed_for_update(bool mark_binlog_columns)
{

  DBUG_ENTER("mark_columns_needed_for_update");
  if (mark_binlog_columns)
    mark_columns_per_binlog_row_image();
  if (file->ha_table_flags() & HA_REQUIRES_KEY_COLUMNS_FOR_DELETE)
  {
    /* Mark all used key columns for read */
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      /* Merge keys is all keys that had a column refered to in the query */
      if (merge_keys.is_overlapping((*reg_field)->part_of_key))
        bitmap_set_bit(read_set, (*reg_field)->field_index);
    }
    file->column_bitmaps_signal();
  }

  if (file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_DELETE)
  {
    /*
      If the handler has no cursor capabilites we have to read either
      the primary key, the hidden primary key or all columns to be
      able to do an update
    */
    if (s->primary_key == MAX_KEY)
    {
      /*
        If in RBR, we have alreay marked the full before image
        in mark_columns_per_binlog_row_image, if not, then use
        the hidden primary key
      */
      if (!(mysql_bin_log.is_open() && in_use &&
          in_use->is_current_stmt_binlog_format_row()))
        file->use_hidden_primary_key();
    }
    else
      mark_columns_used_by_index_no_reset(s->primary_key, read_set);

    file->column_bitmaps_signal();
  }
  /* Mark dependent generated columns as writable */
  if (vfield)
    mark_generated_columns(true);
  DBUG_VOID_RETURN;
}

/*
  Mark columns according the binlog row image option.

  When logging in RBR, the user can select whether to
  log partial or full rows, depending on the table
  definition, and the value of binlog_row_image.

  Semantics of the binlog_row_image are the following 
  (PKE - primary key equivalent, ie, PK fields if PK 
  exists, all fields otherwise):

  binlog_row_image= MINIMAL
    - This marks the PKE fields in the read_set
    - This marks all fields where a value was specified
      in the write_set

  binlog_row_image= NOBLOB
    - This marks PKE + all non-blob fields in the read_set
    - This marks all fields where a value was specified
      and all non-blob fields in the write_set

  binlog_row_image= FULL
    - all columns in the read_set
    - all columns in the write_set
    
  This marking is done without resetting the original 
  bitmaps. This means that we will strip extra fields in
  the read_set at binlogging time (for those cases that 
  we only want to log a PK and we needed other fields for
  execution).
 */
void TABLE::mark_columns_per_binlog_row_image()
{
  DBUG_ENTER("mark_columns_per_binlog_row_image");
  DBUG_ASSERT(read_set->bitmap);
  DBUG_ASSERT(write_set->bitmap);

  /**
    If in RBR we may need to mark some extra columns,
    depending on the binlog-row-image command line argument.
   */
  if ((mysql_bin_log.is_open() && in_use &&
       in_use->is_current_stmt_binlog_format_row() &&
       !ha_check_storage_engine_flag(s->db_type(), HTON_NO_BINLOG_ROW_OPT)))
  {

    THD *thd= current_thd;

    /* if there is no PK, then mark all columns for the BI. */
    if (s->primary_key >= MAX_KEY)
      bitmap_set_all(read_set);

    switch (thd->variables.binlog_row_image)
    {
      case BINLOG_ROW_IMAGE_FULL:
        if (s->primary_key < MAX_KEY)
          bitmap_set_all(read_set);
        bitmap_set_all(write_set);
        break;
      case BINLOG_ROW_IMAGE_NOBLOB:
        /* for every field that is not set, mark it unless it is a blob */
        for (Field **ptr=field ; *ptr ; ptr++)
        {
          Field *my_field= *ptr;
          /* 
            bypass blob fields. These can be set or not set, we don't care.
            Later, at binlogging time, if we don't need them in the before 
            image, we will discard them.

            If set in the AI, then the blob is really needed, there is 
            nothing we can do about it.
           */
          if ((s->primary_key < MAX_KEY) && 
              ((my_field->flags & PRI_KEY_FLAG) || 
              (my_field->type() != MYSQL_TYPE_BLOB)))
            bitmap_set_bit(read_set, my_field->field_index);

          if (my_field->type() != MYSQL_TYPE_BLOB)
            bitmap_set_bit(write_set, my_field->field_index);
        }
        break;
      case BINLOG_ROW_IMAGE_MINIMAL:
        /* mark the primary key if available in the read_set */
        if (s->primary_key < MAX_KEY)
          mark_columns_used_by_index_no_reset(s->primary_key, read_set);
        break;

      default: 
        DBUG_ASSERT(FALSE);
    }
    file->column_bitmaps_signal();
  }

  DBUG_VOID_RETURN;
}



/**
  @brief
  Allocate space for keys

  @param key_count  number of keys to allocate.

  @details
  Allocate space enough to fit 'key_count' keys for this table.

  @return FALSE space was successfully allocated.
  @return TRUE OOM error occur.
*/

bool TABLE::alloc_keys(uint key_count)
{
  DBUG_ASSERT(!s->keys);
  max_keys= key_count;
  if (!(key_info= s->key_info=
        (KEY*) alloc_root(&mem_root, sizeof(KEY)*max_keys)))
    return TRUE;

  memset(key_info, 0, sizeof(KEY)*max_keys);
  return FALSE;
}


/**
  @brief Add one key to a temporary table.

  @param key_parts      bitmap of fields that take a part in the key.
  @param key_name       name of the key

  @details
  Creates a key for this table from fields which corresponds the bits set to 1
  in the 'key_parts' bitmap. The 'key_name' name is given to the newly created
  key.
  @see add_derived_key

  @TODO somehow manage to create keys in tmp_table_param for unification
        purposes

  @return TRUE OOM error.
  @return FALSE the key was created or ignored (too long key).
*/

bool TABLE::add_tmp_key(Field_map *key_parts, char *key_name)
{
  DBUG_ASSERT(!created && s->keys < max_keys && key_parts);

  KEY* cur_key= key_info + s->keys;
  Field **reg_field;
  uint i;
  bool key_start= TRUE;
  uint field_count= 0;
  uchar *key_buf;
  KEY_PART_INFO* key_part_info;
  uint key_len= 0;

  for (i= 0, reg_field=field ; *reg_field; i++, reg_field++)
  {
    if (key_parts->is_set(i))
    {
      KEY_PART_INFO tkp;
      // Ensure that we're not creating a key over a blob field.
      DBUG_ASSERT(!((*reg_field)->flags & BLOB_FLAG));
      /*
        Check if possible key is too long, ignore it if so.
        The reason to use MI_MAX_KEY_LENGTH (myisam's default) is that it is
        smaller than MAX_KEY_LENGTH (heap's default) and it's unknown whether
        myisam or heap will be used for tmp table.
      */
      tkp.init_from_field(*reg_field);
      key_len+= tkp.store_length;
      if (key_len > MI_MAX_KEY_LENGTH)
      {
        max_keys--;
        return FALSE;
      }
    }
    field_count++;
  }
  const uint key_part_count= key_parts->bits_set();

  /*
    Allocate storage for the key part array and the two rec_per_key arrays in
    the tables' mem_root.
  */
  const size_t key_buf_size= sizeof(KEY_PART_INFO) * key_part_count;
  ulong *rec_per_key;
  rec_per_key_t *rec_per_key_float;

  if(!multi_alloc_root(&mem_root,
                       &key_buf, key_buf_size,
                       &rec_per_key, sizeof(ulong) * key_part_count,
                       &rec_per_key_float,
                       sizeof(rec_per_key_t) * key_part_count,
                       NULL))
    return true;                                /* purecov: inspected */

  memset(key_buf, 0, key_buf_size);
  cur_key->key_part= key_part_info= (KEY_PART_INFO*) key_buf;
  cur_key->usable_key_parts= cur_key->user_defined_key_parts= key_part_count;
  cur_key->actual_key_parts= cur_key->user_defined_key_parts;
  s->key_parts+= key_part_count;
  cur_key->key_length= key_len;
  cur_key->algorithm= HA_KEY_ALG_BTREE;
  cur_key->name= key_name;
  cur_key->actual_flags= cur_key->flags= HA_GENERATED_KEY;
  cur_key->set_rec_per_key_array(rec_per_key, rec_per_key_float);
  cur_key->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
  cur_key->table= this;

  /* Initialize rec_per_key and rec_per_key_float */
  for (uint kp= 0; kp < key_part_count; ++kp)
  {
    cur_key->rec_per_key[kp]= 0;
    cur_key->set_records_per_key(kp, REC_PER_KEY_UNKNOWN);
  }

  if (field_count == key_part_count)
    covering_keys.set_bit(s->keys);

  keys_in_use_for_group_by.set_bit(s->keys);
  keys_in_use_for_order_by.set_bit(s->keys);
  for (i= 0, reg_field=field ; *reg_field; i++, reg_field++)
  {
    if (!(key_parts->is_set(i)))
      continue;

    if (key_start)
      (*reg_field)->key_start.set_bit(s->keys);
    key_start= FALSE;
    (*reg_field)->part_of_key.set_bit(s->keys);
    (*reg_field)->part_of_sortkey.set_bit(s->keys);
    (*reg_field)->flags|= PART_KEY_FLAG;
    key_part_info->init_from_field(*reg_field);
    key_part_info++;
  }
  set_if_bigger(s->max_key_length, cur_key->key_length);
  s->keys++;
  return FALSE;
}

/*
  @brief
  Save the specified index for later use for ref access.

  @param key_to_save the key to save

  @details
  Save given index as index #0. Table is configured to ignore other indexes.
  Memory occupied by other indexes and index parts will be freed along with
  the table. If the 'key_to_save' is negative then all indexes are freed.
  After keys info being changed, info in fields regarding taking part in keys
  becomes outdated. This function fixes this also.
  @see add_derived_key
*/

void TABLE::use_index(int key_to_save)
{
  DBUG_ASSERT(!created && s->keys && key_to_save < (int)s->keys);

  Field **reg_field;
  /*
    Reset the flags and maps associated with the fields. They are set
    only for the key chosen by the optimizer later.
   */
  for (reg_field=field ; *reg_field; reg_field++)
  {
    if (key_to_save < 0 || !(*reg_field)->part_of_key.is_set(key_to_save))
      (*reg_field)->key_start.clear_all();
    (*reg_field)->part_of_key.clear_all();
    (*reg_field)->part_of_sortkey.clear_all();
    (*reg_field)->flags&= ~PART_KEY_FLAG;
  }

  /* Drop all keys if none of them were chosen */
  if (key_to_save < 0)
  {
    key_info= s->key_info= 0;
    s->key_parts= 0;
    s->keys= 0;
    covering_keys.clear_all();
    keys_in_use_for_group_by.clear_all();
    keys_in_use_for_order_by.clear_all();
  }
  else
  {
    /* Set the flags and maps for the key chosen by the optimizer */
    uint i;
    KEY_PART_INFO *kp;
    for (kp= key_info[key_to_save].key_part, i= 0;
         i < key_info[key_to_save].user_defined_key_parts;
         i++, kp++)
    {
      if (kp->field->key_start.is_set(key_to_save))
        kp->field->key_start.set_prefix(1);
      kp->field->part_of_key.set_prefix(1);
      kp->field->part_of_sortkey.set_prefix(1);
      kp->field->flags|= PART_KEY_FLAG;
    }

    /* Save the given key. No need to copy key#0. */
    if (key_to_save > 0)
      key_info[0]= key_info[key_to_save];
    s->keys= 1;
    s->key_parts= key_info[0].user_defined_key_parts;
    if (covering_keys.is_set(key_to_save))
      covering_keys.set_prefix(1);
    else
      covering_keys.clear_all();
    keys_in_use_for_group_by.set_prefix(1);
    keys_in_use_for_order_by.set_prefix(1);
  }
}



/*
  Mark columns the handler needs for doing an insert

  For now, this is used to mark fields used by the trigger
  as changed.
*/

void TABLE::mark_columns_needed_for_insert()
{
  mark_columns_per_binlog_row_image();
  if (triggers)
  {
    /*
      We don't need to mark columns which are used by ON DELETE and
      ON UPDATE triggers, which may be invoked in case of REPLACE or
      INSERT ... ON DUPLICATE KEY UPDATE, since before doing actual
      row replacement or update write_record() will mark all table
      fields as used.
    */
    if (triggers->mark_fields(TRG_EVENT_INSERT))
      return;
  }
  if (found_next_number_field)
    mark_auto_increment_column();
  /* Mark all generated columns as writable */
  if (vfield)
    mark_generated_columns(false);
}


/* 
  @brief Update the write/read_set for generated columns
         when doing update and insert operation.

  @param        is_update  TRUE means the operation is UPDATE.
                           FALSE means it's INSERT.

  @return       void

  @detail

  Prerequisites for INSERT:

  - write_map is filled with all base columns.

  - read_map is filled with base columns and generated columns to be read. 
  Otherwise, it is empty. covering_keys and merge_keys are adjusted according
  to read_map.

  Actions for INSERT:

  - Fill write_map with all generated columns.
  Stored columns are needed because their values will be stored.
  Virtual columns are needed because their values must be checked against
  constraints and it might be referenced by latter generated columns.

  - Fill read_map with base columns for all generated columns.
  This has no technical reason, but is required because the function that
  evaluates generated functions asserts that base columns are in the read_map.
  covering_keys and merge_keys are adjusted according to read_map.

  Prerequisites for UPDATE:

  - write_map is filled with base columns to be updated.

  - read_map is filled with base columns and generated columns to be read
  prior to the row update. covering_keys and merge_keys are adjusted
  according to read_map.

  Actions for UPDATE:

  - Fill write_map with generated columns that are dependent on updated base columns
  and all virtual generated columns.
  Stored columns are needed because their values will be stored.
  Virtual columns are needed because their values must be checked against
  constraints and might be referenced by latter generated columns.
*/

void TABLE::mark_generated_columns(bool is_update)
{
  Field **vfield_ptr, *tmp_vfield;
  bool bitmap_updated= FALSE;

  if (is_update)
  {
    MY_BITMAP dependent_fields;
    my_bitmap_map bitbuf[bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)];
    bitmap_init(&dependent_fields, bitbuf, s->fields, 0);

    for (vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
    {
      tmp_vfield= *vfield_ptr;
      DBUG_ASSERT(tmp_vfield->gcol_info && tmp_vfield->gcol_info->expr_item);

      /*
        We need to evaluate the GC if:
        - it depends on any updated column
        - or it is virtual indexed, for example:
           * UPDATE changes the primary key's value, and the virtual index
           is a secondary index which includes the pk's value
           * the gcol is in a multi-column index, and UPDATE changes another
           column of this index
           * in both cases the entry in the index needs to change, so needs to
           be located first, for that the GC's value is needed.
      */
      if ((!tmp_vfield->stored_in_db && tmp_vfield->m_indexed) ||
          bitmap_is_overlapping(write_set,
                                &tmp_vfield->gcol_info->base_columns_map))
      {
        // The GC needs to be updated
        tmp_vfield->table->mark_column_used(in_use, tmp_vfield,
                                            MARK_COLUMNS_WRITE);
        // In order to update the new value, we have to read the old value
        tmp_vfield->table->mark_column_used(in_use, tmp_vfield,
                                            MARK_COLUMNS_READ);
        bitmap_updated= TRUE;
      }
    }
  }
  else // Insert needs to evaluate all generated columns
  {
    for (vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
    {
      tmp_vfield= *vfield_ptr;
      DBUG_ASSERT(tmp_vfield->gcol_info && tmp_vfield->gcol_info->expr_item);
      tmp_vfield->table->mark_column_used(in_use, tmp_vfield,
                                          MARK_COLUMNS_WRITE);
      bitmap_updated= TRUE;
    }
  }

  if (bitmap_updated)
    file->column_bitmaps_signal();
}

/*
  Check whether a base field is dependent on any generated columns.

  @return
    TRUE     The field is dependent by some GC.

*/
bool TABLE::is_field_used_by_generated_columns(uint field_index)
{
  MY_BITMAP dependent_fields;
  my_bitmap_map bitbuf[bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)];
  bitmap_init(&dependent_fields, bitbuf, s->fields, 0);
  MY_BITMAP *save_old_read_set= read_set;
  read_set= &dependent_fields;

  for (Field **vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *tmp_vfield= *vfield_ptr;
    DBUG_ASSERT(tmp_vfield->gcol_info && tmp_vfield->gcol_info->expr_item);
    Mark_field mark_fld(MARK_COLUMNS_TEMP);
    tmp_vfield->gcol_info->expr_item->walk(&Item::mark_field_in_map,
                                           Item::WALK_PREFIX, (uchar *) &mark_fld);
    if (bitmap_is_set(read_set, field_index))
    {
      read_set= save_old_read_set;
      return true;
    }
  }
  read_set= save_old_read_set;
  return false;
}


bool TABLE::has_virtual_gcol() const
{
  if (vfield == NULL)
    return false;
  for (Field **gc= vfield; *gc; gc++)
  {
    if (!(*gc)->stored_in_db)
      return true;
  }
  return false;
}

/*
  Cleanup this table for re-execution.

  SYNOPSIS
    TABLE_LIST::reinit_before_use()
*/

void TABLE_LIST::reinit_before_use(THD *thd)
{
  /*
    Reset old pointers to TABLEs: they are not valid since the tables
    were closed in the end of previous prepare or execute call.
  */
  table= 0;

  /*
    Reset table_name and table_name_length for schema table.
    They are not valid as TABLEs were closed in the end of previous prepare
    or execute call.
  */
  if (schema_table_name)
  {
    table_name= schema_table_name;
    table_name_length= strlen(schema_table_name);
  }

  /* Reset is_schema_table_processed value(needed for I_S tables */
  schema_table_state= NOT_PROCESSED;

  mdl_request.ticket= NULL;
}


uint TABLE_LIST::query_block_id() const
{
  return derived ? derived->first_select()->select_number : 0;
}

/*
  Compiles the tagged hints list and fills up the bitmasks.

  SYNOPSIS
    process_index_hints()
      table         the TABLE to operate on.

  DESCRIPTION
    The parser collects the index hints for each table in a "tagged list" 
    (TABLE_LIST::index_hints). Using the information in this tagged list
    this function sets the members st_table::keys_in_use_for_query,
    st_table::keys_in_use_for_group_by, st_table::keys_in_use_for_order_by,
    st_table::force_index, st_table::force_index_order,
    st_table::force_index_group and st_table::covering_keys.

    Current implementation of the runtime does not allow mixing FORCE INDEX
    and USE INDEX, so this is checked here. Then the FORCE INDEX list 
    (if non-empty) is appended to the USE INDEX list and a flag is set.

    Multiple hints of the same kind are processed so that each clause 
    is applied to what is computed in the previous clause.
    For example:
        USE INDEX (i1) USE INDEX (i2)
    is equivalent to
        USE INDEX (i1,i2)
    and means "consider only i1 and i2".
        
    Similarly
        USE INDEX () USE INDEX (i1)
    is equivalent to
        USE INDEX (i1)
    and means "consider only the index i1"

    It is OK to have the same index several times, e.g. "USE INDEX (i1,i1)" is
    not an error.
        
    Different kind of hints (USE/FORCE/IGNORE) are processed in the following
    order:
      1. All indexes in USE (or FORCE) INDEX are added to the mask.
      2. All IGNORE INDEX

    e.g. "USE INDEX i1, IGNORE INDEX i1, USE INDEX i1" will not use i1 at all
    as if we had "USE INDEX i1, USE INDEX i1, IGNORE INDEX i1".

  RETURN VALUE
    FALSE                no errors found
    TRUE                 found and reported an error.
*/
bool TABLE_LIST::process_index_hints(TABLE *tbl)
{
  /* initialize the result variables */
  tbl->keys_in_use_for_query= tbl->keys_in_use_for_group_by= 
    tbl->keys_in_use_for_order_by= tbl->s->keys_in_use;

  /* index hint list processing */
  if (index_hints)
  {
    /* Temporary variables used to collect hints of each kind. */
    key_map index_join[INDEX_HINT_FORCE + 1];
    key_map index_order[INDEX_HINT_FORCE + 1];
    key_map index_group[INDEX_HINT_FORCE + 1];
    Index_hint *hint;
    bool have_empty_use_join= FALSE, have_empty_use_order= FALSE, 
         have_empty_use_group= FALSE;
    List_iterator <Index_hint> iter(*index_hints);

    /* iterate over the hints list */
    while ((hint= iter++))
    {
      uint pos;

      /* process empty USE INDEX () */
      if (hint->type == INDEX_HINT_USE && !hint->key_name.str)
      {
        if (hint->clause & INDEX_HINT_MASK_JOIN)
        {
          index_join[hint->type].clear_all();
          have_empty_use_join= TRUE;
        }
        if (hint->clause & INDEX_HINT_MASK_ORDER)
        {
          index_order[hint->type].clear_all();
          have_empty_use_order= TRUE;
        }
        if (hint->clause & INDEX_HINT_MASK_GROUP)
        {
          index_group[hint->type].clear_all();
          have_empty_use_group= TRUE;
        }
        continue;
      }

      /* 
        Check if an index with the given name exists and get his offset in 
        the keys bitmask for the table 
      */
      if (tbl->s->keynames.type_names == 0 ||
          (pos= find_type(&tbl->s->keynames, hint->key_name.str,
                          hint->key_name.length, 1)) <= 0)
      {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), hint->key_name.str, alias);
        return 1;
      }

      pos--;

      /* add to the appropriate clause mask */
      if (hint->clause & INDEX_HINT_MASK_JOIN)
        index_join[hint->type].set_bit (pos);
      if (hint->clause & INDEX_HINT_MASK_ORDER)
        index_order[hint->type].set_bit (pos);
      if (hint->clause & INDEX_HINT_MASK_GROUP)
        index_group[hint->type].set_bit (pos);
    }

    /* cannot mix USE INDEX and FORCE INDEX */
    if ((!index_join[INDEX_HINT_FORCE].is_clear_all() ||
         !index_order[INDEX_HINT_FORCE].is_clear_all() ||
         !index_group[INDEX_HINT_FORCE].is_clear_all()) &&
        (!index_join[INDEX_HINT_USE].is_clear_all() ||  have_empty_use_join ||
         !index_order[INDEX_HINT_USE].is_clear_all() || have_empty_use_order ||
         !index_group[INDEX_HINT_USE].is_clear_all() || have_empty_use_group))
    {
      my_error(ER_WRONG_USAGE, MYF(0), index_hint_type_name[INDEX_HINT_USE],
               index_hint_type_name[INDEX_HINT_FORCE]);
      return 1;
    }

    /* process FORCE INDEX as USE INDEX with a flag */
    if (!index_order[INDEX_HINT_FORCE].is_clear_all())
    {
      tbl->force_index_order= TRUE;
      index_order[INDEX_HINT_USE].merge(index_order[INDEX_HINT_FORCE]);
    }

    if (!index_group[INDEX_HINT_FORCE].is_clear_all())
    {
      tbl->force_index_group= TRUE;
      index_group[INDEX_HINT_USE].merge(index_group[INDEX_HINT_FORCE]);
    }

    /*
      TODO: get rid of tbl->force_index (on if any FORCE INDEX is specified) and
      create tbl->force_index_join instead.
      Then use the correct force_index_XX instead of the global one.
    */
    if (!index_join[INDEX_HINT_FORCE].is_clear_all() ||
        tbl->force_index_group || tbl->force_index_order)
    {
      tbl->force_index= TRUE;
      index_join[INDEX_HINT_USE].merge(index_join[INDEX_HINT_FORCE]);
    }

    /* apply USE INDEX */
    if (!index_join[INDEX_HINT_USE].is_clear_all() || have_empty_use_join)
      tbl->keys_in_use_for_query.intersect(index_join[INDEX_HINT_USE]);
    if (!index_order[INDEX_HINT_USE].is_clear_all() || have_empty_use_order)
      tbl->keys_in_use_for_order_by.intersect (index_order[INDEX_HINT_USE]);
    if (!index_group[INDEX_HINT_USE].is_clear_all() || have_empty_use_group)
      tbl->keys_in_use_for_group_by.intersect (index_group[INDEX_HINT_USE]);

    /* apply IGNORE INDEX */
    tbl->keys_in_use_for_query.subtract (index_join[INDEX_HINT_IGNORE]);
    tbl->keys_in_use_for_order_by.subtract (index_order[INDEX_HINT_IGNORE]);
    tbl->keys_in_use_for_group_by.subtract (index_group[INDEX_HINT_IGNORE]);
  }

  /* make sure covering_keys don't include indexes disabled with a hint */
  tbl->covering_keys.intersect(tbl->keys_in_use_for_query);
  return 0;
}


size_t max_row_length(TABLE *table, const uchar *data)
{
  TABLE_SHARE *table_s= table->s;
  size_t length= table_s->reclength + 2 * table_s->fields;
  uint *const beg= table_s->blob_field;
  uint *const end= beg + table_s->blob_fields;

  for (uint *ptr= beg ; ptr != end ; ++ptr)
  {
    Field_blob* const blob= (Field_blob*) table->field[*ptr];
    length+= blob->get_length((data + blob->offset(table->record[0]))) +
      HA_KEY_BLOB_LENGTH;
  }
  return length;
}

/**
   Helper function which allows to allocate metadata lock request
   objects for all elements of table list.
*/

void init_mdl_requests(TABLE_LIST *table_list)
{
  for ( ; table_list ; table_list= table_list->next_global)
    MDL_REQUEST_INIT(&table_list->mdl_request,
                     MDL_key::TABLE,
                     table_list->db, table_list->table_name,
                     mdl_type_for_dml(table_list->lock_type),
                     MDL_TRANSACTION);
}


/**
  @returns true if view or derived table and
            - algorithm (for view) does not force materialization
            - the derived table definition is mergeable
            - this is a view, or, if unnamed derived table, the enclosing
              query block allows merging of derived tables.
*/
bool TABLE_LIST::is_mergeable() const
{
  return is_view_or_derived() &&
         algorithm != VIEW_ALGORITHM_TEMPTABLE &&
         derived->is_mergeable() &&
         (is_view() || select_lex->allow_merge_derived);
}

///  @returns true if materializable table contains one or zero rows
bool TABLE_LIST::materializable_is_const() const
{
  DBUG_ASSERT(uses_materialization());
  return derived_unit()->query_result()->estimated_rowcount <= 1;
}


/**
  Return the number of leaf tables for a merged view.
*/

uint TABLE_LIST::leaf_tables_count() const
{
  // Join nests are not permissible, except as merged views
  DBUG_ASSERT(nested_join == NULL || is_merged());
  if (!is_merged())  // Base table or materialized view
    return 1;

  uint count= 0;
  for (TABLE_LIST *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
    count+= tbl->leaf_tables_count();

  return count;
}


/**
  @brief
  Retrieve number of rows in the table

  @details
  Retrieve number of rows in the table referred by this TABLE_LIST and
  store it in the table's stats.records variable. If this TABLE_LIST refers
  to a materialized derived table/view, then the estimated number of rows of
  the derived table/view is used instead.

  @return 0          ok
  @return non zero   error
*/

int TABLE_LIST::fetch_number_of_rows()
{
  int error= 0;
  if (uses_materialization())
  {
    /*
      @todo: CostModel: This updates the stats.record value to the
      estimated number of records. This number is used when estimating 
      the cost of a table scan for a heap table (ie. it helps producing
      a reasonable good cost estimate for heap tables). If the materialized
      table is stored in MyISAM, this number is not used in the cost estimate
      for table scan. The table scan cost for MyISAM thus always becomes
      the estimate for an empty table.
    */
    table->file->stats.records= derived->query_result()->estimated_rowcount;
  }
  else
    error= table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  return error;
}


/**
  A helper function to add a derived key to the list of possible keys

  @param derived_key_list  list of all possible derived keys
  @param field             referenced field
  @param ref_by_tbl        the table that refers to given field

  @details The possible key to be used for join with table with ref_by_tbl
  table map is extended to include 'field'. If ref_by_tbl == 0 then the key
  that includes all referred fields is extended.

  @note
  Procedure of keys generation for result tables of materialized derived
  tables/views for allowing ref access to them.

  A key is generated for each equi-join pair (derived table, another table).
  Each generated key consists of fields of derived table used in equi-join.
  Example:

    SELECT * FROM (SELECT f1, f2, count(*) FROM t1 GROUP BY f1) tt JOIN
                  t1 ON tt.f1=t1.f3 and tt.f2=t1.f4;

  In this case for the derived table tt one key will be generated. It will
  consist of two parts f1 and f2.
  Example:

    SELECT * FROM (SELECT f1, f2, count(*) FROM t1 GROUP BY f1) tt JOIN
                  t1 ON tt.f1=t1.f3 JOIN
                  t2 ON tt.f2=t2.f4;

  In this case for the derived table tt two keys will be generated.
  One key over f1 field, and another key over f2 field.
  Currently optimizer may choose to use only one such key, thus the second
  one will be dropped after the range optimizer is finished.
  See also JOIN::drop_unused_derived_keys function.
  Example:

    SELECT * FROM (SELECT f1, f2, count(*) FROM t1 GROUP BY f1) tt JOIN
                  t1 ON tt.f1=a_function(t1.f3);

  In this case for the derived table tt one key will be generated. It will
  consist of one field - f1.
  In all cases beside one-per-table keys one additional key is generated.
  It includes all fields referenced by other tables.

  Implementation is split in two steps:
    gather information on all used fields of derived tables/view and
      store it in lists of possible keys, one per a derived table/view.
    add keys to result tables of derived tables/view using info from above
      lists.

  The above procedure is implemented in 4 functions:
    TABLE_LIST::update_derived_keys
                          Create/extend list of possible keys for one derived
                          table/view based on given field/used tables info.
                          (Step one)
    JOIN::generate_derived_keys
                          This function is called from update_ref_and_keys
                          when all possible info on keys is gathered and it's
                          safe to add keys - no keys or key parts would be
                          missed.  Walk over list of derived tables/views and
                          call to TABLE_LIST::generate_keys to actually
                          generate keys. (Step two)
    TABLE_LIST::generate_keys
                          Walks over list of possible keys for this derived
                          table/view to add keys to the result table.
                          Calls to TABLE::add_tmp_key to actually add
                          keys. (Step two)
    TABLE::add_tmp_key    Creates one index description according to given
                          bitmap of used fields. (Step two)
  There is also the fifth function called TABLE::use_index. It saves used
  key and frees others. It is called when the optimizer has chosen which key
  it will use, thus we don't need other keys anymore.

  @return TRUE  OOM
  @return FALSE otherwise
*/

static bool add_derived_key(List<Derived_key> &derived_key_list, Field *field,
                             table_map ref_by_tbl)
{
  uint key= 0;
  Derived_key *entry= 0;
  List_iterator<Derived_key> ki(derived_key_list);

  /* Search for already existing possible key. */
  while ((entry= ki++))
  {
    key++;
    if (ref_by_tbl)
    {
      /* Search for the entry for the specified table.*/
      if (entry->referenced_by & ref_by_tbl)
        break;
    }
    else
    {
      /*
        Search for the special entry that should contain fields referred
        from any table.
      */
      if (!entry->referenced_by)
        break;
    }
  }
  /* Add new possible key if nothing is found. */
  if (!entry)
  {
    THD *thd= field->table->in_use;
    key++;
    entry= new (thd->mem_root) Derived_key();
    if (!entry)
      return TRUE;
    entry->referenced_by= ref_by_tbl;
    entry->used_fields.clear_all();
    if (derived_key_list.push_back(entry, thd->mem_root))
      return TRUE;
    field->table->max_keys++;
  }
  /* Don't create keys longer than REF access can use. */
  if (entry->used_fields.bits_set() < MAX_REF_PARTS)
  {
    field->part_of_key.set_bit(key - 1);
    field->flags|= PART_KEY_FLAG;
    entry->used_fields.set_bit(field->field_index);
  }
  return FALSE;
}

/*
  @brief
  Update derived table's list of possible keys

  @param field      derived table's field to take part in a key
  @param values     array of values that a part of equality predicate with the
                    field above
  @param num_values number of elements in the array values

  @details
  This function creates/extends a list of possible keys for this derived
  table/view. For each table used by a value from the 'values' array the
  corresponding possible key is extended to include the 'field'.
  If there is no such possible key, then it is created. field's
  part_of_key bitmaps are updated accordingly.
  @see add_derived_key

  @return TRUE  new possible key can't be allocated.
  @return FALSE list of possible keys successfully updated.
*/

bool TABLE_LIST::update_derived_keys(Field *field, Item **values,
                                     uint num_values)
{
  /*
    Don't bother with keys for CREATE VIEW, BLOB fields and fields with
    zero length.
  */
  if (field->table->in_use->lex->is_ps_or_view_context_analysis() ||
      field->flags & BLOB_FLAG ||
      field->field_length == 0)
    return FALSE;

  /* Allow all keys to be used. */
  if (derived_key_list.elements == 0)
  {
    table->keys_in_use_for_query.set_all();
    table->s->uniques= 0;
  }

  for (uint i= 0; i < num_values; i++)
  {
    table_map tables= values[i]->used_tables() & ~PSEUDO_TABLE_BITS;
    if (!tables || values[i]->real_item()->type() != Item::FIELD_ITEM)
      continue;
    for (table_map tbl= 1; tables >= tbl; tbl<<= 1)
    {
      if (! (tables & tbl))
        continue;
      if (add_derived_key(derived_key_list, field, tbl))
        return TRUE;
    }
  }
  /* Extend key which includes all referenced fields. */
  if (add_derived_key(derived_key_list, field, (table_map)0))
    return TRUE;
  return FALSE;
}


/*
  Comparison function for Derived_key entries.
  See TABLE_LIST::generate_keys.
*/

static int Derived_key_comp(Derived_key *e1, Derived_key *e2, void *arg)
{
  /* Move entries for tables with greater table bit to the end. */
  return ((e1->referenced_by < e2->referenced_by) ? -1 :
          ((e1->referenced_by > e2->referenced_by) ? 1 : 0));
}


/**
  @brief
  Generate keys for a materialized derived table/view

  @details
  This function adds keys to the result table by walking over the list of
  possible keys for this derived table/view and calling the
  TABLE::add_tmp_key to actually add keys. A name <auto_keyN>, where N is a
  sequential number, is given to each key to ease debugging.
  @see add_derived_key

  @return TRUE  an error occur.
  @return FALSE all keys were successfully added.
*/

bool TABLE_LIST::generate_keys()
{
  List_iterator<Derived_key> it(derived_key_list);
  Derived_key *entry;
  uint key= 0;
  char buf[NAME_CHAR_LEN];
  DBUG_ASSERT(uses_materialization());

  if (!derived_key_list.elements)
    return FALSE;

  if (table->alloc_keys(derived_key_list.elements))
    return TRUE;

  /* Sort entries to make key numbers sequence deterministic. */
  derived_key_list.sort((Node_cmp_func)Derived_key_comp, 0);
  while ((entry= it++))
  {
    sprintf(buf, "<auto_key%i>", key++);
    if (table->add_tmp_key(&entry->used_fields,
                           table->in_use->mem_strdup(buf)))
      return TRUE;
  }
  return FALSE;
}


/**
  Update TABLE::const_key_parts for single table UPDATE/DELETE query

  @param conds               WHERE clause expression

  @retval TRUE   error (OOM)
  @retval FALSE  success

  @note
    Set const_key_parts bits if key fields are equal to constants in
    the WHERE expression.
*/

bool TABLE::update_const_key_parts(Item *conds)
{
  memset(const_key_parts, 0, sizeof(key_part_map) * s->keys);

  if (conds == NULL)
    return FALSE;

  for (uint index= 0; index < s->keys; index++)
  {
    KEY_PART_INFO *keyinfo= key_info[index].key_part;
    KEY_PART_INFO *keyinfo_end= keyinfo + key_info[index].user_defined_key_parts;

    for (key_part_map part_map= (key_part_map)1; 
        keyinfo < keyinfo_end;
        keyinfo++, part_map<<= 1)
    {
      if (const_expression_in_where(conds, NULL, keyinfo->field))
        const_key_parts[index]|= part_map;
    }
  }
  return FALSE;
}


/**
  Read removal is possible if the selected quick read
  method is using full unique index

  @see HA_READ_BEFORE_WRITE_REMOVAL

  @param index              Number of the index used for read

  @retval true   success, read removal started
  @retval false  read removal not started
*/

bool TABLE::check_read_removal(uint index)
{
  bool retval= false;

  DBUG_ENTER("check_read_removal");
  DBUG_ASSERT(file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL);
  DBUG_ASSERT(index != MAX_KEY);

  // Index must be unique
  if ((key_info[index].flags & HA_NOSAME) == 0)
    DBUG_RETURN(false);

  // Full index must be used
  bitmap_clear_all(&tmp_set);
  mark_columns_used_by_index_no_reset(index, &tmp_set);

  if (bitmap_cmp(&tmp_set, read_set))
  {
    // Start read removal in handler
    retval= file->start_read_removal();
  }

  bitmap_clear_all(&tmp_set);
  DBUG_RETURN(retval);
}


/**
  Test if the order list consists of simple field expressions

  @param order                Linked list of ORDER BY arguments

  @return TRUE if @a order is empty or consist of simple field expressions
*/

bool is_simple_order(ORDER *order)
{
  for (ORDER *ord= order; ord; ord= ord->next)
  {
    if (ord->item[0]->real_item()->type() != Item::FIELD_ITEM)
      return FALSE;
  }
  return TRUE;
}


/**
  Repoint a table's fields from old_rec to new_rec

  @param table     the table of fields needed to be repointed
  @param old_rec   the original record buffer fields point to
  @param new_rec   the target record buff fields need to repoint
*/

void repoint_field_to_record(TABLE *table, uchar *old_rec, uchar *new_rec)
{
  Field **fields= table->field;
  my_ptrdiff_t ptrdiff= new_rec - old_rec;
  for (uint i= 0; i < table->s->fields; i++)
    fields[i]->move_field_offset(ptrdiff);
}


/**
  Evaluate necessary virtual generated columns.
  This is used right after reading a row from the storage engine.

  @note this is not necessary for stored generated columns, as they are
  provided by the storage engine.

  @param buf[in,out]     the buffer to store data
  @param table           the TABLE object
  @param active_index    the number of key for index scan (MAX_KEY is default)

  @return true if error.

  @todo see below for potential conflict with Bug#21815348 .
 */
bool update_generated_read_fields(uchar *buf, TABLE *table, uint active_index)
{
  DBUG_ENTER("update_generated_read_fields");
  DBUG_ASSERT(table && table->vfield);
  if (active_index != MAX_KEY && table->key_read)
  {
    /*
      The covering index is providing all necessary columns, including
      generated ones.
      Note that this logic may have to be reconsidered when we fix
      Bug#21815348; indeed, for that bug it could be possible to implement the
      following optimization: if A is an indexed base column, and B is a
      virtual generated column dependent on A, "select B from t" could choose
      an index-only scan over the index of A and calculate values of B on the
      fly. In that case, we would come here, however calculation of B would
      still be needed.
      Currently MySQL doesn't choose an index scan in that case because it
      considers B as independent from A, in its index-scan decision logic.
    */
    DBUG_RETURN(false);
  }

  int error= 0;

  /*
    If the buffer storing the record data is not record[0], then the field
    objects must be temporarily changed to point into the supplied buffer.
    The field pointers are restored at the end of this function.
  */
  if (buf != table->record[0])
    repoint_field_to_record(table, table->record[0], buf);

  for (Field **vfield_ptr= table->vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *vfield= *vfield_ptr;
    DBUG_ASSERT(vfield->gcol_info && vfield->gcol_info->expr_item);
    /*
      Only calculate those virtual generated fields that are marked in the
      read_set bitmap.
    */
    if (!vfield->stored_in_db &&
        bitmap_is_set(table->read_set, vfield->field_index))
    {
      if ((vfield->flags & BLOB_FLAG) != 0)
      {
        (down_cast<Field_blob*>(vfield))->keep_old_value();
        (down_cast<Field_blob*>(vfield))->set_keep_old_value(true);
      }

      error= vfield->gcol_info->expr_item->save_in_field(vfield, 0);
      DBUG_PRINT("info", ("field '%s' - updated", vfield->field_name));
      if (error && !table->in_use->is_error())
      {
        /*
          Most likely a calculation error which only triggered a warning, so
          let's not make the read fail.
        */
        error= 0;
      }
    }
    else
    {
      DBUG_PRINT("info", ("field '%s' - skipped", vfield->field_name));
    }
  }

  if (buf != table->record[0])
    repoint_field_to_record(table, buf, table->record[0]);

  DBUG_RETURN(error != 0);
  /*
    @todo
    this function is used by ha_rnd/etc, those ha_* functions are expected to
    return 0 or a HA_ERR code (and such codes are picked up by
    handler::print_error), but update_generated_read_fields returns true/false
    (0/1), which is then returned by the ha_* functions. If it
    returns 1 we get:
    ERROR 1030 (HY000): Got error 1 from storage engine
    which isn't informative for the user.
  */
}

/**
  Calculate data for each generated field marked for write in the
  corresponding column map.

  @note We need calculate data for both virtual and stored generated
  fields.

  @param bitmap         Bitmap over fields to update
  @param table          the TABLE object

  @return
    @retval
      false  - Success
    @retval
      true   - Error occurred during the generation/calculation of a generated
               field value
 */
bool update_generated_write_fields(const MY_BITMAP *bitmap, TABLE *table)
{
  DBUG_ENTER("update_generated_write_fields");
  Field **vfield_ptr;
  int error= 0;

  DBUG_ASSERT(table->vfield);
  /* Iterate over generated fields in the table */
  for (vfield_ptr= table->vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *vfield;
    vfield= (*vfield_ptr);
    DBUG_ASSERT(vfield->gcol_info && vfield->gcol_info->expr_item);

    /* Only update those fields that are marked in the bitmap */
    if (bitmap_is_set(bitmap, vfield->field_index))
    {
      /*
        For a virtual generated column based on the blob type, we have to keep
        the current blob value since this might be needed by the
        storage engine during updates.
      */
      if ((vfield->flags & BLOB_FLAG) != 0 && vfield->is_virtual_gcol())
      {
        (down_cast<Field_blob*>(vfield))->keep_old_value();
        (down_cast<Field_blob*>(vfield))->set_keep_old_value(true);
      }

      /* Generate the actual value of the generated fields */
      error= vfield->gcol_info->expr_item->save_in_field(vfield, 0);

      DBUG_PRINT("info", ("field '%s' - updated", vfield->field_name));
      if (error && !table->in_use->is_error())
        error= 0;
      if (table->fields_set_during_insert)
        bitmap_set_bit(table->fields_set_during_insert, vfield->field_index);
    }
    else
    {
      DBUG_PRINT("info", ("field '%s' - skipped", vfield->field_name));
    }
  }

  if (error > 0)
    DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}




/**
  Adds a generated column and its dependencies to the read_set/write_set
  bitmaps.

  If the value of a generated column (gcol) must be calculated, it needs to
  be in write_set (to satisfy the assertion in Field::store); the value of
  its underlying base columns is necessary to the calculation so those must
  be in read_set.

  A gcol must be calculated in two cases:
  - we're sending the gcol to the engine
  - the gcol is virtual and we're reading it from the engine without using a
  covering index on it.
*/
void TABLE::mark_gcol_in_maps(Field *field)
{
  bitmap_set_bit(write_set, field->field_index);
  /*
    Note that underlying base columns are here added to read_set but not added
    to requirements for an index to be covering (covering_keys is not touched).
    So, if we have:
    SELECT gcol FROM t :
    - an index covering gcol only (not including base columns), can still be
    chosen by the optimizer; note that InnoDB's build_template_needs_field()
    properly ignores read_set when MySQL asks for "index only" reads
    (table->key_read == true); if it didn't, it would do useless reads.
    - but if gcol is not read from an index, we will read base columns because
    they are in read_set.
    - Note how this relies on InnoDB's behaviour.
  */
  for (uint i= 0; i < s->fields; i++)
  {
    if (bitmap_is_set(&field->gcol_info->base_columns_map, i))
    {
      bitmap_set_bit(read_set, i);
      if (this->field[i]->is_virtual_gcol())
        bitmap_set_bit(write_set, i);
    }
  }
}

bool TABLE::contains_records(THD *thd, bool *retval)
{
  READ_RECORD info_read_record;
  *retval= true;
  if (init_read_record(&info_read_record, thd, this, NULL, 1, 1, FALSE))
    return true;

  // read_record returns -1 for EOF.
  *retval= (info_read_record.read_record(&info_read_record) != -1);
  end_read_record(&info_read_record);

  return false;
}

void TABLE::blobs_need_not_keep_old_value()
{
  for (Field **vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *vfield= *vfield_ptr;
    /*
      Set this flag so that all blob columns can keep the old value.
    */
    if (vfield->type() == MYSQL_TYPE_BLOB && vfield->is_virtual_gcol())
      (down_cast<Field_blob*>(vfield))->set_keep_old_value(false);
  }
}
