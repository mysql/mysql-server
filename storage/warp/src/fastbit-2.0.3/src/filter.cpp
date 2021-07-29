// File $Id$    
// author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif

#include "tab.h"        // ibis::tabula
#include "bord.h"       // ibis::bord
#include "mensa.h"      // ibis::mensa
#include "countQuery.h" // ibis::countQuery
#include "filter.h"     // ibis::filter
#include "util.h"       // ibis::util::makeGuard

#include <memory>       // std::unique_ptr
#include <algorithm>    // std::sort
#include <sstream>      // std::ostringstream
#include <limits>       // std::numeric_limits

/// Constructor.  The incoming where clause is applied to all known data
/// partitions in ibis::datasets.
ibis::filter::filter(const ibis::whereClause *w)
    : wc_(w != 0 && w->empty() == false ? new whereClause(*w) : 0),
      parts_(0), sel_(0) {
    LOGGER(ibis::gVerbose > 5)
        << "Constructed a filter @ " << this << " with a where clause";
} // constructor

/// Constructor.  The caller supplies all three clauses of a SQL select
/// statement.  The arguments are copied if they are not empty.
///
/// @note This constructor makes a copy of the container for the data
/// partitions, but not the data partitions themselves.  In the
/// destructor, only the container is freed, not the data partitions.
ibis::filter::filter(const ibis::selectClause *s,
                     const ibis::constPartList *p,
                     const ibis::whereClause *w)
    : wc_(w == 0 || w->empty() ? 0 : new whereClause(*w)),
      parts_(p == 0 || p->empty() ? 0 : new constPartList(*p)),
      sel_(s == 0 || s->empty() ? 0 : new selectClause(*s)) {
    LOGGER(ibis::gVerbose > 5)
        << "Constructed a filter @ " << this << " with three components";
} // constructor

/// Constructor.  This constructor takes a bit vector and a single data
/// partition.  It is intended to regenerate a query result set saved as a
/// hit vector.  The caller can use various versions of the function select
/// to reprocess the result from another query.
ibis::filter::filter(const ibis::bitvector &s, const ibis::part &p)
    : wc_(0), parts_(new ibis::constPartList(1, &p)), sel_(0), hits_(1) {
    // put a copy of the bitvector as the hits_[0]
    hits_[0] = new ibis::bitvector(s);
    LOGGER(ibis::gVerbose > 5)
        << "Constructed a filter @ " << this
        << " with a bit vector on data partition " << p.name();
    LOGGER(s.size() != p.nRows() && ibis::gVerbose > 0)
        << "Warning -- filter::ctor received a bitvector with " << s.size()
        << " bit" << (s.size()>1?"s":"") << ", but a data partition with "
        << p.nRows() << " row" << (p.nRows()>1?"s":"");
} // constructor

/// Destructor.
ibis::filter::~filter() {
    LOGGER(ibis::gVerbose > 5)
        << "Freeing filter @ " << this;
    ibis::util::clear(cand_);
    ibis::util::clear(hits_);
    delete parts_;
    delete sel_;
    delete wc_;
} // ibis::filter::~filter

/// Produce a rough count of the number of hits.
void ibis::filter::roughCount(uint64_t &nmin, uint64_t &nmax) const {
    const ibis::constPartList &myparts =
        (parts_ != 0 ? *parts_ :
         reinterpret_cast<const ibis::constPartList&>(ibis::datasets));
    nmin = 0;
    nmax = 0;
    if (wc_ == 0) {
        LOGGER(ibis::gVerbose > 3)
            << "filter::roughCount assumes all rows are hits because no "
            "query condition is specified";
        for (ibis::constPartList::const_iterator it = myparts.begin();
             it != myparts.end(); ++ it)
            nmax += (*it)->nRows();
        nmin = nmax;
        return;
    }
    if (hits_.size() == myparts.size()) {
        for (size_t j = 0; j < myparts.size(); ++ j) {
            if (hits_[j] != 0)
                nmin += hits_[j]->cnt();
            if (j >= cand_.size() || cand_[j] == 0) {
                if (hits_[j] != 0)
                    nmax += hits_[j]->cnt();
            }
            else {
                nmax += cand_[j]->cnt();
            }
        }
        return;
    }
    hits_.reserve(myparts.size());
    cand_.reserve(myparts.size());

    ibis::countQuery qq;
    int ierr = qq.setWhereClause(wc_->getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- filter::roughCount failed to assign the "
            "where clause, assume all rows may be hits";
        for (ibis::constPartList::const_iterator it = myparts.begin();
             it != myparts.end(); ++ it)
            nmax += (*it)->nRows();
        return;
    }
    if (sel_ != 0) {
        ierr = qq.setSelectClause(sel_);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- filter::roughCount failed to assign the "
                "select clause, assume all rows may be hits";
            for (ibis::constPartList::const_iterator it = myparts.begin();
                 it != myparts.end(); ++ it)
                nmax += (*it)->nRows();
            return;
        }
    }

    for (unsigned j = 0; j < myparts.size(); ++ j) {
        if (j < hits_.size()) {
            if (hits_[j] != 0)
                nmin += hits_[j]->cnt();
            if (j >= cand_.size() || cand_[j] == 0) {
                if (hits_[j] != 0)
                    nmax += hits_[j]->cnt();
            }
            else {
                nmax += cand_[j]->cnt();
            }
        }
        else {
            ierr = qq.setPartition(myparts[j]);
            if (ierr >= 0) {
                ierr = qq.estimate();
                if (ierr >= 0) {
                    nmin += qq.getMinNumHits();
                    nmax += qq.getMaxNumHits();
                    while (hits_.size() < j)
                        hits_.push_back(0);
                    while (cand_.size() < j)
                        cand_.push_back(0);
                    if (hits_.size() == j) {
                        if (qq.getHitVector())
                            hits_.push_back
                                (new ibis::bitvector(*qq.getHitVector()));
                        else
                            hits_.push_back(0);
                    }
                    else {
                        delete hits_[j];
                        if (qq.getHitVector())
                            hits_[j] = new ibis::bitvector(*qq.getHitVector());
                        else
                            hits_[j] = 0;
                    }
                    if (cand_.size() == j) {
                        if (qq.getCandVector() != 0 &&
                            qq.getCandVector() != qq.getHitVector())
                            cand_.push_back
                            (new ibis::bitvector(*qq.getCandVector()));
                        else
                            cand_.push_back(0);
                    }
                    else {
                        delete cand_[j];
                        if (qq.getCandVector())
                            cand_[j] = new ibis::bitvector(*qq.getCandVector());
                        else
                            cand_[j] = 0;
                    }
                }
                else {
                    nmax += myparts[j]->nRows();
                }
            }
            else {
                nmax += myparts[j]->nRows();
            }
        }
    }
} // ibis::filter::roughCount

