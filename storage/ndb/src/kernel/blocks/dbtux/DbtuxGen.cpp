/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#define DBTUX_GEN_CPP
#include "util/require.h"
#include "Dbtux.hpp"

#include <signaldata/NodeStateSignalData.hpp>

#ifdef VM_TRACE
//#define DO_TRANSIENT_POOL_STAT
#endif

#define JAM_FILE_ID 365

Dbtux::Dbtux(Block_context& ctx,
             Uint32 instanceNumber,
             Uint32 blockNo) :
  SimulatedBlock(blockNo, ctx, instanceNumber),
  c_tup(0),
  c_lqh(0),
  c_acc(0),
  c_descPageList(RNIL),
#ifdef VM_TRACE
  debugFile(0),
  tuxDebugOut(*new NullOutputStream()),
  debugFlags(0),
#endif
  c_internalStartPhase(0),
  c_typeOfStart(NodeState::ST_ILLEGAL_TYPE),
  c_indexStatAutoUpdate(false),
  c_indexStatSaveSize(0),
  c_indexStatSaveScale(0),
  c_indexStatTriggerPct(0),
  c_indexStatTriggerScale(0),
  c_indexStatUpdateDelay(0)
{
  BLOCK_CONSTRUCTOR(Dbtux);
  // verify size assumptions (also when release-compiled)
  ndbrequire(
      (sizeof(TreeEnt) & 0x3) == 0 &&
      (sizeof(TreeNode) & 0x3) == 0 &&
      (sizeof(DescHead) & 0x3) == 0 &&
      (sizeof(KeyType) & 0x3) == 0
  );
  /*
   * DbtuxGen.cpp
   */
  if (blockNo == DBTUX)
  {
    addRecSignal(GSN_CONTINUEB, &Dbtux::execCONTINUEB);
    addRecSignal(GSN_STTOR, &Dbtux::execSTTOR);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dbtux::execREAD_CONFIG_REQ, true);
    /*
     * DbtuxMeta.cpp
     */
    addRecSignal(GSN_CREATE_TAB_REQ, &Dbtux::execCREATE_TAB_REQ);
    addRecSignal(GSN_TUXFRAGREQ, &Dbtux::execTUXFRAGREQ);
    addRecSignal(GSN_TUX_ADD_ATTRREQ, &Dbtux::execTUX_ADD_ATTRREQ);
    addRecSignal(GSN_ALTER_INDX_IMPL_REQ, &Dbtux::execALTER_INDX_IMPL_REQ);
    addRecSignal(GSN_DROP_TAB_REQ, &Dbtux::execDROP_TAB_REQ);
    /*
     * DbtuxMaint.cpp
     */
    addRecSignal(GSN_TUX_MAINT_REQ, &Dbtux::execTUX_MAINT_REQ);
    /*
     * DbtuxScan.cpp
     */
    addRecSignal(GSN_ACC_SCANREQ, &Dbtux::execACC_SCANREQ);
    addRecSignal(GSN_TUX_BOUND_INFO, &Dbtux::execTUX_BOUND_INFO);
    addRecSignal(GSN_NEXT_SCANREQ, &Dbtux::execNEXT_SCANREQ);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dbtux::execACC_CHECK_SCAN);
    addRecSignal(GSN_ACCKEYCONF, &Dbtux::execACCKEYCONF);
    addRecSignal(GSN_ACCKEYREF, &Dbtux::execACCKEYREF);
    addRecSignal(GSN_ACC_ABORTCONF, &Dbtux::execACC_ABORTCONF);
    /*
     * DbtuxStat.cpp
     */
    addRecSignal(GSN_INDEX_STAT_REP, &Dbtux::execINDEX_STAT_REP);
    addRecSignal(GSN_INDEX_STAT_IMPL_REQ, &Dbtux::execINDEX_STAT_IMPL_REQ);
    /*
     * DbtuxDebug.cpp
     */
    addRecSignal(GSN_DUMP_STATE_ORD, &Dbtux::execDUMP_STATE_ORD);
    addRecSignal(GSN_DBINFO_SCANREQ, &Dbtux::execDBINFO_SCANREQ);
    addRecSignal(GSN_NODE_STATE_REP, &Dbtux::execNODE_STATE_REP, true);

    addRecSignal(GSN_DROP_FRAG_REQ, &Dbtux::execDROP_FRAG_REQ);
    m_is_query_block = false;
    m_acc_block = DBACC;
    m_lqh_block = DBLQH;
    m_tux_block = DBTUX;
  }
  else
  {
    m_is_query_block = true;
    m_acc_block = DBQACC;
    m_lqh_block = DBQLQH;
    m_tux_block = DBQTUX;
    ndbrequire(blockNo == DBQTUX);
    addRecSignal(GSN_CONTINUEB, &Dbtux::execCONTINUEB);
    addRecSignal(GSN_STTOR, &Dbtux::execSTTOR);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dbtux::execREAD_CONFIG_REQ, true);
    addRecSignal(GSN_TUX_MAINT_REQ, &Dbtux::execTUX_MAINT_REQ);
    addRecSignal(GSN_ACC_SCANREQ, &Dbtux::execACC_SCANREQ);
    addRecSignal(GSN_TUX_BOUND_INFO, &Dbtux::execTUX_BOUND_INFO);
    addRecSignal(GSN_NEXT_SCANREQ, &Dbtux::execNEXT_SCANREQ);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dbtux::execACC_CHECK_SCAN);
    addRecSignal(GSN_ACCKEYCONF, &Dbtux::execACCKEYCONF);
    addRecSignal(GSN_ACCKEYREF, &Dbtux::execACCKEYREF);
    addRecSignal(GSN_ACC_ABORTCONF, &Dbtux::execACC_ABORTCONF);
    addRecSignal(GSN_DUMP_STATE_ORD, &Dbtux::execDUMP_STATE_ORD);
    addRecSignal(GSN_DBINFO_SCANREQ, &Dbtux::execDBINFO_SCANREQ);
    addRecSignal(GSN_NODE_STATE_REP, &Dbtux::execNODE_STATE_REP, true);
  }
  c_signal_bug32040 = 0;

  c_transient_pools[DBTUX_SCAN_OPERATION_TRANSIENT_POOL_INDEX] =
    &c_scanOpPool;
  c_transient_pools[DBTUX_SCAN_LOCK_TRANSIENT_POOL_INDEX] =
    &c_scanLockPool;
  c_transient_pools[DBTUX_SCAN_BOUND_TRANSIENT_POOL_INDEX] =
    &c_scanBoundPool;
  static_assert(c_transient_pool_count == 3);
  c_transient_pools_shrinking.clear();
}

