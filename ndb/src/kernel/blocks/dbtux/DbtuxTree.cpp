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

#define DBTUX_TREE_CPP
#include "Dbtux.hpp"

/*
 * Search for entry.
 *
 * Search key is index attribute data and tree entry value.  Start from
 * root node and compare the key to min/max of each node.  Use linear
 * search on the final (bounding) node.  Initial attributes which are
 * same in min/max need not be checked.
 */
void
Dbtux::treeSearch(Signal* signal, Frag& frag, SearchPar searchPar, TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  const unsigned numAttrs = frag.m_numAttrs;
  treePos.m_loc = tree.m_root;
  NodeHandlePtr nodePtr;
  if (treePos.m_loc == NullTupLoc) {
    // empty tree
    jam();
    treePos.m_pos = 0;
    treePos.m_match = false;
    return;
  }
loop: {
    jam();
    selectNode(signal, frag, nodePtr, treePos.m_loc, AccPref);
    const unsigned occup = nodePtr.p->getOccup();
    ndbrequire(occup != 0);
    // number of equal initial attributes in bounding node
    unsigned numEq = ZNIL;
    for (unsigned i = 0; i <= 1; i++) {
      jam();
      // compare prefix
      CmpPar cmpPar;
      cmpPar.m_data1 = searchPar.m_data;
      cmpPar.m_data2 = nodePtr.p->getPref(i);
      cmpPar.m_len2 = tree.m_prefSize;
      cmpPar.m_first = 0;
      cmpPar.m_numEq = 0;
      int ret = cmpTreeAttrs(frag, cmpPar);
      if (ret == NdbSqlUtil::CmpUnknown) {
        jam();
        // read full value
        ReadPar readPar;
        readPar.m_ent = nodePtr.p->getMinMax(i);
        ndbrequire(cmpPar.m_numEq < numAttrs);
        readPar.m_first = cmpPar.m_numEq;
        readPar.m_count = numAttrs - cmpPar.m_numEq;
        readPar.m_data = 0;     // leave in signal data
        tupReadAttrs(signal, frag, readPar);
        // compare full value
        cmpPar.m_data2 = readPar.m_data;
        cmpPar.m_len2 = ZNIL;   // big
        cmpPar.m_first = readPar.m_first;
        ret = cmpTreeAttrs(frag, cmpPar);
        ndbrequire(ret != NdbSqlUtil::CmpUnknown);
      }
      if (numEq > cmpPar.m_numEq)
        numEq = cmpPar.m_numEq;
      if (ret == 0) {
        jam();
        // keys are equal, compare entry values
        ret = searchPar.m_ent.cmp(nodePtr.p->getMinMax(i));
      }
      if (i == 0 ? (ret < 0) : (ret > 0)) {
        jam();
        const TupLoc loc = nodePtr.p->getLink(i);
        if (loc != NullTupLoc) {
          jam();
          // continue to left/right subtree
          treePos.m_loc = loc;
          goto loop;
        }
        // position is immediately before/after this node
        treePos.m_pos = (i == 0 ? 0 : occup);
        treePos.m_match = false;
        return;
      }
      if (ret == 0) {
        jam();
        // position is at first/last entry
        treePos.m_pos = (i == 0 ? 0 : occup - 1);
        treePos.m_match = true;
        return;
      }
    }
    // read rest of the bounding node
    accessNode(signal, frag, nodePtr, AccFull);
    // position is strictly within the node
    ndbrequire(occup >= 2);
    const unsigned numWithin = occup - 2;
    for (unsigned j = 1; j <= numWithin; j++) {
      jam();
      int ret = 0;
      // compare remaining attributes
      if (numEq < numAttrs) {
        jam();
        ReadPar readPar;
        readPar.m_ent = nodePtr.p->getEnt(j);
        readPar.m_first = numEq;
        readPar.m_count = numAttrs - numEq;
        readPar.m_data = 0;     // leave in signal data
        tupReadAttrs(signal, frag, readPar);
        // compare
        CmpPar cmpPar;
        cmpPar.m_data1 = searchPar.m_data;
        cmpPar.m_data2 = readPar.m_data;
        cmpPar.m_len2 = ZNIL;  // big
        cmpPar.m_first = readPar.m_first;
        ret = cmpTreeAttrs(frag, cmpPar);
        ndbrequire(ret != NdbSqlUtil::CmpUnknown);
      }
      if (ret == 0) {
        jam();
        // keys are equal, compare entry values
        ret = searchPar.m_ent.cmp(nodePtr.p->getEnt(j));
      }
      if (ret <= 0) {
        jam();
        // position is before or at this entry
        treePos.m_pos = j;
        treePos.m_match = (ret == 0);
        return;
      }
    }
    // position is before last entry
    treePos.m_pos = occup - 1;
    treePos.m_match = false;
    return;
  }
}

