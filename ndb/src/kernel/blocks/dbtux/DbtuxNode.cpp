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

#define DBTUX_NODE_CPP
#include "Dbtux.hpp"

/*
 * Node handles.
 *
 * We use the "cache" implementation.  Node operations are done on
 * cached copies.  Index memory is updated at the end of the operation.
 * At most one node is inserted and it is always pre-allocated.
 *
 * An alternative "pointer" implementation which writes directly into
 * index memory is planned for later.
 */

// Dbtux

void
Dbtux::seizeNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr)
{
  if (! c_nodeHandlePool.seize(nodePtr)) {
    jam();
    return;
  }
  new (nodePtr.p) NodeHandle(*this, frag);
  nodePtr.p->m_next = frag.m_nodeList;
  frag.m_nodeList = nodePtr.i;
  // node cache used always
  nodePtr.p->m_node = (TreeNode*)nodePtr.p->m_cache;
  new (nodePtr.p->m_node) TreeNode();
#ifdef VM_TRACE
  TreeHead& tree = frag.m_tree;
  TreeNode* node = nodePtr.p->m_node;
  memset(tree.getPref(node, 0), 0xa2, tree.m_prefSize << 2);
  memset(tree.getPref(node, 1), 0xa2, tree.m_prefSize << 2);
  TreeEnt* entList = tree.getEntList(node);
  memset(entList, 0xa4, (tree.m_maxOccup + 1) * (TreeEntSize << 2));
#endif
}

void
Dbtux::preallocNode(Signal* signal, Frag& frag, Uint32& errorCode)
{
  ndbrequire(frag.m_nodeFree == RNIL);
  NodeHandlePtr nodePtr;
  seizeNode(signal, frag, nodePtr);
  ndbrequire(nodePtr.i != RNIL);
  // remove from cache  XXX ugly
  frag.m_nodeFree = frag.m_nodeList;
  frag.m_nodeList = nodePtr.p->m_next;
  StorePar storePar;
  storePar.m_opCode = TupStoreTh::OpInsert;
  storePar.m_offset = 0;
  storePar.m_size = 0;
  tupStoreTh(signal, frag, nodePtr, storePar);
  if (storePar.m_errorCode != 0) {
    jam();
    errorCode = storePar.m_errorCode;
    c_nodeHandlePool.release(nodePtr);
    frag.m_nodeFree = RNIL;
  }
}

/*
 * Find node in the cache.  XXX too slow, use direct links instead
 */
void
Dbtux::findNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, TupAddr addr)
{
  NodeHandlePtr tmpPtr;
  tmpPtr.i = frag.m_nodeList;
  while (tmpPtr.i != RNIL) {
    jam();
    c_nodeHandlePool.getPtr(tmpPtr);
    if (tmpPtr.p->m_addr == addr) {
      jam();
      nodePtr = tmpPtr;
      return;
    }
    tmpPtr.i = tmpPtr.p->m_next;
  }
  nodePtr.i = RNIL;
  nodePtr.p = 0;
}

/*
 * Get handle for existing node.
 */
void
Dbtux::selectNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, TupAddr addr, AccSize acc)
{
  ndbrequire(addr != NullTupAddr && acc > AccNone);
  NodeHandlePtr tmpPtr;
  // search in cache
  findNode(signal, frag, tmpPtr, addr);
  if (tmpPtr.i == RNIL) {
    jam();
    // add new node
    seizeNode(signal, frag, tmpPtr);
    ndbrequire(tmpPtr.i != RNIL);
    tmpPtr.p->m_addr = addr;
  }
  if (tmpPtr.p->m_acc < acc) {
    jam();
    accessNode(signal, frag, tmpPtr, acc);
  }
  nodePtr = tmpPtr;
}

/*
 * Create new node in the cache and mark it for insert.
 */
void
Dbtux::insertNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, AccSize acc)
{
  ndbrequire(acc > AccNone);
  NodeHandlePtr tmpPtr;
  // use the pre-allocated node
  tmpPtr.i = frag.m_nodeFree;
  frag.m_nodeFree = RNIL;
  c_nodeHandlePool.getPtr(tmpPtr);
  // move it to the cache
  tmpPtr.p->m_next = frag.m_nodeList;
  frag.m_nodeList = tmpPtr.i;
  tmpPtr.p->m_acc = acc;
  tmpPtr.p->m_flags |= NodeHandle::DoInsert;
  nodePtr = tmpPtr;
}