Dbtux::~Dbtux()
{
}

Uint64 Dbtux::getTransactionMemoryNeed(
    const Uint32 ldm_instance_count,
    const ndb_mgm_configuration_iterator * mgm_cfg,
    bool use_reserved)
{
  Uint64 scan_op_byte_count = 0;
  Uint32 tux_scan_recs = 0;
  Uint32 tux_scan_lock_recs = 0;
  if (use_reserved)
  {
    require(!ndb_mgm_get_int_parameter(mgm_cfg,
                                       CFG_TUX_RESERVED_SCAN_RECORDS,
                                       &tux_scan_recs));
    tux_scan_lock_recs = 1000;
  }
  else
  {
    Uint32 scanBatch = 0;
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_TUX_SCAN_OP, &tux_scan_recs));
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LDM_BATCH_SIZE, &scanBatch));
    tux_scan_lock_recs = tux_scan_recs * scanBatch;
  }
  scan_op_byte_count += ScanOp_pool::getMemoryNeed(tux_scan_recs);
  scan_op_byte_count *= ldm_instance_count;

  Uint64 scan_lock_byte_count = 0;
  scan_lock_byte_count += ScanLock_pool::getMemoryNeed(tux_scan_lock_recs);
  scan_lock_byte_count *= ldm_instance_count;

  const Uint32 nScanBoundWords = tux_scan_recs * ScanBoundSegmentSize * 4;
  Uint64 scan_bound_byte_count = nScanBoundWords * ldm_instance_count;

  return (scan_op_byte_count + scan_lock_byte_count + scan_bound_byte_count);
}

