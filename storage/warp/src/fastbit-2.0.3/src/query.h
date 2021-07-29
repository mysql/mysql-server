// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_QUERY_H
#define IBIS_QUERY_H
///@file
/// The header file defining the individual query objects.
///
#include "part.h"	// ibis::part
#include "whereClause.h"	// ibis::whereClause
#include "selectClause.h"	// ibis::selectClause

/// A data structure for representing user queries.  This is the primary
/// entry for user to take advantage of bitmap indexing facilities.  A
/// query is a very limited version of the SQL SELECT statement.  It is
/// only defined on one data partition and it takes a where clause and a
/// select clause.  The where clause is mandatory!
///
/// It contains a list of range conditions joined together with logical
/// operators, such as "temperature > 700 and 100 <= presessure < 350".
/// Records whose values satisfy the conditions defined in the
/// where clause is considered hits.  A query may retrieve values of
/// variables/columns specified in the select clause.  A select clause is
/// optional.  If specified, it contains a list of column names.  These
/// columns must not be NULL in order for a record to be a hit.  If any
/// additional functions are needed in the select clause, use the function
/// ibis::table::select instead of using this class.
///
/// The hits can be computed in two ways by using functions @c estimate or
/// @c evaluate.  The function @c estimate can take advantage of the
/// indices to give two approximate solutions, one as an upper bound and
/// the other as a lower bound.  The bitmap indices will be automatically
/// built according to the specification if they are not present.  The
/// accuracy of the bounds depend on the nature of the indices available.
/// If no index can be constructed, the lower bound would be empty and the
/// upper bound would include every record.  When the function @c evaluate
/// is called, the exact solution is computed no matter whether the
/// function @c estimate has been called or not.  The solution produced is
/// recorded as a bit vector.  The user may use ibis::bitvector::indexSet
/// to extract the record numbers of the hits or use one of the functions
/// @c getQualifiedInts, @c getQualifiedFloats, and @c getQualifiedDoubles
/// to retrieve the values of the selected columns.  Additionally, one
/// may call either @c printSelected or @c printSelectedWithRID to print
/// the selected values to the specified I/O stream.
///
/// @ingroup FastBitMain
class FASTBIT_CXX_DLLSPEC ibis::query {
public:
    enum QUERY_STATE {
	UNINITIALIZED,	//!< The query object is currently empty.
	SET_COMPONENTS,	//!< The query object has a select clause.
	SET_RIDS,	//!< The query object contains a list of RIDs.
	SET_PREDICATE,	//!< The query object has a where clause.
	SPECIFIED,	//!< SET_COMPONENTS & (SET_RIDS | SET_PREDICATE).
	QUICK_ESTIMATE, //!< A upper and a lower bound are computed.
	FULL_EVALUATE,	//!< The exact hits are computed.
	BUNDLES_TRUNCATED,	//!< Only top-K results are stored.
	HITS_TRUNCATED	//!< The hit vector has been updated to match bundles.
    };

    virtual ~query();
    query(const char* dir, const ibis::partList& tl);
    query(const char* uid=0, const part* et=0, const char* pref=0);

    /// Return an identifier of the query
    const char* id() const {return myID;}
    /// Return the directory for any persistent data.  This is not nil only
    /// if the recovery feature is enabled.  By default, the recovery
    /// feature is disabled.
    const char* dir() const {return myDir;}
    /// User started the query
    const char* userName() const {return user;}
    /// The time stamp on the data used to process the query.
    time_t timestamp() const {return dstime;}
    /// Return the pointer to the data partition used to process the query.
    const part* partition() const {return mypart;}
    /// Return a list of names specified in the select clause.
    const selectClause& components() const {return comps;};

    int setRIDs(const RIDSet& set);
    int setWhereClause(const char *str);
    int setWhereClause(const std::vector<const char*>& names,
		       const std::vector<double>& lbounds,
		       const std::vector<double>& rbounds);
    int setWhereClause(const ibis::qExpr* qexp);
    int addConditions(const ibis::qExpr* qexp);
    int addConditions(const char*);
    virtual int setSelectClause(const char *str);
    /// Resets the data partition associated with the query.
    int setPartition(const ibis::part* tbl);
    /// This is deprecated, will be removed soon.
    int setTable(const ibis::part* tbl) {return setPartition(tbl);}
    /// Return the where clause string.
    virtual const char* getWhereClause() const {return conds.getString();}
    /// Return the select clause string.
    virtual const char* getSelectClause() const {return *comps;}

    void expandQuery();
    void contractQuery();
    std::string removeComplexConditions();

    /// Return a const pointer to the copy of the user supplied RID set.
    const RIDSet* getUserRIDs() const {return rids_in;}

    // Functions to perform estimation.

