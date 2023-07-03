/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#define DBTUX_SEARCH_CPP
#include "Dbtux.hpp"

#define JAM_FILE_ID 368


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
 *
 * Can be called by MT-build of ordered indexes.
 */
void
Dbtux::findNodeToUpdate(TuxCtx& ctx,
                        Frag& frag,
                        const KeyBoundArray& searchBound,
                        TreeEnt searchEnt,
                        NodeHandle& currNode)
{
  const Index& index = *ctx.indexPtr.p;
  const Uint32 numAttrs = index.m_numAttrs;
  const Uint32 prefAttrs = index.m_prefAttrs;
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true)
  {
    thrjamDebug(ctx.jamBuffer);
    selectNode(ctx, currNode, currNode.m_loc);
    int ret = 0;
    if (likely(prefAttrs > 0))
    {
      thrjamDebug(ctx.jamBuffer);
      KeyDataArray key_data;
      key_data.init_poai(currNode.getPref(), prefAttrs);
      ret = searchBound.cmp(&key_data, prefAttrs, true);
    }
    if (ret == 0 && prefAttrs < numAttrs)
    {
      thrjamDebug(ctx.jamBuffer);
      // read and compare all attributes
      readKeyAttrs(ctx,
                   frag,
                   currNode.getEnt(0),
                   numAttrs,
                   ctx.c_dataBuffer);
      KeyDataArray key_data;
      key_data.init_poai(ctx.c_dataBuffer, numAttrs);
      ret = searchBound.cmp(&key_data, numAttrs, true);
    }
    if (unlikely(ret == 0))
    {
      thrjamDebug(ctx.jamBuffer);
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getEnt(0));
    }
    if (ret < 0)
    {
      thrjamDebug(ctx.jamBuffer);
      const TupLoc loc = currNode.getLink(0);
      if (loc != NullTupLoc)
      {
        thrjamDebug(ctx.jamBuffer);
        // continue to left subtree
        currNode.m_loc = loc;
        continue;
      }
      if (! glbNode.isNull())
      {
        thrjamDebug(ctx.jamBuffer);
        // move up to the g.l.b
        currNode = glbNode;
      }
      break;
    }
    if (ret > 0)
    {
      thrjamDebug(ctx.jamBuffer);
      const TupLoc loc = currNode.getLink(1);
      if (loc != NullTupLoc)
      {
        thrjamDebug(ctx.jamBuffer);
        // save potential g.l.b
        glbNode = currNode;
        // continue to right subtree
        currNode.m_loc = loc;
        continue;
      }
      break;
    }
    // ret == 0
    thrjamDebug(ctx.jamBuffer);
    break;
  }
}

/*
 * Find position within the final node to add entry to.  Use binary
 * search.  Return true if ok i.e. entry to add is not a duplicate.
 *
 * Can be called from MT-build of ordered indexes.
 */