void
Dbtux::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32* data = signal->getDataPtr();
  switch (data[0]) {
  case TuxContinueB::ShrinkTransientPools:
  {
    jam();
    Uint32 pool_index = signal->theData[1];
    ndbassert(signal->getLength() == 2);
    shrinkTransientPools(pool_index);
    return;
  }
#if (defined(VM_TRACE) || \
     defined(ERROR_INSERT)) && \
    defined(DO_TRANSIENT_POOL_STAT)

  case TuxContinueB::TransientPoolStat:
  {
    for (Uint32 pool_index = 0;
         pool_index < c_transient_pool_count;
         pool_index++)
    {
      g_eventLogger->info(
        "DBTUP %u: Transient slot pool %u %p: Entry size %u:"
       " Free %u: Used %u: Used high %u: Size %u: For shrink %u",
       instance(),
       pool_index,
       c_transient_pools[pool_index],
       c_transient_pools[pool_index]->getEntrySize(),
       c_transient_pools[pool_index]->getNoOfFree(),
       c_transient_pools[pool_index]->getUsed(),
       c_transient_pools[pool_index]->getUsedHi(),
       c_transient_pools[pool_index]->getSize(),
       c_transient_pools_shrinking.get(pool_index));
    }
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5000, 1);
    break;
  }
#endif
  case TuxContinueB::DropIndex: // currently unused
    {
      IndexPtr indexPtr;
      ndbrequire(c_indexPool.getPtr(indexPtr, data[1]));
      dropIndex(signal, indexPtr, data[2], data[3]);
    }
    break;
  case TuxContinueB::StatMon:
    {
      Uint32 id = data[1];
      ndbrequire(id == c_statMon.m_loopIndexId);
      statMonExecContinueB(signal);
    }
    break;
  default:
    ndbabort();
  }
}

/*
 * STTOR is sent to one block at a time.  In NDBCNTR it triggers
 * NDB_STTOR to the "old" blocks.  STTOR carries start phase (SP) and
 * NDB_STTOR carries internal start phase (ISP).
 *
 *      SP      ISP     activities
 *      1       none
 *      2       1       
 *      3       2       recover metadata, activate indexes
 *      4       3       recover data
 *      5       4-6     
 *      6       skip    
 *      7       skip    
 *      8       7       build non-logged indexes on SR
 *
 * DBTUX catches type of start (IS, SR, NR, INR) at SP 3 and updates
 * internal start phase at SP 7.  These are used to prevent index
 * maintenance operations caused by redo log at SR.
 */
void
Dbtux::execSTTOR(Signal* signal)
{
  jamEntry();
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    m_my_scan_instance = get_my_scan_instance();
    if (m_is_query_block)
    {
      c_tup = (Dbtup*)globalData.getBlock(DBQTUP, instance());
      ndbrequire(c_tup != 0);
      c_lqh = (Dblqh*)globalData.getBlock(DBQLQH, instance());
      ndbrequire(c_lqh != 0);
      c_acc = (Dbacc*)globalData.getBlock(DBQACC, instance());
      ndbrequire(c_acc != 0);
    }
    else
    {
      c_tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
      ndbrequire(c_tup != 0);
      c_lqh = (Dblqh*)globalData.getBlock(DBLQH, instance());
      ndbrequire(c_lqh != 0);
      c_acc = (Dbacc*)globalData.getBlock(DBACC, instance());
      ndbrequire(c_acc != 0);
    }
    c_signal_bug32040 = signal;
    break;
  case 3:
    jam();
#if (defined(VM_TRACE) || \
     defined(ERROR_INSERT)) && \
    defined(DO_TRANSIENT_POOL_STAT)
    /* Start reporting statistics for transient pools */
    signal->theData[0] = TuxContinueB::TransientPoolStat;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
#endif
    c_typeOfStart = signal->theData[7];
    break;
    return;
  case 7:
    c_internalStartPhase = 6;
    /*
     * config cannot yet be changed dynamically but we start the
     * loop always anyway because the cost is minimal
     */
    c_statMon.m_loopIndexId = 0;
    statMonSendContinueB(signal);
    break;
  default:
    jam();
    break;
  }
  if (m_is_query_block)
  {
    jam();
    signal->theData[0] = 0;       // garbage
    signal->theData[1] = 0;       // garbage
    signal->theData[2] = 0;       // garbage
    signal->theData[3] = 1;
    signal->theData[4] = 3;       // for c_typeOfStart
    signal->theData[5] = 255;
    sendSignal(DBQTUX_REF, GSN_STTORRY, signal, 6, JBB);
  }
  else
  {
    jam();
    signal->theData[0] = 0;       // garbage
    signal->theData[1] = 0;       // garbage
    signal->theData[2] = 0;       // garbage
    signal->theData[3] = 1;
    signal->theData[4] = 3;       // for c_typeOfStart
    signal->theData[5] = 7;       // for c_internalStartPhase
    signal->theData[6] = 255;
    BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : DBTUX_REF;
    sendSignal(cntrRef, GSN_STTORRY, signal, 7, JBB);
  }
}

