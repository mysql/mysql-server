/*
   Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef NDBT_HISTORY_HPP
#define NDBT_HISTORY_HPP

#include <Vector.hpp>
#include <NdbMutex.h>


/**
 * Class for giving e.g. a set of steps their own
 * id out of a range of 0..totalWorkers
 *
 * Handy for e.g. subdividing a range of records
 * amongst a variable number of workers
 * 
 * Usage :
 *    WorkerIdentifier()
 *    Repeat
 *      init(totalWorkers)
 *      Repeat
 *        getNextWorkerId()
 *        getTotalWorkers()
 * ...
 */
class WorkerIdentifier : public NdbLockable
{
  Uint32 m_totalWorkers;
  Uint32 m_nextWorker;
public:
  WorkerIdentifier();
  void init(const Uint32 totalWorkers);

  Uint32 getTotalWorkers() const;

  Uint32 getNextWorkerId();
};


/**
 * EpochRange
 *
 * A representation of a range of epochs
 * This is useful when comparing versions between
 * multiple histories
 * 
 * Epoch range has an open start and closed end
 *
 *  [start, end)
 * 
 *  start = 11/20
 *  end   = 12/3
 *
 *  includes 
 *   11/20,11/21,...11/0xffffffff,12/0,12/1
 *
 *  does not include
 *   <= 11/19 >= 12/3
 * 
 * Two ranges intersect if they have any epochs
 * in common
 */
struct EpochRange
{
  static const Uint64 MAX_EPOCH = 0xffffffffffffffffULL;
  /* Start is included */
  Uint64 m_start;
  /* End is not included */
  Uint64 m_end;

  EpochRange intersect(const EpochRange& other) const
  {
    EpochRange i;
    i.m_start = MAX(m_start, other.m_start);
    i.m_end   = MIN(m_end, other.m_end);

    return i;
  }

  bool isEmpty() const
  {
    return (m_start >= m_end);
  }

  bool spansGciBoundary() const
  {
    /* Does this range span at least one GCI ? */
    assert(m_end > m_start);
    return ((m_end >> 32) >
            (m_start >> 32));
  }
  
  static Uint32 hi(const Uint64 epoch)
  {
    return Uint32(epoch>>32);
  }
  
  static Uint32 lo(const Uint64 epoch)
  {
    return Uint32(epoch & 0xffffffff);
  }

  void dump() const;
};

/**
 * EpochRangeSet
 *
 * Set of EpochRanges of interest.
 *
 * This is useful for describing consistent points in 
 * history when some condition was true
 *
 * Not guaranteed that all contained ranges are unique or
 * disjoint with each other
 */
struct EpochRangeSet
{
  Vector<EpochRange> m_ranges;

  /**
   * Add an epochRange to an EpochRangeSet
   */
  void addEpochRange(const EpochRange& er)
  {
    m_ranges.push_back(er);
  }

  /**
   * Does this EpochRangeSet describe any range
   * of epochs?
   */
  bool isEmpty() const
  {
    for (Uint32 i=0; i<m_ranges.size(); i++)
    {
      if (!m_ranges[i].isEmpty())
      {
        return false;
      }
    }
    return true;
  }


  /**
   * Create an EpochRangeSet which contains the
   * set of intersecting epoch ranges in two
   * input EpochRanges
   */
  static EpochRangeSet intersect(const EpochRangeSet& a,
                                 const EpochRangeSet& b)
  {
    EpochRangeSet result;

    /* Try to intersect every range in A with 
     * every range in B, and keep the non-empty
     * results
     */
    for (Uint32 ai = 0; ai < a.m_ranges.size(); ai++)
    {
      for (Uint32 bi = 0; bi < b.m_ranges.size(); bi++)
      {
        const EpochRange& erA = a.m_ranges[ai];
        const EpochRange& erB = b.m_ranges[bi];
        const EpochRange intersection = erA.intersect(erB);
        
        if (!intersection.isEmpty())
        {
          result.addEpochRange(intersection);
        }
      }
    }
    
    return result;
  }

  void dump() const
  {
    for (Uint32 i=0; i<m_ranges.size(); i++)
    {
      m_ranges[i].dump();
    }
  }
    
};


/**
 * RecordRange
 *
 * Contiguous range of logical tuple ids
 */
struct RecordRange
{
  Uint32 m_start;
  Uint32 m_len;

  RecordRange(const Uint32 start,
              const Uint32 len) :
    m_start(start),
    m_len(len) {}
};


/**
 * NdbHistory
 *
 * Class for tracking and inspecting a history of changes to
 * a range of rows.
 * The granularity of the history collected can be configured
 * to adjust the cost of history tracking.
 *
 * Intended to be maintained on a unique range of rows from a 
 * single thread at a time.
 */
class NdbHistory
{
public:
  /**
   * RecordState
   * 
   * Logical state of a record covering
   * existence and value (if exists).
   * Future : Include uncertainty about
   * commit states for use with 
   * disconnection or isolation tests.
   */
  struct RecordState
  {
    enum RowState
    {
      RS_NOT_EXISTS = 0,
      RS_EXISTS = 1
    };
    Uint32 m_state;
    Uint32 m_updatesValue;

