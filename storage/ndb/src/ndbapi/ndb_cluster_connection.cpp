/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "ndb_cluster_connection_impl.hpp"
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>
#include "TransporterFacade.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <ndb_limits.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>
#include <mgmapi_debug.h>
#include <mgmapi_internal.h>
#include "NdbImpl.hpp"
#include "NdbDictionaryImpl.hpp"
#include "ProcessInfo.hpp"

#include <NdbMutex.h>
#ifdef VM_TRACE
NdbMutex *ndb_print_state_mutex= NULL;
#endif

#include <EventLogger.hpp>
extern EventLogger *g_eventLogger;

static int g_ndb_connection_count = 0;

/*
 * Ndb_cluster_connection
 */
Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string)
  : m_impl(* new Ndb_cluster_connection_impl(connect_string, 0, 0))
{
}

Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string,
                                               int force_api_nodeid)
  : m_impl(* new Ndb_cluster_connection_impl(connect_string, 0,
                                             force_api_nodeid))
{
}

Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string,
                                               Ndb_cluster_connection *
                                               main_connection)
  : m_impl(* new Ndb_cluster_connection_impl(connect_string,
                                             main_connection, 0))
{
}


Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string,
                                               Ndb_cluster_connection *
                                               main_connection,
                                               int force_api_nodeid)
  : m_impl(* new Ndb_cluster_connection_impl(connect_string,
                                             main_connection, force_api_nodeid))
{
}

Ndb_cluster_connection::Ndb_cluster_connection
(Ndb_cluster_connection_impl& impl) : m_impl(impl)
{
}

Ndb_cluster_connection::~Ndb_cluster_connection()
{
  Ndb_cluster_connection_impl *tmp = &m_impl;
  if (this != tmp)
    delete tmp;
}

int Ndb_cluster_connection::get_connected_port() const
{
  if (m_impl.m_config_retriever)
    return m_impl.m_config_retriever->get_mgmd_port();
  return -1;
}

const char *Ndb_cluster_connection::get_connected_host() const
{
  if (m_impl.m_config_retriever)
    return m_impl.m_config_retriever->get_mgmd_host();
  return 0;
}

int
Ndb_cluster_connection::unset_recv_thread_cpu(Uint32 recv_thread_id)
{
  if (m_impl.m_transporter_facade)
  {
    return m_impl.m_transporter_facade->unset_recv_thread_cpu(recv_thread_id);
  }
  return -1;
}

int
Ndb_cluster_connection::set_recv_thread_cpu(Uint16 *cpuid_array,
                                            Uint32 array_len,
                                            Uint32 recv_thread_id)
{
  if (m_impl.m_transporter_facade)
  {
    return m_impl.m_transporter_facade->set_recv_thread_cpu(cpuid_array,
                                                            array_len,
                                                            recv_thread_id);
  }
  return -1;
}

int
Ndb_cluster_connection::set_recv_thread_activation_threshold(Uint32 threshold)
{
  TransporterFacade *fac = m_impl.m_transporter_facade;
  if (fac)
  {
    return fac->set_recv_thread_activation_threshold(threshold);
  }
  return -1;
}

int
Ndb_cluster_connection::get_recv_thread_activation_threshold() const
{
  if (m_impl.m_transporter_facade)
  {
    return m_impl.m_transporter_facade->get_recv_thread_activation_threshold();
  }
  return -1;
}

const char *Ndb_cluster_connection::get_connectstring(char *buf,
						      int buf_sz) const
{
  if (m_impl.m_config_retriever)
    return m_impl.m_config_retriever->get_connectstring(buf,buf_sz);
  return 0;
}

extern "C"
void *
run_ndb_cluster_connection_connect_thread(void *me)
{
  Ndb_cluster_connection_impl* connection= (Ndb_cluster_connection_impl*) me;
  connection->m_run_connect_thread= 1;
  connection->connect_thread();
  return me;
}

int Ndb_cluster_connection::start_connect_thread(int (*connect_callback)(void))
{
  int r;
  DBUG_ENTER("Ndb_cluster_connection::start_connect_thread");
  m_impl.m_connect_callback= connect_callback;
  if ((r = connect(0,0,0)) == 1)
  {
    DBUG_PRINT("info",("starting thread"));
    m_impl.m_connect_thread= 
      NdbThread_Create(run_ndb_cluster_connection_connect_thread,
		       (void**)&m_impl,
                       0, // default stack size
                       "ndb_cluster_connection",
		       NDB_THREAD_PRIO_LOW);
  }
  else if (r < 0)
  {
    DBUG_RETURN(-1);
  }
  else if (m_impl.m_connect_callback)
  { 
    (*m_impl.m_connect_callback)();
  }
  DBUG_RETURN(0);
}

void Ndb_cluster_connection::set_optimized_node_selection(int val)
{
  m_impl.m_optimized_node_selection= val;
}

void
Ndb_cluster_connection_impl::init_get_next_node
(Ndb_cluster_connection_node_iter &iter)
{
  if (iter.scan_state != (Uint8)~0)
    iter.cur_pos= iter.scan_state;
  if (iter.cur_pos >= no_db_nodes())
    iter.cur_pos= 0;
  iter.init_pos= iter.cur_pos;
  iter.scan_state= 0;
  //  fprintf(stderr,"[init %d]",iter.init_pos);
  return;
}

Uint32
Ndb_cluster_connection_impl::get_next_node(Ndb_cluster_connection_node_iter &iter)
{
  /**
   * Note that iter may be out dated by changes to m_nodes_proximity.
   * m_nodes_proximity can be changed by application calling
   * set_data_node_neighbour() which in turn calls adjust_node_proximity that
   * can rearrange the nodes.  This can even happen concurrently by another
   * thread.
   *
   * It is assumed that each fields in the Node struct will at least be updated
   * atomically.  And the fact that sometimes the next node selected may be the
   * wrong one is ignored and taken as an glitch choosing a possible non
   * optimal node once after call to set_data_node_neighbour().
   */
  Uint32 cur_pos= iter.cur_pos;
  if (cur_pos >= no_db_nodes())
    return 0;

  Ndb_cluster_connection_impl::Node *nodes= m_nodes_proximity.getBase();
  Ndb_cluster_connection_impl::Node &node=  nodes[cur_pos];

  if (iter.scan_state != (Uint8)~0)
  {
    assert(iter.scan_state < no_db_nodes());
    if (nodes[iter.scan_state].adjusted_group == node.adjusted_group)
      iter.scan_state= ~0;
    else
      return nodes[iter.scan_state++].id;
  }

  //  fprintf(stderr,"[%d]",node.id);

  cur_pos++;
  Uint32 init_pos= iter.init_pos;
  if (cur_pos == node.next_group_idx)
  {
    cur_pos= nodes[init_pos].this_group_idx;
  }

  //  fprintf(stderr,"[cur_pos %d]",cur_pos);
  if (cur_pos != init_pos)
    iter.cur_pos= cur_pos;
  else
  {
    iter.cur_pos= node.next_group_idx;
    iter.init_pos= node.next_group_idx;
  }
  return node.id;
}

