// File $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2008-2016 the Regents of the University of California
//
//  The implementation of class ibis::countQuery.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "countQuery.h"         // class countQuery
#include "query.h"              // ibis::query

#include <memory>       // std::unique_ptr
#include <sstream>      // std::ostringstream

////////////////////////////////////////////////////////////
// public functions of ibis::countQuery
////////////////////////////////////////////////////////////

/// Integer error code:
///  0: successful completion of the requested operation.
/// -1: nil pointer to data partition or empty partition.
/// -2: invalid string for select clause.
/// -3: select clause contains invalid column name.
/// -4: invalid string for where clause.
/// -5: where clause can not be parsed correctly.
/// -6: where clause contains invalid column names or unsupported functions.
/// -7: empty rid list for set rid operation.
/// -8: neither rids nor range conditions are set.
/// -9: encountered some exceptional conditions during query evaluations.
/// -10: no private directory to store bundles.
/// -11: Query not fully evaluated.
int ibis::countQuery::setPartition(const part* tbl) {
    if (tbl == 0) return -1;
    if (tbl == mypart) return 0;
    if (tbl->nRows() == 0 || tbl->nColumns() == 0 || tbl->name() == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::setPartition will not use an empty "
            "data partition";
        return -1;
    }

    if (! conds.empty()) {
        int ierr = conds.verify(*tbl, m_sel);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- countQuery where clause \"" << conds
                << "\" can not be processed on data partition " << tbl->name()
                << ", ierr = " << ierr;
            return -6;
        }
    }

    if (mypart != 0)
        LOGGER(ibis::gVerbose > 1)
            << "countQuery changing data partition from "
            << mypart->name() << " to " << tbl->name();
    else
        LOGGER(ibis::gVerbose > 1)
            << "countQuery assigned data partition " << tbl->name();
    mypart = tbl;
    if (hits == cand) {
        delete hits;
        hits = 0;
        cand = 0;
    }
    else {
        delete hits;
        delete cand;
        hits = 0;
        cand = 0;
    }
    return 0;
} // ibis::countQuery::setPartition

/// The where clause is a string representing a list of range conditions.
/// A where clause is mandatory if a query is to be estimated or evaluated.
/// This function may be called multiple times and each invocation will
/// overwrite the previous where clause.
int ibis::countQuery::setWhereClause(const char* str) {
    if (str == 0 || *str == static_cast<char>(0)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::setWhereClause will not use an empty "
            "where clause";
        return -4; // invalid input where clause
    }
    if (conds.getString() != 0 && stricmp(conds.getString(), str) == 0)
        return 0; // no change in where clause
    int ret = 0;
    try {
        ibis::whereClause tmp(str);
        if (tmp.getExpr() == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- countQuery::setWhereClause failed to "
                "parse \"" << str << "\"";
            return -5;
        }
        if (mypart != 0) {
            int ierr = tmp.verify(*mypart, m_sel);
            if (ierr != 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- countQuery::setWhereClause detected "
                    "error " << ierr << " in the WHERE clause \""
                    << str << "\"";
               /**
                * accepted a suggestion from Robert Wong to allow the
                * condition to be accepted even if some columns are not found.
                **/ 
                ret = -6;
            }
        }
        
        if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "countQuery::setWhereClause -- ";
            if (conds.getString() != 0)
                lg() << "replace the where clause \""
                     << conds << "\" with \"" << tmp << "\"";
            else
                lg() << "add a new where clause \"" << tmp << "\"";
        }
        conds.swap(tmp);
        if (hits == cand) {
            delete hits;
            hits = 0;
            cand = 0;
        }
        else {
            delete hits;
            delete cand;
            hits = 0;
            cand = 0;
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- countQuery::setWhereClause failed to parse \""
            << str << "\"";
        ret = -5;
    }
    return ret;
} // ibis::countQuery::setWhereClause

