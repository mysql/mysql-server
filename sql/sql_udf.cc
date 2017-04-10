/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/* This implements 'user defined functions' */

/*
   Known bugs:

   Memory for functions is never freed!
   Shared libraries are not closed before mysqld exits;
     - This is because we can't be sure if some threads are using
       a function.

   The bugs only affect applications that create and free a lot of
   dynamic functions, so this shouldn't be a real problem.
*/

#include "sql_udf.h"

#include <string.h>
#include <new>

#include "derror.h"             // ER_DEFAULT
#include "field.h"
#include "handler.h"
#include "hash.h"               // HASH
#include "item_create.h"
#include "log.h"                // sql_print_error
#include "m_ctype.h"
#include "m_string.h"           // my_stpcpy
#include "mdl.h"
#include "my_base.h"
#include "my_config.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/psi/psi_rwlock.h"
#include "mysqld.h"             // opt_allow_suspicious_udfs
#include "mysqld_error.h"       // ER_*
#include "records.h"            // READ_RECORD
#include "sql_base.h"           // close_mysql_tables
#include "sql_class.h"          // THD
#include "sql_const.h"
#include "sql_parse.h"          // check_string_char_length
#include "sql_plugin.h"         // check_valid_path
#include "sql_servers.h"
#include "sql_table.h"          // write_bin_log
#include "table.h"              // TABLE_LIST
#include "thr_lock.h"
#include "thr_malloc.h"
#include "transaction.h"        // trans_*

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

/**
  @page page_ext_udf User Defined Functions

  @todo Document me

  @sa add_udf, udf_hash_delete.
*/

static bool initialized = 0;
static MEM_ROOT mem;
static HASH udf_hash;
static mysql_rwlock_t THR_LOCK_udf;


static udf_func *add_udf(LEX_STRING *name, Item_result ret,
                         char *dl, Item_udftype typ);
static void udf_hash_delete(udf_func *udf);
static void *find_udf_dl(const char *dl);

static char *init_syms(udf_func *tmp, char *nm)
{
  char *end;

  if (!((tmp->func= (Udf_func_any) dlsym(tmp->dlhandle, tmp->name.str))))
    return tmp->name.str;

  end=my_stpcpy(nm,tmp->name.str);

  if (tmp->type == UDFTYPE_AGGREGATE)
  {
    (void)my_stpcpy(end, "_clear");
    if (!((tmp->func_clear= (Udf_func_clear) dlsym(tmp->dlhandle, nm))))
      return nm;
    (void)my_stpcpy(end, "_add");
    if (!((tmp->func_add= (Udf_func_add) dlsym(tmp->dlhandle, nm))))
      return nm;
  }

  (void) my_stpcpy(end,"_deinit");
  tmp->func_deinit= (Udf_func_deinit) dlsym(tmp->dlhandle, nm);

  (void) my_stpcpy(end,"_init");
  tmp->func_init= (Udf_func_init) dlsym(tmp->dlhandle, nm);

  /*
    to prevent loading "udf" from, e.g. libc.so
    let's ensure that at least one auxiliary symbol is defined
  */
  if (!tmp->func_init && !tmp->func_deinit && tmp->type != UDFTYPE_AGGREGATE)
  {
    if (!opt_allow_suspicious_udfs)
      return nm;
    sql_print_warning(ER_DEFAULT(ER_CANT_FIND_DL_ENTRY), nm);
  }
  return 0;
}


static const uchar* get_hash_key(const uchar *buff, size_t *length)
{
  udf_func *udf=(udf_func*) buff;
  *length=(uint) udf->name.length;
  return (uchar*) udf->name.str;
}

static PSI_memory_key key_memory_udf_mem;

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_key key_rwlock_THR_LOCK_udf;

static PSI_rwlock_info all_udf_rwlocks[]=
{
  { &key_rwlock_THR_LOCK_udf, "THR_LOCK_udf", PSI_FLAG_GLOBAL}
};

static PSI_memory_info all_udf_memory[]=
{
  { &key_memory_udf_mem, "udf_mem", PSI_FLAG_GLOBAL}
};

static void init_udf_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= static_cast<int>(array_elements(all_udf_rwlocks));
  mysql_rwlock_register(category, all_udf_rwlocks, count);

  count= static_cast<int>(array_elements(all_udf_memory));
  mysql_memory_register(category, all_udf_memory, count);
}
#endif

/*
  Read all predeclared functions from mysql.func and accept all that
  can be used.
*/