/*
 * Add entry.
 */
void
Dbtux::treeAdd(Signal* signal, Frag& frag, TreePos treePos, TreeEnt ent)
{
  TreeHead& tree = frag.m_tree;
  unsigned pos = treePos.m_pos;
  NodeHandlePtr nodePtr;
  // check for empty tree
  if (treePos.m_loc == NullTupLoc) {
    jam();
    insertNode(signal, frag, nodePtr, AccPref);
    nodePtr.p->pushUp(signal, 0, ent);
    nodePtr.p->setSide(2);
    tree.m_root = nodePtr.p->m_loc;
    return;
  }
  // access full node
  selectNode(signal, frag, nodePtr, treePos.m_loc, AccFull);
  // check if it is bounding node
  if (pos != 0 && pos != nodePtr.p->getOccup()) {
    jam();
    // check if room for one more
    if (nodePtr.p->getOccup() < tree.m_maxOccup) {
      jam();
      nodePtr.p->pushUp(signal, pos, ent);
      return;
    }
    // returns min entry
    nodePtr.p->pushDown(signal, pos - 1, ent);
    // find position to add the removed min entry
    TupLoc childLoc = nodePtr.p->getLink(0);
    if (childLoc == NullTupLoc) {
      jam();
      // left child will be added
      pos = 0;
    } else {
      jam();
      // find glb node
      while (childLoc != NullTupLoc) {
        jam();
        selectNode(signal, frag, nodePtr, childLoc, AccHead);
        childLoc = nodePtr.p->getLink(1);
      }
      // access full node again
      accessNode(signal, frag, nodePtr, AccFull);
      pos = nodePtr.p->getOccup();
    }
    // fall thru to next case
  }
  // adding new min or max
  unsigned i = (pos == 0 ? 0 : 1);
  ndbrequire(nodePtr.p->getLink(i) == NullTupLoc);
  // check if the half-leaf/leaf has room for one more
  if (nodePtr.p->getOccup() < tree.m_maxOccup) {
    jam();
    nodePtr.p->pushUp(signal, pos, ent);
    return;
  }
  // add a new node
  NodeHandlePtr childPtr;
  insertNode(signal, frag, childPtr, AccPref);
  childPtr.p->pushUp(signal, 0, ent);
  // connect parent and child
  nodePtr.p->setLink(i, childPtr.p->m_loc);
  childPtr.p->setLink(2, nodePtr.p->m_loc);
  childPtr.p->setSide(i);
  // re-balance tree at each node
  while (true) {
    // height of subtree i has increased by 1
    int j = (i == 0 ? -1 : +1);
    int b = nodePtr.p->getBalance();
    if (b == 0) {
      // perfectly balanced
      jam();
      nodePtr.p->setBalance(j);
      // height change propagates up
    } else if (b == -j) {
      // height of shorter subtree increased
      jam();
      nodePtr.p->setBalance(0);
      // height of tree did not change - done
      break;
    } else if (b == j) {
      // height of longer subtree increased
      jam();
      NodeHandlePtr childPtr;
      selectNode(signal, frag, childPtr, nodePtr.p->getLink(i), AccHead);
      int b2 = childPtr.p->getBalance();
      if (b2 == b) {
        jam();
        treeRotateSingle(signal, frag, nodePtr, i);
      } else if (b2 == -b) {
        jam();
        treeRotateDouble(signal, frag, nodePtr, i);
      } else {
        // height of subtree increased so it cannot be perfectly balanced
        ndbrequire(false);
      }
      // height of tree did not increase - done
      break;
    } else {
      ndbrequire(false);
    }
    TupLoc parentLoc = nodePtr.p->getLink(2);
    if (parentLoc == NullTupLoc) {
      jam();
      // root node - done
      break;
    }
    i = nodePtr.p->getSide();
    selectNode(signal, frag, nodePtr, parentLoc, AccHead);
  }
}