/// This function accepts a user constructed query expression object.  It
/// can be used to bypass the parsing of where clause string.
int ibis::countQuery::setWhereClause(const ibis::qExpr* qx) {
    if (qx == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::setWhereClause will not use an empty "
            "where clause";
        return -4;
    }

    ibis::whereClause wc;
    int ierr = 0;
    wc.setExpr(qx);
    if (mypart != 0) {
        int ierr = wc.verify(*mypart);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- countQuery::setWhereClause(" << *qx
                << ") found the qExpr object with " << ierr << " incorrect name"
                << (ierr > 1 ? "s" : "")
                << ".  Keeping the existing where clause ";
            ierr = -6;
            return ierr;
        }
    }
    if (ibis::gVerbose > 0 &&
        wc.getExpr()->nItems() <= static_cast<unsigned>(ibis::gVerbose)) {
        wc.resetString(); // regenerate the string form of the query expression
    }

    wc.swap(conds);
    if (hits == cand) {
        delete hits;
        hits = 0;
        cand = 0;
    }
    else {
        delete hits;
        delete cand;
        hits = 0;
        cand = 0;
    }
    LOGGER(ibis::gVerbose > 1)
        << "countQuery::setWhereClause accepted new query conditions \""
        << *conds.getExpr() << "\"";
    return ierr;
} // ibis::countQuery::setWhereClause

/// Compute the possible hits expressed as hits and cand, where hits
/// contains definite hits and cand may contain additional rows that need
/// to be further examined.  This is done by using the indexes.  If
/// possible it will build new indices.  The lower bound contains only
/// records that are hits and the upper bound contains all hits but may
/// also contain some records that are not hits.  Returns 0 for success, a
/// negative value for error.
int ibis::countQuery::estimate() {
    if (mypart == 0 || mypart->nRows() == 0 || mypart->nColumns() == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::estimate() can not proceed on an "
            "empty data partition";
        return -1;
    }
    ibis::util::timer mytime("countQuery::estimate", 2);
#ifndef DONOT_REORDER_EXPRESSION
    if (conds.getExpr() != 0 && false == conds->directEval()) {
        ibis::query::weight wt(mypart);
        conds->reorder(wt);
    }
#endif

    ibis::bitvector mask;
    conds.getNullMask(*mypart, mask);
    if (m_sel != 0) {
        ibis::bitvector tmp;
        m_sel->getNullMask(*mypart, tmp);
        if (mask.size() > 0)
            mask &= tmp;
        else
            mask.swap(tmp);
    }
    if (mask.size() != mypart->nRows())
        mask.adjustSize(mypart->nRows(), mypart->nRows());

    if (conds.getExpr() != 0) { // range condition
        cand = new ibis::bitvector;
        hits = new ibis::bitvector;
        doEstimate(conds.getExpr(), *hits, *cand);
        if (cand->size() == hits->size())
            cand->adjustSize(mypart->nRows(), mypart->nRows());
        if (hits->size() != mypart->nRows()) {
            LOGGER(ibis::gVerbose > 1)
                << "countQuery::estimate -- hits.size(" << hits->size()
                << ") differs from expected value(" << mypart->nRows() << ")";
            hits->setBit(mypart->nRows()-1, 0);
        }
        *hits &= mask;
        hits->compress();

        if (cand->size() == hits->size()) {
            *cand &= mask;
            cand->compress();
        }
        else {
            delete cand;
            cand = 0;
        }
    }
    else { // everything is a hit
        hits = new ibis::bitvector(mask);
        cand = 0;
    }

    if (ibis::gVerbose > 1) {
        ibis::util::logger lg;
        lg() << "countQuery::estimate -- number of hits ";
        if (hits != 0) {
            if (cand != 0)
                lg() << "in [" << hits->cnt() << ", "
                            << cand->cnt() << "]";
            else
                lg() << " is " << hits->cnt();
        }
        else {
            delete cand;
            cand = 0;
            lg() << " is unknown";
        }
    }
    return 0;
} // ibis::countQuery::estimate

long ibis::countQuery::getMinNumHits() const {
    long nHits = (hits != 0 ? static_cast<long>(hits->cnt()) : -1);
    return nHits;
}

long ibis::countQuery::getMaxNumHits() const {
    long nHits = (cand != 0 ? static_cast<long>(cand->cnt()) :
                  (hits != 0 ? static_cast<long>(hits->cnt()) : -1));
    return nHits;
}