/// Produce the exact number of hits.
int64_t ibis::filter::count() const {
    int64_t nhits = 0;
    const ibis::constPartList &myparts =
        (parts_ != 0 ? *parts_ :
         reinterpret_cast<const constPartList&>(ibis::datasets));
    nhits = 0;
    if (wc_ == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "filter::count assumes all rows are hits because no "
            "query condition is specified";
        for (ibis::constPartList::const_iterator it = myparts.begin();
             it != myparts.end(); ++ it)
            nhits += (*it)->nRows();
        return nhits;
    }
    if (hits_.size() == myparts.size()) {
        if (cand_.empty()) {
            for (array_t<bitvector*>::const_iterator it = hits_.begin();
                 it != hits_.end(); ++ it)
                nhits += (*it)->cnt();
            return nhits;
        }
        else {
            bool exact = true;
            for (size_t j = 0; j < myparts.size() && exact; ++ j) {
                if (j >= cand_.size() || cand_[j] == 0)
                    nhits += hits_[j]->cnt();
                else
                    exact = false;
            }
            if (exact) {
                cand_.clear();
                return nhits;
            }
            else {
                nhits = 0;
            }
        }
    }
    hits_.reserve(myparts.size());

    ibis::countQuery qq;
    int ierr = qq.setWhereClause(wc_->getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- filter::count failed to assign the "
            "where clause";
        nhits = ierr;
        return nhits;
    }
    if (sel_ != 0) {
        ierr = qq.setSelectClause(sel_);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- filter::count failed to assign the "
                "select clause";
            nhits = ierr;
            return nhits;
        }
    }

    for (size_t j = 0; j < myparts.size(); ++ j) {
        if (j < hits_.size() && hits_[j] != 0 &&
            (j >= cand_.size() || cand_[j] == 0)) {
            nhits += hits_[j]->cnt();
        }
        else {
            ierr = qq.setPartition(myparts[j]);
            if (ierr >= 0) {
                ierr = qq.evaluate();
                if (ierr >= 0) {
                    nhits += qq.getNumHits();
                    while (hits_.size() < j)
                        hits_.push_back(0);
                    if (hits_.size() == j) {
                        if (qq.getHitVector() != 0)
                            hits_.push_back(new ibis::bitvector
                                            (*qq.getHitVector()));
                        else
                            hits_.push_back(0);
                    }
                    else {
                        if (qq.getHitVector() != 0) {
                            hits_[j]->copy(*qq.getHitVector());
                        }
                        else {
                            delete hits_[j];
                            hits_[j] = 0;
                        }
                    }
                    if (cand_.size() > j) {
                        delete cand_[j];
                        cand_[j] = 0;
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- filter::count failed to evaluate "
                        << qq.getWhereClause() << " on " << myparts[j]->name()
                        << ", ierr = " << ierr;
                }
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- filter::count failed to assign "
                    << qq.getWhereClause() << " on " << myparts[j]->name()
                    << ", ierr = " << ierr;
            }
        }
    }
    return nhits;
} // ibis::filter::count