    bool equal(const RecordState& other) const;
  };
  
  /**
   * Version
   *
   * Set of row states for a range, 
   * describing a snapshot of the version of 
   * the data for that range.
   */
  class Version
  {
  public:
    RecordRange m_range;
    RecordState* m_states;
    
    /* Create empty version for range */
    Version(RecordRange range);

    /* Create version for range same as an existing one */
    Version(const Version* other);

    // TODO : Create version from subrange of another version

    /* Release version storage */
    ~Version();

    /** Version modification **/
    
    /* Assign row states from another version, ranges must align */
    void assign(const Version* other);

    /**
     * setRows
     *
     * Set the updates values of the row(s) to the passed value
     * Row range must be contained within the version's range
     */
    void setRows(const Uint32 start,
                 const Uint32 updatesValue,
                 const Uint32 len = 1);

    /**
     * clearRows
     *
     * Clears (marks as deleted) the row(s) in the passed range.
     * The passed range must be contained within the version's
     * range
     */
    void clearRows(const Uint32 start,
                   const Uint32 len = 1);

    /**
     * diffRowCount
     * 
     * return count of rows which differ between
     * two versions of the same row range
     */
    Uint32 diffRowCount(const Version* other) const;

    /**
     * equal
     *
     * return true if both versions are equal
     */
    bool equal(const Version* other) const;

    /**
     * dump
     *
     * Helper for dumping a version
     * When full = false, only contiguous subranges are output
     */
    void dump(bool full=false,
              const char* indent = "") const;

    /**
     * dumpDiff
     * 
     * Helper for dumping a diff between two versions
     */
    void dumpDiff(const Version* other) const;

  private:
    void setRowsImpl(const Uint32 start,
                     const Uint32 rowState,
                     const Uint32 updatesValue,
                     const Uint32 len);
  };
  
  /**
   * VersionType
   *
   * Type of version relative to other versions in
   * the history of a range
   */
  enum VersionType
  {
    VT_LATEST,         // Version contains latest changes
    VT_END_OF_GCI,     // Version contains end-of-GCI consistent state
    VT_END_OF_EPOCH,   // Version contains end-of-epoch consistent state
    VT_OTHER           // Version is none of the above
  };

  static const char* getVersionTypeName(const VersionType vt);

  /**
   * VersionMeta
   *
   * Metadata concerning a version stored in a history
   * TODO Consider/add rollback + unknown outcome types
   */
  struct VersionMeta
  {
    Uint64 m_number;        /* Sequential number of version in history */
    VersionType m_type;     /* Type of version in this history */
    Uint64 m_latest_epoch;  /* Epoch of most recent change in this version */

    /* Dump VersionMeta */
    void dump() const;
  };

  /**
   * Granularity
   *
   * Granularity at which distinct versions are kept
   * in the history.
   * When a change to a range is added, it will either
   * be merged into the description of the latest version
   * or it will cause the a new version description to 
   * be allocated.
   * This is decided based on the change epoch and the
   * Granularity of the history.
   * Note that only changes result in history being 
   * recorded.  Where there is no change for multiple
   * epochs or GCIs, nothing will be recorded.
   *
   * Note on epoch and GCI numbers :
   *   The last recorded version with a given epoch or
   *   GCI number is the final state associated with 
   *   that epoch or GCI in the history.
   *   These last states are marked in the history with
   *   their version type
   *   If the rows are unchanged for some time then
   *   of course the same version may be in-force for
   *   several epochs
   *   
   * 
   *   GR_LATEST_ONLY
   *   Only latest version (1 version)
   *     LATEST : (1 : 22) (2 : 22) (3 : 44) ...  53/6
   *
   *   GR_LATEST_GCI
   *   Latest version + last in GCI versions
   *     LATEST : (1 : 22) (2 : 22) (3 : 44) ... 53/6
   *     GCI    : (1 : 21) (2 : 21) (3 : 43) ... 52/19
   *     GCI    : (1 : 21) (2 : 20) (3 : 41) ... 51/19
   *     GCI    : (1 : 21) (2 : 19) (3 : 40) ... 50/19
   *     ...
   *
   *   GR_LATEST_GCI_EPOCH
   *   Latest version + last in GCI + last in Epoch versions
   *     LATEST : (1 : 22) (2 : 22) (3 : 44) ... 53/6
   *     EPOCH  : (1 : 22) (2 : 21) (3 : 44) ... 53/5
   *     EPOCH  : (1 : 22) (2 : 21) (3 : 44) ... 54/2
   *     ...
   *     GCI    : (1 : 21) (2 : 21) (3 : 43) ... 52/19
   *     EPOCH  : (1 : 21) (2 : 21) (3 : 42) ... 52/18
   *     EPOCH  : ...
   *     ...
   *     GCI    : (1 : 21) (2 : 20) (3 : 41) ... 51/19
   *     EPOCH  : ...
   *     ...
   *
   *   GR_ALL
   *   All versions
   *     LATEST : (1 : 22) (2 : 22) (3 : 44) ... 53/6
   *     OTHER  : (1 : 22) (2 : 22) (3 : 44) ... 53/6
   *     OTHER  : (1 : 22) (2 : 22) (3 : 44) ... 53/6
   *     ...
   *     EPOCH  : (1 : 22) (2 : 21) (3 : 44) ... 53/5
   *     OTHER  ...
   *     OTHER  ...
   *     ...
   *     EPOCH  : (1 : 22) (2 : 21) (3 : 44) ... 54/2
   *     ...
   *     GCI    : (1 : 21) (2 : 21) (3 : 43) ... 52/19
   *     ...
   */
  enum Granularity
  {
    GR_LATEST_ONLY,
    GR_LATEST_GCI,
    GR_LATEST_GCI_EPOCH,
    GR_ALL
  };

