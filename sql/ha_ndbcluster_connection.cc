/*
   Copyright (C) 2000-2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ha_ndbcluster_glue.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include <ndbapi/NdbApi.hpp>
#include <portlib/NdbTick.h>
#include "ha_ndbcluster_connection.h"

Ndb* g_ndb= NULL;
Ndb_cluster_connection* g_ndb_cluster_connection= NULL;
static Ndb_cluster_connection **g_pool= NULL;
static uint g_pool_alloc= 0;
static uint g_pool_pos= 0;
static pthread_mutex_t g_pool_mutex;

/*
  Global flag in ndbapi to specify if api should wait to connect
  until dict cache is clean.

  Set to 1 below to not wait, as ndb handler makes sure that no
  old ndb objects are used.
*/
extern int global_flag_skip_waiting_for_clean_cache;

int
ndbcluster_connect(int (*connect_callback)(void),
                   ulong wait_connected,
                   uint connection_pool_size,
                   bool optimized_node_select,
                   const char* connect_string,
                   uint force_nodeid)
{
  NDB_TICKS end_time;

#ifndef EMBEDDED_LIBRARY
  const char mysqld_name[]= "mysqld";
#else
  const char mysqld_name[]= "libmysqld";
#endif
  int res;
  DBUG_ENTER("ndbcluster_connect");
  DBUG_PRINT("enter", ("connect_string: %s, force_nodeid: %d",
                       connect_string, force_nodeid));

  global_flag_skip_waiting_for_clean_cache= 1;

  g_ndb_cluster_connection=
    new Ndb_cluster_connection(connect_string, force_nodeid);
  if (!g_ndb_cluster_connection)
  {
    sql_print_error("NDB: failed to allocate global ndb cluster connection");
    DBUG_PRINT("error", ("Ndb_cluster_connection(%s)", connect_string));
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1);
  }
  {
    char buf[128];
    my_snprintf(buf, sizeof(buf), "%s --server-id=%lu",
                mysqld_name, server_id);
    g_ndb_cluster_connection->set_name(buf);
  }
  g_ndb_cluster_connection->set_optimized_node_selection(optimized_node_select);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    sql_print_error("NDB: failed to allocate global ndb object");
    DBUG_PRINT("error", ("failed to create global ndb object"));
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(-1);
  }
  if (g_ndb->init() != 0)
  {
    DBUG_PRINT("error", ("%d  message: %s",
                         g_ndb->getNdbError().code,
                         g_ndb->getNdbError().message));
    DBUG_RETURN(-1);
  }

  /* Connect to management server */

  end_time= NdbTick_CurrentMillisecond();
  end_time+= 1000 * wait_connected;

  while ((res= g_ndb_cluster_connection->connect(0,0,0)) == 1)
  {
    if (NdbTick_CurrentMillisecond() > end_time)
      break;
    do_retry_sleep(100);
    if (abort_loop)
      DBUG_RETURN(-1);
  }

  {
    g_pool_alloc= connection_pool_size;
    g_pool= (Ndb_cluster_connection**)
      my_malloc(g_pool_alloc * sizeof(Ndb_cluster_connection*),
                MYF(MY_WME | MY_ZEROFILL));
    pthread_mutex_init(&g_pool_mutex,
                       MY_MUTEX_INIT_FAST);
    g_pool[0]= g_ndb_cluster_connection;
    for (uint i= 1; i < g_pool_alloc; i++)
    {
      if ((g_pool[i]=
           new Ndb_cluster_connection(connect_string,
                                      g_ndb_cluster_connection)) == 0)
      {
        sql_print_error("NDB[%u]: failed to allocate cluster connect object",
                        i);
        DBUG_PRINT("error",("Ndb_cluster_connection[%u](%s)",
                            i, connect_string));
        DBUG_RETURN(-1);
      }
      {
        char buf[128];
        my_snprintf(buf, sizeof(buf), "%s --server-id=%lu (connection %u)",
                    mysqld_name, server_id, i+1);
        g_pool[i]->set_name(buf);
      }
      g_pool[i]->set_optimized_node_selection(optimized_node_select);
    }
  }

  if (res == 0)
  {
    connect_callback();
    for (uint i= 0; i < g_pool_alloc; i++)
    {
      int node_id= g_pool[i]->node_id();
      if (node_id == 0)
      {
        // not connected to mgmd yet, try again
        g_pool[i]->connect(0,0,0);
        if (g_pool[i]->node_id() == 0)
        {
          sql_print_warning("NDB[%u]: starting connect thread", i);
          g_pool[i]->start_connect_thread();
          continue;
        }
        node_id= g_pool[i]->node_id();
      }
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine (%u) at %s on port %d", i,
                  g_pool[i]->get_connected_host(),
                  g_pool[i]->get_connected_port()));

      NDB_TICKS now_time;
      do
      {
        res= g_pool[i]->wait_until_ready(1, 1);
        now_time= NdbTick_CurrentMillisecond();
      } while (res != 0 && now_time < end_time);

      const char *msg= 0;
      if (res == 0)
      {
        msg= "all storage nodes connected";
      }
      else if (res > 0)
      {
        msg= "some storage nodes connected";
      }
      else if (res < 0)
      {
        msg= "no storage nodes connected (timed out)";
      }
      sql_print_information("NDB[%u]: NodeID: %d, %s",
                            i, node_id, msg);
    }
  }
  else if (res == 1)
  {
    for (uint i= 0; i < g_pool_alloc; i++)
    {
      if (g_pool[i]->
          start_connect_thread(i == 0 ? connect_callback :  NULL))
      {
        sql_print_error("NDB[%u]: failed to start connect thread", i);
        DBUG_PRINT("error", ("g_ndb_cluster_connection->start_connect_thread()"));
        DBUG_RETURN(-1);
      }
    }
#ifndef DBUG_OFF
    {
      char buf[1024];
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine not started, "
                  "will connect using %s",
                  g_ndb_cluster_connection->
                  get_connectstring(buf,sizeof(buf))));
    }