ibis::table* ibis::filter::select() const {
    const ibis::constPartList &myparts =
        (parts_ != 0 ? *parts_ :
         reinterpret_cast<const ibis::constPartList&>(ibis::datasets));
    if (sel_ == 0) {
        return new ibis::tabula(count());
    }
    try {
        const bool sep = sel_->isSeparable();
        if (wc_ == 0 || wc_->empty()) {
            if (sep && myparts.size() > 1)
                return ibis::filter::sift0S(*sel_, myparts);
            else
                return ibis::filter::sift0(*sel_, myparts);
        }

        if (hits_.size() == myparts.size()) {
            bool exact = true;
            for (size_t j = 0; j < cand_.size() && exact; ++ j)
                exact = (cand_[j] == 0);
            if (exact) {
                cand_.clear();
                if (sep && myparts.size() > 1)
                    return ibis::filter::sift2S(*sel_, myparts, hits_);
                else
                    return ibis::filter::sift2(*sel_, myparts, hits_);
            }
        }

        for (size_t j = 0; j < cand_.size(); ++ j)
            delete cand_[j];
        cand_.clear();
        if (sep && myparts.size() > 1)
            return ibis::filter::sift2S(*sel_, myparts, *wc_, hits_);
        else
            return ibis::filter::sift2(*sel_, myparts, *wc_, hits_);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a bad_alloc ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::filter::select

ibis::table* ibis::filter::select(const char* sstr) const {
    const ibis::constPartList &myparts =
        (parts_ != 0 ? *parts_ :
         reinterpret_cast<const ibis::constPartList&>(ibis::datasets));
    if (sstr == 0 || *sstr == 0) {
        return new ibis::tabula(count());
    }
    try {
        ibis::selectClause sel(sstr);
        const bool sep = sel.isSeparable();
        if (wc_ == 0 || wc_->empty()) {
            if (sep && myparts.size() > 1)
                return ibis::filter::sift0S(sel, myparts);
            else
                return ibis::filter::sift0(sel, myparts);
        }

        if (hits_.size() == myparts.size()) {
            bool exact = true;
            for (size_t j = 0; j < cand_.size() && exact; ++ j)
                exact = (cand_[j] == 0);
            if (exact) {
                cand_.clear();
                if (sep && myparts.size() > 1)
                    return ibis::filter::sift2S(sel, myparts, hits_);
                else
                    return ibis::filter::sift2(sel, myparts, hits_);
            }
        }

        for (size_t j = 0; j < cand_.size(); ++ j)
            delete cand_[j];
        cand_.clear();
        if (sep && myparts.size() > 1)
            return ibis::filter::sift2S(sel, myparts, *wc_, hits_);
        else
            return ibis::filter::sift2(sel, myparts, *wc_, hits_);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a bad_alloc ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::filter::select

ibis::table*
ibis::filter::select(const ibis::table::stringArray& colnames) const {
    const ibis::constPartList &myparts =
        (parts_ ? *parts_ :
         reinterpret_cast<const constPartList&>(ibis::datasets));
    ibis::selectClause sc(colnames);
    if (sc.empty()) {
        return new tabula(count());
    }
    try {
        const bool sep = sc.isSeparable();
        if (wc_ == 0 || wc_->empty()) {
            if (sep && myparts.size() > 1)
                return ibis::filter::sift0S(sc, myparts);
            else
                return ibis::filter::sift0(sc, myparts);
        }

        if (hits_.size() == myparts.size()) {
            bool exact = true;
            for (size_t j = 0; j < cand_.size() && exact; ++ j)
                exact = (cand_[j] == 0);
            if (exact) {
                cand_.clear();
                if (sep && myparts.size() > 1)
                    return ibis::filter::sift2S(*sel_, myparts, hits_);
                else
                    return ibis::filter::sift2(*sel_, myparts, hits_);
            }
        }

        for (size_t j = 0; j < cand_.size(); ++ j) {
            delete hits_[j];
            delete cand_[j];
        }
        hits_.clear();
        cand_.clear();
        return ibis::filter::sift(sc, myparts, *wc_);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a bad_alloc ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- filter::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 1)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::filter::select

/// Select the rows satisfying the where clause and store the results in a
/// table object.  This function determine which of the various variations
/// to call based on the number of columns involved in the query and
/// whether the aggregation functions are separable or not.
ibis::table* ibis::filter::sift(const ibis::selectClause  &tms,
                                const ibis::constPartList &plist,
                                const ibis::whereClause   &cond) {
    if (plist.empty())
        return new ibis::tabula();
    if (tms.empty())
        return new
            ibis::tabula(ibis::table::computeHits(plist, cond.getExpr()));

    const bool separable = tms.isSeparable();
    if (cond.empty())
        return sift0(tms, plist);
    if (cond->getType() == ibis::qExpr::RANGE) {
        // involving a single variable with a simple range condition
        const char *tvar = tms.isUnivariate();
        if (tvar != 0 && 0 == 
            stricmp(tvar, static_cast<const ibis::qContinuousRange*>
                    (cond.getExpr())->colName())) {
                if (separable)
                    return ibis::filter::sift1S(tms, plist, cond);
                else
                    return ibis::filter::sift1(tms, plist, cond);
        }
    }

    if (separable && plist.size() > 1)
        return ibis::filter::sift2S(tms, plist, cond);
    else
        return ibis::filter::sift2(tms, plist, cond);
} // ibis::filter::sift

/// Select all rows from each data partition and place them in a table
/// object.  It concatenates the results from different data partitions in
/// the order of the data partitions given in mylist.
///
/// It expects both incoming arguments to be valid and non-trivial.  It
/// will return a nil pointer if those arguments are nil pointers or empty.
ibis::table* ibis::filter::sift0(const ibis::selectClause  &tms,
                                 const ibis::constPartList &plist) {
    long int ierr = 0;
    if (tms.empty() || plist.empty())
        return 0;

    std::string mesg = "filter::sift0";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "");
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // main loop through each data partition, fill the initial selection
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }

        ibis::bitvector msk;
        (*it)->getNullMask(msk);
        ierr = brd1->append(tms, **it, msk);
        LOGGER(ierr < 0 && ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to append " << msk.cnt()
            << " row" << (msk.cnt() > 1 ? "s" : "") << " from "
            << (*it)->name() << ", ierr = " << ierr;
        if (ierr < 0)
            return 0; // failed to process this partition
    }

    if (brd1.get() == 0) return 0;
    if (ibis::gVerbose > 2 && brd1.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " created an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
        }
    }
    if (brd1->nRows() == 0) {
        if (ierr >= 0) {
            // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd1->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd1->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd1->renameColumns(tms);
        return brd1.release();
    }

    std::unique_ptr<ibis::table> brd2(brd1->groupby(tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift0

/// Select all rows from each data partition and place them in a table
/// object.  It works with select clause containing separable aggregation
/// operations.
///
/// This function does check whether aggregations are separable!  The
/// caller need to make sure the aggregations are separable befor calling
/// this function.
///
/// @note As of 12/21/2011, Tomas Rybka introduced a logarithmic list of
/// accumulators in this function to reduce the number of times the
/// intermediate results are copied.  In this approach, all partitions are
/// merged to a similar-sized accumulator.  As their sizes grow, they are
/// merged into larger ones.  Finally, all of them are merged together into
/// the final partition.  This approach copies each row at most log(n)
/// times instead of n times as in the earlier implementation, where n is
/// the number of data partitions in plist.  However, because this appraoch
/// holds the partial results in memory for longer period of time,
/// therefore, it may require more memory than the previous version.
/// Overall this function should still takes less memory than the function
/// sift0.
ibis::table* ibis::filter::sift0S(const ibis::selectClause  &tms,
                                  const ibis::constPartList &plist) {
    long int ierr = 0;
    if (tms.empty() || plist.empty())
        return 0;

    std::string mesg = "filter::sift0S";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "");
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd0;
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // Fixed array of 64 partial aggr. accumulators,
    // for each accumulator A at index I applies: size(A) < 2*(2^I).
    // For each grouped partition, the proper index is found and merged into
    // the accumulator at the index, if it exists, and if its new size doesn't
    // match the rule, it is merged into the accumulator of higher degree.
    // At the end, everything is merged together, from smaller to larger.
    // Effect: during merge, every record is compared/copied <= log(n) times.
    std::unique_ptr<ibis::bord> merges[sizeof(uint64_t)*8];
    unsigned int mergesFirst = sizeof(merges)/sizeof(merges[0]),
        mergesLast = 0;

    // main loop through each data partition, fill the initial selection
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }

        ibis::bitvector msk;
        (*it)->getNullMask(msk);
        ierr = brd1->append(tms, **it, msk);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to append " << msk.cnt()
                << " row" << (msk.cnt() > 1 ? "s" : "") << " from "
                << (*it)->name() << ", ierr = " << ierr;
            return 0;
        }
        if (ierr > 0) {
            std::unique_ptr<ibis::bord> tmp(ibis::bord::groupbya(*brd1, tms));
            if (tmp.get() == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to evaluate the "
                    "aggregation operation on the results from data partition "
                    << (*it)->name();
                continue;
            }

            // find the merging accumulator index,
            // matching to the size of "merged-in" greouped partition.
            unsigned int lg2 = ibis::util::log2(tmp->nRows());
            if (lg2 < mergesFirst) mergesFirst = lg2;
            if (lg2 > mergesLast) mergesLast = lg2;

            if (merges[lg2].get() == 0) {
                // no matching merging accumulator found, fill in the degree.
                merges[lg2] = std::move(tmp);
            }
            else {
                // merge in the grouped partition
                ierr = merges[lg2]->merge(*tmp, tms);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg
                        << " failed to merge partial results, ierr = " << ierr;
                    return 0;
                }
                // let's check the fill factor, eventually do cascade merges.
                while (lg2 < sizeof(merges)/sizeof(merges[0]) - 1) {
                    // find the suitable merging accumulator index for
                    // the accumulator containing new data
                    unsigned newlg2 = ibis::util::log2(merges[lg2]->nRows());
                    // if it still matches the current index, no cascade merge
                    if (newlg2 <= lg2)
                        break;
                    // cascade merge is about to happen
                    if (merges[newlg2].get() == 0) {
                        // no accumulator exists yet at the index, just move
                        merges[newlg2] = std::move(merges[lg2]);
                    }
                    else {
                        // merge in the lower degree accumulator
                        ierr = merges[newlg2]->merge(*merges[lg2], tms);
                        if (ierr < 0) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- " << mesg
                                << " failed to merge partial results, ierr = "
                                << ierr;
                            return 0;
                        }
                        // lower degree accumulator merged, free it up
                        merges[lg2].reset(0);
                    }
                    // let's continue with the accumulator at the new index
                    lg2 = newlg2;
                    if (lg2 > mergesLast) mergesLast = lg2;
                }
            }
        }
        brd1->limit(0);
    }

    // merge all accumulators, used ones are within interval
    // [mergesFirst..mergesLast] only.
    // Walk from smaller to larger accumulators.
    for (unsigned j = mergesFirst; j <= mergesLast; ++j) {
        if (merges[j].get() != 0) {
            // the smallest accumulator found, let's use it as base one
            brd0 = std::move(merges[j]);
            // process all the other accumulators, until we reach the end
            while (++j <= mergesLast) {
                // the slot may not be used, let's check
                if (merges[j].get() != 0) {
                    // slot is used, so merge it with the result
                    ierr = merges[j]->merge(*brd0, tms);
                    if (ierr < 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << mesg
                            << " failed to merge partial results, ierr = "
                            << ierr;
                        return 0;
                    }
                    // the result is the merged accumulator now
                    brd0 = std::move(merges[j]);
                }
            }
            break;
        }
    }

    if (brd0.get() == 0) // no answer
        return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
    if (ibis::gVerbose > 2 && brd0.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " completed per partition aggregation to produce "
             << brd0->nRows() << " row" << (brd0->nRows()>1?"s":"")
             << " and " << brd0->nColumns() << " column"
             << (brd0->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd0->describe(lg());
        }
    }
    if (brd0->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd0->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd0->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd0->renameColumns(tms);
        // brd0->restoreCategoriesAsStrings(*plist.front());
        return brd0.release();
    }

    std::unique_ptr<ibis::table> brd2(ibis::bord::groupbyc(*brd0, tms));
    // if (brd2.get() != 0)
    //  static_cast<ibis::bord*>(brd2.get())
    //      ->restoreCategoriesAsStrings(*plist.front());
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift0S

/// Select the rows satisfying the where clause and store the results
/// in a table object.  It concatenates the results from different data
/// partitions in the order of the data partitions given in mylist.
///
/// This version is intended to work with only one column of raw data in
/// the select clause and one term in the where clause, the where clause
/// must be a simple range expression, and the column involved in these
/// clauses are expected to be the same.  If any of these conditions is not
/// satisfied, it returns a nil pointer.
ibis::table* ibis::filter::sift1(const ibis::selectClause  &tms,
                                 const ibis::constPartList &plist,
                                 const ibis::whereClause   &cond) {
    long int ierr = 0;
    if (plist.empty() || cond->getType() != ibis::qExpr::RANGE) {
        return 0;
    }
    else {
        const char* tvar = tms.isUnivariate();
        if (tvar == 0)
            return 0;
        else if (0 != stricmp(tvar,
                              static_cast<const ibis::qContinuousRange*>
                              (cond.getExpr())->colName()))
            return 0;
    }

    std::string mesg = "filter::sift1";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 30; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // main loop through each data partition, fill the initial selection
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }
        ierr = brd1->append(tms, **it,
                            *static_cast<const ibis::qContinuousRange*>
                            (cond.getExpr()));
        LOGGER(ierr < 0 && ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to append rows satisfying "
            << cond << " from " << (*it)->name() << ", ierr = " << ierr;
        if (ierr < 0)
            return 0;
    }

    if (brd1.get() == 0) return 0;
    if (ibis::gVerbose > 2 && brd1.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " created an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
        }
    }
    if (brd1->nRows() == 0) {
        if (ierr >= 0) {
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd1->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd1->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd1->renameColumns(tms);
        return brd1.release();
    }

    std::unique_ptr<ibis::table> brd2(brd1->groupby(tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift1

/// Perform the filter operation involving one column only.  The operations
/// in the select clause are all separable.  The caller is to make sure the
/// where clause and the select clause involve one column and the
/// aggregations operations in the select clause are separable.  If any of
/// these conditions are violated, this function will return a nil pointer.
ibis::table* ibis::filter::sift1S(const ibis::selectClause  &tms,
                                  const ibis::constPartList &plist,
                                  const ibis::whereClause   &cond) {
    long int ierr = 0;
    if (plist.empty() || cond->getType() != ibis::qExpr::RANGE) {
        return 0;
    }
    else {
        const char* tvar = tms.isUnivariate();
        if (tvar == 0)
            return 0;
        else if (0 != stricmp(tvar,
                              static_cast<const ibis::qContinuousRange*>
                              (cond.getExpr())->colName()))
            return 0;
    }

    std::string mesg = "filter::sift1S";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 30; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd0;
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // Fixed array of 64 partial aggr. accumulators,
    // for each accumulator A at index I applies: size(A) < 2*(2^I).
    // For each grouped partition, the proper index is found and merged into
    // the accumulator at the index, if it exists, and if its new size doesn't
    // match the rule, it is merged into the accumulator of higher degree.
    // At the end, everything is merged together, from smaller to larger.
    // Effect: during merge, every record is compared/copied <= log(n) times.
    std::unique_ptr<ibis::bord> merges[sizeof(uint64_t)*8];
    unsigned int mergesFirst = sizeof(merges)/sizeof(merges[0]),
        mergesLast = 0;

    // main loop through each data partition, fill the initial selection
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }
        ierr = brd1->append(tms, **it,
                            *static_cast<const ibis::qContinuousRange*>
                            (cond.getExpr()));
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to append rows satisfying "
                << cond << " from " << (*it)->name() << ", ierr = " << ierr;
            return 0;
        }
        if (ierr > 0) {
            std::unique_ptr<ibis::bord> tmp(ibis::bord::groupbya(*brd1, tms));
            if (tmp.get() == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to evaluate the "
                    "aggregation operation on the results from data partition "
                    << (*it)->name();
                continue;
            }

            // find the merging accumulator index,
            // matching to the size of "merged-in" greouped partition.
            unsigned int lg2 = ibis::util::log2(tmp->nRows());
            if (lg2 < mergesFirst) mergesFirst = lg2;
            if (lg2 > mergesLast) mergesLast = lg2;

            if (merges[lg2].get() == 0) {
                // no matching merging accumulator found, fill in the degree.
                merges[lg2] = std::move(tmp);
            }
            else {
                // merge in the grouped partition
                ierr = merges[lg2]->merge(*tmp, tms);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg
                        << " failed to merge partial results, ierr = " << ierr;
                    return 0;
                }
                // let's check the fill factor, eventually do cascade merges.
                while (lg2 < sizeof(merges)/sizeof(merges[0]) - 1) {
                    // find the suitable merging accumulator index for
                    // the accumulator containing new data
                    unsigned newlg2 = ibis::util::log2(merges[lg2]->nRows());
                    // if it still matches the current index, no cascade merge
                    if (newlg2 <= lg2)
                        break;
                    // cascade merge is about to happen
                    if (merges[newlg2].get() == 0) {
                        // no accumulator exists yet at the index, just move
                        merges[newlg2] = std::move(merges[lg2]);
                    }
                    else {
                        // merge in the lower degree accumulator
                        ierr = merges[newlg2]->merge(*merges[lg2], tms);
                        if (ierr < 0) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- " << mesg
                                << " failed to merge partial results, ierr = "
                                << ierr;
                            return 0;
                        }
                        // lower degree accumulator merged, free it up
                        merges[lg2].reset(0);
                    }
                    // let's continue with the accumulator at the new index
                    lg2 = newlg2;
                    if (lg2 > mergesLast) mergesLast = lg2;
                }
            }
        }
        brd1->limit(0);
    }

    // merge all accumulators, used ones are within interval
    // [mergesFirst..mergesLast] only.
    // Walk from smaller to larger accumulators.
    for (unsigned j = mergesFirst; j <= mergesLast; ++j) {
        if (merges[j].get() != 0) {
            // the smallest accumulator found, let's use it as base one
            brd0 = std::move(merges[j]);
            // process all the other accumulators, until we reach the end
            while (++j <= mergesLast) {
                // the slot may not be used, let's check
                if (merges[j].get() != 0) {
                    // slot is used, so merge it with the result
                    ierr = merges[j]->merge(*brd0, tms);
                    if (ierr < 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << mesg
                            << " failed to merge partial results, ierr = "
                            << ierr;
                        return 0;
                    }
                    // the result is the merged accumulator now
                    brd0 = std::move(merges[j]);
                }
            }
            break;
        }
    }

    if (brd0.get() == 0) // no answer
        return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << mesg << " created an in-memory data partition with "
             << brd0->nRows() << " row" << (brd0->nRows()>1?"s":"")
             << " and " << brd0->nColumns() << " column"
             << (brd0->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd0->describe(lg());
        }
    }
    if (brd0->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd0->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd0->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd0->renameColumns(tms);
        return brd0.release();
    }

    std::unique_ptr<ibis::table> brd2(ibis::bord::groupbyc(*(brd0.get()), tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift1S

/// Select the rows satisfying the where clause and store the results in a
/// table object.  There can be arbitrary number of columns involved in the
/// where clause and the select clause.  It concatenates the results from
/// different data partitions in the order of the data partitions given in
/// mylist and therefore requires more memory than sift2S.
///
/// It expects all three arguments to be valid and non-trivial.  It will
/// return a nil pointer if those arguments are nil pointers or empty.
ibis::table* ibis::filter::sift2(const ibis::selectClause  &tms,
                                 const ibis::constPartList &plist,
                                 const ibis::whereClause   &cond) {
    if (plist.empty())
        return new ibis::tabula();
    if (tms.empty())
        return new
            ibis::tabula(ibis::table::computeHits(plist, cond.getExpr()));
    if (cond.empty())
        return sift0(tms, plist);

    std::string mesg = "filter::sift2";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    // a single query object is used for different data partitions
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond.getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to assign externally "
            "provided query expression \"" << cond
            << "\" to a countQuery object, ierr=" << ierr;
        return 0;
    }

    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // main loop through each data partition, fill the initial selection
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }
        ierr = qq.setSelectClause(&tms);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to modify the select clause of "
                << "the countQuery object (" << qq.getWhereClause()
                << ") on data partition " << (*it)->name();
            ierr = -12;
            continue;
        }

        ierr = qq.setPartition(*it);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- query.setPartition(" << (*it)->name()
                << ") failed with error code " << ierr;
            ierr = -13;
            continue;
        }

        ierr = qq.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to process query on data partition "
                << (*it)->name();
            ierr = -14;
            continue;
        }

        const ibis::bitvector* hits = qq.getHitVector();
        if (hits == 0 || hits->cnt() == 0) continue;

        ierr = brd1->append(tms, **it, *hits);
        LOGGER(ierr < 0 && ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to append " << hits->cnt()
            << " row" << (hits->cnt() > 1 ? "s" : "") << " from "
            << (*it)->name() << ", ierr = " << ierr;
    }

    if (brd1.get() == 0) return 0;
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << mesg << " created an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
            uint32_t nr = (ibis::gVerbose < 30 ?
                           1U << ibis::gVerbose :
                           brd1->nRows());
            if (nr > brd1->nRows()/2) {
                brd1->dump(lg(), ", ");
            }
            else {
                lg() << "\t... first " << nr << " row" << (nr>1?"s":"") << "\n";
                brd1->dump(lg(), nr, ", ");
                lg() << "\t... skipping " << brd1->nRows() - nr;
            }
        }
    }
    if (brd1->nRows() == 0) {
        if (ierr >= 0) {
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd1->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd1->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd1->renameColumns(tms);
        return brd1.release();
    }

    std::unique_ptr<ibis::table> brd2(brd1->groupby(tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
            uint32_t nr = (ibis::gVerbose < 30 ? 1U << ibis::gVerbose :
                           brd2->nRows());
            if (nr > brd2->nRows()/2) {
                brd2->dump(lg(), ", ");
            }
            else {
                lg() << "\t... first " << nr << " row" << (nr>1?"s":"") << "\n";
                brd2->dump(lg(), nr, ", ");
                lg() << "\t... skipping " << brd2->nRows() - nr;
            }
        }
    }
    return brd2.release();
} // ibis::filter::sift2