void udf_init()
{
  udf_func *tmp;
  TABLE_LIST tables;
  READ_RECORD read_record_info;
  TABLE *table;
  int error;
  DBUG_ENTER("ufd_init");
  char db[]= "mysql"; /* A subject to casednstr, can't be constant */

  if (initialized)
    DBUG_VOID_RETURN;

#ifdef HAVE_PSI_INTERFACE
  init_udf_psi_keys();
#endif

  mysql_rwlock_init(key_rwlock_THR_LOCK_udf, &THR_LOCK_udf);
  init_sql_alloc(key_memory_udf_mem, &mem, UDF_ALLOC_BLOCK_SIZE, 0);

  THD *new_thd = new(std::nothrow) THD;
  if (new_thd == nullptr ||
      my_hash_init(&udf_hash,system_charset_info,32,0,get_hash_key, nullptr, 0,
                   key_memory_udf_mem))
  {
    sql_print_error("Can't allocate memory for udf structures");
    my_hash_free(&udf_hash);
    free_root(&mem,MYF(0));
    delete new_thd;
    DBUG_VOID_RETURN;
  }
  initialized = 1;
  new_thd->thread_stack= (char*) &new_thd;
  new_thd->store_globals();
  {
    LEX_CSTRING db_lex_cstr= { STRING_WITH_LEN(db) };
    new_thd->set_db(db_lex_cstr);
  }

  tables.init_one_table(db, sizeof(db)-1, C_STRING_WITH_LEN("func"), "func",
                        TL_READ, MDL_SHARED_READ_ONLY);

  if (open_trans_system_tables_for_read(new_thd, &tables))
  {
    DBUG_PRINT("error",("Can't open udf table"));
    sql_print_error("Can't open the mysql.func table. Please "
                    "run mysql_upgrade to create it.");
    goto end;
  }

  table= tables.table;
  if (init_read_record(&read_record_info, new_thd, table, NULL, 1, 1, FALSE))
    goto end;
  while (!(error= read_record_info.read_record(&read_record_info)))
  {
    DBUG_PRINT("info",("init udf record"));
    LEX_STRING name;
    name.str=get_field(&mem, table->field[0]);
    name.length = strlen(name.str);
    char *dl_name= get_field(&mem, table->field[2]);
    bool new_dl=0;
    Item_udftype udftype=UDFTYPE_FUNCTION;
    if (table->s->fields >= 4)			// New func table
      udftype=(Item_udftype) table->field[3]->val_int();

    /*
      Ensure that the .dll doesn't have a path
      This is done to ensure that only approved dll from the system
      directories are used (to make this even remotely secure).

      On windows we must check both FN_LIBCHAR and '/'.
    */

    LEX_CSTRING name_cstr= {name.str, name.length};
    if (check_valid_path(dl_name, strlen(dl_name)) ||
        check_string_char_length(name_cstr, "", NAME_CHAR_LEN,
                                 system_charset_info, 1))
    {
      sql_print_error("Invalid row in mysql.func table for function '%.64s'",
                      name.str);
      continue;
    }

    if (!(tmp= add_udf(&name,(Item_result) table->field[1]->val_int(),
                       dl_name, udftype)))
    {
      sql_print_error("Can't alloc memory for udf function: '%.64s'", name.str);
      continue;
    }

    void *dl = find_udf_dl(tmp->dl);
    if (dl == NULL)
    {
      char dlpath[FN_REFLEN];
      strxnmov(dlpath, sizeof(dlpath) - 1, opt_plugin_dir, "/", tmp->dl,
               NullS);
      (void) unpack_filename(dlpath, dlpath);
      if (!(dl= dlopen(dlpath, RTLD_NOW)))
      {
        const char *errmsg;
        int error_number= dlopen_errno;
        DLERROR_GENERATE(errmsg, error_number);

        // Print warning to log
        sql_print_error(ER_DEFAULT(ER_CANT_OPEN_LIBRARY),
                        tmp->dl, error_number, errmsg);
        // Keep the udf in the hash so that we can remove it later
        continue;
      }
      new_dl=1;
    }
    tmp->dlhandle = dl;
    {
      char buf[NAME_LEN+16], *missing;
      if ((missing= init_syms(tmp, buf)))
      {
        sql_print_error(ER_DEFAULT(ER_CANT_FIND_DL_ENTRY), missing);
        udf_hash_delete(tmp);
        if (new_dl)
          dlclose(dl);
      }
    }
  }
  if (error > 0)
    sql_print_error("Got unknown error: %d", my_errno());
  end_read_record(&read_record_info);
  table->m_needs_reopen= TRUE;                  // Force close to free memory

end:
  close_trans_system_tables(new_thd);
  delete new_thd;
  DBUG_VOID_RETURN;
}

