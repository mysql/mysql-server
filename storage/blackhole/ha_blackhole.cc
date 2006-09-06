/* Copyright (C) 2005 MySQL AB

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


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "ha_blackhole.h"

/* Static declarations for handlerton */

handlerton blackhole_hton;
static handler *blackhole_create_handler(TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_blackhole(table);
}


/*****************************************************************************
** BLACKHOLE tables
*****************************************************************************/

ha_blackhole::ha_blackhole(TABLE_SHARE *table_arg)
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
  thr_lock_init(&thr_lock);
  thr_lock_data_init(&thr_lock,&lock,NULL);
  DBUG_RETURN(0);
}

int ha_blackhole::close(void)
{
  DBUG_ENTER("ha_blackhole::close");
  thr_lock_delete(&thr_lock);
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


void ha_blackhole::info(uint flag)
{
  DBUG_ENTER("ha_blackhole::info");

  bzero((char*) &stats, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  DBUG_VOID_RETURN;
}

int ha_blackhole::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_blackhole::external_lock");
  DBUG_RETURN(0);
}


uint ha_blackhole::lock_count(void) const
{
  DBUG_ENTER("ha_blackhole::lock_count");
  DBUG_RETURN(0);
}

THR_LOCK_DATA **ha_blackhole::store_lock(THD *thd,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  DBUG_ENTER("ha_blackhole::store_lock");
  DBUG_RETURN(to);
}


int ha_blackhole::index_read(byte * buf, const byte * key,
                             uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_blackhole::index_read");
  DBUG_RETURN(0);
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

static int blackhole_init()
{
  blackhole_hton.state= SHOW_OPTION_YES;
  blackhole_hton.db_type= DB_TYPE_BLACKHOLE_DB;
  blackhole_hton.create= blackhole_create_handler;
  blackhole_hton.flags= HTON_CAN_RECREATE;
  return 0;
}

struct st_mysql_storage_engine blackhole_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION, &blackhole_hton };

mysql_declare_plugin(blackhole)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &blackhole_storage_engine,
  "BLACKHOLE",
  "MySQL AB",
  "/dev/null storage engine (anything you write to it disappears)",
  blackhole_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
