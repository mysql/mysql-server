/* Copyright (C) 2000 MySQL AB

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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <my_pthread.h>

#ifdef HAVE_DLOPEN
extern "C"
{
#include <stdarg.h>
#include <hash.h>
}

static bool initialized = 0;
static MEM_ROOT mem;
static HASH udf_hash;
static rw_lock_t THR_LOCK_udf;


static udf_func *add_udf(LEX_STRING *name, Item_result ret,
                         char *dl, Item_udftype typ);
static void del_udf(udf_func *udf);
static void *find_udf_dl(const char *dl);

static char *init_syms(udf_func *tmp, char *nm)
{
  char *end;

  if (!((tmp->func= (Udf_func_any) dlsym(tmp->dlhandle, tmp->name.str))))
    return tmp->name.str;

  end=strmov(nm,tmp->name.str);

  if (tmp->type == UDFTYPE_AGGREGATE)
  {
    (void)strmov(end, "_clear");
    if (!((tmp->func_clear= (Udf_func_clear) dlsym(tmp->dlhandle, nm))))
      return nm;
    (void)strmov(end, "_add");
    if (!((tmp->func_add= (Udf_func_add) dlsym(tmp->dlhandle, nm))))
      return nm;
  }

  (void) strmov(end,"_deinit");
  tmp->func_deinit= (Udf_func_deinit) dlsym(tmp->dlhandle, nm);

  (void) strmov(end,"_init");
  tmp->func_init= (Udf_func_init) dlsym(tmp->dlhandle, nm);

  /*
    to prefent loading "udf" from, e.g. libc.so
    let's ensure that at least one auxiliary symbol is defined
  */
  if (!tmp->func_init && !tmp->func_deinit && tmp->type != UDFTYPE_AGGREGATE)
  {
    if (!opt_allow_suspicious_udfs)
      return nm;
    if (current_thd->variables.log_warnings)
      sql_print_warning(ER(ER_CANT_FIND_DL_ENTRY), nm);
  }
  return 0;
}


extern "C" byte* get_hash_key(const byte *buff,uint *length,
			      my_bool not_used __attribute__((unused)))
{
  udf_func *udf=(udf_func*) buff;
  *length=(uint) udf->name.length;
  return (byte*) udf->name.str;
}


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

  my_rwlock_init(&THR_LOCK_udf,NULL);
  
  init_sql_alloc(&mem, UDF_ALLOC_BLOCK_SIZE, 0);
  THD *new_thd = new THD;
  if (!new_thd ||
      hash_init(&udf_hash,system_charset_info,32,0,0,get_hash_key, NULL, 0))
  {
    sql_print_error("Can't allocate memory for udf structures");
    hash_free(&udf_hash);
    free_root(&mem,MYF(0));
    delete new_thd;
    DBUG_VOID_RETURN;
  }
  initialized = 1;
  new_thd->thread_stack= (char*) &new_thd;
  new_thd->store_globals();
  new_thd->set_db(db, sizeof(db)-1);

  bzero((gptr) &tables,sizeof(tables));
  tables.alias= tables.table_name= (char*) "func";
  tables.lock_type = TL_READ;
  tables.db= db;

  if (simple_open_n_lock_tables(new_thd, &tables))
  {
    DBUG_PRINT("error",("Can't open udf table"));
    sql_print_error("Can't open the mysql.func table. Please run the mysql_upgrade script to create it.");
    goto end;
  }

  table= tables.table;
  init_read_record(&read_record_info, new_thd, table, NULL,1,0);
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
    */
    if (my_strchr(files_charset_info, dl_name, dl_name + strlen(dl_name), FN_LIBCHAR) || 
		strlen(name.str) > NAME_LEN)
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
      if (!(dl= dlopen(tmp->dl, RTLD_NOW)))
      {
	/* Print warning to log */
	sql_print_error(ER(ER_CANT_OPEN_LIBRARY), tmp->dl, errno, dlerror());
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
    sql_print_error(ER(ER_GET_ERRNO), my_errno);
  end_read_record(&read_record_info);
  new_thd->version--;				// Force close to free memory

end:
  close_thread_tables(new_thd);
  delete new_thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_VOID_RETURN;
}


