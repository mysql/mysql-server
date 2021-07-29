//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2008-2016 the Regents of the University of California
#ifndef IBIS_COUNTQUERY_H
#define IBIS_COUNTQUERY_H
///@file
/// The header file defining the individual countQuery objects.
///
#include "part.h"	// class part
#include "whereClause.h"	// ibis::whereClause

namespace ibis {
    class countQuery;
}

/// @ingroup FastBitMain
/// A simple count query.  A count query is a special form of the SQL
/// select statement, where the select clause is "count(*)".  This data
/// structure is much simpler than ibis::query because it does not produce
/// an identifier for itself and does not ever attempt to record the status
/// of the query.  However, it does accept the same where clause as
/// ibis::query.  In addition, it may take a select clause to provide
/// definitions of aliases in the where clause.
class FASTBIT_CXX_DLLSPEC ibis::countQuery {
public:
    /// Destructor.
    virtual ~countQuery() {clear();}
    /// Constructor.  Generates a new countQuery on the data partition et.
    countQuery(const part* et=0, const ibis::selectClause *s=0)
	: mypart(et), m_sel(s), hits(0), cand(0) {};

    /// Specify the where clause in string form.
    int setWhereClause(const char *str);
    /// Specify the where clause in the form of a qExpr object.
    int setWhereClause(const ibis::qExpr*);
    /// Return the where clause string.
    const char* getWhereClause() const {return conds.getString();}
    /// Resets the data partition used to evaluate the query conditions to
    /// the partition specified in the argument.
    int setPartition(const ibis::part* tbl);
    /// Return the pointer to the data partition used to process the count.
    const part* getPartition() const {return mypart;}
    /// Change the select clause.
    int setSelectClause(const ibis::selectClause *s);
    /// Return the pointer to the select clause.
    const selectClause* getSelectClause() const {return m_sel;}

    /// Functions to perform estimation.
    int estimate();
    /// Return the number of records in the lower bound.
    long getMinNumHits() const;
    /// Return the number of records in the upper bound.
    long getMaxNumHits() const;

    /// Computes the exact hits.
    int evaluate();
    /// Return the number of records in the exact solution.
    long getNumHits() const;
    /// Get the row numbers of the hits.
    long getHitRows(std::vector<uint32_t> &rids) const;
    /// Return the pointer to the internal hit vector.  The user should NOT
    /// attempt to free the returned pointer.
    const ibis::bitvector* getHitVector() const {return hits;}
    /// Return the pointer to the candidates vector.  The user should NOT
    /// attempt to free the returned pointer.
    const ibis::bitvector* getCandVector() const {return cand;}

    /// Releases the resources held by the query object and re-initialize
    /// the select clause and the where clause to blank.
    void clear();

protected:
    whereClause conds;	///!< Query conditions.
    const part* mypart;	///!< Data partition used to process the query.
    const selectClause *m_sel;	///!< Select clause.
    ibis::bitvector* hits;///!< Solution in bitvector form (or lower bound)
    ibis::bitvector* cand;///!< Candidate query results.

    /// Estimate one term of a query expression.
    void doEstimate(const qExpr* term, ibis::bitvector& low,
		    ibis::bitvector& high) const;
    /// Evaluate one term of a query expression.
    int doEvaluate(const qExpr* term, const ibis::bitvector& mask,
		   ibis::bitvector& hits) const;
    /// Evaluate one term using the base data.
    int doScan(const ibis::qExpr* term, const ibis::bitvector& mask,
	       ibis::bitvector& ht) const;

private:
    countQuery(const countQuery&);
    countQuery& operator=(const countQuery&);
}; // class ibis::countQuery

inline int ibis::countQuery::setSelectClause(const ibis::selectClause *s) {
    if (s == 0) return -1;
    m_sel = s;
    return 0;
} // ibis::countQuery::setSelectClause
#endif // IBIS_COUNTQUERY_H