Uint32
Ndb_cluster_connection_impl::get_next_alive_node(Ndb_cluster_connection_node_iter &iter)
{
  Uint32 id;

  TransporterFacade *tp = m_impl.m_transporter_facade;
  if (tp == 0 || tp->ownId() == 0)
    return 0;

  while ((id = get_next_node(iter)))
  {
    tp->lock_poll_mutex();
    if (tp->get_node_alive(id) != 0)
    {
      tp->unlock_poll_mutex();
      return id;
    }
    tp->unlock_poll_mutex();
  }
  return 0;
}

unsigned
Ndb_cluster_connection::no_db_nodes()
{
  assert(m_impl.m_db_nodes.count() ==
           m_impl.m_nodes_proximity.size());
  return m_impl.m_nodes_proximity.size();
}

unsigned
Ndb_cluster_connection::node_id()
{
  return m_impl.m_transporter_facade->ownId();
}

unsigned
Ndb_cluster_connection::max_nodegroup()
{
  TransporterFacade *tp = m_impl.m_transporter_facade;
  if (tp == 0 || tp->ownId() == 0)
    return 0;

  NdbNodeBitmask ng;
  tp->lock_poll_mutex();
  for (Uint32 node_id = m_impl.m_db_nodes.find_first();
       node_id != NdbNodeBitmask::NotFound;
       node_id = m_impl.m_db_nodes.find_next(node_id + 1))
  {
    //************************************************
    // If any node is answering, ndb is answering
    //************************************************
    trp_node n = tp->theClusterMgr->getNodeInfo(node_id);
    if (n.is_confirmed() && n.m_state.nodeGroup <= MAX_NDB_NODES)
      ng.set(n.m_state.nodeGroup);
  }
  tp->unlock_poll_mutex();

  if (ng.isclear())
    return 0;

  Uint32 n = ng.find_first();
  Uint32 m;
  do
  {
    m = n;
  } while ((n = ng.find(n+1)) != ng.NotFound);

  return m;
}

int Ndb_cluster_connection::get_no_ready()
{
  TransporterFacade *tp = m_impl.m_transporter_facade;
  if (tp == 0 || tp->ownId() == 0)
    return -1;

  unsigned int foundAliveNode = 0;
  tp->lock_poll_mutex();
  for (Uint32 node_id = m_impl.m_db_nodes.find_first();
       node_id != NdbNodeBitmask::NotFound;
       node_id = m_impl.m_db_nodes.find_next(node_id + 1))
  {
    //************************************************
    // If any node is answering, ndb is answering
    //************************************************
    if (tp->get_node_alive(node_id) != 0)
    {
      foundAliveNode++;
    }
  }
  tp->unlock_poll_mutex();

  return foundAliveNode;
}

int
Ndb_cluster_connection::wait_until_ready(int timeout,
					 int timeout_after_first_alive)
{
  DBUG_ENTER("Ndb_cluster_connection::wait_until_ready");
  TransporterFacade *tp = m_impl.m_transporter_facade;
  if (tp == 0)
  {
    DBUG_RETURN(-1);
  }
  if (tp->ownId() == 0)
  {
    DBUG_RETURN(-1);
  }
  int secondsCounter = 0;
  int milliCounter = 0;
  int noChecksSinceFirstAliveFound = 0;
  do {
    const Uint32 unconnected_nodes = m_impl.get_unconnected_nodes();

    if (unconnected_nodes == 0)
    {
      DBUG_RETURN(0);
    }
    else if (unconnected_nodes < no_db_nodes())
    {
      noChecksSinceFirstAliveFound++;
      // 100 ms delay -> 10*
      if (noChecksSinceFirstAliveFound > 10*timeout_after_first_alive)
	DBUG_RETURN(1);
    }
    else if (secondsCounter >= timeout)
    { // no alive nodes and timed out
      DBUG_RETURN(-1);
    }
    NdbSleep_MilliSleep(100);
    milliCounter += 100;
    if (milliCounter >= 1000) {
      secondsCounter++;
      milliCounter = 0;
    }//if
  } while (1);
}

unsigned Ndb_cluster_connection::get_connect_count() const
{
  return m_impl.get_connect_count();
}

unsigned Ndb_cluster_connection::get_min_db_version() const
{
  return m_impl.get_min_db_version();
}

int Ndb_cluster_connection::get_latest_error() const
{
  return m_impl.m_latest_error;
}

const char *Ndb_cluster_connection::get_latest_error_msg() const
{
  return m_impl.m_latest_error_msg.c_str();
}

/*
 * Ndb_cluster_connection_impl
 */