void udf_free()
{
  /* close all shared libraries */
  DBUG_ENTER("udf_free");
  for (uint idx=0 ; idx < udf_hash.records ; idx++)
  {
    udf_func *udf=(udf_func*) hash_element(&udf_hash,idx);
    if (udf->dlhandle)				// Not closed before
    {
      /* Mark all versions using the same handler as closed */
      for (uint j=idx+1 ;  j < udf_hash.records ; j++)
      {
	udf_func *tmp=(udf_func*) hash_element(&udf_hash,j);
	if (udf->dlhandle == tmp->dlhandle)
	  tmp->dlhandle=0;			// Already closed
      }
      dlclose(udf->dlhandle);
    }
  }
  hash_free(&udf_hash);
  free_root(&mem,MYF(0));
  if (initialized)
  {
    initialized= 0;
    rwlock_destroy(&THR_LOCK_udf);
  }
  DBUG_VOID_RETURN;
}


static void del_udf(udf_func *udf)
{
  DBUG_ENTER("del_udf");
  if (!--udf->usage_count)
  {
    hash_delete(&udf_hash,(byte*) udf);
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
    uint name_length=udf->name.length;
    udf->name.str=(char*) "*";
    udf->name.length=1;
    hash_update(&udf_hash,(byte*) udf,(byte*) name,name_length);
  }
  DBUG_VOID_RETURN;
}


void free_udf(udf_func *udf)
{
  DBUG_ENTER("free_udf");
  
  if (!initialized)
    DBUG_VOID_RETURN;

  rw_wrlock(&THR_LOCK_udf);
  if (!--udf->usage_count)
  {
    /*
      We come here when someone has deleted the udf function
      while another thread still was using the udf
    */
    hash_delete(&udf_hash,(byte*) udf);
    using_udf_functions=udf_hash.records != 0;
    if (!find_udf_dl(udf->dl))
      dlclose(udf->dlhandle);
  }
  rw_unlock(&THR_LOCK_udf);
  DBUG_VOID_RETURN;
}


/* This is only called if using_udf_functions != 0 */

udf_func *find_udf(const char *name,uint length,bool mark_used)
{
  udf_func *udf=0;
  DBUG_ENTER("find_udf");

  if (!initialized)
    DBUG_RETURN(NULL);

  /* TODO: This should be changed to reader locks someday! */
  if (mark_used)
    rw_wrlock(&THR_LOCK_udf);  /* Called during fix_fields */
  else
    rw_rdlock(&THR_LOCK_udf);  /* Called during parsing */

  if ((udf=(udf_func*) hash_search(&udf_hash,(byte*) name,
				   length ? length : (uint) strlen(name))))
  {
    if (!udf->dlhandle)
      udf=0;					// Could not be opened
    else if (mark_used)
      udf->usage_count++;
  }
  rw_unlock(&THR_LOCK_udf);
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
    udf_func *udf=(udf_func*) hash_element(&udf_hash,idx);
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
  bzero((char*) tmp,sizeof(*tmp));
  tmp->name = *name; //dup !!
  tmp->dl = dl;
  tmp->returns = ret;
  tmp->type = type;
  tmp->usage_count=1;
  if (my_hash_insert(&udf_hash,(byte*)  tmp))
    return 0;
  using_udf_functions=1;
  return tmp;
}