void
Dbtux::execNODE_STATE_REP(Signal* signal)
{
  /**
   * This is to handle TO during SR
   *   and STUPID tux looks at c_typeOfStart in TUX_MAINT_REQ
   */
  NodeStateRep* rep = (NodeStateRep*)signal->getDataPtr();
  if (rep->nodeState.startLevel == NodeState::SL_STARTING)
  {
    c_typeOfStart = rep->nodeState.starting.restartType;
  }
  SimulatedBlock::execNODE_STATE_REP(signal);
}

void
Dbtux::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();
 
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  Uint32 nIndex;
  Uint32 nFragment;
  Uint32 nAttribute;
  Uint32 nScanOp; 
  Uint32 nScanBatch;
  Uint32 nStatAutoUpdate;
  Uint32 nStatSaveSize;
  Uint32 nStatSaveScale;
  Uint32 nStatTriggerPct;
  Uint32 nStatTriggerScale;
  Uint32 nStatUpdateDelay;

#if defined(USE_INIT_GLOBAL_VARIABLES)
  {
    void* tmp[] = {
      &c_ctx.scanPtr,
      &c_ctx.indexPtr,
      &c_ctx.fragPtr,
    }; 
    init_global_ptrs(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
  {
    void* tmp[] = {
      &c_ctx.keyAttrs,
      &c_ctx.tupIndexFragPtr,
      &c_ctx.tupIndexTablePtr,
      &c_ctx.tupRealFragPtr,
      &c_ctx.tupRealTablePtr,
    }; 
    init_global_uint32_ptrs(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
  {
    void* tmp[] = {
      &c_ctx.scanBoundCnt,
      &c_ctx.descending,
      &c_ctx.attrDataOffset,
      &c_ctx.tuxFixHeaderSize,
      &c_ctx.m_current_ent,
    }; 
    init_global_uint32(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_INDEX, &nIndex));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_FRAGMENT, &nFragment));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_ATTRIBUTE, &nAttribute));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_SCAN_OP, &nScanOp));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_BATCH_SIZE, &nScanBatch));

  nStatAutoUpdate = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_AUTO_UPDATE,
                            &nStatAutoUpdate);

  nStatSaveSize = 32768;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_SAVE_SIZE,
                            &nStatSaveSize);

  nStatSaveScale = 100;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_SAVE_SCALE,
                            &nStatSaveScale);

  nStatTriggerPct = 100;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_TRIGGER_PCT,
                            &nStatTriggerPct);

  nStatTriggerScale = 100;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_TRIGGER_SCALE,
                            &nStatTriggerScale);

  nStatUpdateDelay = 60;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_UPDATE_DELAY,
                            &nStatUpdateDelay);

  const Uint32 nDescPage = (nIndex * DescHeadSize +
                            nAttribute * KeyTypeSize +
                            nAttribute * AttributeHeaderSize +
                            DescPageSize - 1) / DescPageSize;
  const Uint32 nStatOp = 8;

  if (m_is_query_block)
  {
    c_fragOpPool.setSize(0);
    c_indexPool.setSize(0);
    c_fragPool.setSize(0);
    c_descPagePool.setSize(0);
  }
  else
  {
    c_fragOpPool.setSize(MaxIndexFragments);
    c_indexPool.setSize(nIndex);
    c_fragPool.setSize(nFragment);
    c_descPagePool.setSize(nDescPage);
  }
  c_statOpPool.setSize(nStatOp);
  c_indexStatAutoUpdate = nStatAutoUpdate;
  c_indexStatSaveSize = nStatSaveSize;
  c_indexStatSaveScale = nStatSaveScale;
  c_indexStatTriggerPct = nStatTriggerPct;
  c_indexStatTriggerScale = nStatTriggerScale;
  c_indexStatUpdateDelay = nStatUpdateDelay;

  /*
   * Index id is physical array index.  We seize and initialize all
   * index records now.
   */
  while (1) {
    jam();
    refresh_watch_dog();
    IndexPtr indexPtr;
    if (!c_indexPool.seize(indexPtr))
    {
      jam();
      break;
    }
    new (indexPtr.p) Index();
  }
  // allocate buffers
  c_ctx.jamBuffer = jamBuffer();
  c_ctx.c_searchKey = (Uint32*)allocRecord("c_searchKey",
                                           sizeof(Uint32),
                                           MaxAttrDataSize);
  c_ctx.c_nextKey = (Uint32*)allocRecord("c_nextKey",
                                         sizeof(Uint32),
                                         MaxAttrDataSize);
  c_ctx.c_entryKey = (Uint32*)allocRecord("c_entryKey",
                                          sizeof(Uint32),
                                          MaxAttrDataSize);

  c_ctx.c_dataBuffer = (Uint32*)allocRecord("c_dataBuffer",
                                            sizeof(Uint64),
                                            (MaxXfrmDataSize + 1) >> 1);
  c_ctx.c_boundBuffer = (Uint32*)allocRecord("c_boundBuffer",
                                             sizeof(Uint64),
                                             (MaxXfrmDataSize + 1) >> 1);