Ndb_cluster_connection_impl::
Ndb_cluster_connection_impl(const char * connect_string,
                            Ndb_cluster_connection *main_connection,
                            int force_api_nodeid)
  : Ndb_cluster_connection(*this),
    m_main_connection(main_connection),
    m_optimized_node_selection(1),
    m_run_connect_thread(0),
    m_latest_trans_gci(0),
    m_first_ndb_object(0),
    m_latest_error_msg(),
    m_latest_error(0),
    m_data_node_neighbour(0),
    m_multi_wait_group(0),
    m_uri_scheme(NULL),
    m_uri_host(NULL),
    m_uri_path(NULL),
    m_uri_port(0)
{
  DBUG_ENTER("Ndb_cluster_connection");
  DBUG_PRINT("enter",("Ndb_cluster_connection this=0x%lx", (long) this));

  NdbMutex_Lock(g_ndb_connection_mutex);
  if(g_ndb_connection_count++ == 0)
  {
    NdbColumnImpl::create_pseudo_columns();
    g_eventLogger->createConsoleHandler();
    g_eventLogger->setCategory("NdbApi");
    g_eventLogger->enable(Logger::LL_ON, Logger::LL_ERROR);
    /*
      Disable repeated message handling as it interfers
      with mysqld logging, in which case messages come out
      of order.  Same applies for regular ndbapi user.
    */
    g_eventLogger->setRepeatFrequency(0);

#ifdef VM_TRACE
    ndb_print_state_mutex= NdbMutex_Create();
#endif

  }
  NdbMutex_Unlock(g_ndb_connection_mutex);

  m_event_add_drop_mutex= NdbMutex_Create();
  m_new_delete_ndb_mutex = NdbMutex_Create();
  m_new_delete_ndb_cond = NdbCondition_Create();
  m_nodes_proximity_mutex = NdbMutex_Create();

  m_connect_thread= 0;
  m_connect_callback= 0;

  /* Clear global stats baseline */
  memset(globalApiStatsBaseline, 0, sizeof(globalApiStatsBaseline));

  m_config_retriever=
    new ConfigRetriever(connect_string, force_api_nodeid,
                        NDB_VERSION, NDB_MGM_NODE_TYPE_API);
  if (m_config_retriever->hasError())
  {
    m_latest_error= 1;
    m_latest_error_msg.assfmt
      ("Could not initialize handle to management server: %s",
       m_config_retriever->getErrorString());
    printf("%s\n", get_latest_error_msg());
  }
  if (!m_main_connection)
  {
    m_globalDictCache = new GlobalDictCache;
    m_transporter_facade= new TransporterFacade(m_globalDictCache);
  }
  else
  {
    assert(m_main_connection->m_impl.m_globalDictCache != NULL);
    m_globalDictCache = 0;
    m_transporter_facade=
      new TransporterFacade(m_main_connection->m_impl.m_globalDictCache);

    // The secondary connection can't use same nodeid, but it's ok
    // to specify one to use. Use the force_api_nodeid given(although
    // it will normally be 0 in order to allocate dynamic nodeid)
    m_config_retriever->setNodeId(force_api_nodeid);

  }

  DBUG_VOID_RETURN;
}

Ndb_cluster_connection_impl::~Ndb_cluster_connection_impl()
{
  DBUG_ENTER("~Ndb_cluster_connection");

  // Wait until all Ndb instances belonging to this Ndb_cluster_connection
  // have been released(they have references to the TransporterFacade)
  NdbMutex_Lock(m_new_delete_ndb_mutex);
  if (m_first_ndb_object)
  {
    g_eventLogger->warning("Waiting for Ndb instances belonging to "
                           "Ndb_cluster_connection %p to be deleted...",
                           this);

    while(m_first_ndb_object)
    {
      NdbCondition_WaitTimeout(m_new_delete_ndb_cond,
                               m_new_delete_ndb_mutex,
                               1000);
    }
  }
  NdbMutex_Unlock(m_new_delete_ndb_mutex);

  if (m_transporter_facade != 0)
  {
    m_transporter_facade->stop_instance();
  }
  if (m_globalDictCache)
  {
    delete m_globalDictCache;
  }
  if (m_connect_thread)
  {
    void *status;
    m_run_connect_thread= 0;
    NdbThread_WaitFor(m_connect_thread, &status);
    NdbThread_Destroy(&m_connect_thread);
    m_connect_thread= 0;
  }
  if (m_transporter_facade != 0)
  {
    delete m_transporter_facade;
    m_transporter_facade = 0;
  }
  if (m_config_retriever)
  {
    delete m_config_retriever;
    m_config_retriever= NULL;
  }

  NdbMutex_Lock(g_ndb_connection_mutex);
  if(--g_ndb_connection_count == 0)
  {
    NdbColumnImpl::destory_pseudo_columns();

#ifdef VM_TRACE
    NdbMutex_Destroy(ndb_print_state_mutex);
    ndb_print_state_mutex= NULL;
#endif

  }
  NdbMutex_Unlock(g_ndb_connection_mutex);

  if (m_nodes_proximity_mutex != NULL)
  {
    NdbMutex_Destroy(m_nodes_proximity_mutex);
    m_nodes_proximity_mutex = NULL;
  }

  if (m_event_add_drop_mutex)
    NdbMutex_Destroy(m_event_add_drop_mutex);
  m_event_add_drop_mutex = 0;

  if (m_new_delete_ndb_mutex)
    NdbMutex_Destroy(m_new_delete_ndb_mutex);
  m_new_delete_ndb_mutex = 0;

  if (m_new_delete_ndb_cond)
    NdbCondition_Destroy(m_new_delete_ndb_cond);
  m_new_delete_ndb_cond = 0;
  
  if(m_multi_wait_group)
    delete m_multi_wait_group;
  m_multi_wait_group = 0;

  m_uri_scheme.clear();
  m_uri_path.clear();
  m_uri_host.clear();

  DBUG_VOID_RETURN;
}

void
Ndb_cluster_connection::lock_ndb_objects() const
{
  NdbMutex_Lock(m_impl.m_new_delete_ndb_mutex);
}

void
Ndb_cluster_connection::unlock_ndb_objects() const
{
  NdbMutex_Unlock(m_impl.m_new_delete_ndb_mutex);
}

const Ndb*
Ndb_cluster_connection::get_next_ndb_object(const Ndb* p)
{
  if (p == 0)
    return m_impl.m_first_ndb_object;
  
  return p->theImpl->m_next_ndb_object;
}

void
Ndb_cluster_connection_impl::link_ndb_object(Ndb* p)
{
  lock_ndb_objects();
  if (m_first_ndb_object != 0)
  {
    m_first_ndb_object->theImpl->m_prev_ndb_object = p;
  }
  
  p->theImpl->m_next_ndb_object = m_first_ndb_object;
  m_first_ndb_object = p;
  
  // Wake up anyone waiting for changes to the Ndb instance list
  NdbCondition_Broadcast(m_new_delete_ndb_cond);

  unlock_ndb_objects();
}

