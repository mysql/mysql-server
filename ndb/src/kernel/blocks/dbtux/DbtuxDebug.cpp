/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUX_DEBUG_CPP
#include "Dbtux.hpp"

/*
 * 12001 log file 0-close 1-open 2-append 3-append to signal log
 * 12002 log flags 1-meta 2-maint 4-tree 8-scan
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
        debugOut = *new NdbOut(*new NullOutputStream());
      }
      if (flag == 1)
        debugFile = fopen(tuxlog, "w");
      if (flag == 2)
        debugFile = fopen(tuxlog, "a");
      if (flag == 3)
        debugFile = slFile;
      if (debugFile != 0)
        debugOut = *new NdbOut(*new FileOutputStream(debugFile));
    }
    return;
  }
  if (signal->theData[0] == DumpStateOrd::TuxSetLogFlags) {
    debugFlags = signal->theData[1];
    return;
  }
  if (signal->theData[0] == DumpStateOrd::TuxMetaDataJunk) {
    // read table definition
    Uint32 tableId = signal->theData[1];
    Uint32 tableVersion = signal->theData[2];
    int ret;
    MetaData md(this);
    MetaData::Table table;
    MetaData::Attribute attribute;
    infoEvent("md: read table %u %u", tableId, tableVersion);
    if ((ret = md.lock(false)) < 0) {
      infoEvent("md.lock error %d", ret);
      return;
    }
    if ((ret = md.getTable(table, tableId, tableVersion)) < 0) {
      infoEvent("md.getTable error %d", ret);
      // lock is released by destructor
      return;
    }
    infoEvent("md: %s type=%d attrs=%u", table.tableName, table.tableType, table.noOfAttributes);
    for (Uint32 i = 0; i < table.noOfAttributes; i++) {
      if ((ret = md.getAttribute(attribute, table, i)) < 0) {
        infoEvent("mg.getAttribute %u error %d", i, ret);
        // lock is released by destructor
        return;
      }
      infoEvent("md: %d %s", attribute.attributeId, attribute.attributeName);
    }
    if ((ret = md.unlock(false)) < 0) {
      infoEvent("md.unlock error %d", ret);
      return;
    }
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
  printNode(frag, out, tree.m_root, par);
  out.m_out->flush();
  if (! par.m_ok) {
    if (debugFile == 0) {
      signal->theData[0] = 12001;
      signal->theData[1] = 1;
      execDUMP_STATE_ORD(signal);
      if (debugFile != 0) {
        printTree(signal, frag, debugOut);
      }
    }
    ndbrequire(false);
  }
}

void
Dbtux::printNode(Frag& frag, NdbOut& out, TupLoc loc, PrintPar& par)
{
  if (loc == NullTupLoc) {
    par.m_depth = 0;
    return;
  }
  TreeHead& tree = frag.m_tree;
  NodeHandle node(frag);
  selectNode(node, loc);
  out << par.m_path << " " << node << endl;
  // check children
  PrintPar cpar[2];
  ndbrequire(strlen(par.m_path) + 1 < sizeof(par.m_path));
  for (unsigned i = 0; i <= 1; i++) {
    sprintf(cpar[i].m_path, "%s%c", par.m_path, "LR"[i]);
    cpar[i].m_side = i;
    cpar[i].m_depth = 0;
    cpar[i].m_parent = loc;
    printNode(frag, out, node.getLink(i), cpar[i]);
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
        // our semi-leaf seems to satify interior minOccup condition
        node.getOccup() < tree.m_minOccup) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << "missed merge with child " << i << endl;
    }
  }
#endif
  // check inline prefix
  { ConstData data1 = node.getPref();
    Uint32 data2[MaxPrefSize];
    memset(data2, DataFillByte, MaxPrefSize << 2);
    readKeyAttrs(frag, node.getMinMax(0), 0, c_searchKey);
    copyAttrs(frag, c_searchKey, data2, tree.m_prefSize);
    for (unsigned n = 0; n < tree.m_prefSize; n++) {
      if (data1[n] != data2[n]) {
        par.m_ok = false;
        out << par.m_path << sep;
        out << "inline prefix mismatch word " << n;
        out << " value " << hex << data1[n];
        out << " should be " << hex << data2[n] << endl;
        break;
      }
    }
  }
  // check ordering within node
  for (unsigned j = 1; j < node.getOccup(); j++) {
    const TreeEnt ent1 = node.getEnt(j - 1);
    const TreeEnt ent2 = node.getEnt(j);
    unsigned start = 0;
    readKeyAttrs(frag, ent1, start, c_searchKey);
    readKeyAttrs(frag, ent2, start, c_entryKey);
    int ret = cmpSearchKey(frag, start, c_searchKey, c_entryKey);
    if (ret == 0)
      ret = ent1.cmp(ent2);
    if (ret != -1) {
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
    const TreeEnt ent2 = node.getMinMax(i);
    unsigned start = 0;
    readKeyAttrs(frag, ent1, start, c_searchKey);
    readKeyAttrs(frag, ent2, start, c_entryKey);
    int ret = cmpSearchKey(frag, start, c_searchKey, c_entryKey);
    if (ret == 0)
      ret = ent1.cmp(ent2);
    if (ret != (i == 0 ? -1 : +1)) {
      par.m_ok = false;
      out << par.m_path << sep;
      out << " disorder wrt subtree " << i << endl;
    }
  }
  // return values
  par.m_depth = 1 + max(cpar[0].m_depth, cpar[1].m_depth);
  par.m_occup = node.getOccup();
  for (unsigned i = 0; i <= 1; i++) {
    if (node.getLink(i) == NullTupLoc)
      par.m_minmax[i] = node.getMinMax(i);
    else
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
  out << dec << ent.m_fragBit;
  out << "-" << ent.m_tupLoc;
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
  out << " [nodeScan " << hex << node.m_nodeScan << "]";
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
  out << " [AccHead " << dec << tree.getSize(Dbtux::AccHead) << "]";
  out << " [AccPref " << dec << tree.getSize(Dbtux::AccPref) << "]";
  out << " [AccFull " << dec << tree.getSize(Dbtux::AccFull) << "]";
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
  out << " [match " << dec << pos.m_match << "]";
  out << " [dir " << dec << pos.m_dir << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::DescAttr& descAttr)
{
  out << "[DescAttr " << hex << &descAttr;
  out << " [attrDesc " << hex << descAttr.m_attrDesc;
  out << " [primaryAttrId " << dec << descAttr.m_primaryAttrId << "]";
  out << " [typeId " << dec << descAttr.m_typeId << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::ScanOp& scan)
{
  out << "[ScanOp " << hex << &scan;
  out << " [state " << dec << scan.m_state << "]";
  out << " [lockwait " << dec << scan.m_lockwait << "]";
  out << " [indexId " << dec << scan.m_indexId << "]";
  out << " [fragId " << dec << scan.m_fragId << "]";
  out << " [transId " << hex << scan.m_transId1 << " " << scan.m_transId2 << "]";
  out << " [savePointId " << dec << scan.m_savePointId << "]";
  out << " [accLockOp " << hex << scan.m_accLockOp << "]";
  out << " [accLockOps";
  for (unsigned i = 0; i < Dbtux::MaxAccLockOps; i++) {
    if (scan.m_accLockOps[i] != RNIL)
      out << " " << hex << scan.m_accLockOps[i];
  }
  out << "]";
  out << " [readCommitted " << dec << scan.m_readCommitted << "]";
  out << " [lockMode " << dec << scan.m_lockMode << "]";
  out << " [keyInfo " << dec << scan.m_keyInfo << "]";
  out << " [pos " << scan.m_scanPos << "]";
  out << " [ent " << scan.m_scanEnt << "]";
  for (unsigned i = 0; i <= 1; i++) {
    out << " [bound " << dec << i;
    Dbtux::ScanBound& bound = *scan.m_bound[i];
    Dbtux::ScanBoundIterator iter;
    bound.first(iter);
    for (unsigned j = 0; j < bound.getSize(); j++) {
      out << " " << hex << *iter.data;
      bound.next(iter);
    }
    out << "]";
  }
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::Index& index)
{
  out << "[Index " << hex << &index;
  out << " [tableId " << dec << index.m_tableId << "]";
  out << " [fragOff " << dec << index.m_fragOff << "]";
  out << " [numFrags " << dec << index.m_numFrags << "]";
  for (unsigned i = 0; i < index.m_numFrags; i++) {
    out << " [frag " << dec << i << " ";
    // dangerous and wrong
    Dbtux* tux = (Dbtux*)globalData.getBlock(DBTUX);
    const Dbtux::Frag& frag = *tux->c_fragPool.getPtr(index.m_fragPtrI[i]);
    out << frag;
    out << "]";
  }
  out << " [descPage " << hex << index.m_descPage << "]";
  out << " [descOff " << dec << index.m_descOff << "]";
  out << " [numAttrs " << dec << index.m_numAttrs << "]";
  out << "]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtux::Frag& frag)
{
  out << "[Frag " << hex << &frag;
  out << " [tableId " << dec << frag.m_tableId << "]";
  out << " [indexId " << dec << frag.m_indexId << "]";
  out << " [fragOff " << dec << frag.m_fragOff << "]";
  out << " [fragId " << dec << frag.m_fragId << "]";
  out << " [descPage " << hex << frag.m_descPage << "]";
  out << " [descOff " << dec << frag.m_descOff << "]";
  out << " [numAttrs " << dec << frag.m_numAttrs << "]";
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
  // print entries in logical order
  for (unsigned pos = 1; pos <= numpos; pos++)
    out << " " << entList[pos % numpos];
  out << "]";
  out << "]";
  return out;
}

#endif
