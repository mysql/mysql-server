/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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
** Known bugs:
**
** Memory for functions are never freed!
** Shared libraries are not closed before mysqld exists;
**   - This is because we can't be sure if some threads is using
**     a functions.
**
** The buggs only affects applications that creates and frees a lot of
** dynamic functions, so this shouldn't be a real problem.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: implement sql_udf.h
#endif

#include "mysql_priv.h"
#ifdef HAVE_DLOPEN
extern "C"
{
#include <dlfcn.h>
#include <stdarg.h>
#include <hash.h>
}

#ifndef RTLD_NOW
#define RTLD_NOW 1				// For FreeBSD 2.2.2
#endif

#ifndef HAVE_DLERROR
#define dlerror() ""
#endif

static bool initialized = 0;
static MEM_ROOT mem;
static HASH udf_hash;
static pthread_mutex_t THR_LOCK_udf;


static udf_func *add_udf(char *name, Item_result ret, char *dl,
			 Item_udftype typ);
static void del_udf(udf_func *udf);
static void *find_udf_dl(const char *dl);

static void init_syms(udf_func *tmp)
{
  char nm[MAX_FIELD_NAME+16],*end;

  tmp->func = dlsym(tmp->dlhandle, tmp->name);
  end=strmov(nm,tmp->name);
  (void) strmov(end,"_init");
  tmp->func_init = dlsym(tmp->dlhandle, nm);
  (void) strmov(end,"_deinit");
  tmp->func_deinit = dlsym(tmp->dlhandle, nm);
  if (tmp->type == UDFTYPE_AGGREGATE)
  {
    (void)strmov( end, "_reset" );
    tmp->func_reset = dlsym( tmp->dlhandle, nm );
    (void)strmov( end, "_add" );
    tmp->func_add = dlsym( tmp->dlhandle, nm );
  }
}

static byte* get_hash_key(const byte *buff,uint *length,
			  my_bool not_used __attribute__((unused)))
{
  udf_func *udf=(udf_func*) buff;
  *length=(uint) udf->name_length;
  return (byte*) udf->name;
}

/*
** Read all predeclared functions from func@mysql and accept all that
** can be used.
*/

void udf_init()
{
  udf_func *tmp;
  TABLE_LIST tables;
  READ_RECORD read_record_info;
  int error;
  DBUG_ENTER("ufd_init");

  if (initialized)
    DBUG_VOID_RETURN;

  pthread_mutex_init(&THR_LOCK_udf,NULL);

  init_sql_alloc(&mem, 1024,0);
  THD *new_thd = new THD;
  if (!new_thd ||
      hash_init(&udf_hash,32,0,0,get_hash_key, NULL, HASH_CASE_INSENSITIVE))
  {
    sql_print_error("Can't allocate memory for udf structures");
    hash_free(&udf_hash);
    free_root(&mem,MYF(0));
    DBUG_VOID_RETURN;
  }
  initialized = 1;
  new_thd->mysys_var=my_thread_var;
  new_thd->version = refresh_version;	//current_thd->version;
  new_thd->current_tablenr = 0;
  new_thd->open_tables = 0;
  new_thd->db = my_strdup("mysql", MYF(0));

  bzero((gptr) &tables,sizeof(tables));
  tables.name = tables.real_name = (char*) "func";
  tables.lock_type = TL_READ;
  tables.db=new_thd->db;

  if (open_tables(new_thd, &tables))
  {
    DBUG_PRINT("error",("Can't open udf table"));
    sql_print_error("Can't open mysql/func table");
    close_thread_tables(new_thd);
    delete new_thd;
    DBUG_VOID_RETURN;
  }

  TABLE *table = tables.table;
  init_read_record(&read_record_info, new_thd, table, NULL,1,0);
  while (!(error = read_record_info.read_record(&read_record_info)))
  {
    DBUG_PRINT("info",("init udf record"));
    char *name=get_field(&mem, table, 0);
    char *dl_name= get_field(&mem, table, 2);
    bool new_dl=0;
    Item_udftype udftype=UDFTYPE_FUNCTION;
    if (table->fields >= 4)			// New func table
      udftype=(Item_udftype) table->field[3]->val_int();

    if (!(tmp = add_udf(name,(Item_result) table->field[1]->val_int(),
			dl_name, udftype)))
    {
      sql_print_error("Can't alloc memory for udf function: name");
      continue;
    }

    void *dl = find_udf_dl(tmp->dl);
    if (dl == NULL)
    {
      if (!(dl = dlopen(tmp->dl, RTLD_NOW)))
      {
	sql_print_error(ER(ER_CANT_OPEN_LIBRARY),
			tmp->dl,errno,dlerror());
	del_udf(tmp);
	continue;
      }
      new_dl=1;
    }
    tmp->dlhandle = dl;
    init_syms(tmp);
    if (!tmp->func)
    {
      sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), name);
      del_udf(tmp);
      if (new_dl)
	dlclose(dl);
    }
  }
  if (error > 0)
    sql_print_error(ER(ER_GET_ERRNO), my_errno);
  end_read_record(&read_record_info);
  new_thd->version--;				// Force close to free memory
  close_thread_tables(new_thd);
  delete new_thd;
  DBUG_VOID_RETURN;
}