void
Ndb_cluster_connection_impl::unlink_ndb_object(Ndb* p)
{
  lock_ndb_objects();
  Ndb* prev = p->theImpl->m_prev_ndb_object;
  Ndb* next = p->theImpl->m_next_ndb_object;

  if (prev == 0)
  {
    assert(m_first_ndb_object == p);
    m_first_ndb_object = next;
  }
  else
  {
    prev->theImpl->m_next_ndb_object = next;
  }

  if (next)
  {
    next->theImpl->m_prev_ndb_object = prev;
  }
  
  p->theImpl->m_prev_ndb_object = 0;
  p->theImpl->m_next_ndb_object = 0;

  /* This Ndb is leaving for a better place,
   * record its contribution to global warming
   * for posterity
   */
  for (Uint32 i=0; i<Ndb::NumClientStatistics; i++)
  {
    globalApiStatsBaseline[i] += p->theImpl->clientStats[i];
  }

  // Wake up anyone waiting for changes to the Ndb instance list
  NdbCondition_Broadcast(m_new_delete_ndb_cond);

  unlock_ndb_objects();  
}

Uint32
Ndb_cluster_connection_impl::get_next_transid(Uint32 reference) const
{    
  Uint32 next = 0;
  reference = m_transporter_facade->mapRefToIdx(reference);
  
  /* Access map with lock to avoid resize issues */
  lock_ndb_objects();
  {
    if (reference < m_next_transids.size())
    {
      next = m_next_transids[reference];
    }
  }
  unlock_ndb_objects();
  
  return next;
}

void 
Ndb_cluster_connection_impl::set_next_transid(Uint32 reference, Uint32 value)
{
  reference = m_transporter_facade->mapRefToIdx(reference);
  
  /* Access map with lock to avoid resize issues */
  lock_ndb_objects();
  {
    Uint32 zero = 0;
    m_next_transids.set(value, reference, zero);
  }
  unlock_ndb_objects();
}

/**
 * adjust_node_proximity
 *
 * A negative adjustment means nearer.
 *
 * May rearrange m_nodes_proximity and change links and group value.
 * The vector m_nodes_proximity itself, including size(), is not modified
 * only the elements within.
 *
 * m_nodes_proximity_mutex must be locked and m_nodes_proximity_seqlock
 * must be locked for write.
 */
void
Ndb_cluster_connection_impl::adjust_node_proximity(Uint32 node_id, Int32 adjustment)
{
  assert(m_db_nodes.get(node_id)); // Ensured in set_data_node_neighbour

  if (adjustment == 0)
    return; // No change

  Uint32 old_idx;
  for (old_idx = 0; old_idx < m_nodes_proximity.size(); old_idx++)
  {
    if (m_nodes_proximity[old_idx].id == node_id)
      break;
  }
  require(old_idx < m_nodes_proximity.size());

  const Int32 old_group = m_nodes_proximity[old_idx].adjusted_group;
  const Int32 new_group = old_group + adjustment;
  Node node = m_nodes_proximity[old_idx];
  node.adjusted_group = new_group;

  Uint32 new_idx;
  if (adjustment < 0)
  {
    /**
     * Node is moved to be new last in its new group.
     */
    for (new_idx = 0; new_idx < old_idx; new_idx ++)
    {
      if (m_nodes_proximity[new_idx].adjusted_group > new_group)
        break;
    }
    /**
     * Move nodes between new_idx (inclusive) and old_idx (exclusive) up,
     * making room for node in its new group.
     */
    for (Uint32 idx = old_idx;
         idx > new_idx;
         idx--)
    {
      m_nodes_proximity[idx] = m_nodes_proximity[idx-1];
      m_nodes_proximity[idx].this_group_idx++;
      if (m_nodes_proximity[idx].next_group_idx > 0 &&
          m_nodes_proximity[idx].next_group_idx <= old_idx)
      {
        m_nodes_proximity[idx].next_group_idx++;
        if (m_nodes_proximity[idx].next_group_idx == m_nodes_proximity.size())
        {
          m_nodes_proximity[idx].next_group_idx = 0;
        }
      }
    }
    /**
     * For elements after old place with same group, this_group_idx needs
     * increase.
     */
    for (Uint32 idx = old_idx + 1;
         idx < m_nodes_proximity.size() &&
           m_nodes_proximity[idx].adjusted_group == old_group;
         idx++)
    {
      m_nodes_proximity[idx].this_group_idx++;
    }
    /**
     * Update this_group_idx and next_group_idx for node.
     */
    if (new_idx == 0)
    {
      node.this_group_idx = 0;
    }
    else if (m_nodes_proximity[new_idx - 1].adjusted_group == new_group)
    {
      node.this_group_idx = m_nodes_proximity[new_idx - 1].this_group_idx;
    }
    else
    {
      node.this_group_idx = new_idx;
    }
    Uint32 next_group_idx = new_idx + 1;
    if (next_group_idx < m_nodes_proximity.size())
    {
      node.next_group_idx = next_group_idx;
    }
    else
    {
      node.next_group_idx = 0;
    }
  }
  else
  {
    /**
     * Node is moved to be first in its new group.
     */
    for (new_idx = old_idx; new_idx + 1 < m_nodes_proximity.size(); new_idx++)
    {
      if (m_nodes_proximity[new_idx + 1].adjusted_group >= new_group)
        break;
    }
    /**
     * Move nodes between old_idx (exclusive) and new_idx (inclusive) down,
     * making room for node in its new group.
     */
    for (Uint32 idx = old_idx;
         idx < new_idx;
         idx++)
    {
      m_nodes_proximity[idx] = m_nodes_proximity[idx + 1];
      if (m_nodes_proximity[idx].this_group_idx > old_idx)
      {
        m_nodes_proximity[idx].this_group_idx--;
      }
      if (m_nodes_proximity[idx].next_group_idx > 0 &&
          m_nodes_proximity[idx].next_group_idx < new_idx)
      {
        m_nodes_proximity[idx].next_group_idx--;
      }
      else
      {
        m_nodes_proximity[idx].next_group_idx = new_idx;
      }
    }
    /**
     * Update this_group_idx and next_group_idx for node.
     */
    if (old_idx < new_idx)
    {
      node.this_group_idx = new_idx;
    }
    if (new_idx + 1 == m_nodes_proximity.size())
    {
      node.next_group_idx = 0;
    }
    else if (m_nodes_proximity[new_idx + 1].adjusted_group == new_group)
    {
      node.next_group_idx = m_nodes_proximity[new_idx + 1].next_group_idx;
    }
    else
    {
      node.next_group_idx = new_idx + 1;
    }
  }
  m_nodes_proximity[new_idx] = node;

  /**
   * Clear hint count in new group since the node adjusted will not have a
   * hint count in sync with its new group.
   */
  for (Uint32 idx = node.this_group_idx;
       idx <= new_idx;
       idx++)
  {
    m_nodes_proximity[idx].hint_count = 0;
  }
}