    int estimate();
    long getMinNumHits() const;
    long getMaxNumHits() const;
    /// Return a pointer to the bit vector representing the candidates.
    const ibis::bitvector* getCandidateVector() const
    {return (sup!=0?sup:hits);}
    long getCandidateRows(std::vector<uint32_t>&) const;

    // Functions related to full evaluation.

    int evaluate(const bool evalSelect=false);
    /// Return the pointer to the internal hit vector.  The user should NOT
    /// attempt to free the returned pointer.  If this function is called
    /// before calling ibis::query::evaluate, it might return a nil
    /// pointer.
    const ibis::bitvector* getHitVector() const {return hits;}
    long getNumHits() const;
    long getHitRows(std::vector<uint32_t> &rids) const;
    long countHits() const;

    int  orderby(const char *names) const;
    long limit(const char *names, uint32_t keep,
	       bool updateHits = true);

    /// The functions @c getQualifiedTTT return the values of selected
    /// columns in the records that satisfies the specified conditions.
    /// The caller must call the operator @c delete to free the pointers
    /// returned.
    ///
    /// Any column in the data partition may be used with @c
    /// getQualifiedTTT, not just those given in the select clause.  The
    /// content returned is read from disk when these functions are called.
    /// @{
    /// Retrieve the values of column_name as 8-bit integers.
    array_t<signed char>*   getQualifiedBytes(const char* column_name);
    /// Retrieve the values of column_name as 8-bit unsigned integers.
    array_t<unsigned char>* getQualifiedUBytes(const char* column_name);
    /// Retrieve the values of column_name as 16-bit integers.
    array_t<int16_t>* getQualifiedShorts(const char* column_name);
    /// Retrieve the values of column_name as 16-bit unsigned integers.
    array_t<uint16_t>* getQualifiedUShorts(const char* column_name);
    /// Retrieve integer values from records satisfying the query conditions.
    array_t<int32_t>* getQualifiedInts(const char* column_name);
    /// Retrieve unsigned integer values from records satisfying the query
    /// conditions.
    array_t<uint32_t>* getQualifiedUInts(const char* column_name);
    /// Retrieve values of column_name as 64-bit integers.
    array_t<int64_t>* getQualifiedLongs(const char* column_name);
    /// Retrieve values of column_name as 64-bit unsigned integers.
    array_t<uint64_t>* getQualifiedULongs(const char* column_name);
    /// Retrieve floating-point values from records satisfying the query
    /// conditions.
    array_t<float>* getQualifiedFloats(const char* column_name);
    /// Retrieve double precision floating-point values from records
    /// satisfying the query conditions.
    array_t<double>* getQualifiedDoubles(const char* column_name);
    /// Retrieve string values from records satisfying the query conditions.
    std::vector<std::string>* getQualifiedStrings(const char* column_name);
    /// Return the list of row IDs of the hits.
    RIDSet* getRIDs() const;
    /// Return a list of row IDs that match the mask.
    RIDSet* getRIDs(const ibis::bitvector& mask) const;
    /// Return the list of row IDs of the hits within the specified bundle.
    const RIDSet* getRIDsInBundle(const uint32_t bid) const;
    /// @}

    /// Print the values of the selected columns to the specified output
    /// stream.  The printed values are grouped by the columns without
    /// functions.  For each group, the functions are evaluated on the
    /// columns named in the function.  This is equivalent to have implicit
    /// "GROUP BY" and "ORDER BY" keywords on all columns appears without a
    /// function in the select clause.
    void printSelected(std::ostream& out) const;
    /// Print the values of the columns in the select clause without
    /// functions.  One the groups of unique values are printed.  For each
    /// group, the row ID (RID) of the rows are also printed.
    void printSelectedWithRID(std::ostream& out) const;

    long sequentialScan(ibis::bitvector& bv) const;
    long getExpandedHits(ibis::bitvector&) const;

    // used by ibis::bundle
    RIDSet* readRIDs() const;
    void writeRIDs(const RIDSet* rids) const;

    void logMessage(const char* event, const char* fmt, ...) const;

    // Functions for cleaning up, retrieving query states
    // and error messages.

    /// Releases the resources held by the query object.
    void clear();
    /// Return the current state of query.
    QUERY_STATE getState() const;
    /// Return the last error message recorded internally.
    const char* getLastError() const {return lastError;}
    /// Reset the last error message to blank.
    void clearErrorMessage() const {*lastError=0;}

    /// Is the given string a valid query token.  Return true if it has the
    /// expected token format, otherwise false.
    static bool isValidToken(const char* tok);
    /// Length of the query token.
    // *** the value 16 is hard coded in functions newToken and ***
    // *** isValidToken ***
    static unsigned tokenLength() {return 16;}

