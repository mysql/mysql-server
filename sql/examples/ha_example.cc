/* Copyright (C) 2003 MySQL AB

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

#ifdef __GNUC__
#pragma implementation        // gcc: Class implementation
#endif

#include <mysql_priv.h>

#ifdef HAVE_EXAMPLE_DB
#include "ha_example.h"

/* Variables for example share methods */
pthread_mutex_t example_mutex;
static HASH example_open_tables;
static int example_init= 0;

static byte* example_get_key(EXAMPLE_SHARE *share,uint *length,
                             my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}


/*
  Example of simple lock controls.
*/
static EXAMPLE_SHARE *get_share(const char *table_name, TABLE *table)
{
  EXAMPLE_SHARE *share;
  uint length;
  char *tmp_name;

  if (!example_init)
  {
    /* Hijack a mutex for init'ing the storage engine */
    pthread_mutex_lock(&LOCK_mysql_create_db);
    if (!example_init)
    {
      example_init++;
      VOID(pthread_mutex_init(&example_mutex,MY_MUTEX_INIT_FAST));
      (void) hash_init(&example_open_tables,system_charset_info,32,0,0,
                       (hash_get_key) example_get_key,0,0);
    }
    pthread_mutex_unlock(&LOCK_mysql_create_db);
  }
  pthread_mutex_lock(&example_mutex);
  length=(uint) strlen(table_name);

  if (!(share=(EXAMPLE_SHARE*) hash_search(&example_open_tables,
                                           (byte*) table_name,
                                           length)))
  {
    if (!(share=(EXAMPLE_SHARE *)
          my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share),
                          &tmp_name, length+1,
                          NullS))) 
    {
      pthread_mutex_unlock(&example_mutex);
      return NULL;
    }

    share->use_count=0;
    share->table_name_length=length;
    share->table_name=tmp_name;
    strmov(share->table_name,table_name);
    if (my_hash_insert(&example_open_tables, (byte*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&example_mutex);

  return share;

error2:
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->mutex);
error:
  pthread_mutex_unlock(&example_mutex);
  my_free((gptr) share, MYF(0));

  return NULL;
}


/* 
  Free lock controls.
*/
static int free_share(EXAMPLE_SHARE *share)
{
  pthread_mutex_lock(&example_mutex);
  if (!--share->use_count)
  {
    hash_delete(&example_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&example_mutex);

  return 0;
}


const char **ha_example::bas_ext() const
{ static const char *ext[]= { NullS }; return ext; }


int ha_example::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_example::open");

  if (!(share = get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock,&lock,NULL);

  DBUG_RETURN(0);
}

int ha_example::close(void)
{
  DBUG_ENTER("ha_example::close");
  DBUG_RETURN(free_share(share));
}

int ha_example::write_row(byte * buf)
{
  DBUG_ENTER("ha_example::write_row");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::update_row(const byte * old_data, byte * new_data)
{

  DBUG_ENTER("ha_example::update_row");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::delete_row(const byte * buf)
{
  DBUG_ENTER("ha_example::delete_row");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::index_read(byte * buf, const byte * key,
                           uint key_len __attribute__((unused)),
                           enum ha_rkey_function find_flag
                           __attribute__((unused)))
{
  DBUG_ENTER("ha_example::index_read");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::index_read_idx(byte * buf, uint index, const byte * key,
                               uint key_len __attribute__((unused)),
                               enum ha_rkey_function find_flag
                               __attribute__((unused)))
{
  DBUG_ENTER("ha_example::index_read_idx");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}


int ha_example::index_next(byte * buf)
{
  DBUG_ENTER("ha_example::index_next");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::index_prev(byte * buf)
{
  DBUG_ENTER("ha_example::index_prev");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::index_first(byte * buf)
{
  DBUG_ENTER("ha_example::index_first");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::index_last(byte * buf)
{
  DBUG_ENTER("ha_example::index_last");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::rnd_init(bool scan)
{
  DBUG_ENTER("ha_example::rnd_init");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::rnd_next(byte *buf)
{
  DBUG_ENTER("ha_example::rnd_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}

void ha_example::position(const byte *record)
{
  DBUG_ENTER("ha_example::position");
  DBUG_VOID_RETURN;
}

int ha_example::rnd_pos(byte * buf, byte *pos)
{
  DBUG_ENTER("ha_example::rnd_pos");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

void ha_example::info(uint flag)
{
  DBUG_ENTER("ha_example::info");
  DBUG_VOID_RETURN;
}

int ha_example::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_example::extra");
  DBUG_RETURN(0);
}

int ha_example::reset(void)
{
  DBUG_ENTER("ha_example::reset");
  DBUG_RETURN(0);
}


int ha_example::delete_all_rows()
{
  DBUG_ENTER("ha_example::delete_all_rows");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

int ha_example::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_example::external_lock");
  DBUG_RETURN(0);
}

THR_LOCK_DATA **ha_example::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

int ha_example::delete_table(const char *name)
{
  DBUG_ENTER("ha_example::delete_table");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
}

int ha_example::rename_table(const char * from, const char * to)
{
  DBUG_ENTER("ha_example::rename_table ");
  DBUG_RETURN(HA_ERR_NOT_IMPLEMENTED);
}

ha_rows ha_example::records_in_range(int inx,
                                     const byte *start_key,uint start_key_len,
                                     enum ha_rkey_function start_search_flag,
                                     const byte *end_key,uint end_key_len,
                                     enum ha_rkey_function end_search_flag)
{
  DBUG_ENTER("ha_example::records_in_range ");
  DBUG_RETURN(records); // HA_ERR_NOT_IMPLEMENTED 
}


int ha_example::create(const char *name, TABLE *table_arg, HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_example::create");
  /* This is not implemented but we want someone to be able that it works. */
  DBUG_RETURN(0);
}
#endif /* HAVE_EXAMPLE_DB */