void
Ndb_cluster_connection_impl::set_data_node_neighbour(Uint32 node)
{
  Uint32 const old_node = m_data_node_neighbour;
  if (old_node == node)
    return; // No change

  NdbMutex_Lock(m_nodes_proximity_mutex);
  if (old_node != 0 && m_db_nodes.get(old_node))
  {
    adjust_node_proximity(old_node,
                          -DATA_NODE_NEIGHBOUR_PROXIMITY_ADJUSTMENT);
  }
  if (node != 0 && m_db_nodes.get(node))
  {
    adjust_node_proximity(node, DATA_NODE_NEIGHBOUR_PROXIMITY_ADJUSTMENT);
  }
  m_data_node_neighbour = node;
  NdbMutex_Unlock(m_nodes_proximity_mutex);
}

void
Ndb_cluster_connection_impl::set_name(const char *name)
{
  NdbMgmHandle h= m_config_retriever->get_mgmHandle();
  ndb_mgm_set_name(h, name);
}

int
Ndb_cluster_connection_impl::set_service_uri(const char * scheme,
                                             const char * host,
                                             int port, const char * path)
{
  if(! ProcessInfo::isValidUri(scheme, path))
  {
    return 1;
  }

  /* Clear out existing values */
  m_uri_scheme.clear();
  m_uri_host.clear();
  m_uri_port = 0;
  m_uri_path.clear();

  /* If already connected, ClusterMgr will send new ProcessInfo reports.
     Otherwise save a copy of values until connected.
  */
  if(m_transporter_facade->theClusterMgr->getNoOfConnectedNodes())
  {
    m_transporter_facade->theClusterMgr->setProcessInfoUri(scheme, host,
                                                           port, path);
  }
  else
  {
    m_uri_scheme.assign(scheme);
    m_uri_host.assign(host);
    m_uri_port = port;
    m_uri_path.assign(path);
  }

  return 0;
}

int
Ndb_cluster_connection_impl::init_nodes_vector(Uint32 nodeid,
					       const ndb_mgm_configuration 
					       &config)
{
  DBUG_ENTER("Ndb_cluster_connection_impl::init_nodes_vector");
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_CONNECTION);
  
  for(iter.first(); iter.valid(); iter.next())
  {
    Uint32 nodeid1, nodeid2, remoteNodeId, group= 5;
    const char * remoteHostName= 0, * localHostName= 0;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeid1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeid2)) continue;

    if(nodeid1 != nodeid && nodeid2 != nodeid) continue;
    remoteNodeId = (nodeid == nodeid1 ? nodeid2 : nodeid1);

    iter.get(CFG_CONNECTION_GROUP, &group);

    {
      const char * host1= 0, * host2= 0;
      iter.get(CFG_CONNECTION_HOSTNAME_1, &host1);
      iter.get(CFG_CONNECTION_HOSTNAME_2, &host2);
      localHostName  = (nodeid == nodeid1 ? host1 : host2);
      remoteHostName = (nodeid == nodeid1 ? host2 : host1);
    }

    Uint32 type = ~0;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;

    switch(type){
    case CONNECTION_TYPE_SHM:{
      break;
    }
    case CONNECTION_TYPE_SCI:{
      break;
    }
    case CONNECTION_TYPE_TCP:{
      // connecting through localhost
      // check if config_hostname is local
      if (SocketServer::tryBind(0,remoteHostName))
	group--; // upgrade group value
      break;
    }
    }
    m_db_nodes.set(remoteNodeId);
    if (m_nodes_proximity.push_back(Node(group,remoteNodeId)))
    {
      DBUG_RETURN(-1);
    }
    DBUG_PRINT("info",("saved %d %d", group,remoteNodeId));
    for (int i= m_nodes_proximity.size()-2;
	 i >= 0 &&
           m_nodes_proximity[i].adjusted_group >
             m_nodes_proximity[i+1].adjusted_group;
	 i--)
    {
      Node tmp= m_nodes_proximity[i];
      m_nodes_proximity[i]= m_nodes_proximity[i+1];
      m_nodes_proximity[i+1]= tmp;
    }
  }

  int i;
  Int32 cur_group= INVALID_PROXIMITY_GROUP;
  Uint32 group_idx= 0;
  for (i= (int)m_nodes_proximity.size()-1; i >= 0; i--)
  {
    if (m_nodes_proximity[i].adjusted_group != cur_group)
    {
      cur_group= m_nodes_proximity[i].adjusted_group;
      group_idx= i+1;
    }
    m_nodes_proximity[i].next_group_idx= group_idx;
  }
  cur_group= INVALID_PROXIMITY_GROUP;
  for (i= 0; i < (int)m_nodes_proximity.size(); i++)
  {
    if (m_nodes_proximity[i].adjusted_group != cur_group)
    {
      cur_group= m_nodes_proximity[i].adjusted_group;
      group_idx= i;
    }
    m_nodes_proximity[i].this_group_idx= group_idx;
  }
#if 0
  for (i= 0; i < (int)m_nodes_proximity.size(); i++)
  {
    fprintf(stderr, "[%d] %d %d %d %d\n",
	   i,
	   m_nodes_proximity[i].id,
	   m_nodes_proximity[i].adjusted_group,
	   m_nodes_proximity[i].this_group_idx,
	   m_nodes_proximity[i].next_group_idx);
  }

  do_test();
#endif
  DBUG_RETURN(0);
}

Uint32
Ndb_cluster_connection_impl::get_db_nodes(Uint8 arr[MAX_NDB_NODES]) const
{
  require(m_db_nodes.count() < MAX_NDB_NODES);
  Uint32 cnt = 0;
  for (Uint32 node_id = m_db_nodes.find_first();
       node_id != NdbNodeBitmask::NotFound;
       node_id = m_db_nodes.find_next(node_id + 1))
  {
    arr[cnt] = node_id;
    cnt++;
  }
  return cnt;
}

