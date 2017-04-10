/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "ha_ndbcluster_connection.h"

#include <mysql/psi/mysql_thread.h>

#include "sql_class.h"
#include "kernel/ndb_limits.h"
#include "my_dbug.h"
#include "ndbapi/NdbApi.hpp"
#include "portlib/NdbTick.h"
#include "util/BaseString.hpp"
#include "util/Vector.hpp"
#include "mysqld.h"         // server_id, connection_events_loop_aborted 

#include "ndb_sleep.h"

#include "log.h"            // sql_print_*

Ndb* g_ndb= NULL;
Ndb_cluster_connection* g_ndb_cluster_connection= NULL;
static Ndb_cluster_connection **g_pool= NULL;
static uint g_pool_alloc= 0;
static uint g_pool_pos= 0;
static mysql_mutex_t g_pool_mutex;


/**
   @brief Parse the --ndb-cluster-connection-pool-nodeids=nodeid[,nodeidN]
          comma separated list of nodeids to use for the pool

   @param opt_str      string containing list of nodeids to parse.
   @param pool_size    size used for the connection pool
   @param force_nodeid nodeid requested with --ndb-nodeid
   @param nodeids      the parsed list of nodeids
   @return             true or false when option parsing failed. Error message
                       describing the problem has been printed to error log.
 */
static
bool parse_pool_nodeids(const char* opt_str,
                        uint pool_size,
                        uint force_nodeid,
                        Vector<uint>& nodeids)
{
  if (!opt_str)
  {
    // The option was not specified.
    return true;
  }

  BaseString tmp(opt_str);
  Vector<BaseString> list(pool_size);
  tmp.split(list, ",");

  for (unsigned i = 0; i<list.size(); i++)
  {
    list[i].trim();

    // Don't allow empty string
    if (list[i].empty())
    {
      sql_print_error("NDB: Found empty nodeid specified in "
                      "--ndb-cluster-connection-pool-nodeids='%s'.",
                      opt_str);
      return false;
    }

    // Convert string to number
    uint nodeid = 0;
    if (sscanf(list[i].c_str(), "%u", &nodeid) != 1)
    {
      sql_print_error("NDB: Could not parse '%s' in "
                      "--ndb-cluster-connection-pool-nodeids='%s'.",
                      list[i].c_str(),
                      opt_str);
      return false;
    }

    // Check that number is a valid nodeid
    if (nodeid <= 0 || nodeid > MAX_NODES_ID)
    {
      sql_print_error("NDB: Invalid nodeid %d in "
                      "--ndb-cluster-connection-pool-nodeids='%s'.",
                      nodeid, opt_str);
      return false;
    }

    // Check that nodeid is unique(not already in the list)
    for(unsigned j = 0; j<nodeids.size(); j++)
    {
      if (nodeid == nodeids[j])
      {
        sql_print_error("NDB: Found duplicate nodeid %d in "
                        "--ndb-cluster-connection-pool-nodeids='%s'.",
                        nodeid, opt_str);
        return false;
      }
    }

    nodeids.push_back(nodeid);
  }

  // Check that size of nodeids match the pool size
  if (nodeids.size() != pool_size)
  {
    sql_print_error("NDB: The size of the cluster connection pool must be "
                    "equal to the number of nodeids in "
                    "--ndb-cluster-connection-pool-nodeids='%s'.",
                    opt_str);
    return false;
  }

  // Check that --ndb-nodeid(if given) is first in the list
  if (force_nodeid != 0 &&
      force_nodeid != nodeids[0])
  {
    sql_print_error("NDB: The nodeid specified by --ndb-nodeid must be equal "
                    "to the first nodeid in "
                    "--ndb-cluster-connection-pool-nodeids='%s'.",
                    opt_str);
    return false;
  }

  return true;
}


/*
  Global flag in ndbapi to specify if api should wait to connect
  until dict cache is clean.

  Set to 1 below to not wait, as ndb handler makes sure that no
  old ndb objects are used.
*/
extern int global_flag_skip_waiting_for_clean_cache;