/// Evaluate the hits of the query condition.  It computes the exact hits.
///
/// The same answer shall be computed whether there is any index or not.
///
/// Returns 0 for success, a negative value for error.
int ibis::countQuery::evaluate() {
    if (mypart == 0 || mypart->nRows() == 0 || mypart->nColumns() == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::evaluate() can not proceed on an "
            "empty data partition";
        return -1;
    }
    int ierr;
    ibis::util::timer mytime("countQuery::evaluate", 1);

    if (hits == 0) { // have not performed an estimate
        ibis::bitvector mask;
        conds.getNullMask(*mypart, mask);
        if (m_sel != 0) {
            ibis::bitvector tmp;
            m_sel->getNullMask(*mypart, tmp);
            if (mask.size() > 0)
                mask &= tmp;
            else
                mask.swap(tmp);
        }
        if (mask.size() != mypart->nRows())
            mask.adjustSize(mypart->nRows(), mypart->nRows());

        if (conds.getExpr() != 0) { // usual range query
#ifndef DONOT_REORDER_EXPRESSION
            if (! conds->directEval()) {
                ibis::query::weight wt(mypart);
                conds->reorder(wt);
            }
#endif
            delete cand;
            cand = 0;
            hits = new ibis::bitvector;
            ierr = doEvaluate(conds.getExpr(), mask, *hits);
            if (ierr < 0) {
                delete hits;
                hits = 0;
                return ierr - 20;
            }
            hits->compress();
        }
        else { // everything is a hit
            hits = new ibis::bitvector(mask);
            cand = 0;
        }
    }
    else if (cand != 0 && cand->cnt() > hits->cnt()) {
        ibis::bitvector delta;
        (*cand) -= (*hits);

        if (cand->cnt() < (hits->size() >> 2)) { // use doScan
            ierr = doScan(conds.getExpr(), *cand, delta);
            delete cand;  // no longer need it
            cand = 0;
            if (ierr >= 0) {
                *hits |= delta;
            }
            else {
                delete hits;
                hits = 0;
                return ierr - 20;
            }
        }
        else { // use doEvaluate
            ierr = doEvaluate(conds.getExpr(), *cand, delta);
            delete cand;
            cand = 0;
            if (ierr < 0) {
                delete hits;
                hits = 0;
                return ierr - 20;
            }
            *hits |= delta;
            hits->compress();
        }
    }
    LOGGER(ibis::gVerbose > 0)
        << "From " << mypart->name() << " Where " << conds << " --> "
        << hits->cnt();
    return 0;
} // ibis::countQuery::evaluate

/// A negative number will be returned if the query has not been evaluated.
long ibis::countQuery::getNumHits() const {
    long nHits = (hits != 0 && (cand == 0 || cand == hits) ?
                  static_cast<long int>(hits->cnt()) : -1);
    return nHits;
} // ibis::countQuery::getNumHits

/// Extract the positions of the bits that are 1s in the solution.  This is
/// only valid after the query has been evaluated.  If it has not been
/// evaluated, it will return a negative number to indicate error.  Upon
/// a successful completion of this function, the return value should be
/// the rids.size().
long ibis::countQuery::getHitRows(std::vector<uint32_t> &rids) const {
    if (hits == 0 || (cand != 0 && cand != hits)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::getHitRows can proceed because "
            "the query is not fully resolved";
        return -1; // no accurate solution yet
    }

    long ierr = hits->cnt();
    try {
        rids.clear();
        rids.reserve(hits->cnt());
        for (ibis::bitvector::indexSet is = hits->firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *ii = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = *ii; j < ii[1]; ++ j)
                    rids.push_back(j);
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j)
                    rids.push_back(ii[j]);
            }
        }
        return ierr;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- countQuery::getHitRows failed to extract the ids";
        return -2;
    }
} // ibis::countQuery::getHitRows

