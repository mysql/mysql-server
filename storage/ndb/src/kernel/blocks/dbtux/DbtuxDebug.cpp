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

#define DBTUX_DEBUG_CPP
#include "Dbtux.hpp"

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>

#define JAM_FILE_ID 366



void Dbtux::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Index",
        c_indexPool.getUsed(),
        c_indexPool.getSize(),
        c_indexPool.getEntrySize(),
        c_indexPool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,0 },
        0},
      { "Fragment",
        c_fragPool.getUsed(),
        c_fragPool.getSize(),
        c_fragPool.getEntrySize(),
        c_fragPool.getUsedHi(),
        { CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_REPLICAS,0,0 },
        0},
      { "Descriptor page",
        c_descPagePool.getUsed(),
        c_descPagePool.getSize(),
        c_descPagePool.getEntrySize(),
        c_descPagePool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,0 },
        0},
      { "Fragment Operation",
        c_fragOpPool.getUsed(),
        c_fragOpPool.getSize(),
        c_fragOpPool.getEntrySize(),
        c_fragOpPool.getUsedHi(),
        { 0,0,0,0 },
        0},
      { "Scan Operation",
        c_scanOpPool.getUsed(),
        c_scanOpPool.getSize(),
        c_scanOpPool.getEntrySize(),
        c_scanOpPool.getUsedHi(),
        { CFG_DB_NO_LOCAL_SCANS,0,0,0 },
        0},
      { "Scan Bound",
        c_scanBoundPool.getUsed(),
        c_scanBoundPool.getSize(),
        c_scanBoundPool.getEntrySize(),
        c_scanBoundPool.getUsedHi(),
        { CFG_DB_NO_LOCAL_SCANS,0,0,0 },
        0},
      { "Scan Lock",
        c_scanLockPool.getUsed(),
        c_scanLockPool.getSize(),
        c_scanLockPool.getEntrySize(),
        c_scanLockPool.getUsedHi(),
        { CFG_DB_NO_LOCAL_SCANS,
          CFG_DB_BATCH_SIZE,0,0 },
        0},
      { NULL, 0,0,0,0,{ 0,0,0,0 },0}
    };

    const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    const Uint32 numPools = NDB_ARRAY_SIZE(pools);
    Uint32 pool = cursor->data[0];
    ndbrequire(pool < numPools);
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);
      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      row.write_uint32(GET_RG(pools[pool].record_type));
      row.write_uint32(GET_TID(pools[pool].record_type));
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

/*
 * 12001 log file 0-close 1-open 2-append 3-append to signal log
 * 12002 log flags 1-meta 2-maint 4-tree 8-scan lock-16 stat-32
 */
void
Dbtux::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
#ifdef VM_TRACE
  if (signal->theData[0] == DumpStateOrd::TuxLogToFile) {
    unsigned flag = signal->theData[1];
    const char* const tuxlog = "tux.log";
    FILE* slFile = globalSignalLoggers.getOutputStream();
    if (flag <= 3) {
      if (debugFile != 0) {
        if (debugFile != slFile)
          fclose(debugFile);
        debugFile = 0;
        tuxDebugOut = *new NdbOut(*new NullOutputStream());
      }
      if (flag == 1)
        debugFile = fopen(tuxlog, "w");
      if (flag == 2)
        debugFile = fopen(tuxlog, "a");
      if (flag == 3)
        debugFile = slFile;
      if (debugFile != 0)
        tuxDebugOut = *new NdbOut(*new FileOutputStream(debugFile));
    }
    return;
  }
  if (signal->theData[0] == DumpStateOrd::TuxSetLogFlags) {
    debugFlags = signal->theData[1];
    return;
  }
  if (signal->theData[0] == DumpStateOrd::TuxMetaDataJunk) {
    abort();
  }
#endif

  if (signal->theData[0] == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_AP_SNAPSHOT_SAVE(c_indexPool);
    RSS_AP_SNAPSHOT_SAVE(c_fragPool);
    RSS_AP_SNAPSHOT_SAVE(c_fragOpPool);
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_AP_SNAPSHOT_CHECK(c_indexPool);
    RSS_AP_SNAPSHOT_CHECK(c_fragPool);
    RSS_AP_SNAPSHOT_CHECK(c_fragOpPool);
  }
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  if (signal->theData[0] == DumpStateOrd::TuxSetTransientPoolMaxSize)
  {
    jam();
    if (signal->getLength() < 3)
      return;
    const Uint32 pool_index = signal->theData[1];
    const Uint32 new_size = signal->theData[2];
    if (pool_index >= c_transient_pool_count)
      return;
    c_transient_pools[pool_index]->setMaxSize(new_size);
    return;
  }
  if (signal->theData[0] == DumpStateOrd::TuxResetTransientPoolMaxSize)
  {
    jam();
    if(signal->getLength() < 2)
      return;
    const Uint32 pool_index = signal->theData[1];
    if (pool_index >= c_transient_pool_count)
      return;
    c_transient_pools[pool_index]->resetMaxSize();
    return;
  }
