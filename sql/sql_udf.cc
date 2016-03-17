/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_base.h"                           // close_mysql_tables
#include "sql_parse.h"                        // check_string_char_length
#include "sql_table.h"                        // write_bin_log
#include "records.h"          // init_read_record, end_read_record
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT
#include "log.h"
#include "sql_plugin.h"                         // check_valid_path

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef HAVE_DLOPEN
#include <stdarg.h>
#include <hash.h>

static bool initialized = 0;
static MEM_ROOT mem;
static HASH udf_hash;
static mysql_rwlock_t THR_LOCK_udf;


static udf_func *add_udf(LEX_STRING *name, Item_result ret,
                         char *dl, Item_udftype typ);
static void del_udf(udf_func *udf);
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
    to prefent loading "udf" from, e.g. libc.so
    let's ensure that at least one auxiliary symbol is defined
  */
  if (!tmp->func_init && !tmp->func_deinit && tmp->type != UDFTYPE_AGGREGATE)
  {
    if (!opt_allow_suspicious_udfs)
      return nm;
    sql_print_warning(ER(ER_CANT_FIND_DL_ENTRY), nm);
  }
  return 0;
}


extern "C" uchar* get_hash_key(const uchar *buff, size_t *length,
			      my_bool not_used MY_ATTRIBUTE((unused)))
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

  count= array_elements(all_udf_rwlocks);
  mysql_rwlock_register(category, all_udf_rwlocks, count);

  count= array_elements(all_udf_memory);
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
  THD *new_thd = new THD;
  if (!new_thd ||
      my_hash_init(&udf_hash,system_charset_info,32,0,0,get_hash_key, NULL, 0,
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

  tables.init_one_table(db, sizeof(db)-1, "func", 4, "func", TL_READ);

  if (open_and_lock_tables(new_thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    DBUG_PRINT("error",("Can't open udf table"));
    sql_print_error("Can't open the mysql.func table. Please "
                    "run mysql_upgrade to create it.");
    goto end;
  }

  table= tables.table;
  if (init_read_record(&read_record_info, new_thd, table, NULL, 1, 1, FALSE))
    goto end;
  table->use_all_columns();
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

	/* Print warning to log */
        sql_print_error(ER(ER_CANT_OPEN_LIBRARY), tmp->dl, error_number, errmsg);
	/* Keep the udf in the hash so that we can remove it later */
	continue;
      }
      new_dl=1;
    }
    tmp->dlhandle = dl;
    {
      char buf[NAME_LEN+16], *missing;
      if ((missing= init_syms(tmp, buf)))
      {
        sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), missing);
        del_udf(tmp);
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
  close_mysql_tables(new_thd);
  delete new_thd;
  DBUG_VOID_RETURN;
}


void udf_free()
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


static void del_udf(udf_func *udf)
{
  DBUG_ENTER("del_udf");
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
    return 0;
  udf_func *tmp= (udf_func*) alloc_root(&mem, sizeof(udf_func));
  if (!tmp)
    return 0;
  memset(tmp, 0, sizeof(*tmp));
  tmp->name = *name; //dup !!
  tmp->dl = dl;
  tmp->returns = ret;
  tmp->type = type;
  tmp->usage_count=1;
  if (my_hash_insert(&udf_hash,(uchar*)  tmp))
    return 0;
  using_udf_functions=1;
  return tmp;
}


