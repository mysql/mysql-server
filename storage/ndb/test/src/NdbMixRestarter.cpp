/* Copyright (C) 2003 MySQL AB

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

#include "NdbMixRestarter.hpp"

NdbMixRestarter::NdbMixRestarter(const char* _addr) :
  NdbRestarter(_addr),
  m_mask(~(Uint32)0)
{
}

NdbMixRestarter::~NdbMixRestarter()
{
  
}

#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int
NdbMixRestarter::restart_cluster(NDBT_Context* ctx, 
                                 NDBT_Step* step,
                                 bool stopabort)
{
  int timeout = 180;
  int result = NDBT_OK;

  do 
  {
    ctx->setProperty(NMR_SR_THREADS_STOPPED, (Uint32)0);
    ctx->setProperty(NMR_SR_VALIDATE_THREADS_DONE, (Uint32)0);
    
    ndbout << " -- Shutting down " << endl;
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_STOPPING);
    CHECK(restartAll(false, true, stopabort) == 0);
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_STOPPED);
    CHECK(waitClusterNoStart(timeout) == 0);
    
    Uint32 cnt = ctx->getProperty(NMR_SR_THREADS);
    Uint32 curr= ctx->getProperty(NMR_SR_THREADS_STOPPED);
    while(curr != cnt && !ctx->isTestStopped())
    {
      if (curr > cnt)
      {
        ndbout_c("stopping: curr: %d cnt: %d", curr, cnt);
        abort();
      }
      
      NdbSleep_MilliSleep(100);
      curr= ctx->getProperty(NMR_SR_THREADS_STOPPED);
    }

    CHECK(ctx->isTestStopped() == false);
    CHECK(startAll() == 0);
    CHECK(waitClusterStarted(timeout) == 0);
    
    cnt = ctx->getProperty(NMR_SR_VALIDATE_THREADS);
    if (cnt)
    {
      ndbout << " -- Validating starts " << endl;
      ctx->setProperty(NMR_SR_VALIDATE_THREADS_DONE, (Uint32)0);
      ctx->setProperty(NMR_SR, NdbMixRestarter::SR_VALIDATING);
      curr = ctx->getProperty(NMR_SR_VALIDATE_THREADS_DONE);
      while (curr != cnt && !ctx->isTestStopped())
      {
        if (curr > cnt)
        {
          ndbout_c("validating: curr: %d cnt: %d", curr, cnt);
          abort();
        }

        NdbSleep_MilliSleep(100);
        curr = ctx->getProperty(NMR_SR_VALIDATE_THREADS_DONE);
      }
      ndbout << " -- Validating complete " << endl;
    }
    CHECK(ctx->isTestStopped() == false);    
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_RUNNING);

  } while(0);

  return result;
}

static
ndb_mgm_node_state*
select_node_to_stop(Vector<ndb_mgm_node_state>& nodes)
{
  Uint32 i, j;
  Vector<ndb_mgm_node_state*> alive_nodes;
  for(i = 0; i<nodes.size(); i++)
  {
    ndb_mgm_node_state* node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_STARTED)
      alive_nodes.push_back(node);
  }

  Vector<ndb_mgm_node_state*> victims;
  // Remove those with one in node group
  for(i = 0; i<alive_nodes.size(); i++)
  {
    int group = alive_nodes[i]->node_group;
    for(j = 0; j<alive_nodes.size(); j++) 
    {
      if (i != j && alive_nodes[j]->node_group == group)
      {
	victims.push_back(alive_nodes[i]);
	break;
      }
    }
  }

  if (victims.size())
  {
    int victim = rand() % victims.size();
    return victims[victim];
  }
  else
  {
    return 0;
  }
}

static
ndb_mgm_node_state*
select_node_to_start(Vector<ndb_mgm_node_state>& nodes)
{
  Uint32 i;
  Vector<ndb_mgm_node_state*> victims;
  for(i = 0; i<nodes.size(); i++)
  {
    ndb_mgm_node_state* node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_NOT_STARTED)
      victims.push_back(node);
  }

  if (victims.size())
  {
    int victim = rand() % victims.size();
    return victims[victim];
  }
  else
  {
    return 0;
  }
}

void
NdbMixRestarter::setRestartTypeMask(Uint32 mask)
{
  m_mask = mask;
}

int
NdbMixRestarter::runUntilStopped(NDBT_Context* ctx, 
                                 NDBT_Step* step, 
                                 Uint32 freq)
{
  if (init(ctx, step))
    return NDBT_FAILED;

  while (!ctx->isTestStopped())
  {
    if (dostep(ctx, step))
      return NDBT_FAILED;
    NdbSleep_SecSleep(freq);
  }
  
  if (!finish(ctx, step))
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int
NdbMixRestarter::runPeriod(NDBT_Context* ctx, 
                           NDBT_Step* step, 
                           Uint32 period, Uint32 freq)
{
  if (init(ctx, step))
    return NDBT_FAILED;

  Uint32 stop = time(0) + period;
  while (!ctx->isTestStopped() && (time(0) < stop))
  {
    if (dostep(ctx, step))
    {
      return NDBT_FAILED;
    }
    NdbSleep_SecSleep(freq);
  }
  
  if (finish(ctx, step))
  {
    return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
NdbMixRestarter::init(NDBT_Context* ctx, NDBT_Step* step)
{
  waitClusterStarted();
  m_nodes = ndbNodes;
  return 0;
}

int 
NdbMixRestarter::dostep(NDBT_Context* ctx, NDBT_Step* step)
{
  ndb_mgm_node_state* node = 0;
  int action;
loop:
  while(((action = (1 << (rand() % RTM_COUNT))) & m_mask) == 0);
  switch(action){
  case RTM_RestartCluster:
    if (restart_cluster(ctx, step))
      return NDBT_FAILED;
    for (Uint32 i = 0; i<m_nodes.size(); i++)
      m_nodes[i].node_status = NDB_MGM_NODE_STATUS_STARTED;
    break;
  case RTM_RestartNode:
  case RTM_RestartNodeInitial:
  case RTM_StopNode:
  case RTM_StopNodeInitial:
  {
    if ((node = select_node_to_stop(m_nodes)) == 0)
      goto loop;
    
    if (action == RTM_RestartNode || action == RTM_RestartNodeInitial)
      ndbout << "Restarting " << node->node_id;
    else
      ndbout << "Stopping " << node->node_id;
    
    bool initial = 
      action == RTM_RestartNodeInitial || action == RTM_StopNodeInitial;

    if (initial)
      ndbout << " inital";
    ndbout << endl;
    
    if (restartOneDbNode(node->node_id, initial, true, true))
      return NDBT_FAILED;
      
    if (waitNodesNoStart(&node->node_id, 1))
      return NDBT_FAILED;
    
    node->node_status = NDB_MGM_NODE_STATUS_NOT_STARTED;
    
    if (action == RTM_StopNode || action == RTM_StopNodeInitial)
      break;
    else
      goto start;
  }
  case RTM_StartNode:
    if ((node = select_node_to_start(m_nodes)) == 0)
      goto loop;
start:
    ndbout << "Starting " << node->node_id << endl;
    if (startNodes(&node->node_id, 1))
      return NDBT_FAILED;
    if (waitNodesStarted(&node->node_id, 1))
      return NDBT_FAILED;
    
    node->node_status = NDB_MGM_NODE_STATUS_STARTED;      
    break;
  }
  return NDBT_OK;
}

int 
NdbMixRestarter::finish(NDBT_Context* ctx, NDBT_Step* step)
{
  Vector<int> not_started;
  {
    ndb_mgm_node_state* node = 0;
    while((node = select_node_to_start(m_nodes)))
    {
      not_started.push_back(node->node_id);
      node->node_status = NDB_MGM_NODE_STATUS_STARTED;
    }
  }
  
  if (not_started.size())
  {
    ndbout << "Starting stopped nodes " << endl;
    if (startNodes(not_started.getBase(), not_started.size()))
      return NDBT_FAILED;
    if (waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

template class Vector<ndb_mgm_node_state*>;
