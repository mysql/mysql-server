/*
   Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUX_STAT_CPP
#include "Dbtux.hpp"
#include <math.h>

#define JAM_FILE_ID 367


// debug note: uses new-style debug macro "D" unlike rest of DBTUX
// there is no filtering feature (yet) like "DebugStat"

void
Dbtux::execREAD_PSEUDO_REQ(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  StatOpPtr statPtr;
  statPtr.i = scanPtr.p->m_statOpPtrI;

  Uint32 attrId = signal->theData[1];
  Uint32* out = &signal->theData[0];

  switch (attrId) {
  case AttributeHeader::RECORDS_IN_RANGE:
    jam();
    ndbrequire(statPtr.i == RNIL);
    statRecordsInRange(scanPtr, out);
    break;
  case AttributeHeader::INDEX_STAT_KEY:
    jam();
    ndbrequire(statPtr.i != RNIL);
    c_statOpPool.getPtr(statPtr);
    statScanReadKey(statPtr, out);
    break;
  case AttributeHeader::INDEX_STAT_VALUE:
    jam();
    ndbrequire(statPtr.i != RNIL);
    c_statOpPool.getPtr(statPtr);
    statScanReadValue(statPtr, out);
    break;
  default:
    ndbabort();
  }
}

// RECORDS_IN_RANGE

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
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  TreeHead& tree = frag.m_tree;
  // get first and last position
  TreePos pos1 = scan.m_scanPos;
  TreePos pos2;
  { // as in scanFirst()
    const unsigned idir = 1;
    const ScanBound& scanBound = scan.m_scanBound[idir];
    KeyDataC searchBoundData(index.m_keySpec, true);
    KeyBoundC searchBound(searchBoundData);
    unpackBound(c_ctx, scanBound, searchBound);
    searchToScan(frag, idir, searchBound, pos2);
    // committed read (same timeslice) and range not empty
    ndbrequire(pos2.m_loc != NullTupLoc);
  }
  // wl4124_todo change all to Uint64 if ever needed (unlikely)
  out[0] = (Uint32)frag.m_entryCount;
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
  // compiler warning unused: TreeHead& tree = frag.m_tree;
  Uint32 cnt = 0;
  Uint32 tot = (Uint32)frag.m_entryCount;
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

// stats scan

// windows has no log2
static double
tux_log2(double x)
{
  return ::log(x) / ::log((double)2.0);
}

int
Dbtux::statScanInit(StatOpPtr statPtr, const Uint32* data, Uint32 len,
                    Uint32* usedLen)
{
  StatOp& stat = *statPtr.p;
  ScanOp& scan = *c_scanOpPool.getPtr(stat.m_scanOpPtrI);
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  const Index& index = *c_indexPool.getPtr(scan.m_indexId);
  D("statScanInit");

  // options
  stat.m_saveSize = c_indexStatSaveSize;
  stat.m_saveScale = c_indexStatSaveScale;
  Uint32 offset = 0;
  while (offset + 1 <= len)
  {
    const Uint32 type = data[offset];
    const Uint32 value = data[offset + 1];
    switch (type) {
    case TuxBoundInfo::StatSaveSize:
      jam();
      stat.m_saveSize = value;
      break;
    case TuxBoundInfo::StatSaveScale:
      jam();
      stat.m_saveScale = value;
      break;
    default:
      jam();
      scan.m_errorCode = TuxBoundInfo::InvalidBounds;
      return -1;
    }
    offset += 2;
  }
  *usedLen = offset;

  // average key bytes as stored in stats
  Uint32 avgKeyBytes = 0;
  if (frag.m_entryCount != 0)
  {
    avgKeyBytes = (Uint32)(frag.m_entryBytes / frag.m_entryCount);
    if (avgKeyBytes > stat.m_keySpec.get_max_data_len(false))
      avgKeyBytes = stat.m_keySpec.get_max_data_len(false);
  }

  // compute batch size - see wl4124.txt
  {
    double a = stat.m_saveSize;
    double b = stat.m_saveScale;
    double c = avgKeyBytes;
    double d = index.m_numAttrs;
    double e =  c + (1 + d) * 4; // approx size of one sample
    double f = (double)frag.m_entryCount;
    double g = f * e; // max possible sample bytes
    if (g < 1.0)
      g = 1.0;
    double h = 1 + 0.01 * b * tux_log2(g); // scale factor
    double i = a * h; // sample bytes allowed
    double j = i / e; // sample count
    double k = f / j; // sampling frequency
    if (k < 1.0)
      k = 1.0;
    double l = e * f / k; // estimated sample bytes

    stat.m_batchSize = (Uint32)(k + 0.5);
    stat.m_estBytes = (Uint32)(l + 0.5);
    ndbrequire(stat.m_batchSize != 0);
    D("computed batch size" << V(stat));
  }

  // key spec is already defined as ref to index key spec
  stat.m_keyCount = index.m_numAttrs;
  stat.m_valueCount = 1 + stat.m_keyCount;
  stat.m_keyData1.reset();
  stat.m_keyData2.reset();

  // define value spec
  stat.m_valueCount = 1 + stat.m_keyCount;
  NdbPack::Spec& valueSpec = stat.m_valueSpec;
  valueSpec.reset();
  {
    NdbPack::Type type(NDB_TYPE_UNSIGNED, 4, false, 0);
    int ret = valueSpec.add(type, stat.m_valueCount);
    ndbrequire(ret == 0);
  }

  return 0;
}

int
Dbtux::statScanAddRow(StatOpPtr statPtr, TreeEnt ent)
{
  StatOp& stat = *statPtr.p;
  ScanOp& scan = *c_scanOpPool.getPtr(stat.m_scanOpPtrI);
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  D("statScanAddRow" << V(stat));

  KeyData& keyData1 = stat.m_keyData1;
  KeyData& keyData2 = stat.m_keyData2;
  StatOp::Value& value1 = stat.m_value1;
  StatOp::Value& value2 = stat.m_value2;

  stat.m_rowCount++;
  stat.m_batchCurr++;
  const bool firstRow = (stat.m_rowCount == 1);
  int ret;

  // save previous value
  if (!firstRow) {
    ret = keyData1.copy(keyData2);
    ndbrequire(ret == 0);
    value1 = value2;
  }

  // read current entry key
  readKeyAttrs(c_ctx, frag, ent, keyData2, stat.m_keyCount);

  // calculate new values
  value2.m_rir = stat.m_rowCount;
  if (firstRow)
  {
    for (Uint32 i = 0; i < stat.m_keyCount; i++)
      value2.m_unq[i] = 1;
    stat.m_keyChange = false;
  }
  else
  {
    // how many initial attrs are equal
    Uint32 num_eq;
    int res = keyData1.cmp(keyData2, stat.m_keyCount, num_eq);
    ndbrequire(res <= 0);
    stat.m_keyChange = (res != 0);

    if (stat.m_keyChange)
    {
      ndbrequire(num_eq < stat.m_keyCount);
      value2.m_unq[num_eq]++;
      // propagate down
      for (Uint32 i = num_eq + 1; i < stat.m_keyCount; i++)
        value2.m_unq[i]++;
    }
  }

  // always report last index entry
  bool lastEntry = false;
  do
  {
    NodeHandle node(frag);
    TreePos pos = scan.m_scanPos;
    selectNode(node, pos.m_loc);
    // more entries in this node
    const unsigned occup = node.getOccup();
    // funny cast to avoid signed vs unsigned warning
    if (pos.m_dir == 3 && pos.m_pos + (unsigned)1 < occup)
    {
      jam();
      break;
    }
    // can continue to right sub-tree
    if (node.getLink(1) != NullTupLoc)
    {
      jam();
      break;
    }
    // while child on right
    while (node.getSide() == 1)
    {
      jam();
      TupLoc loc = node.getLink(2);
      selectNode(node, loc);
    }
    // did not reach root
    if (node.getSide() != 2)
    {
      jam();
      break;
    }
    lastEntry = true;
  }
  while (0);

  stat.m_usePrev = true;
  if (lastEntry)
  {
    jam();
    stat.m_usePrev = false;
    return 1;
  }
  if (stat.m_batchCurr >= stat.m_batchSize && stat.m_keyChange)
  {
    jam();
    stat.m_batchCurr = 0;
    return 1;
  }
  /* Take a break to avoid problems with a long stretch of equal keys */
  const Uint32 MaxAddRowsWithoutBreak = 16;
  if (stat.m_rowCount % MaxAddRowsWithoutBreak == 0)
  {
    jam();
    D("Taking a break from stat scan");
    return 2; // Take a break
  }

  /* Iterate to next index entry */
  return 0;
}

