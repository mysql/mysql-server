/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUX_NODE_CPP
#include "Dbtux.hpp"

#define JAM_FILE_ID 372


/*
 * Allocate index node in TUP.
 *
 * Can be called from MT-build of ordered indexes.
 */
int
Dbtux::allocNode(TuxCtx& ctx, NodeHandle& node)
{
  if (ERROR_INSERTED(12007)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    return TuxMaintReq::NoMemError;
  }
  Uint32 pageId = NullTupLoc.getPageId();
  Uint32 pageOffset = NullTupLoc.getPageOffset();
  Uint32* node32 = 0;
  int errorCode = c_tup->tuxAllocNode(ctx.jamBuffer,
                                      ctx.tupIndexFragPtr,
                                      ctx.tupIndexTablePtr,
                                      pageId,
                                      pageOffset,
                                      node32);
  thrjamEntryDebug(ctx.jamBuffer);
  if (likely(errorCode == 0))
  {
    thrjamDebug(ctx.jamBuffer);
    node.m_loc = TupLoc(pageId, pageOffset);
    node.m_node = reinterpret_cast<TreeNode*>(node32);
    ndbrequire(node.m_loc != NullTupLoc && node.m_node != 0);
  }
  else
  {
    switch (errorCode) {
    case 827:
      errorCode = TuxMaintReq::NoMemError;
      break;
    case 921:
      errorCode = TuxMaintReq::NoTransMemError;
      break;
    }
  }
  return errorCode;
}

/*
 * Free index node in TUP
 */
void
Dbtux::freeNode(NodeHandle& node)
{
  Uint32 pageId = node.m_loc.getPageId();
  Uint32 pageOffset = node.m_loc.getPageOffset();
  Uint32* node32 = reinterpret_cast<Uint32*>(node.m_node);
  c_tup->tuxFreeNode(c_ctx.tupIndexFragPtr,
                     c_ctx.tupIndexTablePtr,
                     pageId,
                     pageOffset,
                     node32);
  jamEntry();
  // invalidate the handle
  node.m_loc = NullTupLoc;
  node.m_node = 0;
}

/*
 * Set handle to point to existing node.
 * Can be called from MT-build of ordered indexes.
 */
void
Dbtux::selectNode(TuxCtx& ctx, NodeHandle& node, TupLoc loc)
{
  ndbrequire(loc != NullTupLoc);
  Uint32 pageId = loc.getPageId();
  Uint32 pageOffset = loc.getPageOffset();
  Uint32* node32 = 0;
  c_tup->tuxGetNode(ctx.attrDataOffset,
                    ctx.tuxFixHeaderSize,
                    pageId,
                    pageOffset,
                    node32);
  node.m_loc = loc;
  node.m_node = reinterpret_cast<TreeNode*>(node32);
  ndbrequire(node.m_loc != NullTupLoc && node.m_node != 0);
}

/*
 * Set handle to point to new node.  Uses a pre-allocated node.
 *
 * Can be called from MT-build of ordered indexes.
 */
void
Dbtux::insertNode(TuxCtx& ctx, NodeHandle& node)
{
  Frag& frag = node.m_frag;
  // use up pre-allocated node
  selectNode(ctx, node, frag.m_freeLoc);
  frag.m_freeLoc = NullTupLoc;
  new (node.m_node) TreeNode();
#ifdef VM_TRACE
  TreeHead& tree = frag.m_tree;
  memset(node.getPref(), DataFillByte, tree.m_prefSize << 2);
  TreeEnt* entList = tree.getEntList(node.m_node);
  memset(entList, NodeFillByte, tree.m_maxOccup * (TreeEntSize << 2));
#endif
}

/*
 * Delete existing node.  Make it the pre-allocated free node if there
 * is none.  Otherwise return it to fragment's free list.
 */
void
Dbtux::deleteNode(NodeHandle& node)
{
  Frag& frag = node.m_frag;
  ndbrequire(node.getOccup() == 0);
  if (frag.m_freeLoc == NullTupLoc)
  {
    jam();
    frag.m_freeLoc = node.m_loc;
    // invalidate the handle
    node.m_loc = NullTupLoc;
    node.m_node = 0;
  }
  else
  {
    jam();
    freeNode(node);
  }
}