/*
 * Remove entry.
 */
void
Dbtux::treeRemove(Signal* signal, Frag& frag, TreePos treePos)
{
  TreeHead& tree = frag.m_tree;
  unsigned pos = treePos.m_pos;
  NodeHandlePtr nodePtr;
  // access full node
  selectNode(signal, frag, nodePtr, treePos.m_loc, AccFull);
  TreeEnt ent;
  // check interior node first
  if (nodePtr.p->getChilds() == 2) {
    jam();
    ndbrequire(nodePtr.p->getOccup() >= tree.m_minOccup);
    // check if no underflow
    if (nodePtr.p->getOccup() > tree.m_minOccup) {
      jam();
      nodePtr.p->popDown(signal, pos, ent);
      return;
    }
    // save current handle
    NodeHandlePtr parentPtr = nodePtr;
    // find glb node
    TupLoc childLoc = nodePtr.p->getLink(0);
    while (childLoc != NullTupLoc) {
      jam();
      selectNode(signal, frag, nodePtr, childLoc, AccHead);
      childLoc = nodePtr.p->getLink(1);
    }
    // access full node again
    accessNode(signal, frag, nodePtr, AccFull);
    // use glb max as new parent min
    ent = nodePtr.p->getEnt(nodePtr.p->getOccup() - 1);
    parentPtr.p->popUp(signal, pos, ent);
    // set up to remove glb max
    pos = nodePtr.p->getOccup() - 1;
    // fall thru to next case
  }
  // remove the element
  nodePtr.p->popDown(signal, pos, ent);
  ndbrequire(nodePtr.p->getChilds() <= 1);
  // handle half-leaf
  for (unsigned i = 0; i <= 1; i++) {
    jam();
    TupLoc childLoc = nodePtr.p->getLink(i);
    if (childLoc != NullTupLoc) {
      // move to child
      selectNode(signal, frag, nodePtr, childLoc, AccFull);
      // balance of half-leaf parent requires child to be leaf
      break;
    }
  }
  ndbrequire(nodePtr.p->getChilds() == 0);
  // get parent if any
  TupLoc parentLoc = nodePtr.p->getLink(2);
  NodeHandlePtr parentPtr;
  unsigned i = nodePtr.p->getSide();
  // move all that fits into parent
  if (parentLoc != NullTupLoc) {
    jam();
    selectNode(signal, frag, parentPtr, nodePtr.p->getLink(2), AccFull);
    parentPtr.p->slide(signal, nodePtr, i);
    // fall thru to next case
  }
  // non-empty leaf
  if (nodePtr.p->getOccup() >= 1) {
    jam();
    return;
  }
  // remove empty leaf
  deleteNode(signal, frag, nodePtr);
  if (parentLoc == NullTupLoc) {
    jam();
    // tree is now empty
    tree.m_root = NullTupLoc;
    return;
  }
  nodePtr = parentPtr;
  nodePtr.p->setLink(i, NullTupLoc);
#ifdef dbtux_min_occup_less_max_occup
  // check if we created a half-leaf
  if (nodePtr.p->getBalance() == 0) {
    jam();
    // move entries from the other child
    TupLoc childLoc = nodePtr.p->getLink(1 - i);
    NodeHandlePtr childPtr;
    selectNode(signal, frag, childPtr, childLoc, AccFull);
    nodePtr.p->slide(signal, childPtr, 1 - i);
    if (childPtr.p->getOccup() == 0) {
      jam();
      deleteNode(signal, frag, childPtr);
      nodePtr.p->setLink(1 - i, NullTupLoc);
      // we are balanced again but our parent balance changes by -1
      parentLoc = nodePtr.p->getLink(2);
      if (parentLoc == NullTupLoc) {
        jam();
        return;
      }
      // fix side and become parent
      i = nodePtr.p->getSide();
      selectNode(signal, frag, nodePtr, parentLoc, AccHead);
    }
  }
#endif
  // re-balance tree at each node
  while (true) {
    // height of subtree i has decreased by 1
    int j = (i == 0 ? -1 : +1);
    int b = nodePtr.p->getBalance();
    if (b == 0) {
      // perfectly balanced
      jam();
      nodePtr.p->setBalance(-j);
      // height of tree did not change - done
      return;
    } else if (b == j) {
      // height of longer subtree has decreased
      jam();
      nodePtr.p->setBalance(0);
      // height change propagates up
    } else if (b == -j) {
      // height of shorter subtree has decreased
      jam();
      NodeHandlePtr childPtr;
      // child on the other side
      selectNode(signal, frag, childPtr, nodePtr.p->getLink(1 - i), AccHead);
      int b2 = childPtr.p->getBalance();
      if (b2 == b) {
        jam();
        treeRotateSingle(signal, frag, nodePtr, 1 - i);
        // height of tree decreased and propagates up
      } else if (b2 == -b) {
        jam();
        treeRotateDouble(signal, frag, nodePtr, 1 - i);
        // height of tree decreased and propagates up
      } else {
        jam();
        treeRotateSingle(signal, frag, nodePtr, 1 - i);
        // height of tree did not change - done
        return;
      }
    } else {
      ndbrequire(false);
    }
    TupLoc parentLoc = nodePtr.p->getLink(2);
    if (parentLoc == NullTupLoc) {
      jam();
      // root node - done
      return;
    }
    i = nodePtr.p->getSide();
    selectNode(signal, frag, nodePtr, parentLoc, AccHead);
  }
}

