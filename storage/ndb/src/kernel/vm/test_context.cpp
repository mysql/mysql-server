/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "blocks/dbdih/Dbdih.hpp"
#include "GlobalData.hpp"
#include "ndbapi/NdbApi.hpp"
#include "ndbd_malloc_impl.hpp"
#include "NdbTick.h"
#include "SimBlockList.hpp"
#include "SimulatedBlock.hpp"
#include "test_context.hpp"

#define JAM_FILE_ID 250


struct DummyBlock : public SimulatedBlock
{
  DummyBlock(int no, Block_context& ctx) : SimulatedBlock(no, ctx) {}
  //~DummyBlock() { }
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
    rl.m_max = Resource_limit::HIGHEST_LIMIT;
    rl.m_resource_id = resid;
    mm.set_resource_limit(rl);
  }

  if (!mm.init(NULL /* watchCounter */, pages))
  {
    abort();
  }

  Uint32 dummy_watchdog_counter_marking_page_mem = 0;
  mm.map(&dummy_watchdog_counter_marking_page_mem, 0 /* memlock */); // Map all

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

Uint32
Dbdih::dihGetInstanceKeyCanFail(Uint32 tabId, Uint32 fragId)
{
  abort();
  return 0;
}

Dbdih::~Dbdih()
{
}
