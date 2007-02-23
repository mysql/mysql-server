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

#define DBTUX_STAT_CPP
#include "Dbtux.hpp"

void
Dbtux::execREAD_PSEUDO_REQ(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  if (signal->theData[1] == AttributeHeader::RECORDS_IN_RANGE) {
    jam();
    statRecordsInRange(scanPtr, &signal->theData[0]);
  } else {
    ndbassert(false);
  }
}

/*
 * Estimate entries in range.  Scan is at first entry.  Search for last
 * entry i.e. start of descending scan.  Use the 2 positions to estimate
 * entries before and after the range.  Finally get entries in range by
 * subtracting from total.  Errors come from imperfectly balanced tree
 * and from uncommitted entries which differ only in tuple version.
 *
 * Returns 4 Uint32 values: 0) total entries 1) in range 2) before range
 * 3) after range.  1-3) are estimates and need not add up to 0).
 */
void
Dbtux::statRecordsInRange(ScanOpPtr scanPtr, Uint32* out)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  TreeHead& tree = frag.m_tree;
  // get first and last position
  TreePos pos1 = scan.m_scanPos;
  TreePos pos2;
  { // as in scanFirst()
    setKeyAttrs(frag);
    const unsigned idir = 1;
    const ScanBound& bound = *scan.m_bound[idir];
    ScanBoundIterator iter;
    bound.first(iter);
    for (unsigned j = 0; j < bound.getSize(); j++) {
      jam();
      c_dataBuffer[j] = *iter.data;
      bound.next(iter);
    }
    searchToScan(frag, c_dataBuffer, scan.m_boundCnt[idir], true, pos2);
    // committed read (same timeslice) and range not empty
    ndbrequire(pos2.m_loc != NullTupLoc);
  }
  out[0] = frag.m_tree.m_entryCount;
  out[2] = getEntriesBeforeOrAfter(frag, pos1, 0);
  out[3] = getEntriesBeforeOrAfter(frag, pos2, 1);
  if (pos1.m_loc == pos2.m_loc) {
    ndbrequire(pos2.m_pos >= pos1.m_pos);
    out[1] = pos2.m_pos - pos1.m_pos + 1;
  } else {
    Uint32 rem = out[2] + out[3];
    if (out[0] > rem) {
      out[1] = out[0] - rem;
    } else {
      // random guess one node apart
      out[1] = tree.m_maxOccup;
    }
  }
}

/*
 * Estimate number of entries strictly before or after given position.
 * Each branch to right direction wins parent node and the subtree on
 * the other side.  Subtree entries is estimated from depth and total
 * entries by assuming that the tree is perfectly balanced.
 */
Uint32
Dbtux::getEntriesBeforeOrAfter(Frag& frag, TreePos pos, unsigned idir)
{
  NodeHandle node(frag);
  selectNode(node, pos.m_loc);
  Uint16 path[MaxTreeDepth + 1];
  unsigned depth = getPathToNode(node, path);
  ndbrequire(depth != 0 && depth <= MaxTreeDepth);
  TreeHead& tree = frag.m_tree;
  Uint32 cnt = 0;
  Uint32 tot = tree.m_entryCount;
  unsigned i = 0;
  // contribution from levels above
  while (i + 1 < depth) {
    unsigned occup2 = (path[i] >> 8);
    unsigned side = (path[i + 1] & 0xFF);
    // subtree of this node has about half the entries
    tot = tot >= occup2 ? (tot - occup2) / 2 : 0;
    // branch to other side wins parent and a subtree
    if (side != idir) {
      cnt += occup2;
      cnt += tot;
    }
    i++;
  }
  // contribution from this node
  unsigned occup = (path[i] >> 8);
  ndbrequire(pos.m_pos < occup);
  if (idir == 0) {
    if (pos.m_pos != 0)
      cnt += pos.m_pos - 1;
  } else {
    cnt += occup - (pos.m_pos + 1);
  }
  // contribution from levels below
  tot = tot >= occup ? (tot - occup) / 2 : 0;
  cnt += tot;
  return cnt;
}

/*
 * Construct path to given node.  Returns depth.  Root node has path
 * 2 and depth 1.  In general the path is 2{0,1}* where 0,1 is the side
 * (left,right branch).  In addition the occupancy of each node is
 * returned in the upper 8 bits.
 */
unsigned
Dbtux::getPathToNode(NodeHandle node, Uint16* path)
{
  TupLoc loc = node.m_loc;
  unsigned i = MaxTreeDepth;
  while (loc != NullTupLoc) {
    jam();
    selectNode(node, loc);
    path[i] = node.getSide() | (node.getOccup() << 8);
    loc = node.getLink(2);
    ndbrequire(i != 0);
    i--;
  }
  unsigned depth = MaxTreeDepth - i;
  unsigned j = 0;
  while (j < depth) {
    path[j] = path[i + 1 + j];
    j++;
  }
  path[j] = 0xFFFF; // catch bug
  return depth;
}