#endif
}

#ifdef VM_TRACE

void
Dbtux::printTree(Signal* signal, Frag& frag, NdbOut& out)
{
  TreeHead& tree = frag.m_tree;
  PrintPar par;
  strcpy(par.m_path, ".");
  par.m_side = 2;
  par.m_parent = NullTupLoc;
  printNode(c_ctx, frag, out, tree.m_root, par);
  out.m_out->flush();
  if (! par.m_ok) {
    if (debugFile == 0) {
      signal->theData[0] = 12001;
      signal->theData[1] = 1;
      execDUMP_STATE_ORD(signal);
      if (debugFile != 0) {
        printTree(signal, frag, tuxDebugOut);
      }
    }
    ndbabort();
  }
}

void
Dbtux::printNode(TuxCtx & ctx,
                 Frag& frag, NdbOut& out, TupLoc loc, PrintPar& par)
{
  if (loc == NullTupLoc) {
    par.m_depth = 0;
    return;
  }
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  TreeHead& tree = frag.m_tree;
  NodeHandle node(frag);
  selectNode(ctx, node, loc);
  out << par.m_path << " " << node << endl;
  // check children
  PrintPar cpar[2];
  ndbrequire(strlen(par.m_path) + 1 < sizeof(par.m_path));
  for (unsigned i = 0; i <= 1; i++) {
    sprintf(cpar[i].m_path, "%s%c", par.m_path, "LR"[i]);
    cpar[i].m_side = i;
    cpar[i].m_depth = 0;
    cpar[i].m_parent = loc;
    printNode(ctx, frag, out, node.getLink(i), cpar[i]);
    if (! cpar[i].m_ok) {
      par.m_ok = false;
    }
  }
  static const char* const sep = " *** ";
  // check child-parent links
  if (node.getLink(2) != par.m_parent) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "parent loc " << hex << node.getLink(2);
    out << " should be " << hex << par.m_parent << endl;
  }
  if (node.getSide() != par.m_side) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "side " << dec << node.getSide();
    out << " should be " << dec << par.m_side << endl;
  }
  // check balance
  const int balance = -cpar[0].m_depth + cpar[1].m_depth;
  if (node.getBalance() != balance) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "balance " << node.getBalance();
    out << " should be " << balance << endl;
  }
  if (abs(node.getBalance()) > 1) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "balance " << node.getBalance() << " is invalid" << endl;
  }
  // check occupancy
  if (node.getOccup() == 0 || node.getOccup() > tree.m_maxOccup) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "occupancy " << node.getOccup();
    out << " zero or greater than max " << tree.m_maxOccup << endl;
  }
  // check for occupancy of interior node
  if (node.getChilds() == 2 && node.getOccup() < tree.m_minOccup) {
    par.m_ok = false;
    out << par.m_path << sep;
    out << "occupancy " << node.getOccup() << " of interior node";
    out << " less than min " << tree.m_minOccup << endl;
  }
#ifdef dbtux_totally_groks_t_trees
  // check missed semi-leaf/leaf merge
  for (unsigned i = 0; i <= 1; i++) {
    if (node.getLink(i) != NullTupLoc &&
        node.getLink(1 - i) == NullTupLoc &&
        // our semi-leaf seems to satisfy interior minOccup condition
        node.getOccup() < tree.m_minOccup) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << "missed merge with child " << i << endl;
    }
  }
