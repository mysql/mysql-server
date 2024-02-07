/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include "SimBlockList.hpp"
#include <NdbEnv.h>
#include <Backup.hpp>
#include <BackupProxy.hpp>
#include <Cmvmi.hpp>
#include <DbUtil.hpp>
#include <Dbacc.hpp>
#include <DbaccProxy.hpp>
#include <Dbdict.hpp>
#include <Dbdih.hpp>
#include <Dbinfo.hpp>
#include <Dblqh.hpp>
#include <DblqhProxy.hpp>
#include <Dbqacc.hpp>
#include <DbqaccProxy.hpp>
#include <Dbqlqh.hpp>
#include <DbqlqhProxy.hpp>
#include <Dbqtup.hpp>
#include <DbqtupProxy.hpp>
#include <Dbqtux.hpp>
#include <DbqtuxProxy.hpp>
#include <Dbspj.hpp>
#include <DbspjProxy.hpp>
#include <Dbtc.hpp>
#include <DbtcProxy.hpp>
#include <Dbtup.hpp>
#include <DbtupProxy.hpp>
#include <Dbtux.hpp>
#include <DbtuxProxy.hpp>
#include <Emulator.hpp>
#include <LocalProxy.hpp>
#include <Ndbcntr.hpp>
#include <Ndbfs.hpp>
#include <PgmanProxy.hpp>
#include <QBackup.hpp>
#include <QBackupProxy.hpp>
#include <QRestore.hpp>
#include <QRestoreProxy.hpp>
#include <Qmgr.hpp>
#include <RestoreProxy.hpp>
#include <SimulatedBlock.hpp>
#include <Suma.hpp>
#include <Trix.hpp>
#include <lgman.hpp>
#include <mt.hpp>
#include <pgman.hpp>
#include <restore.hpp>
#include <thrman.hpp>
#include <trpman.hpp>
#include <tsman.hpp>
#include "portlib/NdbMem.h"

#define JAM_FILE_ID 492

#define NEW_BLOCK(B) new B

