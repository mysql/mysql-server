/* Copyright (C) 2005 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_BLACKHOLE_DB
#include "ha_blackhole.h"

/* Static declarations for shared structures */

static pthread_mutex_t blackhole_mutex;
static HASH blackhole_open_tables;
static int blackhole_init= FALSE;

static st_blackhole_share *get_share(const char *table_name);
static void free_share(st_blackhole_share *share);

/* Blackhole storage engine handlerton */

handlerton blackhole_hton= {
  "BLACKHOLE",
  SHOW_OPTION_YES,
  "/dev/null storage engine (anything you write to it disappears)",
  DB_TYPE_BLACKHOLE_DB,
  blackhole_db_init,
  0,       /* slot */
  0,       /* savepoint size. */
  NULL,    /* close_connection */
  NULL,    /* savepoint */
  NULL,    /* rollback to savepoint */
  NULL,    /* release savepoint */
  NULL,    /* commit */
  NULL,    /* rollback */
  NULL,    /* prepare */
  NULL,    /* recover */
  NULL,    /* commit_by_xid */
  NULL,    /* rollback_by_xid */
  NULL,    /* create_cursor_read_view */
  NULL,    /* set_cursor_read_view */
  NULL,    /* close_cursor_read_view */
  HTON_CAN_RECREATE
};

/*****************************************************************************
** BLACKHOLE tables
*****************************************************************************/

ha_blackhole::ha_blackhole(TABLE *table_arg)
  :handler(&blackhole_hton, table_arg)
{}


static const char *ha_blackhole_exts[] = {
  NullS
};

const char **ha_blackhole::bas_ext() const
{
  return ha_blackhole_exts;
}

int ha_blackhole::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_blackhole::open");

  if (!(share= get_share(name)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&share->lock, &lock, NULL);
  DBUG_RETURN(0);
}

int ha_blackhole::close(void)
{
  DBUG_ENTER("ha_blackhole::close");
  free_share(share);
  DBUG_RETURN(0);
}

int ha_blackhole::create(const char *name, TABLE *table_arg,
                         HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_blackhole::create");
  DBUG_RETURN(0);
}

const char *ha_blackhole::index_type(uint key_number)
{
  DBUG_ENTER("ha_blackhole::index_type");
  DBUG_RETURN((table->key_info[key_number].flags & HA_FULLTEXT) ? 
              "FULLTEXT" :
              (table->key_info[key_number].flags & HA_SPATIAL) ?
              "SPATIAL" :
              (table->key_info[key_number].algorithm == HA_KEY_ALG_RTREE) ?
              "RTREE" :
              "BTREE");
}

int ha_blackhole::write_row(byte * buf)
{
  DBUG_ENTER("ha_blackhole::write_row");
  DBUG_RETURN(0);
}

int ha_blackhole::rnd_init(bool scan)
{
  DBUG_ENTER("ha_blackhole::rnd_init");
  DBUG_RETURN(0);
}


int ha_blackhole::rnd_next(byte *buf)
{
  DBUG_ENTER("ha_blackhole::rnd_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::rnd_pos(byte * buf, byte *pos)
{
  DBUG_ENTER("ha_blackhole::rnd_pos");
  DBUG_ASSERT(0);
  DBUG_RETURN(0);
}


void ha_blackhole::position(const byte *record)
{
  DBUG_ENTER("ha_blackhole::position");
  DBUG_ASSERT(0);
  DBUG_VOID_RETURN;
}


int ha_blackhole::info(uint flag)
{
  DBUG_ENTER("ha_blackhole::info");

  records= 0;
  deleted= 0;
  errkey= 0;
  mean_rec_length= 0;
  data_file_length= 0;
  index_file_length= 0;
  max_data_file_length= 0;
  delete_length= 0;
  if (flag & HA_STATUS_AUTO)
    auto_increment_value= 1;
  DBUG_RETURN(0);
}

int ha_blackhole::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_blackhole::external_lock");
  DBUG_RETURN(0);
}


THR_LOCK_DATA **ha_blackhole::store_lock(THD *thd,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  DBUG_ENTER("ha_blackhole::store_lock");
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type = TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;
  DBUG_RETURN(to);
}


int ha_blackhole::index_read(byte * buf, const byte * key,
                             uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_blackhole::index_read");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_idx(byte * buf, uint idx, const byte * key,
                                 uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_blackhole::index_read_idx");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_last(byte * buf, const byte * key, uint key_len)
{
  DBUG_ENTER("ha_blackhole::index_read_last");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_next(byte * buf)
{
  DBUG_ENTER("ha_blackhole::index_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_prev(byte * buf)
{
  DBUG_ENTER("ha_blackhole::index_prev");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_first(byte * buf)
{
  DBUG_ENTER("ha_blackhole::index_first");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_last(byte * buf)
{
  DBUG_ENTER("ha_blackhole::index_last");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


static st_blackhole_share *get_share(const char *table_name)
{
  st_blackhole_share *share;
  uint length;

  length= (uint) strlen(table_name);
  pthread_mutex_lock(&blackhole_mutex);
    
  if (!(share= (st_blackhole_share*) hash_search(&blackhole_open_tables,
                                                 (byte*) table_name, length)))
  {
    if (!(share= (st_blackhole_share*) my_malloc(sizeof(st_blackhole_share) +
                                                 length,
                                                 MYF(MY_WME | MY_ZEROFILL))))
      goto error;

    share->table_name_length= length;
    strmov(share->table_name, table_name);
    
    if (my_hash_insert(&blackhole_open_tables, (byte*) share))
    {
      my_free((gptr) share, MYF(0));
      share= NULL;
      goto error;
    }
    
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  
error:
  pthread_mutex_unlock(&blackhole_mutex);
  return share;
}

static void free_share(st_blackhole_share *share)
{
  pthread_mutex_lock(&blackhole_mutex);
  if (!--share->use_count)
    hash_delete(&blackhole_open_tables, (byte*) share);
  pthread_mutex_unlock(&blackhole_mutex);
}


static byte* blackhole_get_key(st_blackhole_share *share, uint *length,
                               my_bool not_used __attribute__((unused)))
{
  *length= share->table_name_length;
  return (byte*) share->table_name;
}


static void blackhole_free_key(st_blackhole_share *share)
{
  thr_lock_delete(&share->lock);
  my_free((gptr) share, MYF(0));
} 


bool blackhole_db_init()
{
  DBUG_ENTER("blackhole_db_init");
  if (pthread_mutex_init(&blackhole_mutex, MY_MUTEX_INIT_FAST))
    goto error;
  if (hash_init(&blackhole_open_tables, &my_charset_bin, 32, 0, 0,
                    (hash_get_key) blackhole_get_key,
                    (hash_free_key) blackhole_free_key, 0))
  {
    VOID(pthread_mutex_destroy(&blackhole_mutex));
  }
  else
  {
    blackhole_init= TRUE;
    DBUG_RETURN(FALSE);
  }
error:
  have_blackhole_db= SHOW_OPTION_DISABLED;	// If we couldn't use handler
  DBUG_RETURN(TRUE);
}


bool blackhole_db_end()
{
  if (blackhole_init)
  {
    hash_free(&blackhole_open_tables);
    VOID(pthread_mutex_destroy(&blackhole_mutex));
  }
  blackhole_init= 0;
  return FALSE;
}

#endif /* HAVE_BLACKHOLE_DB */