Uint32
Ndb_cluster_connection_impl::get_unconnected_nodes() const
{
  TransporterFacade *tp = m_transporter_facade;

  NdbNodeBitmask connected; // All nodes connected
  NdbNodeBitmask started;   // All started nodes known by connected db nodes

  tp->lock_poll_mutex();
  for (Uint32 node_id = m_db_nodes.find_first();
       node_id != NdbNodeBitmask::NotFound;
       node_id = m_db_nodes.find_next(node_id + 1))
  {
    const trp_node& node = tp->theClusterMgr->getNodeInfo(node_id);
    if (!node.m_alive)
    {
      continue;
    }
    connected.set(node_id);
    NdbNodeBitmask nodes;
    // Truncate NodeBitmask to NdbNodeBitmask, data nodes are in lower bits
    nodes.assign(nodes.Size, node.m_state.m_connected_nodes.rep.data);
    started.bitOR(nodes);
  }
  tp->unlock_poll_mutex();

  if (connected.count() == 0)
  {
    /**
     * No db nodes connected, means all unconnected.
     */
    assert(m_db_nodes.count() ==
             m_nodes_proximity.size());
    return m_nodes_proximity.size();
  }

  /**
   * Return count of started but not connected db nodes
   */
  started.bitAND(m_db_nodes);
  return started.bitANDC(connected).count();
}

int
Ndb_cluster_connection_impl::configure(Uint32 nodeId,
                                       const ndb_mgm_configuration &config)
{
  DBUG_ENTER("Ndb_cluster_connection_impl::configure");
  {
    ndb_mgm_configuration_iterator iter(config, CFG_SECTION_NODE);
    if(iter.find(CFG_NODE_ID, nodeId))
      DBUG_RETURN(-1);

    // Configure scan settings
    Uint32 scan_batch_size= 0;
    if (!iter.get(CFG_MAX_SCAN_BATCH_SIZE, &scan_batch_size)) {
      m_config.m_scan_batch_size= scan_batch_size;
    }
    Uint32 batch_byte_size= 0;
    if (!iter.get(CFG_BATCH_BYTE_SIZE, &batch_byte_size)) {
      m_config.m_batch_byte_size= batch_byte_size;
    }
    Uint32 batch_size= 0;
    if (!iter.get(CFG_BATCH_SIZE, &batch_size)) {
      m_config.m_batch_size= batch_size;
    }

    Uint32 queue = 0;
    if (!iter.get(CFG_DEFAULT_OPERATION_REDO_PROBLEM_ACTION, &queue))
    {
      m_config.m_default_queue_option = queue;
    }

    Uint32 default_hashmap_size = 0;
    if (!iter.get(CFG_DEFAULT_HASHMAP_SIZE, &default_hashmap_size) &&
        default_hashmap_size != 0)
    {
      m_config.m_default_hashmap_size = default_hashmap_size;
    }

    Uint32 verbose= 0;
    if (!iter.get(CFG_API_VERBOSE, &verbose))
    {
      m_config.m_verbose = verbose;
    }

    // If DefaultHashmapSize is not set or zero, use the minimum
    // value set (not zero) for any other node, since this size
    // should be supported by the other nodes.  Also this allows
    // the DefaultHashmapSize to be set for the entire cluster
    // if set for a single node or node type.
    // Otherwise use NDB_DEFAULT_HASHMAP_BUCKETS
    if (default_hashmap_size == 0)
    {
      // Use new iterator to leave iter valid.
      ndb_mgm_configuration_iterator iterall(config, CFG_SECTION_NODE);
      for (; iterall.valid(); iterall.next())
      {
        Uint32 tmp = 0;
        if (!iterall.get(CFG_DEFAULT_HASHMAP_SIZE, &tmp) &&
            tmp != 0 &&
            ((default_hashmap_size == 0) || (tmp < default_hashmap_size)))
          default_hashmap_size = tmp;
      }
      if (default_hashmap_size == 0)
        default_hashmap_size = NDB_DEFAULT_HASHMAP_BUCKETS;

      m_config.m_default_hashmap_size = default_hashmap_size;
    }

    // Configure timeouts
    {
      Uint32 timeout = 120000;
      // Use new iterator to leave iter valid.
      ndb_mgm_configuration_iterator iterall(config, CFG_SECTION_NODE);
      for (; iterall.valid(); iterall.next())
      {
        Uint32 tmp1 = 0, tmp2 = 0;
        iterall.get(CFG_DB_TRANSACTION_CHECK_INTERVAL, &tmp1);
        iterall.get(CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT, &tmp2);
        tmp1 += tmp2;
        if (tmp1 > timeout)
          timeout = tmp1;
      }
      m_config.m_waitfor_timeout = timeout;
    }
  }

  // System name
  ndb_mgm_configuration_iterator s_iter(config, CFG_SECTION_SYSTEM);
  const char * tmp_system_name;
  s_iter.get(CFG_SYS_NAME, & tmp_system_name);
  m_system_name.assign(tmp_system_name);

  DBUG_RETURN(init_nodes_vector(nodeId, config));
}

void
Ndb_cluster_connection_impl::do_test()
{
  Ndb_cluster_connection_node_iter iter;
  int n= no_db_nodes()+5;
  Uint32 *nodes= new Uint32[n+1];

  for (int g= 0; g < n; g++)
  {
    for (int h= 0; h < n; h++)
    {
      Uint32 id;
      Ndb_cluster_connection_node_iter iter2;
      {
	for (int j= 0; j < g; j++)
	{
	  nodes[j]= get_next_node(iter2);
	}
      }

      for (int i= 0; i < n; i++)
      {
	init_get_next_node(iter);
	fprintf(stderr, "%d dead:(", g);
	id= 0;
	while (id == 0)
	{
	  if ((id= get_next_node(iter)) == 0)
	    break;
	  for (int j= 0; j < g; j++)
	  {
	    if (nodes[j] == id)
	    {
	      fprintf(stderr, " %d", id);
	      id= 0;
	      break;
	    }
	  }
	}
	fprintf(stderr, ")");
	if (id == 0)
	{
	  break;
	}
	fprintf(stderr, " %d\n", id);
      }
      fprintf(stderr, "\n");
    }
  }
  delete [] nodes;
}

void Ndb_cluster_connection::set_data_node_neighbour(Uint32 node)
{
  m_impl.set_data_node_neighbour(node);
}