int
ndbcluster_connect(int (*connect_callback)(void),
                   ulong wait_connected, // Timeout in seconds
                   uint connection_pool_size,
                   const char* connection_pool_nodeids_str,
                   bool optimized_node_select,
                   const char* connect_string,
                   uint force_nodeid,
                   uint recv_thread_activation_threshold,
                   uint data_node_neighbour)
{
  const char mysqld_name[]= "mysqld";
  int res;
  DBUG_ENTER("ndbcluster_connect");
  DBUG_PRINT("enter", ("connect_string: %s, force_nodeid: %d",
                       connect_string, force_nodeid));

  // Parse the --ndb-cluster-connection-pool-nodeids=nodeid[,nodeidN]
  // comma separated list of nodeids to use for the pool
  Vector<uint> nodeids;
  if (!parse_pool_nodeids(connection_pool_nodeids_str, connection_pool_size,
                          force_nodeid, nodeids))
  {
    // Error message already printed
    DBUG_RETURN(-1);
  }

  // Find specified nodeid for first connection and let it override
  // force_nodeid(if both has been specified they are equal).
  if (nodeids.size())
  {
    assert(force_nodeid == 0 || force_nodeid == nodeids[0]);
    force_nodeid = nodeids[0];
    sql_print_information("NDB: using nodeid %u", force_nodeid);
  }

  global_flag_skip_waiting_for_clean_cache= 1;

  g_ndb_cluster_connection=
    new Ndb_cluster_connection(connect_string, force_nodeid);
  if (!g_ndb_cluster_connection)
  {
    sql_print_error("NDB: failed to allocate global ndb cluster connection");
    DBUG_PRINT("error", ("Ndb_cluster_connection(%s)", connect_string));
    set_my_errno(HA_ERR_OUT_OF_MEM);
    DBUG_RETURN(-1);
  }
  {
    char buf[128];
    my_snprintf(buf, sizeof(buf), "%s --server-id=%lu",
                mysqld_name, server_id);
    g_ndb_cluster_connection->set_name(buf);
  }
  g_ndb_cluster_connection->set_optimized_node_selection(optimized_node_select);
  g_ndb_cluster_connection->set_recv_thread_activation_threshold(
                                      recv_thread_activation_threshold);
  g_ndb_cluster_connection->set_data_node_neighbour(data_node_neighbour);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    sql_print_error("NDB: failed to allocate global ndb object");
    DBUG_PRINT("error", ("failed to create global ndb object"));
    set_my_errno(HA_ERR_OUT_OF_MEM);
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

  const NDB_TICKS start= NdbTick_getCurrentTicks();

  while ((res= g_ndb_cluster_connection->connect(0,0,0)) == 1)
  {
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    if (NdbTick_Elapsed(start,now).seconds() > wait_connected)
      break;
    ndb_retry_sleep(100);
    if (connection_events_loop_aborted())
      DBUG_RETURN(-1);
  }

  {
    g_pool_alloc= connection_pool_size;
    g_pool= (Ndb_cluster_connection**)
      my_malloc(PSI_INSTRUMENT_ME,
                g_pool_alloc * sizeof(Ndb_cluster_connection*),
                MYF(MY_WME | MY_ZEROFILL));
    mysql_mutex_init(PSI_INSTRUMENT_ME,
                     &g_pool_mutex,
                     MY_MUTEX_INIT_FAST);
    g_pool[0]= g_ndb_cluster_connection;
    for (uint i= 1; i < g_pool_alloc; i++)
    {
      // Find specified nodeid for this connection or use default zero
      uint nodeid = 0;
      if (i < nodeids.size())
      {
        nodeid = nodeids[i];
        sql_print_information("NDB[%u]: using nodeid %u", i, nodeid);
      }

      if ((g_pool[i]=
           new Ndb_cluster_connection(connect_string,
                                      g_ndb_cluster_connection,
                                      nodeid)) == 0)
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
      g_pool[i]->set_recv_thread_activation_threshold(recv_thread_activation_threshold);
      g_pool[i]->set_data_node_neighbour(data_node_neighbour);
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

      Uint64 waited;
      do
      {
        res= g_pool[i]->wait_until_ready(1, 1);
        const NDB_TICKS now = NdbTick_getCurrentTicks();
        waited = NdbTick_Elapsed(start,now).seconds();
      } while (res != 0 && waited < wait_connected);

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
      my_free(g_pool);
      mysql_mutex_destroy(&g_pool_mutex);
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
  mysql_mutex_lock(&g_pool_mutex);
  Ndb_cluster_connection *connection= g_pool[g_pool_pos];
  g_pool_pos++;
  if (g_pool_pos == g_pool_alloc)
    g_pool_pos= 0;
  mysql_mutex_unlock(&g_pool_mutex);
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

int ndb_set_recv_thread_activation_threshold(Uint32 threshold)
{
  for (uint i= 0; i < g_pool_alloc; i++)
  {
    g_pool[i]->set_recv_thread_activation_threshold(threshold);
  }
  return 0;
}

int
ndb_set_recv_thread_cpu(Uint16 *cpuid_array,
                        Uint32 cpuid_array_size)
{
  int ret_code = 0;
  Uint32 num_cpu_needed = g_pool_alloc;

  if (cpuid_array_size == 0)
  {
    for (Uint32 i = 0; i < g_pool_alloc; i++)
    {
      ret_code = g_pool[i]->unset_recv_thread_cpu(0);
    }
    return ret_code;
  }

  if (cpuid_array_size < num_cpu_needed)
  {
    /* Ignore cpu masks that is too short */
    sql_print_information(
      "Ignored receive thread CPU mask, mask too short,"
      " %u CPUs needed in mask, only %u CPUs provided",
      num_cpu_needed, cpuid_array_size);
    return 1;
  }
  for (Uint32 i = 0; i < g_pool_alloc; i++)
  {
    ret_code = g_pool[i]->set_recv_thread_cpu(&cpuid_array[i],
                                   (Uint32)1,
                                   0);
  }
  return ret_code;
}

void
ndb_set_data_node_neighbour(ulong data_node_neighbour)
{
  for (uint i= 0; i < g_pool_alloc; i++)
    g_pool[i]->set_data_node_neighbour(data_node_neighbour);
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

static ST_FIELD_INFO ndb_transid_mysql_connection_map_fields_info[] =
{
  {
    "mysql_connection_id",
    MY_INT64_NUM_DECIMAL_DIGITS,
    MYSQL_TYPE_LONGLONG,
    0,
    MY_I_S_UNSIGNED,
    "",
    SKIP_OPEN_TABLE
  },

  {
    "node_id",
    MY_INT64_NUM_DECIMAL_DIGITS,
    MYSQL_TYPE_LONG,
    0,
    MY_I_S_UNSIGNED,
    "",
    SKIP_OPEN_TABLE
  },
  {
    "ndb_transid",
    MY_INT64_NUM_DECIMAL_DIGITS,
    MYSQL_TYPE_LONGLONG,
    0,
    MY_I_S_UNSIGNED,
    "",
    SKIP_OPEN_TABLE
  },

  { 0, 0, MYSQL_TYPE_NULL, 0, 0, "", SKIP_OPEN_TABLE }
};

#include <mysql/innodb_priv.h>

static
int
ndb_transid_mysql_connection_map_fill_table(THD* thd, TABLE_LIST* tables,
                                            Item*)
{
  DBUG_ENTER("ndb_transid_mysql_connection_map_fill_table");

  const bool all = (check_global_access(thd, PROCESS_ACL) == 0);
  const ulonglong self = thd_get_thread_id(thd);

  TABLE* table= tables->table;
  for (uint i = 0; i<g_pool_alloc; i++)
  {
    if (g_pool[i])
    {
      g_pool[i]->lock_ndb_objects();
      const Ndb * p = g_pool[i]->get_next_ndb_object(0);
      while (p)
      {
        Uint64 connection_id = p->getCustomData64();
        if ((connection_id == self) || all)
        {
          table->field[0]->set_notnull();
          table->field[0]->store(p->getCustomData64(), true);
          table->field[1]->set_notnull();
          table->field[1]->store(g_pool[i]->node_id());
          table->field[2]->set_notnull();
          table->field[2]->store(p->getNextTransactionId(), true);
          schema_table_store_record(thd, table);
        }
        p = g_pool[i]->get_next_ndb_object(p);
      }
      g_pool[i]->unlock_ndb_objects();
    }
  }

  DBUG_RETURN(0);
}

static
int
ndb_transid_mysql_connection_map_init(void *p)
{
  DBUG_ENTER("ndb_transid_mysql_connection_map_init");
  ST_SCHEMA_TABLE* schema = reinterpret_cast<ST_SCHEMA_TABLE*>(p);
  schema->fields_info = ndb_transid_mysql_connection_map_fields_info;
  schema->fill_table = ndb_transid_mysql_connection_map_fill_table;
  DBUG_RETURN(0);
}

static
int
ndb_transid_mysql_connection_map_deinit(void *p)
{
  DBUG_ENTER("ndb_transid_mysql_connection_map_deinit");
  DBUG_RETURN(0);
}

#include <mysql/plugin.h>

static struct st_mysql_information_schema i_s_info =
{
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

struct st_mysql_plugin i_s_ndb_transid_mysql_connection_map_plugin =
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &i_s_info,
  "ndb_transid_mysql_connection_map",
  "Oracle Corporation",
  "Map between mysql connection id and ndb transaction id",
  PLUGIN_LICENSE_GPL,
  ndb_transid_mysql_connection_map_init,
  ndb_transid_mysql_connection_map_deinit,
  0x0001,
  NULL,
  NULL,
  NULL,
  0
};