    /// Tell the destructor to remove all stored information about queries.
    static void removeQueryRecords()
    {ibis::gParameters().add("query.purgeTempFiles", "true");}
    /// Tell the destructor to leave stored information on disk.
    static void keepQueryRecords()
    {ibis::gParameters().add("query.purgeTempFiles", "false");}

    class result; // Forward declaration, defined in bundles.h
    class weight;
    class readLock;
    class writeLock;
    friend class readLock;
    friend class writeLock;

protected:
    char* user; 	///!< Name of the user who specified the query
    whereClause conds;	///!< Query conditions
    selectClause comps;	///!< Select clause
    QUERY_STATE state;	///!< Status of the query
    ibis::bitvector* hits;///!< Solution in bitvector form (or lower bound)
    ibis::bitvector* sup;///!< Estimated upper bound
    mutable ibis::part::readLock* dslock;	///!< A read lock on the mypart
    mutable char lastError[MAX_LINE+PATH_MAX];	///!< The warning/error message

    void logError(const char* event, const char* fmt, ...) const;
    void logWarning(const char* event, const char* fmt, ...) const;
    void storeErrorMesg(const char*) const;

    void reorderExpr(); // reorder query expression

    bool hasBundles() const;
    void getBounds();
    void doEstimate(const qExpr* term, ibis::bitvector& low,
		    ibis::bitvector& high) const;

    int computeHits();
    int doEvaluate(const qExpr* term, ibis::bitvector& hits) const;
    int doEvaluate(const qExpr* term, const ibis::bitvector& mask,
		   ibis::bitvector& hits) const;
    int doScan(const qExpr* term, const ibis::bitvector& mask,
	       ibis::bitvector& hits) const;
    int doScan(const qExpr* term, ibis::bitvector& hits) const;

    int64_t processJoin();

    /// Write the basic information about the query to disk.
    virtual void writeQuery();
    /// Read the status information from disk.
    void readQuery(const ibis::partList& tl);
    /// Remove the files written by this object.
    void removeFiles();

    /// Read the results of the query.
    void readHits();
    /// Write the results of the query.
    void writeHits() const;
    /// Export the Row IDs of the hits to log file.
    void printRIDs(const RIDSet& ridset) const;
    /// Count the number of pages accessed to retrieve every value in the
    /// hit vector.
    uint32_t countPages(unsigned wordsize) const;

    /// Expand range conditions to remove the need of candidate check.
    int doExpand(ibis::qExpr* exp0) const;
    /// Contract range conditions to remove the need of candidate check.
    int doContract(ibis::qExpr* exp0) const;

    // A group of functions to count the number of pairs
    // satisfying the join conditions.
    int64_t sortJoin(const std::vector<const ibis::deprecatedJoin*>& terms,
		     const ibis::bitvector& mask) const;
    int64_t sortJoin(const ibis::deprecatedJoin& cmp,
		     const ibis::bitvector& mask) const;
    int64_t sortEquiJoin(const ibis::deprecatedJoin& cmp,
			 const ibis::bitvector& mask) const;
    int64_t sortRangeJoin(const ibis::deprecatedJoin& cmp,
			  const ibis::bitvector& mask) const;
    int64_t sortEquiJoin(const ibis::deprecatedJoin& cmp,
			 const ibis::bitvector& mask,
			 const char* pairfile) const;
    int64_t sortRangeJoin(const ibis::deprecatedJoin& cmp,
			  const ibis::bitvector& mask,
			  const char* pairfile) const;
    void orderPairs(const char* pairfile) const;
    int64_t mergePairs(const char* pairfile) const;

    template <typename T1, typename T2>
    int64_t countEqualPairs(const array_t<T1>& val1,
			    const array_t<T2>& val2) const;
    template <typename T1, typename T2>
    int64_t countDeltaPairs(const array_t<T1>& val1,
			    const array_t<T2>& val2, const T1& delta) const;
    template <typename T1, typename T2>
    int64_t recordEqualPairs(const array_t<T1>& val1,
			     const array_t<T2>& val2,
			     const array_t<uint32_t>& ind1,
			     const array_t<uint32_t>& ind2,
			     const char* pairfile) const;
    template <typename T1, typename T2>
    int64_t recordDeltaPairs(const array_t<T1>& val1,
			     const array_t<T2>& val2,
			     const array_t<uint32_t>& ind1,
			     const array_t<uint32_t>& ind2,
			     const T1& delta, const char* pairfile) const;

