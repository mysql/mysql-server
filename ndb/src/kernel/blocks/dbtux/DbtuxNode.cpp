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
 * Temporary version between "cache" and "pointer" implementations.
 */

// Dbtux

void
Dbtux::seizeNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr)
{
  if (! c_nodeHandlePool.seize(nodePtr)) {
    jam();
    return;
  }
  new (nodePtr.p) NodeHandle(frag);
  nodePtr.p->m_next = frag.m_nodeList;
  frag.m_nodeList = nodePtr.i;
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
  // alloc index node in TUP
  Uint32 pageId = NullTupLoc.m_pageId;
  Uint32 pageOffset = NullTupLoc.m_pageOffset;
  Uint32* node32 = 0;
  errorCode = c_tup->tuxAllocNode(signal, frag.m_tupIndexFragPtrI, pageId, pageOffset, node32);
  if (errorCode != 0) {
    jam();
    c_nodeHandlePool.release(nodePtr);
    frag.m_nodeFree = RNIL;
    return;
  }
  nodePtr.p->m_loc = TupLoc(pageId, pageOffset);
  nodePtr.p->m_node = reinterpret_cast<TreeNode*>(node32);
  ndbrequire(nodePtr.p->m_loc != NullTupLoc && nodePtr.p->m_node != 0);
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

/*
 * Find node in the cache.  XXX too slow, use direct links instead
 */
void
Dbtux::findNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, TupLoc loc)
{
  NodeHandlePtr tmpPtr;
  tmpPtr.i = frag.m_nodeList;
  while (tmpPtr.i != RNIL) {
    jam();
    c_nodeHandlePool.getPtr(tmpPtr);
    if (tmpPtr.p->m_loc == loc) {
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
Dbtux::selectNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, TupLoc loc, AccSize acc)
{
  ndbrequire(loc != NullTupLoc && acc > AccNone);
  NodeHandlePtr tmpPtr;
  // search in cache
  findNode(signal, frag, tmpPtr, loc);
  if (tmpPtr.i == RNIL) {
    jam();
    // add new node
    seizeNode(signal, frag, tmpPtr);
    ndbrequire(tmpPtr.i != RNIL);
    tmpPtr.p->m_loc = loc;
    Uint32 pageId = loc.m_pageId;
    Uint32 pageOffset = loc.m_pageOffset;
    Uint32* node32 = 0;
    c_tup->tuxGetNode(frag.m_tupIndexFragPtrI, pageId, pageOffset, node32);
    tmpPtr.p->m_node = reinterpret_cast<TreeNode*>(node32);
    ndbrequire(tmpPtr.p->m_loc != NullTupLoc && tmpPtr.p->m_node != 0);
  }
  if (tmpPtr.p->m_acc < acc) {
    jam();
    accessNode(signal, frag, tmpPtr, acc);
  }
  nodePtr = tmpPtr;
}

/*
 * Create new node in the cache using the pre-allocated node.
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
  nodePtr = tmpPtr;
}

/*
 * Delete existing node.
 */
void
Dbtux::deleteNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr)
{
  NodeHandlePtr tmpPtr = nodePtr;
  ndbrequire(tmpPtr.p->getOccup() == 0);
  Uint32 pageId = tmpPtr.p->m_loc.m_pageId;
  Uint32 pageOffset = tmpPtr.p->m_loc.m_pageOffset;
  Uint32* node32 = reinterpret_cast<Uint32*>(tmpPtr.p->m_node);
  c_tup->tuxFreeNode(signal, frag.m_tupIndexFragPtrI, pageId, pageOffset, node32);
  // invalidate handle and storage
  tmpPtr.p->m_loc = NullTupLoc;
  tmpPtr.p->m_node = 0;
  // scans have already been moved by nodePopDown or nodePopUp
}

/*
 * Access more of the node.
 */
void
Dbtux::accessNode(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, AccSize acc)
{
  NodeHandlePtr tmpPtr = nodePtr;
  ndbrequire(tmpPtr.p->m_loc != NullTupLoc && tmpPtr.p->m_node != 0);
  if (tmpPtr.p->m_acc >= acc)
    return;
  // XXX could do prefetch
  tmpPtr.p->m_acc = acc;
}

/*
 * Set prefix.
 */
void
Dbtux::setNodePref(Signal* signal, NodeHandle& node, unsigned i)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  ReadPar readPar;
  ndbrequire(i <= 1);
  readPar.m_ent = node.getMinMax(i);
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
  Data pref = node.getPref(i);
  copyAttrs(pref, readPar.m_data, copyPar);
}

