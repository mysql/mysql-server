// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2010-2016 the Regents of the University of California
#ifndef IBIS_JRANGE_H
#define IBIS_JRANGE_H
/**@file
   @brief In-memory Range Join.

   This is a concrete implementation of the range join operation involving
   two data partitions that can fit in memory.  The join range is defined
   by fixed contants known internally as delta1 and delta2.
 */
#include "quaere.h"	// ibis::quaere

namespace ibis {
    class jRange; // forward definition
} // namespace ibis

/// In-memory Range Join.  A range join is a SQL query of the form
///@code
/// SELECT count(*) FROM partR, partS WHERE
///   delta1 <= partR.colR - partS.colS <= delta2
///   and conditions-on-partR and conditions-on-partS;
///@endcode
/// or
///@code
/// SELECT count(*) FROM partR, partS WHERE partR.colR between
///   partS.colS + delta1 and partS.colS + delta2 and
///   conditions-on-partR and conditions-on-partS;
///@endcode
/// where delta1 and delta2 are constants.
///
/// @warning This is an experimental feature of FastBit.  The current
/// design is very limited and is likely to go through major revisions
/// frequently.  Feel free to express your opinions on the FastBit mailing
/// list fastbit-users@hpcrdm.lbl.gov.
class ibis::jRange : public ibis::quaere {
public:
    jRange(const ibis::part& partr, const ibis::part& parts,
	   const ibis::column& colr, const ibis::column& cols,
	   double delta1, double delta2,
	   const ibis::qExpr* condr, const ibis::qExpr* conds,
	   const ibis::selectClause* sel, const ibis::fromClause* frm,
	   const char* desc);
    virtual ~jRange();

    virtual void roughCount(uint64_t& nmin, uint64_t& nmax) const;
    virtual int64_t count() const;

    virtual ibis::table* select() const;
    virtual ibis::table* select(const char*) const;
    virtual ibis::table* select(const ibis::table::stringArray& colnames) const;

protected:
    std::string desc_;
    const ibis::selectClause *sel_;
    const ibis::fromClause *frm_;
    const ibis::part& partr_;
    const ibis::part& parts_;
    const ibis::column& colr_;
    const ibis::column& cols_;
    ibis::bitvector maskr_;
    ibis::bitvector masks_;
    const double delta1_;
    const double delta2_;

    mutable array_t<uint32_t> *orderr_;
    mutable array_t<uint32_t> *orders_;
    mutable void *valr_;
    mutable void *vals_;
    mutable int64_t nrows;

    template <typename T>
    static table*
    fillResult(size_t nrows, double delta1, double delta2,
	       const std::string &desc,
	       const ibis::array_t<T>& rjcol,
	       const ibis::table::typeArray& rtypes,
	       const ibis::table::bufferArray& rbuff,
	       const ibis::array_t<T>& sjcol,
	       const ibis::table::typeArray& stypes,
	       const ibis::table::bufferArray& sbuff,
	       const ibis::table::stringArray& cnamet,
	       const std::vector<uint32_t>& cnpos);

private:
    jRange(const jRange&); // no copying
    jRange& operator=(const jRange&); // no assignment
}; // class ibis::jRange
#endif
