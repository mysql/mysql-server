/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "NdbMixRestarter.hpp"

NdbMixRestarter::NdbMixRestarter(unsigned *_seed, const char *_addr)
    : NdbRestarter(_addr), m_mask(~(Uint32)0) {
  if (_seed == 0) {
    ownseed = (unsigned)NdbTick_CurrentMillisecond();
    seed = &ownseed;
  } else {
    seed = _seed;
  }
}

NdbMixRestarter::~NdbMixRestarter() {}

#define CHECK(b)                                                           \
  if (!(b)) {                                                              \
    ndbout << "ERR: " << step->getName() << " failed on line " << __LINE__ \
           << endl;                                                        \
    result = NDBT_FAILED;                                                  \
    continue;                                                              \
  }

int NdbMixRestarter::restart_cluster(NDBT_Context *ctx, NDBT_Step *step,
                                     bool stopabort) {
  int timeout = 180;
  int result = NDBT_OK;

  do {
    ndbout << " -- Shutting down " << endl;
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_STOPPING);
    CHECK(restartAll(false, true, stopabort) == 0);
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_STOPPED);
    ndbout << " -- waitClusterNoStart" << endl;
    CHECK(waitClusterNoStart(timeout) == 0);
    ndbout << " -- available" << endl;

    while (ctx->getProperty(NMR_SR_THREADS_ACTIVE) > 0 &&
           !ctx->isTestStopped()) {
      ndbout << "Await threads to stop"
             << ", active: " << ctx->getProperty(NMR_SR_THREADS_ACTIVE) << endl;

      NdbSleep_MilliSleep(100);
    }

    ndbout << " -- startAll" << endl;
    CHECK(startAll() == 0);

    ndbout << " -- waitClusterStarted" << endl;
    CHECK(waitClusterStarted(timeout) == 0);
    ndbout << " -- Started" << endl;

    if (ctx->getProperty(NMR_SR_VALIDATE_THREADS) > 0) {
      ndbout << " -- Validating starts " << endl;
      ctx->setProperty(NMR_SR, NdbMixRestarter::SR_VALIDATING);

      while (ctx->getProperty(NMR_SR_VALIDATE_THREADS_ACTIVE) > 0 &&
             !ctx->isTestStopped()) {
        NdbSleep_MilliSleep(100);
      }

      ndbout << " -- Validating complete " << endl;
    }
    ctx->setProperty(NMR_SR, NdbMixRestarter::SR_RUNNING);

  } while (0);

  return result;
}

static void select_nodes_to_stop(Vector<ndb_mgm_node_state *> &victims,
                                 Vector<ndb_mgm_node_state> &nodes) {
  Uint32 i, j;
  Vector<ndb_mgm_node_state *> alive_nodes;
  for (i = 0; i < nodes.size(); i++) {
    ndb_mgm_node_state *node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_STARTED)
      alive_nodes.push_back(node);
  }

  // Remove those with one in node group
  for (i = 0; i < alive_nodes.size(); i++) {
    int group = alive_nodes[i]->node_group;
    for (j = 0; j < alive_nodes.size(); j++) {
      if (i != j && alive_nodes[j]->node_group == group) {
        victims.push_back(alive_nodes[i]);
        break;
      }
    }
  }
}

static ndb_mgm_node_state *select_node_to_stop(
    unsigned *seed, Vector<ndb_mgm_node_state> &nodes) {
  Vector<ndb_mgm_node_state *> victims;
  select_nodes_to_stop(victims, nodes);

  if (victims.size()) {
    int victim = ndb_rand_r(seed) % victims.size();
    return victims[victim];
  } else {
    return 0;
  }
}

static void select_nodes_to_start(Vector<ndb_mgm_node_state *> &victims,
                                  Vector<ndb_mgm_node_state> &nodes) {
  Uint32 i;
  for (i = 0; i < nodes.size(); i++) {
    ndb_mgm_node_state *node = &nodes[i];
    if (node->node_status == NDB_MGM_NODE_STATUS_NOT_STARTED)
      victims.push_back(node);
  }
}

static ndb_mgm_node_state *select_node_to_start(
    unsigned *seed, Vector<ndb_mgm_node_state> &nodes) {
  Vector<ndb_mgm_node_state *> victims;
  select_nodes_to_start(victims, nodes);

  if (victims.size()) {
    int victim = ndb_rand_r(seed) % victims.size();
    return victims[victim];
  } else {
    return 0;
  }
}

void NdbMixRestarter::setRestartTypeMask(Uint32 mask) { m_mask = mask; }

