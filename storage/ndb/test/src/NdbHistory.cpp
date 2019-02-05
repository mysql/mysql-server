/*
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbHistory.hpp>
#include <NdbOut.hpp>

WorkerIdentifier:: WorkerIdentifier():
    m_totalWorkers(0),
    m_nextWorker(0)
{}

void
WorkerIdentifier::init(const Uint32 totalWorkers)
{
  assert(totalWorkers != 0);
  lock();
  {
    m_totalWorkers = totalWorkers;
    m_nextWorker = 0;
  }
  unlock();
}

Uint32
WorkerIdentifier::getTotalWorkers() const
{
  return m_totalWorkers;
}

Uint32
WorkerIdentifier::getNextWorkerId()
{
  Uint32 r;
  lock();
  {
    assert(m_nextWorker < m_totalWorkers);
    r = m_nextWorker++;
  }
  unlock();
  return r;
}

void 
EpochRange::dump() const
{
  ndbout_c("[%u/%u,%u/%u)",
           hi(m_start), lo(m_start),
           hi(m_end), lo(m_end));
}

bool
NdbHistory::RecordState::equal(const RecordState& other) const
{
  if (m_state != other.m_state)
  {
    return false;
  }
  if (m_state == RS_EXISTS &&
      m_updatesValue != other.m_updatesValue)
  {
    return false;
  }
  return true;
}

NdbHistory::Version::Version(RecordRange range) : 
  m_range(range) 
{
  m_states = new RecordState[m_range.m_len];
  memset(m_states, 0, (sizeof(RecordState) * m_range.m_len));
}

NdbHistory::Version::Version(const Version* other) :
  m_range(other->m_range)
{
  m_states = new RecordState[m_range.m_len];
  memcpy(m_states, other->m_states, (sizeof(RecordState) * m_range.m_len));
}

NdbHistory::Version::~Version()
{
  delete [] m_states;
  m_states = NULL;
}

void 
NdbHistory::Version::assign(const Version* other)
{
  assert(m_range.m_start == other->m_range.m_start);
  assert(m_range.m_len == other->m_range.m_len);
  
  memcpy(m_states, other->m_states, (sizeof(RecordState) * m_range.m_len));
}

void
NdbHistory::Version::setRows(const Uint32 start,
                             const Uint32 updatesValue,
                             const Uint32 len)
{
  setRowsImpl(start,
              RecordState::RS_EXISTS,
              updatesValue,
              len);
}

void
NdbHistory::Version::clearRows(const Uint32 start,
                               const Uint32 len)
{
  setRowsImpl(start,
              RecordState::RS_NOT_EXISTS,
              0,
              len);
}

Uint32
NdbHistory::Version::diffRowCount(const Version* other) const
{
  if ((m_range.m_start == other->m_range.m_start) &&
      (m_range.m_len == other->m_range.m_len))
  {
    Uint32 count = 0;
    for (Uint32 i=0; i < m_range.m_len; i++)
    {
      if (!m_states[i].equal(other->m_states[i]))
      {
        /* Note no notion of 'distance' for updatesValue */
        count++;
      }
    }
    return count;
  }
  abort();
  return ~Uint32(0);
}

bool 
NdbHistory::Version::equal(const Version* other) const
{
  return (diffRowCount(other) == 0);
}

/* dumpV print helper */
static void dumpV(const char* indent,
                  const Uint32 start, 
                  const Uint32 end,
                  const NdbHistory::RecordState* rs)
{
  if (rs->m_state == NdbHistory::RecordState::RS_NOT_EXISTS)
  {
    ndbout_c("%s  r %5u -> %5u : -", 
             indent,
             start,
             end);
  }
  else
  {
    ndbout_c("%s  r %5u -> %5u : %u", 
             indent, 
             start,
             end,
             rs->m_updatesValue);
  }
}