  static const char* getGranularityName(const Granularity gr);


  /** Creation + Deletion **/
  
  /**
   * Create an NdbHistory for recording versions of rows in the given
   * range, at the given granularity
   *
   * TODO : Bound history length by # versions, #GCIs, # epochs, manually
   #        etc
   */
  NdbHistory(const Granularity granularity,
          const RecordRange range);

  ~NdbHistory();


  /** Modification **/

  /**
   * checkVersionBoundary
   * 
   * Method which checks whether a commit in the passed epoch
   * represents a version boundary between the previous history
   * and the new commit according to the history's recording 
   * granularity.
   *
   * If true is returned then a new version should be used for
   * the new commit, and the type of the last version is
   * updated.
   * If false is returned, the type of the last version is 
   * unmodified
   */
  bool checkVersionBoundary(const Uint64 epoch, 
                            VersionType& lastVersionType) const;

  /**
   * commitVersion
   *
   * This method is used to add a committed version 
   * to the history.
   * The new version will be recorded according to
   * the history's granularity.
   *
   * This generally results in the Version state
   * being copied.
   * 
   * Note that one way to optimise performance if
   * necessary could be to guard calls to this
   * method using checkVersionBoundary(), so that
   * only versions which are significant to the 
   * history's granularity are recorded.
   * 
   * Other potential optimisations :
   *   - Track/copy only modified subrange
   */
  // TODO : Add rollback + unknown result recording variants
  void commitVersion(const Version* version,
                     Uint64 commitEpoch);

  /** Analysis **/

  /**
   * getLatestVersion()
   * 
   * This method returns a pointer to the latest version
   * stored in the history.
   * Will return NULL for an empty history.
   */
  const Version* getLatestVersion() const;

  /**
   * VersionIterator
   * 
   * Iterator for iterating over the recorded version(s)
   * in ascending order, oldest to latest
   * NULL is returned at the end, per version metadata
   * is put into the passed referenced variable.
   */
  class VersionIterator
  {
  public:
    VersionIterator(const NdbHistory& history);

    const Version* next(VersionMeta& vm);

    void reset();

  private:
    const NdbHistory& m_history;
    Uint32 m_index;
  };


  /**
   * VersionMatchIterator
   *
   * Iterator for iterating over the recorded version(s)
   * in ascending order, looking for versions matching
   * the version passed in as match.
   * Note that there can be 0, 1 or multiple matching
   * versions.
   */
  class VersionMatchIterator
  {
  public:
    VersionMatchIterator(const NdbHistory& history,
                         const Version* match);

    const Version* next(VersionMeta& vm);
    void reset();
  private:
    VersionIterator m_vi;
    const Version* m_match;
  };

  /**
   * MatchingEpochRangeIterator
   *
   * Iterator for iterating over the recorded version(s)
   * in ascending order, returning ranges of epochs which
   * contain versions which match the passed version.
   * Note that only matches spanning epoch boundaries
   * are considered - matches contained within an epoch
   * are filtered out.
   */
  class MatchingEpochRangeIterator
  {
  public:
    MatchingEpochRangeIterator(const NdbHistory& history,
                               const Version* match);
    bool next(EpochRange& er);
    void reset();
  private:
    VersionIterator m_vi;
    const Version* m_match;
  };

  /**
   * Find the first closest matching version in history
   * according to the diffRowCount method on Version
   *
   * Useful for debugging version mismatches.
   * See also dumpClosestMatch below
   */
  const Version* findFirstClosestMatch(const Version* match, VersionMeta& vm) const;

  /**
   * dump
   *
   * Helper for dumping out a history
   * full gives all version info as well as summary
   */
  void dump(const bool full = false) const;

  /**
   * dumpClosestMatch
   *
   * Helper for dumping out closest matching version
   * in history 
   */
  void dumpClosestMatch(const Version* target) const;

  /** Public member vars */

  const Granularity m_granularity;
  const RecordRange m_range;
private:
  /**
   * StoredVersion
   * Internal structure used to track versions
   */
  struct StoredVersion
  {
    VersionMeta m_meta;
    Version* m_version;
  };

  Vector<StoredVersion> m_storedVersions;
  Uint64 m_nextNumber;
};

#endif
