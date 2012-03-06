/*
   Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBT_MIX_RESTARTER_HPP
#define NDBT_MIX_RESTARTER_HPP

#include <mgmapi.h>
#include <Vector.hpp>
#include <BaseString.hpp>
#include "NdbRestarter.hpp"
#include "NDBT_Test.hpp"

#define NMR_SR                       "SR"
#define NMR_SR_THREADS               "SR_ThreadCount"
#define NMR_SR_THREADS_STOPPED       "SR_ThreadsStoppedCount"
#define NMR_SR_VALIDATE_THREADS      "SR_ValidateThreadCount"
#define NMR_SR_VALIDATE_THREADS_DONE "SR_ValidateThreadsDoneCount"

class NdbMixRestarter : public NdbRestarter 
{
public:
  enum RestartTypeMask 
  {
    RTM_RestartCluster     = 0x01,
    RTM_RestartNode        = 0x02,
    RTM_RestartNodeInitial = 0x04,
    RTM_StopNode           = 0x08,
    RTM_StopNodeInitial    = 0x10,
    RTM_StartNode          = 0x20,

    RTM_COUNT = 6,

    RTM_ALL = 0xFF,
    RTM_SR  = RTM_RestartCluster,
    RTM_NR  = 0x2 | 0x4 | 0x8 | 0x10 | 0x20
  };
  
  enum SR_State {
    SR_RUNNING    = 0,
    SR_STOPPING   = 1,
    SR_STOPPED    = 2,
    SR_VALIDATING = 3
  };
  
  NdbMixRestarter(unsigned * seed = 0, const char* _addr = 0);
  ~NdbMixRestarter();

  void setRestartTypeMask(Uint32 mask);
  int runUntilStopped(NDBT_Context* ctx, NDBT_Step* step, Uint32 freq);
  int runPeriod(NDBT_Context* ctx, NDBT_Step* step, Uint32 time, Uint32 freq);
  
  int init(NDBT_Context* ctx, NDBT_Step* step);
  int dostep(NDBT_Context* ctx, NDBT_Step* step);
  int finish(NDBT_Context* ctx, NDBT_Step* step);

private:
  unsigned * seed;
  unsigned ownseed;
  Uint32 m_mask;
  Vector<ndb_mgm_node_state> m_nodes;
  int restart_cluster(NDBT_Context* ctx, NDBT_Step* step, bool abort = true);
};

#endif