void
NdbHistory::Version::dump(bool full,
                       const char* indent) const
{
  ndbout_c("%sRange start %u len %u",
           indent,
           m_range.m_start,
           m_range.m_len);
  
  const Uint32 offset = m_range.m_start;
  
  Uint32 sameStart = 0;
  
  for (Uint32 i=0; i < m_range.m_len; i++)
  {
    const RecordState* rs = &m_states[i];
    if (full)
    {
      /* TODO : Put > 1 version per line */
      dumpV(indent,
            offset+i,
            offset+i,
            rs);
    }
    else
    {
      if (i > 0)
      {
        /* Output contiguous ranges of same value */
        bool same;
        const RecordState* prev = &m_states[sameStart];
        
        same = ((rs->m_state == prev->m_state) &&
                (rs->m_updatesValue == prev->m_updatesValue));
        
        if (!same)
        {
          /* Output line for previous */
          dumpV(indent,
                offset + sameStart,
                offset + i - 1,
                prev);
          
          sameStart = i;
        }
      }
      
      const bool last = (i == (m_range.m_len - 1));
      if (last)
      {
        /* Output line for last set */
        dumpV(indent,
              offset + sameStart,
              offset + i,
              rs);
      }
    }  
  }
}

void
NdbHistory::Version::dumpDiff(const Version* other) const
{
  assert(m_range.m_start == other->m_range.m_start);
  assert(m_range.m_len == other->m_range.m_len);

  const Uint32 offset = m_range.m_start;

  /* Simple - full diff view attm */
  for (Uint32 i=0; i < m_range.m_len; i++)
  {
    if (m_states[i].equal(other->m_states[i]))
    {
      dumpV("      ",
            offset + i,
            offset + i,
            &m_states[i]);
    }
    else
    {
      dumpV("DIFF A",
            offset + i,
            offset + i,
            &m_states[i]);
      dumpV("DIFF B",
            offset + i,
            offset + i,
            &other->m_states[i]);
    }
  }

}

void
NdbHistory::Version::setRowsImpl(const Uint32 start,
                                 const Uint32 rowState,
                                 const Uint32 updatesValue,
                                 const Uint32 len)
{
  assert(start >= m_range.m_start);
  assert((start + len) <= (m_range.m_start + m_range.m_len));
  
  const Uint32 offset = start - m_range.m_start;
  for (Uint32 i=0; i < len; i++)
  {
    RecordState* rs = &m_states[offset + i];
    
    rs->m_state = rowState;
    rs->m_updatesValue = updatesValue;
  }
}

const char*
NdbHistory::getVersionTypeName(const VersionType vt)
{
  switch(vt)
  {
  case VT_LATEST: return "VT_LATEST";
  case VT_END_OF_GCI : return "VT_END_OF_GCI";
  case VT_END_OF_EPOCH : return "VT_END_OF_EPOCH";
  case VT_OTHER : return "VT_OTHER";
  default:
    ndbout_c("Unknown versionType %u", vt);
    abort();
    return NULL;
  }
}

void
NdbHistory::VersionMeta::dump() const
{
  ndbout_c("  -- VERSION %llu %s %u/%u --",
           m_number,
           getVersionTypeName(m_type),
           Uint32(m_latest_epoch >> 32),
           Uint32(m_latest_epoch & 0xffffffff));
}


NdbHistory::NdbHistory(const Granularity granularity,
                       const RecordRange range):
  m_granularity(granularity),
  m_range(range),
  m_nextNumber(0)
{
  /* Add initial version with nothing present in the range */
  StoredVersion start;
  start.m_meta.m_number = m_nextNumber++;
  start.m_meta.m_type = VT_LATEST;
  start.m_meta.m_latest_epoch = 0;
  
  start.m_version = new Version(m_range);
  m_storedVersions.push_back(start);
}

NdbHistory::~NdbHistory()
{
  for (Uint32 i=0; i < m_storedVersions.size(); i++)
  {
    delete m_storedVersions[i].m_version;
  }
}

