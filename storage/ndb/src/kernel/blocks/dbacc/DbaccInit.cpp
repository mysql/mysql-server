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

#define DBACC_C
#include "Dbacc.hpp"
#include "util/require.h"

#define JAM_FILE_ID 346

#define DEBUG(x) \
  { ndbout << "ACC::" << x << endl; }

Uint64 Dbacc::getTransactionMemoryNeed(
    const Uint32 ldm_instance_count,
    const ndb_mgm_configuration_iterator *mgm_cfg, const bool use_reserved) {
  Uint32 acc_scan_recs = 0;
  Uint32 acc_op_recs = 0;

  if (use_reserved) {
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_ACC_RESERVED_SCAN_RECORDS,
                                       &acc_scan_recs));
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LDM_RESERVED_OPERATIONS,
                                       &acc_op_recs));
  } else {
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_ACC_SCAN, &acc_scan_recs));
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_ACC_OP_RECS, &acc_op_recs));
  }
  Uint64 scan_byte_count = 0;
  scan_byte_count += ScanRec_pool::getMemoryNeed(acc_scan_recs);
  scan_byte_count *= ldm_instance_count;

  Uint64 op_byte_count = 0;
  op_byte_count += Operationrec_pool::getMemoryNeed(acc_op_recs);
  op_byte_count *= ldm_instance_count;
  return (scan_byte_count + op_byte_count);
}

void Dbacc::initData() {
  ctablesize = ZTABLESIZE;
  cfragmentsize = ZFRAGMENTSIZE;

  Pool_context pc;
  pc.m_block = this;
  if (!m_is_query_block) {
    directoryPool.init(RT_DBACC_DIRECTORY, pc);
    directoryPoolPtr = &directoryPool;
  } else {
    directoryPoolPtr = 0;
  }

  fragmentrec = 0;
  tabrec = 0;

  void *ptr = m_ctx.m_mm.get_memroot();
  c_page_pool.set((Page32 *)ptr, (Uint32)~0);

  c_allow_use_of_spare_pages = false;
  cfreeopRec = RNIL;

  cnoOfAllocatedPagesMax = cnoOfAllocatedPages = cpageCount = 0;
  // Records with constant sizes

  RSS_OP_COUNTER_INIT(cnoOfFreeFragrec);

}  // Dbacc::initData()

void Dbacc::initRecords(const ndb_mgm_configuration_iterator *mgm_cfg) {
  jam();
#if defined(USE_INIT_GLOBAL_VARIABLES)
  {
    void *tmp[] = {&fragrecptr, &operationRecPtr, &queOperPtr, &scanPtr,
                   &tabptr};
    init_global_ptrs(tmp, sizeof(tmp) / sizeof(tmp[0]));
  }
#endif
  cfreepages.init();
  ndbassert(pages.getCount() - cfreepages.getCount() + cnoOfAllocatedPages ==
            cpageCount);

  if (m_is_query_block) {
    cfragmentsize = 0;
    ctablesize = 0;
  }
  fragmentrec = (Fragmentrec *)allocRecord("Fragmentrec", sizeof(Fragmentrec),
                                           cfragmentsize);

  tabrec = (Tabrec *)allocRecord("Tabrec", sizeof(Tabrec), ctablesize);

  /**
   * Records moved into poolification is created and the
   * static part of the pool is allocated as well.
   */

  Pool_context pc;
  pc.m_block = this;

  Uint32 reserveScanRecs = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_ACC_RESERVED_SCAN_RECORDS,
                                        &reserveScanRecs));
  if (m_is_query_block) {
    reserveScanRecs = 1;
  }
  scanRec_pool.init(ScanRec::TYPE_ID, pc, reserveScanRecs, UINT32_MAX);
  while (scanRec_pool.startup()) {
    refresh_watch_dog();
  }

  Uint32 reserveOpRecs = 1;
  ndbrequire(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LDM_RESERVED_OPERATIONS,
                                        &reserveOpRecs));
  reserveOpRecs += 200;
  if (m_is_query_block) {
    reserveOpRecs = 200;
  }
  oprec_pool.init(Operationrec::TYPE_ID, pc, reserveOpRecs, UINT32_MAX);
  while (oprec_pool.startup()) {
    refresh_watch_dog();
  }
  if (!m_is_query_block) {
    ndbrequire(oprec_pool.seize(operationRecPtr));
    operationRecPtr.p->userptr = RNIL;
    operationRecPtr.p->userblockref = 0;
    c_copy_frag_oprec = operationRecPtr.i;
  }
}  // Dbacc::initRecords()

