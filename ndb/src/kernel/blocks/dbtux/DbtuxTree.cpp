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
 * Add entry.
 */
void
Dbtux::treeAdd(Signal* signal, Frag& frag, TreePos treePos, TreeEnt ent)
{
  TreeHead& tree = frag.m_tree;
  unsigned pos = treePos.m_pos;
  NodeHandle node(frag);
  // check for empty tree
  if (treePos.m_loc == NullTupLoc) {
    jam();
    insertNode(signal, node);
    nodePushUp(signal, node, 0, ent);
    node.setSide(2);
    tree.m_root = node.m_loc;
    return;
  }
  selectNode(signal, node, treePos.m_loc);
  // check if it is bounding node
  if (pos != 0 && pos != node.getOccup()) {
    jam();
    // check if room for one more
    if (node.getOccup() < tree.m_maxOccup) {
      jam();
      nodePushUp(signal, node, pos, ent);
      return;
    }
    // returns min entry
    nodePushDown(signal, node, pos - 1, ent);
    // find position to add the removed min entry
    TupLoc childLoc = node.getLink(0);
    if (childLoc == NullTupLoc) {
      jam();
      // left child will be added
      pos = 0;
    } else {
      jam();
      // find glb node
      while (childLoc != NullTupLoc) {
        jam();
        selectNode(signal, node, childLoc);
        childLoc = node.getLink(1);
      }
      pos = node.getOccup();
    }
    // fall thru to next case
  }
  // adding new min or max
  unsigned i = (pos == 0 ? 0 : 1);
  ndbrequire(node.getLink(i) == NullTupLoc);
  // check if the half-leaf/leaf has room for one more
  if (node.getOccup() < tree.m_maxOccup) {
    jam();
    nodePushUp(signal, node, pos, ent);
    return;
  }
  // add a new node
  NodeHandle childNode(frag);
  insertNode(signal, childNode);
  nodePushUp(signal, childNode, 0, ent);
  // connect parent and child
  node.setLink(i, childNode.m_loc);
  childNode.setLink(2, node.m_loc);
  childNode.setSide(i);
  // re-balance tree at each node
  while (true) {
    // height of subtree i has increased by 1
    int j = (i == 0 ? -1 : +1);
    int b = node.getBalance();
    if (b == 0) {
      // perfectly balanced
      jam();
      node.setBalance(j);
      // height change propagates up
    } else if (b == -j) {
      // height of shorter subtree increased
      jam();
      node.setBalance(0);
      // height of tree did not change - done
      break;
    } else if (b == j) {
      // height of longer subtree increased
      jam();
      NodeHandle childNode(frag);
      selectNode(signal, childNode, node.getLink(i));
      int b2 = childNode.getBalance();
      if (b2 == b) {
        jam();
        treeRotateSingle(signal, frag, node, i);
      } else if (b2 == -b) {
        jam();
        treeRotateDouble(signal, frag, node, i);
      } else {
        // height of subtree increased so it cannot be perfectly balanced
        ndbrequire(false);
      }
      // height of tree did not increase - done
      break;
    } else {
      ndbrequire(false);
    }
    TupLoc parentLoc = node.getLink(2);
    if (parentLoc == NullTupLoc) {
      jam();
      // root node - done
      break;
    }
    i = node.getSide();
    selectNode(signal, node, parentLoc);
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
  NodeHandle node(frag);
  selectNode(signal, node, treePos.m_loc);
  TreeEnt ent;
  // check interior node first
  if (node.getChilds() == 2) {
    jam();
    ndbrequire(node.getOccup() >= tree.m_minOccup);
    // check if no underflow
    if (node.getOccup() > tree.m_minOccup) {
      jam();
      nodePopDown(signal, node, pos, ent);
      return;
    }
    // save current handle
    NodeHandle parentNode = node;
    // find glb node
    TupLoc childLoc = node.getLink(0);
    while (childLoc != NullTupLoc) {
      jam();
      selectNode(signal, node, childLoc);
      childLoc = node.getLink(1);
    }
    // use glb max as new parent min
    ent = node.getEnt(node.getOccup() - 1);
    nodePopUp(signal, parentNode, pos, ent);
    // set up to remove glb max
    pos = node.getOccup() - 1;
    // fall thru to next case
  }
  // remove the element
  nodePopDown(signal, node, pos, ent);
  ndbrequire(node.getChilds() <= 1);
  // handle half-leaf
  unsigned i;
  for (i = 0; i <= 1; i++) {
    jam();
    TupLoc childLoc = node.getLink(i);
    if (childLoc != NullTupLoc) {
      // move to child
      selectNode(signal, node, childLoc);
      // balance of half-leaf parent requires child to be leaf
      break;
    }
  }
  ndbrequire(node.getChilds() == 0);
  // get parent if any
  TupLoc parentLoc = node.getLink(2);
  NodeHandle parentNode(frag);
  i = node.getSide();
  // move all that fits into parent
  if (parentLoc != NullTupLoc) {
    jam();
    selectNode(signal, parentNode, node.getLink(2));
    nodeSlide(signal, parentNode, node, i);
    // fall thru to next case
  }
  // non-empty leaf
  if (node.getOccup() >= 1) {
    jam();
    return;
  }
  // remove empty leaf
  deleteNode(signal, node);
  if (parentLoc == NullTupLoc) {
    jam();
    // tree is now empty
    tree.m_root = NullTupLoc;
    return;
  }
  node = parentNode;
  node.setLink(i, NullTupLoc);
#ifdef dbtux_min_occup_less_max_occup
  // check if we created a half-leaf
  if (node.getBalance() == 0) {
    jam();
    // move entries from the other child
    TupLoc childLoc = node.getLink(1 - i);
    NodeHandle childNode(frag);
    selectNode(signal, childNode, childLoc);
    nodeSlide(signal, node, childNode, 1 - i);
    if (childNode.getOccup() == 0) {
      jam();
      deleteNode(signal, childNode);
      node.setLink(1 - i, NullTupLoc);
      // we are balanced again but our parent balance changes by -1
      parentLoc = node.getLink(2);
      if (parentLoc == NullTupLoc) {
        jam();
        return;
      }
      // fix side and become parent
      i = node.getSide();
      selectNode(signal, node, parentLoc);
    }
  }
#endif
  // re-balance tree at each node
  while (true) {
    // height of subtree i has decreased by 1
    int j = (i == 0 ? -1 : +1);
    int b = node.getBalance();
    if (b == 0) {
      // perfectly balanced
      jam();
      node.setBalance(-j);
      // height of tree did not change - done
      return;
    } else if (b == j) {
      // height of longer subtree has decreased
      jam();
      node.setBalance(0);
      // height change propagates up
    } else if (b == -j) {
      // height of shorter subtree has decreased
      jam();
      // child on the other side
      NodeHandle childNode(frag);
      selectNode(signal, childNode, node.getLink(1 - i));
      int b2 = childNode.getBalance();
      if (b2 == b) {
        jam();
        treeRotateSingle(signal, frag, node, 1 - i);
        // height of tree decreased and propagates up
      } else if (b2 == -b) {
        jam();
        treeRotateDouble(signal, frag, node, 1 - i);
        // height of tree decreased and propagates up
      } else {
        jam();
        treeRotateSingle(signal, frag, node, 1 - i);
        // height of tree did not change - done
        return;
      }
    } else {
      ndbrequire(false);
    }
    TupLoc parentLoc = node.getLink(2);
    if (parentLoc == NullTupLoc) {
      jam();
      // root node - done
      return;
    }
    i = node.getSide();
    selectNode(signal, node, parentLoc);
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
                        NodeHandle& node,
                        unsigned i)
{
  ndbrequire(i <= 1);
  /*
  5 is the old top node that have been unbalanced due to an insert or
  delete. The balance is still the old balance before the update.
  Verify that bal5 is 1 if RR rotate and -1 if LL rotate.
  */
  NodeHandle node5 = node;
  const TupLoc loc5 = node5.m_loc;
  const int bal5 = node5.getBalance();
  const int side5 = node5.getSide();
  ndbrequire(bal5 + (1 - i) == i);
  /*
  3 is the new root of this part of the tree which is to swap place with
  node 5. For an insert to cause this it must have the same balance as 5.
  For deletes it can have the balance 0.
  */
  TupLoc loc3 = node5.getLink(i);
  NodeHandle node3(frag);
  selectNode(signal, node3, loc3);
  const int bal3 = node3.getBalance();
  /*
  2 must always be there but is not changed. Thus we mereley check that it
  exists.
  */
  ndbrequire(node3.getLink(i) != NullTupLoc);
  /*
  4 is not necessarily there but if it is there it will move from one
  side of 3 to the other side of 5. For LL it moves from the right side
  to the left side and for RR it moves from the left side to the right
  side. This means that it also changes parent from 3 to 5.
  */
  TupLoc loc4 = node3.getLink(1 - i);
  NodeHandle node4(frag);
  if (loc4 != NullTupLoc) {
    jam();
    selectNode(signal, node4, loc4);
    ndbrequire(node4.getSide() == (1 - i) &&
               node4.getLink(2) == loc3);
    node4.setSide(i);
    node4.setLink(2, loc5);
  }//if

  /*
  Retrieve the address of 5's parent before it is destroyed
  */
  TupLoc loc0 = node5.getLink(2);

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
  ndbrequire(node3.getLink(2) == loc5);
  ndbrequire(node3.getSide() == i);
  node3.setLink(1 - i, loc5);
  node3.setLink(2, loc0);
  node3.setSide(side5);
  node5.setLink(i, loc4);
  node5.setLink(2, loc3);
  node5.setSide(1 - i);
  if (loc0 != NullTupLoc) {
    jam();
    NodeHandle node0(frag);
    selectNode(signal, node0, loc0);
    node0.setLink(side5, loc3);
  } else {
    jam();
    frag.m_tree.m_root = loc3;
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
  if (bal3 == bal5) {
    jam();
    node3.setBalance(0);
    node5.setBalance(0);
  } else if (bal3 == 0) {
    jam();
    node3.setBalance(-bal5);
    node5.setBalance(bal5);
  } else {
    ndbrequire(false);
  }//if
  /*
  Set node to 3 as return parameter for enabling caller to continue
  traversing the tree.
  */
  node = node3;
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
Dbtux::treeRotateDouble(Signal* signal, Frag& frag, NodeHandle& node, unsigned i)
{
  // old top node
  NodeHandle node6 = node;
  const TupLoc loc6 = node6.m_loc;
  // the un-updated balance
  const int bal6 = node6.getBalance();
  const unsigned side6 = node6.getSide();

  // level 1
  TupLoc loc2 = node6.getLink(i);
  NodeHandle node2(frag);
  selectNode(signal, node2, loc2);
  const int bal2 = node2.getBalance();

  // level 2
  TupLoc loc4 = node2.getLink(1 - i);
  NodeHandle node4(frag);
  selectNode(signal, node4, loc4);
  const int bal4 = node4.getBalance();

  ndbrequire(i <= 1);
  ndbrequire(bal6 + (1 - i) == i);
  ndbrequire(bal2 == -bal6);
  ndbrequire(node2.getLink(2) == loc6);
  ndbrequire(node2.getSide() == i);
  ndbrequire(node4.getLink(2) == loc2);

  // level 3
  TupLoc loc3 = node4.getLink(i);
  TupLoc loc5 = node4.getLink(1 - i);

  // fill up leaf before it becomes internal
  if (loc3 == NullTupLoc && loc5 == NullTupLoc) {
    jam();
    TreeHead& tree = frag.m_tree;
    nodeSlide(signal, node4, node2, i);
    // implied by rule of merging half-leaves with leaves
    ndbrequire(node4.getOccup() >= tree.m_minOccup);
    ndbrequire(node2.getOccup() != 0);
  } else {
    if (loc3 != NullTupLoc) {
      jam();
      NodeHandle node3(frag);
      selectNode(signal, node3, loc3);
      node3.setLink(2, loc2);
      node3.setSide(1 - i);
    }
    if (loc5 != NullTupLoc) {
      jam();
      NodeHandle node5(frag);
      selectNode(signal, node5, loc5);
      node5.setLink(2, node6.m_loc);
      node5.setSide(i);
    }
  }
  // parent
  TupLoc loc0 = node6.getLink(2);
  NodeHandle node0(frag);
  // perform the rotation
  node6.setLink(i, loc5);
  node6.setLink(2, loc4);
  node6.setSide(1 - i);

  node2.setLink(1 - i, loc3);
  node2.setLink(2, loc4);

  node4.setLink(i, loc2);
  node4.setLink(1 - i, loc6);
  node4.setLink(2, loc0);
  node4.setSide(side6);

  if (loc0 != NullTupLoc) {
    jam();
    selectNode(signal, node0, loc0);
    node0.setLink(side6, loc4);
  } else {
    jam();
    frag.m_tree.m_root = loc4;
  }
  // set balance of changed nodes
  node4.setBalance(0);
  if (bal4 == 0) {
    jam();
    node2.setBalance(0);
    node6.setBalance(0);
  } else if (bal4 == -bal2) {
    jam();
    node2.setBalance(0);
    node6.setBalance(bal2);
  } else if (bal4 == bal2) {
    jam();
    node2.setBalance(-bal2);
    node6.setBalance(0);
  } else {
    ndbrequire(false);
  }
  // new top node
  node = node4;
}