/**
   Deintialize the UDF subsystem.

   This function does the following:
   1. Closes the shared libaries.
   2. Free the UDF hash.
   3. Free the memroot allocated.
   4. Destroy the RW mutex object.
*/
void udf_deinit()
{
  /* close all shared libraries */
  DBUG_ENTER("udf_free");
  for (uint idx=0 ; idx < udf_hash.records ; idx++)
  {
    udf_func *udf=(udf_func*) my_hash_element(&udf_hash,idx);
    if (udf->dlhandle)				// Not closed before
    {
      /* Mark all versions using the same handler as closed */
      for (uint j=idx+1 ;  j < udf_hash.records ; j++)
      {
        udf_func *tmp=(udf_func*) my_hash_element(&udf_hash,j);
        if (udf->dlhandle == tmp->dlhandle)
          tmp->dlhandle=0;			// Already closed
      }
      dlclose(udf->dlhandle);
    }
  }
  my_hash_free(&udf_hash);
  free_root(&mem,MYF(0));
  if (initialized)
  {
    initialized= 0;
    mysql_rwlock_destroy(&THR_LOCK_udf);
  }
  DBUG_VOID_RETURN;
}


/**
   Delete the UDF function from the UDF hash.

   @param udf  Pointer to the UDF function.

   @note The function remove the udf function from the udf
         hash if it is not in use. If the function is in use,
         the function name is renamed so that it is not used.
         The function shall be removed when no threads use it.
*/
static void udf_hash_delete(udf_func *udf)
{
  DBUG_ENTER("udf_hash_delete");

  mysql_rwlock_wrlock(&THR_LOCK_udf);
  if (!--udf->usage_count)
  {
    my_hash_delete(&udf_hash,(uchar*) udf);
    using_udf_functions=udf_hash.records != 0;
  }
  else
  {
    /*
      The functions is in use ; Rename the functions instead of removing it.
      The functions will be automaticly removed when the least threads
      doesn't use it anymore
    */
    char *name= udf->name.str;
    size_t name_length=udf->name.length;
    udf->name.str=(char*) "*";
    udf->name.length=1;
    my_hash_update(&udf_hash,(uchar*) udf,(uchar*) name,name_length);
  }
  mysql_rwlock_unlock(&THR_LOCK_udf);
  DBUG_VOID_RETURN;
}


void free_udf(udf_func *udf)
{
  DBUG_ENTER("free_udf");

  if (!initialized)
    DBUG_VOID_RETURN;

  mysql_rwlock_wrlock(&THR_LOCK_udf);
  if (!--udf->usage_count)
  {
    /*
      We come here when someone has deleted the udf function
      while another thread still was using the udf
    */
    my_hash_delete(&udf_hash,(uchar*) udf);
    using_udf_functions=udf_hash.records != 0;
    if (!find_udf_dl(udf->dl))
      dlclose(udf->dlhandle);
  }
  mysql_rwlock_unlock(&THR_LOCK_udf);
  DBUG_VOID_RETURN;
}


/* This is only called if using_udf_functions != 0 */

udf_func *find_udf(const char *name, size_t length,bool mark_used)
{
  udf_func *udf=0;
  DBUG_ENTER("find_udf");

  if (!initialized)
    DBUG_RETURN(NULL);

  /* TODO: This should be changed to reader locks someday! */
  if (mark_used)
    mysql_rwlock_wrlock(&THR_LOCK_udf);  /* Called during fix_fields */
  else
    mysql_rwlock_rdlock(&THR_LOCK_udf);  /* Called during parsing */

  if ((udf=(udf_func*) my_hash_search(&udf_hash,(uchar*) name,
                                      length ? length : strlen(name))))
  {
    if (!udf->dlhandle)
      udf=0;					// Could not be opened
    else if (mark_used)
      udf->usage_count++;
  }
  mysql_rwlock_unlock(&THR_LOCK_udf);
  DBUG_RETURN(udf);
}


static void *find_udf_dl(const char *dl)
{
  DBUG_ENTER("find_udf_dl");

  /*
    Because only the function name is hashed, we have to search trough
    all rows to find the dl.
  */
  for (uint idx=0 ; idx < udf_hash.records ; idx++)
  {
    udf_func *udf=(udf_func*) my_hash_element(&udf_hash,idx);
    if (!strcmp(dl, udf->dl) && udf->dlhandle != NULL)
      DBUG_RETURN(udf->dlhandle);
  }
  DBUG_RETURN(0);
}