void SimBlockList::load(EmulatorData &data) {
  noOfBlocks = NO_OF_BLOCKS;
  theList = new SimulatedBlock *[noOfBlocks];
  if (!theList) {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "Failed to create the block list", "");
  }

  Block_context ctx(*data.theConfiguration, *data.m_mem_manager);

  SimulatedBlock *fs = 0;
  {
    Uint32 dl;
    const ndb_mgm_configuration_iterator *p =
        ctx.m_config.getOwnConfigIterator();
    if (p && !ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &dl) && dl) {
      fs = NEW_BLOCK(VoidFs)(ctx);
    } else {
      fs = NEW_BLOCK(Ndbfs)(ctx);
    }
  }

  const bool mtLqh = globalData.isNdbMtLqh;

  if (!mtLqh)
    theList[0] = NEW_BLOCK(Pgman)(ctx);
  else
    theList[0] = NEW_BLOCK(PgmanProxy)(ctx);
  theList[1] = NEW_BLOCK(Lgman)(ctx);
  theList[2] = NEW_BLOCK(Tsman)(ctx);
  if (!mtLqh)
    theList[3] = NEW_BLOCK(Dbacc)(ctx);
  else
    theList[3] = NEW_BLOCK(DbaccProxy)(ctx);
  theList[4] = NEW_BLOCK(Cmvmi)(ctx);
  theList[5] = fs;
  theList[6] = NEW_BLOCK(Dbdict)(ctx);
  theList[7] = NEW_BLOCK(Dbdih)(ctx);
  if (!mtLqh)
    theList[8] = NEW_BLOCK(Dblqh)(ctx);
  else
    theList[8] = NEW_BLOCK(DblqhProxy)(ctx);
  if (globalData.ndbMtTcWorkers == 0)
    theList[9] = NEW_BLOCK(Dbtc)(ctx);
  else
    theList[9] = NEW_BLOCK(DbtcProxy)(ctx);
  if (!mtLqh)
    theList[10] = NEW_BLOCK(Dbtup)(ctx);
  else
    theList[10] = NEW_BLOCK(DbtupProxy)(ctx);
  theList[11] = NEW_BLOCK(Ndbcntr)(ctx);
  theList[12] = NEW_BLOCK(Qmgr)(ctx);
  theList[13] = NEW_BLOCK(Trix)(ctx);
  if (!mtLqh)
    theList[14] = NEW_BLOCK(Backup)(ctx);
  else
    theList[14] = NEW_BLOCK(BackupProxy)(ctx);
  theList[15] = NEW_BLOCK(DbUtil)(ctx);
  theList[16] = NEW_BLOCK(Suma)(ctx);
  if (!mtLqh)
    theList[17] = NEW_BLOCK(Dbtux)(ctx);
  else
    theList[17] = NEW_BLOCK(DbtuxProxy)(ctx);
  if (!mtLqh)
    theList[18] = NEW_BLOCK(Restore)(ctx);
  else
    theList[18] = NEW_BLOCK(RestoreProxy)(ctx);
  theList[19] = NEW_BLOCK(Dbinfo)(ctx);
  if (globalData.ndbMtTcWorkers == 0)
    theList[20] = NEW_BLOCK(Dbspj)(ctx);
  else
    theList[20] = NEW_BLOCK(DbspjProxy)(ctx);
  if (NdbIsMultiThreaded() == false)
    theList[21] = NEW_BLOCK(Thrman)(ctx);
  else
    theList[21] = NEW_BLOCK(ThrmanProxy)(ctx);
  if (NdbIsMultiThreaded() == false)
    theList[22] = NEW_BLOCK(Trpman)(ctx);
  else
    theList[22] = NEW_BLOCK(TrpmanProxy)(ctx);

  /* Create Query/Recover Blocks */
  theList[23] = NEW_BLOCK(DbqlqhProxy)(ctx);
  theList[24] = NEW_BLOCK(DbqaccProxy)(ctx);
  theList[25] = NEW_BLOCK(DbqtupProxy)(ctx);
  theList[26] = NEW_BLOCK(DbqtuxProxy)(ctx);
  theList[27] = NEW_BLOCK(QBackupProxy)(ctx);
  theList[28] = NEW_BLOCK(QRestoreProxy)(ctx);
  assert(NO_OF_BLOCKS == 29);

  // Check that all blocks could be created
  for (int i = 0; i < noOfBlocks; i++) {
    if (!theList[i]) {
      ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "Failed to create block", "");
    }
  }

  if (globalData.isNdbMt) {
    /**
      This is where we bind blocks to their respective threads.
      mt_init_thr_map binds the blocks to the two main threads,
      the thread for Global blocks (thr = 0), and the thread
      for Local blocks (thr = 1) and it puts CMVMI into the receiver
      thread.

      For those blocks where we created proxies above the loadWorkers
      function will map the instances of the block into the right
      thread. mt_add_thr_map will be called for each of the block
      instances.
    */
    mt_init_thr_map();
    for (int i = 0; i < noOfBlocks; i++) {
      if (theList[i]) theList[i]->loadWorkers();
    }
    mt_finalize_thr_map();
  }
}

void SimBlockList::unload() {
  if (theList != 0) {
    for (int i = 0; i < noOfBlocks; i++) {
      if (theList[i] != 0) {
#ifdef VM_TRACE
        theList[i]->~SimulatedBlock();
        free(theList[i]);
#else
        delete (theList[i]);
#endif
        theList[i] = 0;
      }
    }
    delete[] theList;
    theList = 0;
    noOfBlocks = 0;
  }
}

Uint64 SimBlockList::getTransactionMemoryNeed(
    const Uint32 dbtc_instance_count, const Uint32 ldm_instance_count,
    const ndb_mgm_configuration_iterator *mgm_cfg,
    const bool use_reserved) const {
  Uint64 byte_count = Dbtc::getTransactionMemoryNeed(dbtc_instance_count,
                                                     mgm_cfg, use_reserved);
  byte_count += Dbacc::getTransactionMemoryNeed(ldm_instance_count, mgm_cfg,
                                                use_reserved);
  byte_count += Dblqh::getTransactionMemoryNeed(ldm_instance_count, mgm_cfg,
                                                use_reserved);
  byte_count += Dbtup::getTransactionMemoryNeed(ldm_instance_count, mgm_cfg,
                                                use_reserved);
  byte_count += Dbtux::getTransactionMemoryNeed(ldm_instance_count, mgm_cfg,
                                                use_reserved);

  byte_count += Dbqacc::getTransactionMemoryNeed();
  byte_count += Dbqlqh::getTransactionMemoryNeed();
  byte_count += Dbqtup::getTransactionMemoryNeed();
  byte_count += Dbqtux::getTransactionMemoryNeed();
  return byte_count;
}