/*
 * Mark existing node for deletion.
 */
void
Dbtux::deleteNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr)
{
  NodeHandlePtr tmpPtr = nodePtr;
  ndbrequire(tmpPtr.p->getOccup() == 0);
  tmpPtr.p->m_flags |= NodeHandle::DoDelete;
  // scans have already been moved by popDown or popUp
}

/*
 * Access more of the node.
 */
void
Dbtux::accessNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, AccSize acc)
{
  TreeHead& tree = frag.m_tree;
  NodeHandlePtr tmpPtr = nodePtr;
  if (tmpPtr.p->m_acc >= acc)
    return;
  if (! (tmpPtr.p->m_flags & NodeHandle::DoInsert)) {
    jam();
    StorePar storePar;
    storePar.m_opCode = TupStoreTh::OpRead;
    storePar.m_offset = tree.getSize(tmpPtr.p->m_acc);
    storePar.m_size = tree.getSize(acc) - tree.getSize(tmpPtr.p->m_acc);
    tmpPtr.p->m_tux.tupStoreTh(signal, frag, tmpPtr, storePar);
    ndbrequire(storePar.m_errorCode == 0);
  }
  tmpPtr.p->m_acc = acc;
}

/*
 * Set prefix.
 */
void
Dbtux::setNodePref(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, unsigned i)
{
  TreeHead& tree = frag.m_tree;
  NodeHandlePtr tmpPtr = nodePtr;
  ReadPar readPar;
  ndbrequire(i <= 1);
  readPar.m_ent = tmpPtr.p->getMinMax(i);
  readPar.m_first = 0;
  readPar.m_count = frag.m_numAttrs;
  // leave in signal data
  readPar.m_data = 0;
  // XXX implement max words to read
  tupReadAttrs(signal, frag, readPar);
  // copy whatever fits
  CopyPar copyPar;
  copyPar.m_items = readPar.m_count;
  copyPar.m_headers = true;
  copyPar.m_maxwords = tree.m_prefSize;
  Data pref = tmpPtr.p->getPref(i);
  copyAttrs(pref, readPar.m_data, copyPar);
  nodePtr.p->m_flags |= NodeHandle::DoUpdate;
}

/*
 * Commit and release nodes at the end of an operation.  Used also on
 * error since no changes have been made (updateOk false).
 */
void
Dbtux::commitNodes(Signal* signal, Frag& frag, bool updateOk)
{
  TreeHead& tree = frag.m_tree;
  NodeHandlePtr nodePtr;
  nodePtr.i = frag.m_nodeList;
  frag.m_nodeList = RNIL;
  while (nodePtr.i != RNIL) {
    c_nodeHandlePool.getPtr(nodePtr);
    const unsigned flags = nodePtr.p->m_flags;
    if (flags & NodeHandle::DoDelete) {
      jam();
      ndbrequire(updateOk);
      // delete
      StorePar storePar;
      storePar.m_opCode = TupStoreTh::OpDelete;
      nodePtr.p->m_tux.tupStoreTh(signal, frag, nodePtr, storePar);
      ndbrequire(storePar.m_errorCode == 0);
    } else if (flags & NodeHandle::DoUpdate) {
      jam();
      ndbrequire(updateOk);
      // set prefixes
      if (flags & (1 << 0)) {
        jam();
        setNodePref(signal, frag, nodePtr, 0);
      }
      if (flags & (1 << 1)) {
        jam();
        setNodePref(signal, frag, nodePtr, 1);
      }
      // update
      StorePar storePar;
      storePar.m_opCode = TupStoreTh::OpUpdate;
      storePar.m_offset = 0;
      storePar.m_size = tree.getSize(nodePtr.p->m_acc);
      nodePtr.p->m_tux.tupStoreTh(signal, frag, nodePtr, storePar);
      ndbrequire(storePar.m_errorCode == 0);
    }
    // release
    NodeHandlePtr tmpPtr = nodePtr;
    nodePtr.i = nodePtr.p->m_next;
    c_nodeHandlePool.release(tmpPtr);
  }
}