void udf_free()
{
  /* close all shared libraries */
  DBUG_ENTER("udf_free");
  for (uint idx=0 ; idx < udf_hash.records ; idx++)
  {
    udf_func *udf=(udf_func*) hash_element(&udf_hash,idx);
    if (udf->dl)
    {
      for (uint j=idx+1 ;  j < udf_hash.records ; j++)
      {
	udf_func *tmp=(udf_func*) hash_element(&udf_hash,j);
	if (tmp->dl && !strcmp(udf->dl,tmp->dl))
	  tmp->dl=0;
      }
    }
    dlclose(udf->dlhandle);
  }
  hash_free(&udf_hash);
  free_root(&mem,MYF(0));
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
    ** The functions is in use ; Rename the functions instead of removing it.
    ** The functions will be automaticly removed when the least threads
    ** doesn't use it anymore
    */
    char *name= udf->name;
    uint name_length=udf->name_length;
    udf->name=(char*) "*";
    udf->name_length=1;
    hash_update(&udf_hash,(byte*) udf,name,name_length);
  }
  DBUG_VOID_RETURN;
}


void free_udf(udf_func *udf)
{
  DBUG_ENTER("free_udf");
  pthread_mutex_lock(&THR_LOCK_udf);
  if (!--udf->usage_count)
  {
    hash_delete(&udf_hash,(byte*) udf);
    using_udf_functions=udf_hash.records != 0;
    if (!find_udf_dl(udf->dl))
      dlclose(udf->dlhandle);
  }
  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_VOID_RETURN;
}

/* This is only called if using_udf_functions != 0 */

udf_func *find_udf(const char *name,uint length,bool mark_used)
{
  udf_func *udf=0;
  DBUG_ENTER("find_udf");

  /* TODO: This should be changed to reader locks someday! */
  pthread_mutex_lock(&THR_LOCK_udf);
  udf=(udf_func*) hash_search(&udf_hash,name,
			      length ? length : (uint) strlen(name));
  if (mark_used)
    udf->usage_count++;
  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_RETURN(udf);
}

