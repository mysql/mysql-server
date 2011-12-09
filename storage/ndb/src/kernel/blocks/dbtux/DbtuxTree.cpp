/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUX_TREE_CPP
#include "Dbtux.hpp"

/*
 * Add entry.  Handle the case when there is room for one more.  This
 * is the common case given slack in nodes.
 */
void
Dbtux::treeAdd(TuxCtx& ctx, Frag& frag, TreePos treePos, TreeEnt ent)
{
  TreeHead& tree = frag.m_tree;
  NodeHandle node(frag);
  do {
    if (treePos.m_loc != NullTupLoc) {
      // non-empty tree
      thrjam(ctx.jamBuffer);
      selectNode(node, treePos.m_loc);
      unsigned pos = treePos.m_pos;
      if (node.getOccup() < tree.m_maxOccup) {
        // node has room
        thrjam(ctx.jamBuffer);
        nodePushUp(ctx, node, pos, ent, RNIL);
        break;
      }
      treeAddFull(ctx, frag, node, pos, ent);
      break;
    }
    thrjam(ctx.jamBuffer);
    insertNode(node);
    nodePushUp(ctx, node, 0, ent, RNIL);
    node.setSide(2);
    tree.m_root = node.m_loc;
    break;
  } while (0);
}

/*
 * Add entry when node is full.  Handle the case when there is g.l.b
 * node in left subtree with room for one more.  It will receive the min
 * entry of this node.  The min entry could be the entry to add.
 */
void
Dbtux::treeAddFull(TuxCtx& ctx, Frag& frag, NodeHandle lubNode, unsigned pos, TreeEnt ent)
{
  TreeHead& tree = frag.m_tree;
  TupLoc loc = lubNode.getLink(0);
  if (loc != NullTupLoc) {
    // find g.l.b node
    NodeHandle glbNode(frag);
    do {
      thrjam(ctx.jamBuffer);
      selectNode(glbNode, loc);
      loc = glbNode.getLink(1);
    } while (loc != NullTupLoc);
    if (glbNode.getOccup() < tree.m_maxOccup) {
      // g.l.b node has room
      thrjam(ctx.jamBuffer);
      Uint32 scanList = RNIL;
      if (pos != 0) {
        thrjam(ctx.jamBuffer);
        // add the new entry and return min entry
        nodePushDown(ctx, lubNode, pos - 1, ent, scanList);
      }
      // g.l.b node receives min entry from l.u.b node
      nodePushUp(ctx, glbNode, glbNode.getOccup(), ent, scanList);
      return;
    }
    treeAddNode(ctx, frag, lubNode, pos, ent, glbNode, 1);
    return;
  }
  treeAddNode(ctx, frag, lubNode, pos, ent, lubNode, 0);
}

/*
 * Add entry when there is no g.l.b node in left subtree or the g.l.b
 * node is full.  We must add a new left or right child node which
 * becomes the new g.l.b node.
 */
void
Dbtux::treeAddNode(TuxCtx& ctx,
                   Frag& frag, NodeHandle lubNode, unsigned pos, TreeEnt ent, NodeHandle parentNode, unsigned i)
{
  NodeHandle glbNode(frag);
  insertNode(glbNode);
  // connect parent and child
  parentNode.setLink(i, glbNode.m_loc);
  glbNode.setLink(2, parentNode.m_loc);
  glbNode.setSide(i);
  Uint32 scanList = RNIL;
  if (pos != 0) {
    thrjam(ctx.jamBuffer);
    // add the new entry and return min entry
    nodePushDown(ctx, lubNode, pos - 1, ent, scanList);
  }
  // g.l.b node receives min entry from l.u.b node
  nodePushUp(ctx, glbNode, 0, ent, scanList);
  // re-balance the tree
  treeAddRebalance(ctx, frag, parentNode, i);
}

/*
 * Re-balance tree after adding a node.  The process starts with the
 * parent of the added node.
 */