// Dbtux::NodeHandle

/*
 * Add entry at position.  Move entries greater than or equal to the old
 * one (if any) to the right.
 *
 *            X
 *            v
 *      A B C D E _ _  =>  A B C X D E _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::NodeHandle::pushUp(Signal* signal, unsigned pos, const TreeEnt& ent)
{
  TreeHead& tree = m_frag.m_tree;
  const unsigned occup = getOccup();
  ndbrequire(occup < tree.m_maxOccup && pos <= occup);
  // fix scans
  ScanOpPtr scanPtr;
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    if (scanPos.m_pos >= pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At pushUp pos=" << pos << " " << *this << endl;
      }
#endif
      scanPos.m_pos++;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  for (unsigned i = occup; i > pos; i--) {
    jam();
    tmpList[i] = tmpList[i - 1];
  }
  tmpList[pos] = ent;
  if (occup == 0 || pos == 0)
    m_flags |= (1 << 0);
  if (occup == 0 || pos == occup)
    m_flags |= (1 << 1);
  entList[0] = entList[occup + 1];
  setOccup(occup + 1);
  m_flags |= DoUpdate;
}

/*
 * Remove and return entry at position.  Move entries greater than the
 * removed one to the left.  This is the opposite of pushUp.
 *
 *                               D
 *            ^                  ^
 *      A B C D E F _  =>  A B C E F _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::NodeHandle::popDown(Signal* signal, unsigned pos, TreeEnt& ent)
{
  TreeHead& tree = m_frag.m_tree;
  const unsigned occup = getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At popDown pos=" << pos << " " << *this << endl;
      }
#endif
      m_tux.scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != pos);
    if (scanPos.m_pos > pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At popDown pos=" << pos << " " << *this << endl;
      }
#endif
      scanPos.m_pos--;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  ent = tmpList[pos];
  for (unsigned i = pos; i < occup - 1; i++) {
    jam();
    tmpList[i] = tmpList[i + 1];
  }
  if (occup != 1 && pos == 0)
    m_flags |= (1 << 0);
  if (occup != 1 && pos == occup - 1)
    m_flags |= (1 << 1);
  entList[0] = entList[occup - 1];
  setOccup(occup - 1);
  m_flags |= DoUpdate;
}

/*
 * Add entry at existing position.  Move entries less than or equal to
 * the old one to the left.  Remove and return old min entry.
 *
 *            X            A
 *      ^     v            ^
 *      A B C D E _ _  =>  B C D X E _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::NodeHandle::pushDown(Signal* signal, unsigned pos, TreeEnt& ent)
{
  TreeHead& tree = m_frag.m_tree;
  const unsigned occup = getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == 0) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At pushDown pos=" << pos << " " << *this << endl;
      }
#endif
      // here we may miss a valid entry "X"  XXX known bug
      m_tux.scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != 0);
    if (scanPos.m_pos <= pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At pushDown pos=" << pos << " " << *this << endl;
      }
#endif
      scanPos.m_pos--;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  TreeEnt oldMin = tmpList[0];
  for (unsigned i = 0; i < pos; i++) {
    jam();
    tmpList[i] = tmpList[i + 1];
  }
  tmpList[pos] = ent;
  ent = oldMin;
  if (true)
    m_flags |= (1 << 0);
  if (occup == 1 || pos == occup - 1)
    m_flags |= (1 << 1);
  entList[0] = entList[occup];
  m_flags |= DoUpdate;
}

/*
 * Remove and return entry at position.  Move entries less than the
 * removed one to the right.  Replace min entry by the input entry.
 * This is the opposite of pushDown.
 *
 *      X                        D
 *      v     ^                  ^
 *      A B C D E _ _  =>  X A B C E _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::NodeHandle::popUp(Signal* signal, unsigned pos, TreeEnt& ent)
{
  TreeHead& tree = m_frag.m_tree;
  const unsigned occup = getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At popUp pos=" << pos << " " << *this << endl;
      }
#endif
      // here we may miss a valid entry "X"  XXX known bug
      m_tux.scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_addr == m_addr && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != pos);
    if (scanPos.m_pos < pos) {
      jam();
#ifdef VM_TRACE
      if (m_tux.debugFlags & m_tux.DebugScan) {
        m_tux.debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        m_tux.debugOut << "At popUp pos=" << pos << " " << *this << endl;
      }
#endif
      scanPos.m_pos++;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  TreeEnt newMin = ent;
  ent = tmpList[pos];
  for (unsigned i = pos; i > 0; i--) {
    jam();
    tmpList[i] = tmpList[i - 1];
  }
  tmpList[0] = newMin;
  if (true)
    m_flags |= (1 << 0);
  if (occup == 1 || pos == occup - 1)
    m_flags |= (1 << 1);
  entList[0] = entList[occup];
  m_flags |= DoUpdate;
}

/*
 * Move all possible entries from another node before the min (i=0) or
 * after the max (i=1).  XXX can be optimized
 */