/**
  Create a user defined function. 

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

int mysql_create_function(THD *thd,udf_func *udf)
{
  int error;
  void *dl=0;
  bool new_dl=0;
  TABLE *table;
  TABLE_LIST tables;
  udf_func *u_d;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_create_function");

  if (!initialized)
  {
    if (opt_noacl)
      my_error(ER_CANT_INITIALIZE_UDF, MYF(0),
               udf->name.str,
               "UDFs are unavailable with the --skip-grant-tables option");
    else
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
    DBUG_RETURN(1);
  }

  /*
    Ensure that the .dll doesn't have a path
    This is done to ensure that only approved dll from the system
    directories are used (to make this even remotely secure).
  */
  if (check_valid_path(udf->dl, strlen(udf->dl)))
  {
    my_message(ER_UDF_NO_PATHS, ER(ER_UDF_NO_PATHS), MYF(0));
    DBUG_RETURN(1);
  }
  LEX_CSTRING udf_name_cstr= {udf->name.str, udf->name.length};
  if (check_string_char_length(udf_name_cstr, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), udf->name.str);
    DBUG_RETURN(1);
  }

  tables.init_one_table("mysql", 5, "func", 4, "func", TL_WRITE);
  if (!(table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(1);

  /* 
    Turn off row binlogging of this statement and use statement-based 
    so that all supporting tables are updated for CREATE FUNCTION command.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  mysql_rwlock_wrlock(&THR_LOCK_udf);
  if ((my_hash_search(&udf_hash,(uchar*) udf->name.str, udf->name.length)))
  {
    my_error(ER_UDF_EXISTS, MYF(0), udf->name.str);
    goto err;
  }
  if (!(dl = find_udf_dl(udf->dl)))
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
      goto err;
    }
    new_dl=1;
  }
  udf->dlhandle=dl;
  {
    char buf[NAME_LEN+16], *missing;
    if ((missing= init_syms(udf, buf)))
    {
      my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), missing);
      goto err;
    }
  }
  udf->name.str=strdup_root(&mem,udf->name.str);
  udf->dl=strdup_root(&mem,udf->dl);
  if (!(u_d=add_udf(&udf->name,udf->returns,udf->dl,udf->type)))
    goto err;
  u_d->dlhandle = dl;
  u_d->func=udf->func;
  u_d->func_init=udf->func_init;
  u_d->func_deinit=udf->func_deinit;
  u_d->func_clear=udf->func_clear;
  u_d->func_add=udf->func_add;

  /* create entry in mysql.func table */

  table->use_all_columns();
  restore_record(table, s->default_values);	// Default values for fields
  table->field[0]->store(u_d->name.str, u_d->name.length, system_charset_info);
  table->field[1]->store((longlong) u_d->returns, TRUE);
  table->field[2]->store(u_d->dl, strlen(u_d->dl), system_charset_info);
  if (table->s->fields >= 4)			// If not old func format
    table->field[3]->store((longlong) u_d->type, TRUE);
  error = table->file->ha_write_row(table->record[0]);

  if (error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), "mysql.func", error,
             my_strerror(errbuf, sizeof(errbuf), error));
    del_udf(u_d);
    goto err;
  }
  mysql_rwlock_unlock(&THR_LOCK_udf);

  /* Binlog the create function. */
  if (write_bin_log(thd, true, thd->query().str, thd->query().length))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(1);
  }
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(0);

 err:
  if (new_dl)
    dlclose(dl);
  mysql_rwlock_unlock(&THR_LOCK_udf);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(1);
}


int mysql_drop_function(THD *thd,const LEX_STRING *udf_name)
{
  TABLE *table;
  TABLE_LIST tables;
  udf_func *udf;
  char *exact_name_str;
  size_t exact_name_len;
  bool save_binlog_row_based;
  int error= 1;
  DBUG_ENTER("mysql_drop_function");

  if (!initialized)
  {
    if (opt_noacl)
      my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), udf_name->str);
    else
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
    DBUG_RETURN(1);
  }

  tables.init_one_table("mysql", 5, "func", 4, "func", TL_WRITE);
  if (!(table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(1);

  /* 
    Turn off row binlogging of this statement and use statement-based
    so that all supporting tables are updated for DROP FUNCTION command.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  mysql_rwlock_wrlock(&THR_LOCK_udf);
  if (!(udf=(udf_func*) my_hash_search(&udf_hash,(uchar*) udf_name->str,
                                       (uint) udf_name->length)))
  {
    my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), udf_name->str);
    mysql_rwlock_unlock(&THR_LOCK_udf);
    goto exit;
  }
  exact_name_str= udf->name.str;
  exact_name_len= udf->name.length;
  del_udf(udf);
  /*
    Close the handle if this was function that was found during boot or
    CREATE FUNCTION and it's not in use by any other udf function
  */
  if (udf->dlhandle && !find_udf_dl(udf->dl))
    dlclose(udf->dlhandle);
  mysql_rwlock_unlock(&THR_LOCK_udf);

  table->use_all_columns();
  table->field[0]->store(exact_name_str, exact_name_len, &my_charset_bin);
  if (!table->file->ha_index_read_idx_map(table->record[0], 0,
                                          table->field[0]->ptr,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT))
  {
    int delete_err;
    if ((delete_err = table->file->ha_delete_row(table->record[0])))
      table->file->print_error(delete_err, MYF(0));
  }

  /*
    Binlog the drop function. Keep the table open and locked
    while binlogging, to avoid binlog inconsistency.
  */
  if (!write_bin_log(thd, true, thd->query().str, thd->query().length))
    error= 0;
exit:
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(error);
}

#endif /* HAVE_DLOPEN */