/*
 * Free the pre-allocated node, called when tree is empty.  This avoids
 * leaving any used pages in DataMemory.
 */
void
Dbtux::freePreallocatedNode(Frag& frag)
{
  if (frag.m_freeLoc != NullTupLoc)
  {
    jam();
    NodeHandle node(frag);
    selectNode(c_ctx, node, frag.m_freeLoc);
    freeNode(node);
    frag.m_freeLoc = NullTupLoc;
  }
}

/*
 * Set prefix.  Copies the defined number of attributes.
 *
 * Can be called from MT-build of ordered indexes.
 */
void
Dbtux::setNodePref(TuxCtx & ctx, NodeHandle& node)
{
  const Frag& frag = node.m_frag;
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  /*
   * bug#12873640
   * Node prefix exists if it has non-zero number of attributes.  It is
   * then a partial instance of KeyData.  If the prefix does not exist
   * then set_buf() could overwrite m_pageId1 in first entry, causing
   * random crash in TUP via readKeyAttrs().
   */
  if (index.m_prefAttrs > 0)
  {
    thrjam(ctx.jamBuffer);
    readKeyAttrs(ctx,
                 frag,
                 node.getEnt(0),
                 index.m_prefAttrs,
                 node.getPref());
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
 *
 * Add list of scans at the new entry.
 *
 * Can be called from MT-build of ordered indexes.
 */
void
Dbtux::nodePushUp(TuxCtx & ctx,
                  NodeHandle& node,
                  unsigned pos,
                  const TreeEnt& ent,
                  Uint32 scanList)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup < tree.m_maxOccup && pos <= occup);
  // fix old scans
  if (node.getNodeScan() != RNIL)
    nodePushUpScans(node, pos);
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  for (unsigned i = occup; i > pos; i--) {
    thrjamDebug(ctx.jamBuffer);
    entList[i] = entList[i - 1];
  }
  entList[pos] = ent;
  node.setOccup(occup + 1);
  // add new scans
  if (scanList != RNIL)
    addScanList(node, pos, scanList);
  // fix prefix
  if (occup == 0 || pos == 0)
    setNodePref(ctx, node);
}

/**
 * Can be called from MT-build of ordered indexes.
 * But should never enter here since there cannot be
 * any active scans while we are rebuilding ordered
 * index.
 */
void
Dbtux::nodePushUpScans(NodeHandle& node, unsigned pos)
{
  const unsigned occup = node.getOccup();
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
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
  } while (scanPtr.i != RNIL);
}

/*
 * Remove and return entry at position.  Move entries greater than the
 * removed one to the left.  This is the opposite of nodePushUp.
 *
 *                               D
 *            ^                  ^
 *      A B C D E F _  =>  A B C E F _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 *
 * Scans at removed entry are returned if non-zero location is passed or
 * else moved forward.
 */
void
Dbtux::nodePopDown(TuxCtx& ctx, NodeHandle& node, unsigned pos, TreeEnt& ent, Uint32* scanList)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  if (node.getNodeScan() != RNIL) {
    // remove or move scans at this position
    if (scanList == 0)
      moveScanList(node, pos);
    else
      removeScanList(node, pos, *scanList);
    // fix other scans
    if (node.getNodeScan() != RNIL)
      nodePopDownScans(node, pos);
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  ent = entList[pos];
  for (unsigned i = pos; i < occup - 1; i++) {
    thrjam(ctx.jamBuffer);
    entList[i] = entList[i + 1];
  }
  node.setOccup(occup - 1);
  // fix prefix
  if (occup != 1 && pos == 0)
    setNodePref(ctx, node);
}

void
Dbtux::nodePopDownScans(NodeHandle& node, unsigned pos)
{
  const unsigned occup = node.getOccup();
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    // handled before
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
  } while (scanPtr.i != RNIL);
}

/*
 * Add entry at existing position.  Move entries less than or equal to
 * the old one to the left.  Remove and return old min entry.
 *
 *            X            A
 *      ^     v            ^
 *      A B C D E _ _  =>  B C D X E _ _
 *      0 1 2 3 4 5 6      0 1 2 3 4 5 6
 *
 * Return list of scans at the removed position 0.
 *
 * Can be called from MT-build of ordered indexes.
 */