/// Select the rows satisfying the where clause and store the results
/// in a table object.  It concatenates the results from different data
/// partitions in the order of the data partitions given in mylist.
///
/// This verison takes the existing solutions as the 3rd argument instead
/// of a set of query conditions.
ibis::table* ibis::filter::sift2(const ibis::selectClause  &tms,
                                 const ibis::constPartList &plist,
                                 const ibis::array_t<ibis::bitvector*> &hits) {
    if (plist.empty())
        return new ibis::tabula();
    if (plist.size() != hits.size())
        return 0;
    if (tms.empty()) {
        uint64_t nhits = 0;
        for (size_t j = 0; j < hits.size(); ++ j)
            if (hits[j] != 0)
                nhits += hits[j]->cnt();
        return new ibis::tabula(nhits);
    }

    std::string mesg = "filter::sift2";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE ...";
        mesg += oss.str();
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // main loop through each data partition, fill the initial selection
    for (unsigned j = 0; j < plist.size(); ++ j) {
        const ibis::bitvector* hv = hits[j];
        if (hv == 0 || hv->cnt() == 0) continue;

        ierr = tms.verify(*plist[j]);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << plist[j]->name();
            ierr = -11;
            continue;
        }

        ierr = brd1->append(tms, *plist[j], *hv);
        LOGGER(ierr < 0 && ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to append " << hv->cnt()
            << " row" << (hv->cnt() > 1 ? "s" : "") << " from "
            << plist[j]->name() << ", ierr = " << ierr;
    }

    if (brd1.get() == 0) return 0;
    if (ibis::gVerbose > 2 && brd1.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " creates an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
        }
    }
    if (brd1->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd1->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd1->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd1->renameColumns(tms);
        return brd1.release();
    }

    std::unique_ptr<ibis::table> brd2(brd1->groupby(tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produces an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift2

/// Select the rows satisfying the where clause and store the results
/// in a table object.  It concatenates the results from different data
/// partitions in the order of the data partitions given in mylist.
///
/// This version records the bitvectors generated as the intermediate
/// solutions.
ibis::table* ibis::filter::sift2(const ibis::selectClause        &tms,
                                 const ibis::constPartList       &plist,
                                 const ibis::whereClause         &cond,
                                 ibis::array_t<ibis::bitvector*> &hits) {
    ibis::util::clear(hits);
    if (plist.empty())
        return new ibis::tabula();
    if (tms.empty())
        return new
            ibis::tabula(ibis::table::computeHits(plist, cond.getExpr()));
    if (cond.empty())
        return sift0(tms, plist);

    std::string mesg = "filter::sift2";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    // a single query object is used for different data partitions
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond.getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to assign externally "
            "provided query expression \"" << cond
            << "\" to a countQuery object, ierr=" << ierr;
        return 0;
    }

    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    hits.reserve(plist.size());
    // main loop through each data partition, fill the initial selection
    for (size_t j = 0; j < plist.size(); ++ j) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << plist[j]->name();
        ierr = tms.verify(*plist[j]);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << plist[j]->name();
            ierr = -11;
            continue;
        }
        ierr = qq.setSelectClause(&tms);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to modify the select clause of "
                << "the countQuery object (" << qq.getWhereClause()
                << ") on data partition " << plist[j]->name();
            ierr = -12;
            continue;
        }

        ierr = qq.setPartition(plist[j]);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- query.setPartition(" << plist[j]->name()
                << ") failed with error code " << ierr;
            ierr = -13;
            continue;
        }

        ierr = qq.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to process query on data partition "
                << plist[j]->name();
            ierr = -14;
            continue;
        }

        const ibis::bitvector* hv = qq.getHitVector();
        if (hv == 0 || hv->cnt() == 0) continue;

        while (hits.size() < j)
            hits.push_back(0);
        if (hits.size() == j) {
            hits.push_back(new ibis::bitvector(*hv));
        }
        else if (hits[j] != 0) {
            hits[j]->copy(*hv);
        }
        else {
            hits[j] = new ibis::bitvector(*hv);
        }
        ierr = brd1->append(tms, *plist[j], *hv);
        LOGGER(ierr < 0 && ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to append " << hv->cnt()
            << " row" << (hv->cnt() > 1 ? "s" : "") << " from "
            << plist[j]->name() << ", ierr = " << ierr;
        if (ierr < 0)
            return 0;
    }

    if (brd1.get() == 0) return 0;
    if (ibis::gVerbose > 2 && brd1.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " creates an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
        }
    }
    if (brd1->nRows() == 0) {
        if (ierr >= 0) {
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd1->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd1->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd1->renameColumns(tms);
        return brd1.release();
    }

    std::unique_ptr<ibis::table> brd2(brd1->groupby(tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produces an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift2

/// Select the rows satisfying the where clause and store the results in a
/// table object.  This version can accept an arbitrary where clause.  It
/// assumes the select clause can be evaluated one partition at a time, in
/// other word, the aggregations are separable.
ibis::table* ibis::filter::sift2S(const ibis::selectClause  &tms,
                                  const ibis::constPartList &plist,
                                  const ibis::whereClause   &cond) {
    if (plist.empty())
        return new ibis::tabula();
    if (tms.empty())
        return new
            ibis::tabula(ibis::table::computeHits(plist, cond.getExpr()));
    if (cond.empty())
        return sift0(tms, plist);

    std::string mesg = "filter::sift2S";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    // a single query object is used for different data partitions
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond.getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to assign externally "
            "provided query expression \"" << cond
            << "\" to a countQuery object, ierr=" << ierr;
        return 0;
    }

    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd0;
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // Fixed array of 64 partial aggr. accumulators,
    // for each accumulator A at index I applies: size(A) < 2*(2^I).
    // For each grouped partition, the proper index is found and merged into
    // the accumulator at the index, if it exists, and if its new size doesn't
    // match the rule, it is merged into the accumulator of higher degree.
    // At the end, everything is merged together, from smaller to larger.
    // Effect: during merge, every record is compared/copied <= log(n) times.
    std::unique_ptr<ibis::bord> merges[sizeof(uint64_t)*8];
    unsigned int mergesFirst = sizeof(merges)/sizeof(merges[0]),
        mergesLast = 0;

    // main loop through each data partition
    for (ibis::constPartList::const_iterator it = plist.begin();
         it != plist.end(); ++ it) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << (*it)->name();
        ierr = tms.verify(**it);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << (*it)->name();
            ierr = -11;
            continue;
        }
        ierr = qq.setSelectClause(&tms);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to modify the select clause of "
                << "the countQuery object (" << qq.getWhereClause()
                << ") on data partition " << (*it)->name();
            ierr = -12;
            continue;
        }

        ierr = qq.setPartition(*it);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- query.setPartition(" << (*it)->name()
                << ") failed with error code " << ierr;
            ierr = -13;
            continue;
        }

        ierr = qq.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to process query on data partition "
                << (*it)->name();
            ierr = -14;
            continue;
        }

        const ibis::bitvector* hits = qq.getHitVector();
        if (hits == 0 || hits->cnt() == 0) continue;

        ierr = brd1->append(tms, **it, *hits);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to append " << hits->cnt()
                << " row" << (hits->cnt() > 1 ? "s" : "") << " from "
                << (*it)->name() << ", ierr = " << ierr;
            return 0;
        }
        if (ierr > 0) {
            std::unique_ptr<ibis::bord> tmp(ibis::bord::groupbya(*brd1, tms));
            if (tmp.get() == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to evaluate the "
                    "aggregation operation on the results from data partition "
                    << (*it)->name();
                continue;
            }

            // find the merging accumulator index,
            // matching to the size of "merged-in" grouped partition.
            unsigned int lg2 = ibis::util::log2(tmp->nRows());
            if (lg2 < mergesFirst) mergesFirst = lg2;
            if (lg2 > mergesLast) mergesLast = lg2;

            if (merges[lg2].get() == 0) {
                // no matching merging accumulator found, fill in the degree.
                merges[lg2] = std::move(tmp);
            }
            else {
                // merge in the grouped partition
                ierr = merges[lg2]->merge(*tmp, tms);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg
                        << " failed to merge partial results, ierr = " << ierr;
                    return 0;
                }
                // let's check the fill factor, eventually do cascade merges.
                while (lg2 < sizeof(merges)/sizeof(merges[0]) - 1) {
                    // find the suitable merging accumulator index for
                    // the accumulator containing new data
                    unsigned newlg2 = ibis::util::log2(merges[lg2]->nRows());
                    // if it still matches the current index, no cascade merge
                    if (newlg2 <= lg2)
                        break;
                    // cascade merge is about to happen
                    if (merges[newlg2].get() == 0) {
                        // no accumulator exists yet at the index, just move
                        merges[newlg2] = std::move(merges[lg2]);
                    }
                    else {
                        // merge in the lower degree accumulator
                        ierr = merges[newlg2]->merge(*merges[lg2], tms);
                        if (ierr < 0) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- " << mesg
                                << " failed to merge partial results, ierr = "
                                << ierr;
                            return 0;
                        }
                        // lower degree accumulator merged, free it up
                        merges[lg2].reset(0);
                    }
                    // let's continue with the accumulator at the new index
                    lg2 = newlg2;
                    if (lg2 > mergesLast) mergesLast = lg2;
                }
            }
        }
        brd1->limit(0);
    }

    // merge all accumulators, used ones are within interval
    // [mergesFirst..mergesLast] only.
    // Walk from smaller to larger accumulators.
    for (unsigned j = mergesFirst; j <= mergesLast; ++j) {
        if (merges[j].get() != 0) {
            // the smallest accumulator found, let's use it as base one
            brd0 = std::move(merges[j]);
            // process all the other accumulators, until we reach the end
            while (++j <= mergesLast) {
                // the slot may not be used, let's check
                if (merges[j].get() != 0) {
                    // slot is used, so merge it with the result
                    ierr = merges[j]->merge(*brd0, tms);
                    if (ierr < 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << mesg
                            << " failed to merge partial results, ierr = "
                            << ierr;
                        return 0;
                    }
                    // the result is the merged accumulator now
                    brd0 = std::move(merges[j]);
                }
            }
            break;
        }
    }

    if (brd0.get() == 0) // no answer
        return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << mesg << " created an in-memory data partition with "
             << brd0->nRows() << " row" << (brd0->nRows()>1?"s":"")
             << " and " << brd0->nColumns() << " column"
             << (brd0->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd0->describe(lg());
        }
    }
    if (brd0->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd0->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd0->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd0->renameColumns(tms);
        return brd0.release();
    }

    std::unique_ptr<ibis::table> brd2(ibis::bord::groupbyc(*(brd0.get()), tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produced an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift2S

/// Select the rows satisfying the where clause and store the results
/// in a table object.  It performs the aggregations on each data partition
/// separately.  This is only valid if the aggregations in the select
/// clause is indeed separable.  The caller is responsible for making sure
/// the select clause is separable.
ibis::table* ibis::filter::sift2S
(const ibis::selectClause  &tms, const ibis::constPartList &plist,
 const ibis::array_t<ibis::bitvector*> &hits) {
    if (plist.empty())
        return new ibis::tabula();
    if (plist.size() != hits.size())
        return 0;
    if (tms.empty()) {
        uint64_t nhits = 0;
        for (size_t j = 0; j < hits.size(); ++ j)
            if (hits[j] != 0)
                nhits += hits[j]->cnt();
        return new ibis::tabula(nhits);
    }

    std::string mesg = "filter::sift2S";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE ...";
        mesg += oss.str();
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd0;
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    // Fixed array of 64 partial aggr. accumulators,
    // for each accumulator A at index I applies: size(A) < 2*(2^I).
    // For each grouped partition, the proper index is found and merged into
    // the accumulator at the index, if it exists, and if its new size doesn't
    // match the rule, it is merged into the accumulator of higher degree.
    // At the end, everything is merged together, from smaller to larger.
    // Effect: during merge, every record is compared/copied <= log(n) times.
    std::unique_ptr<ibis::bord> merges[sizeof(uint64_t)*8];
    unsigned int mergesFirst = sizeof(merges)/sizeof(merges[0]),
        mergesLast = 0;

    // main loop through each data partition
    for (unsigned j = 0; j < plist.size(); ++ j) {
        const ibis::bitvector* hv = hits[j];
        if (hv == 0 || hv->cnt() == 0) continue;

        ierr = tms.verify(*plist[j]);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << plist[j]->name();
            ierr = -11;
            continue;
        }

        ierr = brd1->append(tms, *plist[j], *hv);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to append " << hv->cnt()
                << " row" << (hv->cnt() > 1 ? "s" : "") << " from "
                << plist[j]->name() << ", ierr = " << ierr;
            return 0;
        }
        if (ierr > 0) {
            std::unique_ptr<ibis::bord> tmp(ibis::bord::groupbya(*brd1, tms));
            if (tmp.get() == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to evaluate the "
                    "aggregation operation on the results from data partition "
                    << plist[j]->name();
                continue;
            }

            // find the merging accumulator index,
            // matching to the size of "merged-in" greouped partition.
            unsigned int lg2 = ibis::util::log2(tmp->nRows());
            if (lg2 < mergesFirst) mergesFirst = lg2;
            if (lg2 > mergesLast) mergesLast = lg2;

            if (merges[lg2].get() == 0) {
                // no matching merging accumulator found, fill in the degree.
                merges[lg2] = std::move(tmp);
            }
            else {
                // merge in the grouped partition
                ierr = merges[lg2]->merge(*tmp, tms);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg
                        << " failed to merge partial results, ierr = " << ierr;
                    return 0;
                }
                // let's check the fill factor, eventually do cascade merges.
                while (lg2 < sizeof(merges)/sizeof(merges[0]) - 1) {
                    // find the suitable merging accumulator index for
                    // the accumulator containing new data
                    unsigned newlg2 = ibis::util::log2(merges[lg2]->nRows());
                    // if it still matches the current index, no cascade merge
                    if (newlg2 <= lg2)
                        break;
                    // cascade merge is about to happen
                    if (merges[newlg2].get() == 0) {
                        // no accumulator exists yet at the index, just move
                        merges[newlg2] = std::move(merges[lg2]);
                    }
                    else {
                        // merge in the lower degree accumulator
                        ierr = merges[newlg2]->merge(*merges[lg2], tms);
                        if (ierr < 0) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- " << mesg
                                << " failed to merge partial results, ierr = "
                                << ierr;
                            return 0;
                        }
                        // lower degree accumulator merged, free it up
                        merges[lg2].reset(0);
                    }
                    // let's continue with the accumulator at the new index
                    lg2 = newlg2;
                    if (lg2 > mergesLast) mergesLast = lg2;
                }
            }
        }
        brd1->limit(0);
    }

    // merge all accumulators, used ones are within interval
    // [mergesFirst..mergesLast] only.
    // Walk from smaller to larger accumulators.
    for (unsigned j = mergesFirst; j <= mergesLast; ++j) {
        if (merges[j].get() != 0) {
            // the smallest accumulator found, let's use it as base one
            brd0 = std::move(merges[j]);
            // process all the other accumulators, until we reach the end
            while (++j <= mergesLast) {
                // the slot may not be used, let's check
                if (merges[j].get() != 0) {
                    // slot is used, so merge it with the result
                    ierr = merges[j]->merge(*brd0, tms);
                    if (ierr < 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << mesg
                            << " failed to merge partial results, ierr = "
                            << ierr;
                        return 0;
                    }
                    // the result is the merged accumulator now
                    brd0 = std::move(merges[j]);
                }
            }
            break;
        }
    }

    if (brd0.get() == 0) // no answer
        return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << mesg << " creates an in-memory data partition with "
             << brd0->nRows() << " row" << (brd0->nRows()>1?"s":"")
             << " and " << brd0->nColumns() << " column"
             << (brd0->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd0->describe(lg());
        }
    }
    if (brd0->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd0->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd0->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd0->renameColumns(tms);
        return brd0.release();
    }

    std::unique_ptr<ibis::table> brd2(ibis::bord::groupbyc(*(brd0.get()), tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produces an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift2S

/// Select the rows satisfying the where clause and store the results in a
/// table object.  It performs the aggregation operations one data
/// partition at a time.  This is only valid if the aggregations are indeed
/// separable.
ibis::table* ibis::filter::sift2S(const ibis::selectClause        &tms,
                                  const ibis::constPartList       &plist,
                                  const ibis::whereClause         &cond,
                                  ibis::array_t<ibis::bitvector*> &hits) {
    ibis::util::clear(hits);
    if (plist.empty())
        return new ibis::tabula();
    if (tms.empty())
        return new
            ibis::tabula(ibis::table::computeHits(plist, cond.getExpr()));
    if (cond.empty())
        return sift0(tms, plist);

    std::string mesg = "filter::sift2S";
    if (ibis::gVerbose > 0) {
        mesg += "(SELECT ";
        std::ostringstream oss;
        oss << tms;
        if (oss.str().size() <= 20) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 20; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        oss.clear();
        oss.str("");
        oss << " FROM " << plist.size() << " data partition"
            << (plist.size() > 1 ? "s" : "")
            << " WHERE " << cond;
        if (oss.str().size() <= 35) {
            mesg += oss.str();
        }
        else {
            for (unsigned j = 0; j < 35; ++ j)
                mesg += oss.str()[j];
            mesg += " ...";
        }
        mesg += ')';
    }

    long int ierr = 0;
    ibis::util::timer atimer(mesg.c_str(), 2);
    // a single query object is used for different data partitions
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond.getExpr());
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " failed to assign externally "
            "provided query expression \"" << cond
            << "\" to a countQuery object, ierr=" << ierr;
        return 0;
    }

    std::string tn = ibis::util::shortName(mesg);
    std::unique_ptr<ibis::bord> brd0;
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), mesg.c_str(), tms, plist));
    const uint32_t nplain = tms.numGroupbyKeys();
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << mesg << " -- processing a select clause with " << tms.aggSize()
             << " term" << (tms.aggSize()>1?"s":"") << ", " << nplain
             << " of which " << (nplain>1?"are":"is") << " plain";
        if (ibis::gVerbose > 6) {
            lg() << "\nTemporary data will be stored in the following:\n";
            brd1->describe(lg());
        }
    }

    hits.reserve(plist.size());

    // Fixed array of 64 partial aggr. accumulators,
    // for each accumulator A at index I applies: size(A) < 2*(2^I).
    // For each grouped partition, the proper index is found and merged into
    // the accumulator at the index, if it exists, and if its new size doesn't
    // match the rule, it is merged into the accumulator of higher degree.
    // At the end, everything is merged together, from smaller to larger.
    // Effect: during merge, every record is compared/copied <= log(n) times.
    std::unique_ptr<ibis::bord> merges[sizeof(uint64_t)*8];
    unsigned int mergesFirst = sizeof(merges)/sizeof(merges[0]),
        mergesLast = 0;

    // main loop through each data partition, fill the initial selection
    for (size_t j = 0; j < plist.size(); ++ j) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- processing data partition " << plist[j]->name();
        ierr = tms.verify(*plist[j]);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- select clause (" << tms
                << ") contains variables that are not in data partition "
                << plist[j]->name();
            ierr = -11;
            continue;
        }
        ierr = qq.setSelectClause(&tms);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to modify the select clause of "
                << "the countQuery object (" << qq.getWhereClause()
                << ") on data partition " << plist[j]->name();
            ierr = -12;
            continue;
        }

        ierr = qq.setPartition(plist[j]);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- query.setPartition(" << plist[j]->name()
                << ") failed with error code " << ierr;
            ierr = -13;
            continue;
        }

        ierr = qq.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << mesg << " -- failed to process query on data partition "
                << plist[j]->name();
            ierr = -14;
            continue;
        }

        const ibis::bitvector* hv = qq.getHitVector();
        if (hv == 0 || hv->cnt() == 0) continue;

        while (hits.size() < j)
            hits.push_back(0);
        if (hits.size() == j) {
            hits.push_back(new ibis::bitvector(*hv));
        }
        else if (hits[j] != 0) {
            hits[j]->copy(*hv);
        }
        else {
            hits[j] = new ibis::bitvector(*hv);
        }
        ierr = brd1->append(tms, *plist[j], *hv);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to append " << hv->cnt()
                << " row" << (hv->cnt() > 1 ? "s" : "") << " from "
                << plist[j]->name() << ", ierr = " << ierr;
            return 0;
        }
        if (ierr > 0) {
            std::unique_ptr<ibis::bord> tmp(ibis::bord::groupbya(*brd1, tms));
            if (tmp.get() == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to evaluate the "
                    "aggregation operation on the results from data partition "
                    << plist[j]->name();
                continue;
            }

            // find the merging accumulator index,
            // matching to the size of "merged-in" greouped partition.
            unsigned int lg2 = ibis::util::log2(tmp->nRows());
            if (lg2 < mergesFirst) mergesFirst = lg2;
            if (lg2 > mergesLast) mergesLast = lg2;

            if (merges[lg2].get() == 0) {
                // no matching merging accumulator found, fill in the degree.
                merges[lg2] = std::move(tmp);
            }
            else {
                // merge in the grouped partition
                ierr = merges[lg2]->merge(*tmp, tms);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg
                        << " failed to merge partial results, ierr = " << ierr;
                    return 0;
                }
                // let's check the fill factor, eventually do cascade merges.
                while (lg2 < sizeof(merges)/sizeof(merges[0]) - 1) {
                    // find the suitable merging accumulator index for
                    // the accumulator containing new data
                    unsigned newlg2 = ibis::util::log2(merges[lg2]->nRows());
                    // if it still matches the current index, no cascade merge
                    if (newlg2 <= lg2)
                        break;
                    // cascade merge is about to happen
                    if (merges[newlg2].get() == 0) {
                        // no accumulator exists yet at the index, just move
                        merges[newlg2] = std::move(merges[lg2]);
                    }
                    else {
                        // merge in the lower degree accumulator
                        ierr = merges[newlg2]->merge(*merges[lg2], tms);
                        if (ierr < 0) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- " << mesg
                                << " failed to merge partial results, ierr = "
                                << ierr;
                            return 0;
                        }
                        // lower degree accumulator merged, free it up
                        merges[lg2].reset(0);
                    }
                    // let's continue with the accumulator at the new index
                    lg2 = newlg2;
                    if (lg2 > mergesLast) mergesLast = lg2;
                }
            }
        }
        brd1->limit(0);
    }

    // merge all accumulators, used ones are within interval
    // [mergesFirst..mergesLast] only.
    // Walk from smaller to larger accumulators.
    for (unsigned j = mergesFirst; j <= mergesLast; ++j) {
        if (merges[j].get() != 0) {
            // the smallest accumulator found, let's use it as base one
            brd0 = std::move(merges[j]);
            // process all the other accumulators, until we reach the end
            while (++j <= mergesLast) {
                // the slot may not be used, let's check
                if (merges[j].get() != 0) {
                    // slot is used, so merge it with the result
                    ierr = merges[j]->merge(*brd0, tms);
                    if (ierr < 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << mesg
                            << " failed to merge partial results, ierr = "
                            << ierr;
                        return 0;
                    }
                    // the result is the merged accumulator now
                    brd0 = std::move(merges[j]);
                }
            }
            break;
        }
    }

    if (brd0.get() == 0) // no answer
        return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << mesg << " creates an in-memory data partition with "
             << brd0->nRows() << " row" << (brd0->nRows()>1?"s":"")
             << " and " << brd0->nColumns() << " column"
             << (brd0->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd0->describe(lg());
        }
    }
    if (brd0->nRows() == 0) {
        if (ierr >= 0) { // return an empty table of type tabula
            return new ibis::tabula(tn.c_str(), mesg.c_str(), 0);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " failed to produce any result, "
                "the last error code was " << ierr;
            return 0;
        }
    }
    else if (brd0->nColumns() == 0) { // count(*)
        return new ibis::tabele(tn.c_str(), mesg.c_str(), brd0->nRows(),
                                tms.termName(0));
    }

    if (nplain >= tms.aggSize()) {
        brd0->renameColumns(tms);
        return brd0.release();
    }

    std::unique_ptr<ibis::table> brd2(ibis::bord::groupbyc(*(brd0.get()), tms));
    if (ibis::gVerbose > 2 && brd2.get() != 0) {
        ibis::util::logger lg;
        lg() << mesg << " produces an in-memory data partition with "
             << brd2->nRows() << " row" << (brd2->nRows()>1?"s":"")
             << " and " << brd2->nColumns() << " column"
             << (brd2->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd2->describe(lg());
        }
    }
    return brd2.release();
} // ibis::filter::sift2S