void Ndb_cluster_connection::set_name(const char *name)
{
  m_impl.set_name(name);
}

int Ndb_cluster_connection::set_service_uri(const char * scheme,
                                            const char * host, int port,
                                            const char * path)
{
  return m_impl.set_service_uri(scheme, host, port, path);
}

const char * Ndb_cluster_connection::get_system_name() const
{
  return m_impl.m_system_name.c_str();
}

int Ndb_cluster_connection_impl::connect(int no_retries,
                                         int retry_delay_in_seconds,
                                         int verbose)
{
  DBUG_ENTER("Ndb_cluster_connection::connect");
  do {
    if (m_config_retriever == 0)
    {
      if (!m_latest_error)
      {
        m_latest_error = 1;
        m_latest_error_msg.assign("Ndb_cluster_connection init "
                                  "error: m_config_retriever==0");
      }
      DBUG_PRINT("exit", ("no m_config_retriever, ret: -1"));
      DBUG_RETURN(-1);
    }

    // the allocNodeId function will connect if not connected
    int alloc_error;
    Uint32 nodeId = m_config_retriever->allocNodeId(no_retries,
                                                   retry_delay_in_seconds,
                                                   verbose, alloc_error);
    if (!nodeId)
    {
      // Failed to allocate nodeid from mgmt server, find out
      // the cause and set proper error message

      if (!m_config_retriever->is_connected())
      {
        // Could not connect to mgmt server
        m_latest_error = alloc_error;
        m_latest_error_msg.assfmt("%s", m_config_retriever->getErrorString());
        DBUG_RETURN(1); // Recoverable error
      }

      if (alloc_error == NDB_MGM_ALLOCID_ERROR)
      {
        // A nodeid for this node was found in config, but it was not
        // free right now. Retry later and it might be free.
        m_latest_error = alloc_error;
        m_latest_error_msg.assfmt("%s", m_config_retriever->getErrorString());
        DBUG_RETURN(1); // Recoverable error
      }

      // Fatal error, use default error
      break;
    }

    ndb_mgm_configuration * props = m_config_retriever->getConfig(nodeId);
    if(props == 0)
      break;

    if (configure(nodeId, *props))
    {
      ndb_mgm_destroy_configuration(props);
      DBUG_PRINT("exit", ("malloc failure, ret: -1"));
      DBUG_RETURN(-1);
    }

    if (m_transporter_facade->start_instance(nodeId, props) < 0)
    {
      ndb_mgm_destroy_configuration(props);
      DBUG_RETURN(-1);
    }
    m_transporter_facade->theClusterMgr->setProcessInfoUri(m_uri_scheme.c_str(),
                                                           m_uri_host.c_str(),
                                                           m_uri_port,
                                                           m_uri_path.c_str());
    ndb_mgm_destroy_configuration(props);
    m_transporter_facade->connected();
    m_latest_error = 0;
    m_latest_error_msg.assign("");
    DBUG_PRINT("exit", ("connect ok, ret: 0"));
    DBUG_RETURN(0);
  } while(0);

  const char* erString = m_config_retriever->getErrorString();
  if (erString == 0) {
    erString = "No error specified!";
  }
  m_latest_error = 1;
  m_latest_error_msg.assfmt("Configuration error: %s", erString);
  ndbout << get_latest_error_msg() << endl;
  DBUG_PRINT("exit", ("connect failed, '%s' ret: -1", erString));
  DBUG_RETURN(-1);
}


int
Ndb_cluster_connection::connect(int no_retries,
                                int retry_delay_in_seconds,
                                int verbose)
{
  return m_impl.connect(no_retries, retry_delay_in_seconds, verbose);
}


void Ndb_cluster_connection_impl::connect_thread()
{
  DBUG_ENTER("Ndb_cluster_connection_impl::connect_thread");
  int r;
  do {
    // Wait before making a new connect attempt
    NdbSleep_SecSleep(1);

    if ((r = connect(0,0,0)) == 0)
      break;
    if (r == -1) {
      printf("Ndb_cluster_connection::connect_thread error\n");
      DBUG_ASSERT(false);
      m_run_connect_thread= 0;
    }
  } while (m_run_connect_thread);
  if (m_connect_callback)
    (*m_connect_callback)();
  DBUG_VOID_RETURN;
}

Uint64 *
Ndb_cluster_connection::get_latest_trans_gci()
{
 return m_impl.get_latest_trans_gci(); 
} 

void
Ndb_cluster_connection::init_get_next_node(Ndb_cluster_connection_node_iter &iter)
{
  m_impl.init_get_next_node(iter);
}

Uint32
Ndb_cluster_connection::get_next_node(Ndb_cluster_connection_node_iter &iter)
{
  return m_impl.get_next_node(iter);
}

unsigned int 
Ndb_cluster_connection::get_next_alive_node(Ndb_cluster_connection_node_iter &iter)
{
  return m_impl.get_next_alive_node(iter);
}

unsigned
Ndb_cluster_connection::get_active_ndb_objects() const
{
  return m_impl.m_transporter_facade->get_active_ndb_objects();
}

int Ndb_cluster_connection::set_timeout(int timeout_ms)
{
  return ndb_mgm_set_timeout(m_impl.m_config_retriever->get_mgmHandle(),
                             timeout_ms);
}

int
Ndb_cluster_connection::get_auto_reconnect() const
{
  return m_impl.m_transporter_facade->get_auto_reconnect();
}

void
Ndb_cluster_connection::set_auto_reconnect(int value)
{
  m_impl.m_transporter_facade->set_auto_reconnect(value);
}

Uint32 
Ndb_cluster_connection::collect_client_stats(Uint64* statsArr, Uint32 sz)
{
  /* We have a global stats baseline which contains all
   * the stats for Ndb objects which have been and gone.
   * Start with that, then add in stats for Ndb objects
   * currently in use.
   * Note that despite the lock, this is not thread safe
   * as we are reading data that other threads may be
   * concurrently writing.  The lock just guards against
   * concurrent changes to the set of active Ndbs while
   * we are iterating it.
   */
  const Uint32 relevant = MIN((Uint32)Ndb::NumClientStatistics, sz);
  const Ndb* ndb = NULL;
  lock_ndb_objects();
  {
    memcpy(statsArr, &m_impl.globalApiStatsBaseline[0], sizeof(Uint64)*relevant);
  
    while((ndb = get_next_ndb_object(ndb)) != NULL)
    {
      for (Uint32 i=0; i<relevant; i++)
      {
        statsArr[i] += ndb->theImpl->clientStats[i];
      }
    }
  }
  unlock_ndb_objects();

  return relevant;
}