bool
Dbtux::findPosToAdd(TuxCtx& ctx,
                    Frag& frag,
                    const KeyBoundArray& searchBound,
                    TreeEnt searchEnt,
                    NodeHandle& currNode,
                    TreePos& treePos)
{
  const Index& index = *ctx.indexPtr.p;
  int lo = -1;
  int hi = (int)currNode.getOccup();
  while (hi - lo > 1)
  {
    thrjamDebug(ctx.jamBuffer);
    // hi - lo > 1 implies lo < j < hi
    int j = (hi + lo) / 2;
    // read and compare all attributes
    readKeyAttrs(ctx,
                 frag,
                 currNode.getEnt(j),
                 index.m_numAttrs,
                 ctx.c_dataBuffer);
    KeyDataArray key_data;
    Uint32 numAttrs = index.m_numAttrs;
    key_data.init_poai(ctx.c_dataBuffer, numAttrs);
    int ret = searchBound.cmp(&key_data, numAttrs, true);
    if (unlikely(ret == 0))
    {
      thrjamDebug(ctx.jamBuffer);
      // keys are equal, compare entry values
      ret = searchEnt.cmp(currNode.getEnt(j));
    }
    if (ret < 0)
    {
      thrjamDebug(ctx.jamBuffer);
      hi = j;
    }
    else if (ret > 0)
    {
      thrjamDebug(ctx.jamBuffer);
      lo = j;
    }
    else
    {
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
Dbtux::findPosToRemove(TuxCtx& ctx,
                       TreeEnt searchEnt,
                       NodeHandle& currNode,
                       TreePos& treePos)
{
  const unsigned occup = currNode.getOccup();
  for (unsigned j = 0; j < occup; j++)
  {
    thrjamDebug(ctx.jamBuffer);
    // compare only the entry
    if (searchEnt.eq(currNode.getEnt(j)))
    {
      thrjamDebug(ctx.jamBuffer);
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
 * Can be called from MT-build of ordered indexes.
 */
bool
Dbtux::searchToAdd(TuxCtx& ctx,
                   Frag& frag,
                   const KeyBoundArray& searchBound,
                   TreeEnt searchEnt,
                   TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (unlikely(currNode.m_loc == NullTupLoc))
  {
    // empty tree
    thrjam(ctx.jamBuffer);
    return true;
  }
  findNodeToUpdate(ctx, frag, searchBound, searchEnt, currNode);
  treePos.m_loc = currNode.m_loc;
  if (likely(findPosToAdd(ctx,
                          frag,
                          searchBound,
                          searchEnt,
                          currNode,
                          treePos)))
  {
    return true;
  }
  thrjam(ctx.jamBuffer);
  return false;
}

/*
 * Search for entry to remove.
 */
bool
Dbtux::searchToRemove(TuxCtx& ctx,
                      Frag& frag,
                      const KeyBoundArray& searchBound,
                      TreeEnt searchEnt,
                      TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (unlikely(currNode.m_loc == NullTupLoc))
  {
    // empty tree - failed
    thrjam(ctx.jamBuffer);
    return false;
  }
  findNodeToUpdate(ctx, frag, searchBound, searchEnt, currNode);
  treePos.m_loc = currNode.m_loc;
  if (likely(findPosToRemove(ctx,
                             searchEnt,
                             currNode,
                             treePos)))
  {
    return true;
  }
  thrjam(ctx.jamBuffer);
  return false;
}

/*
 * Search down non-empty tree for node to start scan from.  Similar to
 * findNodeToUpdate().  Direction is 0-ascending or 1-descending.
 * Search within the found node is done by caller.
 */
void
Dbtux::findNodeToScan(Frag& frag,
                      unsigned idir,
                      const KeyBoundArray& searchBound,
                      NodeHandle& currNode)
{
  const int jdir = 1 - 2 * int(idir);
  const Index& index = *c_ctx.indexPtr.p;
  const Uint32 numAttrs = searchBound.cnt();
  const Uint32 prefAttrs = min(index.m_prefAttrs, numAttrs);
  NodeHandle glbNode(frag);     // potential g.l.b of final node
  while (true)
  {
    jamDebug();
    selectNode(c_ctx, currNode, currNode.m_loc);
    int ret = 0;
    if (likely(numAttrs > 0))
    {
      if (likely(prefAttrs > 0))
      {
        jamDebug();
        KeyDataArray key_data;
        key_data.init_poai(currNode.getPref(), prefAttrs);
        ret = searchBound.cmp(&key_data, prefAttrs, false);
        // compare node prefix - result 0 implies bound is longer
      }
      if (unlikely(ret == 0))
      {
        jamDebug();
        // read and compare all attributes
        readKeyAttrs(c_ctx,
                     frag,
                     currNode.getEnt(0),
                     numAttrs,
                     c_ctx.c_dataBuffer);
        KeyDataArray key_data;
        key_data.init_poai(c_ctx.c_dataBuffer, numAttrs);
        ret = searchBound.cmp(&key_data, numAttrs, false);
      }
    }
    else
    {
      jamDebug();
      ret = (-1) * jdir;
    }
    if (ret < 0)
    {
      // bound is left of this node
      jamDebug();
      const TupLoc loc = currNode.getLink(0);
      if (loc != NullTupLoc)
      {
        jamDebug();
        // continue to left subtree
        currNode.m_loc = loc;
        continue;
      }
      if (! glbNode.isNull())
      {
        jamDebug();
        // move up to the g.l.b
        currNode = glbNode;
      }
      break;
    }
    if (likely(ret > 0))
    {
      // bound is at or right of this node
      jamDebug();
      const TupLoc loc = currNode.getLink(1);
      if (loc != NullTupLoc)
      {
        jamDebug();
        // save potential g.l.b
        glbNode = currNode;
        // continue to right subtree
        currNode.m_loc = loc;
        continue;
      }
      break;
    }
    // ret == 0 never
    ndbabort();
  }
}

/*
 * Search across final node for position to start scan from.  Use binary
 * search similar to findPosToAdd().
 */
void
Dbtux::findPosToScan(Frag& frag,
                     unsigned idir,
                     const KeyBoundArray& searchBound,
                     NodeHandle& currNode,
                     Uint32* pos)
{
  const int jdir = 1 - 2 * int(idir);
  const Uint32 numAttrs = searchBound.cnt();
  int lo = -1;
  int hi = (int)currNode.getOccup();
  while ((hi - lo) > 1)
  {
    jamDebug();
    // hi - lo > 1 implies lo < j < hi
    int j = (hi + lo) / 2;
    int ret = (-1) * jdir;
    if (likely(numAttrs != 0))
    {
      // read and compare all attributes
      readKeyAttrs(c_ctx,
                   frag,
                   currNode.getEnt(j),
                   numAttrs,
                   c_ctx.c_dataBuffer);
      KeyDataArray key_data;
      key_data.init_poai(c_ctx.c_dataBuffer, numAttrs);
      ret = searchBound.cmp(&key_data, numAttrs, false);
    }
    if (ret < 0)
    {
      jamDebug();
      hi = j;
    }
    else if (ret > 0)
    {
      jamDebug();
      lo = j;
    }
    else
    {
      // ret == 0 never
      ndbabort();
    }
  }
  // return hi pos, caller handles ascending vs descending
  *pos = hi;
}

/*
 * Search for scan start position.
 */
void
Dbtux::searchToScan(Frag& frag,
                    unsigned idir,
                    const KeyBoundArray& searchBound,
                    TreePos& treePos)
{
  const TreeHead& tree = frag.m_tree;
  NodeHandle currNode(frag);
  currNode.m_loc = tree.m_root;
  if (unlikely(currNode.m_loc == NullTupLoc))
  {
    // empty tree
    jamDebug();
    return;
  }
  findNodeToScan(frag, idir, searchBound, currNode);
  treePos.m_loc = currNode.m_loc;
  Uint32 pos;
  findPosToScan(frag, idir, searchBound, currNode, &pos);
  const unsigned occup = currNode.getOccup();
  if (idir == 0)
  {
    if (likely(pos < occup))
    {
      jamDebug();
      treePos.m_pos = pos;
      treePos.m_dir = 3;
    }
    else
    {
      // start scan after node end i.e. proceed to right child
      jamDebug();
      treePos.m_pos = Uint32(~0);
      treePos.m_dir = 5;
    }
  }
  else
  {
    if (likely(pos > 0))
    {
      jamDebug();
      // start scan from previous entry
      treePos.m_pos = pos - 1;
      treePos.m_dir = 3;
    }
    else
    {
      jamDebug();
      treePos.m_pos = Uint32(~0);
      treePos.m_dir = 0;
    }
  }
}