void
Dbtux::nodePushDown(TuxCtx& ctx,
                    NodeHandle& node,
                    unsigned pos,
                    TreeEnt& ent,
                    Uint32& scanList)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  if (node.getNodeScan() != RNIL) {
    // remove scans at 0
    removeScanList(node, 0, scanList);
    // fix other scans
    if (node.getNodeScan() != RNIL)
      nodePushDownScans(node, pos);
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  TreeEnt oldMin = entList[0];
  for (unsigned i = 0; i < pos; i++) {
    thrjamDebug(ctx.jamBuffer);
    entList[i] = entList[i + 1];
  }
  entList[pos] = ent;
  ent = oldMin;
  // fix prefix
  if (true)
    setNodePref(ctx, node);
}

/**
 * Can be called from MT-build of ordered indexes, but should
 * never happen since no active scans can be around when
 * building ordered indexes.
 */
void
Dbtux::nodePushDownScans(NodeHandle& node, unsigned pos)
{
  const unsigned occup = node.getOccup();
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc && scanPos.m_pos < occup);
    // handled before
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
  } while (scanPtr.i != RNIL);
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
 *
 * Move scans at removed entry and add scans at the new entry.
 */
void
Dbtux::nodePopUp(TuxCtx& ctx, NodeHandle& node, unsigned pos, TreeEnt& ent, Uint32 scanList)
{
  Frag& frag = node.m_frag;
  TreeHead& tree = frag.m_tree;
  const unsigned occup = node.getOccup();
  ndbrequire(occup <= tree.m_maxOccup && pos < occup);
  if (node.getNodeScan() != RNIL) {
    // move scans whose entry disappears
    moveScanList(node, pos);
    // fix other scans
    if (node.getNodeScan() != RNIL)
      nodePopUpScans(node, pos);
  }
  // fix node
  TreeEnt* const entList = tree.getEntList(node.m_node);
  TreeEnt newMin = ent;
  ent = entList[pos];
  for (unsigned i = pos; i > 0; i--) {
    thrjam(ctx.jamBuffer);
    entList[i] = entList[i - 1];
  }
  entList[0] = newMin;
  // add scans
  if (scanList != RNIL)
    addScanList(node, 0, scanList);
  // fix prefix
  if (true)
    setNodePref(ctx, node);
}

void
Dbtux::nodePopUpScans(NodeHandle& node, unsigned pos)
{
  const unsigned occup = node.getOccup();
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
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
  } while (scanPtr.i != RNIL);
}

/*
 * Move number of entries from another node to this node before the min
 * (i=0) or after the max (i=1).  Expensive but not often used.
 */
void
Dbtux::nodeSlide(TuxCtx& ctx, NodeHandle& dstNode, NodeHandle& srcNode, unsigned cnt, unsigned i)
{
  ndbrequire(i <= 1);
  while (cnt != 0) {
    TreeEnt ent;
    Uint32 scanList = RNIL;
    nodePopDown(ctx, srcNode, i == 0 ? srcNode.getOccup() - 1 : 0, ent, &scanList);
    nodePushUp(ctx, dstNode, i == 0 ? 0 : dstNode.getOccup(), ent, scanList);
    cnt--;
  }
}

// scans linked to node


/*
 * Add list of scans to node at given position.
 *
 * Can be called from MT-build of ordered indexes, but it
 * should never happen since no active scans should be around
 * when building ordered indexes.
 */
void
Dbtux::addScanList(NodeHandle& node, unsigned pos, Uint32 scanList)
{
  ScanOpPtr scanPtr;
  scanPtr.i = scanList;
  do {
    jam();
    c_scanOpPool.getPtr(scanPtr);
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Add scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "To pos=" << pos << " " << node << endl;
      }
#endif
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    scanPtr.p->m_nodeScan = RNIL;
    linkScan(node, scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    // set position but leave direction alone
    scanPos.m_loc = node.m_loc;
    scanPos.m_pos = pos;
    scanPtr.i = nextPtrI;
  } while (scanPtr.i != RNIL);
}

/*
 * Remove list of scans from node at given position.  The return
 * location must point to existing list (in fact RNIL always).
 *
 * Can be called from MT-build of ordered indexes, but should
 * never occur since no active scans can be around when
 * building ordered indexes.
 */
