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

#define DBTUX_SEARCH_CPP
#include "Dbtux.hpp"

/*
 * Search for entry to add.
 *
 * Similar to searchToRemove (see below).
 *
 * TODO optimize for initial equal attrs in node min/max
 */
void
Dbtux::searchToAdd(Signal* signal, Frag& frag, TableData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  const unsigned numAttrs = frag.m_numAttrs;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (currNode.m_loc == NullTupLoc) {
    // empty tree
    jam();
    treePos.m_match = false;
    return;
  }
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  /*
   * In order to not (yet) change old behaviour, a position between
   * 2 nodes returns the one at the bottom of the tree.
   */
  NodeHandle bottomNode(frag);
  while (true) {
    jam();
    selectNode(signal, currNode, currNode.m_loc, AccPref);
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
      treePos.m_match = true;
      return;
    }
    break;
  }
  // access rest of current node
  accessNode(signal, currNode, AccFull);
  for (unsigned j = 0, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    int ret;
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
    if (ret <= 0) {
      jam();
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = j;
      treePos.m_match = (ret == 0);
      return;
    }
  }
  if (! bottomNode.isNull()) {
    jam();
    // backwards compatible for now
    treePos.m_loc = bottomNode.m_loc;
    treePos.m_pos = 0;
    treePos.m_match = false;
    return;
  }
  treePos.m_loc = currNode.m_loc;
  treePos.m_pos = currNode.getOccup();
  treePos.m_match = false;
}

/*
 * Search for entry to remove.
 *
 * Compares search key to each node min.  A move to right subtree can
 * overshoot target node.  The last such node is saved.  The final node
 * is a half-leaf or leaf.  If search key is less than final node min
 * then the saved node is the g.l.b of the final node and we move back
 * to it.
 */
void
Dbtux::searchToRemove(Signal* signal, Frag& frag, TableData searchKey, TreeEnt searchEnt, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  const unsigned numAttrs = frag.m_numAttrs;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (currNode.m_loc == NullTupLoc) {
    // empty tree
    jam();
    treePos.m_match = false;
    return;
  }
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true) {
    jam();
    selectNode(signal, currNode, currNode.m_loc, AccPref);
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
      treePos.m_match = true;
      return;
    }
    break;
  }
  // access rest of current node
  accessNode(signal, currNode, AccFull);
  // pos 0 was handled above
  for (unsigned j = 1, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    // compare only the entry
    if (searchEnt.eq(currNode.getEnt(j))) {
      jam();
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = j;
      treePos.m_match = true;
      return;
    }
  }
  treePos.m_loc = currNode.m_loc;
  treePos.m_pos = currNode.getOccup();
  treePos.m_match = false;
}

/*
 * Search for scan start position.
 *
 * Similar to searchToAdd.
 */
void
Dbtux::searchToScan(Signal* signal, Frag& frag, ConstData boundInfo, unsigned boundCount, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (currNode.m_loc == NullTupLoc) {
    // empty tree
    jam();
    treePos.m_match = false;
    return;
  }
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  NodeHandle bottomNode(frag);
  while (true) {
    jam();
    selectNode(signal, currNode, currNode.m_loc, AccPref);
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
        treePos.m_match = false;
        treePos.m_dir = 3;
        return;
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
      ndbassert(false);
    }
    break;
  }
  // access rest of current node
  accessNode(signal, currNode, AccFull);
  for (unsigned j = 0, occup = currNode.getOccup(); j < occup; j++) {
    jam();
    int ret;
    // read and compare attributes
    readKeyAttrs(frag, currNode.getEnt(j), 0, c_entryKey);
    ret = cmpScanBound(frag, 0, boundInfo, boundCount, c_entryKey);
    ndbrequire(ret != NdbSqlUtil::CmpUnknown);
    if (ret < 0) {
      // start scanning from current entry
      treePos.m_loc = currNode.m_loc;
      treePos.m_pos = j;
      treePos.m_match = false;
      treePos.m_dir = 3;
      return;
    }
  }
  if (! bottomNode.isNull()) {
    jam();
    // start scanning the l.u.b
    treePos.m_loc = bottomNode.m_loc;
    treePos.m_pos = 0;
    treePos.m_match = false;
    treePos.m_dir = 3;
    return;
  }
  // start scanning upwards (pretend we came from right child)
  treePos.m_loc = currNode.m_loc;
  treePos.m_dir = 1;
}