bool
NdbHistory::checkVersionBoundary(const Uint64 epoch, 
                                 VersionType& lastVersionType) const
{
  /* Check epoch compared to last then
   * decide what to do based on
   * recording granularity
   */
  if (m_granularity == GR_LATEST_ONLY)
  {
    /* Latest always represented as one version */
    return false;
  }
  
  const StoredVersion& lastVersion = m_storedVersions.back();
  const Uint64 lastEpoch = lastVersion.m_meta.m_latest_epoch;
  assert(epoch >= lastEpoch);
  const bool sameEpoch = (epoch == lastEpoch);
  const bool sameGci = ((epoch >> 32) ==
                        (lastEpoch >> 32));
  
  if (m_granularity == GR_LATEST_GCI  && sameGci)
  {
    /* No boundary, same version */
    return false;
  }
  else if (m_granularity == GR_LATEST_GCI_EPOCH && sameEpoch)
  {
    /* No boundary, same version */
    return false;
  }
  
  /**
   * Some kind of boundary, determine implied
   * type of last version
   */
  if (!sameGci)
  {
    /* Gci boundary */
    lastVersionType = VT_END_OF_GCI;
  }
  else if (!sameEpoch)
  {
    /* Epoch boundary */
    lastVersionType = VT_END_OF_EPOCH;
  }
  else
  {
    lastVersionType = VT_OTHER;
  }
  
  return true;
}

void 
NdbHistory::commitVersion(const Version* version,
                          Uint64 commitEpoch)
{
  assert(m_range.m_start == version->m_range.m_start);
  assert(m_range.m_len == version->m_range.m_len);
  
  VersionType lastVersionType;
  StoredVersion& lastVersion = m_storedVersions.back();
  
  bool newVersion = checkVersionBoundary(commitEpoch,
                                         lastVersionType);
  
  if (newVersion)
  {
    /**
     * Epoch is sufficiently different to current latest, 
     * so 'save' current latest, and create a new copy for 
     * storing this change
     */
    
    /* TODO : Optionally include */
    if (false)
    {
      ndbout_c("New version (%llu) "
               "prev epoch (%u/%u) new epoch (%u/%u) "
               "prev epoch type %s",
               m_nextNumber + 1,
               Uint32(lastVersion.m_meta.m_latest_epoch >> 32),
               Uint32(lastVersion.m_meta.m_latest_epoch & 0xffffffff),
               Uint32(commitEpoch >> 32),
               Uint32(commitEpoch & 0xffffffff),
               getVersionTypeName(lastVersion.m_meta.m_type));
    }

    /* Set type of last version based on boundary type */
    lastVersion.m_meta.m_type = lastVersionType;
    
    StoredVersion newVersion;
    newVersion.m_meta.m_number = m_nextNumber++;
    newVersion.m_meta.m_type = VT_LATEST;
    newVersion.m_meta.m_latest_epoch = commitEpoch;
    newVersion.m_version = new Version(version);
    
    m_storedVersions.push_back(newVersion);
  }
  else
  {
    /* Update current latest version */
    lastVersion.m_version->assign(version);
    lastVersion.m_meta.m_latest_epoch = commitEpoch;
  }
}

const NdbHistory::Version* 
NdbHistory::getLatestVersion() const
{
  return m_storedVersions.back().m_version;
}


const NdbHistory::Version* 
NdbHistory::findFirstClosestMatch(const Version* match, 
                                  VersionMeta& vm) const
{
  VersionIterator vi(*this);
  const Version* v;
  VersionMeta meta;
  const Version* closest = NULL;
  Uint32 minDistance = ~Uint32(0);
  
  while((v = vi.next(meta)))
  {
    const Uint32 distance = match->diffRowCount(v);
    if (distance < minDistance)
    {
      minDistance = distance;
      closest = v;
      vm = meta;
    }
  }
  
  return closest;
}


NdbHistory::VersionIterator::VersionIterator(const NdbHistory& history):
  m_history(history),
  m_index(0)
{
}


const NdbHistory::Version*
NdbHistory::VersionIterator::next(VersionMeta& vm)
{
  if (m_index < m_history.m_storedVersions.size())
  {
    const StoredVersion& sv = m_history.m_storedVersions[m_index];
    
    vm = sv.m_meta;
    m_index++;
    
    return sv.m_version;
  }
  return NULL;
}

void
NdbHistory::VersionIterator::reset()
{
  m_index = 0;
}


NdbHistory::VersionMatchIterator::VersionMatchIterator(const NdbHistory& history,
                                                       const Version* match) :
  m_vi(history),
  m_match(match)
{
  assert(history.m_range.m_start ==
         match->m_range.m_start);
  assert(history.m_range.m_len ==
         match->m_range.m_len);
}

const NdbHistory::Version*
NdbHistory::VersionMatchIterator::next(VersionMeta& vm)
{
  const Version* v;
  while ((v = m_vi.next(vm)) != NULL)
  {
    if (m_match->equal(v))
    {
      return v;
    }
  }
  return NULL;
}