int mysql_create_function(THD *thd,udf_func *udf)
{
  int error;
  void *dl=0;
  bool new_dl=0;
  TABLE *table;
  TABLE_LIST tables;
  udf_func *u_d;
  DBUG_ENTER("mysql_create_function");

  if (!initialized)
  {
    my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
    DBUG_RETURN(1);
  }

  /*
    Ensure that the .dll doesn't have a path
    This is done to ensure that only approved dll from the system
    directories are used (to make this even remotely secure).
  */
  if (my_strchr(files_charset_info, udf->dl, udf->dl + strlen(udf->dl), FN_LIBCHAR))
  {
    my_message(ER_UDF_NO_PATHS, ER(ER_UDF_NO_PATHS), MYF(0));
    DBUG_RETURN(1);
  }
  if (udf->name.length > NAME_LEN)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), udf->name);
    DBUG_RETURN(1);
  }

  rw_wrlock(&THR_LOCK_udf);
  if ((hash_search(&udf_hash,(byte*) udf->name.str, udf->name.length)))
  {
    my_error(ER_UDF_EXISTS, MYF(0), udf->name);
    goto err;
  }
  if (!(dl = find_udf_dl(udf->dl)))
  {
    if (!(dl = dlopen(udf->dl, RTLD_NOW)))
    {
      DBUG_PRINT("error",("dlopen of %s failed, error: %d (%s)",
			  udf->dl, errno, dlerror()));
      my_error(ER_CANT_OPEN_LIBRARY, MYF(0),
               udf->dl, errno, dlerror());
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

  bzero((char*) &tables,sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "func";
  /* Allow creation of functions even if we can't open func table */
  if (!(table = open_ltable(thd,&tables,TL_WRITE)))
    goto err;
  table->use_all_columns();
  restore_record(table, s->default_values);	// Default values for fields
  table->field[0]->store(u_d->name.str, u_d->name.length, system_charset_info);
  table->field[1]->store((longlong) u_d->returns, TRUE);
  table->field[2]->store(u_d->dl,(uint) strlen(u_d->dl), system_charset_info);
  if (table->s->fields >= 4)			// If not old func format
    table->field[3]->store((longlong) u_d->type, TRUE);
  error = table->file->ha_write_row(table->record[0]);

  close_thread_tables(thd);
  if (error)
  {
    my_error(ER_ERROR_ON_WRITE, MYF(0), "mysql.func", error);
    del_udf(u_d);
    goto err;
  }
  rw_unlock(&THR_LOCK_udf);
  DBUG_RETURN(0);

 err:
  if (new_dl)
    dlclose(dl);
  rw_unlock(&THR_LOCK_udf);
  DBUG_RETURN(1);
}


int mysql_drop_function(THD *thd,const LEX_STRING *udf_name)
{
  TABLE *table;
  TABLE_LIST tables;
  udf_func *udf;
  DBUG_ENTER("mysql_drop_function");
  if (!initialized)
  {
    my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
    DBUG_RETURN(1);
  }
  rw_wrlock(&THR_LOCK_udf);  
  if (!(udf=(udf_func*) hash_search(&udf_hash,(byte*) udf_name->str,
				    (uint) udf_name->length)))
  {
    my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), udf_name->str);
    goto err;
  }
  del_udf(udf);
  /*
    Close the handle if this was function that was found during boot or
    CREATE FUNCTION and it's not in use by any other udf function
  */
  if (udf->dlhandle && !find_udf_dl(udf->dl))
    dlclose(udf->dlhandle);

  bzero((char*) &tables,sizeof(tables));
  tables.db=(char*) "mysql";
  tables.table_name= tables.alias= (char*) "func";
  if (!(table = open_ltable(thd,&tables,TL_WRITE)))
    goto err;
  table->use_all_columns();
  table->field[0]->store(udf_name->str, udf_name->length, system_charset_info);
  if (!table->file->index_read_idx(table->record[0], 0,
				   (byte*) table->field[0]->ptr,
				   table->key_info[0].key_length,
				   HA_READ_KEY_EXACT))
  {
    int error;
    if ((error = table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }
  close_thread_tables(thd);

  rw_unlock(&THR_LOCK_udf);  
  DBUG_RETURN(0);
 err:
  rw_unlock(&THR_LOCK_udf);
  DBUG_RETURN(1);
}

#endif /* HAVE_DLOPEN */