/* Assume that name && dl is already allocated */

static udf_func *add_udf(LEX_STRING *name, Item_result ret, char *dl,
                         Item_udftype type)
{
  if (!name || !dl || !(uint) type || (uint) type > (uint) UDFTYPE_AGGREGATE)
    return nullptr;

  udf_func *tmp= (udf_func*) alloc_root(&mem, sizeof(udf_func));
  if (!tmp)
    return nullptr;
  memset(tmp, 0, sizeof(*tmp));
  tmp->name = *name; //dup !!
  tmp->dl = dl;
  tmp->returns = ret;
  tmp->type = type;
  tmp->usage_count=1;

  mysql_rwlock_wrlock(&THR_LOCK_udf);

  if (my_hash_insert(&udf_hash,(uchar*)  tmp))
    tmp= nullptr;
  else
    using_udf_functions= 1;

  mysql_rwlock_unlock(&THR_LOCK_udf);
  return tmp;
}

/**
   Commit or rollback a transaction. Also close tables
   which it has opened and release metadata locks.
   Add/Remove from the in-memory hash depending on transaction
   commit or rollback and the bool flag passed to this function.

   @param thd                 THD context.
   @param rollback            Rollback transaction if true.
   @param udf                 Pointer to UDF function.
   @param insert_udf          Insert UDF in hash if true.

   @retval False - Success.
   @retval True  - Error.
*/

static bool udf_end_transaction(THD *thd, bool rollback,
                                udf_func *udf, bool insert_udf)
{
  bool result;
  bool rollback_transaction= thd->transaction_rollback_request || rollback;
  udf_func *u_f= nullptr;

  DBUG_ASSERT(stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END));

  if (!rollback_transaction && insert_udf)
  {
    udf->name.str= strdup_root(&mem,udf->name.str);
    udf->dl= strdup_root(&mem,udf->dl);
    // create entry in mysql.func table
    u_f= add_udf(&udf->name, udf->returns, udf->dl, udf->type);
    if (u_f != nullptr)
    {
      u_f->dlhandle= udf->dlhandle;
      u_f->func= udf->func;
      u_f->func_init= udf->func_init;
      u_f->func_deinit= udf->func_deinit;
      u_f->func_clear= udf->func_clear;
      u_f->func_add= udf->func_add;
    }
  }
  else
    udf_hash_delete(udf);

  rollback_transaction= rollback_transaction || (insert_udf && u_f == nullptr);

  /*
    Rollback the transaction if there is an error or there is a request by the
    SE (which is unlikely).
  */
  if (rollback_transaction)
  {
    result= trans_rollback_stmt(thd);
    result= result || trans_rollback_implicit(thd);
  }
  else
  {
    result= trans_commit_stmt(thd);
    result= result || trans_commit_implicit(thd);

  }

  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();

  return result || rollback || (insert_udf && u_f == nullptr);
}