Dbacc::Dbacc(Block_context &ctx, Uint32 instanceNumber, Uint32 blockNo)
    : SimulatedBlock(blockNo, ctx, instanceNumber),
      c_tup(0),
      c_page8_pool(c_page_pool) {
  BLOCK_CONSTRUCTOR(Dbacc);

  // Transit signals
  if (blockNo == DBACC) {
    addRecSignal(GSN_DUMP_STATE_ORD, &Dbacc::execDUMP_STATE_ORD);
    addRecSignal(GSN_DEBUG_SIG, &Dbacc::execDEBUG_SIG);
    addRecSignal(GSN_CONTINUEB, &Dbacc::execCONTINUEB);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dbacc::execACC_CHECK_SCAN);
    addRecSignal(GSN_EXPANDCHECK2, &Dbacc::execEXPANDCHECK2);
    addRecSignal(GSN_SHRINKCHECK2, &Dbacc::execSHRINKCHECK2);

    // Received signals
    addRecSignal(GSN_STTOR, &Dbacc::execSTTOR);
    addRecSignal(GSN_ACCSEIZEREQ, &Dbacc::execACCSEIZEREQ);
    addRecSignal(GSN_ACCFRAGREQ, &Dbacc::execACCFRAGREQ);
    addRecSignal(GSN_NEXT_SCANREQ, &Dbacc::execNEXT_SCANREQ);
    addRecSignal(GSN_ACC_SCANREQ, &Dbacc::execACC_SCANREQ);
    addRecSignal(GSN_ACC_TO_REQ, &Dbacc::execACC_TO_REQ);
    addRecSignal(GSN_ACC_LOCKREQ, &Dbacc::execACC_LOCKREQ);
    addRecSignal(GSN_NDB_STTOR, &Dbacc::execNDB_STTOR);
    addRecSignal(GSN_DROP_TAB_REQ, &Dbacc::execDROP_TAB_REQ);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dbacc::execREAD_CONFIG_REQ, true);
    addRecSignal(GSN_DROP_FRAG_REQ, &Dbacc::execDROP_FRAG_REQ);

    addRecSignal(GSN_DBINFO_SCANREQ, &Dbacc::execDBINFO_SCANREQ);
    m_is_query_block = false;
    m_is_in_query_thread = false;
    m_lqh_block = DBLQH;
    m_ldm_instance_used = this;
  } else {
    m_lqh_block = DBQLQH;
    m_is_query_block = true;
    m_is_in_query_thread = true;
    m_ldm_instance_used = nullptr;
    ndbrequire(blockNo == DBQACC);
    addRecSignal(GSN_STTOR, &Dbacc::execSTTOR);
    addRecSignal(GSN_EXPANDCHECK2, &Dbacc::execEXPANDCHECK2);
    addRecSignal(GSN_SHRINKCHECK2, &Dbacc::execSHRINKCHECK2);
    addRecSignal(GSN_ACCSEIZEREQ, &Dbacc::execACCSEIZEREQ);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dbacc::execREAD_CONFIG_REQ, true);
    addRecSignal(GSN_NEXT_SCANREQ, &Dbacc::execNEXT_SCANREQ);
    addRecSignal(GSN_CONTINUEB, &Dbacc::execCONTINUEB);
    addRecSignal(GSN_DUMP_STATE_ORD, &Dbacc::execDUMP_STATE_ORD);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dbacc::execACC_CHECK_SCAN);
  }
  initData();

  c_transient_pools[DBACC_SCAN_RECORD_TRANSIENT_POOL_INDEX] = &scanRec_pool;
  c_transient_pools[DBACC_OPERATION_RECORD_TRANSIENT_POOL_INDEX] = &oprec_pool;
  static_assert(c_transient_pool_count == 2);
  c_transient_pools_shrinking.clear();
}  // Dbacc::Dbacc()

Dbacc::~Dbacc() {
  deallocRecord((void **)&fragmentrec, "Fragmentrec", sizeof(Fragmentrec),
                cfragmentsize);

  deallocRecord((void **)&tabrec, "Tabrec", sizeof(Tabrec), ctablesize);
}  // Dbacc::~Dbacc()
BLOCK_FUNCTIONS(Dbacc)