/*
 * Single rotation about node 5.  One of LL (i=0) or RR (i=1).
 *
 *           0                   0
 *           |                   |
 *           5       ==>         3
 *         /   \               /   \
 *        3     6             2     5
 *       / \                 /     / \
 *      2   4               1     4   6
 *     /
 *    1
 *
 * In this change 5,3 and 2 must always be there. 0, 1, 2, 4 and 6 are
 * all optional. If 4 are there it changes side.
*/
void
Dbtux::treeRotateSingle(Signal* signal,
                        Frag& frag,
                        NodeHandlePtr& nodePtr,
                        unsigned i)
{
  ndbrequire(i <= 1);
  /*
  5 is the old top node that have been unbalanced due to an insert or
  delete. The balance is still the old balance before the update.
  Verify that n5Bal is 1 if RR rotate and -1 if LL rotate.
  */
  NodeHandlePtr n5Ptr = nodePtr;
  const TupLoc n5Loc = n5Ptr.p->m_loc;
  const int n5Bal = n5Ptr.p->getBalance();
  const int n5side = n5Ptr.p->getSide();
  ndbrequire(n5Bal + (1 - i) == i);
  /*
  3 is the new root of this part of the tree which is to swap place with
  node 5. For an insert to cause this it must have the same balance as 5.
  For deletes it can have the balance 0.
  */
  TupLoc n3Loc = n5Ptr.p->getLink(i);
  NodeHandlePtr n3Ptr;
  selectNode(signal, frag, n3Ptr, n3Loc, AccHead);
  const int n3Bal = n3Ptr.p->getBalance();
  /*
  2 must always be there but is not changed. Thus we mereley check that it
  exists.
  */
  ndbrequire(n3Ptr.p->getLink(i) != NullTupLoc);
  /*
  4 is not necessarily there but if it is there it will move from one
  side of 3 to the other side of 5. For LL it moves from the right side
  to the left side and for RR it moves from the left side to the right
  side. This means that it also changes parent from 3 to 5.
  */
  TupLoc n4Loc = n3Ptr.p->getLink(1 - i);
  NodeHandlePtr n4Ptr;
  if (n4Loc != NullTupLoc) {
    jam();
    selectNode(signal, frag, n4Ptr, n4Loc, AccHead);
    ndbrequire(n4Ptr.p->getSide() == (1 - i) &&
               n4Ptr.p->getLink(2) == n3Loc);
    n4Ptr.p->setSide(i);
    n4Ptr.p->setLink(2, n5Loc);
  }//if

  /*
  Retrieve the address of 5's parent before it is destroyed
  */
  TupLoc n0Loc = n5Ptr.p->getLink(2);

  /*
  The next step is to perform the rotation. 3 will inherit 5's parent 
  and side. 5 will become a child of 3 on the right side for LL and on
  the left side for RR.
  5 will get 3 as the parent. It will get 4 as a child and it will be
  on the right side of 3 for LL and left side of 3 for RR.
  The final step of the rotate is to check whether 5 originally had any
  parent. If it had not then 3 is the new root node.
  We will also verify some preconditions for the change to occur.
  1. 3 must have had 5 as parent before the change.
  2. 3's side is left for LL and right for RR before change.
  */
  ndbrequire(n3Ptr.p->getLink(2) == n5Loc);
  ndbrequire(n3Ptr.p->getSide() == i);
  n3Ptr.p->setLink(1 - i, n5Loc);
  n3Ptr.p->setLink(2, n0Loc);
  n3Ptr.p->setSide(n5side);
  n5Ptr.p->setLink(i, n4Loc);
  n5Ptr.p->setLink(2, n3Loc);
  n5Ptr.p->setSide(1 - i);
  if (n0Loc != NullTupLoc) {
    jam();
    NodeHandlePtr n0Ptr;
    selectNode(signal, frag, n0Ptr, n0Loc, AccHead);
    n0Ptr.p->setLink(n5side, n3Loc);
  } else {
    jam();
    frag.m_tree.m_root = n3Loc;
  }//if
  /* The final step of the change is to update the balance of 3 and
  5 that changed places. There are two cases here. The first case is
  when 3 unbalanced in the same direction by an insert or a delete.
  In this case the changes will make the tree balanced again for both
  3 and 5.
  The second case only occurs at deletes. In this case 3 starts out
  balanced. In the figure above this could occur if 4 starts out with
  a right node and the rotate is triggered by a delete of 6's only child.
  In this case 5 will change balance but still be unbalanced and 3 will
  be unbalanced in the opposite direction of 5.
  */
  if (n3Bal == n5Bal) {
    jam();
    n3Ptr.p->setBalance(0);
    n5Ptr.p->setBalance(0);
  } else if (n3Bal == 0) {
    jam();
    n3Ptr.p->setBalance(-n5Bal);
    n5Ptr.p->setBalance(n5Bal);
  } else {
    ndbrequire(false);
  }//if
  /*
  Set nodePtr to 3 as return parameter for enabling caller to continue
  traversing the tree.
  */
  nodePtr = n3Ptr;
}