/*
 * Commit and release nodes at the end of an operation.  Used also on
 * error since no changes have been made (updateOk false).
 */
void
Dbtux::commitNodes(Signal* signal, Frag& frag, bool updateOk)
{
  NodeHandlePtr nodePtr;
  nodePtr.i = frag.m_nodeList;
  frag.m_nodeList = RNIL;
  while (nodePtr.i != RNIL) {
    c_nodeHandlePool.getPtr(nodePtr);
    // release
    NodeHandlePtr tmpPtr = nodePtr;
    nodePtr.i = nodePtr.p->m_next;
    c_nodeHandlePool.release(tmpPtr);
  }
}

// node operations

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
Dbtux::nodePushUp(Signal* signal, NodeHandle& node, unsigned pos, const TreeEnt& ent)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup < tree.m_maxOccup && pos <= occup);
  // fix scans
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    if (scanPos.m_pos >= pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At pushUp pos=" << pos << " " << node << endl;
      }
#endif
      scanPos.m_pos++;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  for (unsigned i = occup; i > pos; i--) {
    jam();
    tmpList[i] = tmpList[i - 1];
  }
  tmpList[pos] = ent;
  entList[0] = entList[occup + 1];
  node.setOccup(occup + 1);
  // fix prefixes
  if (occup == 0 || pos == 0)
    setNodePref(signal, node, 0);
  if (occup == 0 || pos == occup)
    setNodePref(signal, node, 1);
}

/*
 * Remove and return entry at position.  Move entries greater than the
 * removed one to the left.  This is the opposite of nodePushUp.
 *
 *                               D
 *            ^                  ^
 *      A B C D E F _  =>  A B C E F _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::nodePopDown(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At popDown pos=" << pos << " " << node << endl;
      }
#endif
      scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != pos);
    if (scanPos.m_pos > pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At popDown pos=" << pos << " " << node << endl;
      }
#endif
      scanPos.m_pos--;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  ent = tmpList[pos];
  for (unsigned i = pos; i < occup - 1; i++) {
    jam();
    tmpList[i] = tmpList[i + 1];
  }
  entList[0] = entList[occup - 1];
  node.setOccup(occup - 1);
  // fix prefixes
  if (occup != 1 && pos == 0)
    setNodePref(signal, node, 0);
  if (occup != 1 && pos == occup - 1)
    setNodePref(signal, node, 1);
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
Dbtux::nodePushDown(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == 0) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At pushDown pos=" << pos << " " << node << endl;
      }
#endif
      // here we may miss a valid entry "X"  XXX known bug
      scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != 0);
    if (scanPos.m_pos <= pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At pushDown pos=" << pos << " " << node << endl;
      }
#endif
      scanPos.m_pos--;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  TreeEnt oldMin = tmpList[0];
  for (unsigned i = 0; i < pos; i++) {
    jam();
    tmpList[i] = tmpList[i + 1];
  }
  tmpList[pos] = ent;
  ent = oldMin;
  entList[0] = entList[occup];
  // fix prefixes
  if (true)
    setNodePref(signal, node, 0);
  if (occup == 1 || pos == occup - 1)
    setNodePref(signal, node, 1);
}

/*
 * Remove and return entry at position.  Move entries less than the
 * removed one to the right.  Replace min entry by the input entry.
 * This is the opposite of nodePushDown.
 *
 *      X                        D
 *      v     ^                  ^
 *      A B C D E _ _  =>  X A B C E _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 */
void
Dbtux::nodePopUp(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  ScanOpPtr scanPtr;
  // move scans whose entry disappears
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At popUp pos=" << pos << " " << node << endl;
      }