void
Ndb_cluster_connection::set_max_adaptive_send_time(Uint32 milliseconds)
{
  m_impl.m_transporter_facade->setSendThreadInterval(milliseconds);
}

Uint32
Ndb_cluster_connection::get_max_adaptive_send_time()
{
  return m_impl.m_transporter_facade->getSendThreadInterval();
}

NdbWaitGroup *
Ndb_cluster_connection::create_ndb_wait_group(int size)
{
  if(m_impl.m_multi_wait_group == NULL)
  {
    m_impl.m_multi_wait_group = new NdbWaitGroup(this, size);
    return m_impl.m_multi_wait_group;
  }
  else
  {
    return NULL;  // NdbWaitGroup already exists
  }
}

bool
Ndb_cluster_connection::release_ndb_wait_group(NdbWaitGroup *group)
{
  if(m_impl.m_multi_wait_group && m_impl.m_multi_wait_group == group)
  {
    delete m_impl.m_multi_wait_group;
    m_impl.m_multi_wait_group = 0;
    return true;
  }
  else
  {
    return false;
  }
}

Uint32
Ndb_cluster_connection_impl::select_node(const Uint16 * nodes,
                                         Uint32 cnt)
{
  if (cnt == 1)
  {
    return nodes[0];
  }
  else if (cnt == 0)
  {
    return 0;
  }

  NdbNodeBitmask checked;
  Node *nodes_arr = m_nodes_proximity.getBase();
  const Uint32 nodes_arr_cnt = m_nodes_proximity.size();

  Uint32 best_node = nodes[0];
  Uint32 best_idx;
  Uint32 best_usage;
  Int32 best_score = MAX_PROXIMITY_GROUP; // Lower is better

  if (!m_impl.m_optimized_node_selection)
  {
    /**
     * optimized_node_selection is off.  Use round robin.
     * Uses hint_count in m_nodes_proximity but not the group value.
     */
    for (Uint32 j = 0; j < cnt; j++)
    {
      Uint32 candidate_node = nodes[j];
      if (checked.get(candidate_node))
        continue;

      checked.set(candidate_node);

      for (Uint32 i = 0; i < nodes_arr_cnt; i++)
      {
        if (nodes_arr[i].id == j)
        {
          Uint32 usage = nodes_arr[i].hint_count;
          if (best_score == MAX_PROXIMITY_GROUP)
          {
            best_idx = i;
            best_node = candidate_node;
            best_score = 0;
            best_usage = usage;
          }
          else if (best_usage - usage < HINT_COUNT_HALF)
          {
            best_idx = i;
            best_node = candidate_node;
            best_usage = usage;
          }
          break;
        }
      }
    }
  }
  else
  {
    /**
     * optimized_node_selection is on.  Use proximity.
     */
    for (Uint32 j = 0; j < cnt; j++)
    {
      Uint32 candidate_node = nodes[j];
      if (checked.get(candidate_node))
        continue;

      checked.set(candidate_node);

      for (Uint32 i = 0; i < nodes_arr_cnt; i++)
      {
        if (nodes_arr[i].adjusted_group > best_score)
        {
          // We already got a better match
          break;
        }
        if (nodes_arr[i].id == candidate_node)
        {
          if (nodes_arr[i].adjusted_group < best_score)
          {
            best_idx = i;
            best_node = candidate_node;
            best_score = nodes_arr[i].adjusted_group;
            best_usage = nodes_arr[i].hint_count;
          }
          else if (nodes_arr[i].adjusted_group == best_score)
          {
            Uint32 usage = nodes_arr[i].hint_count;
            if (best_usage - usage < HINT_COUNT_HALF)
            {
              /**
               * hint_count may wrap, for this calculation it is assummed that
               * the two counts should be near each other, and so if the
               * difference is small above, best_usage is greater than usage.
               */
              best_idx = i;
              best_node = candidate_node;
              best_usage = usage;
            }
          }
          break;
        }
      }
    }
  }
  nodes_arr[best_idx].hint_count =
    (nodes_arr[best_idx].hint_count + 1) & HINT_COUNT_MASK;
  return best_node;
}

template class Vector<Ndb_cluster_connection_impl::Node>;

int
Ndb_cluster_connection::wait_until_ready(const int * nodes, int cnt,
                                         int timeout)
{
  DBUG_ENTER("Ndb_cluster_connection::wait_until_ready(nodelist)");

  NodeBitmask mask;
  for (int i = 0; i < cnt; i++)
  {
    if (nodes[i] <= 0 || nodes[i] > (int)mask.max_size())
    {
      DBUG_RETURN(-1);
    }
    mask.set(nodes[i]);
  }

  TransporterFacade *tp = m_impl.m_transporter_facade;
  if (tp == 0)
  {
    DBUG_RETURN(-1);
  }
  if (tp->ownId() == 0)
  {
    DBUG_RETURN(-1);
  }

  timeout *= 10; // try each 100ms

  NodeBitmask dead;
  NodeBitmask alive;
  do
  {
    dead.clear();
    alive.clear();
    tp->lock_poll_mutex();
    for (Uint32 node_id = m_impl.m_db_nodes.find_first();
         node_id != NdbNodeBitmask::NotFound;
         node_id = m_impl.m_db_nodes.find_next(node_id + 1))
    {
      //************************************************
      // If any node is answering, ndb is answering
      //************************************************
      if (tp->get_node_alive(node_id) != 0)
        alive.set(node_id);
      else
        dead.set(node_id);
    }
    tp->unlock_poll_mutex();

    if (alive.contains(mask))
    {
      DBUG_RETURN(mask.count());
    }

    NodeBitmask all;
    all.bitOR(alive);
    all.bitOR(dead);
    if (!all.contains(mask))
    {
      DBUG_RETURN(-1);
    }

    if (timeout == 0)
      break;

    timeout--;
    NdbSleep_MilliSleep(100);
  } while (true);

  mask.bitAND(alive);
  DBUG_RETURN(mask.count());
}