#endif
  }
  else
  {
    DBUG_ASSERT(res == -1);
    DBUG_PRINT("error", ("permanent error"));
    sql_print_error("NDB: error (%u) %s",
                    g_ndb_cluster_connection->get_latest_error(),
                    g_ndb_cluster_connection->get_latest_error_msg());
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

void ndbcluster_disconnect(void)
{
  DBUG_ENTER("ndbcluster_disconnect");
  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  {
    if (g_pool)
    {
      /* first in pool is the main one, wait with release */
      for (uint i= 1; i < g_pool_alloc; i++)
      {
        if (g_pool[i])
          delete g_pool[i];
      }
      my_free((uchar*) g_pool, MYF(MY_ALLOW_ZERO_PTR));
      pthread_mutex_destroy(&g_pool_mutex);
      g_pool= 0;
    }
    g_pool_alloc= 0;
    g_pool_pos= 0;
  }
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;
  DBUG_VOID_RETURN;
}

Ndb_cluster_connection *ndb_get_cluster_connection()
{
  pthread_mutex_lock(&g_pool_mutex);
  Ndb_cluster_connection *connection= g_pool[g_pool_pos];
  g_pool_pos++;
  if (g_pool_pos == g_pool_alloc)
    g_pool_pos= 0;
  pthread_mutex_unlock(&g_pool_mutex);
  return connection;
}

ulonglong ndb_get_latest_trans_gci()
{
  ulonglong val= *g_ndb_cluster_connection->get_latest_trans_gci();
  for (uint i= 1; i < g_pool_alloc; i++)
  {
    ulonglong tmp= *g_pool[i]->get_latest_trans_gci();
    if (tmp > val)
      val= tmp;
  }
  return val;
}

void ndb_set_latest_trans_gci(ulonglong val)
{
  for (uint i= 0; i < g_pool_alloc; i++)
  {
    *g_pool[i]->get_latest_trans_gci()= val;
  }
}

int ndb_has_node_id(uint id)
{
  for (uint i= 0; i < g_pool_alloc; i++)
  {
    if (id == g_pool[i]->node_id())
      return 1;
  }
  return 0;
}

void ndb_get_connection_stats(Uint64* statsArr)
{
  Uint64 connectionStats[ Ndb::NumClientStatistics ];
  memset(statsArr, 0, sizeof(connectionStats));
  
  for (uint i=0; i < g_pool_alloc; i++)
  {
    g_pool[i]->collect_client_stats(connectionStats, Ndb::NumClientStatistics);
    
    for (Uint32 s=0; s < Ndb::NumClientStatistics; s++)
      statsArr[s]+= connectionStats[s];
  }
}

#endif /* WITH_NDBCLUSTER_STORAGE_ENGINE */