/// Attempt to estimate the number of hits based on indexes.  Interpret a
/// null expression as satisfy everything to allow empty where clause to
/// follow the SQL standard.
void ibis::countQuery::doEstimate(const ibis::qExpr* term,
                                  ibis::bitvector& low,
                                  ibis::bitvector& high) const {
    if (term == 0) {
        high.set(1, mypart->nRows());
        low.set(1, mypart->nRows());
        return;
    }
    LOGGER(ibis::gVerbose > 7)
        << "countQuery::doEstimate -- starting to estimate "
        << *term;

    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        doEstimate(term->getLeft(), high, low);
        high.flip();
        if (low.size() == high.size()) {
            low.flip();
        }
        else {
            low.swap(high);
        }
        break;
    }
    case ibis::qExpr::LOGICAL_AND: {
        doEstimate(term->getLeft(), low, high);
        // there is no need to evaluate the right-hand side if the left-hand
        // is evaluated to have no hit
        if (low.sloppyCount() > 0) {
            // continue to evaluate the right-hand side
            ibis::bitvector b1, b2;
            doEstimate(term->getRight(), b1, b2);
            if (high.size() == low.size()) {
                if (b2.size() == b1.size()) {
                    high &= b2;
                }
                else {
                    high &= b1;
                }
            }
            else if (b2.size() == b1.size()) {
                high.copy(low);
                high &= b2;
            }
            low &= b1;
        }
        break;
    }
    case ibis::qExpr::LOGICAL_OR: {
        ibis::bitvector b1, b2;
        doEstimate(term->getLeft(), low, high);
        doEstimate(term->getRight(), b1, b2);
        if (high.size() == low.size()) {
            if (b2.size() == b1.size()) {
                high |= b2;
            }
            else {
                high |= b1;
            }
        }
        else if (b2.size() == b1.size()) {
            high.copy(low);
            high |= b2;
        }
        low |= b1;
        break;
    }
    case ibis::qExpr::LOGICAL_XOR: {
        // based on the fact that a ^ b = a - b | b - a
        // the lower and upper bounds can be computed as two separated
        // quantities
        // the whole process generates 10 new bit vectors and explicitly
        // destroys 6 of them, returns two to the caller and implicitly
        // destroys 2 (b1, b2)
        ibis::bitvector b1, b2;
        ibis::bitvector *b3, *b4, *b5;
        doEstimate(term->getLeft(), b1, b2);
        doEstimate(term->getRight(), low, high);
        if (high.size() == low.size()) {
            if (b1.size() == b2.size()) {
                b3 = b1 - high;
                b4 = low - b2;
                b5 = *b3 | *b4;
                low.swap(*b5);
                delete b3;
                delete b4;
                b3 = high - b1;
                b4 = b2 - *b5;
                delete b5;
                b5 = *b3 | *b4;
                high.swap(*b5);
                delete b5;
                delete b4;
                delete b3;
            }
            else {
                b3 = b1 - high;
                b4 = low - b1;
                b5 = *b3 | *b4;
                low.swap(*b5);
                delete b3;
                delete b4;
                b3 = high - b1;
                b4 = b1 - *b5;
                delete b5;
                b5 = *b3 | *b4;
                high.swap(*b5);
                delete b5;
                delete b4;
                delete b3;
            }
        }
        else if (b1.size() == b2.size()) {
            b3 = b1 - low;
            b4 = low - b2;
            b5 = *b3 | *b4;
            low.swap(*b5);
            delete b3;
            delete b4;
            b3 = low - b1;
            b4 = b2 - *b5;
            delete b5;
            b5 = *b3 | *b4;
            high.swap(*b5);
            delete b5;
            delete b4;
            delete b3;
        }
        else {
            low ^= b1;
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        doEstimate(term->getLeft(), low, high);
        // there is no need to evaluate the right-hand side if the left-hand
        // is evaluated to have no hit
        if (high.sloppyCount() > 0) {
            // continue to evaluate the right-hand side
            ibis::bitvector b1, b2;
            doEstimate(term->getRight(), b2, b1);
            if (high.size() == low.size()) {
                if (b1.size() == b2.size()) {
                    high -= b2;
                    low -= b1;
                }
                else {
                    high -= b2;
                    low -= b2;
                }
            }
            else if (b1.size() == b2.size()) {
                high.copy(low);
                high -= b2;
                low -= b1;
            }
            else {
                low -= b2;
            }
        }
        break;
    }
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(low);
            mypart->getNullMask(high);
        }
        else { // does not exist
            high.set(0, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;}
    case ibis::qExpr::RANGE:
        mypart->estimateRange
            (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
             low, high);
        break;
    case ibis::qExpr::DRANGE:
        mypart->estimateRange
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)),
             low, high);
        break;
    case ibis::qExpr::INTHOD:
        mypart->estimateRange
            (*(reinterpret_cast<const ibis::qIntHod*>(term)),
             low, high);
        break;
    case ibis::qExpr::UINTHOD:
        mypart->estimateRange
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)),
             low, high);
        break;
    case ibis::qExpr::STRING:
        if (0 <= mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), low)) {
            high.clear();
        }
        else {
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    case ibis::qExpr::ANYSTRING:
        if (0 <= mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), low)) {
            high.clear();
        }
        else {
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    case ibis::qExpr::KEYWORD:
        if (0 <= mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), low)) {
            high.clear();
        }
        else {
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    case ibis::qExpr::ALLWORDS:
        if (0 <= mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), low)) {
            high.clear();
        }
        else {
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    case ibis::qExpr::LIKE:
        if (0 <= mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), low)) {
            high.clear();
        }
        else {
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    case ibis::qExpr::ANYANY:
        mypart->estimateMatchAny
            (*(reinterpret_cast<const ibis::qAnyAny*>(term)), low, high);
        break;
    case ibis::qExpr::COMPRANGE:
        if (term->isConstant()) {
            const int tf = (reinterpret_cast<const ibis::compRange*>(term)
                            ->inRange() ? 1 : 0);
            high.set(tf, mypart->nRows());
            low.set(tf, mypart->nRows());
        }
        else {  // can not estimate complex range condition yet
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;
    default:
        if (term->isConstant() && term->getType() == ibis::qExpr::MATHTERM) {
            const int tf = (reinterpret_cast<const ibis::math::term*>
                            (term)->isTrue() ? 1 : 0);
            high.set(tf, mypart->nRows());
            low.set(tf, mypart->nRows());
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- countQuery::doEstimate encountered a "
                "unexpected term, presume every row is a possible hit";
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
    }
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg;
    lg() << "countQuery::doEestimate("
         << static_cast<const void*>(term) << ": " << *term
         << ") --> [" << low.cnt() << ",  "
         << (high.size()==low.size()?high.cnt():low.cnt()) << "]\n";
#if DEBUG + 0 > 1 || _DEBUG + 0 > 1
    lg() << "low \n" << low << "\nhigh \n" << high;
#else
    if (ibis::gVerbose > 30 || (low.bytes() < (2U << ibis::gVerbose)))
        lg() << "low \n" << low << "\nhigh \n" << high;
#endif
#else
    LOGGER(ibis::gVerbose > 3)
        << "countQuery::doEstimate(" << *term
        << ") --> [" << low.cnt() << ",  "
        << (high.size()==low.size()?high.cnt():low.cnt()) << ']';
#endif
} // ibis::countQuery::doEstimate

// masked sequential scan
int ibis::countQuery::doScan(const ibis::qExpr* term,
                             const ibis::bitvector& mask,
                             ibis::bitvector& ht) const {
    int ierr = 0;
    if (term == 0) {
        ht.copy(mask);
        return mask.cnt();
    }
    if (mask.cnt() == 0) { // no hits
        ht.set(0, mask.size());
        return ierr;
    }
    LOGGER(ibis::gVerbose > 7)
        << "countQuery::doScan -- reading data to resolve "
        << *term << " with mask.size() = " << mask.size()
        << " and mask.cnt() = " << mask.cnt();

    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            std::unique_ptr<ibis::bitvector> tmp(mask - ht);
            ht.copy(*tmp);
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
        }
        break;}
    case ibis::qExpr::LOGICAL_OR: {
        ibis::bitvector b1;
        ierr = doScan(term->getLeft(), mask, ht);
        // decide whether to update the mask use for the next evalutation
        // the reason for using the new mask is that we can avoid examining
        // the rows that already known to satisfy the query condition (i.e.,
        // already known to be hits)
        // want to make sure the cost of generating the new mask is less
        // than the time saved by using the new task
        // cost of generating new mask is roughly proportional
        // (mask.bytes() + ht.bytes())
        // the reduction in query evalution time is likely to be proportional
        // to ht.cnt()
        // since there are no good estimates on the coefficients, we will
        // simply directly compare the two
        if (ierr >= 0 && ht.cnt() < mask.cnt()) {
            if (ht.cnt() > mask.bytes() + ht.bytes()) {
                std::unique_ptr<ibis::bitvector> newmask(mask - ht);
                ierr = doScan(term->getRight(), *newmask, b1);
            }
            else {
                ierr = doScan(term->getRight(), mask, b1);
            }
            if (ierr > 0)
                ht |= b1;
            if (ierr >= 0)
                ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), mask, b1);
            if (ierr >= 0)
                ht ^= b1;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht -= b1;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(ht);
            ht &= mask;
        }
        else { // does not exist
            ht.set(0, mypart->nRows());
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::RANGE: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qRange*>(term)), mask, ht);
        break;}
    case ibis::qExpr::DRANGE: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)), mask, ht);
        break;}
    case ibis::qExpr::INTHOD: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qIntHod*>(term)), mask, ht);
        break;}
    case ibis::qExpr::UINTHOD: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)), mask, ht);
        break;}
    case ibis::qExpr::ANYANY: {
        ierr = mypart->matchAny
            (*(reinterpret_cast<const ibis::qAnyAny*>(term)), mask, ht);
        break;}
    case ibis::qExpr::STRING: {
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::ANYSTRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.cnt();
        }
        break;
    case ibis::qExpr::KEYWORD:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.cnt();
        }
        break;
    case ibis::qExpr::ALLWORDS:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.cnt();
        }
        break;
    case ibis::qExpr::LIKE: {
        ierr = mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *(reinterpret_cast<const ibis::compRange*>(term));
        if (cr.isConstant()) {
            if (cr.inRange()) {
                ht.copy(mask);
                ierr = mask.cnt();
            }
            else {
                ht.set(0, mask.size());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(cr, mask, ht);
        }
        break;}
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.copy(mask);
        ierr = -2;
        break;}
    case ibis::qExpr::MATHTERM: { // arithmetic expressions as true/false
        const ibis::math::term &mt =
            *reinterpret_cast<const ibis::math::term*>(term);
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.copy(mask);
                ierr = mask.sloppyCount();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose >= 0)
            << "countQuery::doScan -- unable to evaluate query term of "
            "unexpected type";
        ht.set(0, mypart->nRows());
        ierr = -1;
        break;}
    }
    if (ierr < 0) // no confirmed hits
        ht.set(0, mypart->nRows());
    LOGGER(ibis::gVerbose > 4)
        << "countQuery::doScan(" << static_cast<const void*>(term) << ": "
        << *term << ") --> " << ht.cnt() << ", ierr = " << ierr;
    return ierr;
} // ibis::countQuery::doScan