int NdbMixRestarter::runUntilStopped(NDBT_Context *ctx, NDBT_Step *step,
                                     Uint32 freq) {
  if (init(ctx, step)) {
    ndbout << "Line: " << __LINE__ << " init failed" << endl;
    return NDBT_FAILED;
  }

  while (!ctx->isTestStopped()) {
    if (dostep(ctx, step)) {
      ndbout << "Line: " << __LINE__ << " dostep failed" << endl;
      return NDBT_FAILED;
    }
    NdbSleep_SecSleep(freq);
  }

  if (!finish(ctx, step)) {
    ndbout << "Line: " << __LINE__ << " finish failed" << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int NdbMixRestarter::runPeriod(NDBT_Context *ctx, NDBT_Step *step,
                               Uint32 period, Uint32 freq) {
  if (init(ctx, step)) {
    ndbout << "Line: " << __LINE__ << " init failed" << endl;
    return NDBT_FAILED;
  }

  Uint32 stop = (Uint32)time(0) + period;
  while (!ctx->isTestStopped() && ((Uint32)time(0) < stop)) {
    if (dostep(ctx, step)) {
      ndbout << "Line: " << __LINE__ << " dostep failed" << endl;
      return NDBT_FAILED;
    }
    NdbSleep_SecSleep(freq);
  }

  if (finish(ctx, step)) {
    ndbout << "Line: " << __LINE__ << " finish failed" << endl;
    return NDBT_FAILED;
  }

  ctx->stopTest();
  return NDBT_OK;
}

int NdbMixRestarter::init(NDBT_Context *ctx, NDBT_Step *step) {
  waitClusterStarted();
  m_nodes = ndbNodes;
  return 0;
}

int NdbMixRestarter::dostep(NDBT_Context *ctx, NDBT_Step *step) {
  ndb_mgm_node_state *node = 0;
  int action;
loop:
  while (((action = (1 << (ndb_rand_r(seed) % RTM_COUNT))) & m_mask) == 0)
    ;
  switch (action) {
    case RTM_RestartCluster:
      if (restart_cluster(ctx, step)) {
        ndbout << "Line: " << __LINE__ << " restart_cluster failed" << endl;
        return NDBT_FAILED;
      }
      ndbout << " -- cluster restarted" << endl;
      for (Uint32 i = 0; i < m_nodes.size(); i++)
        m_nodes[i].node_status = NDB_MGM_NODE_STATUS_STARTED;
      break;
    case RTM_RestartNode:
    case RTM_RestartNodeInitial:
    case RTM_StopNode:
    case RTM_StopNodeInitial: {
      if ((node = select_node_to_stop(seed, m_nodes)) == 0) goto loop;

      if (action == RTM_RestartNode || action == RTM_RestartNodeInitial)
        ndbout << "Restarting " << node->node_id;
      else
        ndbout << "Stopping " << node->node_id;

      bool initial =
          action == RTM_RestartNodeInitial || action == RTM_StopNodeInitial;

      if (initial) ndbout << " inital";
      ndbout << endl;

      ndbout << " -- restartOneDbNode" << endl;
      if (restartOneDbNode(node->node_id, initial, true, true)) {
        ndbout << "Line: " << __LINE__ << " restart node failed" << endl;
        return NDBT_FAILED;
      }

      ndbout << " -- waitNodesNoStart" << endl;
      if (waitNodesNoStart(&node->node_id, 1)) {
        ndbout << "Line: " << __LINE__ << " wait node nostart failed" << endl;
        return NDBT_FAILED;
      }

      node->node_status = NDB_MGM_NODE_STATUS_NOT_STARTED;

      if (action != RTM_StopNode && action != RTM_StopNodeInitial) goto start;

      break;
    }
    case RTM_StartNode:
      if ((node = select_node_to_start(seed, m_nodes)) == 0) goto loop;
    start:
      ndbout << "Starting " << node->node_id << endl;
      if (startNodes(&node->node_id, 1)) {
        ndbout << "Line: " << __LINE__ << " start node failed" << endl;
        return NDBT_FAILED;
      }

      ndbout << " -- waitNodesStarted" << endl;
      if (waitNodesStarted(&node->node_id, 1)) {
        ndbout << "Line: " << __LINE__ << " wait node start failed" << endl;
        return NDBT_FAILED;
      }

      ndbout << "Started " << node->node_id << endl;
      node->node_status = NDB_MGM_NODE_STATUS_STARTED;
      break;
  }
  ndbout << "Step done" << endl;
  return NDBT_OK;
}

int NdbMixRestarter::finish(NDBT_Context *ctx, NDBT_Step *step) {
  Vector<int> not_started;
  {
    ndb_mgm_node_state *node = 0;
    while ((node = select_node_to_start(seed, m_nodes))) {
      not_started.push_back(node->node_id);
      node->node_status = NDB_MGM_NODE_STATUS_STARTED;
    }
  }

  if (not_started.size()) {
    ndbout << "Starting stopped nodes " << endl;
    if (startNodes(not_started.getBase(), not_started.size())) {
      ndbout << "Line: " << __LINE__ << " start node failed" << endl;
      return NDBT_FAILED;
    }
    if (waitClusterStarted()) {
      ndbout << "Line: " << __LINE__ << " wait cluster failed" << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

template class Vector<ndb_mgm_node_state *>;