void
Dbtux::removeScanList(NodeHandle& node, unsigned pos, Uint32& scanList)
{
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    TreePos& scanPos = scanPtr.p->m_scanPos;
    ndbrequire(scanPos.m_loc == node.m_loc);
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Remove scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "Fron pos=" << pos << " " << node << endl;
      }
#endif
      unlinkScan(node, scanPtr);
      scanPtr.p->m_nodeScan = scanList;
      scanList = scanPtr.i;
      // unset position but leave direction alone
      scanPos.m_loc = NullTupLoc;
      scanPos.m_pos = Uint32(~0);
    }
    scanPtr.i = nextPtrI;
  } while (scanPtr.i != RNIL);
}

/*
 * Move list of scans away from entry about to be removed.  Uses scan
 * method scanNext().
 */
void
Dbtux::moveScanList(NodeHandle& node, unsigned pos)
{
  ScanOpPtr scanPtr;
  scanPtr.i = node.getNodeScan();
  do {
    jam();
    c_scanOpPool.getPtr(scanPtr);
    TreePos& scanPos = scanPtr.p->m_scanPos;
    const Uint32 nextPtrI = scanPtr.p->m_nodeScan;
    ndbrequire(scanPos.m_loc == node.m_loc);
    if (scanPos.m_pos == pos) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & DebugScan) {
        debugOut << "Move scan " << scanPtr.i << " " << *scanPtr.p << endl;
        debugOut << "At pos=" << pos << " " << node << endl;
      }
#endif
      prepare_move_scan_ctx(scanPtr);
      Frag& frag = *c_ctx.fragPtr.p;
      scanNext(scanPtr, true, frag);
      ndbrequire(! (scanPos.m_loc == node.m_loc && scanPos.m_pos == pos));
    }
    scanPtr.i = nextPtrI;
  } while (scanPtr.i != RNIL);
}

/*
 * Link scan to the list under the node.  The list is single-linked and
 * ordering does not matter.
 */
void
Dbtux::linkScan(NodeHandle& node, ScanOpPtr scanPtr)
{
  ndbassert(! islinkScan(node, scanPtr) && scanPtr.p->m_nodeScan == RNIL);
  scanPtr.p->m_nodeScan = node.getNodeScan();
  node.setNodeScan(scanPtr.i);
}

/*
 * Unlink a scan from the list under the node.
 *
 * Can be called from MT-build of ordered indexes, but should
 * not since no active scans should be around when building
 * ordered indexes.
 */
void
Dbtux::unlinkScan(NodeHandle& node, ScanOpPtr scanPtr)
{
  ScanOpPtr currPtr;
  currPtr.i = node.getNodeScan();
  ScanOpPtr prevPtr;
  prevPtr.i = RNIL;
  while (true) {
    jamDebug();
    c_scanOpPool.getPtr(currPtr);
    Uint32 nextPtrI = currPtr.p->m_nodeScan;
    if (currPtr.i == scanPtr.i) {
      jamDebug();
      if (prevPtr.i == RNIL) {
        node.setNodeScan(nextPtrI);
      } else {
        jamDebug();
        prevPtr.p->m_nodeScan = nextPtrI;
      }
      scanPtr.p->m_nodeScan = RNIL;
      // check for duplicates
      ndbassert(!islinkScan(node, scanPtr));
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
    jamDebug();
    c_scanOpPool.getPtr(currPtr);
    if (currPtr.i == scanPtr.i) {
      jamDebug();
      return true;
    }
    currPtr.i = currPtr.p->m_nodeScan;
  }
  return false;
}

void
Dbtux::NodeHandle::progError(int line, int cause, const char* file,
                             const char* check)
{
  char buf[500];
  /*Add the check to the log message only if default value of ""
    is over-written. */
  if(native_strcasecmp(check,"") == 0)
    BaseString::snprintf(buf, sizeof(buf),
                         "Dbtux::NodeHandle: assert failed");
  else
    BaseString::snprintf(buf, sizeof(buf),
                         "Dbtux::NodeHandle: assert %.400s failed",
                         check);

  ErrorReporter::handleAssert(buf, file, line);
}