/**
  Create a user defined function.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_create_function(THD *thd,udf_func *udf)
{
  bool error= true;
  void *dl= 0;
  int new_dl= 0;
  TABLE *table;
  TABLE_LIST tables;

  DBUG_ENTER("mysql_create_function");

  if (!initialized)
  {
    if (opt_noacl)
      my_error(ER_CANT_INITIALIZE_UDF, MYF(0),
               udf->name.str,
               "UDFs are unavailable with the --skip-grant-tables option");
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(error);
  }

  /*
    Ensure that the .dll doesn't have a path
    This is done to ensure that only approved dll from the system
    directories are used (to make this even remotely secure).
  */
  if (check_valid_path(udf->dl, strlen(udf->dl)))
  {
    my_error(ER_UDF_NO_PATHS, MYF(0));
    DBUG_RETURN(error);
  }
  LEX_CSTRING udf_name_cstr= {udf->name.str, udf->name.length};
  if (check_string_char_length(udf_name_cstr, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), udf->name.str);
    DBUG_RETURN(error);
  }

  /*
    Acquire MDL SNRW for TL_WRITE type so that deadlock and
    timeout errors are avoided from the Storage Engine.
  */
  tables.init_one_table(C_STRING_WITH_LEN("mysql"),
                        C_STRING_WITH_LEN("func"), "func", TL_WRITE,
                        MDL_SHARED_NO_READ_WRITE);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
    DBUG_RETURN(error);
  table= tables.table;
  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for CREATE FUNCTION command.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  mysql_rwlock_rdlock(&THR_LOCK_udf);
  if ((my_hash_search(&udf_hash,(uchar*) udf->name.str, udf->name.length)))
  {
    my_error(ER_UDF_EXISTS, MYF(0), udf->name.str);
    mysql_rwlock_unlock(&THR_LOCK_udf);
    DBUG_RETURN(error);
  }
  dl= find_udf_dl(udf->dl);
  mysql_rwlock_unlock(&THR_LOCK_udf);

  if (dl == nullptr)
  {
    char dlpath[FN_REFLEN];
    strxnmov(dlpath, sizeof(dlpath) - 1, opt_plugin_dir, "/", udf->dl, NullS);
    (void) unpack_filename(dlpath, dlpath);

    if (!(dl = dlopen(dlpath, RTLD_NOW)))
    {
      const char *errmsg;
      int error_number= dlopen_errno;
      DLERROR_GENERATE(errmsg, error_number);

      DBUG_PRINT("error",("dlopen of %s failed, error: %d (%s)",
                          udf->dl, error_number, errmsg));
      my_error(ER_CANT_OPEN_LIBRARY, MYF(0),
               udf->dl, error_number, errmsg);
      DBUG_RETURN(error);
    }
    new_dl= 1;
  }
  udf->dlhandle=dl;
  {
    char buf[NAME_LEN+16], *missing;
    if ((missing= init_syms(udf, buf)))
    {
      my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), missing);
      if (new_dl)
        dlclose(dl);
      DBUG_RETURN(error);
    }
  }

  // create entry in mysql.func table

  table->use_all_columns();
  restore_record(table, s->default_values);	// Default values for fields
  table->field[0]->store(udf->name.str, udf->name.length, system_charset_info);
  table->field[1]->store((longlong) udf->returns, TRUE);
  table->field[2]->store(udf->dl, strlen(udf->dl), system_charset_info);
  if (table->s->fields >= 4)			// If not old func format
    table->field[3]->store((longlong) udf->type, TRUE);
  error = (table->file->ha_write_row(table->record[0]) != 0);

  // Binlog the create function.
  if (!error)
    error= (write_bin_log(thd, true, thd->query().str, thd->query().length) != 0);

  error= udf_end_transaction(thd, thd->transaction_rollback_request || error,
                             udf, true);

  if (error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), "mysql.func", error,
             my_strerror(errbuf, sizeof(errbuf), error));
    if (new_dl)
      dlclose(dl);
  }
  DBUG_RETURN(error);
}


bool mysql_drop_function(THD *thd,const LEX_STRING *udf_name)
{
  TABLE *table;
  TABLE_LIST tables;
  udf_func *udf;
  bool error= true;

  DBUG_ENTER("mysql_drop_function");

  if (!initialized)
  {
    if (opt_noacl)
      my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), udf_name->str);
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    DBUG_RETURN(error);
  }

  tables.init_one_table(C_STRING_WITH_LEN("mysql"), C_STRING_WITH_LEN("func"),
                        "func", TL_WRITE, MDL_SHARED_NO_READ_WRITE);

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
    DBUG_RETURN(error);
  table= tables.table;
  /*
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for DROP FUNCTION command.
  */
  Save_and_Restore_binlog_format_state binlog_format_state(thd);

  mysql_rwlock_rdlock(&THR_LOCK_udf);
  if (!(udf=(udf_func*) my_hash_search(&udf_hash,(uchar*) udf_name->str,
                                       (uint) udf_name->length)))
  {
    my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), udf_name->str);
    mysql_rwlock_unlock(&THR_LOCK_udf);
    DBUG_RETURN(error);
  }
  mysql_rwlock_unlock(&THR_LOCK_udf);

  table->use_all_columns();
  table->field[0]->store(udf->name.str, udf->name.length, &my_charset_bin);
  if (!table->file->ha_index_read_idx_map(table->record[0], 0,
                                          table->field[0]->ptr,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT))
  {
    int delete_err;
    if ((delete_err = table->file->ha_delete_row(table->record[0])))
      table->file->print_error(delete_err, MYF(0));
    error= delete_err != 0;
  }

  /*
    Binlog the drop function. Keep the table open and locked
    while binlogging, to avoid binlog inconsistency.
  */
  if (!error)
    error= (write_bin_log(thd, true, thd->query().str,
                          thd->query().length) != 0);

  error= udf_end_transaction(thd, error, udf, false);

  /*
    Close the handle if this was function that was found during boot or
    CREATE FUNCTION and it's not in use by any other udf function
  */
  if (udf->dlhandle && !find_udf_dl(udf->dl))
    dlclose(udf->dlhandle);


  DBUG_RETURN(error);
}
