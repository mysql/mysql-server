// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2010-2016 the Regents of the University of California
#ifndef IBIS_QUAERE_H
#define IBIS_QUAERE_H
/**@file
   @brief FastBit Quaere Interface.

   This is the public interface to a set of functions that performs query
   operations.  It is intended to replace query.h.
 */

#include "part.h"	// ibis::part

namespace ibis {
    class quaere;	// forward definition
} // namespace ibis

/// An abstract query interface.  It provides three key functions,
/// specifying a query, computing the number of hits, and producing a table
/// to represent the selection.  The task of specifying a query is done
/// with the function create.  There are two functions to compute the
/// number of results, roughCount and count, where the function roughCount
/// produce a range to indicate the number of hits is between nmin and
/// nmax, and the function count computes the precise number of hits.
///
/// @warning This is an experimental feature of FastBit.  The current
/// design is very limited and is likely to go through major revisions
/// frequently.  Feel free to express your opinions about the design of
/// this class on the FastBit mailing list fastbit-users@hpcrdm.lbl.gov.
///
/// @note The word quaere is the latin equivalent of query.  Once the
/// implementation of this class stablizes, we intend to swap the names
/// quaere and query.
class FASTBIT_CXX_DLLSPEC ibis::quaere {
public:
    static quaere* create(const char* sel, const char* from, const char* where);
    static quaere* create(const char* sel, const char* from, const char* where,
			  const ibis::partList& prts);
    static quaere* create(const ibis::part* partr, const ibis::part* parts,
			  const char* colname, const char* condr = 0,
			  const char* conds = 0, const char* sel = 0);

    /// Provide an estimate of the number of hits.  It never fails.  In
    /// the worst case, it will simply set the minimum (nmin) to 0 and the
    /// maximum (nmax) to the maximum possible number of results.
    virtual void roughCount(uint64_t& nmin, uint64_t& nmax) const = 0;
    /// Compute the number of results.  This function provides the exact
    /// answer.  If it fails to do so, it will return a negative number to
    /// indicate error.
    virtual int64_t count() const = 0;

    /// Produce a projection of the joint table.  The select clause
    /// associated with the query object is evaluated.  If no select clause
    /// is provided, it returns a table with no columns.  This is different
    /// from having a 'count(*)' as the select clause, which produce a
    /// table with one row and one column.
    ///
    /// @note We assume that this query object might be reused later and
    /// therefore store partial results associated with the query object.
    virtual table* select() const = 0;
    /// Produce a project based on the given select clause.  The joint
    /// data table is defined by the where clause and the from clause given
    /// to the constructor of this object.
    virtual table* select(const char*) const = 0;
    /// Produce a projection of all known data partitions.  This function
    /// selects all values of the named columns that are not NULL.
    ///
    /// @note This function assumes that this query object is used only
    /// once and therefore does not cache the results.
    virtual ibis::table* 
	select(const ibis::table::stringArray& colnames) const = 0;

    virtual ~quaere() {};

protected:
    quaere() {} //!< Default constructor.  Only used by derived classes.

private:
    quaere(const quaere&); // no copying
    quaere& operator=(const quaere&); // no assignment
}; // class ibis::quaere

namespace ibis {
    ibis::part* findDataset(const char*);
    ibis::part* findDataset(const char*, const ibis::partList&);
}
#endif