#ifdef VM_TRACE
  c_ctx.c_debugBuffer = (char*)allocRecord("c_debugBuffer",
                                           sizeof(char),
                                           DebugBufferBytes);
#endif

  Pool_context pc;
  pc.m_block = this;

  Uint32 reserveScanOpRecs = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_RESERVED_SCAN_RECORDS,
                                        &reserveScanOpRecs));
  if (m_is_query_block)
  {
    reserveScanOpRecs = 1;
  }
  c_scanOpPool.init(
    ScanOp::TYPE_ID,
    pc,
    reserveScanOpRecs,
    UINT32_MAX);
  while (c_scanOpPool.startup())
  {
    refresh_watch_dog();
  }

  c_freeScanLock = RNIL;
  Uint32 reserveScanLockRecs = 1000;
  if (m_is_query_block)
  {
    reserveScanLockRecs = 1;
  }
  c_scanLockPool.init(
    ScanLock::TYPE_ID,
    pc,
    reserveScanLockRecs,
    UINT32_MAX);
  while (c_scanLockPool.startup())
  {
    refresh_watch_dog();
  }
  const Uint32 nScanBoundWords = reserveScanOpRecs * ScanBoundSegmentSize * 4;
  c_scanBoundPool.init(
    RT_DBTUX_SCAN_BOUND,
    pc,
    nScanBoundWords,
    UINT32_MAX);
  while (c_scanBoundPool.startup())
  {
    refresh_watch_dog();
  }

  // ack
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

// utils

/**
 * This method can be called from MT-build process.
 */
void
Dbtux::readKeyAttrs(TuxCtx& ctx,
                    const Frag& frag,
                    TreeEnt ent,
                    KeyData& keyData,
                    Uint32 count)
{
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  const DescHead& descHead = getDescHead(index);
  const AttributeHeader* keyAttrs = getKeyAttrs(descHead);
  Uint32* const outputBuffer = ctx.c_dataBuffer;

#ifdef VM_TRACE
  ndbrequire(&keyData.get_spec() == &index.m_keySpec);
  ndbrequire(keyData.get_spec().validate() == 0);
  ndbrequire(count <= index.m_numAttrs);
#endif

  const TupLoc tupLoc = ent.m_tupLoc;
  const Uint32 pageId = tupLoc.getPageId();
  const Uint32 pageOffset = tupLoc.getPageOffset();
  const Uint32 tupVersion = ent.m_tupVersion;
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI;
  const Uint32* keyAttrs32 = (const Uint32*)&keyAttrs[0];

  int ret;
  ret = c_tup->tuxReadAttrs(ctx.jamBuffer,
                            tableFragPtrI,
                            pageId,
                            pageOffset,
                            tupVersion,
                            keyAttrs32,
                            count,
                            outputBuffer,
                            false);
  thrjamDebug(ctx.jamBuffer);
  ndbrequire(ret > 0);
  keyData.reset();
  Uint32 len;
  ret = keyData.add_poai(outputBuffer, count, &len);
  ndbrequire(ret == 0);
  ret = keyData.finalize();
  ndbrequire(ret == 0);

#ifdef VM_TRACE
  if (debugFlags & (DebugMaint | DebugScan)) {
    tuxDebugOut << "readKeyAttrs: ";
    tuxDebugOut << " ent:" << ent << " count:" << count;
    tuxDebugOut << " data:" << keyData.print(ctx.c_debugBuffer,
                                             DebugBufferBytes);
    tuxDebugOut << endl;
  }
#endif
}