/*
 * Double rotation about node 6.  One of LR (i=0) or RL (i=1).
 *
 *        0                  0
 *        |                  |
 *        6      ==>         4
 *       / \               /   \
 *      2   7             2     6
 *     / \               / \   / \
 *    1   4             1   3 5   7
 *       / \
 *      3   5
 *
 * In this change 6, 2 and 4 must be there, all others are optional.
 * We will start by proving a Lemma.
 * Lemma:
 *   The height of the sub-trees 1 and 7 and the maximum height of the
 *   threes from 3 and 5 are all the same.
 * Proof:
 *   maxheight(3,5) is defined as the maximum height of 3 and 5.
 *   If height(7) > maxheight(3,5) then the AVL condition is ok and we
 *   don't need to perform a rotation.
 *   If height(7) < maxheight(3,5) then the balance of 6 would be at least
 *   -3 which cannot happen in an AVL tree even before a rotation.
 *   Thus we conclude that height(7) == maxheight(3,5)
 *
 *   The next step is to prove that the height of 1 is equal to maxheight(3,5).
 *   If height(1) - 1 > maxheight(3,5) then we would have
 *   balance in 6 equal to -3 at least which cannot happen in an AVL-tree.
 *   If height(1) - 1 = maxheight(3,5) then we should have solved the
 *   unbalance with a single rotate and not with a double rotate.
 *   If height(1) + 1 = maxheight(3,5) then we would be doing a rotate
 *   with node 2 as the root of the rotation.
 *   If height(1) + k = maxheight(3,5) where k >= 2 then the tree could not have
 *   been an AVL-tree before the insert or delete.
 *   Thus we conclude that height(1) = maxheight(3,5)
 *
 *   Thus we conclude that height(1) = maxheight(3,5) = height(7).
 *
 * Observation:
 *   The balance of node 4 before the rotation can be any (-1, 0, +1).
 *
 * The following changes are needed:
 * Node 6:
 * 1) Changes parent from 0 -> 4
 * 2) 1 - i link stays the same
 * 3) i side link is derived from 1 - i side link from 4
 * 4) Side is set to 1 - i
 * 5) Balance change:
 *    If balance(4) == 0 then balance(6) = 0
 *      since height(3) = height(5) = maxheight(3,5) = height(7)
 *    If balance(4) == +1 then balance(6) = 0 
 *      since height(5) = maxheight(3,5) = height(7)
 *    If balance(4) == -1 then balance(6) = 1
 *      since height(5) + 1 = maxheight(3,5) = height(7)
 *
 * Node 2:
 * 1) Changes parent from 6 -> 4
 * 2) i side link stays the same
 * 3) 1 - i side link is derived from i side link of 4
 * 4) Side is set to i (thus not changed)
 * 5) Balance change:
 *    If balance(4) == 0 then balance(2) = 0
 *      since height(3) = height(5) = maxheight(3,5) = height(1)
 *    If balance(4) == -1 then balance(2) = 0 
 *      since height(3) = maxheight(3,5) = height(1)
 *    If balance(4) == +1 then balance(6) = 1
 *      since height(3) + 1 = maxheight(3,5) = height(1)
 *
 * Node 4:
 * 1) Inherits parent from 6
 * 2) i side link is 2
 * 3) 1 - i side link is 6
 * 4) Side is inherited from 6
 * 5) Balance(4) = 0 independent of previous balance
 *    Proof:
 *      If height(1) = 0 then only 2, 4 and 6 are involved and then it is
 *      trivially true.
 *      If height(1) >= 1 then we are sure that 1 and 7 exist with the same
 *      height and that if 3 and 5 exist they are of the same height as 1 and
 *      7 and thus we know that 4 is balanced since newheight(2) = newheight(6).
 *
 * If Node 3 exists:
 * 1) Change parent from 4 to 2
 * 2) Change side from i to 1 - i
 *
 * If Node 5 exists:
 * 1) Change parent from 4 to 6
 * 2) Change side from 1 - i to i
 * 
 * If Node 0 exists:
 * 1) previous link to 6 is replaced by link to 4 on proper side
 *
 * Node 1 and 7 needs no changes at all.
 * 
 * Some additional requires are that balance(2) = - balance(6) = -1/+1 since
 * otherwise we would do a single rotate.
 *
 * The balance(6) is -1 if i == 0 and 1 if i == 1
 *
 */