void
Dbtux::statScanReadKey(StatOpPtr statPtr, Uint32* out)
{
  StatOp& stat = *statPtr.p;
  int ret;

  KeyData& keyData = stat.m_keyData;
  ret = keyData.copy(stat.m_usePrev ? stat.m_keyData1 : stat.m_keyData2);
  ndbrequire(ret == 0);
  D("statScanReadKey" << V(keyData));
  keyData.convert(NdbPack::Endian::Little);
  memcpy(out, keyData.get_full_buf(), keyData.get_full_len());
}

void
Dbtux::statScanReadValue(StatOpPtr statPtr, Uint32* out)
{
  StatOp& stat = *statPtr.p;
  int ret;
  Uint32 len_out;

  const StatOp::Value& value = stat.m_usePrev ? stat.m_value1 : stat.m_value2;

  // verify sanity
  ndbrequire(value.m_rir != 0);
  for (Uint32 k = 0; k < stat.m_keyCount; k++)
  {
    ndbrequire(value.m_unq[k] != 0);
    ndbrequire(value.m_rir >= value.m_unq[k]);
    ndbrequire(k == 0 || value.m_unq[k] >= value.m_unq[k - 1]);
  }

  NdbPack::Data& valueData = stat.m_valueData;
  valueData.reset();

  ret = valueData.add(&value.m_rir, &len_out);
  ndbrequire(ret == 0 && len_out == 4);
  ret = valueData.add(&value.m_unq[0], stat.m_keyCount, &len_out);
  ndbrequire(ret == 0 && len_out == stat.m_keyCount * 4);
  ret = valueData.finalize();
  ndbrequire(ret == 0);

  D("statScanReadValue" << V(valueData));
  valueData.convert(NdbPack::Endian::Little);
  memcpy(out, valueData.get_full_buf(), valueData.get_full_len());
}