// combines the operations on index and the sequential scan in one function
int ibis::countQuery::doEvaluate(const ibis::qExpr* term,
                                 const ibis::bitvector& mask,
                                 ibis::bitvector& ht) const {
    int ierr = 0;
    if (term == 0) { // no hits
        ht.set(0, mypart->nRows());
        return ierr;
    }
    if (mask.cnt() == 0) { // no hits
        ht.set(0, mask.size());
        return ierr;
    }
    LOGGER(ibis::gVerbose > 7)
        << "countQuery::doEvaluate -- starting to evaluate " << *term;

    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ht.flip();
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
            else
                ht.clear();
        }
        break;}
    case ibis::qExpr::LOGICAL_OR: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0 && ht.cnt() < mask.cnt()) {
            ibis::bitvector b1;
            if (ht.cnt() > mask.bytes() + ht.bytes()) {
                std::unique_ptr<ibis::bitvector> newmask(mask - ht);
                ierr = doEvaluate(term->getRight(), *newmask, b1);
            }
            else {
                ierr = doEvaluate(term->getRight(), mask, b1);
            }
            if (ierr > 0)
                ht |= b1;
            if (ierr >= 0)
                ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), mask, b1);
            if (ierr > 0)
                ht ^= b1;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht -= b1;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(ht);
            ht &= mask;
        }
        else { // does not exist
            ht.set(0, mypart->nRows());
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::RANGE: {
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
             mask, ht);
        if (ierr < 0) {
            ibis::bitvector tmp;
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 ht, tmp);
            if (ierr >= 0 && tmp.sloppyCount() > 0) {
                tmp -= ht;
                ht  &= mask;
                tmp &= mask;
                if (tmp.sloppyCount() > 0) {
                    ibis::bitvector res;
                    ierr = mypart->doScan
                        (*(reinterpret_cast<const ibis::qRange*>(term)),
                         tmp, res);
                    if (ierr >= 0)
                        ht |= res;
                }
                ierr = ht.sloppyCount();
            }
        }
        break;}
    case ibis::qExpr::DRANGE: { // try evaluateRange, then doScan
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)), mask, ht);
        if (ierr < 0) { // revert to estimate and scan
            ibis::bitvector tmp;
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)),
                 ht, tmp);
            if (ierr >= 0) {
                if (ht.size() != tmp.size() || ht.cnt() >= tmp.cnt()) {
                    // tmp is taken to be the same as ht, i.e., estimateRange
                    // produced an exactly solution
                    ht &= mask;
                }
                else { // estimateRange produced an approximate solution
                    tmp -= ht;
                    ht &= mask;
                    tmp &= mask;
                    ibis::bitvector res;
                    ierr = mypart->doScan
                        (*(reinterpret_cast<const ibis::qRange*>(term)),
                         tmp, res);
                    if (ierr > 0)
                        ht |= res;
                }
            }
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::INTHOD: {
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qIntHod*>(term)), mask, ht);
        break;}
    case ibis::qExpr::UINTHOD: {
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)), mask, ht);
        break;}
    case ibis::qExpr::STRING: {
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::ANYSTRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::KEYWORD:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::ALLWORDS:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::LIKE: {
        ierr = mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;}
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *reinterpret_cast<const ibis::compRange*>(term);
        if (cr.isConstant()) {
            if (cr.inRange()) {
                ht.copy(mask);
                ierr = mask.cnt();
            }
            else {
                ht.set(0, mask.size());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(cr, mask, ht);
        }
        break;}
    case ibis::qExpr::ANYANY: {
        const ibis::qAnyAny *tmp =
            reinterpret_cast<const ibis::qAnyAny*>(term);
        ibis::bitvector more;
        ierr = mypart->estimateMatchAny(*tmp, ht, more);
        ht &= mask;
        if (ht.size() == more.size() && ht.cnt() < more.cnt()) {
            more -= ht;
            more &= mask;
            if (more.cnt() > 0) {
                ibis::bitvector res;
                mypart->matchAny(*tmp, more, res);
                ht |= res;
            }
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.copy(mask);
        ierr = mask.cnt();
        break;}
    case ibis::qExpr::MATHTERM: { // arithmetic expressions as true/false
        const ibis::math::term &mt =
            *reinterpret_cast<const ibis::math::term*>(term);
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.copy(mask);
                ierr = mask.cnt();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- countQuery::doEvaluate unable to evaluate a "
            "query term of unexpected type, copy the mask as the solution";
        ht.set(0, mask.size());
        ierr = -1;
        break;}
    }
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg;
    lg() << "countQuery::doEvaluate("
         << static_cast<const void*>(term) << ": " << *term
         << ", mask.cnt()=" << mask.cnt() << ") --> " << ht.cnt()
         << ", ierr = " << ierr << "\n";
#if DEBUG + 0 > 1 || _DEBUG + 0 > 1
    lg() << "ht \n" << ht;
#else
    if (ibis::gVerbose > 30 || (ht.bytes() < (2U << ibis::gVerbose)))
        lg() << "ht \n" << ht;
#endif
#else
    LOGGER(ibis::gVerbose > 3)
        << "countQuery::doEvaluate(" << *term << ", mask.cnt()="
        << mask.cnt() << ") --> " << ht.cnt() << ", ierr = " << ierr;
#endif
    return ierr;
} // ibis::countQuery::doEvaluate

// the function to clear most of the resouce consuming parts of a query
void ibis::countQuery::clear() {
    delete hits;
    delete cand;
    hits = 0;
    cand = 0;
} // ibis::countQuery::clear
