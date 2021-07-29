// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2010-2016 the Regents of the University of California
#ifndef IBIS_FILTER_H
#define IBIS_FILTER_H
/**@file
   @brief FastBit Filter class.

   This is the simplest version of a query.  Following the older code, this
   class attempts to apply the same where clause on all known data
   partitions and produce a concatenated result set.
 */

#include "quaere.h"		// ibis::quaere
#include "whereClause.h"	// ibis::whereClause
#include "selectClause.h"	// ibis::selectClause

namespace ibis {
    class filter;	// forward definition
} // namespace ibis

/// A simple filtering query.  The where clause does not contain any table
/// names.  Following the convention used in older version of the query
/// class, the same where clause is applied to all known data partitions.
class ibis::filter : public ibis::quaere {
public:
    explicit filter(const ibis::whereClause *);
    filter(const ibis::selectClause *, const ibis::constPartList *,
	   const ibis::whereClause *);
    filter(const ibis::bitvector &, const ibis::part &);
    virtual ~filter();

    virtual void    roughCount(uint64_t& nmin, uint64_t& nmax) const;
    virtual int64_t count() const;
    virtual table*  select() const;
    virtual table*  select(const char*) const;
    virtual table*  select(const ibis::table::stringArray& colnames) const;

    static table*   sift(const ibis::selectClause  &sel,
			 const ibis::constPartList &pl,
			 const ibis::whereClause   &wc);
    static table*   sift0(const ibis::selectClause  &,
			  const ibis::constPartList &);
    static table*   sift0S(const ibis::selectClause  &,
			   const ibis::constPartList &);
    static table*   sift1(const ibis::selectClause  &,
			  const ibis::constPartList &,
			  const ibis::whereClause   &);
    static table*   sift1S(const ibis::selectClause  &,
			   const ibis::constPartList &,
			   const ibis::whereClause   &);
    static table*   sift2(const ibis::selectClause  &,
			  const ibis::constPartList &,
			  const ibis::whereClause   &);
    static table*   sift2(const ibis::selectClause  &,
			  const ibis::constPartList &,
			  const ibis::array_t<ibis::bitvector*> &);
    static table*   sift2(const ibis::selectClause  &,
			  const ibis::constPartList &,
			  const ibis::whereClause   &,
			  ibis::array_t<ibis::bitvector*> &);
    static table*   sift2S(const ibis::selectClause  &,
			   const ibis::constPartList &,
			   const ibis::whereClause   &);
    static table*   sift2S(const ibis::selectClause  &,
			   const ibis::constPartList &,
			   const ibis::array_t<ibis::bitvector*> &);
    static table*   sift2S(const ibis::selectClause  &,
			   const ibis::constPartList &,
			   const ibis::whereClause   &,
			   ibis::array_t<ibis::bitvector*> &);

protected:
    /// The where clause.
    const ibis::whereClause *wc_;
    /// A list of data partitions to query.
    const ibis::constPartList *parts_;
    /// The select clause.  Also used to spply aliases.  If the function
    /// select is called with an empty select clause, then this variable
    /// will be used as the substitute.
    const ibis::selectClause *sel_;
    /// Solution in bitvector form.  If cand is no nil, then this bit
    /// vector is a lower bound.
    mutable array_t<ibis::bitvector*> hits_;
    /// Candidate query results.
    mutable array_t<ibis::bitvector*> cand_;

    /// Default constructor.  Nothing can be done without explicitly
    /// initializing the member variables.
    filter() : wc_(0), parts_(0), sel_(0) {}

private:
    filter(const filter&); // no copying
    filter& operator=(const filter&); // no assignment
}; // class ibis::filter
#endif