// at end of stats update, TRIX sends loadTime
void
Dbtux::execINDEX_STAT_REP(Signal* signal)
{
  jamEntry();
  const IndexStatRep* rep = (const IndexStatRep*)signal->getDataPtr();

  switch (rep->requestType) {
  case IndexStatRep::RT_UPDATE_REQ:
    ndbabort();
  case IndexStatRep::RT_UPDATE_CONF:
    {
      Index& index = *c_indexPool.getPtr(rep->indexId);
      FragPtr fragPtr;
      findFrag(jamBuffer(), index, rep->fragId, fragPtr);
      ndbrequire(fragPtr.i != RNIL);
      // index.m_statFragPtrI need not be defined yet
      D("loadTime" << V(index.m_statLoadTime) << " ->" << V(rep->loadTime));
      index.m_statLoadTime = rep->loadTime;
    }
    break;
  default:
    ndbabort();
  }
}

// stats monitor

void
Dbtux::execINDEX_STAT_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const IndexStatImplReq* req = (const IndexStatImplReq*)signal->getDataPtr();

  StatMon& mon = c_statMon;
  mon.m_req = *req;
  mon.m_requestType = req->requestType;

  switch (mon.m_requestType) {
  case IndexStatReq::RT_START_MON:
    statMonStart(signal, mon);
    break;
  case IndexStatReq::RT_STOP_MON:
    statMonStop(signal, mon);
    break;
  default:
    ndbabort();
  }
}

void
Dbtux::statMonStart(Signal* signal, StatMon& mon)
{
  const IndexStatImplReq* req = &mon.m_req;
  Index& index = *c_indexPool.getPtr(req->indexId);
  D("statMonStart" << V(mon));

  FragPtr fragPtr;
  fragPtr.setNull();

  if (req->fragId != ZNIL)
  {
    jam();
    findFrag(jamBuffer(), index, req->fragId, fragPtr);
  }

  if (fragPtr.i != RNIL)
  {
    jam();
    index.m_statFragPtrI = fragPtr.i;
    fragPtr.p->m_entryOps = 0;
    D("monitoring node" << V(index));
  }
  else
  {
    jam();
    index.m_statFragPtrI = RNIL;
  }

  statMonConf(signal, mon);
}

void
Dbtux::statMonStop(Signal* signal, StatMon& mon)
{
  const IndexStatImplReq* req = &mon.m_req;
  Index& index = *c_indexPool.getPtr(req->indexId);
  D("statMonStop" << V(mon));

  // RT_STOP_MON simply sends ZNIL to every node
  ndbrequire(req->fragId == ZNIL);
  index.m_statFragPtrI = RNIL;

  statMonConf(signal, mon);
}

