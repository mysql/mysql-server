// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2008-2016 the Regents of the University of California
#ifndef IBIS_JNATURAL_H
#define IBIS_JNATURAL_H
/**@file
   @brief In-memory Natual Join.

   This is a concrete implementation of the join operation involving two
   data partitions that can fit in memory.
 */
#include "quaere.h"	// ibis::quaere

namespace ibis {
    class jNatural; // forward definition
} // namespace ibis

/// In-memory Natual Join.
///
/// @warning This is an experimental feature of FastBit.  The current
/// design is very limited and is likely to go through major revisions
/// frequently.  Feel free to express your opinions on the FastBit mailing
/// list fastbit-users@hpcrdm.lbl.gov.
class FASTBIT_CXX_DLLSPEC ibis::jNatural : public ibis::quaere {
public:
    jNatural(const ibis::part* partr, const ibis::part* parts,
	     const char* colname, const char* condr, const char* conds,
	     const char* sel);
    jNatural(const ibis::part* partr, const ibis::part* parts,
	     const ibis::column* colr, const ibis::column* cols,
	     const ibis::qExpr* condr, const ibis::qExpr* conds,
	     const ibis::selectClause* sel, const ibis::fromClause* frm,
	     const char* desc);
    virtual ~jNatural();

    virtual void roughCount(uint64_t& nmin, uint64_t& nmax) const;
    virtual int64_t count() const;

    virtual ibis::table* select() const;
    virtual ibis::table* select(const char*) const;
    virtual ibis::table* select(const ibis::table::stringArray& colnames) const;

protected:
    std::string desc_;
    const ibis::selectClause *sel_;
    const ibis::fromClause *frm_;
    const ibis::part& R_;
    const ibis::part& S_;
    const ibis::column& colR_;
    const ibis::column& colS_;
    ibis::bitvector maskR_;
    ibis::bitvector maskS_;

    mutable array_t<uint32_t> *orderR_;
    mutable array_t<uint32_t> *orderS_;
    mutable void *valR_;
    mutable void *valS_;
    mutable int64_t nrows;

    template <typename T>
    static table*
    fillResult(size_t nrows,
	       const std::string &desc,
	       const ibis::array_t<T>& rjcol,
	       const ibis::table::typeArray& rtypes,
	       const ibis::table::bufferArray& rbuff,
	       const ibis::array_t<T>& sjcol,
	       const ibis::table::typeArray& stypes,
	       const ibis::table::bufferArray& sbuff,
	       const ibis::table::stringArray& cnamet,
	       const std::vector<uint32_t>& cnpos);
    static table*
    fillResult(size_t nrows,
	       const std::string &desc,
	       const std::vector<std::string>& rjcol,
	       const ibis::table::typeArray& rtypes,
	       const ibis::table::bufferArray& rbuff,
	       const std::vector<std::string>& sjcol,
	       const ibis::table::typeArray& stypes,
	       const ibis::table::bufferArray& sbuff,
	       const ibis::table::stringArray& cnamet,
	       const std::vector<uint32_t>& cnpos);

private:
    jNatural(const jNatural&); // no copying
    jNatural& operator=(const jNatural&); // no assignment
}; // class ibis::jNatural
#endif
