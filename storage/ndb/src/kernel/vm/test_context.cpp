/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "blocks/dbdih/Dbdih.hpp"
#include "GlobalData.hpp"
#include "ndbapi/NdbApi.hpp"
#include "ndbd_malloc_impl.hpp"
#include "NdbTick.h"
#include "SimBlockList.hpp"
#include "SimulatedBlock.hpp"
#include "test_context.hpp"

struct DummyBlock : public SimulatedBlock
{
  DummyBlock(int no, Block_context& ctx) : SimulatedBlock(no, ctx) {}
  ~DummyBlock() { }
};

static Ndbd_mem_manager mm;
static Configuration cfg;
static Block_context ctx(cfg, mm);
static DummyBlock block(DBACC, ctx);

// Force enough modules from libkernel that libsched need
static SimulatedBlock::MutexManager mxm(block);
static SafeCounterManager scm(block);

Pool_context
test_context(Uint32 pages)
{
  ndb_init();

  Pool_context pc;
  pc.m_block = &block;

  Resource_limit rl;
  for (Uint32 resid = 1; resid < RG_COUNT; resid++)
  {
    rl.m_min = 0;
    rl.m_max = 0;
    rl.m_resource_id = resid;
    mm.set_resource_limit(rl);
  }
  rl.m_min = 0;
  rl.m_max = pages;
  rl.m_resource_id = 0;
  mm.set_resource_limit(rl);

  if (!mm.init(NULL /* watchCounter */))
  {
    abort();
  }

  mm.map(NULL /* watchCounter */, 0 /* memlock */); // Map all

  return pc;
}

void dummy_calls_to_force_some_modules_from_libkernel_needed_by_libsched()
{
  globalData.getBlock(0,0);
  Ndbinfo::getNumTables();
}

// Some undefined globals needed

Uint32 g_currentStartPhase;
Uint32 g_start_type;
NdbNodeBitmask g_nowait_nodes;

void
SimBlockList::unload()
{

}

void
NdbShutdown(int error_code,
            NdbShutdownType type,
            NdbRestartType restartType)
{
  abort();
}

Uint32
Dbdih::dihGetInstanceKey(Uint32 tabId, Uint32 fragId)
{
  abort();
  return 0;
}