void
Dbtux::readKeyAttrs(TuxCtx& ctx,
                    const Frag& frag,
                    TreeEnt ent,
                    Uint32 count,
                    Uint32 *outputBuffer)
{
#ifdef VM_TRACE
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  ndbrequire(count <= index.m_numAttrs);
#endif

  const TupLoc tupLoc = ent.m_tupLoc;
  const Uint32 pageId = tupLoc.getPageId();
  const Uint32 pageOffset = tupLoc.getPageOffset();
  const Uint32 tupVersion = ent.m_tupVersion;
  const Uint32* keyAttrs32 = (const Uint32*)ctx.keyAttrs;

  int ret;
  ret = c_tup->tuxReadAttrsOpt(ctx.jamBuffer,
                               ctx.tupRealFragPtr,
                               ctx.tupRealTablePtr,
                               pageId,
                               pageOffset,
                               tupVersion,
                               keyAttrs32,
                               count,
                               outputBuffer,
                               false);
  thrjamDebug(ctx.jamBuffer);
  ndbrequire(ret > 0);
}

void
Dbtux::readTablePk(TreeEnt ent, Uint32* pkData, unsigned& pkSize)
{
  const TupLoc tupLoc = ent.m_tupLoc;
  int ret = c_tup->tuxReadPk(c_ctx.tupRealFragPtr,
                             c_ctx.tupRealTablePtr,
                             tupLoc.getPageId(),
                             tupLoc.getPageOffset(),
                             pkData,
                             true);
  jamEntry();
  if (unlikely(ret <= 0))
  {
    Frag& frag = *c_ctx.fragPtr.p;
    Uint32 lkey1, lkey2;
    getTupAddr(frag, ent, lkey1, lkey2);
    g_eventLogger->info("(%u) readTablePk error tab(%u,%u) row(%u,%u)",
                        instance(),
                        frag.m_tableId,
                        frag.m_fragId,
                        lkey1,
                        lkey2);
    ndbrequire(ret > 0);
  }
  pkSize = ret;
}

void
Dbtux::unpackBound(Uint32* const outputBuffer,
                   const ScanBound& scanBound,
                   KeyBoundC& searchBound)
{
  // there is no const version of LocalDataBuffer
  ScanBoundBuffer::Head head = scanBound.m_head;
  LocalScanBoundBuffer b(c_scanBoundPool, head);
  ScanBoundBuffer::ConstDataBufferIterator iter;
  b.first(iter);
  const Uint32 n = b.getSize();
  ndbrequire(n <= MaxAttrDataSize);
  for (Uint32 i = 0; i < n; i++) {
    outputBuffer[i] = *iter.data;
    b.next(iter);
  }
  // set bound to the unpacked data buffer
  KeyDataC& searchBoundData = searchBound.get_data();
  searchBoundData.set_buf(outputBuffer,
                          MaxAttrDataSize << 2,
                          scanBound.m_cnt);
  int ret = searchBound.finalize(scanBound.m_side);
  ndbrequire(ret == 0);
}

void
Dbtux::findFrag(EmulatedJamBuffer* jamBuf, const Index& index, 
                Uint32 fragId, FragPtr& fragPtr)
{
  const Uint32 numFrags = index.m_numFrags;
  for (Uint32 i = 0; i < numFrags; i++) {
    thrjamDebug(jamBuf);
    if (index.m_fragId[i] == fragId) {
      thrjamDebug(jamBuf);
      fragPtr.i = index.m_fragPtrI[i];
      c_fragPool.getPtr(fragPtr);
      return;
    }
  }
  fragPtr.i = RNIL;
}

void
Dbtux::sendPoolShrink(const Uint32 pool_index)
{
  const bool need_send = c_transient_pools_shrinking.get(pool_index) == 0;
  c_transient_pools_shrinking.set(pool_index);
  if (need_send)
  {
    Signal25 signal[1] = {};
    signal->theData[0] = TuxContinueB::ShrinkTransientPools;
    signal->theData[1] = pool_index;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}

void
Dbtux::shrinkTransientPools(Uint32 pool_index)
{
  ndbrequire(pool_index < c_transient_pool_count);
  ndbrequire(c_transient_pools_shrinking.get(pool_index));
  if (c_transient_pools[pool_index]->rearrange_free_list_and_shrink(1))
  {
    sendPoolShrink(pool_index);
  }
  else
  {
    c_transient_pools_shrinking.clear(pool_index);
  }
}

BLOCK_FUNCTIONS(Dbtux)
