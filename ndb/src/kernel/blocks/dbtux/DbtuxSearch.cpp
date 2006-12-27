/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUX_SEARCH_CPP
#include "Dbtux.hpp"

/*
 * Search for entry to add.
 *
 * Similar to searchToRemove (see below).
 */
bool
Dbtux::searchToAdd(Frag& frag, ConstData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  const unsigned numAttrs = frag.m_numAttrs;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (currNode.m_loc == NullTupLoc) {
    // empty tree
    jam();
    return true;
  }
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  /*
   * In order to not (yet) change old behaviour, a position between
   * 2 nodes returns the one at the bottom of the tree.
   */
  NodeHandle bottomNode(frag);
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    unsigned start = 0;
    ret = cmpSearchKey(frag, start, searchKey, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare remaining attributes
      ndbrequire(start < numAttrs);
      readKeyAttrs(frag, currNode.getMinMax(0), start, c_entryKey);
      ret = cmpSearchKey(frag, start, searchKey, c_entryKey);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    }
    if (ret == 0) {
      jam();
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getMinMax(0));
    }
    if (ret < 0) {
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
        // move up to the g.l.b but remember the bottom node
        bottomNode = currNode;
        currNode = glbNode;
      }
    } else if (ret > 0) {
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
    } else {
      jam();
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = 0;
      // entry found - error
      return false;
    }
    break;
  }
  // anticipate
  treePos.m_loc = currNode.m_loc;
  // binary search
  int lo = -1;
  int hi = currNode.getOccup();
  int ret;
  while (1) {
    jam();
    // hi - lo > 1 implies lo < j < hi
    int j = (hi + lo) / 2;
    // read and compare attributes
    unsigned start = 0;
    readKeyAttrs(frag, currNode.getEnt(j), start, c_entryKey);
    ret = cmpSearchKey(frag, start, searchKey, c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret == 0) {
      jam();
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getEnt(j));
    }
    if (ret < 0)
      hi = j;
    else if (ret > 0)
      lo = j;
    else {
      treePos.m_pos = j;
      // entry found - error
      return false;
    }
    if (hi - lo == 1)
      break;
  }
  if (ret < 0) {
    jam();
    treePos.m_pos = hi;
    return true;
  }
  if ((uint) hi < currNode.getOccup()) {
    jam();
    treePos.m_pos = hi;
    return true;
  }
  if (bottomNode.isNull()) {
    jam();
    treePos.m_pos = hi;
    return true;
  }
  jam();
  // backwards compatible for now
  treePos.m_loc = bottomNode.m_loc;
  treePos.m_pos = 0;
  return true;
}

/*
 * Search for entry to remove.
 *
 * Compares search key to each node min.  A move to right subtree can
 * overshoot target node.  The last such node is saved.  The final node
 * is a semi-leaf or leaf.  If search key is less than final node min
 * then the saved node is the g.l.b of the final node and we move back
 * to it.
 */
bool
Dbtux::searchToRemove(Frag& frag, ConstData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  const unsigned numAttrs = frag.m_numAttrs;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (currNode.m_loc == NullTupLoc) {
    // empty tree - failed
    jam();
    return false;
  }
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    unsigned start = 0;
    ret = cmpSearchKey(frag, start, searchKey, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare remaining attributes
      ndbrequire(start < numAttrs);
      readKeyAttrs(frag, currNode.getMinMax(0), start, c_entryKey);
      ret = cmpSearchKey(frag, start, searchKey, c_entryKey);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    }
    if (ret == 0) {
      jam();
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getMinMax(0));
    }
    if (ret < 0) {
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
    } else if (ret > 0) {
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
    } else {
      jam();
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = 0;
      return true;
    }
    break;
  }
  // anticipate
  treePos.m_loc = currNode.m_loc;
  // pos 0 was handled above
  for (unsigned j = 1, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    // compare only the entry
    if (searchEnt.eq(currNode.getEnt(j))) {
      jam();
      treePos.m_pos = j;
      return true;
    }
  }
  treePos.m_pos = currNode.getOccup();
  // not found - failed
  return false;
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
  NodeHandle bottomNode(frag);
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    ret = cmpScanBound(frag, 0, boundInfo, boundCount, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare all attributes
      readKeyAttrs(frag, currNode.getMinMax(0), 0, c_entryKey);
      ret = cmpScanBound(frag, 0, boundInfo, boundCount, c_entryKey);
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
        // move up to the g.l.b but remember the bottom node
        bottomNode = currNode;
        currNode = glbNode;
      } else {
        // start scanning this node
        treePos.m_loc = currNode.m_loc;
        treePos.m_pos = 0;
        treePos.m_dir = 3;
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
    readKeyAttrs(frag, currNode.getEnt(j), 0, c_entryKey);
    ret = cmpScanBound(frag, 0, boundInfo, boundCount, c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret < 0) {
      // found first entry satisfying the bound
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = j;
      treePos.m_dir = 3;
      return;
    }
  }
  // bound is to right of this node
  if (! bottomNode.isNull()) {
    jam();
    // start scanning the l.u.b
    treePos.m_loc = bottomNode.m_loc;
    treePos.m_pos = 0;
    treePos.m_dir = 3;
    return;
  }
  // start scanning upwards (pretend we came from right child)
  treePos.m_loc = currNode.m_loc;
  treePos.m_dir = 1;
}

void
Dbtux::searchToScanDescending(Frag& frag, ConstData boundInfo, unsigned boundCount, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  NodeHandle bottomNode(frag);
  while (true) {
    jam();
    selectNode(currNode, currNode.m_loc);
    int ret;
    // compare prefix
    ret = cmpScanBound(frag, 1, boundInfo, boundCount, currNode.getPref(), tree.m_prefSize);
    if (ret == NdbSqlUtil::CmpUnknown) {
      jam();
      // read and compare all attributes
      readKeyAttrs(frag, currNode.getMinMax(0), 0, c_entryKey);
      ret = cmpScanBound(frag, 1, boundInfo, boundCount, c_entryKey);
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
        // move up to the g.l.b but remember the bottom node
        bottomNode = currNode;
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
    readKeyAttrs(frag, currNode.getEnt(j), 0, c_entryKey);
    ret = cmpScanBound(frag, 1, boundInfo, boundCount, c_entryKey);
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