void
Dbtux::NodeHandle::slide(Signal* signal, NodeHandlePtr nodePtr, unsigned i)
{
  ndbrequire(i <= 1);
  TreeHead& tree = m_frag.m_tree;
  while (getOccup() < tree.m_maxOccup && nodePtr.p->getOccup() != 0) {
    TreeEnt ent;
    nodePtr.p->popDown(signal, i == 0 ? nodePtr.p->getOccup() - 1 : 0, ent);
    pushUp(signal, i == 0 ? 0 : getOccup(), ent);
  }
}

/*
 * Link scan to the list under the node.  The list is single-linked and
 * ordering does not matter.
 */
void
Dbtux::NodeHandle::linkScan(Dbtux::ScanOpPtr scanPtr)
{
#ifdef VM_TRACE
  if (m_tux.debugFlags & m_tux.DebugScan) {
    m_tux.debugOut << "Link scan " << scanPtr.i << " " << *scanPtr.p << endl;
    m_tux.debugOut << "To node " << *this << endl;
  }
#endif
  ndbrequire(! islinkScan(scanPtr) && scanPtr.p->m_nodeScan == RNIL);
  scanPtr.p->m_nodeScan = getNodeScan();
  setNodeScan(scanPtr.i);
}

/*
 * Unlink a scan from the list under the node.
 */
void
Dbtux::NodeHandle::unlinkScan(Dbtux::ScanOpPtr scanPtr)
{
#ifdef VM_TRACE
  if (m_tux.debugFlags & m_tux.DebugScan) {
    m_tux.debugOut << "Unlink scan " << scanPtr.i << " " << *scanPtr.p << endl;
    m_tux.debugOut << "From node " << *this << endl;
  }
#endif
  Dbtux::ScanOpPtr currPtr;
  currPtr.i = getNodeScan();
  Dbtux::ScanOpPtr prevPtr;
  prevPtr.i = RNIL;
  while (true) {
    jam();
    m_tux.c_scanOpPool.getPtr(currPtr);
    Uint32 nextPtrI = currPtr.p->m_nodeScan;
    if (currPtr.i == scanPtr.i) {
      jam();
      if (prevPtr.i == RNIL) {
        setNodeScan(nextPtrI);
      } else {
        jam();
        prevPtr.p->m_nodeScan = nextPtrI;
      }
      scanPtr.p->m_nodeScan = RNIL;
      // check for duplicates
      ndbrequire(! islinkScan(scanPtr));
      return;
    }
    prevPtr = currPtr;
    currPtr.i = nextPtrI;
  }
}

/*
 * Check if a scan is linked to this node.  Only for ndbrequire.
 */
bool
Dbtux::NodeHandle::islinkScan(Dbtux::ScanOpPtr scanPtr)
{
  Dbtux::ScanOpPtr currPtr;
  currPtr.i = getNodeScan();
  while (currPtr.i != RNIL) {
    jam();
    m_tux.c_scanOpPool.getPtr(currPtr);
    if (currPtr.i == scanPtr.i) {
      jam();
      return true;
    }
    currPtr.i = currPtr.p->m_nodeScan;
  }
  return false;
}

void
Dbtux::NodeHandle::progError(int line, int cause, const char* extra)
{
  m_tux.progError(line, cause, extra);
}