void
Dbtux::treeRotateDouble(Signal* signal, Frag& frag, NodeHandlePtr& nodePtr, unsigned i)
{
  // old top node
  NodeHandlePtr n6Ptr = nodePtr;
  const TupLoc n6Loc = n6Ptr.p->m_loc;
  // the un-updated balance
  const int n6Bal = n6Ptr.p->getBalance();
  const unsigned n6Side = n6Ptr.p->getSide();

  // level 1
  TupLoc n2Loc = n6Ptr.p->getLink(i);
  NodeHandlePtr n2Ptr;
  selectNode(signal, frag, n2Ptr, n2Loc, AccHead);
  const int n2Bal = n2Ptr.p->getBalance();

  // level 2
  TupLoc n4Loc = n2Ptr.p->getLink(1 - i);
  NodeHandlePtr n4Ptr;
  selectNode(signal, frag, n4Ptr, n4Loc, AccHead);
  const int n4Bal = n4Ptr.p->getBalance();

  ndbrequire(i <= 1);
  ndbrequire(n6Bal + (1 - i) == i);
  ndbrequire(n2Bal == -n6Bal);
  ndbrequire(n2Ptr.p->getLink(2) == n6Loc);
  ndbrequire(n2Ptr.p->getSide() == i);
  ndbrequire(n4Ptr.p->getLink(2) == n2Loc);

  // level 3
  TupLoc n3Loc = n4Ptr.p->getLink(i);
  TupLoc n5Loc = n4Ptr.p->getLink(1 - i);

  // fill up leaf before it becomes internal
  if (n3Loc == NullTupLoc && n5Loc == NullTupLoc) {
    jam();
    TreeHead& tree = frag.m_tree;
    accessNode(signal, frag, n2Ptr, AccFull);
    accessNode(signal, frag, n4Ptr, AccFull);
    n4Ptr.p->slide(signal, n2Ptr, i);
    // implied by rule of merging half-leaves with leaves
    ndbrequire(n4Ptr.p->getOccup() >= tree.m_minOccup);
    ndbrequire(n2Ptr.p->getOccup() != 0);
  } else {
    if (n3Loc != NullTupLoc) {
      jam();
      NodeHandlePtr n3Ptr;
      selectNode(signal, frag, n3Ptr, n3Loc, AccHead);
      n3Ptr.p->setLink(2, n2Loc);
      n3Ptr.p->setSide(1 - i);
    }
    if (n5Loc != NullTupLoc) {
      jam();
      NodeHandlePtr n5Ptr;
      selectNode(signal, frag, n5Ptr, n5Loc, AccHead);
      n5Ptr.p->setLink(2, n6Ptr.p->m_loc);
      n5Ptr.p->setSide(i);
    }
  }
  // parent
  TupLoc n0Loc = n6Ptr.p->getLink(2);
  NodeHandlePtr n0Ptr;
  // perform the rotation
  n6Ptr.p->setLink(i, n5Loc);
  n6Ptr.p->setLink(2, n4Loc);
  n6Ptr.p->setSide(1 - i);

  n2Ptr.p->setLink(1 - i, n3Loc);
  n2Ptr.p->setLink(2, n4Loc);

  n4Ptr.p->setLink(i, n2Loc);
  n4Ptr.p->setLink(1 - i, n6Loc);
  n4Ptr.p->setLink(2, n0Loc);
  n4Ptr.p->setSide(n6Side);

  if (n0Loc != NullTupLoc) {
    jam();
    selectNode(signal, frag, n0Ptr, n0Loc, AccHead);
    n0Ptr.p->setLink(n6Side, n4Loc);
  } else {
    jam();
    frag.m_tree.m_root = n4Loc;
  }
  // set balance of changed nodes
  n4Ptr.p->setBalance(0);
  if (n4Bal == 0) {
    jam();
    n2Ptr.p->setBalance(0);
    n6Ptr.p->setBalance(0);
  } else if (n4Bal == -n2Bal) {
    jam();
    n2Ptr.p->setBalance(0);
    n6Ptr.p->setBalance(n2Bal);
  } else if (n4Bal == n2Bal) {
    jam();
    n2Ptr.p->setBalance(-n2Bal);
    n6Ptr.p->setBalance(0);
  } else {
    ndbrequire(false);
  }
  // new top node
  nodePtr = n4Ptr;
}
