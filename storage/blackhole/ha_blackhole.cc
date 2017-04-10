/* Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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


#define MYSQL_SERVER 1
#include "ha_blackhole.h"

#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_memory.h"
#include "sql_class.h"                          // THD, SYSTEM_THREAD_SLAVE_*
#include "template_utils.h"

static PSI_memory_key bh_key_memory_blackhole_share;

static bool is_slave_applier(THD *thd)
{
  return thd->system_thread == SYSTEM_THREAD_SLAVE_SQL ||
    thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER;
}

/* Static declarations for handlerton */

static handler *blackhole_create_handler(handlerton *hton,
                                         TABLE_SHARE *table,
                                         bool,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_blackhole(hton, table);
}


/* Static declarations for shared structures */

static mysql_mutex_t blackhole_mutex;
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


int ha_blackhole::open(const char *name, int, uint, const dd::Table*)
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

int ha_blackhole::create(const char*, TABLE*, HA_CREATE_INFO*, dd::Table*)
{
  DBUG_ENTER("ha_blackhole::create");
  DBUG_RETURN(0);
}

int ha_blackhole::write_row(uchar*)
{
  DBUG_ENTER("ha_blackhole::write_row");
  DBUG_RETURN(table->next_number_field ? update_auto_increment() : 0);
}

int ha_blackhole::update_row(const uchar*, uchar*)
{
  DBUG_ENTER("ha_blackhole::update_row");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    DBUG_RETURN(0);
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_blackhole::delete_row(const uchar*)
{
  DBUG_ENTER("ha_blackhole::delete_row");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    DBUG_RETURN(0);
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_blackhole::rnd_init(bool)
{
  DBUG_ENTER("ha_blackhole::rnd_init");
  DBUG_RETURN(0);
}


int ha_blackhole::rnd_next(uchar*)
{
  int rc;
  DBUG_ENTER("ha_blackhole::rnd_next");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::rnd_pos(uchar*, uchar*)
{
  DBUG_ENTER("ha_blackhole::rnd_pos");
  DBUG_ASSERT(0);
  DBUG_RETURN(0);
}


void ha_blackhole::position(const uchar*)
{
  DBUG_ENTER("ha_blackhole::position");
  DBUG_ASSERT(0);
  DBUG_VOID_RETURN;
}


int ha_blackhole::info(uint flag)
{
  DBUG_ENTER("ha_blackhole::info");

  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  DBUG_RETURN(0);
}

int ha_blackhole::external_lock(THD*, int)
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
         lock_type <= TL_WRITE) && !thd_in_lock_tables(thd))
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


int ha_blackhole::index_read_map(uchar*, const uchar*, key_part_map,
                                 enum ha_rkey_function)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_read");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_read_idx_map(uchar*, uint, const uchar*, key_part_map,
                                     enum ha_rkey_function)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_read_idx");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_read_last_map(uchar*, const uchar*, key_part_map)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_read_last");
  THD *thd= ha_thd();
  if (is_slave_applier(thd) && thd->query().str == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_next(uchar*)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_next");
  rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_prev(uchar*)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_prev");
  rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_first(uchar*)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_first");
  rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


int ha_blackhole::index_last(uchar*)
{
  int rc;
  DBUG_ENTER("ha_blackhole::index_last");
  rc= HA_ERR_END_OF_FILE;
  DBUG_RETURN(rc);
}


static st_blackhole_share *get_share(const char *table_name)
{
  st_blackhole_share *share;
  uint length;

  length= (uint) strlen(table_name);
  mysql_mutex_lock(&blackhole_mutex);
    
  if (!(share= (st_blackhole_share*)
        my_hash_search(&blackhole_open_tables,
                       (uchar*) table_name, length)))
  {
    if (!(share= (st_blackhole_share*) my_malloc(bh_key_memory_blackhole_share,
                                                 sizeof(st_blackhole_share) +
                                                 length,
                                                 MYF(MY_WME | MY_ZEROFILL))))
      goto error;

    share->table_name_length= length;
    my_stpcpy(share->table_name, table_name);
    
    if (my_hash_insert(&blackhole_open_tables, (uchar*) share))
    {
      my_free(share);
      share= NULL;
      goto error;
    }
    
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  
error:
  mysql_mutex_unlock(&blackhole_mutex);
  return share;
}

static void free_share(st_blackhole_share *share)
{
  mysql_mutex_lock(&blackhole_mutex);
  if (!--share->use_count)
    my_hash_delete(&blackhole_open_tables, (uchar*) share);
  mysql_mutex_unlock(&blackhole_mutex);
}

static void blackhole_free_key(void *arg)
{
  st_blackhole_share *share= pointer_cast<st_blackhole_share*>(arg);
  thr_lock_delete(&share->lock);
  my_free(share);
}

static const uchar* blackhole_get_key(const uchar *arg, size_t *length)
{
  const st_blackhole_share *share= pointer_cast<const st_blackhole_share*>(arg);
  *length= share->table_name_length;
  return (uchar*) share->table_name;
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key bh_key_mutex_blackhole;

static PSI_mutex_info all_blackhole_mutexes[]=
{
  { &bh_key_mutex_blackhole, "blackhole", PSI_FLAG_GLOBAL, 0}
};

static PSI_memory_info all_blackhole_memory[]=
{
  { &bh_key_memory_blackhole_share, "blackhole_share", 0}
};

static void init_blackhole_psi_keys()
{
  const char* category= "blackhole";
  int count;

  count= static_cast<int>(array_elements(all_blackhole_mutexes));
  mysql_mutex_register(category, all_blackhole_mutexes, count);

  count= static_cast<int>(array_elements(all_blackhole_memory));
  mysql_memory_register(category, all_blackhole_memory, count);
}
#endif

static int blackhole_init(void *p)
{
  handlerton *blackhole_hton;

#ifdef HAVE_PSI_INTERFACE
  init_blackhole_psi_keys();
#endif

  blackhole_hton= (handlerton *)p;
  blackhole_hton->state= SHOW_OPTION_YES;
  blackhole_hton->db_type= DB_TYPE_BLACKHOLE_DB;
  blackhole_hton->create= blackhole_create_handler;
  blackhole_hton->flags= HTON_CAN_RECREATE;

  mysql_mutex_init(bh_key_mutex_blackhole,
                   &blackhole_mutex, MY_MUTEX_INIT_FAST);
  (void) my_hash_init(&blackhole_open_tables, system_charset_info,32,0,
                      blackhole_get_key,
                      blackhole_free_key, 0,
                      bh_key_memory_blackhole_share);

  return 0;
}

static int blackhole_fini(void*)
{
  my_hash_free(&blackhole_open_tables);
  mysql_mutex_destroy(&blackhole_mutex);

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
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