#endif
      // here we may miss a valid entry "X"  XXX known bug
      scanNext(signal, scanPtr);
    }
    scanPtr.i = nextPtrI;
  }
  // fix other scans
  scanPtr.i = node.getNodeScan();
  while (scanPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    ndbrequire(scanPos.m_pos != pos);
    if (scanPos.m_pos < pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Fix scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At popUp pos=" << pos << " " << node << endl;
      }
#endif
      scanPos.m_pos++;
    }
    scanPtr.i = scanPtr.p->m_nodeScan;
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  entList[occup] = entList[0];
  TreeEnt* const tmpList = entList + 1;
  TreeEnt newMin = ent;
  ent = tmpList[pos];
  for (unsigned i = pos; i > 0; i--) {
    jam();
    tmpList[i] = tmpList[i - 1];
  }
  tmpList[0] = newMin;
  entList[0] = entList[occup];
  // fix prefixes
  if (true)
    setNodePref(signal, node, 0);
  if (occup == 1 || pos == occup - 1)
    setNodePref(signal, node, 1);
}

/*
 * Move all possible entries from another node before the min (i=0) or
 * after the max (i=1).  XXX can be optimized
 */
void
Dbtux::nodeSlide(Signal* signal, NodeHandle& dstNode, NodeHandle& srcNode, unsigned i)
{
  Frag& frag = dstNode.m_frag;
  TreeHead& tree = frag.m_tree;
  ndbrequire(i <= 1);
  while (dstNode.getOccup() < tree.m_maxOccup && srcNode.getOccup() != 0) {
    TreeEnt ent;
    nodePopDown(signal, srcNode, i == 0 ? srcNode.getOccup() - 1 : 0, ent);
    nodePushUp(signal, dstNode, i == 0 ? 0 : dstNode.getOccup(), ent);
  }
}

/*
 * Link scan to the list under the node.  The list is single-linked and
 * ordering does not matter.
 */
void
Dbtux::linkScan(NodeHandle& node, ScanOpPtr scanPtr)
{
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Link scan " << scanPtr.i << " " << *scanPtr.p << endl;
    debugOut << "To node " << node << endl;
  }
#endif
  ndbrequire(! islinkScan(node, scanPtr) && scanPtr.p->m_nodeScan == RNIL);
  scanPtr.p->m_nodeScan = node.getNodeScan();
  node.setNodeScan(scanPtr.i);
}

/*
 * Unlink a scan from the list under the node.
 */
void
Dbtux::unlinkScan(NodeHandle& node, ScanOpPtr scanPtr)
{
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Unlink scan " << scanPtr.i << " " << *scanPtr.p << endl;
    debugOut << "From node " << node << endl;
  }
#endif
  ScanOpPtr currPtr;
  currPtr.i = node.getNodeScan();
  ScanOpPtr prevPtr;
  prevPtr.i = RNIL;
  while (true) {
    jam();
    c_scanOpPool.getPtr(currPtr);
    Uint32 nextPtrI = currPtr.p->m_nodeScan;
    if (currPtr.i == scanPtr.i) {
      jam();
      if (prevPtr.i == RNIL) {
        node.setNodeScan(nextPtrI);
      } else {
        jam();
        prevPtr.p->m_nodeScan = nextPtrI;
      }
      scanPtr.p->m_nodeScan = RNIL;
      // check for duplicates
      ndbrequire(! islinkScan(node, scanPtr));
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
Dbtux::islinkScan(NodeHandle& node, ScanOpPtr scanPtr)
{
  ScanOpPtr currPtr;
  currPtr.i = node.getNodeScan();
  while (currPtr.i != RNIL) {
    jam();
    c_scanOpPool.getPtr(currPtr);
    if (currPtr.i == scanPtr.i) {
      jam();
      return true;
    }
    currPtr.i = currPtr.p->m_nodeScan;
  }
  return false;
}

void
Dbtux::NodeHandle::progError(int line, int cause, const char* file)
{
  ErrorReporter::handleAssert("Dbtux::NodeHandle: assert failed", file, line);
}