#endif
  // check inline prefix
  {
    KeyDataC keyData1(index.m_keySpec, false);
    const Uint32* data1 = node.getPref();
    keyData1.set_buf(data1, index.m_prefBytes, index.m_prefAttrs);
    KeyData keyData2(index.m_keySpec, false, 0);
    Uint32 data2[MaxPrefSize];
    keyData2.set_buf(data2, MaxPrefSize << 2);
    readKeyAttrs(ctx, frag, node.getEnt(0), keyData2, index.m_prefAttrs);
    if (cmpSearchKey(ctx, keyData1, keyData2, index.m_prefAttrs) != 0) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << "inline prefix mismatch" << endl;
    }
  }
  // check ordering within node
  for (unsigned j = 1; j < node.getOccup(); j++) {
    const TreeEnt ent1 = node.getEnt(j - 1);
    const TreeEnt ent2 = node.getEnt(j);
    KeyData entryKey1(index.m_keySpec, false, 0);
    KeyData entryKey2(index.m_keySpec, false, 0);
    entryKey1.set_buf(ctx.c_searchKey, MaxAttrDataSize << 2);
    entryKey2.set_buf(ctx.c_entryKey, MaxAttrDataSize << 2);
    readKeyAttrs(ctx, frag, ent1, entryKey1, index.m_numAttrs);
    readKeyAttrs(ctx, frag, ent2, entryKey2, index.m_numAttrs);
    int ret = cmpSearchKey(ctx, entryKey1, entryKey2, index.m_numAttrs);
    if (ret == 0)
      ret = ent1.cmp(ent2);
    if (! (ret < 0)) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << " disorder within node at pos " << j << endl;
    }
  }
  // check ordering wrt subtrees
  for (unsigned i = 0; i <= 1; i++) {
    if (node.getLink(i) == NullTupLoc)
      continue;
    const TreeEnt ent1 = cpar[i].m_minmax[1 - i];
    const unsigned pos = (i == 0 ? 0 : node.getOccup() - 1);
    const TreeEnt ent2 = node.getEnt(pos);
    KeyData entryKey1(index.m_keySpec, false, 0);
    KeyData entryKey2(index.m_keySpec, false, 0);
    entryKey1.set_buf(ctx.c_searchKey, MaxAttrDataSize << 2);
    entryKey2.set_buf(ctx.c_entryKey, MaxAttrDataSize << 2);
    readKeyAttrs(ctx, frag, ent1, entryKey1, index.m_numAttrs);
    readKeyAttrs(ctx, frag, ent2, entryKey2, index.m_numAttrs);
    int ret = cmpSearchKey(ctx, entryKey1, entryKey2, index.m_numAttrs);
    if (ret == 0)
      ret = ent1.cmp(ent2);
    if ((i == 0 && ! (ret < 0)) ||
        (i == 1 && ! (ret > 0))) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << " disorder wrt subtree " << i << endl;
    }
  }
  // return values
  par.m_depth = 1 + max(cpar[0].m_depth, cpar[1].m_depth);
  par.m_occup = node.getOccup();
  for (unsigned i = 0; i <= 1; i++) {
    if (node.getLink(i) == NullTupLoc) {
      const unsigned pos = (i == 0 ? 0 : node.getOccup() - 1);
      par.m_minmax[i] = node.getEnt(pos);
    } else
      par.m_minmax[i] = cpar[i].m_minmax[i];
  }
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::TupLoc& loc)
{
  if (loc == Dbtux::NullTupLoc) {
    out << "null";
  } else {
    out << dec << loc.getPageId();
    out << "." << dec << loc.getPageOffset();
  }
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::TreeEnt& ent)
{
  out << ent.m_tupLoc;
  out << "-" << dec << ent.m_tupVersion;
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::TreeNode& node)
{
  out << "[TreeNode " << hex << &node;
  out << " [left " << node.m_link[0] << "]";
  out << " [right " << node.m_link[1] << "]";
  out << " [up " << node.m_link[2] << "]";
  out << " [side " << dec << node.m_side << "]";
  out << " [occup " << dec << node.m_occup << "]";
  out << " [balance " << dec << (int)node.m_balance - 1 << "]";
  out << " [nodeScanPtrI " << hex << node.m_nodeScanPtrI << "]";
  out << " [nodeScanInstance " << hex << node.m_nodeScanInstance << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::TreeHead& tree)
{
  out << "[TreeHead " << hex << &tree;
  out << " [nodeSize " << dec << tree.m_nodeSize << "]";
  out << " [prefSize " << dec << tree.m_prefSize << "]";
  out << " [minOccup " << dec << tree.m_minOccup << "]";
  out << " [maxOccup " << dec << tree.m_maxOccup << "]";
  out << " [root " << hex << tree.m_root << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::TreePos& pos)
{
  out << "[TreePos " << hex << &pos;
  out << " [loc " << pos.m_loc << "]";
  out << " [pos " << dec << pos.m_pos << "]";
  out << " [dir " << dec << pos.m_dir << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::ScanOp& scan)
{
  Dbtux* tux = (Dbtux*)globalData.getBlock(DBTUX);
  out << "[ScanOp " << hex << &scan;
  out << " [state " << dec << scan.m_state << "]";
  out << " [lockwait " << dec << scan.m_lockwait << "]";
  out << " [errorCode " << dec << scan.m_errorCode << "]";
  out << " [indexId " << dec << scan.m_indexId << "]";
  out << " [fragId " << dec << scan.m_fragId << "]";
  out << " [transId " << hex << scan.m_transId1 << " " << scan.m_transId2 << "]";
  out << " [savePointId " << dec << scan.m_savePointId << "]";
  out << " [accLockOp " << hex << scan.m_accLockOp << "]";
  out << " [accLockOps";
  if (globalData.isNdbMtLqh)//TODO
    return out;
  {
    const Dbtux::ScanLock_fifo::Head& head = scan.m_accLockOps;
    Dbtux::ConstLocal_ScanLock_fifo list(tux->c_scanLockPool, head);
    Dbtux::ScanLockPtr lockPtr;
    list.first(lockPtr);
    while (lockPtr.i != RNIL) {
      out << " " << hex << lockPtr.p->m_accLockOp;
      list.next(lockPtr);
    }
  }
  out << "]";
  out << " [readCommitted " << dec << scan.m_readCommitted << "]";
  out << " [lockMode " << dec << scan.m_lockMode << "]";
  out << " [descending " << dec << scan.m_descending << "]";
  out << " [pos " << scan.m_scanPos << "]";
  out << " [ent " << scan.m_scanEnt << "]";
  for (unsigned i = 0; i <= 1; i++) {
    const Dbtux::ScanBound scanBound = scan.m_scanBound[i];
    const Dbtux::Index& index = *tux->c_indexPool.getPtr(scan.m_indexId);
    Dbtux::KeyDataC keyBoundData(index.m_keySpec, true);
    Dbtux::KeyBoundC keyBound(keyBoundData);
    tux->unpackBound(tux->c_ctx.c_searchKey, scanBound, keyBound);
    out << " [scanBound " << dec << i;
    out << " " << keyBound.print(tux->c_ctx.c_debugBuffer, Dbtux::DebugBufferBytes);
    out << "]";
  }
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::Index& index)
{
  Dbtux* tux = (Dbtux*)globalData.getBlock(DBTUX);
  out << "[Index " << hex << &index;
  out << " [tableId " << dec << index.m_tableId << "]";
  out << " [numFrags " << dec << index.m_numFrags << "]";
  if (globalData.isNdbMtLqh)//TODO
    return out;
  for (unsigned i = 0; i < index.m_numFrags; i++) {
    out << " [frag " << dec << i << " ";
    const Dbtux::Frag& frag = *tux->c_fragPool.getPtr(index.m_fragPtrI[i]);
    out << frag;
    out << "]";
  }
  out << " [descPage " << hex << index.m_descPage << "]";
  out << " [descOff " << dec << index.m_descOff << "]";
  out << " [numAttrs " << dec << index.m_numAttrs << "]";
  out << " [prefAttrs " << dec << index.m_prefAttrs << "]";
  out << " [prefBytes " << dec << index.m_prefBytes << "]";
  out << " [statFragPtrI " << hex << index.m_statFragPtrI << "]";
  out << " [statLoadTime " << dec << index.m_statLoadTime << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::Frag& frag)
{
  out << "[Frag " << hex << &frag;
  out << " [tableId " << dec << frag.m_tableId << "]";
  out << " [indexId " << dec << frag.m_indexId << "]";
  out << " [fragId " << dec << frag.m_fragId << "]";
  out << " [entryCount " << dec << frag.m_entryCount << "]";
  out << " [entryBytes " << dec << frag.m_entryBytes << "]";
  out << " [entryOps " << dec << frag.m_entryOps << "]";
  out << " [tree " << frag.m_tree << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::FragOp& fragOp)
{
  out << "[FragOp " << hex << &fragOp;
  out << " [userPtr " << dec << fragOp.m_userPtr << "]";
  out << " [indexId " << dec << fragOp.m_indexId << "]";
  out << " [fragId " << dec << fragOp.m_fragId << "]";
  out << " [fragNo " << dec << fragOp.m_fragNo << "]";
  out << " numAttrsRecvd " << dec << fragOp.m_numAttrsRecvd << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::NodeHandle& node)
{
  const Dbtux::Frag& frag = node.m_frag;
  const Dbtux::TreeHead& tree = frag.m_tree;
  out << "[NodeHandle " << hex << &node;
  out << " [loc " << node.m_loc << "]";
  out << " [node " << *node.m_node << "]";
  const Uint32* data;
  out << " [pref";
  data = (const Uint32*)node.m_node + Dbtux::NodeHeadSize;
  for (unsigned j = 0; j < tree.m_prefSize; j++)
    out << " " << hex << data[j];
  out << "]";
  out << " [entList";
  unsigned numpos = node.m_node->m_occup;
  data = (const Uint32*)node.m_node + Dbtux::NodeHeadSize + tree.m_prefSize;
  const Dbtux::TreeEnt* entList = (const Dbtux::TreeEnt*)data;
  for (unsigned pos = 0; pos < numpos; pos++)
    out << " " << entList[pos];
  out << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::StatOp& stat)
{
  out << "[StatOp " << hex << &stat;
  out << " [saveSize " << dec << stat.m_saveSize << "]";
  out << " [saveScale " << dec << stat.m_saveScale << "]";
  out << " [batchSize " << dec << stat.m_batchSize << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::StatMon& mon)
{
  out << "[StatMon";
  out << " [loopIndexId " << dec << mon.m_loopIndexId << "]";
  out << "]";
  return out;
}

#endif