    // functions for access control
    void gainReadAccess(const char* mesg) const {
	if (ibis::gVerbose > 10)
	    logMessage("gainReadAccess", "acquiring a read lock for %s",
		       mesg);
	if (0 != pthread_rwlock_rdlock(&lock))
	    logMessage("gainReadAccess",
		       "unable to gain read access to rwlock for %s", mesg);
    }
    void gainWriteAccess(const char* mesg) const {
	if (ibis::gVerbose > 10)
	    logMessage("gainWriteAccess", "acquiring a write lock for %s",
		       mesg);
	if (0 != pthread_rwlock_wrlock(&lock))
	    logMessage("gainWriteAccess",
		       "unable to gain write access to rwlock for %s", mesg);
    }
    void releaseAccess(const char* mesg) const {
	if (ibis::gVerbose > 10)
	    logMessage("releaseAccess", "releasing rwlock for %s", mesg);
	if (0 != pthread_rwlock_unlock(&lock))
	    logMessage("releaseAccess", "unable to unlock the rwlock for %s",
		       mesg);
    }

private:
    char* myID; 	// The unique ID of this query object
    char* myDir;	// Name of the directory containing the query record
    RIDSet* rids_in;	// Rid list specified in an RID query
    const part* mypart;	// Data partition used to process the query
    time_t dstime;		// When query evaluation started
    mutable pthread_rwlock_t lock; // Rwlock for access control

    // private functions
    static char* newToken(const char*); ///!< Generate a new unique token.
    /// Determine a directory for storing information about the query.
    void setMyDir(const char *pref);

    query(const query&);
    query& operator=(const query&);
}; // class ibis::query

namespace ibis {
    ///@{
    /// This is an explicit specialization of a protected member of
    /// ibis::query class.
    /// @note The C++ language rules require explicit specialization of
    /// template member function be declared in the namespace containing
    /// the function, not inside the class!  This apparently causes them to
    /// be listed as public functions in Doxygen document.
    template <>
    int64_t query::countEqualPairs(const array_t<int32_t>& val1,
				   const array_t<uint32_t>& val2) const;
    template <>
    int64_t query::countEqualPairs(const array_t<uint32_t>& val1,
				   const array_t<int32_t>& val2) const;
    template <>
    int64_t query::countDeltaPairs(const array_t<int32_t>& val1,
				   const array_t<uint32_t>& val2,
				   const int32_t& delta) const;
    template <>
    int64_t query::countDeltaPairs(const array_t<uint32_t>& val1,
				   const array_t<int32_t>& val2,
				   const uint32_t& delta) const;
    template <>
    int64_t query::recordEqualPairs(const array_t<int32_t>& val1,
				    const array_t<uint32_t>& val2,
				    const array_t<uint32_t>& ind1,
				    const array_t<uint32_t>& ind2,
				    const char *pairfile) const;
    template <>
    int64_t query::recordEqualPairs(const array_t<uint32_t>& val1,
				    const array_t<int32_t>& val2,
				    const array_t<uint32_t>& ind1,
				    const array_t<uint32_t>& ind2,
				    const char *pairfile) const;
    template <>
    int64_t query::recordDeltaPairs(const array_t<int32_t>& val1,
				    const array_t<uint32_t>& val2,
				    const array_t<uint32_t>& ind1,
				    const array_t<uint32_t>& ind2,
				    const int32_t& delta,
				    const char *pairfile) const;
    template <>
    int64_t query::recordDeltaPairs(const array_t<uint32_t>& val1,
				    const array_t<int32_t>& val2,
				    const array_t<uint32_t>& ind1,
				    const array_t<uint32_t>& ind2,
				    const uint32_t& delta,
				    const char *pairfile) const;
    ///@}
}

/// A class to be used for reordering the terms in the where clauses.
class ibis::query::weight : public ibis::qExpr::weight {
public:
    virtual double operator()(const ibis::qExpr* ex) const;
    weight(const ibis::part* ds) : dataset(ds) {}

private:
    const ibis::part* dataset;
};

/// A read lock on a query object.  To take advantage of the automatic
/// clean up feature guaranteed by the C++ language, use this lock as
/// an automatic variable with a limited scope to ensure the release of
/// lock.
class ibis::query::readLock {
public:
    readLock(const query* q, const char* m) : theQuery(q), mesg(m) {
	theQuery->gainReadAccess(m);
    };
    ~readLock() {theQuery->releaseAccess(mesg);}
private:
    const query* theQuery;
    const char* mesg;

    readLock() {}; // no default constructor
    readLock(const readLock&) {}; // can not copy
}; // class ibis::query::readLock

/// A write lock on a query object.  To take advantage of the automatic
/// clean up feature guaranteed by the C++ language, use this lock as
/// an automatic variable with a limited scope to ensure the release of
/// lock.
class ibis::query::writeLock {
public:
    writeLock(const query* q, const char* m) : theQuery(q), mesg(m) {
	theQuery->gainWriteAccess(m);
    };
    ~writeLock() {theQuery->releaseAccess(mesg);}
private:
    const query* theQuery;
    const char* mesg;

    writeLock() {}; // no default constructor
    writeLock(const writeLock&) {}; // can not copy
}; // ibis::query::writeLock
#endif // IBIS_QUERY_H