static void *find_udf_dl(const char *dl)
{
  DBUG_ENTER("find_udf_dl");

  /* because only the function name is hashed, we have to search trough
  ** all rows to find the dl.
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

static udf_func *add_udf(char *name, Item_result ret, char *dl,
			 Item_udftype type)
{
  if (!name || !dl)
    return 0;
  udf_func *tmp= (udf_func*) alloc_root(&mem, sizeof(udf_func));
  if (!tmp)
    return 0;
  bzero((char*) tmp,sizeof(*tmp));
  tmp->name = name;
  tmp->name_length=(uint) strlen(tmp->name);
  tmp->dl = dl;
  tmp->returns = ret;
  tmp->type = type;
  tmp->usage_count=1;
  if (hash_insert(&udf_hash,(char*) tmp))
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
    send_error(&thd->net, ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES));
    DBUG_RETURN(1);
  }

  /*
    Ensure that the .dll doesn't have a path
    This is done to ensure that only approved dll from the system
    directories are used (to make this even remotely secure).
  */
  if (strchr(udf->dl, '/'))
  {
    send_error(&thd->net, ER_UDF_NO_PATHS,ER(ER_UDF_NO_PATHS));
    DBUG_RETURN(1);
  }
  if (udf->name_length > NAME_LEN)
  {
    net_printf(&thd->net, ER_TOO_LONG_IDENT,udf->name);
    DBUG_RETURN(1);
  }

  pthread_mutex_lock(&THR_LOCK_udf);
  if (hash_search(&udf_hash,udf->name, udf->name_length))
  {
    net_printf(&thd->net, ER_UDF_EXISTS, udf->name);
    goto err;
  }
  if (!(dl = find_udf_dl(udf->dl)))
  {
    if (!(dl = dlopen(udf->dl, RTLD_NOW)))
    {
      DBUG_PRINT("error",("dlopen of %s failed, error: %d (%s)",
			  udf->dl,errno,dlerror()));
      net_printf(&thd->net, ER_CANT_OPEN_LIBRARY, udf->dl, errno, dlerror());
      goto err;
    }
    new_dl=1;
  }
  udf->dlhandle=dl;
  init_syms(udf);

  if (udf->func == NULL)
  {
    net_printf(&thd->net, ER_CANT_FIND_DL_ENTRY, udf->name);
    goto err;
  }
  udf->name=strdup_root(&mem,udf->name);
  udf->dl=strdup_root(&mem,udf->dl);
  if (!udf->name || !udf->dl ||
      !(u_d=add_udf(udf->name,udf->returns,udf->dl,udf->type)))
  {
    send_error(&thd->net,0);		// End of memory
    goto err;
  }
  u_d->dlhandle = dl;
  u_d->func=udf->func;
  u_d->func_init=udf->func_init;
  u_d->func_deinit=udf->func_deinit;
  u_d->func_reset=udf->func_reset;
  u_d->func_add=udf->func_add;

  /* create entry in mysql/func table */

  bzero((char*) &tables,sizeof(tables));
  tables.db= (char*) "mysql";
  tables.real_name=tables.name= (char*) "func";
  /* Allow creation of functions even if we can't open func table */
  if (!(table = open_ltable(thd,&tables,TL_WRITE)))
    goto err;

  restore_record(table,2);		// Get default values for fields
  table->field[0]->store(u_d->name, u_d->name_length);
  table->field[1]->store((longlong) u_d->returns);
  table->field[2]->store(u_d->dl,(uint) strlen(u_d->dl));
  if (table->fields >= 4)			// If not old func format
    table->field[3]->store((longlong) u_d->type);
  error = table->file->write_row(table->record[0]);

  close_thread_tables(thd);
  if (error)
  {
    net_printf(&thd->net, ER_ERROR_ON_WRITE, "func@mysql",error);
    del_udf(u_d);
    goto err;
  }
  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_RETURN(0);

 err:
  if (new_dl)
    dlclose(dl);
  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_RETURN(1);
}


int mysql_drop_function(THD *thd,const char *udf_name)
{
  TABLE *table;
  TABLE_LIST tables;
  udf_func *udf;
  DBUG_ENTER("mysql_drop_function");
  if (!initialized)
  {
    send_error(&thd->net, ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&THR_LOCK_udf);
  if (!(udf=(udf_func*) hash_search(&udf_hash,udf_name, (uint) strlen(udf_name))))
  {
    net_printf(&thd->net, ER_FUNCTION_NOT_DEFINED, udf_name);
    goto err;
  }
  del_udf(udf);
  if (!find_udf_dl(udf->dl))
    dlclose(udf->dlhandle);

  bzero((char*) &tables,sizeof(tables));
  tables.db=(char*) "mysql";
  tables.real_name=tables.name=(char*) "func";
  if (!(table = open_ltable(thd,&tables,TL_WRITE)))
    goto err;
  if (!table->file->index_read_idx(table->record[0],0,(byte*) udf_name,
				   (uint) strlen(udf_name),
				   HA_READ_KEY_EXACT))
  {
    int error;
    if ((error = table->file->delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }
  close_thread_tables(thd);

  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_RETURN(0);
 err:
  pthread_mutex_unlock(&THR_LOCK_udf);
  DBUG_RETURN(1);
}

#endif /* HAVE_DLOPEN */

/*
**	Local variables:
**	tab-width: 8
**	c-basic-offset: 2
**	End:
*/