void
NdbHistory::VersionMatchIterator::reset()
{
  m_vi.reset();
}


NdbHistory::MatchingEpochRangeIterator::
MatchingEpochRangeIterator(const NdbHistory& history,
                           const Version* match):
  m_vi(history),
  m_match(match)
{
  assert(history.m_range.m_start ==
         match->m_range.m_start);
  assert(history.m_range.m_len ==
         match->m_range.m_len);
}

bool 
NdbHistory::MatchingEpochRangeIterator::next(EpochRange& er)
{
  const Version* v;
  VersionMeta vm;
  const Version* matchStart = NULL;
  VersionMeta matchStartVm;
  
  while ((v = m_vi.next(vm)) != NULL)
  {
    if (m_match->equal(v))
    {
      if (matchStart == NULL)
      {
        /* Start of matching range */
        matchStart = v;
        matchStartVm = vm;
      }
      /* else continuing range */
    }
    else
    {
      if (matchStart != NULL)
      {
        /* End of matching range */
        /* Does it include an epoch boundary? */
        if (vm.m_latest_epoch ==
            matchStartVm.m_latest_epoch)
        {
          /* No epoch boundary, skip it and continue */
          matchStart = NULL;
        }
        else
        {
          /* Boundary */
          break;
        }
      }
    }
  }
  
  if (matchStart)
  {
    /* Have a match */
    er.m_start = matchStartVm.m_latest_epoch;
    
    if (v != NULL)
    {
      er.m_end = vm.m_latest_epoch;
    }
    else
    {
      /* Latest is a kind of implicit epoch boundary */
      er.m_end = EpochRange::MAX_EPOCH;
    }
    
    return true;
  }
  
  return false;
}

void
NdbHistory::MatchingEpochRangeIterator::reset()
{
  m_vi.reset();
}


const char*
NdbHistory::getGranularityName(const Granularity gr)
{
  switch (gr)
  {
  case GR_LATEST_ONLY: return "GR_LATEST_ONLY";
  case GR_LATEST_GCI : return "GR_LATEST_GCI";
  case GR_LATEST_GCI_EPOCH : return "GR_LATEST_GCI_EPOCH";
  case GR_ALL : return "GR_ALL";
  default:
    ndbout_c("Unknown granularity : %u", gr);
    abort();
    return NULL;
  }
}

void 
NdbHistory::dump(const bool full) const
{
  ndbout_c("NdbHistory %p", this);
  ndbout_c("  Granularity : %s", getGranularityName(m_granularity));
  ndbout_c("  Range start %u len %u",
           m_range.m_start,
           m_range.m_len);
  ndbout_c("  Num versions stored %u ", m_storedVersions.size());
  const Uint32 lastVersion = m_storedVersions.size() - 1;
  ndbout_c("  Commit epoch range %u/%u -> %u/%u",
           Uint32(m_storedVersions[0].m_meta.m_latest_epoch >> 32),
           Uint32(m_storedVersions[0].m_meta.m_latest_epoch & 0xffffffff),
           Uint32(m_storedVersions[lastVersion].m_meta.m_latest_epoch >> 32),
           Uint32(m_storedVersions[lastVersion].m_meta.m_latest_epoch & 0xffffffff));
  
  if (full)
  {
    ndbout_c("Contained versions first->last : ");
    VersionIterator vi(*this);
    VersionMeta vm;
    const Version* v;
    
    while ((v = vi.next(vm)))
    {
      vm.dump();
      v->dump(false, "     ");
    }
    ndbout_c("End of versions");
  }
}

void
NdbHistory::dumpClosestMatch(const Version* target) const
{
  /* Dump some useful info about an attempted match */
  const Version* closestMatch = NULL;
  NdbHistory::VersionMeta closestMatchMeta;
  
  if ((closestMatch = findFirstClosestMatch(target,
                                            closestMatchMeta)))
  {
    ndbout_c("Closest version in history :");
    closestMatchMeta.dump();
    closestMatch->dump();

    // TODO : Dump a version diff, with good diff format
  }
  else
  {
    ndbout_c("Failed to find a close match in history");
  }
}