/// Upon successful completion of this function, it produces an in-memory
/// data partition holding the selected data records.  It will fail in a
/// unpredictable way if the selected records can not fit in the available
/// memory.
///
/// If the select clause is missing, the return table will have no columns
/// and the number of rows is the number of rows satisfying the query
/// conditions.  An empty query condition matches all rows following the
/// SQL convension.
ibis::table* ibis::table::select(const ibis::constPartList& mylist,
                                 const char *sel, const char *cond) {
    try {
        if (mylist.empty())
            return new ibis::tabula(); // return an empty unnamed table

        if (sel == 0 || *sel == 0)
            return new ibis::tabula(ibis::table::computeHits(mylist, cond));

        ibis::selectClause sc(sel);
        if (sc.empty())
            return new ibis::tabula(ibis::table::computeHits(mylist, cond));

        if (cond == 0 || *cond == 0)
            return ibis::filter::sift0(sc, mylist);

        ibis::whereClause wc(cond);
        return ibis::filter::sift(sc, mylist, wc);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a bad_alloc exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::table::select

/// Upon successful completion of this function, it produces an in-memory
/// data partition holding the selected data records.  It will fail in an
/// unpredictable way if the selected records can not fit in available
/// memory.
///
/// If the select clause is missing, the return table will have no columns
/// and the number of rows is the number of rows satisfying the query
/// conditions.  An empty query condition matches all rows following the
/// SQL convension.
ibis::table* ibis::table::select(const ibis::constPartList& plist,
                                 const char *sel, const ibis::qExpr *cond) {
    try {
        if (plist.empty())
            return new ibis::tabula(); // return an empty unnamed table

        if (sel == 0 || *sel == 0)
            return new ibis::tabula(ibis::table::computeHits(plist, cond));

        ibis::selectClause sc(sel);
        if (sc.empty())
            return new ibis::tabula(ibis::table::computeHits(plist, cond));

        if (cond == 0)
            return ibis::filter::sift0(sc, plist);

        ibis::whereClause wc;
        wc.setExpr(cond);
        return ibis::filter::sift(sc, plist, wc);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a bad_alloc exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 3)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::table::select
