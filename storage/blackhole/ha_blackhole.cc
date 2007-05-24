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
#include "ha_blackhole.h"

/* Static declarations for handlerton */

static handler *blackhole_create_handler(handlerton *hton,
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_blackhole(hton, table);
}


/* Static declarations for shared structures */

static pthread_mutex_t blackhole_mutex;
static HASH blackhole_open_tables;

static st_blackhole_share *get_share(const char *table_name);
static void free_share(st_blackhole_share *share);

/*****************************************************************************
** BLACKHOLE tables
*****************************************************************************/

ha_blackhole::ha_blackhole(handlerton *hton,
                           TABLE_SHARE *table_arg)
  :handler(hton, table_arg)
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
  DBUG_RETURN((table_share->key_info[key_number].flags & HA_FULLTEXT) ? 
              "FULLTEXT" :
              (table_share->key_info[key_number].flags & HA_SPATIAL) ?
              "SPATIAL" :
              (table_share->key_info[key_number].algorithm ==
               HA_KEY_ALG_RTREE) ? "RTREE" : "BTREE");
}

int ha_blackhole::write_row(uchar * buf)
{
  DBUG_ENTER("ha_blackhole::write_row");
  DBUG_RETURN(0);
}

int ha_blackhole::rnd_init(bool scan)
{
  DBUG_ENTER("ha_blackhole::rnd_init");
  DBUG_RETURN(0);
}


int ha_blackhole::rnd_next(uchar *buf)
{
  DBUG_ENTER("ha_blackhole::rnd_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::rnd_pos(uchar * buf, uchar *pos)
{
  DBUG_ENTER("ha_blackhole::rnd_pos");
  DBUG_ASSERT(0);
  DBUG_RETURN(0);
}


void ha_blackhole::position(const uchar *record)
{
  DBUG_ENTER("ha_blackhole::position");
  DBUG_ASSERT(0);
  DBUG_VOID_RETURN;
}


int ha_blackhole::info(uint flag)
{
  DBUG_ENTER("ha_blackhole::info");

  bzero((char*) &stats, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
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
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd_in_lock_tables(thd)
        && !thd_tablespace_op(thd))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd))
      lock_type = TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;
  DBUG_RETURN(to);
}


int ha_blackhole::index_read(uchar * buf, const uchar * key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_blackhole::index_read");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_idx(uchar * buf, uint idx, const uchar * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_blackhole::index_read_idx");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_last(uchar * buf, const uchar * key,
                                  key_part_map keypart_map)
{
  DBUG_ENTER("ha_blackhole::index_read_last");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_next(uchar * buf)
{
  DBUG_ENTER("ha_blackhole::index_next");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_prev(uchar * buf)
{
  DBUG_ENTER("ha_blackhole::index_prev");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_first(uchar * buf)
{
  DBUG_ENTER("ha_blackhole::index_first");
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_last(uchar * buf)
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

static void blackhole_free_key(st_blackhole_share *share)
{
  thr_lock_delete(&share->lock);
  my_free((gptr) share, MYF(0));
}

static byte* blackhole_get_key(st_blackhole_share *share, uint *length,
                               my_bool not_used __attribute__((unused)))
{
  *length= share->table_name_length;
  return (byte*) share->table_name;
}

static int blackhole_init(void *p)
{
  handlerton *blackhole_hton;
  blackhole_hton= (handlerton *)p;
  blackhole_hton->state= SHOW_OPTION_YES;
  blackhole_hton->db_type= DB_TYPE_BLACKHOLE_DB;
  blackhole_hton->create= blackhole_create_handler;
  blackhole_hton->flags= HTON_CAN_RECREATE;
  
  VOID(pthread_mutex_init(&blackhole_mutex, MY_MUTEX_INIT_FAST));
  (void) hash_init(&blackhole_open_tables, system_charset_info,32,0,0,
                   (hash_get_key) blackhole_get_key,
                   (hash_free_key) blackhole_free_key, 0);

  return 0;
}

static int blackhole_fini(void *p)
{
  hash_free(&blackhole_open_tables);
  pthread_mutex_destroy(&blackhole_mutex);

  return 0;
}

struct st_mysql_storage_engine blackhole_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(blackhole)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &blackhole_storage_engine,
  "BLACKHOLE",
  "MySQL AB",
  "/dev/null storage engine (anything you write to it disappears)",
  PLUGIN_LICENSE_GPL,
  blackhole_init, /* Plugin Init */
  blackhole_fini, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
