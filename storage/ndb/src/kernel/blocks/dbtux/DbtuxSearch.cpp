/*
   Copyright (C) 2004-2006 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#define DBTUX_SEARCH_CPP
#include "Dbtux.hpp"

/*
 * Search down non-empty tree for node to update.  Compare search key to
 * each node minimum.  If greater, move to right subtree.  This can
 * overshoot target node.  The last such node is saved.  The search ends
 * at a final node which is a semi-leaf or leaf.  If search key is less
 * than final node minimum then the saved node (if any) is the g.l.b of
 * the final node and we move back to it.
 *
 * Search within the found node is done by caller.  On add, search key
 * may be before minimum or after maximum entry.  On remove, search key
 * is within the node.
 */
void
Dbtux::findNodeToUpdate(TuxCtx& ctx, Frag& frag, ConstData searchKey, TreeEnt searchEnt, NodeHandle& currNode)
{
  const TreeHead& tree = frag.m_tree;
  const Uint32 numAttrs = frag.m_numAttrs;
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true) {
    thrjam(ctx.jamBuffer);
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    unsigned start = 0;
    ret = cmpSearchKey(ctx, frag, start, searchKey, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      thrjam(ctx.jamBuffer);
      // read and compare remaining attributes
      ndbrequire(start < numAttrs);
      readKeyAttrs(ctx, frag, currNode.getEnt(0), start, ctx.c_entryKey);
      ret = cmpSearchKey(ctx, frag, start, searchKey, ctx.c_entryKey);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    }
    if (ret == 0) {
      thrjam(ctx.jamBuffer);
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getEnt(0));
    }
    if (ret < 0) {
      thrjam(ctx.jamBuffer);
      const TupLoc loc = currNode.getLink(0);
      if (loc != NullTupLoc) {
        thrjam(ctx.jamBuffer);
        // continue to left subtree
        currNode.m_loc = loc;
        continue;
      }
      if (! glbNode.isNull()) {
        thrjam(ctx.jamBuffer);
        // move up to the g.l.b
        currNode = glbNode;
      }
      break;
    }
    if (ret > 0) {
      thrjam(ctx.jamBuffer);
      const TupLoc loc = currNode.getLink(1);
      if (loc != NullTupLoc) {
        thrjam(ctx.jamBuffer);
        // save potential g.l.b
        glbNode = currNode;
        // continue to right subtree
        currNode.m_loc = loc;
        continue;
      }
      break;
    }
    // ret == 0
    thrjam(ctx.jamBuffer);
    break;
  }
}

/*
 * Find position within the final node to add entry to.  Use binary
 * search.  Return true if ok i.e. entry to add is not a duplicate.
 */
bool
Dbtux::findPosToAdd(TuxCtx& ctx, Frag& frag, ConstData searchKey, TreeEnt searchEnt, NodeHandle& currNode, TreePos& treePos)
{
  int lo = -1;
  int hi = (int)currNode.getOccup();
  int ret;
  while (hi - lo > 1) {
    thrjam(ctx.jamBuffer);
    // hi - lo > 1 implies lo < j < hi
    int j = (hi + lo) / 2;
    // read and compare attributes
    unsigned start = 0;
    readKeyAttrs(ctx, frag, currNode.getEnt(j), start, ctx.c_entryKey);
    ret = cmpSearchKey(ctx, frag, start, searchKey, ctx.c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret == 0) {
      thrjam(ctx.jamBuffer);
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getEnt(j));
    }
    if (ret < 0) {
      thrjam(ctx.jamBuffer);
      hi = j;
    } else if (ret > 0) {
      thrjam(ctx.jamBuffer);
      lo = j;
    } else {
      treePos.m_pos = j;
      // entry found - error
      return false;
    }
  }
  ndbrequire(hi - lo == 1);
  // return hi pos, see treeAdd() for next step
  treePos.m_pos = hi;
  return true;
}

/*
 * Find position within the final node to remove entry from.  Use linear
 * search.  Return true if ok i.e. the entry was found.
 */
bool
Dbtux::findPosToRemove(TuxCtx& ctx, Frag& frag, ConstData searchKey, TreeEnt searchEnt, NodeHandle& currNode, TreePos& treePos)
{
  const unsigned occup = currNode.getOccup();
  for (unsigned j = 0; j < occup; j++) {
    thrjam(ctx.jamBuffer);
    // compare only the entry
    if (searchEnt.eq(currNode.getEnt(j))) {
      thrjam(ctx.jamBuffer);
      treePos.m_pos = j;
      return true;
    }
  }
  treePos.m_pos = occup;
  // not found - failed
  return false;
}

/*
 * Search for entry to add.
 */
bool
Dbtux::searchToAdd(TuxCtx& ctx, Frag& frag, ConstData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (unlikely(currNode.m_loc == NullTupLoc)) {
    // empty tree
    thrjam(ctx.jamBuffer);
    return true;
  }
  findNodeToUpdate(ctx, frag, searchKey, searchEnt, currNode);
  treePos.m_loc = currNode.m_loc;
  if (! findPosToAdd(ctx, frag, searchKey, searchEnt, currNode, treePos)) {
    thrjam(ctx.jamBuffer);
    return false;
  }
  return true;
}