void
Dbtux::treeAddRebalance(TuxCtx & ctx, Frag& frag, NodeHandle node, unsigned i)
{
  while (true) {
    // height of subtree i has increased by 1
    int j = (i == 0 ? -1 : +1);
    int b = node.getBalance();
    if (b == 0) {
      // perfectly balanced
      thrjam(ctx.jamBuffer);
      node.setBalance(j);
      // height change propagates up
    } else if (b == -j) {
      // height of shorter subtree increased
      thrjam(ctx.jamBuffer);
      node.setBalance(0);
      // height of tree did not change - done
      break;
    } else if (b == j) {
      // height of longer subtree increased
      thrjam(ctx.jamBuffer);
      NodeHandle childNode(frag);
      selectNode(childNode, node.getLink(i));
      int b2 = childNode.getBalance();
      if (b2 == b) {
        thrjam(ctx.jamBuffer);
        treeRotateSingle(ctx, frag, node, i);
      } else if (b2 == -b) {
        thrjam(ctx.jamBuffer);
        treeRotateDouble(ctx, frag, node, i);
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
      thrjam(ctx.jamBuffer);
      // root node - done
      break;
    }
    i = node.getSide();
    selectNode(node, parentLoc);
  }
}

/*
 * Remove entry.  Optimize for nodes with slack.  Handle the case when
 * there is no underflow i.e. occupancy remains at least minOccup.  For
 * interior nodes this is a requirement.  For others it means that we do
 * not need to consider merge of semi-leaf and leaf.
 */
void
Dbtux::treeRemove(Frag& frag, TreePos treePos)
{
  TreeHead& tree = frag.m_tree;
  unsigned pos = treePos.m_pos;
  NodeHandle node(frag);
  selectNode(node, treePos.m_loc);
  TreeEnt ent;
  do {
    if (node.getOccup() > tree.m_minOccup) {
      // no underflow in any node type
      jam();
      nodePopDown(c_ctx, node, pos, ent, 0);
      break;
    }
    if (node.getChilds() == 2) {
      // underflow in interior node
      jam();
      treeRemoveInner(frag, node, pos);
      break;
    }
    // remove entry in semi/leaf
    nodePopDown(c_ctx, node, pos, ent, 0);
    if (node.getLink(0) != NullTupLoc) {
      jam();
      treeRemoveSemi(frag, node, 0);
      break;
    }
    if (node.getLink(1) != NullTupLoc) {
      jam();
      treeRemoveSemi(frag, node, 1);
      break;
    }
    treeRemoveLeaf(frag, node);
    break;
  } while (0);
}

/*
 * Remove entry when interior node underflows.  There is g.l.b node in
 * left subtree to borrow an entry from.  The max entry of the g.l.b
 * node becomes the min entry of this node.
 */
void
Dbtux::treeRemoveInner(Frag& frag, NodeHandle lubNode, unsigned pos)
{
  TreeEnt ent;
  // find g.l.b node
  NodeHandle glbNode(frag);
  TupLoc loc = lubNode.getLink(0);
  do {
    jam();
    selectNode(glbNode, loc);
    loc = glbNode.getLink(1);
  } while (loc != NullTupLoc);
  // borrow max entry from semi/leaf
  Uint32 scanList = RNIL;
  nodePopDown(c_ctx, glbNode, glbNode.getOccup() - 1, ent, &scanList);
  // g.l.b may be empty now
  // a descending scan may try to enter the empty g.l.b
  // we prevent this in scanNext
  nodePopUp(c_ctx, lubNode, pos, ent, scanList);
  if (glbNode.getLink(0) != NullTupLoc) {
    jam();
    treeRemoveSemi(frag, glbNode, 0);
    return;
  }
  treeRemoveLeaf(frag, glbNode);
}

/*
 * Handle semi-leaf after removing an entry.  Move entries from leaf to
 * semi-leaf to bring semi-leaf occupancy above minOccup, if possible.
 * The leaf may become empty.
 */
void
Dbtux::treeRemoveSemi(Frag& frag, NodeHandle semiNode, unsigned i)
{
  TreeHead& tree = frag.m_tree;
  ndbrequire(semiNode.getChilds() < 2);
  TupLoc leafLoc = semiNode.getLink(i);
  NodeHandle leafNode(frag);
  selectNode(leafNode, leafLoc);
  if (semiNode.getOccup() < tree.m_minOccup) {
    jam();
    unsigned cnt = min(leafNode.getOccup(), tree.m_minOccup - semiNode.getOccup());
    nodeSlide(c_ctx, semiNode, leafNode, cnt, i);
    if (leafNode.getOccup() == 0) {
      // remove empty leaf
      jam();
      treeRemoveNode(frag, leafNode);
    }
  }
}

/*
 * Handle leaf after removing an entry.  If parent is semi-leaf, move
 * entries to it as in the semi-leaf case.  If parent is interior node,
 * do nothing.
 */
void
Dbtux::treeRemoveLeaf(Frag& frag, NodeHandle leafNode)
{
  TreeHead& tree = frag.m_tree;
  TupLoc parentLoc = leafNode.getLink(2);
  if (parentLoc != NullTupLoc) {
    jam();
    NodeHandle parentNode(frag);
    selectNode(parentNode, parentLoc);
    unsigned i = leafNode.getSide();
    if (parentNode.getLink(1 - i) == NullTupLoc) {
      // parent is semi-leaf
      jam();
      if (parentNode.getOccup() < tree.m_minOccup) {
        jam();
        unsigned cnt = min(leafNode.getOccup(), tree.m_minOccup - parentNode.getOccup());
        nodeSlide(c_ctx, parentNode, leafNode, cnt, i);
      }
    }
  }
  if (leafNode.getOccup() == 0) {
    jam();
    // remove empty leaf
    treeRemoveNode(frag, leafNode);
  }
}

/*
 * Remove empty leaf.
 */
void
Dbtux::treeRemoveNode(Frag& frag, NodeHandle leafNode)
{
  TreeHead& tree = frag.m_tree;
  ndbrequire(leafNode.getChilds() == 0);
  TupLoc parentLoc = leafNode.getLink(2);
  unsigned i = leafNode.getSide();
  deleteNode(leafNode);
  if (parentLoc != NullTupLoc) {
    jam();
    NodeHandle parentNode(frag);
    selectNode(parentNode, parentLoc);
    parentNode.setLink(i, NullTupLoc);
    // re-balance the tree
    treeRemoveRebalance(frag, parentNode, i);
    return;
  }
  // tree is now empty
  tree.m_root = NullTupLoc;
  // free even the pre-allocated node
  freePreallocatedNode(frag);
}

/*
 * Re-balance tree after removing a node.  The process starts with the
 * parent of the removed node.
 */
void
Dbtux::treeRemoveRebalance(Frag& frag, NodeHandle node, unsigned i)
{
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
      selectNode(childNode, node.getLink(1 - i));
      int b2 = childNode.getBalance();
      if (b2 == b) {
        jam();
        treeRotateSingle(c_ctx, frag, node, 1 - i);
        // height of tree decreased and propagates up
      } else if (b2 == -b) {
        jam();
        treeRotateDouble(c_ctx, frag, node, 1 - i);
        // height of tree decreased and propagates up
      } else {
        jam();
        treeRotateSingle(c_ctx, frag, node, 1 - i);
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
    selectNode(node, parentLoc);
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
Dbtux::treeRotateSingle(TuxCtx& ctx, Frag& frag, NodeHandle& node, unsigned i)
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
  selectNode(node3, loc3);
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
    thrjam(ctx.jamBuffer);
    selectNode(node4, loc4);
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
    thrjam(ctx.jamBuffer);
    NodeHandle node0(frag);
    selectNode(node0, loc0);
    node0.setLink(side5, loc3);
  } else {
    thrjam(ctx.jamBuffer);
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
    thrjam(ctx.jamBuffer);
    node3.setBalance(0);
    node5.setBalance(0);
  } else if (bal3 == 0) {
    thrjam(ctx.jamBuffer);
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
Dbtux::treeRotateDouble(TuxCtx& ctx, Frag& frag, NodeHandle& node, unsigned i)
{
  TreeHead& tree = frag.m_tree;

  // old top node
  NodeHandle node6 = node;
  const TupLoc loc6 = node6.m_loc;
  // the un-updated balance
  const int bal6 = node6.getBalance();
  const unsigned side6 = node6.getSide();

  // level 1
  TupLoc loc2 = node6.getLink(i);
  NodeHandle node2(frag);
  selectNode(node2, loc2);
  const int bal2 = node2.getBalance();

  // level 2
  TupLoc loc4 = node2.getLink(1 - i);
  NodeHandle node4(frag);
  selectNode(node4, loc4);
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
    thrjam(ctx.jamBuffer);
    if (node4.getOccup() < tree.m_minOccup) {
      thrjam(ctx.jamBuffer);
      unsigned cnt = tree.m_minOccup - node4.getOccup();
      ndbrequire(cnt < node2.getOccup());
      nodeSlide(ctx, node4, node2, cnt, i);
      ndbrequire(node4.getOccup() >= tree.m_minOccup);
      ndbrequire(node2.getOccup() != 0);
    }
  } else {
    if (loc3 != NullTupLoc) {
      thrjam(ctx.jamBuffer);
      NodeHandle node3(frag);
      selectNode(node3, loc3);
      node3.setLink(2, loc2);
      node3.setSide(1 - i);
    }
    if (loc5 != NullTupLoc) {
      thrjam(ctx.jamBuffer);
      NodeHandle node5(frag);
      selectNode(node5, loc5);
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
    thrjam(ctx.jamBuffer);
    selectNode(node0, loc0);
    node0.setLink(side6, loc4);
  } else {
    thrjam(ctx.jamBuffer);
    frag.m_tree.m_root = loc4;
  }
  // set balance of changed nodes
  node4.setBalance(0);
  if (bal4 == 0) {
    thrjam(ctx.jamBuffer);
    node2.setBalance(0);
    node6.setBalance(0);
  } else if (bal4 == -bal2) {
    thrjam(ctx.jamBuffer);
    node2.setBalance(0);
    node6.setBalance(bal2);
  } else if (bal4 == bal2) {
    thrjam(ctx.jamBuffer);
    node2.setBalance(-bal2);
    node6.setBalance(0);
  } else {
    ndbrequire(false);
  }
  // new top node
  node = node4;
}