void
Dbtux::statMonConf(Signal* signal, StatMon& mon)
{
  const IndexStatImplReq* req = &mon.m_req;
  D("statMonConf" << V(mon));

  IndexStatImplConf* conf = (IndexStatImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = req->senderData;
  sendSignal(req->senderRef, GSN_INDEX_STAT_IMPL_CONF,
             signal, IndexStatImplConf::SignalLength, JBB);
}

// continueB loop

void
Dbtux::statMonSendContinueB(Signal* signal)
{
  StatMon& mon = c_statMon;
  D("statMonSendContinueB" << V(mon));

  signal->theData[0] = TuxContinueB::StatMon;
  signal->theData[1] = mon.m_loopIndexId;
  sendSignalWithDelay(reference(), GSN_CONTINUEB,
                      signal, mon.m_loopDelay, 2);
}

void
Dbtux::statMonExecContinueB(Signal* signal)
{
  StatMon& mon = c_statMon;
  D("statMonExecContinueB" << V(mon));

  if (!c_indexStatAutoUpdate ||
      c_indexStatTriggerPct == 0 ||
      getNodeState().startLevel != NodeState::SL_STARTED)
  {
  }
  else
  {
    jam();
    statMonCheck(signal, mon);
  }
  statMonSendContinueB(signal);
}

void
Dbtux::statMonCheck(Signal* signal, StatMon& mon)
{
  const Uint32 now = (Uint32)time(0);
  D("statMonCheck" << V(mon) << V(now));

  const uint maxloop = 32;
  for (uint loop = 0; loop < maxloop; loop++, mon.m_loopIndexId++)
  {
    jam();
    mon.m_loopIndexId %= c_indexPool.getSize();

    const Index& index = *c_indexPool.getPtr(mon.m_loopIndexId);
    if (index.m_state == Index::NotDefined ||
        index.m_state == Index::Dropping ||
        index.m_statFragPtrI == RNIL)
    {
      jam();
      continue;
    }
    const Frag& frag = *c_fragPool.getPtr(index.m_statFragPtrI);

    bool update = false;
    if (index.m_statLoadTime == 0)
    {
      jam();
      update = true;
      D("statMonCheck" << V(update) << V(index.m_statLoadTime));
    }
    else if (now < index.m_statLoadTime + c_indexStatUpdateDelay)
    {
      jam();
      update = false;
      D("statMonCheck" << V(update) << V(index.m_statLoadTime));
    }
    else
    {
      const Uint64 count = frag.m_entryCount;
      const Uint64 ops = frag.m_entryOps;
      if (count <= 1)
      {
        jam();
        update = (ops >= 1);
        D("statMonCheck" << V(update) << V(ops));
      }
      else
      {
        jam();
        // compute scaled percentags - see wl4124.txt
        double a = c_indexStatTriggerPct;
        double b = c_indexStatTriggerScale;
        double c = (double)count;
        double d = 1 + 0.01 * b * tux_log2(c); // inverse scale factor
        double e = a / d; // scaled trigger pct
        double f = (double)ops;
        double g = 100.0 * f / c;
        update = (g >= e);
        D("statMonCheck" << V(update) << V(f) << V(c));
      }
    }

    if (update)
    {
      jam();
      statMonRep(signal, mon);
      // advance index afterwards
      mon.m_loopIndexId++;
      break;
    }
  }
}

void
Dbtux::statMonRep(Signal* signal, StatMon& mon)
{
  const Index& index = *c_indexPool.getPtr(mon.m_loopIndexId);
  const Frag& frag = *c_fragPool.getPtr(index.m_statFragPtrI);
  D("statMonRep" << V(mon));

  IndexStatRep* rep = (IndexStatRep*)signal->getDataPtrSend();
  rep->senderRef = reference();
  rep->senderData = mon.m_loopIndexId;
  rep->requestType = IndexStatRep::RT_UPDATE_REQ;
  rep->requestFlag = 0;
  rep->indexId = mon.m_loopIndexId;
  rep->indexVersion = 0; // not required
  rep->tableId = index.m_tableId;
  rep->fragId = frag.m_fragId;
  rep->loadTime = index.m_statLoadTime;

  sendSignal(DBDICT_REF, GSN_INDEX_STAT_REP,
             signal, IndexStatRep::SignalLength, JBB);
}