/*
 * Search for entry to remove.
 */
bool
Dbtux::searchToRemove(TuxCtx& ctx, Frag& frag, ConstData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (unlikely(currNode.m_loc == NullTupLoc)) {
    // empty tree - failed
    thrjam(ctx.jamBuffer);
    return false;
  }
  findNodeToUpdate(ctx, frag, searchKey, searchEnt, currNode);
  treePos.m_loc = currNode.m_loc;
  if (! findPosToRemove(ctx, frag, searchKey, searchEnt, currNode, treePos)) {
    thrjam(ctx.jamBuffer);
    return false;
  }
  return true;
}

/*
 * Search for scan start position.
 *
 * Similar to searchToAdd.  The routines differ somewhat depending on
 * scan direction and are done by separate methods.
 */
void
Dbtux::searchToScan(Frag& frag, ConstData boundInfo, unsigned boundCount, bool descending, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  if (tree.m_root != NullTupLoc) {
    if (! descending)
      searchToScanAscending(frag, boundInfo, boundCount, treePos);
    else
      searchToScanDescending(frag, boundInfo, boundCount, treePos);
    return;
  }
  // empty tree
}

void
Dbtux::searchToScanAscending(Frag& frag, ConstData boundInfo, unsigned boundCount, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    ret = cmpScanBound(frag, 0, boundInfo, boundCount, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare all attributes
      readKeyAttrs(c_ctx, frag, currNode.getEnt(0), 0, c_ctx.c_entryKey);
      ret = cmpScanBound(frag, 0, boundInfo, boundCount, c_ctx.c_entryKey);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    }
    if (ret < 0) {
      // bound is left of this node
      jam();
      const TupLoc loc = currNode.getLink(0);
      if (loc != NullTupLoc) {
        jam();
        // continue to left subtree
        currNode.m_loc = loc;
        continue;
      }
      if (! glbNode.isNull()) {
        jam();
        // move up to the g.l.b
        currNode = glbNode;
      }
    } else {
      // bound is at or right of this node
      jam();
      const TupLoc loc = currNode.getLink(1);
      if (loc != NullTupLoc) {
        jam();
        // save potential g.l.b
        glbNode = currNode;
        // continue to right subtree
        currNode.m_loc = loc;
        continue;
      }
    }
    break;
  }
  for (unsigned j = 0, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    int ret;
    // read and compare attributes
    readKeyAttrs(c_ctx, frag, currNode.getEnt(j), 0, c_ctx.c_entryKey);
    ret = cmpScanBound(frag, 0, boundInfo, boundCount, c_ctx.c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret < 0) {
      // found first entry satisfying the bound
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = j;
      treePos.m_dir = 3;
      return;
    }
  }
  // start scanning after node end i.e. proceed to right child
  treePos.m_loc = currNode.m_loc;
  treePos.m_pos = ZNIL;
  treePos.m_dir = 5;
}

void
Dbtux::searchToScanDescending(Frag& frag, ConstData boundInfo, unsigned boundCount, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    ret = cmpScanBound(frag, 1, boundInfo, boundCount, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare all attributes
      readKeyAttrs(c_ctx, frag, currNode.getEnt(0), 0, c_ctx.c_entryKey);
      ret = cmpScanBound(frag, 1, boundInfo, boundCount, c_ctx.c_entryKey);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    }
    if (ret < 0) {
      // bound is left of this node
      jam();
      const TupLoc loc = currNode.getLink(0);
      if (loc != NullTupLoc) {
        jam();
        // continue to left subtree
        currNode.m_loc = loc;
        continue;
      }
      if (! glbNode.isNull()) {
        jam();
        // move up to the g.l.b
        currNode = glbNode;
      } else {
        // empty result set
        return;
      }
    } else {
      // bound is at or right of this node
      jam();
      const TupLoc loc = currNode.getLink(1);
      if (loc != NullTupLoc) {
        jam();
        // save potential g.l.b
        glbNode = currNode;
        // continue to right subtree
        currNode.m_loc = loc;
        continue;
      }
    }
    break;
  }
  for (unsigned j = 0, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    int ret;
    // read and compare attributes
    readKeyAttrs(c_ctx, frag, currNode.getEnt(j), 0, c_ctx.c_entryKey);
    ret = cmpScanBound(frag, 1, boundInfo, boundCount, c_ctx.c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret < 0) {
      if (j > 0) {
        // start scanning from previous entry
        treePos.m_loc = currNode.m_loc;
        treePos.m_pos = j - 1;
        treePos.m_dir = 3;
        return;
      }
      // start scanning upwards (pretend we came from left child)
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = 0;
      treePos.m_dir = 0;
      return;
    }
  }
  // start scanning this node
  treePos.m_loc = currNode.m_loc;
  treePos.m_pos = currNode.getOccup() - 1;
  treePos.m_dir = 3;
}
